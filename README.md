## Запуск стресс-тестирования

Для запуска необходимо выполнить следующую последовательность действий:
- предварительно указать кол-во транзакций в [макросе](https://github.com/Truncheon-eng/test-vector-manager/blob/uvm-test-vector-manager-stress-test/include/packet.hpp#L6) (например `#define NUM_TRANSACTIONS 1000`)
- собрать необходимое окружение с помощью `make`
- запустить `./server.out`
- запустить `make uvm_run N=1000000`, указав в качестве `N` кол-во транзакций, указанных на первом шаге

Для очистки всего содержимого можно воспользоваться:
- `make clean`
- `make distclean` (удалакт директорию `uvm-verilator`)

При замене макроса [SIZE](https://github.com/Truncheon-eng/test-vector-manager/blob/uvm-test-vector-manager-stress-test/include/auxiliary.hpp#L27) необходимо также заменять макрос [PAGE_SIZE](https://github.com/Truncheon-eng/test-vector-manager/blob/uvm-test-vector-manager-stress-test/include/auxiliary.hpp#L25), в противнеом случае возможен SIGBUS.