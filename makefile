# ============================================================
# Makefile for test-vector-manager (C, System V IPC)
# ============================================================

CC      := gcc

SRC_DIR := src
HDR_DIR := hdr
OBJ_DIR := obj
BIN_DIR := bin

SERVER  := $(BIN_DIR)/server
CLIENT  := $(BIN_DIR)/client
DESTROY := $(BIN_DIR)/destroy_shm
AUX_OBJ := $(OBJ_DIR)/auxiliary_functions.o

# ============================================================
# Parameters
# ============================================================
NPROC := $(shell nproc 2>/dev/null || echo 4)
MAKEFLAGS += --no-print-directory -j$(NPROC)

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
CFLAGS += -g -Wall -MMD -MP -I$(HDR_DIR)

DEPS := $(wildcard $(OBJ_DIR)/*.d)

# ============================================================
# Main Targets
# ============================================================
.PHONY: all clean list help

all: $(SERVER) $(CLIENT) $(DESTROY)

$(SERVER): $(OBJ_DIR)/server.o $(AUX_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(CLIENT): $(OBJ_DIR)/client.o $(AUX_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(DESTROY): $(OBJ_DIR)/destroy_shm.o $(AUX_OBJ) | $(BIN_DIR)
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

$(OBJ_DIR) $(BIN_DIR):
	@mkdir -p $@

-include $(DEPS)

# ------------------------------------------------------------
# Cleaning
# ------------------------------------------------------------
clean:
	@rm -rf $(BIN_DIR) $(OBJ_DIR)

# ------------------------------------------------------------
# Diagnostics
# ------------------------------------------------------------
list:
	@echo "CC        : $(CC)"
	@echo "CFLAGS    : $(CFLAGS)"
	@echo "LDFLAGS   : $(LDFLAGS)"
	@echo "LD        : $(LINKER_DISPLAY)"
	@echo "NPROC     : $(NPROC)"
	@echo "TARGETS   : $(SERVER) $(CLIENT) $(DESTROY)"

help:
	@echo "Targets:"
	@echo "  all      - Build server, client, destroy_shm (default)"
	@echo "  clean    - Remove bin/ and obj/"
	@echo "  list     - Show variables"
	@echo "  help     - Show this help"
	@echo ""
	@echo "Variables:"
	@echo "  CC=<compiler>    - C compiler (default: gcc)"
	@echo "  LINKER=<linker>  - mold | lld | gold | (empty = system bfd)"
	@echo "  NPROC=<n>        - Parallel jobs (default: nproc)"
