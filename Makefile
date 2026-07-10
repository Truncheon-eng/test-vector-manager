UVM_DEST ?= $(PWD)/uvm-verilator
UVM_REPO = https://github.com/chipsalliance/uvm-verilator.git
OUT_DIR  = out

VERILATOR = verilator

V_FLAGS  = --timing -j $(shell nproc) --quiet
V_FLAGS += -Wno-fatal -Wno-DECLFILENAME -Wno-UNUSED
V_FLAGS += -Wno-WIDTHTRUNC -Wno-UNSIGNED
V_FLAGS += +define+UVM_NO_DPI
V_FLAGS += +define+UVM_REPORT_DISABLE_BANNER
V_FLAGS += +incdir+$(UVM_DEST)/src

SV_FILES  = $(UVM_DEST)/src/uvm_pkg.sv test_dpi.sv
CPP_FILES = dpi_funcs.cpp

TOP = test_dpi
BIN = $(OUT_DIR)/V$(TOP)

SRC_DIR=./server
SRC=$(wildcard $(SRC_DIR)/*.cpp)

OBJ_DIR = ./server_obj
OBJ = $(patsubst $(SRC_DIR)/%.cpp, $(OBJ_DIR)/%.o, $(SRC))
GCC = g++
EXE = server.out


.PHONY: all uvm_download verilate build run clean server clean_server

all: build

uvm_download:
	@if [ ! -d "$(UVM_DEST)" ]; then \
		echo "Downloading UVM to $(UVM_DEST)..."; \
		git -c advice.detachedHead=false clone \
			--depth 1 --branch uvm-1.2 $(UVM_REPO) $(UVM_DEST); \
	fi

verilate: uvm_download $(SV_FILES) $(CPP_FILES)
	@mkdir -p $(OUT_DIR)
	@$(VERILATOR) $(V_FLAGS) \
		--cc $(SV_FILES) $(CPP_FILES) \
		--top-module $(TOP) \
		--Mdir $(OUT_DIR) \
		--exe \
		--main \
		-o V$(TOP)

build: verilate server
	@$(MAKE) -C $(OUT_DIR) -f V$(TOP).mk -j$(shell nproc)

run: build
	@echo "--- Starting Simulation ---"
	@./$(BIN) +UVM_TESTNAME=my_test
	@echo "--- Simulation Finished ---"

clean: clean_server
	@rm -rf $(OUT_DIR)
	@echo "Project cleaned"

clean_server:
	@if [ -d "$(OBJ_DIR)" ]; then rm -rf $(OBJ_DIR) ; fi
	@if [ -f "$(EXE)" ]; then rm "$(EXE)" ; fi

server: $(EXE)

$(EXE): $(OBJ)
	$(GCC) $^ -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp | $(OBJ_DIR)
	$(GCC) -c $< -o $@

$(OBJ_DIR):
	@mkdir $@