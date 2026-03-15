# Data, profiles, and resets

## Config file

- Path: `zezyi.cfg`
- Format: simple INI-style key/value pairs
- Loaded and saved by `src/gameconfig.c`

Key fields:

- `player_name` — profile name used for per-profile score files
- `language` — `en`, `ptbr`, `es`

## High scores

High score storage is per profile:

- File pattern: `zezyi_hs_<PLAYER>.txt`
- `<PLAYER>` is derived from `player_name` (sanitized to be file-safe)

Legacy fallback:

- If the per-profile file is missing, the game can read `zezyi_hs.txt`.

## Reset actions (Settings)

Settings contains destructive actions that require confirmation:

- Reset scores — clears scores for the current profile
- Reset settings — resets settings but keeps profile name + language
- Reset all — clears profile, settings, and scores

## Name entry cancel

In name entry you can cancel and return without changing the current profile:

- Cancel: `Q` or `Esc`

