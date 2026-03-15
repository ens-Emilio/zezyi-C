CC      = gcc
WARN    = -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Werror
SDL_CFLAGS = -I/tmp/usr/include
SDL_LIBS   = -L/tmp/usr/lib64 -Wl,-rpath,/tmp/usr/lib64 -lSDL2 -lSDL2_mixer

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

# -----------------------------------------------------------------------
# WebAssembly / Emscripten build  →  docs/index.html
# Usage: make web   (requires emscripten SDK: source emsdk_env.sh)
# Deploy: push docs/ to GitHub and enable Pages from that folder.
# -----------------------------------------------------------------------
EMCC      = emcc
WARN_WEB  = -Wall -Wextra -Wshadow -Wconversion -Wsign-conversion
WEB_OUT   = docs/index.html

web: $(GAME_SRC) web/shell.html
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

clean:
	rm -f $(TARGET) mitris test_logic
	rm -rf docs

.PHONY: all release test web clean
