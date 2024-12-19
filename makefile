# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2

# Target executable
TARGET = imageCompression

# Source files and object files
SRC = imageCompression.c lzw.c
OBJ = $(SRC:.c=.o)

# Header files
HEADERS = lzw.h

# Default target
all: $(TARGET)

# Build the main target
$(TARGET): $(OBJ)
	$(CC) $(CFLAGS) -o $@ $(OBJ)

# Rule for object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up generated files
clean:
	rm -f $(OBJ) $(TARGET)

# Run the program with sample arguments
run: $(TARGET)
	./$(TARGET) input.txt compressed.txt
