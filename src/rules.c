#include <stddef.h>
#include <string.h>
#include "rules.h"
#include "board.h"
#include "piece.h"
#include "randomizer.h"
#include "highscore.h"

/* -------------------------------------------------------------------------
 * Gravity
 *
 * Marathon / Sprint / Ultra / Blitz / Cheese: level every 10 lines
 * Survival: level every 3 lines (much faster speed ramp)
 * Zen: fixed speed level 0 (relaxing)
 * ---------------------------------------------------------------------- */

static double fall_interval(int level, GameMode mode)
{
    if (mode == MODE_ZEN) return 1.0; /* constant slow gravity */

    double interval = 1.0 - (double)level * 0.08;
    if (interval < 0.05) interval = 0.05;
    return interval;
}

static int level_for_mode(int total_lines, GameMode mode)
{
    if (mode == MODE_SURVIVAL) return total_lines / 3;
    return total_lines / 10;
}

/* -------------------------------------------------------------------------
 * Ghost
 * ---------------------------------------------------------------------- */

static void recalc_ghost(GameState *g)
{
    g->ghost = g->current;
    for (;;) {
        ActivePiece below = g->ghost;
        below.y++;
        if (!board_piece_fits(&g->board, &below))
            break;
        g->ghost = below;
    }
}

/* -------------------------------------------------------------------------
 * SRS wall-kick tables  (screen coords: y+ = down)
 *
 * CW  uses  kick[from_rot]
 * CCW uses  negated kick[to_rot]  ( = negated kick[(from_rot+3)%4] )
 * ---------------------------------------------------------------------- */

static const int KICKS_JLSTZ[4][5][2] = {
    {{ 0, 0},{-1, 0},{-1,-1},{ 0,+2},{-1,+2}}, /* 0→R */
    {{ 0, 0},{+1, 0},{+1,+1},{ 0,-2},{+1,-2}}, /* R→2 */
    {{ 0, 0},{+1, 0},{+1,-1},{ 0,+2},{+1,+2}}, /* 2→L */
    {{ 0, 0},{-1, 0},{-1,+1},{ 0,-2},{-1,-2}}, /* L→0 */
};

static const int KICKS_I[4][5][2] = {
    {{ 0, 0},{-2, 0},{+1, 0},{-2,+1},{+1,-2}}, /* 0→R */
    {{ 0, 0},{-1, 0},{+2, 0},{-1,-2},{+2,+1}}, /* R→2 */
    {{ 0, 0},{+2, 0},{-1, 0},{+2,-1},{-1,+2}}, /* 2→L */
    {{ 0, 0},{+1, 0},{-2, 0},{+1,+2},{-2,-1}}, /* L→0 */
};

/* -------------------------------------------------------------------------
 * Helpers
 * ---------------------------------------------------------------------- */

static int count_bits(uint32_t mask)
{
    int n = 0;
    while (mask) { n += (int)(mask & 1u); mask >>= 1; }
    return n;
}

/* -------------------------------------------------------------------------
 * Scoring
 * ---------------------------------------------------------------------- */

int score_for_lines(int n, int level, int b2b)
{
    int base;
    switch (n) {
        case 1: base = 100;                      break;
        case 2: base = 300;                      break;
        case 3: base = 500;                      break;
        case 4: base = b2b ? 1200 : 800;         break;
        default: return 0;
    }
    return base * (level + 1);
}

int score_for_tspin(int lines, int level, int b2b)
{
    static const int BASE[4] = {400, 800, 1200, 1600};
    if (lines < 0 || lines > 3) return 0;
    int base = BASE[lines];
    if (b2b && lines > 0) base = base * 3 / 2;
    return base * (level + 1);
}

int score_for_all_clear(int cleared, int level)
{
    int base;
    switch (cleared) {
        case 1: base = 800;  break;
        case 2: base = 1200; break;
        case 3: base = 1800; break;
        case 4: base = 2000; break;
        default: return 0;
    }
    return base * (level + 1);
}

/* -------------------------------------------------------------------------
 * T-spin detection (3-corner rule)
 * ---------------------------------------------------------------------- */

static int detect_tspin(const GameState *g)
{
    if (g->current.kind != PIECE_T) return 0;
    if (!g->last_rot)               return 0;

    int cx = g->current.x;
    int cy = g->current.y;
    int corners = 0;
    corners += board_cell_occupied(&g->board, cx,     cy)     ? 1 : 0;
    corners += board_cell_occupied(&g->board, cx + 2, cy)     ? 1 : 0;
    corners += board_cell_occupied(&g->board, cx,     cy + 2) ? 1 : 0;
    corners += board_cell_occupied(&g->board, cx + 2, cy + 2) ? 1 : 0;
    return corners >= 3;
}

