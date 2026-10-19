#pragma once
#include "config.h"

uint32_t rng_xorshift32(uint32_t *state);
void      queue_init(PieceQueue *q, uint32_t seed);
PieceKind queue_pop(PieceQueue *q);
PieceKind queue_peek(const PieceQueue *q, int index);
