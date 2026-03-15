#pragma once
#include "config.h"

void      queue_init(PieceQueue *q, uint32_t seed);
PieceKind queue_pop(PieceQueue *q);
PieceKind queue_peek(const PieceQueue *q, int index);
