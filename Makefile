CC:=gcc
override CFLAGS += -g -Wall -Wextra -c -Iinclude

SRC_DIR := src
BUILD_DIR := build
EXECUTABLE_NAME := server

all: $(BUILD_DIR) $(BUILD_DIR)/$(EXECUTABLE_NAME)

FILES_SRC := $(shell find $(SRC_DIR) -name "*.c")
FILES_OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(FILES_SRC))

SDL_CFLAGS := $(shell pkg-config sdl3 --cflags)
SDL_LIBS := $(shell pkg-config sdl3 --libs)

$(BUILD_DIR):
	mkdir $@

$(BUILD_DIR)/$(EXECUTABLE_NAME): $(FILES_OBJ)
	$(CC) -o $@ $^ $(SDL_LIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(SDL_CFLAGS) -o $@ $<

.PHONY: clean
clean:
	rm -f $(BUILD_DIR)/*
