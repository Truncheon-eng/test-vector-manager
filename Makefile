# ============================================================
# Makefile for test-vector-manager
#   Stage 1: C System V IPC demo (server/client/destroy_shm)
#   Stage 2: TVM stress infra (C++ server + UVM/Verilator + DPI)
# ============================================================

CC      := gcc
CXX     := g++

SRC_DIR := src
HDR_DIR := hdr
INC_DIR := include
OBJ_DIR := obj
BIN_DIR := bin

SERVER  := $(BIN_DIR)/server
CLIENT  := $(BIN_DIR)/client
DESTROY := $(BIN_DIR)/destroy_shm
AUX_OBJ := $(OBJ_DIR)/auxiliary_functions.o

# POSIX shm segment used by the TVM stress infra (auxiliary.hpp: FILENAME)
SHM_FILE := /dev/shm/tvm_shm

# ============================================================
# Parameters
# ============================================================
NPROC := $(shell nproc 2>/dev/null || echo 4)
MAKEFLAGS += --no-print-directory -j$(NPROC)

# Количество транзакций для стресс-теста (сервер и UVM)
N ?= 1000

# ============================================================
# Linker selection (auto-detect: mold -> lld -> gold)
# ============================================================
LINKER ?= $(shell \
	for l in mold lld gold; do \
		if command -v $$l >/dev/null 2>&1; then echo $$l; break; \
	fi; \
	done)

ifneq ($(strip $(LINKER)),)
  LDFLAGS += -fuse-ld=$(LINKER)
  LINKER_DISPLAY := $(LINKER)
else
  LINKER_DISPLAY := (system default: bfd)
endif

# ============================================================
# Flags
# ============================================================
# -MMD -MP: автозависимости от заголовков (пересборка при изменении hdr/*.h)
CFLAGS   += -g -Wall -MMD -MP -I$(HDR_DIR)
CXXFLAGS += -std=c++17 -O2 -g -I$(CURDIR)/$(INC_DIR)

