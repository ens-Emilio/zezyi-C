#pragma once
#include "config.h"

uint16_t piece_mask(PieceKind kind, int rotation);
int      piece_cell_at(PieceKind kind, int rot, int px, int py);
ActivePiece piece_make(PieceKind kind, int x, int y);
ActivePiece piece_rotated(const ActivePiece *p, int dir);
