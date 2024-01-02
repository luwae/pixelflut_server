CC=gcc

.PHONY: all
all: server

server: connection.o canvas.o main.o
	$(CC) -o $@ $^ -lrt -lGL -lGLEW -lglfw

%.o: %.c common.h connection.h canvas.h
	$(CC) -Wall -Wextra -c -g -o $@ $<

.PHONY: clean
clean:
	rm -f connection.o main.o server
