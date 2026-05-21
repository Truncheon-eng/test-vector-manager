GCC := gcc
FLAGS := -g

SERVER := ./bin/server
CLIENT := ./bin/client
DESTROY := ./bin/destroy_shm

OBJ_DIR := ./obj
SRC_DIR := ./src
BIN_DIR := ./bin

.PHONY: all clean

all: $(SERVER) $(CLIENT) $(DESTROY)

clean:
	if [ -d "$(BIN_DIR)" ]; then \
		rm -rf $(BIN_DIR); \
	fi
	
	if [ -d "$(OBJ_DIR)" ]; then \
		rm -rf $(OBJ_DIR); \
	fi

$(SERVER): $(OBJ_DIR)/server.o $(OBJ_DIR)/auxiliary_functions.o | $(BIN_DIR)
	$(GCC) $(CFLAGS) $^ -o $@

$(CLIENT): $(OBJ_DIR)/client.o $(OBJ_DIR)/auxiliary_functions.o | $(BIN_DIR)
	$(GCC) $(CFLAGS) $^ -o $@

$(DESTROY): $(OBJ_DIR)/destroy_shm.o $(OBJ_DIR)/auxiliary_functions.o | $(BIN_DIR)
	$(GCC) $(CFLAGS) $^ -o $@

$(OBJ_DIR) $(BIN_DIR):
	mkdir $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(GCC) $(CFLAGS) -c $< -o $@