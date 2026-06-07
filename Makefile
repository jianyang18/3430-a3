# rename this file to makefile or Makefile if you want make to easily find it

CC     = gcc
CFLAGS = -Wall -Werror -Wextra -Wpedantic -g
TARGET = fat32
SRC    = main.c


$(TARGET): $(SRC) fat32.h
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET)

clean:
	rm -f $(TARGET)
