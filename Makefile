CC=gcc

.PHONY: all
all: server


server: connection.o canvas.o main.o
	$(CC) -o $@ $^ `sdl2-config --libs` -lpthread

%.o: %.c common.h connection.h canvas.h
	$(CC) -Wall -Wextra -c -g `sdl2-config --cflags` -o $@ $<

.PHONY: clean
clean:
	rm -f connection.o main.o canvas.o server
