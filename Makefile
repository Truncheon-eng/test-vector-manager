# ============================================================
# Инструменты и пути
# ============================================================
CXX       := g++
VERILATOR := verilator

UVM_DIR   := $(CURDIR)/uvm-verilator
UVM_REPO  := https://github.com/chipsalliance/uvm-verilator.git
UVM_MARKER:= .uvm_downloaded

OBJ_DIR   := obj
OUT_DIR   := out

# ============================================================
# Настройки компиляции C++
# ============================================================
INCLUDES  := -Iinclude -Isrc
CXXFLAGS  := -std=c++17 -O2 -g $(INCLUDES)
LDFLAGS   := 

# ============================================================
# Исходные файлы и объекты
# ============================================================
TOP        := stress_test_top
SV_SRC     := $(wildcard uvm/*.sv)
UVM_SRC    := $(UVM_DIR)/src/uvm_pkg.sv $(SV_SRC)
UVM_BIN    := $(OUT_DIR)/V$(TOP)

# Исключаем специфичные файлы из общего списка DPI
DPI_SRC    := $(filter-out src/server.cpp src/client.cpp,$(wildcard src/*.cpp))

# Серверная часть
SERVER_SRC := $(filter-out src/client.cpp src/dpi_wrapper.cpp,$(wildcard src/*.cpp))
SERVER_OBJS:= $(patsubst src/%.cpp,$(OBJ_DIR)/%.o,$(SERVER_SRC))
SERVER_BIN := server.out

# ============================================================
# Флаги Verilator
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

# Передаем те же настройки компиляции C++ внутрь Verilator DPI
VERILATOR_CFLAGS := -std=c++17 -O2 -I$(CURDIR)/include -I$(CURDIR)/src


# ============================================================
# Параметры запуска по умолчанию
# ============================================================
N        ?= 1000
SHM_SIZE ?= 4096

# ============================================================
# Основные таргеты
# ============================================================
.PHONY: all uvm_download uvm_build uvm_run server clean distclean

all: server uvm_build

# Глобальный таргет для скачивания (для обратной совместимости)
uvm_download: $(UVM_MARKER)

$(UVM_MARKER):
	@if [ ! -d "$(UVM_DIR)" ]; then \
		echo "Downloading uvm-verilator..."; \
		git clone $(UVM_REPO) $(UVM_DIR); \
	fi
	@touch $(UVM_MARKER)

# ============================================================
# Сборка сервера (C++)
# ============================================================
server: $(SERVER_BIN)

$(SERVER_BIN): $(SERVER_OBJS)
	@echo "Linking Server: $@"
	$(CXX) $(CXXFLAGS) $^ $(LDFLAGS) -o $@

# Папка OBJ_DIR создается как order-only зависимость
$(OBJ_DIR)/%.o: src/%.cpp | $(OBJ_DIR)
	@echo "Compiling C++: $<"
	$(CXX) $(CXXFLAGS) -c $< -o $@

# ============================================================
# Сборка UVM + DPI (Verilator)
# ============================================================
uvm_build: $(UVM_BIN)

# Зависит от файлов исходников и от маркера скачивания UVM
$(UVM_BIN): $(UVM_MARKER) $(UVM_SRC) $(DPI_SRC) | $(OUT_DIR)
	@echo "Running Verilator..."
	$(VERILATOR) \
		$(VFLAGS) \
		$(UVM_SRC) \
		$(DPI_SRC) \
		-CFLAGS "$(VERILATOR_CFLAGS)"

# ============================================================
# Служебные таргеты для создания директорий
# ============================================================
$(OBJ_DIR) $(OUT_DIR):
	@mkdir -p $@

# ============================================================
# Запуск симуляции
# ============================================================
uvm_run: uvm_build
	$(UVM_BIN) +NUM_TRANSACTIONS=$(N) +SHM_SIZE=$(SHM_SIZE)

# ============================================================
# Очистка
# ============================================================
clean:
	@echo "Cleaning build artifacts..."
	@rm -rf $(OUT_DIR) $(OBJ_DIR) $(SERVER_BIN)

distclean: clean
	@echo "Deep cleaning (removing UVM repo)..."
	@rm -rf $(UVM_DIR) $(UVM_MARKER)
