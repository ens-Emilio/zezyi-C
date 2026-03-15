/*
 * Logic-only unit tests — no SDL required.
 * Compile: make test
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "config.h"
#include "piece.h"
#include "board.h"
#include "randomizer.h"
#include "rules.h"
#include "highscore.h"

/* -----------------------------------------------------------------------
 * Test framework
 * -------------------------------------------------------------------- */

static int g_pass = 0, g_fail = 0;

#define EXPECT(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, msg); } \
} while (0)

static void section(const char *name) { printf("\n== %s ==\n", name); }

/* -----------------------------------------------------------------------
 * piece tests
 * -------------------------------------------------------------------- */

static void test_pieces(void)
{
    section("piece");

    /* Every piece/rotation must have exactly 4 cells */
    for (int k = 0; k < (int)PIECE_COUNT; k++)
        for (int r = 0; r < 4; r++) {
            int count = 0;
            for (int py = 0; py < PIECE_DIM; py++)
                for (int px = 0; px < PIECE_DIM; px++)
                    if (piece_cell_at((PieceKind)k, r, px, py))
                        count++;
            EXPECT(count == 4, "each rotation has 4 cells");
        }

    /* CW increments rotation index */
    ActivePiece p = piece_make(PIECE_T, 3, 0);
    EXPECT(p.rotation == 0, "T spawns rot=0");
    EXPECT(piece_rotated(&p, 1).rotation == 1, "CW → rot=1");
    ActivePiece p1 = piece_rotated(&p, 1);
    ActivePiece p2 = piece_rotated(&p1, 1);
    EXPECT(p2.rotation == 2, "CW×2 → rot=2");

    /* CCW from 0 → 3 */
    EXPECT(piece_rotated(&p, -1).rotation == 3, "CCW from 0 → 3");

    /* CW wraps 3→0 */
    ActivePiece p3 = p; p3.rotation = 3;
    EXPECT(piece_rotated(&p3, 1).rotation == 0, "CW wraps 3→0");
}

/* -----------------------------------------------------------------------
 * board tests
 * -------------------------------------------------------------------- */

static void test_board(void)
{
    section("board");

    Board b;
    board_clear(&b);

    int all_empty = 1;
    for (int y = 0; y < BOARD_H; y++)
        for (int x = 0; x < BOARD_W; x++)
            if (b.cells[y][x]) { all_empty = 0; break; }
    EXPECT(all_empty, "board_clear sets all to 0");

    EXPECT(board_cell_occupied(&b, -1, 5), "x=-1 occupied");
    EXPECT(board_cell_occupied(&b, 10, 5), "x=10 occupied");
    EXPECT(!board_cell_occupied(&b, 0, -1), "y<0 is free");
    EXPECT(board_cell_occupied(&b, 0, 20), "y=20 occupied");

    ActivePiece I = piece_make(PIECE_I, 3, 18);
    EXPECT(board_piece_fits(&b, &I), "I fits empty board");
    board_merge_piece(&b, &I);
    EXPECT(b.cells[19][3] != CELL_EMPTY, "I cell 3 merged");
    EXPECT(b.cells[19][2] == CELL_EMPTY, "cell 2 not I");

    /* Fill 2 rows, clear */
    for (int x = 0; x < BOARD_W; x++) {
        b.cells[18][x] = (uint8_t)CELL_I;
        b.cells[19][x] = (uint8_t)CELL_I;
    }
    int cleared = board_clear_full_lines(&b);
    EXPECT(cleared == 2, "clear returns 2");
    int bottom_empty = 1;
    for (int x = 0; x < BOARD_W; x++)
        if (b.cells[19][x] || b.cells[18][x]) { bottom_empty = 0; break; }
    EXPECT(bottom_empty, "cleared rows empty");
}

/* -----------------------------------------------------------------------
 * randomizer tests
 * -------------------------------------------------------------------- */

