#pragma once
#include "config.h"

void     board_clear(Board *b);
int      board_cell_occupied(const Board *b, int x, int y);
int      board_piece_fits(const Board *b, const ActivePiece *p);
void     board_merge_piece(Board *b, const ActivePiece *p);
int      board_clear_full_lines(Board *b);
uint32_t board_full_row_mask(const Board *b);
void     board_clear_rows_by_mask(Board *b, uint32_t mask);
/* Add garbage rows at the bottom (used by Cheese mode) */
void     board_add_garbage(Board *b, int lines, uint32_t seed);
