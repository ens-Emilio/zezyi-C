# Zezyi — Competitive Release Design Document

**Date:** 2026-06-22
**Scope:** All areas — Gameplay, Visual/Audio, UX/Menus, Performance/Tech
**Approach:** Backbone-first (server → competitive → visual → UX → tech)
**Goal:** Competitive release with advanced features
**Timeline:** ~52 days (~2.5 months full-time)

---

## 1. Architecture

### Overview

```
┌─────────────┐     ┌──────────────────┐     ┌──────────────┐
│  Client C   │────▶│  Game Server     │────▶│  Web Dashboard│
│  (SDL2)     │◀────│  (Go/Rust)       │◀────│  (Stats)      │
└─────────────┘     └──────────────────┘     └──────────────┘
      │                     │
      ▼                     ▼
  Ghost replay        Matchmaking,
  recording           ranked, tournament
```

### Key Decisions

- **Netcode:** Frame-sync (deterministic) — all clients run the same logic, server syncs frames
- **Anti-cheat:** Server-authoritative — client sends inputs, server calculates result
- **Protocol:** WebSocket for real-time, HTTP for matchmaker/stats
- **Server language:** Go (concurrency-native, excellent WebSocket support)

### Code Impact

- `rules.c` must be 100% deterministic (seed-based RNG, no float non-determinism)
- `input.c` → capture input frames for ghost replay
- `app.c` → new state `APP_ONLINE` (lobby, vs, spectate)
- New directory: `server/` (Go code)

---

## 2. Competitive Gameplay

### New App States

| State | Description |
|---|---|
| `APP_ONLINE_LOBBY` | Public/private room, see players, ready up |
| `APP_ONLINE_VS` | Online frame-sync match |
| `APP_ONLINE_SPECTATE` | Watch match in real time |
| `APP_RANKED` | Ranked ladder with MMR/ELO |
| `APP_TOURNAMENT` | Bracket, rounds, observer mode |

### Ghost Replay (vs self)

**File format:**
```
[header] seed, mode, opponent_name
[frame 0] input snapshot (bitmask: L/R/D/rot/hard/hold)
[frame 1] input snapshot
...
```
~12 bytes/frame → 5min match = ~3.6KB

**Reproduction:**
- Main player runs `game_tick()` normally
- Ghost runs in parallel with recorded inputs
- Rendering: ghost with alpha 0.5, different color (e.g., cyan vs white)

### Netcode Frame-sync

**How it works:**
1. Player A and Player B connect to server
2. Server sends RNG seed + initial settings
3. Each frame: client sends input → server propagates to the other
4. Both run `game_tick()` with same state — if they diverge, server detects cheat

**Latency:**
- Up to 150ms tolerable (Tetris is not twitch)
- 2-frame input delay (~33ms) to compensate jitter
- Server authoritative — if client says "cleared 4 lines" and server disagrees, sync back

### Online Garbage/Penalty

- T-spin solo = 4 lines (vs 2 casual)
- Quad = 6 lines
- B2B bonus doubled
- Garbage with random offset hole (Tetris 10 Lines style)

### Determinism Fixes

**Problems in `rules.c`:**
- `fall_interval()` uses float → may diverge ARM vs x86
- `SDL_GetTicks()` for seed → use seed from server
- `double` for timers → fix-point with `int64_t` (ms internally)

**Fix:**
```c
// Before
double interval = 1.0 - (double)level * 0.08;

// After
int64_t interval_ms = 1000 - level * 80; // fix-point, 10x precision
```

### Ranked MMR

**Simplified ELO:**
- Start: 1000 MMR
- Win vs 1200 MMR: +25, Lose: -20
- Tiers: Bronze → Silver → Gold → Platinum → Diamond → Master
- Seasonal reset (3 months)

### Tournament

- Elimination bracket (8/16/32 players)
- Server creates rooms, tracks wins/losses
- Observer can spectate any match

---

## 3. Visual / Audio

### Neon Blocks Theme

| Element | Current | New |
|---|---|---|
| Cells | solid color | glow + radial gradient |
| Board border | simple line | neon border with pulse |
| Line clear | white flash | wave animation (bottom→top) |
| Ghost | solid 50% color | dashed outline |
| Hold | static | slide-in animation |
| Next queue | static | stagger fade-in |
| Score pop | fixed text | floating + fade-up |
| Combo counter | text | animated counter with shake |

### Effects by Event

| Event | Effect |
|---|---|
| T-spin | screen shake 3px + cyan flash |
| Quad | screen shake 5px + gold flash |
| All Clear | screen blinks 3x + board zoom |
| Level Up | border pulses + level number anim |
| Garbage incoming | red border pulse |
| Game Over | fade-to-black slow |

### New Palette (10 colors + background)

- Background: `#0F0F1A` (dark navy)
- Grid: `#1A1A2E` (subtle)
- I: `#00F5FF`, O: `#FFE600`, T: `#B44AFF`, S: `#00FF87`
- Z: `#FF3131`, J: `#3165FF`, L: `#FF8C00`, G: `#555555`

### Audio

