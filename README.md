# test-vector-manager

Менеджер тестовых векторов (TVM): генерация потоков пакетов на C++ и их проверка UVM-тестбенчем через разделяемую память. Репозиторий содержит два независимых слоя.

## Слой 1: учебный System V IPC demo

Обмен строкой между двумя процессами через System V shared memory (`ftok`, `shmget`, `shmat`):

- `bin/server` — пишет `Hello world: N!` в сегмент, ждёт подтверждения чтения;
- `bin/client` — читает сообщение, устанавливает флаг `read`;
- `bin/destroy_shm` — удаляет сегмент (`IPC_RMID`);
- `client.sh` — периодический перезапуск клиента.

Флаг `read` не защищён примитивами синхронизации — это демонстрация механизма, а не транспорт векторов.

## Слой 2: стресс-инфраструктура TVM (UVM + Verilator + POSIX shm)

```
                     ◄──── /dev/shm/tvm_ctrl ────  tvm_controller (сервисные команды)
bin/tvm_server ──write_burst──► /dev/shm/tvm_shm ──read_burst──► DPI-C ──► UVM env
 (LFSR-генератор)               (lock-free SPSC ring)          (monitor → refmodel → scoreboard)
```

### Канал управления (sim → сервер)

Отдельный сегмент `/dev/shm/tvm_ctrl` с тем же SPSC+futex механизмом (роли развёрнуты: sim — продюсер, сервер — консьюмер). Команды (`ipc/include/ctrl_packet.hpp`):

| Команда | arg | Действие |
|---|---|---|
| `CTRL_START_STREAM` | число пакетов (0 = argv N) | начать поток; до неё сервер ждёт `ctrl_wait_s` |
| `CTRL_STOP_STREAM` | — | штатно завершить поток и выставить `stream_done` |
| `CTRL_LOAD_VECTORS` | id набора (0..3) | выбрать seed LFSR: `ACE1/BEEF/CAFE/F00D` (таблица зеркальна в `test/uvm_test/lfsr_refmodel.sv`); принимается **только до старта потока**, mid-stream отклоняется |
| `CTRL_SET_RATE` | pps (0 = макс) | изменить темп генерации на лету |

UVM-сторона: компонент `tvm_controller` шлёт `LOAD_VECTORS(+VECTOR_SET=n)`, `SET_RATE(+RATE_PPS=n)` и `START_STREAM` в `start_of_simulation_phase` (строго до run_phase: блокирующий DPI-вызов в Verilator не прерывается другими процессами), `STOP_STREAM` — по завершении сбора. Аргументы сервера: `./bin/tvm_server [N] [pps] [grace_s] [ctrl_wait_s]`; `ctrl_wait_s=0` — стартовать сразу (legacy), по истечении ожидания сервер тоже стартует с argv-значениями.

**Ограничение: `LOAD_VECTORS` в середине потока отклоняется сервером** (команда логируется, seed не меняется). Причина: мгновенная смена seed рассинхронизирует golden-модель консьюмера — первый пакет после переключения фиксируется как `data_errors` (далее `resync()` подхватывает новое состояние LFSR, каскада нет), но итоговый вердикт становится ложным FAILURE. Честная поддержка требует барьерного протокола смены набора: in-band маркер в потоке данных («со следующего пакета seed = X»), который refmodel применяет без засчёта ошибки; это отдельная задача.

### Протокол

- Пакет: `test_packet_t { uint64_t seq; uint16_t data; }` (`ipc/include/packet.hpp`);
- Данные: LFSR 16 бит, полиномиальные отводы 0/2/3/5, seed `0xACE1` (`ipc/include/lfsr.hpp`) — идентичная золотая модель на SV-стороне;
- Кольцо: `RingBuffer<1024, T>` (`ipc/include/RingBuffer.hpp`) — lock-free SPSC на атомарных счётчиках с acquire/release, futex-слова для событийных ожиданий, control-линия с PID процессов и флагом `stream_done`;
- Ожидания: блокирующие вызовы на хост-стороне — симуляционное время не расходуется, `#`-задержек в тестбенче нет.

### Коды возврата DPI (`DpiResult` / `dpi_result_e`)

| Код | Значение |
|---|---|
| 0 `DPI_SUCCESS` | данные выданы |
| 1 `DPI_ERROR` | ошибка IPC |
| 2/3 `DPI_BUFFER_EMPTY/FULL` | только legacy single-packet путь |
| 4 `DPI_TIMEOUT` | нет данных дольше `READ_TIMEOUT_US` |
| 5 `DPI_PEER_GONE` | процесс-продюсер уничтожен |
| 6 `DPI_STREAM_END` | продюсер штатно завершил поток (`stream_done`) |

