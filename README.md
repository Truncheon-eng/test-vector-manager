# test-vector-manager

Ветка: `uvm-test-vector-manager-stress-test`. Состояние: `88c0ea4`; реализация стресс-теста добавлена в `9df0a0e`.

## Назначение и этап разработки

Стресс-тест проверяет передачу потока пакетов из отдельной C++-программы в UVM через POSIX shared memory и DPI-C. Используются кольцевой буфер и RAII-блокировка предыдущего этапа.

Сервер формирует воспроизводимую последовательность с помощью 16-битного LFSR. UVM независимо вычисляет ту же последовательность и проверяет номер и данные каждого полученного пакета. Отдельного RTL DUT с моделью LFSR в этой ветке нет: проверяется канал передачи и согласованность программных генераторов C++/SystemVerilog.

## Структура

- `include/lfsr.hpp` – C++-генератор.
- `include/packet.hpp` – пакет и число отправляемых транзакций.
- `include/RingBuffer.hpp` – шаблон очереди.
- `include/SharedMemoryLock.hpp` – RAII для flock.
- `include/auxiliary.hpp`, `src/auxiliary.cpp` – POSIX-память и операции очереди.
- `src/server.cpp` – генерация и отправка.
- `src/dpi_wrapper.cpp` – интерфейс чтения для SystemVerilog.
- `uvm/stress_test.sv` – получение и проверка.
- `src/client.cpp` – дополнительный C++-клиент; основной Makefile не включает его в стандартный запуск.
- `Makefile` – сборка сервера и UVM; также в ветке присутствует `Makefile_1`.

## Алгоритм LFSR

Начальное состояние равно `0xACE1`. На каждом шаге выполняется:

```text
feedback = state[0] XOR state[2] XOR state[3] XOR state[5]
state = (state >> 1) | (feedback << 15)
```

В пакет записывается новое состояние после сдвига. Один и тот же алгоритм реализован в C++ и SystemVerilog, поэтому при одинаковом seed ожидаемые данные совпадают без хранения полного эталонного массива.

Пакет `test_packet_t` содержит `uint64_t seq` и `uint16_t data`. Последовательность номеров начинается с нуля. Размер структуры может включать выравнивание; он не обязан равняться сумме 8+2 байта.

## Передача и память

Очередь имеет тип `RingBuffer<SIZE, test_packet_t>`, где `SIZE=1024`. Ёмкость должна быть степенью двойки; тип элемента должен удовлетворять:
- `trivially copyable`
- `standard layout`

Сервер открывает память, задаёт размер `PAGE_SIZE=8*0x1000` (32768 байт) и очищает очередь. При полной очереди повторяет попытку записи. UVM при пустой очереди повторяет чтение. Операции защищены `SharedMemoryLock`.

Размер объекта shared memory должен быть не меньше `sizeof(RingBuffer<SIZE, test_packet_t>)`. При изменении SIZE или состава пакета нужно заново проверить PAGE_SIZE. Слишком маленький объект памяти может привести к SIGBUS.

## Запуск стресс-теста

Нужны g++ с C++17, GNU Make, Git и Verilator с поддержкой используемого UVM. При отсутствии `uvm-verilator/` Makefile клонирует библиотеку; фиксированная ревизия в этой ветке не задана.

Последовательность основана на исходном README и согласована с текущими настройками.

1. Проверьте `NUM_TRANSACTIONS` в `include/packet.hpp`. В текущей версии это `1'000'000`.
2. Выполните сборку:

```sh
make
```

3. В первом терминале запустите сервер:

```sh
./server.out
```

4. Во втором терминале из того же каталога запустите приёмник с тем же числом транзакций:

```sh
make uvm_run N=1000000
```

Сервер может ждать освобождения очереди до запуска приёмника; это ожидаемо. Повторно запускать сервер во время приёма нельзя: он очищает общую очередь.

Значение N по умолчанию в Makefile равно 1000 и не совпадает с 1'000'000 пакетов сервера. N следует задавать явно. При изменении числа пакетов измените макрос, пересоберите сервер и передайте такое же N при запуске UVM.

## Результаты

Сервер выводит поля **Generated**, **Written** и **Buffer full**. UVM выводит поля **Requested**, **Received**, **Empty reads**, **Sequence errors** и **Data errors**.

Условие PASS: получено заданное число пакетов, ошибки последовательности и данных равны нулю. Счётчики пустой и полной очереди могут быть ненулевыми при нормальной работе. В сервере Buffer full увеличивается при любой неудачной записи, поэтому он не отличает заполнение от иных ошибок.

В циклах повторных попыток нет timeout-а; при остановке второго процесса или несогласованном числе транзакций выполнение может не завершиться.

## Очистка

```sh
make clean
make distclean
```

Первая команда удаляет результаты сборки, вторая дополнительно удаляет каталог UVM. Удаление POSIX-объекта через shm_unlink в сценарии не предусмотрено.

## Тестирование

Для выполнения необходимо произвести последовательность действий, описанных на этапе __Запуск стресс-теста__

Получатся следующие выводы:

- вывод в окне с запущенным бинарным файлом `server.out`
```shell
[i]: ring buffer cleared
Generated:   1000000
Written:     1000000
Buffer full: 487993
```

