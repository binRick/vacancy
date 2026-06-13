# Vacancy (raylib 6 port) — build a single native binary.
# Links the vendored raylib 6.0 static library (built from source).
#
#   make            build build/vacancy
#   make run        build + run from this dir (shaders load via relative paths)
#   make clean

RAYLIB_DIR := vendor/raylib/src
RAYLIB_LIB := $(RAYLIB_DIR)/libraylib.a

CC      := cc
CFLAGS  := -std=c11 -O2 -Wall -Wextra -Wno-unused-parameter -I$(RAYLIB_DIR)
LDLIBS  := $(RAYLIB_LIB)

UNAME := $(shell uname)
ifeq ($(UNAME),Darwin)
  LDLIBS += -framework CoreVideo -framework IOKit -framework Cocoa -framework OpenGL
else
  LDLIBS += -lGL -lm -lpthread -ldl -lrt -lX11
endif

SRC := $(wildcard src/*.c)
OBJ := $(patsubst src/%.c,build/%.o,$(SRC))
BIN := build/vacancy

all: $(BIN)

$(BIN): $(OBJ) $(RAYLIB_LIB)
	$(CC) $(OBJ) -o $@ $(LDLIBS)

build/%.o: src/%.c src/game.h | build
	$(CC) $(CFLAGS) -c $< -o $@

build:
	mkdir -p build

$(RAYLIB_LIB):
	$(MAKE) -C $(RAYLIB_DIR) PLATFORM=PLATFORM_DESKTOP

run: $(BIN)
	./$(BIN)

clean:
	rm -f build/*.o $(BIN)

.PHONY: all run clean
