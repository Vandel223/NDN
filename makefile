# Compiler
CC = gcc
# Compiler flags
CFLAGS = -Wall -Wextra -Werror -std=c11

# Source directory
SRC_DIR = src
# Include directory
INC_DIR = inc
# Object directory
OBJ_DIR = obj
# Binary directory
BIN_DIR = bin

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)
# Object files
OBJS = $(patsubst $(SRC_DIR)%.c, $(OBJ_DIR)%.o, $(SRCS))
# Include files
INCS = $(wildcard *.h)
# Executable name
EXEC = $(BIN_DIR)/ndn

# Default target
all: $(EXEC)
gdb: CFLAGS += -g
gdb: $(EXEC)

# Link object files to create executable
$(EXEC): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# Pattern rule for object files
obj/%.o: src/%.c $(INCS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up build files
clean:
	rm -f $(OBJS) $(EXEC)

.PHONY: all clean