/* -------------------------------------------------------------------------
 * Spawn
 * ---------------------------------------------------------------------- */

void game_spawn_next(GameState *g, const InputSnapshot *in)
{
    PieceKind kind = queue_pop(&g->queue);
    g->current     = piece_make(kind, -1, -1);

    g->hold_used        = 0;
    g->gravity_accum    = 0.0;
    g->lock_accum       = 0.0;
    g->touching_ground  = 0;
    g->lock_reset_count = 0;
    g->soft_drop_cells  = 0;
    g->hard_drop_cells  = 0;
    g->last_rot         = 0;

    /* IHS */
    if (g->pending_ihs && !g->hold_used) {
        g->pending_ihs = 0;
        game_hold(g);
        return;
    }
    g->pending_ihs = 0;

    /* IRS */
    if (g->pending_irs != 0) {
        int dir = g->pending_irs;
        g->pending_irs = 0;
        ActivePiece rotated = piece_rotated(&g->current, dir);
        if (board_piece_fits(&g->board, &rotated))
            g->current = rotated;
    }

    /* Buffer new IRS/IHS */
    if (in) {
        if (in->hold)
            g->pending_ihs = 1;
        else if (in->rotate_cw)
            g->pending_irs = 1;
        else if (in->rotate_ccw)
            g->pending_irs = -1;
    }

    if (!board_piece_fits(&g->board, &g->current)) {
        if (g->mode == MODE_ZEN) {
            /* Zen: clear board instead of game over */
            board_clear(&g->board);
        } else {
            g->game_over = 1;
            g->app_state = APP_GAME_OVER;
            g->audio_events |= AUDIO_EVT_GAME_OVER;
            return;
        }
    }

    /* Tip: suggest hold when it's never been used after 5 pieces */
    {
        int total_placed = 0;
        for (int i = 0; i < 7; i++) total_placed += g->stat_pieces[i];
        if (total_placed == 5 && g->hold == PIECE_COUNT &&
            !(g->tip_shown & (1u << TIP_TRY_HOLD))) {
            g->tip_id = TIP_TRY_HOLD;
            g->tip_shown |= (1u << TIP_TRY_HOLD);
        }
    }

    recalc_ghost(g);
}

/* -------------------------------------------------------------------------
 * Movement and rotation
 * ---------------------------------------------------------------------- */

int game_try_move(GameState *g, int dx, int dy)
{
    ActivePiece moved = g->current;
    moved.x += dx;
    moved.y += dy;
    if (!board_piece_fits(&g->board, &moved))
        return 0;
    g->current = moved;
    g->last_rot = 0;
    recalc_ghost(g);
    return 1;
}

int game_try_rotate(GameState *g, int dir)
{
    int from_rot = g->current.rotation;
    ActivePiece rotated = piece_rotated(&g->current, dir);

    const int (*kicks)[5][2] =
        (g->current.kind == PIECE_I) ? KICKS_I : KICKS_JLSTZ;

    int kick_set = (dir == 1) ? from_rot : rotated.rotation;
    int sign     = (dir == 1) ? 1 : -1;

    for (int i = 0; i < 5; i++) {
        ActivePiece candidate = rotated;
        candidate.x += sign * kicks[kick_set][i][0];
        candidate.y += sign * kicks[kick_set][i][1];
        if (board_piece_fits(&g->board, &candidate)) {
            g->current  = candidate;
            g->last_rot = 1;
            recalc_ghost(g);
            return 1;
        }
    }

    if (g->current.kind == PIECE_O &&
        board_piece_fits(&g->board, &rotated)) {
        g->current  = rotated;
        g->last_rot = 1;
        recalc_ghost(g);
        return 1;
    }

    return 0;
}

void game_hard_drop(GameState *g)
{
    int cells = g->ghost.y - g->current.y;
    if (g->mode != MODE_ZEN) g->score += 2 * cells;
    g->audio_events |= AUDIO_EVT_HARD_DROP;
    g->current = g->ghost;
    game_lock_piece(g);
}

