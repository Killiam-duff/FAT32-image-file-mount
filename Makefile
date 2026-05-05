CC = gcc
CFLAGS = -Iinclude -Wall -g
TARGET = shell
SRCS = src/main.c src/lexer.c src/filesystem.c src/create.c src/delete.c

all: $(TARGET)

$(TARGET): $(SRCS)
	$(CC) $(CFLAGS) $(SRCS) -o $(TARGET)

clean:
	rm -f $(TARGET)