### Надёжность

- **Ликвидация stale-данных**: сервер делает `shm_unlink` перед стартом — каждый прогон начинается с чистого сегмента;
- **Liveness**: стороны регистрируют PID в кольце; смерть peer'а обнаруживается за ≤250 мс (`kill(pid, 0)` на тиках futex-ожидания);
- **EOF**: после последнего пакета сервер ставит `stream_done` — консьюмер завершается чисто, не дожидаясь таймаута;
- **Контракт порядка запуска**: сначала сервер, затем тест (ранний консьюмер останется на осиротевшем inode и штатно упадёт по таймауту);
- **SPSC-контракт**: ровно один писатель и один читатель; смешивание burst- и single-packet API на одной роли запрещено.

## Сборка

Зависимости: `g++` (C++17), `verilator` 5.x, `git`, GNU Make.

```sh
make                 # C demo + bin/tvm_server
make uvm_build       # UVM-тестбенч (Verilator + DPI, uvm_out/Vstress_test_top)
make uvm_download    # клонировать uvm-verilator (если нет)
```

## Запуск стресс-теста

```sh
# Терминал 1: генератор трафика
./bin/tvm_server 1000000

# Терминал 2: UVM-потребитель
make uvm_run N=1000000
# или напрямую:
./uvm_out/Vstress_test_top +NUM_TRANSACTIONS=1000000
```

Параметры симуляции (plusargs):

| Plusarg | По умолчанию | Описание |
|---|---|---|
| `+NUM_TRANSACTIONS=` | 1000 | сколько пакетов ожидать |
| `+BURST_MAX=` | 256 | пакетов на один DPI-burst (≤1024) |
| `+READ_TIMEOUT_US=` | 10000000 | хост-таймаут ожидания данных |
| `+VECTOR_SET=` | -1 (не отправлять) | id набора тестовых векторов для `LOAD_VECTORS` |
| `+RATE_PPS=` | -1 (не отправлять) | темп потока для `SET_RATE` (0 = максимум) |
| `+UVM_VERBOSITY=` | UVM_MEDIUM | уровень логов UVM |

Итог прогона — дашборд `STRESS_RESULT`: received/errors/resyncs и вердикт SUCCESS/FAILURE.

## Очистка

```sh
make clean       # bin/, obj/, uvm_out/ и /dev/shm/tvm_shm
make distclean   # + uvm-verilator/
```

## Структура

```
├── src/
│   ├── lib/ipc/           # общая C++ IPC-библиотека
│   │   ├── include/       #   заголовки (ring, channel, futex, lfsr, packet, ctrl)
│   │   └── auxiliary.cpp  #   shm_open/truncate/unlink
│   ├── server/            # продюсер: tvm_server.cpp (LFSR-генератор пакетов)
│   ├── client/            # клиентская сторона: dpi_wrapper.cpp (DPI-мост для sim)
│   └── uvm/               # критичные UVM-исходники: types/DPI-пакет, transaction,
│                          #   config, controller, monitor, refmodel, scoreboard,
│                          #   env (пакет tvm_env_pkg)
├── test/
│   ├── tvm_loopback.cpp   # C++ самотест IPC-слоя
│   └── uvm_test/          # тестовые UVM-исходники: stress_test, tvm_test_pkg, топ
├── demo/                  # слой 1: учебный System V IPC demo (server.c, client.c,
│                          #   destroy_shm.c, auxiliary_functions.c, shared_memory.h)
├── uvm-verilator/         # UVM-библиотека (клонируется make uvm_download)
├── next_task/             # отдельная песочница: UVM RAL tutorial, easyUVM
└── Makefile
```

## Ограничения

- IPC-слой Linux-only: `shm_open`, `flock`, `futex(2)`;
- `--timing` Verilator обязателен (UVM содержит `#0`/`wait()`);
- время симуляции в стресс-тесте не продвигается: весь тест проходит в момент `@ 0` — ожидание живёт на хост-стороне;
- PID-reuse теоретически может обмануть liveness-проверку;
- legacy single-packet DPI-функции (`dpi_read_data`/`dpi_write_data`) медленные (flock+mmap на вызов) и не участвуют в liveness/EOF-протоколе.
