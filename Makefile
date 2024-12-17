CC=gcc
CFLAGS=-g -Wall -Wextra -c

all: build/server

BUILD_DIR = build
FILES = buffer canvas connection main net
FILES_OBJ = $(patsubst %,$(BUILD_DIR)/%.o,$(FILES))

SDL2_CFLAGS = $(shell sdl2-config --cflags)
SDL2_LIBS = $(shell sdl2-config --libs)

$(BUILD_DIR)/server: $(FILES_OBJ)
	$(CC) -o $@ $^ $(SDL2_LIBS)

$(BUILD_DIR)/%.o: src/%.c
	$(CC) $(CFLAGS) $(SDL2_CFLAGS) -o $@ $<

.PHONY: clean
clean:
	rm -f $(BUILD_DIR)/*
