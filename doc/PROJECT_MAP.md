# Project map

This repo is a C11 SDL2 project with a small logic core and an SDL-based frontend.

## Top-level

- `Makefile` — native build + tests (logic-only).
- `zezyi.cfg` — INI-style config (auto-created/updated at runtime).
- `assets/` — optional audio files (BGM/SFX).
- `web/` — HTML shell for the web build.
- `docs/` — web build output (WASM + static files).

## Code layout

### `src/`

- `src/app.c` — app state machine and screen routing (menu, gameplay, settings, name entry, confirm).
- `src/rules.c` — gameplay rules, scoring, modes, timers.
- `src/render_sdl.c` — SDL rendering: playfield, HUD, menus, settings, results, confirm dialog.
- `src/input.c` — keyboard input, DAS/ARR, and text entry capture.
- `src/audio.c` — SDL_mixer init + load/play SFX/BGM (missing files are skipped).
- `src/gameconfig.c` — config load/save (including language + player name).
- `src/highscore.c` — load/update/save high scores (per profile).
- `src/i18n.c` — UI strings and mode info (EN/PT-BR/ES).

### `include/`

- `include/config.h` — shared constants, enums (app states, modes, etc).
- `include/gameconfig.h` — config schema (keys, defaults, struct).
- `include/i18n.h` — translation keys and helpers.

### `tests/`

- Unit tests for logic modules (no SDL dependency).

## Runtime data flow (high level)

1. `src/app.c` loads config and sets language.
2. `src/input.c` updates a per-frame snapshot (menu/game/text).
3. `src/rules.c` advances the game state (timers, scoring, goals).
4. `src/render_sdl.c` draws HUD/menu based on the current app state.
5. `src/highscore.c` updates and writes scores to disk at end of a run.

