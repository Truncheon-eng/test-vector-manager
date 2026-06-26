# --- Настройки путей ---
# Библиотека теперь будет лежать в ~/uvm-verilator
UVM_DEST ?= $(PWD)/uvm-verilator
UVM_REPO = https://github.com/chipsalliance/uvm-verilator.git
OUT_DIR   = out

# --- Инструментарий ---
VERILATOR = verilator
V_FLAGS   = --binary --timing -j $(shell nproc) --quiet
V_FLAGS  += -Wno-fatal -Wno-DECLFILENAME -Wno-UNUSED
V_FLAGS  += -Wno-WIDTHTRUNC -Wno-UNSIGNED
V_FLAGS  += +define+UVM_NO_DPI 
V_FLAGS  += +define+UVM_REPORT_DISABLE_BANNER
V_FLAGS  += +incdir+$(UVM_DEST)/src

# --- Файлы ---
SV_FILES  = $(UVM_DEST)/src/uvm_pkg.sv test_dpi.sv
CPP_FILES = dpi_funcs.cpp

.PHONY: all run clean uvm_download

all: run

# Скачиваем UVM только если папки нет физически
uvm_download:
	@if [ ! -d "$(UVM_DEST)" ]; then \
		echo "Downloading UVM to $(UVM_DEST)..."; \
		git -c advice.detachedHead=false clone \
		--depth 1 --branch uvm-1.2 $(UVM_REPO) $(UVM_DEST); \
	fi

# Сборка: зависит от наличия библиотеки и исходников
$(OUT_DIR)/Vtest_dpi: uvm_download test_dpi.sv $(CPP_FILES)
	@mkdir -p $(OUT_DIR)
	@$(VERILATOR) $(V_FLAGS) $(SV_FILES) $(CPP_FILES) \
		--top-module test_dpi --Mdir $(OUT_DIR) -o Vtest_dpi

run: $(OUT_DIR)/Vtest_dpi
	@echo "--- Starting Simulation ---"
	@./$(OUT_DIR)/Vtest_dpi +UVM_TESTNAME=my_test
	@echo "--- Simulation Finished ---"

clean:
	@rm -rf $(OUT_DIR)
	@echo "Project cleaned (UVM library kept in $(UVM_DEST))"
