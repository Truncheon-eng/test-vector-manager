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
bin/tvm_server ──write_burst──> /dev/shm/tvm_shm ──read_burst──> DPI-C ──> UVM stress_test
   (LFSR-генератор)            (lock-free SPSC ring)          (Verilator --timing)
```

### Протокол

- Пакет: `test_packet_t { uint64_t seq; uint16_t data; }` (`include/packet.hpp`);
- Данные: LFSR 16 бит, полиномиальные отводы 0/2/3/5, seed `0xACE1` (`include/lfsr.hpp`) — идентичная золотая модель на SV-стороне;
- Кольцо: `RingBuffer<1024, T>` (`include/RingBuffer.hpp`) — lock-free SPSC на атомарных счётчиках с acquire/release, futex-слова для событийных ожиданий, control-линия с PID процессов и флагом `stream_done`;
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
| `+UVM_VERBOSITY=` | UVM_MEDIUM | уровень логов UVM |

Итог прогона — дашборд `STRESS_RESULT`: received/errors/resyncs и вердикт SUCCESS/FAILURE.

## Очистка

```sh
make clean       # bin/, obj/, uvm_out/ и /dev/shm/tvm_shm
make distclean   # + uvm-verilator/
```

## Структура

```
├── include/               # C++ заголовки TVM (ring, channel, futex, lfsr, packet)
├── src/                   # C demo (слой 1) + C++ TVM: сервер, DPI-обёртка
│   └── uvm/               # SV: types/DPI-пакет, transaction, config,
│                          #     monitor, refmodel, scoreboard, env, test, топ
├── hdr/                   # заголовки C demo
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