void game_hold(GameState *g)
{
    if (g->hold_used) return;
    g->audio_events |= AUDIO_EVT_HOLD;

    PieceKind prev = g->hold;
    g->hold      = g->current.kind;
    g->hold_used = 1;

    if (prev == PIECE_COUNT) {
        game_spawn_next(g, NULL);
    } else {
        g->current         = piece_make(prev, -1, -1);
        g->gravity_accum   = 0.0;
        g->lock_accum      = 0.0;
        g->touching_ground = 0;
        g->lock_reset_count = 0;
        g->last_rot        = 0;
        recalc_ghost(g);
    }
}

/* -------------------------------------------------------------------------
 * Lock, clear, score
 * ---------------------------------------------------------------------- */

void game_lock_piece(GameState *g)
{
    g->tspin = detect_tspin(g);

    /* Track piece placed */
    if (g->current.kind < PIECE_COUNT)
        g->stat_pieces[(int)g->current.kind]++;

    board_merge_piece(&g->board, &g->current);

    uint32_t mask = board_full_row_mask(&g->board);
    int clearing_count = count_bits(mask);

    g->all_clear = 0;
    if (clearing_count > 0) {
        int will_clear = 1;
        for (int y = 0; y < BOARD_H && will_clear; y++) {
            if ((mask >> y) & 1u) continue;
            for (int x = 0; x < BOARD_W; x++)
                if (g->board.cells[y][x]) { will_clear = 0; break; }
        }
        g->all_clear = will_clear;
    }

    g->combo = (clearing_count > 0) ? g->combo + 1 : -1;

    int is_btb    = (clearing_count == 4) || (g->tspin && clearing_count > 0);
    int b2b_bonus = (is_btb && g->b2b) ? 1 : 0;
    if (clearing_count > 0)
        g->b2b = is_btb ? 1 : 0;

    /* Zen: no scoring */
    if (g->mode != MODE_ZEN) {
        g->score += g->soft_drop_cells;

        if (g->tspin)
            g->score += score_for_tspin(clearing_count, g->level, b2b_bonus);
        else
            g->score += score_for_lines(clearing_count, g->level, b2b_bonus);

        if (g->combo > 0)
            g->score += 50 * g->combo * (g->level + 1);

        if (g->all_clear)
            g->score += score_for_all_clear(clearing_count, g->level);
    }

    int level_before    = g->level;
    g->total_lines     += clearing_count;
    g->level            = level_for_mode(g->total_lines, g->mode);

    /* Statistics */
    if (clearing_count == 4) g->stat_quads++;
    if (g->tspin && clearing_count > 0) g->stat_tspins++;
    if (g->combo > g->stat_max_combo) g->stat_max_combo = g->combo;

    /* Tips */
    g->tip_id = TIP_NONE;
    /* Danger: stack reached top 5 rows */
    {
        int highest = BOARD_H;
        for (int y = 0; y < BOARD_H && highest == BOARD_H; y++)
            for (int x = 0; x < BOARD_W; x++)
                if (g->board.cells[y][x] != CELL_EMPTY) { highest = y; break; }
        if (highest < 5 && !(g->tip_shown & (1u << TIP_DANGER))) {
            g->tip_id = TIP_DANGER;
            g->tip_shown |= (1u << TIP_DANGER);
        }
    }
    /* First 4-line clear */
    if (clearing_count == 4 && g->stat_quads == 1 &&
        !(g->tip_shown & (1u << TIP_FIRST_QUAD))) {
        g->tip_id = TIP_FIRST_QUAD;
        g->tip_shown |= (1u << TIP_FIRST_QUAD);
    }
    /* Combo fire */
    if (g->combo >= 4 && !(g->tip_shown & (1u << TIP_COMBO_FIRE))) {
        g->tip_id = TIP_COMBO_FIRE;
        g->tip_shown |= (1u << TIP_COMBO_FIRE);
    }

    g->audio_events |= AUDIO_EVT_LOCK;
    if      (clearing_count == 1) g->audio_events |= AUDIO_EVT_SINGLE;
    else if (clearing_count == 2) g->audio_events |= AUDIO_EVT_DOUBLE;
    else if (clearing_count == 3) g->audio_events |= AUDIO_EVT_TRIPLE;
    else if (clearing_count == 4) g->audio_events |= AUDIO_EVT_QUAD;
    if (g->tspin && clearing_count > 0) g->audio_events |= AUDIO_EVT_TSPIN;
    if (b2b_bonus)                      g->audio_events |= AUDIO_EVT_B2B;
    if (g->all_clear)                   g->audio_events |= AUDIO_EVT_ALL_CLEAR;
    if (g->level > level_before)        g->audio_events |= AUDIO_EVT_LEVEL_UP;

    if (clearing_count > 0) {
        g->clearing_rows = mask;
        g->clear_timer   = 0;
        g->game_phase    = PHASE_LINE_CLEAR_ANIM;
    } else {
        game_spawn_next(g, NULL);
    }
}

