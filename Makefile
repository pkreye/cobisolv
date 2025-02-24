CC=gcc

# Directories
CMD_DIR=./cmd
SRC_DIR=./src
INCLUDE_DIR=./include
OBJ_DIR=./build

# Files
# SRCS=$(wildcard $(SRC_DIR)/*.c)
SRCS=$(addprefix $(SRC_DIR)/, cobi.c  solver.c  util.c) $(addprefix $(CMD_DIR)/, main.c  readqubo.c)
OBJS=$(addprefix $(OBJ_DIR)/,$(notdir $(SRCS:.c=.o)))
MAIN=cobisolv

# Compiler
CFLAGS = -O3 -std=gnu99 -I$(SRC_DIR) -I$(INCLUDE_DIR) -Wall -Wextra -fopenmp
LDFLAGS = -lm -lcobiserv-client
DEBUG = -g

.PHONY: all main clean

all: dirs $(OBJS) main

main: $(OBJS)
	$(CC) $(CFLAGS) $(OBJS) -o $(OBJ_DIR)/$(MAIN) $(LDFLAGS)

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $< -c $(CFLAGS) -o $@

$(OBJ_DIR)/%.o: $(CMD_DIR)/%.c
	$(CC) $< -c $(CFLAGS) -o $@

install: client_lib
	cp $(OBJ_DIR)/$(CLIENT_LIB) $(CLIENT_LIB_DEST_DIR)
	cp $(CLIENT_LIB_HEADERS) $(CLIENT_LIB_HEADERS_DEST_DIR)

python:
	$(MAKE) -C $(PYTHON_DIR)

test:
	@echo "SRCS: " $(SRCS)
	@echo "OBJS: " $(OBJS)

dirs:
	mkdir -p $(OBJ_DIR)

clean:
	rm -f $(OBJ_DIR)/*
