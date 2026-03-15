# zezyi

Falling-block puzzle game written in C (C11) with SDL2 + SDL2_mixer.

## Features

- Multiple game modes: Marathon, Sprint, Ultra, Blitz, Zen, Survival, Cheese.
- Keyboard input with DAS/ARR.
- Config file (`zezyi.cfg`) for input/display/audio/UI language.
- Per-user high scores (stored on disk).
- Built-in logic unit tests (`make test`) that do not require SDL.

## Build

Requirements:

- `gcc` (or compatible C compiler)
- SDL2 headers/libs
- SDL2_mixer headers/libs

Build debug:

```sh
make
```

Build release:

```sh
make release
```

Run:

```sh
./zezyi
```

Run logic tests:

```sh
make test
```

## Controls

In-game:

- Move: `Left/Right`
- Soft drop: hold `Down`
- Hard drop: `Space`
- Rotate: `X` (clockwise), `Z` (counter-clockwise)
- Hold: `C` or `Left Shift`
- Pause/Resume: `P` or `Esc`
- Back to menu: `Q`

Menus:

- Navigate: `Up/Down`
- Change a setting: `Left/Right`
- Activate an action / confirm: `Space`
- Back / cancel: `Q`
- Settings: `S`
- Resolution quick-cycle (main menu): `R`

Name entry:

- Type: `A-Z` / `0-9`
- Confirm: `Enter`
- Cancel: `Q` or `Esc`

## Config (`zezyi.cfg`)

The game reads and writes a simple INI-style config file:

- `player_name` (profile name for scores)
- `language` (`en`, `ptbr`, `es`)
- input timing: `das_ms`, `arr_ms`
- display: `ghost`, `grid_lines`, `resolution`
- audio: `sfx_volume`, `bgm_volume`

See `include/gameconfig.h` and `src/gameconfig.c` for supported keys.

## Data files

- Config: `zezyi.cfg`
- High scores (per user): `zezyi_hs_<PLAYER>.txt`
  - `<PLAYER>` is derived from `player_name` (sanitized).
  - Legacy fallback: `zezyi_hs.txt` (read if per-user file is missing).

## User management & resets

Open `SETTINGS` (`S` on main menu) to:

- Edit name (changes `player_name` and switches the score file)
- Reset scores (for the current user)
- Reset settings (keeps user + language)
- Reset all (clears user, settings, and scores; requires confirmation)

Destructive actions show a confirm screen:

- `Space` confirms
- `Q` cancels

## Project structure

- `src/` — game code
  - `app.c` — main loop + app state machine
  - `rules.c` — core gameplay rules, scoring, mode logic
  - `render_sdl.c` — rendering (HUD/menu/settings/results)
  - `input.c` — keyboard handling + text entry + DAS/ARR
  - `audio.c` — SFX/BGM loading and playback
  - `gameconfig.c` — config load/save
  - `highscore.c` — high score load/save/update
  - `i18n.c` — UI translations (EN/PT-BR/ES)
- `include/` — headers
- `tests/` — logic-only unit tests
- `assets/` — media files (BGM/SFX)
- `doc/` — project documentation (architecture, data, translations)

## Assets

The game runs without media files. To add your own:

- BGM: `assets/bgm/` (OGG/MP3/WAV supported by your SDL_mixer build)
- SFX: `assets/sfx/` (WAV)

See `assets/bgm/README.md` and `assets/sfx/README.md` for filenames.

## License

- Code: MIT — see `LICENSE`
- Audio media in `assets/bgm/` and `assets/sfx/`: CC BY-NC 4.0 — see `LICENSE.assets`