/* -------------------------------------------------------------------------
 * game_init
 * ---------------------------------------------------------------------- */

static void game_reset(GameState *g, uint32_t seed)
{
    board_clear(&g->board);
    queue_init(&g->queue, seed);

    g->hold             = PIECE_COUNT;
    g->hold_used        = 0;
    g->tick             = 0;
    g->score            = 0;
    g->level            = 0;
    g->total_lines      = 0;
    g->combo            = -1;
    g->b2b              = 0;
    g->tspin            = 0;
    g->all_clear        = 0;
    g->gravity_accum    = 0.0;
    g->lock_accum       = 0.0;
    g->time_elapsed     = 0.0;
    g->touching_ground  = 0;
    g->lock_reset_count = 0;
    g->last_rot         = 0;
    g->soft_drop_cells  = 0;
    g->hard_drop_cells  = 0;
    g->pending_irs      = 0;
    g->pending_ihs      = 0;
    g->game_over        = 0;
    g->paused           = 0;
    g->app_state        = APP_PLAYING;
    g->game_phase       = PHASE_FALLING;
    g->clear_timer      = 0;
    g->clearing_rows    = 0;

    /* Stats */
    for (int i = 0; i < 7; i++) g->stat_pieces[i] = 0;
    g->stat_quads     = 0;
    g->stat_tspins    = 0;
    g->stat_max_combo = 0;
    g->stat_actions   = 0;

    /* Tips */
    g->tip_id   = TIP_NONE;
    g->tip_shown = 0;
}

/* Preserve all persistent data across a game reset */
#define PRESERVE_PERSISTENT(g, block) \
    do { \
        int _hm  = (g)->hs_marathon;  \
        int _hs  = (g)->hs_sprint_ms; \
        int _hu  = (g)->hs_ultra;     \
        int _hb  = (g)->hs_blitz;     \
        int _hsv = (g)->hs_survival;  \
        int _hc  = (g)->hs_cheese_ms; \
        int _cur = (g)->menu_cursor;  \
        char _pn[32]; \
        __builtin_memcpy(_pn, (g)->player_name, sizeof(_pn)); \
        block; \
        (g)->hs_marathon  = _hm;  \
        (g)->hs_sprint_ms = _hs;  \
        (g)->hs_ultra     = _hu;  \
        (g)->hs_blitz     = _hb;  \
        (g)->hs_survival  = _hsv; \
        (g)->hs_cheese_ms = _hc;  \
        (g)->menu_cursor  = _cur; \
        __builtin_memcpy((g)->player_name, _pn, sizeof(_pn)); \
    } while(0)

void game_init(GameState *g, uint32_t seed)
{
    PRESERVE_PERSISTENT(g, game_reset(g, seed));
    g->mode = MODE_MARATHON;
    game_spawn_next(g, NULL);
}

void game_init_for_mode(GameState *g, uint32_t seed, GameMode mode)
{
    PRESERVE_PERSISTENT(g, game_reset(g, seed));
    g->mode = mode;

    /* Cheese: fill board with garbage rows */
    if (mode == MODE_CHEESE)
        board_add_garbage(&g->board, CHEESE_LINES, seed ^ 0xDEAD);

    game_spawn_next(g, NULL);
}

void game_init_menu(GameState *g)
{
    char pn[32];
    __builtin_memcpy(pn, g->player_name, sizeof(pn));

    memset(g, 0, sizeof(*g));
    g->hold        = PIECE_COUNT;
    g->combo       = -1;
    g->app_state   = APP_MENU;
    g->menu_cursor = 0;

    __builtin_memcpy(g->player_name, pn, sizeof(pn));
    hs_load(g);
}

/* -------------------------------------------------------------------------
 * game_tick helpers
 * ---------------------------------------------------------------------- */

static void process_movement(GameState *g, const InputSnapshot *in)
{
    if (in->left) {
        if (game_try_move(g, -1, 0)) {
            g->audio_events |= AUDIO_EVT_MOVE;
            if (g->touching_ground && g->lock_reset_count < LOCK_RESET_MAX) {
                g->lock_accum = 0.0;
                g->lock_reset_count++;
            }
        } else {
            g->audio_events |= AUDIO_EVT_WALL_HIT;
        }
    }
    if (in->right) {
        if (game_try_move(g, 1, 0)) {
            g->audio_events |= AUDIO_EVT_MOVE;
            if (g->touching_ground && g->lock_reset_count < LOCK_RESET_MAX) {
                g->lock_accum = 0.0;
                g->lock_reset_count++;
            }
        } else {
            g->audio_events |= AUDIO_EVT_WALL_HIT;
        }
    }
}

