#pragma once
#include "config.h"

/* Load best scores from file into g->hs_* fields. */
void hs_load(GameState *g);

/* Save g->hs_* fields to file (call after hs_update). */
void hs_save(const GameState *g);

/*
 * Compare current game result against stored high scores and update
 * g->hs_* fields in-place if a new record is set.
 * Returns 1 if any record was beaten, 0 otherwise.
 */
int hs_update(GameState *g);
