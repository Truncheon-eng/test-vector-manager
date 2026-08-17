# ============================================================
# Configuration
# ============================================================
CXX := g++

VERILATOR := verilator

UVM_DIR := $(CURDIR)/uvm-verilator
UVM_REPO := https://github.com/chipsalliance/uvm-verilator.git

OBJ_DIR := obj
OUT_DIR := out


# ============================================================
# UVM
# ============================================================
TOP := stress_test_top

SV_SRC := uvm/stress_test.sv


# ============================================================
# C++ DPI sources
# ============================================================
DPI_SRC := \
	src/auxiliary.cpp \
	src/dpi_wrapper.cpp


# ============================================================
# Server
# ============================================================
SERVER_SRC := \
	src/server.cpp \
	src/auxiliary.cpp

SERVER_BIN := server.out


# ============================================================
# Verilator flags
# ============================================================
VFLAGS := \
	--binary \
	--timing \
	-j 0 \
	-Wno-fatal \
	-Wno-DECLFILENAME \
	-Wno-UNUSED \
	-Wno-WIDTHTRUNC \
	-Wno-UNSIGNED \
	+define+UVM_NO_DPI \
	+define+UVM_REPORT_DISABLE_BANNER \
	+incdir+$(UVM_DIR)/src \
	--top-module $(TOP) \
	--Mdir $(OUT_DIR)

# ============================================================
# UVM sources
# ============================================================
UVM_SRC := \
	$(UVM_DIR)/src/uvm_pkg.sv \
	$(SV_SRC)


# ============================================================
# Resulting UVM binary
# ============================================================
UVM_BIN := $(OUT_DIR)/V$(TOP)


# ============================================================
# Runtime configuration
# ============================================================
N ?= 1000

SHM_SIZE ?= 4096


# ============================================================
# Targets
# ============================================================
.PHONY: \
	all \
	uvm_download \
	uvm_build \
	uvm_run \
	server \
	clean \
	distclean


all: server uvm_build


# ============================================================
# Download UVM
# ============================================================
uvm_download:

	@if [ ! -d "$(UVM_DIR)" ]; then \
		echo "Downloading uvm-verilator..."; \
		git clone $(UVM_REPO) $(UVM_DIR); \
	else \
		echo "uvm-verilator already exists"; \
	fi


# ============================================================
# Build server
# ============================================================
server:

	$(CXX) \
		-std=c++17 \
		-O2 \
		-Isrc \
		-g \
		$(SERVER_SRC) \
		-o $(SERVER_BIN)


# ============================================================
# Build UVM + DPI
# ============================================================
uvm_build: uvm_download

	$(VERILATOR) \
		$(VFLAGS) \
		$(UVM_SRC) \
		$(DPI_SRC) \
		-CFLAGS "-std=c++17 -O2 -Isrc"


# ============================================================
# Run UVM
# ============================================================
uvm_run: uvm_build

	$(UVM_BIN) \
		+NUM_TRANSACTIONS=$(N) \
		+SHM_SIZE=$(SHM_SIZE)


# ============================================================
# Clean
# ============================================================
clean:

	@rm -rf \
		$(OUT_DIR) \
		$(OBJ_DIR) \
		$(SERVER_BIN)


distclean: clean
	@rm -rf $(UVM_DIR)