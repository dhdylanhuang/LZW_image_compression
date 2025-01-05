# Compiler and flags
CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -O2

# Target executables
TARGET_COMPRESS = imageCompression
TARGET_DECOMPRESS = lzwDecompression

# Source files and object files
SRC_COMPRESS = imageCompression.c lzw.c
OBJ_COMPRESS = $(SRC_COMPRESS:.c=.o)

SRC_DECOMPRESS = lzwDecompression.c lzw.c
OBJ_DECOMPRESS = $(SRC_DECOMPRESS:.c=.o)

# Header files
HEADERS = lzw.h

# Default target
all: $(TARGET_COMPRESS) $(TARGET_DECOMPRESS)

# Build the compression target
$(TARGET_COMPRESS): $(OBJ_COMPRESS)
	$(CC) $(CFLAGS) -o $@ $(OBJ_COMPRESS)

# Build the decompression target
$(TARGET_DECOMPRESS): $(OBJ_DECOMPRESS)
	$(CC) $(CFLAGS) -o $@ $(OBJ_DECOMPRESS)

# Rule for object files
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up generated files
clean:
	rm -f $(OBJ_COMPRESS) $(OBJ_DECOMPRESS) $(TARGET_COMPRESS) $(TARGET_DECOMPRESS)

# Run the compression program with sample arguments
run_compress: $(TARGET_COMPRESS)
	./$(TARGET_COMPRESS) input.txt compressed.txt

# Run the decompression program with sample arguments
run_decompress: $(TARGET_DECOMPRESS)
	./$(TARGET_DECOMPRESS) compressed.txt output.txt
