CC ?= cc
SDL2_CFLAGS ?= $(shell pkg-config --cflags sdl2 2>/dev/null || sdl2-config --cflags 2>/dev/null)
SDL2_LIBS ?= $(shell pkg-config --libs sdl2 2>/dev/null || sdl2-config --libs 2>/dev/null)

CFLAGS ?= -std=c99 -Wall -Wextra -Werror -pedantic -O2
CFLAGS += $(SDL2_CFLAGS)
LDLIBS += $(SDL2_LIBS)

SRC := src/main.c \
       src/game/game.c \
       src/platform/platform_sdl2.c

BUILD_DIR := build
BIN_DIR := bin
TARGET := $(BIN_DIR)/idp-pong

OBJ := $(SRC:%.c=$(BUILD_DIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJ)
	@mkdir -p $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $(OBJ) $(LDLIBS)

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

.PHONY: all clean
