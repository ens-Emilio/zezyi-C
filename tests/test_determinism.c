/*
 * Determinism test — verify same seed produces identical results across runs.
 * Compile: make test_determinism
 */
#include <stdio.h>
#include <string.h>
#include "config.h"
#include "rules.h"
#include "board.h"
#include "piece.h"
#include "randomizer.h"

static int g_pass = 0, g_fail = 0;
#define EXPECT(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, msg); } \
} while (0)

static uint32_t run_simulation(uint32_t seed, int ticks)
{
    GameState g;
    game_init(&g, seed);
    InputSnapshot in = {0};
    for (int i = 0; i < ticks; i++) {
        in.left = in.right = in.down = 0;
        in.rotate_cw = in.rotate_ccw = in.hard_drop = in.hold = 0;
        if (i % 3 == 0)  in.right = 1;
        if (i % 5 == 0)  in.rotate_cw = 1;
        if (i % 7 == 0)  in.hard_drop = 1;
        game_tick(&g, &in, 1.0 / 60.0);
    }
    uint32_t hash = 0;
    hash ^= (uint32_t)g.score;
    hash ^= (uint32_t)g.level;
    hash ^= (uint32_t)g.total_lines;
    hash ^= (uint32_t)g.combo;
    hash ^= (uint32_t)g.tick;
    for (int y = 0; y < BOARD_H; y++)
        for (int x = 0; x < BOARD_W; x++)
            hash ^= ((uint32_t)g.board.cells[y][x] << ((x + y) % 24));
    return hash;
}

int main(void)
{
    printf("\n== Determinism ==\n");
    const uint32_t seed = 0xCAFEBABE;
    const int ticks = 600;
    uint32_t h1 = run_simulation(seed, ticks);
    uint32_t h2 = run_simulation(seed, ticks);
    uint32_t h3 = run_simulation(seed, ticks);
    EXPECT(h1 == h2, "run 1 == run 2");
    EXPECT(h2 == h3, "run 2 == run 3");
    EXPECT(h1 != 0, "hash non-zero");
    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
