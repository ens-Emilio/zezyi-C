#include "piece.h"

/*
 * Each piece has 4 rotations stored as a 16-bit bitmask in row-major order
 * for a 4×4 grid (bit 15 = row0 col0, bit 12 = row0 col3, etc.).
 *
 * Bit layout:
 *   row0: bits 15-12
 *   row1: bits 11-8
 *   row2: bits  7-4
 *   row3: bits  3-0
 */

/* Spawn X centres pieces on the 10-wide board */
static const uint16_t MASKS[PIECE_COUNT][4] = {
    /* PIECE_I */
    {
        0x0F00, /* ..../XXXX/..../.... */
        0x2222, /* .X../.X../.X../.X.. */
        0x00F0, /* ..../..../XXXX/.... */
        0x4444, /* ..X./..X./..X./..X. */
    },
    /* PIECE_O */
    {
        0x0660, /* ..../.XX./.XX./.... */
        0x0660,
        0x0660,
        0x0660,
    },
    /* PIECE_T */
    {
        0x0E40, /* ..../.###./.#../....  flat-up (spawn) */
        0x0464, /* ..../.#../.##./.#..  pointing right (CW) */
        0x04E0, /* ..../.#../###./....  flat-down (180) */
        0x04C4, /* ..../.#./##../.#..  pointing left (CCW) */
    },
    /* PIECE_S */
    {
        0x06C0, /* ..../.##./##../....  spawn */
        0x08C4, /* ..../#.../##../.#..  CW */
        0x06C0,
        0x08C4,
    },
    /* PIECE_Z */
    {
        0x0C60, /* ..../ ##./.##./....  spawn */
        0x04C8, /* ..../.#./##./#...  CW */
        0x0C60,
        0x04C8,
    },
    /* PIECE_J */
    {
        0x08E0, /* ..../#.../###./....  spawn */
        0x0C88, /* ..../ ##./#.../#...  CW */
        0x0E20, /* ..../.###/..#./....  180 */
        0x044C, /* ..../.#../.#./##..  CCW */
    },
    /* PIECE_L */
    {
        0x02E0, /* ..../ .#./###./....  spawn */
        0x088C, /* ..../#.../#.../##..  CW */
        0x0E80, /* ..../.###/#.../....  180 */
        0x0C44, /* ..../ ##./.#../.#..  CCW */
    },
};

/*
 * Spawn X positions (column offset added when making a piece so it appears
 * near the centre of the board).
 */
static const int SPAWN_X[PIECE_COUNT] = {
    3, /* I – leftmost column of 4-wide bounding box */
    3, /* O */
    3, /* T */
    3, /* S */
    3, /* Z */
    3, /* J */
    3, /* L */
};

static const int SPAWN_Y[PIECE_COUNT] = {
    0, 0, 0, 0, 0, 0, 0
};

uint16_t piece_mask(PieceKind kind, int rotation)
{
    return MASKS[(int)kind][((rotation % 4) + 4) % 4];
}

int piece_cell_at(PieceKind kind, int rot, int px, int py)
{
    if (px < 0 || px >= PIECE_DIM || py < 0 || py >= PIECE_DIM)
        return 0;
    uint16_t mask = piece_mask(kind, rot);
    int bit = 15 - (py * PIECE_DIM + px);
    return (mask >> bit) & 1;
}

ActivePiece piece_make(PieceKind kind, int x, int y)
{
    ActivePiece p;
    p.kind     = kind;
    p.rotation = 0;
    p.x        = (x < 0) ? SPAWN_X[(int)kind] : x;
    p.y        = (y < 0) ? SPAWN_Y[(int)kind] : y;
    return p;
}

ActivePiece piece_rotated(const ActivePiece *p, int dir)
{
    ActivePiece r = *p;
    r.rotation = (((r.rotation + dir) % 4) + 4) % 4;
    return r;
}
