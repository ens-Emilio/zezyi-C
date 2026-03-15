# Architecture

## App states

The main loop uses an explicit app state machine (see `src/app.c`):

- Main menu
- Gameplay
- Pause overlay
- Results screen
- Settings menu
- Name entry (profile name)
- Confirm screen (destructive actions)

The renderer draws the current state each frame (see `src/render_sdl.c`).

## Rendering

Key ideas:

- A bitmap font is used for all UI text (ASCII uppercase friendly).
- Settings lists use a fixed item height and a full-width highlight bar.
- The left HUD panel is structured into sections (hold box + dividers + label/value rows).

## Logic vs frontend

- Core rules and scoring live in `src/rules.c` and are unit-tested in `tests/`.
- SDL-specific code (window, audio, drawing) lives in `src/render_sdl.c` and `src/audio.c`.