DEPS := $(wildcard $(OBJ_DIR)/*.d)

# ============================================================
# TVM stress infra (UVM + Verilator + DPI)
# ============================================================
VERILATOR  := verilator

UVM_DIR    := $(CURDIR)/uvm-verilator
UVM_REPO   := https://github.com/chipsalliance/uvm-verilator.git
UVM_MARKER := .uvm_downloaded

UVM_TOP    := stress_test_top
UVM_SV_DIR := $(SRC_DIR)/uvm
# Единицы компиляции в порядке зависимостей (stress_test.sv включается в пакет через `include)
UVM_SV_SRC := $(UVM_SV_DIR)/tvm_types_pkg.sv \
              $(UVM_SV_DIR)/tvm_test_pkg.sv \
              $(UVM_SV_DIR)/uvm_testchamber.sv
UVM_SV_ALL := $(wildcard $(UVM_SV_DIR)/*.sv)
UVM_OUT    := uvm_out
UVM_BIN    := $(UVM_OUT)/V$(UVM_TOP)

DPI_SRCS   := $(SRC_DIR)/auxiliary.cpp $(SRC_DIR)/dpi_wrapper.cpp
TVM_SRCS   := $(SRC_DIR)/tvm_server.cpp $(SRC_DIR)/auxiliary.cpp
TVM_SERVER := $(BIN_DIR)/tvm_server
TVM_HPPS   := $(wildcard $(INC_DIR)/*.hpp)

TEST_DIR := test
TEST_SRC := $(TEST_DIR)/tvm_loopback.cpp
TEST_BIN := $(BIN_DIR)/tvm_loopback

ifneq ($(strip $(LDFLAGS)),)
  VFLAGS_LDFLAGS := -LDFLAGS "$(LDFLAGS)"
else
  VFLAGS_LDFLAGS :=
endif

VFLAGS := \
	--binary \
	--timing \
	-j $(NPROC) \
	-Wno-fatal \
	-Wno-DECLFILENAME \
	-Wno-UNUSED \
	-Wno-WIDTHTRUNC \
	-Wno-UNSIGNED \
	-Wno-TIMESCALEMOD \
	+define+UVM_NO_DPI \
	+define+UVM_REPORT_DISABLE_BANNER \
	+incdir+$(UVM_DIR)/src \
	+incdir+$(UVM_SV_DIR) \
	--top-module $(UVM_TOP) \
	--Mdir $(UVM_OUT) \
	$(VFLAGS_LDFLAGS)

# ============================================================
# Main Targets
# ============================================================
.PHONY: all tvm_server test uvm_download uvm_build uvm_run uvm_stress clean distclean list help

all: $(SERVER) $(CLIENT) $(DESTROY) $(TVM_SERVER)

# ------------------------------------------------------------
# Stage 1: C IPC demo
# ------------------------------------------------------------
$(SERVER): $(OBJ_DIR)/server.o $(AUX_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(CLIENT): $(OBJ_DIR)/client.o $(AUX_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(DESTROY): $(OBJ_DIR)/destroy_shm.o $(AUX_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# ------------------------------------------------------------
# Stage 2: TVM packet server (C++)
# ------------------------------------------------------------
tvm_server: $(TVM_SERVER)

$(TVM_SERVER): $(TVM_SRCS) $(TVM_HPPS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(TVM_SRCS) -o $@ $(LDFLAGS)

# ------------------------------------------------------------
# C++ IPC layer self-test (no UVM/Verilator, runs in <1 s)
# ------------------------------------------------------------
test: $(TEST_BIN)
	@$(TEST_BIN)

$(TEST_BIN): $(TEST_SRC) $(SRC_DIR)/auxiliary.cpp $(TVM_HPPS) | $(BIN_DIR)
	$(CXX) $(CXXFLAGS) $(TEST_SRC) $(SRC_DIR)/auxiliary.cpp -o $@ $(LDFLAGS)

# ------------------------------------------------------------
# UVM testbench (Verilator + DPI-C)
# ------------------------------------------------------------
uvm_download: $(UVM_MARKER)

$(UVM_MARKER):
	@if [ ! -d "$(UVM_DIR)" ]; then \
		echo ">>> [uvm] Downloading uvm-verilator..."; \
		git clone $(UVM_REPO) $(UVM_DIR); \
	fi
	@touch $@

uvm_build: $(UVM_BIN)

$(UVM_BIN): $(UVM_MARKER) $(UVM_SV_ALL) $(DPI_SRCS) $(TVM_HPPS) | $(UVM_OUT)
	@echo ">>> [build] Verilator: jobs=$(NPROC), ld=$(LINKER_DISPLAY)"
	$(VERILATOR) $(VFLAGS) \
		$(UVM_DIR)/src/uvm_pkg.sv $(UVM_SV_SRC) $(DPI_SRCS) \
		-CFLAGS "$(CXXFLAGS)"

uvm_run: uvm_build
	@echo ">>> [run] NUM_TRANSACTIONS=$(N)"
	$(UVM_BIN) +NUM_TRANSACTIONS=$(N)

# ------------------------------------------------------------
# One-shot stress run: server (background) + sim + cleanup.
# Exit code follows the test verdict, so the target is CI-usable.
# ------------------------------------------------------------
uvm_stress: uvm_build $(TVM_SERVER)
	@echo ">>> [stress] N=$(N)"
	@rm -f $(SHM_FILE); \
	$(TVM_SERVER) $(N) > $(UVM_OUT)/tvm_server.log 2>&1 & \
	sleep 0.3; \
	$(UVM_BIN) +NUM_TRANSACTIONS=$(N) | tee $(UVM_OUT)/sim.log; \
	wait; \
	echo ">>> [stress] server log:"; \
	cat $(UVM_OUT)/tvm_server.log; \
	rm -f $(SHM_FILE); \
	if grep -q "Test status:     SUCCESS" $(UVM_OUT)/sim.log; then \
		exit 0; \
	else \
		exit 1; \
	fi

$(OBJ_DIR) $(BIN_DIR) $(UVM_OUT):
	@mkdir -p $@

-include $(DEPS)

# ------------------------------------------------------------
# Cleaning
# ------------------------------------------------------------
clean:
	@rm -rf $(BIN_DIR) $(OBJ_DIR) $(UVM_OUT)
	@rm -f $(SHM_FILE)

distclean: clean
	@rm -rf $(UVM_DIR) $(UVM_MARKER)

# ------------------------------------------------------------
# Diagnostics
# ------------------------------------------------------------
list:
	@echo "CC         : $(CC)"
	@echo "CXX        : $(CXX)"
	@echo "CFLAGS     : $(CFLAGS)"
	@echo "CXXFLAGS   : $(CXXFLAGS)"
	@echo "LDFLAGS    : $(LDFLAGS)"
	@echo "LD         : $(LINKER_DISPLAY)"
	@echo "NPROC      : $(NPROC)"
	@echo "TARGETS    : $(SERVER) $(CLIENT) $(DESTROY) $(TVM_SERVER)"
	@echo "UVM_SV_SRC : $(UVM_SV_SRC)"
	@echo "DPI_SRCS   : $(DPI_SRCS)"
	@echo "UVM_BIN    : $(UVM_BIN)"

help:
	@echo "Targets:"
	@echo "  all          - Build C demo binaries + tvm_server (default)"
	@echo "  tvm_server   - Build TVM packet server (C++)"
	@echo "  test         - Build and run C++ IPC loopback self-test"
	@echo "  uvm_download - Clone uvm-verilator repository"
	@echo "  uvm_build    - Build UVM testbench with Verilator + DPI"
	@echo "  uvm_run      - Build and run UVM stress test (server separately!)"
	@echo "  uvm_stress   - One-shot: server + sim + cleanup, rc = test verdict"
	@echo "  clean        - Remove bin/, obj/, uvm_out/"
	@echo "  distclean    - clean + remove uvm-verilator/"
	@echo "  list         - Show variables"
	@echo "  help         - Show this help"
	@echo ""
	@echo "Variables:"
	@echo "  N=<count>        - Transactions for tvm_server/uvm_run (default: 1000)"
	@echo "  CC / CXX         - Compilers (default: gcc / g++)"
	@echo "  LINKER=<linker>  - mold | lld | gold | (empty = system bfd)"
	@echo "  NPROC=<n>        - Parallel jobs (default: nproc)"
	@echo ""
	@echo "Stress test:"
	@echo "  ./bin/tvm_server $(N) &   # generator -> /dev/shm/tvm_shm"
	@echo "  make uvm_run N=$(N)       # UVM consumer"
	@echo "  rm -f /dev/shm/tvm_shm    # cleanup"