- вывод в окне, с запущенным UVM-окружением
```shell
- V e r i l a t i o n   R e p o r t: Verilator 5.048 2026-04-26 rev v5.048
- Verilator: Built from 0.000 MB sources in 0 modules, into 0.000 MB in 0 C++ files needing 0.000 MB
- Verilator: Walltime 0.049 s (elab=0.000, cvt=0.000, bld=0.042); cpu 0.007 s on 24 threads; allocated 19.066 MB
out/Vstress_test_top \
        +NUM_TRANSACTIONS=1000000 \
        +SHM_SIZE=4096
UVM_INFO /home/truncheon/test-vector-manager/uvm-verilator/src/base/uvm_root.svh(392) @ 0: reporter [UVM/RELNOTES]
----------------------------------------------------------------
UVM-1.2
(C) 2007-2014 Mentor Graphics Corporation
(C) 2007-2014 Cadence Design Systems, Inc.
(C) 2006-2014 Synopsys, Inc.
(C) 2011-2013 Cypress Semiconductor Corp.
(C) 2013-2014 NVIDIA Corporation
----------------------------------------------------------------

  ***********       IMPORTANT RELEASE NOTES         ************

  You are using a version of the UVM library that has been compiled
  with `UVM_NO_DEPRECATED undefined.
  See http://www.eda.org/svdb/view.php?id=3313 for more details.

  You are using a version of the UVM library that has been compiled
  with `UVM_OBJECT_DO_NOT_NEED_CONSTRUCTOR undefined.
  See http://www.eda.org/svdb/view.php?id=3770 for more details.

      (Specify +UVM_NO_RELNOTES to turn off this notice)

UVM_INFO @ 0: reporter [RNTST] Running test stress_test...
UVM_INFO /home/truncheon/test-vector-manager/uvm-verilator/src/base/uvm_traversal.svh(279) @ 0: reporter [UVM/COMP/NAMECHECK] This implementation of the component name checks requires DPI to be enabled
UVM_INFO uvm/stress_test.sv(105) @ 0: uvm_test_top [CONFIG] Transactions = 1000000
UVM_INFO uvm/stress_test.sv(127) @ 0: uvm_test_top [SHM] Shared memory opened successfully, fd=3
UVM_INFO uvm/stress_test.sv(144) @ 0: uvm_test_top [STRESS] Starting receive loop
UVM_INFO uvm/stress_test.sv(233) @ 0: uvm_test_top [PACKET] seq=0 data=0x5670 expected=0x5670
UVM_INFO uvm/stress_test.sv(233) @ 0: uvm_test_top [PACKET] seq=1 data=0xab38 expected=0xab38
UVM_INFO uvm/stress_test.sv(233) @ 0: uvm_test_top [PACKET] seq=2 data=0x559c expected=0x559c
UVM_INFO uvm/stress_test.sv(233) @ 0: uvm_test_top [PACKET] seq=3 data=0x2ace expected=0x2ace
UVM_INFO uvm/stress_test.sv(233) @ 0: uvm_test_top [PACKET] seq=4 data=0x1567 expected=0x1567
UVM_INFO uvm/stress_test.sv(233) @ 0: uvm_test_top [PACKET] seq=5 data=0x8ab3 expected=0x8ab3
UVM_INFO uvm/stress_test.sv(233) @ 0: uvm_test_top [PACKET] seq=6 data=0x4559 expected=0x4559
UVM_INFO uvm/stress_test.sv(233) @ 0: uvm_test_top [PACKET] seq=7 data=0x22ac expected=0x22ac
UVM_INFO uvm/stress_test.sv(233) @ 0: uvm_test_top [PACKET] seq=8 data=0x9156 expected=0x9156
UVM_INFO uvm/stress_test.sv(233) @ 0: uvm_test_top [PACKET] seq=9 data=0xc8ab expected=0xc8ab
UVM_INFO uvm/stress_test.sv(262) @ 0: uvm_test_top [STRESS_RESULT]
========================================
Stress test result
========================================
Requested:       1000000
Received:        1000000
Empty reads:     104536
Sequence errors: 0
Data errors:     0
========================================
UVM_INFO uvm/stress_test.sv(272) @ 0: uvm_test_top [STRESS_RESULT] PASS
UVM_INFO /home/truncheon/test-vector-manager/uvm-verilator/src/base/uvm_objection.svh(1271) @ 0: reporter [TEST_DONE] 'run' phase is ready to proceed to the 'extract' phase
UVM_INFO /home/truncheon/test-vector-manager/uvm-verilator/src/base/uvm_report_server.svh(847) @ 0: reporter [UVM/REPORT/SERVER]
--- UVM Report Summary ---

** Report counts by severity
UVM_INFO :   19
UVM_WARNING :    0
UVM_ERROR :    0
UVM_FATAL :    0
** Report counts by id
[CONFIG]     1
[PACKET]    10
[RNTST]     1
[SHM]     1
[STRESS]     1
[STRESS_RESULT]     2
[TEST_DONE]     1
[UVM/COMP/NAMECHECK]     1
[UVM/RELNOTES]     1

- /home/truncheon/test-vector-manager/uvm-verilator/src/base/uvm_root.svh:517: Verilog $finish
- S i m u l a t i o n   R e p o r t: Verilator 5.048 2026-04-26
- Verilator: $finish at 0s; walltime 69.340 s; speed 0.000 s/s
- Verilator: cpu 38.111 s on 1 threads; allocated 21 MB
```