- New SFX: 8-bit synth style (use existing `sfx/` as base)
- BGM per mode: menu (chill), marathon (upbeat), sprint (fast), survival (intense)
- Dynamic mixing: reduce BGM volume when garbage incoming
- Mute toggle per channel (SFX/BGM separate)

### Skins (extensible)

- Structure: `themes/<name>/` with palette + SFX override
- Default: `neon`
- Future: `retro` (pixel 8-bit), `minimal` (B&W), `pastel`
- Config: `theme = neon` in `zezyi.cfg`

---

## 4. UX / Menus

### Redesigned HUD

```
┌─────────────────────────────────────────────────────┐
│  Score: 12,450    Level: 7    Lines: 42    Combo: 3 │
├──────┬────────────────────┬──────────────┬──────────┤
│ HOLD │                    │    BOARD     │  NEXT    │
│      │                    │              │  I       │
│      │                    │              │  T       │
│      │                    │              │  S       │
│      │                    │              │  Z       │
│      │                    │              │  J       │
├──────┴────────────────────┴──────────────┴──────────┤
│ Pieces: 247  Quads: 8  T-Spins: 12  Max Combo: 7    │
└─────────────────────────────────────────────────────┘
```

**New HUD elements:**
- **APM** (actions per minute) — real-time
- **Garbage indicator** — pulsing bar when garbage is sent
- **Combo chain** — counter with animation (combo 5+ changes color)
- **Speed indicator** — current speed vs max bar

### Main Menu Redesign

```
  ┌──────────────────────────────────┐
  │          Z E Z Y I               │  ← title with glow
  ├──────────────────────────────────┤
  │  ▶ Marathon                      │
  │    Classic survival              │
  │                                  │
  │   Sprint           Ultra         │
  │   Time Attack      120s Challenge│
  │                                  │
  │   Blitz            Zen           │
  │   40s Score Rush   Relaxed       │
  │                                  │
  │   Survival         Cheese        │
  │   Speed Ramp       Garbage Race  │
  │                                  │
  │   ▶ Online           Ranked      │
  │   Versus players     Ladder      │
  │                                  │
  │   Settings    How to Play        │
  └──────────────────────────────────┘
```

### Interactive Tutorial

- 5 progressive stages (appear on first run):
  1. Movement (move the piece)
  2. Rotation (X and Z)
  3. Hard drop + soft drop
  4. Hold (C)
  5. Line clear + scoring
- Permanent skip (never shows again)

### Expanded Results Screen

- Line-per-time graph (sparkline)
- Piece breakdown (bars per type)
- T-spin breakdown (0-wall, 1-wall, 2-wall, 3-wall)
- Garbage received/sent (online)
- Share button (export image + copy stats link)

### Settings Redesign

- Tabs: Input | Display | Audio | Gameplay | Account
- Input: DAS/ARR slider, key rebinding
- Display: theme selector, ghost opacity, grid, resolution
- Audio: SFX/BGM sliders, mute toggles, test button
- Gameplay: lock delay toggle, IRS toggle, hard drop delay
- Account: player name, reset scores, connected status (online)

### Key Rebinding

- Click to capture new key
- Show current binding + "Press new key..."
- Export/import keybind profile

---

## 5. Performance / Tech

### WASM Optimized

| Optimization | Before | After |
|---|---|---|
| Size | ~500KB+ | ~200KB (strip, lto, wasm-opt) |
| Rendering | software | WebGL2 (via SDL2 renderer) |
| Audio | SDL_mixer OGG | OGG pre-decode + WebAudio |
| Asset loading | preload-file | lazy load (menu before BGM) |
| FS | emulated | MEMFS (zero I/O in browser) |

### Build Pipeline

```
Makefile targets:
  make          — debug native
  make release  — optimized native (-O2 -flto)
  make test     — unit tests
  make web      — WASM build
  make wasm-opt — post-process with wasm-opt (tinygo/wabt)
  make bench    — benchmark rules.c (60fps stress test)
```

### Expanded Tests

| Test | Coverage |
|---|---|
| `test_logic` | Board, pieces, randomizer, rules (existing) |
| `test_determinism` | Same seed → same result across 3 runs |
| `test_netcode` | Frame-sync simulation (server + 2 clients) |
| `test_ghost` | Recorded replay → reproduced identically |
| `test_i18n` | All keys translated in 3 languages |
| `test_config` | Load/save edge cases (empty, overflow, invalid) |

### CI/CD (GitHub Actions)

```yaml
jobs:
  build-linux:   # gcc debug + release
  build-web:    # emscripten
  test:         # make test + test_determinism
  wasm-size:    # verify < 250KB
  i18n-check:   # validate all keys
```

### New Languages

- EN (existing) ✓
- PT-BR (existing) ✓
- ES (existing) ✓
- **FR** (French) — new
- **JA** (Japanese) — new, katakana for piece names
- **DE** (German) — new

### Accessibility

- Colorblind mode: pattern pads (crosshatch, dots, stripes) instead of color
- High contrast mode: thick borders, saturated colors
- Configurable key repeat delay
- Font scale multiplier (small/medium/large)
- Screen reader labels (for menus)