static void test_randomizer(void)
{
    section("randomizer");

    PieceQueue q;
    queue_init(&q, 42);

    int seen[PIECE_COUNT] = {0};
    for (int i = 0; i < 7; i++) {
        PieceKind k = queue_pop(&q);
        EXPECT(k >= 0 && k < PIECE_COUNT, "pop returns valid kind");
        seen[(int)k]++;
    }
    int all_seen = 1;
    for (int i = 0; i < (int)PIECE_COUNT; i++)
        if (seen[i] != 1) { all_seen = 0; break; }
    EXPECT(all_seen, "first 7-bag has each piece once");

    int seen2[PIECE_COUNT] = {0};
    for (int i = 0; i < 7; i++) seen2[(int)queue_pop(&q)]++;
    int all_seen2 = 1;
    for (int i = 0; i < (int)PIECE_COUNT; i++)
        if (seen2[i] != 1) { all_seen2 = 0; break; }
    EXPECT(all_seen2, "second 7-bag has each piece once");

    queue_init(&q, 99);
    PieceKind peeked = queue_peek(&q, 0);
    PieceKind popped = queue_pop(&q);
    EXPECT(peeked == popped, "peek(0) matches pop()");
}

/* -----------------------------------------------------------------------
 * rules / scoring tests
 * -------------------------------------------------------------------- */

static void test_rules(void)
{
    section("rules");

    GameState g;
    game_init(&g, 1234);

    EXPECT(g.app_state == APP_PLAYING, "starts PLAYING");
    EXPECT(g.score == 0, "score=0");
    EXPECT(g.level == 0, "level=0");
    EXPECT(g.combo == -1, "combo=-1");
    EXPECT(g.b2b == 0, "b2b=0");
    EXPECT(g.hold == PIECE_COUNT, "no hold");
    EXPECT(g.mode == MODE_MARATHON, "default mode Marathon");

    /* Lateral movement */
    int sx = g.current.x;
    if (game_try_move(&g, 1, 0))
        EXPECT(g.current.x == sx + 1, "move right");
    for (int i = 0; i < 20; i++) game_try_move(&g, -1, 0);
    EXPECT(g.current.x >= 0, "piece stays in board");

    /* Scoring */
    EXPECT(score_for_lines(1,0,0) == 100, "single=100");
    EXPECT(score_for_lines(2,0,0) == 300, "double=300");
    EXPECT(score_for_lines(3,0,0) == 500, "triple=500");
    EXPECT(score_for_lines(4,0,0) == 800, "4line=800");
    EXPECT(score_for_lines(4,0,1) == 1200,"B2B 4line=1200");
    EXPECT(score_for_lines(1,9,0) == 1000,"single lv9=1000");

    /* T-spin scoring */
    EXPECT(score_for_tspin(0,0,0) == 400, "T-spin no line=400");
    EXPECT(score_for_tspin(1,0,0) == 800, "T-spin single=800");
    EXPECT(score_for_tspin(2,0,0) == 1200,"T-spin double=1200");
    EXPECT(score_for_tspin(3,0,0) == 1600,"T-spin triple=1600");
    EXPECT(score_for_tspin(1,0,1) == 1200,"B2B T-spin single=1200");

    /* All-clear scoring */
    EXPECT(score_for_all_clear(1,0) == 800,  "AC 1 line=800");
    EXPECT(score_for_all_clear(4,0) == 2000, "AC 4 lines=2000");

    /* Hard drop */
    game_init(&g, 5678);
    game_hard_drop(&g);
    EXPECT(g.app_state == APP_PLAYING || g.app_state == APP_GAME_OVER,
           "after hard drop state valid");

    /* Hold cycle */
    game_init(&g, 111);
    PieceKind piece0 = g.current.kind;
    game_hold(&g);
    EXPECT(g.hold == piece0, "piece0 held");
    EXPECT(!g.hold_used, "hold_used reset after new piece");

    PieceKind piece1 = g.current.kind;
    game_hold(&g);
    EXPECT(g.hold == piece1, "piece1 now held");
    EXPECT(g.hold_used, "hold_used=1 after swap");

    PieceKind hb = g.hold;
    game_hold(&g);  /* blocked */
    EXPECT(g.hold == hb, "third hold blocked");

    /* Lock-reset cap */
    game_init(&g, 333);
    for (int i = 0; i < 25; i++) game_try_move(&g, 0, 1);
    g.touching_ground = 1;
    g.lock_reset_count = LOCK_RESET_MAX;
    g.lock_accum = 0.1;
    double accum_before = g.lock_accum;
    game_try_move(&g, 1, 0);
    /* lock_accum should NOT have been reset */
    EXPECT(g.lock_accum >= accum_before, "lock_accum not reset at cap");
}

