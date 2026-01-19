CC = gcc
CFLAGS = -Wall -g

# Directories and Files
SRC = main.c queue.c
OBJ = $(SRC:.c=.o)
DEPS = queue.h node.h
TARGET = main

# Default target
all: $(TARGET)

# Compile the target
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJ)

# Compile source files into object files
%.o: %.c $(DEPS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up the build files
clean:
	rm -f $(OBJ) $(TARGET)

# PHONY targets
.PHONY: all clean