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

# Source files
SRCS = $(wildcard $(SRC_DIR)/*.c)
# Object files
OBJS = $(patsubst $(SRC_DIR)/%.c, $(OBJ_DIR)/%.o, $(SRCS))
# Include files
INCS = $(wildcard *.h)
# Executable name
EXEC = ndn

# Default target
all: $(EXEC)
gdb: CFLAGS += -g
gdb: $(EXEC)

# Link object files to create executable
$(EXEC): $(OBJS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^
	rm -r $(OBJ_DIR)

# Pattern rule for object files
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c $(INCS) | $(OBJ_DIR)
	$(CC) $(CFLAGS) -c $< -o $@

# Create directories
$(OBJ_DIR):
	mkdir -p $@

# Clean up build files
clean:
	rm -f $(EXEC)

.PHONY: all clean