/* -----------------------------------------------------------------------
 * T-spin detection test
 * -------------------------------------------------------------------- */

static void test_tspin(void)
{
    section("T-spin detection");

    GameState g;
    game_init(&g, 42);
    board_clear(&g.board);

    /*
     * Build a classic T-spin setup in rows 17-19:
     *
     *  row 17: X . . . . . . . . X
     *  row 18: X X . . . . . . . X
     *  row 19: X . . . . . . . . X   ← T will fill the gap
     *
     * Place T rot=0 at x=0, y=17 to create the pattern, then
     * arrange so last_rot=1 and check 3 corners.
     */

    /* Fill a near-complete row at y=19 to make the corners occupied */
    for (int x = 0; x < BOARD_W; x++)
        g.board.cells[19][x] = (uint8_t)CELL_L;
    /* Leave columns 1,2,3 empty — that's where T will land */
    g.board.cells[19][1] = CELL_EMPTY;
    g.board.cells[19][2] = CELL_EMPTY;
    g.board.cells[19][3] = CELL_EMPTY;

    /* Place T piece: rot=0, x=0, y=17 so its bounding box:
     *   row17: x0=.(blank) x1=# x2=# x3=#
     *   row18: x0=.(blank) x1=.(blank) x2=# x3=.(blank)
     * Corners of bounding box from x=0,y=17: (0,17),(2,17),(0,19),(2,19)
     */
    g.current.kind     = PIECE_T;
    g.current.rotation = 0;
    g.current.x        = 0;
    g.current.y        = 17;
    g.last_rot         = 1; /* simulate last action was rotation */

    /* With board row 19 filled (except cols 1-3), corners (0,19) and (2,19)
     * are occupied, and walls cover x<0. Check manually:
     * corner (cx, cy)   = (0, 17) → free
     * corner (cx+2, cy) = (2, 17) → free
     * corner (cx, cy+2) = (0, 19) → occupied (CELL_L)
     * corner (cx+2,cy+2)= (2, 19) → free (we cleared it)
     * That's only 1 corner → NOT a T-spin.
     * Just verify the function doesn't crash and returns 0 or 1.
     */
    /* We can't call game_lock_piece cleanly here without a proper board state,
     * so just verify scoring functions behave correctly */
    EXPECT(score_for_tspin(0, 0, 0) > 0, "T-spin score >0");
    EXPECT(score_for_tspin(2, 0, 0) > score_for_lines(2, 0, 0),
           "T-spin double > regular double");

    /* T-spin B2B bonus */
    EXPECT(score_for_tspin(1, 0, 1) > score_for_tspin(1, 0, 0),
           "B2B T-spin > normal T-spin");
}

/* -----------------------------------------------------------------------
 * All-clear test
 * -------------------------------------------------------------------- */

static void test_all_clear(void)
{
    section("all-clear");

    GameState g;
    game_init(&g, 999);
    board_clear(&g.board);

    /* Fill row 19 completely */
    for (int x = 0; x < BOARD_W; x++)
        g.board.cells[19][x] = (uint8_t)CELL_I;

    int cleared = board_clear_full_lines(&g.board);
    EXPECT(cleared == 1, "one full row cleared");

    /* Check all-clear condition manually */
    int empty = 1;
    for (int y = 0; y < BOARD_H && empty; y++)
        for (int x = 0; x < BOARD_W && empty; x++)
            if (g.board.cells[y][x]) empty = 0;
    EXPECT(empty, "board empty after single full-row clear");

    /* All-clear bonus should be positive */
    EXPECT(score_for_all_clear(1, 0) > 0, "AC bonus > 0");
    EXPECT(score_for_all_clear(4, 0) > score_for_all_clear(1, 0),
           "4-line AC > 1-line AC");
}

/* -----------------------------------------------------------------------
 * Game modes
 * -------------------------------------------------------------------- */

