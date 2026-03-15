#pragma once
#include "config.h"

/* Initialize game for Marathon mode (default). */
void game_init(GameState *g, uint32_t seed);

/* Initialize game for a specific mode. */
void game_init_for_mode(GameState *g, uint32_t seed, GameMode mode);

/* Initialize into the main menu (no board, load high scores). */
void game_init_menu(GameState *g);

void game_spawn_next(GameState *g, const InputSnapshot *in);
int  game_try_move(GameState *g, int dx, int dy);
int  game_try_rotate(GameState *g, int dir);
void game_hard_drop(GameState *g);
void game_hold(GameState *g);
void game_lock_piece(GameState *g);
void game_tick(GameState *g, const InputSnapshot *in, double dt);

int score_for_lines(int n, int level, int b2b);
int score_for_tspin(int lines, int level, int b2b);
int score_for_all_clear(int cleared, int level);
