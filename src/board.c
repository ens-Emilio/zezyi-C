#include <string.h>
#include "board.h"
#include "piece.h"

/* xorshift32 PRNG (self-contained, not shared with randomizer) */
static uint32_t xr32(uint32_t *s)
{
    *s ^= *s << 13;
    *s ^= *s >> 17;
    *s ^= *s << 5;
    return *s;
}

void board_clear(Board *b)
{
    memset(b->cells, 0, sizeof(b->cells));
}

int board_cell_occupied(const Board *b, int x, int y)
{
    if (x < 0 || x >= BOARD_W) return 1;
    if (y < 0)                  return 0;  /* above board is free */
    if (y >= BOARD_H)           return 1;
    return b->cells[y][x] != CELL_EMPTY;
}

int board_piece_fits(const Board *b, const ActivePiece *p)
{
    for (int py = 0; py < PIECE_DIM; py++) {
        for (int px = 0; px < PIECE_DIM; px++) {
            if (!piece_cell_at(p->kind, p->rotation, px, py))
                continue;
            int bx = p->x + px;
            int by = p->y + py;
            if (board_cell_occupied(b, bx, by))
                return 0;
        }
    }
    return 1;
}

void board_merge_piece(Board *b, const ActivePiece *p)
{
    for (int py = 0; py < PIECE_DIM; py++) {
        for (int px = 0; px < PIECE_DIM; px++) {
            if (!piece_cell_at(p->kind, p->rotation, px, py))
                continue;
            int bx = p->x + px;
            int by = p->y + py;
            if (by >= 0 && by < BOARD_H && bx >= 0 && bx < BOARD_W)
                b->cells[by][bx] = (uint8_t)(p->kind + 1); /* Cell enum offset */
        }
    }
}

int board_clear_full_lines(Board *b)
{
    int write = BOARD_H - 1;
    int cleared = 0;

    for (int read = BOARD_H - 1; read >= 0; read--) {
        int full = 1;
        for (int x = 0; x < BOARD_W; x++) {
            if (b->cells[read][x] == CELL_EMPTY) { full = 0; break; }
        }
        if (full) {
            cleared++;
        } else {
            if (write != read)
                memcpy(b->cells[write], b->cells[read], (size_t)BOARD_W);
            write--;
        }
    }
    /* Fill top with empty lines */
    for (int y = write; y >= 0; y--)
        memset(b->cells[y], 0, (size_t)BOARD_W);

    return cleared;
}

uint32_t board_full_row_mask(const Board *b)
{
    uint32_t mask = 0;
    for (int y = 0; y < BOARD_H; y++) {
        int full = 1;
        for (int x = 0; x < BOARD_W && full; x++)
            if (b->cells[y][x] == CELL_EMPTY) full = 0;
        if (full) mask |= (1u << y);
    }
    return mask;
}

void board_clear_rows_by_mask(Board *b, uint32_t mask)
{
    int write = BOARD_H - 1;
    for (int read = BOARD_H - 1; read >= 0; read--) {
        if ((mask >> read) & 1u) continue;     /* skip cleared rows */
        if (write != read)
            memcpy(b->cells[write], b->cells[read], (size_t)BOARD_W);
        write--;
    }
    for (int y = write; y >= 0; y--)
        memset(b->cells[y], 0, (size_t)BOARD_W);
}

/*
 * Push existing cells up by `lines` rows and fill the bottom with garbage:
 * every garbage row has exactly one random hole (column = 0 when seed == 0).
 */
void board_add_garbage(Board *b, int lines, uint32_t seed)
{
    if (lines <= 0 || lines > BOARD_H) return;
    if (!seed) seed = 1;

    /* Shift existing rows upward */
    for (int y = 0; y < BOARD_H - lines; y++)
        memcpy(b->cells[y], b->cells[y + lines], (size_t)BOARD_W);

    /* Fill bottom `lines` rows with garbage */
    for (int r = BOARD_H - lines; r < BOARD_H; r++) {
        int hole = (int)(xr32(&seed) % (uint32_t)BOARD_W);
        for (int c = 0; c < BOARD_W; c++)
            b->cells[r][c] = (c == hole) ? CELL_EMPTY : CELL_GARBAGE;
    }
}
