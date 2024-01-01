CC=gcc

.PHONY: all
all: server

server: connection.o main.o
	$(CC) -o $@ $^

%.o: %.c common.h connection.h
	$(CC) -Wall -Wextra -c -g -o $@ $<

.PHONY: clean
clean:
	rm -f connection.o main.o server