static void process_rotation(GameState *g, const InputSnapshot *in)
{
    int rotated = 0;
    if (in->rotate_cw)        rotated = game_try_rotate(g, 1);
    else if (in->rotate_ccw)  rotated = game_try_rotate(g, -1);

    if (rotated) {
        g->audio_events |= AUDIO_EVT_ROTATE;
        if (g->touching_ground && g->lock_reset_count < LOCK_RESET_MAX) {
            g->lock_accum = 0.0;
            g->lock_reset_count++;
        }
    }
}

static void apply_gravity(GameState *g)
{
    g->gravity_accum += 1.0 / 60.0;
    double interval = fall_interval(g->level, g->mode);
    while (g->gravity_accum >= interval) {
        if (!game_try_move(g, 0, 1)) {
            g->touching_ground = 1;
            g->gravity_accum   = 0.0;
            break;
        }
        g->gravity_accum -= interval;
    }
}

static void apply_lock_delay(GameState *g)
{
    g->lock_accum += 1.0 / 60.0;
    if (g->lock_accum >= LOCK_DELAY)
        game_lock_piece(g);
}

static void check_ground_state(GameState *g)
{
    ActivePiece below = g->current;
    below.y++;
    if (board_piece_fits(&g->board, &below))
        g->touching_ground = 0;
}

/* -------------------------------------------------------------------------
 * game_tick
 * ---------------------------------------------------------------------- */

void game_tick(GameState *g, const InputSnapshot *in, double dt)
{
    (void)dt;

    if (g->app_state != APP_PLAYING)
        return;

    g->tick++;
    g->time_elapsed += 1.0 / 60.0;

    /* Count inputs for APM */
    if (in) {
        if (in->left)                         g->stat_actions++;
        if (in->right)                        g->stat_actions++;
        if (in->rotate_cw || in->rotate_ccw)  g->stat_actions++;
        if (in->hard_drop)                    g->stat_actions++;
        if (in->hold)                         g->stat_actions++;
        if (in->down)                         g->stat_actions++;
    }

    /* Timed-end modes */
    if (g->mode == MODE_ULTRA && g->time_elapsed >= ULTRA_TIME) {
        (void)hs_update(g);
        hs_save(g);
        g->app_state = APP_RESULTS;
        return;
    }
    if (g->mode == MODE_BLITZ && g->time_elapsed >= BLITZ_TIME) {
        (void)hs_update(g);
        hs_save(g);
        g->app_state = APP_RESULTS;
        return;
    }

    /* Line-clear animation */
    if (g->game_phase == PHASE_LINE_CLEAR_ANIM) {
        g->clear_timer++;
        if (g->clear_timer >= LINE_CLEAR_TICKS) {
            board_clear_rows_by_mask(&g->board, g->clearing_rows);
            g->clearing_rows = 0;
            g->game_phase    = PHASE_FALLING;

            /* Sprint / Cheese completion */
            if (g->mode == MODE_SPRINT && g->total_lines >= SPRINT_GOAL) {
                (void)hs_update(g);
                hs_save(g);
                g->app_state = APP_RESULTS;
                return;
            }
            if (g->mode == MODE_CHEESE && g->total_lines >= CHEESE_LINES) {
                (void)hs_update(g);
                hs_save(g);
                g->app_state = APP_RESULTS;
                return;
            }

            game_spawn_next(g, in);
        }
        return;
    }

    /* Normal PHASE_FALLING gameplay */
    process_movement(g, in);
    if (g->app_state != APP_PLAYING) return;

    process_rotation(g, in);
    if (g->app_state != APP_PLAYING) return;

    if (in->hold)
        game_hold(g);
    if (g->app_state != APP_PLAYING) return;

    if (in->hard_drop) {
        game_hard_drop(g);
        return;
    }

    if (in->down) {
        if (game_try_move(g, 0, 1)) {
            if (g->mode != MODE_ZEN) g->soft_drop_cells++;
            g->gravity_accum    = 0.0;
            g->audio_events    |= AUDIO_EVT_SOFT_DROP;
        }
    }

    if (!g->touching_ground)
        apply_gravity(g);
    else
        apply_lock_delay(g);
    if (g->app_state != APP_PLAYING) return;

    check_ground_state(g);
}
