## Структура проекта
Ожидаемые файлы:
```text
.
├── Makefile
├── test_dpi.sv
├── dpi_funcs.cpp
├── server/
└── uvm-verilator/
```

## Основыне команды
- `make build` (проверяет наличие `uvm-verilator`; запускает verilator; генерирует C++ файлы в out; собирает бинарный файл в `out/Vtest_dpi`)
- `make run` (выполняет сборку)
- `make verilate` (генерирует C++ файлы в директорию `out/`)
- `make clean` (удаляет директорию `out/`; очищает проект)
- `make server` (собирает все необходимые бинарные файлы для server-а)
- `make clean_server` (удаляет директорию `server_obj` и файл `server.out`)