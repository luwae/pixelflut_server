CC:=gcc
CFLAGS:=-g -Wall -Wextra -c

SRC_DIR := src
BUILD_DIR := build
EXECUTABLE_NAME := server

all: $(BUILD_DIR) $(BUILD_DIR)/$(EXECUTABLE_NAME)

FILES_SRC := $(shell find $(SRC_DIR) -name "*.c")
FILES_OBJ := $(patsubst $(SRC_DIR)/%.c,$(BUILD_DIR)/%.o,$(FILES_SRC))

SDL2_CFLAGS := $(shell sdl2-config --cflags)
SDL2_LIBS := $(shell sdl2-config --libs)

$(BUILD_DIR):
	mkdir $@

$(BUILD_DIR)/$(EXECUTABLE_NAME): $(FILES_OBJ)
	$(CC) -o $@ $^ $(SDL2_LIBS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) $(SDL2_CFLAGS) -o $@ $<

.PHONY: clean
clean:
	rm -f $(BUILD_DIR)/*
