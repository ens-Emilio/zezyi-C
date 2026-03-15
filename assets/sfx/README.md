# SFX assets

Place WAV files here. The game runs without them (missing files are silently skipped).

Required files:

| File           | Event                         |
|----------------|-------------------------------|
| move.wav       | Piece moved left/right        |
| rotate.wav     | Piece rotated                 |
| wall.wav       | Piece hits wall               |
| soft_drop.wav  | Soft drop (down held)         |
| hard_drop.wav  | Hard drop (space)             |
| lock.wav       | Piece locked (no line clear)  |
| single.wav     | 1-line clear                  |
| double.wav     | 2-line clear                  |
| triple.wav     | 3-line clear                  |
| quad.wav       | 4-line clear                  |
| tspin.wav      | T-Spin clear                  |
| b2b.wav        | Back-to-Back bonus            |
| all_clear.wav  | Board cleared                 |
| level_up.wav   | Level increased               |
| game_over.wav  | Game over                     |
| hold.wav       | Hold piece swapped            |

## Free sources

- **jsfxr** (https://sfxr.me) — generate 8-bit SFX in browser, export .wav
- **sfxia** — alternative online generator

## License

Audio media in `assets/sfx/` is licensed under CC BY-NC 4.0 (non-commercial). See `LICENSE.assets` at the repo root.