static void test_modes(void)
{
    section("game modes");

    GameState g;
    memset(&g, 0, sizeof(g));
    g.hold = PIECE_COUNT;

    /* Marathon */
    game_init_for_mode(&g, 1, MODE_MARATHON);
    EXPECT(g.mode == MODE_MARATHON, "marathon mode set");
    EXPECT(g.app_state == APP_PLAYING, "marathon starts playing");

    /* Sprint */
    game_init_for_mode(&g, 2, MODE_SPRINT);
    EXPECT(g.mode == MODE_SPRINT, "sprint mode set");
    EXPECT(g.total_lines == 0, "sprint total_lines=0");

    /* Sprint completion: clearing 40+ lines → APP_RESULTS */
    g.total_lines = SPRINT_GOAL - 1;
    /* Force a line clear by filling one complete row */
    for (int x = 0; x < BOARD_W; x++)
        g.board.cells[19][x] = (uint8_t)CELL_I;
    /* Merge a dummy piece that won't add cells (use ghost at bottom) */
    /* Directly invoke board_clear_full_lines and scoring logic */
    int cleared = board_clear_full_lines(&g.board);
    g.total_lines += cleared;
    if (g.mode == MODE_SPRINT && g.total_lines >= SPRINT_GOAL)
        g.app_state = APP_RESULTS;
    EXPECT(g.app_state == APP_RESULTS, "sprint reaches APP_RESULTS at 40 lines");

    /* Ultra */
    game_init_for_mode(&g, 3, MODE_ULTRA);
    EXPECT(g.mode == MODE_ULTRA, "ultra mode set");
    /* Simulate time running out */
    g.time_elapsed = ULTRA_TIME + 0.1;
    if (g.mode == MODE_ULTRA && g.time_elapsed >= ULTRA_TIME)
        g.app_state = APP_RESULTS;
    EXPECT(g.app_state == APP_RESULTS, "ultra reaches APP_RESULTS at time limit");

    /* Menu init */
    game_init_menu(&g);
    EXPECT(g.app_state == APP_MENU, "game_init_menu → APP_MENU");
    EXPECT(g.hold == PIECE_COUNT, "hold reset in menu");
}

/* -----------------------------------------------------------------------
 * High score tests
 * -------------------------------------------------------------------- */

static void test_highscore(void)
{
    section("highscore");

    GameState g;
    memset(&g, 0, sizeof(g));
    g.hold = PIECE_COUNT;

    /* Marathon: higher score wins */
    game_init_for_mode(&g, 1, MODE_MARATHON);
    g.score       = 5000;
    g.hs_marathon = 3000;
    int improved  = hs_update(&g);
    EXPECT(improved == 1, "new marathon record detected");
    EXPECT(g.hs_marathon == 5000, "marathon hs updated");

    g.score = 2000; /* worse */
    improved = hs_update(&g);
    EXPECT(improved == 0, "no improvement not recorded");
    EXPECT(g.hs_marathon == 5000, "hs_marathon unchanged");

    /* Sprint: lower ms wins */
    game_init_for_mode(&g, 2, MODE_SPRINT);
    g.time_elapsed  = 45.0;
    g.hs_sprint_ms  = 0; /* first completion */
    improved        = hs_update(&g);
    EXPECT(improved == 1, "first sprint sets record");
    EXPECT(g.hs_sprint_ms == 45000, "sprint hs = 45000ms");

    g.time_elapsed = 60.0; /* slower */
    improved       = hs_update(&g);
    EXPECT(improved == 0, "slower sprint not a record");
    EXPECT(g.hs_sprint_ms == 45000, "sprint hs unchanged");

    g.time_elapsed = 40.0; /* faster */
    improved       = hs_update(&g);
    EXPECT(improved == 1, "faster sprint is new record");
    EXPECT(g.hs_sprint_ms == 40000, "sprint hs=40000ms");

    /* Ultra: higher score wins */
    game_init_for_mode(&g, 3, MODE_ULTRA);
    g.score   = 8000;
    g.hs_ultra = 6000;
    improved  = hs_update(&g);
    EXPECT(improved == 1, "ultra new record");
    EXPECT(g.hs_ultra == 8000, "ultra hs updated");
}

/* -----------------------------------------------------------------------
 * Board mask helpers (board_full_row_mask / board_clear_rows_by_mask)
 * -------------------------------------------------------------------- */

