CC = gcc
CFLAGS = -Wall -Wextra -O2
TARGET = lzw
SRC = lzw.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)