### Expanded Config

```ini
[gameplay]
lock_delay     = 1    # 0=off, 1=on (soft lock reset)
irs            = 1    # Immediate Rotation System
hard_drop_delay= 0    # frames of delay on hard drop

[accessibility]
colorblind     = 0    # 0=none, 1=deuteranopia, 2=protanopia, 3=tritanopia
high_contrast  = 0
font_scale     = 1.0  # 0.75, 1.0, 1.25, 1.5

[theme]
name           = neon # neon, retro, minimal, pastel
```

### Performance Targets

- Native: 60fps stable, <50MB RAM
- WASM: <250KB binary, <100MB RAM, load time <3s
- Input latency: <16ms (1 frame)
- Network: <200ms round-trip tolerable

---

## Implementation Plan

### Phase 1 — Foundation (server + determinism) — ~14d

| # | Task | Dependencies |
|---|---|---|
| 1.1 | Create Go server repo | — |
| 1.2 | WebSocket handler (conn/disconn/pong) | 1.1 |
| 1.3 | Frame-sync engine (propagate inputs) | 1.2 |
| 1.4 | Matchmaker (create/join room) | 1.2 |
| 1.5 | Determinism: fix RNG + fix-point timers | — |
| 1.6 | Anti-cheat: server validates frames | 1.3 |
| 1.7 | Ghost replay: record/reproduce (local) | 1.5 |
| 1.8 | Client: APP_ONLINE state (lobby/VS) | 1.2, 1.4 |

**Deliverable:** Functional online match (2 players, frame-sync, basic anti-cheat)

### Phase 2 — Competitive Gameplay — ~12d

| # | Task | Dependencies |
|---|---|---|
| 2.1 | Ranked MMR/ELO (server + client) | 1.2 |
| 2.2 | Tournament bracket system | 1.4 |
| 2.3 | Spectate mode (observer) | 1.3 |
| 2.4 | Advanced scoring (T-spin solo, garbage offset) | 1.5 |
| 2.5 | Ghost VS overlay (client side) | 1.7 |
| 2.6 | Online garbage system (lag lines, B2B doubled) | 2.4 |
| 2.7 | Time attack global ranking | 1.2 |

**Deliverable:** Full competitive mode (ranked, tournament, spectate, ghost VS)

### Phase 3 — Visual / Audio — ~13d

| # | Task | Dependencies |
|---|---|---|
| 3.1 | Neon theme: glow shaders + palette | — |
| 3.2 | Line clear wave animation | 3.1 |
| 3.3 | Screen shake + flash effects | 3.1 |
| 3.4 | Score pop + combo animation | 3.1 |
| 3.5 | Hold/Next slide-in animations | 3.1 |
| 3.6 | Garbage incoming pulse | 3.1 |
| 3.7 | Game Over fade animation | 3.1 |
| 3.8 | Theme system (neon/retro/minimal/pastel) | 3.1 |
| 3.9 | Dynamic audio mixing | — |
| 3.10 | New SFX (8-bit synth, 15 new) | — |
| 3.11 | BGM per mode (4 new tracks) | — |

**Deliverable:** Polished neon visual, themes, effects, dynamic audio

### Phase 4 — UX / Menus — ~11d

| # | Task | Dependencies |
|---|---|---|
| 4.1 | HUD redesign (stats footer, APM) | — |
| 4.2 | Main menu redesign | — |
| 4.3 | Interactive tutorial (5 stages) | 4.2 |
| 4.4 | Expanded results (graphs, breakdown) | — |
| 4.5 | Settings with tabs + key rebinding | — |
| 4.6 | Share stats (image export + link) | 4.4 |
| 4.7 | Settings: theme selector, lock delay, IRS | 3.8 |

**Deliverable:** Polished UX, tutorial, advanced stats

### Phase 5 — Performance / Tech — ~12d

| # | Task | Dependencies |
|---|---|---|
| 5.1 | WASM optimized (lto, wasm-opt) | — |
| 5.2 | WebGL2 renderer (via SDL) | 5.1 |
| 5.3 | Lazy asset loading (WASM) | 5.1 |
| 5.4 | Determinism test (`test_determinism`) | 1.5 |
| 5.5 | Netcode test (`test_netcode`) | 1.3 |
| 5.6 | Ghost test (`test_ghost`) | 1.7 |
| 5.7 | i18n test (`test_i18n`) | — |
| 5.8 | CI/CD (GitHub Actions) | 5.4-5.7 |
| 5.9 | New languages (FR, JA, DE) | — |
| 5.10 | Accessibility (colorblind, high contrast) | 3.1 |

**Deliverable:** Optimized build, tests, CI, multi-language, accessibility

### Summary

| Phase | Tasks | Total Time |
|---|---|---|
| Foundation | 8 | ~14d |
| Competitive | 7 | ~12d |
| Visual/Audio | 11 | ~13d |
| UX/Menus | 7 | ~11d |
| Performance/Tech | 10 | ~12d |
| **Total** | **43 tasks** | **~52 days** (~2.5 months full-time) |