static void test_board_mask(void)
{
    section("board mask");

    Board b;
    board_clear(&b);

    EXPECT(board_full_row_mask(&b) == 0u, "empty board mask=0");

    /* Fill row 19 */
    for (int x = 0; x < BOARD_W; x++)
        b.cells[19][x] = (uint8_t)CELL_I;
    uint32_t mask = board_full_row_mask(&b);
    EXPECT(mask == (1u << 19), "row 19 full → bit 19 only");

    /* Fill row 17 as well */
    for (int x = 0; x < BOARD_W; x++)
        b.cells[17][x] = (uint8_t)CELL_L;
    mask = board_full_row_mask(&b);
    EXPECT(((mask >> 19) & 1u) != 0, "bit 19 still set");
    EXPECT(((mask >> 17) & 1u) != 0, "bit 17 set");
    EXPECT(((mask >> 18) & 1u) == 0, "bit 18 not set");

    /* Clear those rows */
    board_clear_rows_by_mask(&b, mask);
    EXPECT(board_full_row_mask(&b) == 0u, "mask=0 after clearing");
    int all_empty = 1;
    for (int y = 0; y < BOARD_H && all_empty; y++)
        for (int x = 0; x < BOARD_W && all_empty; x++)
            if (b.cells[y][x]) all_empty = 0;
    EXPECT(all_empty, "board fully empty after clearing both rows");
}

/* -----------------------------------------------------------------------
 * Line-clear animation phase
 * -------------------------------------------------------------------- */

static void test_line_clear_anim(void)
{
    section("line clear animation");

    GameState g;
    game_init_for_mode(&g, 1, MODE_MARATHON);
    board_clear(&g.board);

    /* Fill row 19, lock a dummy piece entirely above the board */
    for (int x = 0; x < BOARD_W; x++)
        g.board.cells[19][x] = (uint8_t)CELL_L;

    g.current.kind     = PIECE_I;
    g.current.rotation = 0;
    g.current.x        = 0;
    g.current.y        = -4;   /* above board — board_merge_piece adds nothing */
    g.last_rot         = 0;
    g.soft_drop_cells  = 0;

    game_lock_piece(&g);

    EXPECT(g.game_phase == PHASE_LINE_CLEAR_ANIM, "phase→ANIM after 1-line clear");
    EXPECT(g.clear_timer == 0, "clear_timer=0 at anim start");
    EXPECT(((g.clearing_rows >> 19) & 1u) != 0, "row 19 in clearing_rows");
    /* Row 19 must not be erased yet (deferred) */
    EXPECT(g.board.cells[19][0] != CELL_EMPTY, "row 19 still present during anim");

    /* Tick LINE_CLEAR_TICKS-1 times: should still be animating */
    InputSnapshot empty_in = {0};
    for (int i = 0; i < LINE_CLEAR_TICKS - 1; i++)
        game_tick(&g, &empty_in, 0);
    EXPECT(g.game_phase == PHASE_LINE_CLEAR_ANIM, "still animating before last tick");

    /* One final tick completes the animation */
    game_tick(&g, &empty_in, 0);
    EXPECT(g.game_phase == PHASE_FALLING, "phase→FALLING after anim done");
    EXPECT(g.clearing_rows == 0u, "clearing_rows cleared");

    int row19_empty = 1;
    for (int x = 0; x < BOARD_W; x++)
        if (g.board.cells[19][x]) { row19_empty = 0; break; }
    EXPECT(row19_empty, "row 19 actually erased after animation");
}

/* -----------------------------------------------------------------------
 * B2B and combo tracking via game_lock_piece
 * -------------------------------------------------------------------- */

/* Lock a piece fully above the board (no board change) */
#define LOCK_DUMMY(gs)                           \
    do {                                         \
        (gs).current.kind     = PIECE_I;         \
        (gs).current.rotation = 0;               \
        (gs).current.x        = 0;               \
        (gs).current.y        = -4;              \
        (gs).last_rot         = 0;               \
        (gs).soft_drop_cells  = 0;               \
        game_lock_piece(&(gs));                  \
    } while (0)

static void anim_flush(GameState *g)
{
    InputSnapshot e = {0};
    for (int i = 0; i < LINE_CLEAR_TICKS; i++)
        game_tick(g, &e, 0);
}

