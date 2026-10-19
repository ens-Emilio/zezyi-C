CC      = gcc
WARN    = -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror
# SDL flags via pkg-config with sdl2-config fallback.
# Override by setting SDL_CFLAGS / SDL_LIBS in environment or command line.
PKG_SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
PKG_SDL_LIBS   := $(shell pkg-config --libs sdl2 SDL2_mixer 2>/dev/null)
ifeq ($(strip $(PKG_SDL_CFLAGS)),)
PKG_SDL_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
endif
ifeq ($(strip $(PKG_SDL_CFLAGS)),)
PKG_SDL_CFLAGS := -I/usr/include/SDL2 -D_GNU_SOURCE=1 -D_REENTRANT
endif
ifeq ($(strip $(PKG_SDL_LIBS)),)
PKG_SDL_LIBS := $(shell pkg-config --libs sdl2 2>/dev/null)
ifeq ($(strip $(PKG_SDL_LIBS)),)
PKG_SDL_LIBS := $(shell sdl2-config --libs 2>/dev/null)
ifeq ($(strip $(PKG_SDL_LIBS)),)
PKG_SDL_LIBS := -lSDL2
endif
endif
PKG_SDL_LIBS += -lSDL2_mixer
endif
SDL_CFLAGS ?= $(PKG_SDL_CFLAGS)
SDL_LIBS   ?= $(PKG_SDL_LIBS)
GAME_SRC = $(wildcard src/*.c)
TARGET   = zezyi

# -----------------------------------------------------------------------
# Default: debug build
# -----------------------------------------------------------------------
all: $(TARGET)

$(TARGET): $(GAME_SRC)
	$(CC) -std=c11 $(WARN) $(SDL_CFLAGS) -g -O0 \
	    $(GAME_SRC) -Iinclude $(SDL_LIBS) -o $@

# -----------------------------------------------------------------------
# Release build
# -----------------------------------------------------------------------
release: $(GAME_SRC)
	$(CC) -std=c11 $(WARN) $(SDL_CFLAGS) -O2 -DNDEBUG \
	    $(GAME_SRC) -Iinclude $(SDL_LIBS) -o $(TARGET)

# -----------------------------------------------------------------------
# Logic-only unit tests (no SDL)
# -----------------------------------------------------------------------
TEST_SRC = tests/test_logic.c \
           src/piece.c src/board.c src/randomizer.c \
           src/rules.c src/highscore.c
test: $(TEST_SRC)
	$(CC) -std=c11 $(WARN) -g -O0 \
	    $(TEST_SRC) -Iinclude -DZEZYI_HEADLESS -o test_logic
	./test_logic
	$(CC) -std=c11 $(WARN) -g -O0 \
	    tests/test_determinism.c src/piece.c src/board.c src/randomizer.c src/rules.c src/highscore.c \
	    -Iinclude -DZEZYI_HEADLESS -o test_determinism && ./test_determinism
	$(CC) -std=c11 $(WARN) -g -O0 \
	    tests/test_replay.c src/piece.c src/board.c src/randomizer.c src/rules.c src/highscore.c src/replay.c \
	    -Iinclude -DZEZYI_HEADLESS -o test_replay && ./test_replay
	$(CC) -std=c11 $(WARN) -g -O0 \
	    tests/test_competitive.c src/competitive.c -Iinclude -DZEZYI_HEADLESS -o test_competitive && ./test_competitive

# -----------------------------------------------------------------------
# Determinism test — same seed must produce identical results (no SDL)
# -----------------------------------------------------------------------
DETERM_SRC = tests/test_determinism.c \
             src/piece.c src/board.c src/randomizer.c \
             src/rules.c src/highscore.c
test_determinism: $(DETERM_SRC)
	$(CC) -std=c11 $(WARN) -g -O0 \
	    $(DETERM_SRC) -Iinclude -DZEZYI_HEADLESS -o test_determinism
	./test_determinism

# -----------------------------------------------------------------------
# Replay / ghost recording test (no SDL)
# -----------------------------------------------------------------------
REPLAY_SRC = tests/test_replay.c \
             src/piece.c src/board.c src/randomizer.c \
             src/rules.c src/highscore.c src/replay.c
test_replay: $(REPLAY_SRC)
	$(CC) -std=c11 $(WARN) -g -O0 \
	    $(REPLAY_SRC) -Iinclude -DZEZYI_HEADLESS -o test_replay
	./test_replay

# -----------------------------------------------------------------------
# WebAssembly / Emscripten build  →  docs/index.html
# Usage: make web   (requires emscripten SDK: source emsdk_env.sh)
# Deploy: push docs/ to GitHub and enable Pages from that folder.
# -----------------------------------------------------------------------
EMCC      = emcc
WARN_WEB  = -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion
WEB_OUT   = docs/index.html

web: $(GAME_SRC) web/shell.html
	@command -v $(EMCC) >/dev/null 2>&1 || { echo "error: $(EMCC) not found. Install Emscripten: https://emscripten.org/docs/getting_started/downloads.html"; exit 1; }
	mkdir -p docs
	$(EMCC) -std=gnu11 $(WARN_WEB) \
	    -s USE_SDL=2 \
	    -s USE_SDL_MIXER=2 \
	    -s ALLOW_MEMORY_GROWTH=1 \
	    -s EXPORTED_RUNTIME_METHODS='["FS"]' \
	    --preload-file assets \
	    --shell-file web/shell.html \
	    -O2 \
	    $(GAME_SRC) -Iinclude \
	    -o $(WEB_OUT)
	@echo "Web build ready: $(WEB_OUT)"

COMPETITIVE_SRC = tests/test_competitive.c \
                  src/competitive.c
test_competitive: $(COMPETITIVE_SRC)
	$(CC) -std=c11 $(WARN) -g -O0 \
	    $(COMPETITIVE_SRC) -Iinclude -DZEZYI_HEADLESS -o test_competitive
	./test_competitive

clean:
	rm -f $(TARGET) mitris test_logic test_determinism test_replay test_competitive
	rm -f docs/index.html docs/index.js docs/index.wasm docs/index.data
	rmdir docs 2>/dev/null || true

.PHONY: all release test test_determinism test_replay test_competitive web clean ci

ci: test
	@cd server && go vet ./... && go test ./... 2>&1 | tail -20; echo "ci: done"