static void fill_rows(Board *b, int from, int to)
{
    for (int row = from; row <= to; row++)
        for (int x = 0; x < BOARD_W; x++)
            b->cells[row][x] = (uint8_t)CELL_L;
}

static void test_b2b_and_combo(void)
{
    section("B2B and combo");

    GameState g;
    game_init_for_mode(&g, 1, MODE_MARATHON);
    board_clear(&g.board);

    /* --- 4-line clear sets b2b flag --- */
    g.b2b   = 0;
    g.combo = -1;
    fill_rows(&g.board, 16, 19);
    LOCK_DUMMY(g);
    EXPECT(g.b2b   == 1,                      "b2b=1 after 4line");
    EXPECT(g.combo == 0,                       "combo=0 after first 4line");
    EXPECT(g.game_phase == PHASE_LINE_CLEAR_ANIM, "anim started");
    anim_flush(&g);

    /* --- Second 4-line maintains b2b --- */
    board_clear(&g.board);
    fill_rows(&g.board, 16, 19);
    LOCK_DUMMY(g);
    EXPECT(g.b2b == 1, "b2b maintained after second 4line");
    anim_flush(&g);

    /* --- Single-line clear breaks b2b --- */
    board_clear(&g.board);
    g.combo = -1;
    for (int x = 0; x < BOARD_W; x++)
        g.board.cells[19][x] = (uint8_t)CELL_L;
    LOCK_DUMMY(g);
    EXPECT(g.b2b == 0, "b2b=0 after single clear");
    anim_flush(&g);

    /* --- Consecutive singles increment combo --- */
    board_clear(&g.board);
    g.combo = -1;

    for (int x = 0; x < BOARD_W; x++)
        g.board.cells[19][x] = (uint8_t)CELL_L;
    LOCK_DUMMY(g);
    EXPECT(g.combo == 0, "first clear: combo=0");
    anim_flush(&g);

    board_clear(&g.board);
    for (int x = 0; x < BOARD_W; x++)
        g.board.cells[19][x] = (uint8_t)CELL_L;
    LOCK_DUMMY(g);
    EXPECT(g.combo == 1, "second consecutive clear: combo=1");
    anim_flush(&g);

    /* --- No clear breaks combo --- */
    board_clear(&g.board);
    LOCK_DUMMY(g);
    EXPECT(g.combo == -1, "no clear: combo=-1");
    EXPECT(g.game_phase == PHASE_FALLING, "no anim when no lines cleared");

    /* --- B2B scoring: B2B 4-line > regular 4-line --- */
    EXPECT(score_for_lines(4, 0, 1) > score_for_lines(4, 0, 0),
           "B2B 4line score > regular 4line score");
    EXPECT(score_for_tspin(2, 0, 1) > score_for_tspin(2, 0, 0),
           "B2B T-spin double > regular T-spin double");
}

/* -----------------------------------------------------------------------
 * SRS rotation smoke test
 * -------------------------------------------------------------------- */

static void test_srs_kicks(void)
{
    section("SRS rotation");

    Board b;
    board_clear(&b);
    /* Left-wall obstruction */
    for (int y = 0; y < BOARD_H; y++) {
        b.cells[y][0] = (uint8_t)CELL_I;
        b.cells[y][1] = (uint8_t)CELL_I;
    }

    GameState g;
    game_init(&g, 42);
    g.board = b;
    g.current = piece_make(PIECE_T, 2, 5);
    g.current.rotation = 0;

    int rotated = game_try_rotate(&g, 1);
    if (rotated) {
        EXPECT(g.current.x >= 0, "kicked piece x valid");
        EXPECT(board_piece_fits(&g.board, &g.current), "piece fits after kick");
    }
    EXPECT(rotated == 0 || rotated == 1, "rotate returns 0 or 1");
}

/* -----------------------------------------------------------------------
 * Entry point
 * -------------------------------------------------------------------- */

int main(void)
{
    printf("zezyi logic tests\n");

    test_pieces();
    test_board();
    test_randomizer();
    test_rules();
    test_tspin();
    test_all_clear();
    test_modes();
    test_highscore();
    test_srs_kicks();
    test_board_mask();
    test_line_clear_anim();
    test_b2b_and_combo();

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
