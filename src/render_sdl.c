#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "render_sdl.h"
#include "piece.h"
#include "randomizer.h"
#include "input.h"
#include "i18n.h"

/* -------------------------------------------------------------------------
 * UI palette (HUD)
 * ---------------------------------------------------------------------- */

#define COLOR_LABEL    (SDL_Color){150, 150, 150, 255} /* labels */
#define COLOR_VALUE    (SDL_Color){255, 255, 255, 255} /* values */
#define COLOR_ACCENT   (SDL_Color){100, 220, 255, 255} /* highlights */
#define COLOR_DANGER   (SDL_Color){255,  80,  80, 255} /* alerts */
#define COLOR_HOLD_BG  (SDL_Color){ 20,  20,  35, 255} /* hold box bg */
#define COLOR_DIVIDER  (SDL_Color){ 50,  50,  70, 255} /* separators */

/* Module state */
static Layout s_layout;
static int    opt_ghost      = 1;
static int    opt_grid_lines = 0;

/* Tip display state — updated each frame from GameState */
static TipId    s_tip_active  = TIP_NONE;
static uint32_t s_tip_expire  = 0; /* SDL_GetTicks() target */

/* Piece colours (indexed by Cell enum 0..8) */
static const SDL_Color PIECE_COLORS[9] = {
    {  30,  30,  30, 255 }, /* CELL_EMPTY   */
    {   0, 230, 230, 255 }, /* I – cyan      */
    { 230, 230,   0, 255 }, /* O – yellow    */
    { 160,   0, 230, 255 }, /* T – purple    */
    {   0, 200,   0, 255 }, /* S – green     */
    { 220,   0,   0, 255 }, /* Z – red       */
    {  30,  80, 220, 255 }, /* J – blue      */
    { 220, 140,   0, 255 }, /* L – orange    */
    { 100, 100, 100, 255 }, /* GARBAGE – grey*/
};

/* -------------------------------------------------------------------------
 * Layout computation
 * ---------------------------------------------------------------------- */

void layout_compute(Layout *l, int sw, int sh)
{
    int cell_from_h = (sh * 88 / 100) / BOARD_H;
    int cell_from_w = (sw * 58 / 100) / BOARD_W;
    l->cell_px = (cell_from_h < cell_from_w) ? cell_from_h : cell_from_w;
    if (l->cell_px < 12) l->cell_px = 12;

    int bw = l->cell_px * BOARD_W;
    int bh = l->cell_px * BOARD_H;
    l->board = (SDL_Rect){(sw - bw) / 2, (sh - bh) / 2, bw, bh};

    l->pad = SDL_max(l->cell_px / 5, 3);

    l->left_panel  = (SDL_Rect){0, l->board.y, l->board.x, bh};
    l->right_panel = (SDL_Rect){l->board.x + bw, l->board.y,
                                sw - (l->board.x + bw), bh};

    int rp_budget = (l->right_panel.w - l->pad * 4) / 4;
    l->prev_cell = SDL_min(l->cell_px * 70 / 100, rp_budget);
    if (l->prev_cell < 6) l->prev_cell = 6;

    /* Bitmap font scales (increase a bit for readability) */
    l->font_xl = SDL_max(l->cell_px / 6,  4);
    l->font_lg = SDL_max(l->cell_px / 8,  3);
    l->font_md = SDL_max(l->cell_px / 11, 2);
    l->font_sm = SDL_max(l->cell_px / 16, 1);

    l->screen_w = sw;
    l->screen_h = sh;
}

void render_set_layout(const Layout *l)
{
    s_layout = *l;
}

/* -------------------------------------------------------------------------
 * Low-level draw helpers
 * ---------------------------------------------------------------------- */

static void set_color(SDL_Renderer *r, SDL_Color c)
{
    SDL_SetRenderDrawColor(r, c.r, c.g, c.b, c.a);
}

static void draw_cell(SDL_Renderer *r, int sx, int sy, SDL_Color color, int ghost)
{
    int cp = s_layout.cell_px;
    SDL_Rect rect = { sx + 1, sy + 1, cp - 2, cp - 2 };
    if (ghost) {
        SDL_SetRenderDrawColor(r, color.r / 4, color.g / 4, color.b / 4, 200);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
        SDL_RenderFillRect(r, &rect);
        SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);
        set_color(r, color);
        SDL_RenderDrawRect(r, &rect);
    } else {
        set_color(r, color);
        SDL_RenderFillRect(r, &rect);
        SDL_Color hi = {
            (uint8_t)SDL_min(color.r + 55, 255),
            (uint8_t)SDL_min(color.g + 55, 255),
            (uint8_t)SDL_min(color.b + 55, 255), 255
        };
        set_color(r, hi);
        SDL_RenderDrawLine(r, rect.x, rect.y, rect.x + rect.w - 1, rect.y);
        SDL_RenderDrawLine(r, rect.x, rect.y, rect.x, rect.y + rect.h - 1);
    }
}

static void draw_piece_at(SDL_Renderer *r, const ActivePiece *p,
                          int ox, int oy, int ghost)
{
    int cp = s_layout.cell_px;
    SDL_Color color = PIECE_COLORS[(int)p->kind + 1];
    for (int py = 0; py < PIECE_DIM; py++)
        for (int px = 0; px < PIECE_DIM; px++) {
            if (!piece_cell_at(p->kind, p->rotation, px, py)) continue;
            draw_cell(r,
                      ox + (p->x + px) * cp,
                      oy + (p->y + py) * cp,
                      color, ghost);
        }
}

static void draw_preview(SDL_Renderer *r, PieceKind kind,
                         int box_x, int box_y, int dimmed)
{
    int pc = s_layout.prev_cell;
    SDL_Color color = PIECE_COLORS[(int)kind + 1];
    if (dimmed) {
        color.r = (uint8_t)(color.r / 3);
        color.g = (uint8_t)(color.g / 3);
        color.b = (uint8_t)(color.b / 3);
    }

    int min_px = 4, max_px = -1, min_py = 4, max_py = -1;
    for (int py = 0; py < PIECE_DIM; py++)
        for (int px = 0; px < PIECE_DIM; px++)
            if (piece_cell_at(kind, 0, px, py)) {
                if (px < min_px) min_px = px;
                if (px > max_px) max_px = px;
                if (py < min_py) min_py = py;
                if (py > max_py) max_py = py;
            }

    int w    = (max_px - min_px + 1) * pc;
    int box_w = 4 * pc;
    int ox   = box_x + (box_w - w) / 2 - min_px * pc;
    int oy   = box_y - min_py * pc;

    for (int py = 0; py < PIECE_DIM; py++)
        for (int px = 0; px < PIECE_DIM; px++) {
            if (!piece_cell_at(kind, 0, px, py)) continue;
            SDL_Rect rect = {
                ox + px * pc + 1,
                oy + py * pc + 1,
                pc - 2,
                pc - 2
            };
            set_color(r, color);
            SDL_RenderFillRect(r, &rect);
        }
}

/* -------------------------------------------------------------------------
 * Bitmap font  (5×7 glyphs, uppercase A-Z, 0-9, and : . ! - > _ space)
 * ---------------------------------------------------------------------- */

static const uint8_t ALPHA[26][7] = {
    {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11}, /* A */
    {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E}, /* B */
    {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E}, /* C */
    {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C}, /* D */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F}, /* E */
    {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10}, /* F */
    {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F}, /* G */
    {0x11,0x11,0x11,0x1F,0x11,0x11,0x11}, /* H */
    {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E}, /* I */
    {0x07,0x02,0x02,0x02,0x02,0x12,0x0C}, /* J */
    {0x11,0x12,0x14,0x18,0x14,0x12,0x11}, /* K */
    {0x10,0x10,0x10,0x10,0x10,0x10,0x1F}, /* L */
    {0x11,0x1B,0x15,0x11,0x11,0x11,0x11}, /* M */
    {0x11,0x19,0x15,0x13,0x11,0x11,0x11}, /* N */
    {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E}, /* O */
    {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10}, /* P */
    {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D}, /* Q */
    {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11}, /* R */
    {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E}, /* S */
    {0x1F,0x04,0x04,0x04,0x04,0x04,0x04}, /* T */
    {0x11,0x11,0x11,0x11,0x11,0x11,0x0E}, /* U */
    {0x11,0x11,0x11,0x11,0x11,0x0A,0x04}, /* V */
    {0x11,0x11,0x11,0x15,0x15,0x15,0x0A}, /* W */
    {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11}, /* X */
    {0x11,0x11,0x0A,0x04,0x04,0x04,0x04}, /* Y */
    {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}, /* Z */
};

static const uint8_t DIGITS[10][7] = {
    {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
    {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
    {0x0E,0x11,0x01,0x06,0x08,0x10,0x1F},
    {0x0E,0x11,0x01,0x06,0x01,0x11,0x0E},
    {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
    {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
    {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
    {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
    {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
    {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
};

static const uint8_t GLYPH_COLON[7] = {0,0x04,0,0,0,0x04,0};
static const uint8_t GLYPH_DOT[7]   = {0,0,0,0,0,0,0x04};
static const uint8_t GLYPH_BANG[7]  = {0x04,0x04,0x04,0x04,0,0,0x04};
static const uint8_t GLYPH_DASH[7]  = {0,0,0,0x1F,0,0,0};
static const uint8_t GLYPH_GT[7]    = {0x10,0x08,0x04,0x02,0x04,0x08,0x10};
static const uint8_t GLYPH_UNDER[7] = {0,0,0,0,0,0,0x1F};  /* _ cursor */
static const uint8_t GLYPH_SLASH[7] = {0x01,0x02,0x04,0x04,0x08,0x10,0x10};

static void draw_glyph(SDL_Renderer *r, const uint8_t glyph[7],
                       int x, int y, int scale)
{
    for (int row = 0; row < 7; row++)
        for (int col = 0; col < 5; col++)
            if (glyph[row] & (1 << (4 - col))) {
                SDL_Rect px = { x + col*scale, y + row*scale, scale, scale };
                SDL_RenderFillRect(r, &px);
            }
}

static int draw_char(SDL_Renderer *r, char ch, int x, int y, int scale)
{
    int advance = (5 + 1) * scale;
    if      (ch >= 'A' && ch <= 'Z') draw_glyph(r, ALPHA[(int)(ch-'A')], x,y,scale);
    else if (ch >= '0' && ch <= '9') draw_glyph(r, DIGITS[(int)(ch-'0')], x,y,scale);
    else if (ch == ':')              draw_glyph(r, GLYPH_COLON, x,y,scale);
    else if (ch == '.')              draw_glyph(r, GLYPH_DOT,   x,y,scale);
    else if (ch == '!')              draw_glyph(r, GLYPH_BANG,  x,y,scale);
    else if (ch == '-')              draw_glyph(r, GLYPH_DASH,  x,y,scale);
    else if (ch == '>')              draw_glyph(r, GLYPH_GT,    x,y,scale);
    else if (ch == '_')              draw_glyph(r, GLYPH_UNDER, x,y,scale);
    else if (ch == '/')              draw_glyph(r, GLYPH_SLASH, x,y,scale);
    return advance;
}

static void draw_str(SDL_Renderer *r, const char *s, int x, int y, int scale)
{
    int cx = x;
    for (int i = 0; s[i]; i++)
        cx += draw_char(r, s[i], cx, y, scale);
}

static int str_width(const char *s, int scale)
{
    return (int)SDL_strlen(s) * (5 + 1) * scale;
}

static void draw_number(SDL_Renderer *r, int n, int x, int y, int scale)
{
    char buf[16];
    SDL_snprintf(buf, sizeof(buf), "%d", n);
    draw_str(r, buf, x, y, scale);
}

static void draw_time(SDL_Renderer *r, double sec, int x, int y, int scale)
{
    int total_cs = (int)(sec * 10.0);
    int mm  = total_cs / 600;
    int ss  = (total_cs % 600) / 10;
    int cs  = total_cs % 10;
    char buf[16];
    SDL_snprintf(buf, sizeof(buf), "%02d:%02d.%d", mm, ss, cs);
    draw_str(r, buf, x, y, scale);
}

static void draw_panel_box(SDL_Renderer *r, int x, int y, int w, int h)
{
    SDL_Color border = (SDL_Color){60, 60, 90, 255};
    SDL_Color bg     = (SDL_Color){20, 20, 40, 255};
    SDL_Rect  b      = {x-1, y-1, w+2, h+2};
    SDL_Rect  inner  = {x, y, w, h};
    set_color(r, border);
    SDL_RenderDrawRect(r, &b);
    set_color(r, bg);
    SDL_RenderFillRect(r, &inner);
}

static void draw_panel_box_colored(SDL_Renderer *r, int x, int y, int w, int h,
                                   SDL_Color border, SDL_Color bg)
{
    SDL_Rect b     = {x-1, y-1, w+2, h+2};
    SDL_Rect inner = {x, y, w, h};
    set_color(r, border);
    SDL_RenderDrawRect(r, &b);
    set_color(r, bg);
    SDL_RenderFillRect(r, &inner);
}

static void format_int_sep(int n, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return;
    if (n < 0) n = -n;

    char tmp[32];
    SDL_snprintf(tmp, sizeof(tmp), "%d", n);
    int len = (int)SDL_strlen(tmp);
    int dots = (len > 3) ? (len - 1) / 3 : 0;
    int olen = len + dots;
    if ((size_t)olen + 1 > out_sz) {
        SDL_snprintf(out, out_sz, "%d", n);
        return;
    }

    out[olen] = '\0';
    int si = len - 1;
    int di = olen - 1;
    int group = 0;
    while (si >= 0) {
        out[di--] = tmp[si--];
        group++;
        if (group == 3 && si >= 0) {
            out[di--] = '.';
            group = 0;
        }
    }
}

static void format_time_str(double sec, char *out, size_t out_sz)
{
    if (!out || out_sz == 0) return;
    int total_cs = (int)(sec * 10.0);
    int mm  = total_cs / 600;
    int ss  = (total_cs % 600) / 10;
    int cs  = total_cs % 10;
    SDL_snprintf(out, out_sz, "%02d:%02d.%d", mm, ss, cs);
}

static void render_hud_divider(SDL_Renderer *r, int x, int w, int y)
{
    set_color(r, COLOR_DIVIDER);
    SDL_RenderDrawLine(r, x, y, x + w, y);
}

static int render_hud_row(SDL_Renderer *r, int x, int y,
                          const char *label, const char *value,
                          SDL_Color value_color, int label_scale, int value_scale,
                          int pad)
{
    if (!label) label = "";
    if (!value) value = "";

    set_color(r, COLOR_LABEL);
    draw_str(r, label, x, y, label_scale);
    int value_y = y + 7 * label_scale + 2;
    set_color(r, value_color);
    draw_str(r, value, x, value_y, value_scale);

    return (value_y + 7 * value_scale) - y + pad;
}

/* -------------------------------------------------------------------------
 * Sub-renderers
 * ---------------------------------------------------------------------- */

static void render_board_area(SDL_Renderer *r, const GameState *g)
{
    int ox = s_layout.board.x;
    int oy = s_layout.board.y;
    int cp = s_layout.cell_px;
    int in_anim = (g->game_phase == PHASE_LINE_CLEAR_ANIM);

    for (int y = 0; y < BOARD_H; y++) {
        int clearing = in_anim && ((g->clearing_rows >> y) & 1u);
        for (int x = 0; x < BOARD_W; x++) {
            SDL_Color color;
            if (clearing) {
                if (g->clear_timer < LINE_CLEAR_TICKS * 2 / 3)
                    color = (SDL_Color){220, 240, 255, 255};
                else
                    color = (SDL_Color){45, 45, 70, 255};
            } else {
                int ci = (int)g->board.cells[y][x];
                color = (ci < 9) ? PIECE_COLORS[ci] : PIECE_COLORS[8];
            }
            draw_cell(r, ox + x * cp, oy + y * cp, color, 0);
        }
    }

    if (opt_grid_lines) {
        SDL_SetRenderDrawColor(r, 40, 40, 60, 255);
        for (int x = 1; x < BOARD_W; x++) {
            int lx = ox + x * cp;
            SDL_RenderDrawLine(r, lx, oy, lx, oy + BOARD_H * cp);
        }
        for (int y = 1; y < BOARD_H; y++) {
            int ly = oy + y * cp;
            SDL_RenderDrawLine(r, ox, ly, ox + BOARD_W * cp, ly);
        }
    }

    SDL_SetRenderDrawColor(r, 70, 70, 110, 255);
    SDL_Rect border = { ox - 2, oy - 2, BOARD_W * cp + 4, BOARD_H * cp + 4 };
    SDL_RenderDrawRect(r, &border);

    if (g->app_state != APP_GAME_OVER && !in_anim) {
        if (opt_ghost) {
            /* Dynamic ghost color: red tint when stack is in top 5 rows */
            int highest = BOARD_H;
            for (int gy = 0; gy < BOARD_H && highest == BOARD_H; gy++)
                for (int gx = 0; gx < BOARD_W; gx++)
                    if (g->board.cells[gy][gx] != CELL_EMPTY) {
                        highest = gy; break;
                    }
            int danger = (highest < 5);
            int cp2 = s_layout.cell_px;
            for (int py = 0; py < PIECE_DIM; py++)
                for (int px2 = 0; px2 < PIECE_DIM; px2++) {
                    if (!piece_cell_at(g->ghost.kind, g->ghost.rotation,
                                       px2, py)) continue;
                    SDL_Color gc;
                    if (danger)
                        gc = (SDL_Color){200, 50, 50, 255};
                    else
                        gc = PIECE_COLORS[(int)g->ghost.kind + 1];
                    draw_cell(r,
                              ox + (g->ghost.x + px2) * cp2,
                              oy + (g->ghost.y + py)  * cp2,
                              gc, 1);
                }
        }
        draw_piece_at(r, &g->current, ox, oy, 0);
    }
}

static void render_left_panel(SDL_Renderer *r, const GameState *g)
{
    static int      s_last_score       = 0;
    static uint32_t s_score_bump_until = 0;

    int pad  = s_layout.pad;
    int pc   = s_layout.prev_cell;
    int flg  = s_layout.font_lg;
    int fsm  = s_layout.font_sm;

    int x = s_layout.left_panel.x + pad;
    int w = s_layout.left_panel.w - pad * 2;
    if (w < 80) return;

    uint32_t now = SDL_GetTicks();

    /* HOLD box ---------------------------------------------------------- */
    int hold_w = w;
    int hold_h = 7 * fsm + pad + (4 * pc + 6) + pad;
    int y = s_layout.board.y + pad;

    SDL_Color hold_border = g->hold_used
                            ? (SDL_Color){120, 30, 30, 255}
                            : (SDL_Color){60, 60, 90, 255};
    draw_panel_box_colored(r, x, y, hold_w, hold_h, hold_border, COLOR_HOLD_BG);

    set_color(r, COLOR_LABEL);
    draw_str(r, i18n(I18N_LABEL_HOLD), x + pad, y + pad / 2, fsm);

    int inner_w = 4 * pc + 6;
    int inner_x = x + (hold_w - inner_w) / 2;
    int inner_y = y + 7 * fsm + pad;
    draw_panel_box_colored(r, inner_x, inner_y, inner_w, inner_w,
                           (SDL_Color){50, 50, 80, 255}, (SDL_Color){15, 15, 25, 255});
    if (g->hold == PIECE_COUNT) {
        const char *dash = "---";
        set_color(r, COLOR_DIVIDER);
        draw_str(r, dash,
                 inner_x + (inner_w - str_width(dash, flg)) / 2,
                 inner_y + (inner_w - 7 * flg) / 2,
                 flg);
    } else {
        draw_preview(r, g->hold, inner_x + 3, inner_y + 3, g->hold_used);
    }

    y += hold_h + pad * 2;

    /* Sections ---------------------------------------------------------- */
    render_hud_divider(r, x, w, y);
    y += pad * 2;

    /* SCORE */
    if (g->mode != MODE_ZEN) {
        if (g->score > s_last_score) {
            s_score_bump_until = now + 140;
        }
        s_last_score = g->score;

        char score_buf[24];
        format_int_sep(g->score, score_buf, sizeof(score_buf));
        int score_scale = flg + ((now < s_score_bump_until) ? 1 : 0);
        y += render_hud_row(r, x + pad, y,
                            i18n(I18N_LABEL_SCORE), score_buf,
                            COLOR_ACCENT, fsm, score_scale, pad);
        render_hud_divider(r, x, w, y - pad);
        y += pad * 2;
    }

    /* LEVEL + LINES */
    {
        char buf[16];
        SDL_snprintf(buf, sizeof(buf), "%d", g->level);
        y += render_hud_row(r, x + pad, y,
                            i18n(I18N_LABEL_LEVEL), buf,
                            COLOR_VALUE, fsm, flg, pad);
    }
    {
        char buf[16];
        SDL_snprintf(buf, sizeof(buf), "%d", g->total_lines);
        y += render_hud_row(r, x + pad, y,
                            i18n(I18N_LABEL_LINES), buf,
                            COLOR_VALUE, fsm, flg, pad);
    }

    render_hud_divider(r, x, w, y - pad);
    y += pad * 2;

    /* GOAL (mode-specific) */
    if (g->mode == MODE_SPRINT || g->mode == MODE_CHEESE ||
        g->mode == MODE_MARATHON || g->mode == MODE_SURVIVAL) {
        const char *lbl = "";
        char        val[16];
        SDL_Color   vc  = COLOR_VALUE;

        if (g->mode == MODE_SPRINT) {
            int left = SPRINT_GOAL - g->total_lines;
            if (left < 0) left = 0;
            SDL_snprintf(val, sizeof(val), "%d", left);
            lbl = i18n(I18N_LABEL_LEFT);
            if (left <= 5 && (now % 500u) < 250u) vc = COLOR_DANGER;
        } else if (g->mode == MODE_CHEESE) {
            int left = CHEESE_LINES - g->total_lines;
            if (left < 0) left = 0;
            SDL_snprintf(val, sizeof(val), "%d", left);
            lbl = i18n(I18N_LABEL_LEFT);
            if (left <= 3 && (now % 500u) < 250u) vc = COLOR_DANGER;
        } else {
            int to_next = 10 - (g->total_lines % 10);
            if (to_next == 10) to_next = 10;
            SDL_snprintf(val, sizeof(val), "%d", to_next);
            lbl = i18n(I18N_LABEL_NEXT_LV);
        }

        y += render_hud_row(r, x + pad, y, lbl, val, vc, fsm, flg, pad);
        render_hud_divider(r, x, w, y - pad);
        y += pad * 2;
    }

    /* TIME (elapsed or remaining) */
    {
        char buf[16];
        SDL_Color tc = COLOR_VALUE;

        if (g->mode == MODE_ULTRA) {
            double left = ULTRA_TIME - g->time_elapsed;
            if (left < 0.0) left = 0.0;
            format_time_str(left, buf, sizeof(buf));
            if (left <= 30.0 && (now % 500u) < 250u) tc = COLOR_DANGER;
        } else if (g->mode == MODE_BLITZ) {
            double left = BLITZ_TIME - g->time_elapsed;
            if (left < 0.0) left = 0.0;
            format_time_str(left, buf, sizeof(buf));
            if (left <= 10.0 && (now % 500u) < 250u) tc = COLOR_DANGER;
        } else {
            format_time_str(g->time_elapsed, buf, sizeof(buf));
        }

        y += render_hud_row(r, x + pad, y,
                            i18n(I18N_LABEL_TIME), buf,
                            tc, fsm, flg, pad);
    }

    /* Status chips */
    y += pad;
    int cx = x + pad;
    int cy = y;
    if (g->combo > 0) {
        char b[16];
        SDL_snprintf(b, sizeof(b), "%s %d", i18n(I18N_LABEL_COMBO), g->combo);
        set_color(r, COLOR_ACCENT);
        draw_str(r, b, cx, cy, fsm);
        cy += 7 * fsm + 2;
    }
    if (g->b2b) {
        set_color(r, COLOR_ACCENT);
        draw_str(r, "B2B", cx, cy, fsm);
        cy += 7 * fsm + 2;
    }
    if (g->tspin) {
        set_color(r, COLOR_ACCENT);
        draw_str(r, i18n(I18N_LABEL_TSPIN), cx, cy, fsm);
        cy += 7 * fsm + 2;
    }
    if (g->all_clear) {
        set_color(r, COLOR_ACCENT);
        draw_str(r, i18n(I18N_LABEL_ALL_CLEAR), cx, cy, fsm);
    }
}

static void render_right_panel(SDL_Renderer *r, const GameState *g)
{
    int pad  = s_layout.pad;
    int pc   = s_layout.prev_cell;
    int fmd  = s_layout.font_md;

    int bw   = 4 * pc + 4;
    int bx   = s_layout.right_panel.x + pad * 3;
    int y    = s_layout.board.y + pad;

    SDL_SetRenderDrawColor(r, 180, 180, 200, 255);
    draw_str(r, i18n(I18N_LABEL_NEXT), bx, y, fmd);
    y += 7 * fmd + 2;

    int prev_h = 4 * pc + 6;
    for (int i = 0; i < NEXT_VISIBLE && i < g->queue.count; i++) {
        draw_panel_box(r, bx, y, bw, prev_h - 2);
        draw_preview(r, queue_peek(&g->queue, i), bx + 2, y + 2, 0);
        y += prev_h;
    }
}

static void render_overlay_paused(SDL_Renderer *r)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 150);
    SDL_RenderFillRect(r, &s_layout.board);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    int cx  = s_layout.board.x + s_layout.board.w / 2;
    int my  = s_layout.board.y + s_layout.board.h / 2;
    int flg = s_layout.font_lg;
    int fmd = s_layout.font_md;

    SDL_SetRenderDrawColor(r, 240, 240, 60, 255);
    const char *s = i18n(I18N_PAUSED);
    draw_str(r, s, cx - str_width(s, flg) / 2, my - 7 * flg, flg);

    SDL_SetRenderDrawColor(r, 160, 160, 160, 255);
    const char *h1 = i18n(I18N_HINT_SPACE_RESUME);
    const char *h2 = i18n(I18N_HINT_Q_MENU);
    draw_str(r, h1, cx - str_width(h1, fmd) / 2, my + 4,             fmd);
    draw_str(r, h2, cx - str_width(h2, fmd) / 2, my + 7 * fmd + 6,   fmd);
}

static void render_overlay_game_over(SDL_Renderer *r)
{
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, 160);
    SDL_RenderFillRect(r, &s_layout.board);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    int cx  = s_layout.board.x + s_layout.board.w / 2;
    int my  = s_layout.board.y + s_layout.board.h / 2;
    int flg = s_layout.font_lg;
    int fmd = s_layout.font_md;

    SDL_SetRenderDrawColor(r, 230, 50, 50, 255);
    const char *s1 = i18n(I18N_GAME);
    const char *s2 = i18n(I18N_OVER);
    draw_str(r, s1, cx - str_width(s1, flg) / 2, my - 7 * flg - 4, flg);
    draw_str(r, s2, cx - str_width(s2, flg) / 2, my + 4,            flg);

    SDL_SetRenderDrawColor(r, 160, 160, 160, 255);
    const char *h1 = i18n(I18N_HINT_SPACE_RETRY);
    const char *h2 = i18n(I18N_HINT_Q_MENU);
    draw_str(r, h1, cx - str_width(h1, fmd) / 2, my + 7 * flg + 14,              fmd);
    draw_str(r, h2, cx - str_width(h2, fmd) / 2, my + 7 * flg + 14 + 7 * fmd + 4, fmd);
}

static void render_tip(SDL_Renderer *r, const GameState *g)
{
    /* Latch new tip from game state */
    if (g->tip_id != TIP_NONE && g->tip_id != s_tip_active) {
        s_tip_active = g->tip_id;
        s_tip_expire = SDL_GetTicks() + 3500; /* display for 3.5 s */
    }

    if (s_tip_active == TIP_NONE) return;
    uint32_t now = SDL_GetTicks();
    if (now >= s_tip_expire) { s_tip_active = TIP_NONE; return; }

    uint32_t remaining = s_tip_expire - now;
    uint8_t alpha = (remaining < 600)
                    ? (uint8_t)((remaining * 255u) / 600u)
                    : 255;

    int cx  = s_layout.screen_w / 2;
    int ty  = s_layout.board.y + s_layout.board.h - 7 * s_layout.font_sm - s_layout.pad;
    int fsm = s_layout.font_sm;

    const char *text = i18n_tip(s_tip_active);
    int tw = str_width(text, fsm);

    /* Semi-transparent background bar */
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(r, 0, 0, 0, (uint8_t)((uint32_t)alpha * 160u / 255u));
    SDL_Rect bg = { cx - tw / 2 - 6, ty - 3, tw + 12, 7 * fsm + 6 };
    SDL_RenderFillRect(r, &bg);
    SDL_SetRenderDrawBlendMode(r, SDL_BLENDMODE_NONE);

    SDL_SetRenderDrawColor(r, 255, 230, 80, alpha);
    draw_str(r, text, cx - tw / 2, ty, fsm);
}

static void render_playing(SDL_Renderer *r, const GameState *g)
{
    render_board_area(r, g);
    render_left_panel(r, g);
    render_right_panel(r, g);
    render_tip(r, g);

    if (g->app_state == APP_PAUSED)    render_overlay_paused(r);
    if (g->app_state == APP_GAME_OVER) render_overlay_game_over(r);
}

/* -------------------------------------------------------------------------
 * Mode info screen
 * ---------------------------------------------------------------------- */

static void render_mode_info(SDL_Renderer *r, const GameState *g)
{
    GameMode mode = (GameMode)((int)g->menu_cursor % (int)MODE_COUNT);
    const char *title = i18n_mode_info_title(mode);

    int cx  = s_layout.screen_w / 2;
    int sh  = s_layout.screen_h;
    int fxl = s_layout.font_xl;
    int fmd = s_layout.font_md;
    int fsm = s_layout.font_sm;
    int pad = s_layout.pad;

    /* Mode name */
    SDL_SetRenderDrawColor(r, 80, 200, 255, 255);
    draw_str(r, title, cx - str_width(title, fxl) / 2, sh * 8 / 100, fxl);

    /* Separator */
    int sep_y = sh * 8 / 100 + 7 * fxl + pad;
    SDL_SetRenderDrawColor(r, 50, 60, 90, 255);
    int sep_hw = str_width(title, fxl) / 2 + pad * 4;
    SDL_RenderDrawLine(r, cx - sep_hw, sep_y, cx + sep_hw, sep_y);

    /* Description lines */
    int y = sep_y + pad * 2;
    SDL_SetRenderDrawColor(r, 160, 170, 200, 255);
    for (int i = 0; i < 4; i++) {
        const char *line = i18n_mode_info_line(mode, i);
        if (!line) break;
        draw_str(r, line, cx - str_width(line, fsm) / 2, y, fsm);
        y += 7 * fsm + 4;
    }

    y += pad;

    /* Objective */
    SDL_SetRenderDrawColor(r, 255, 220, 60, 255);
    {
        const char *obj = i18n_mode_info_objective(mode);
        draw_str(r, obj, cx - str_width(obj, fmd) / 2, y, fmd);
    }
    y += 7 * fmd + pad * 3;

    /* Separator */
    SDL_SetRenderDrawColor(r, 40, 50, 80, 255);
    SDL_RenderDrawLine(r, cx - sep_hw, y, cx + sep_hw, y);
    y += pad * 2;

    /* Tips */
    SDL_SetRenderDrawColor(r, 120, 200, 120, 255);
    const char *t1_label = "> ";
    draw_str(r, t1_label, cx - sep_hw + pad, y, fsm);
    SDL_SetRenderDrawColor(r, 150, 200, 150, 255);
    {
        const char *tip = i18n_mode_info_tip(mode, 0);
        draw_str(r, tip,
                 cx - sep_hw + pad + str_width(t1_label, fsm), y, fsm);
    }
    y += 7 * fsm + 4;

    SDL_SetRenderDrawColor(r, 120, 200, 120, 255);
    draw_str(r, t1_label, cx - sep_hw + pad, y, fsm);
    SDL_SetRenderDrawColor(r, 150, 200, 150, 255);
    {
        const char *tip = i18n_mode_info_tip(mode, 1);
        draw_str(r, tip,
                 cx - sep_hw + pad + str_width(t1_label, fsm), y, fsm);
    }
    y += 7 * fsm + pad * 4;

    /* Start hint */
    SDL_SetRenderDrawColor(r, 255, 255, 100, 255);
    const char *h1 = i18n(I18N_HINT_SPACE_PLAY);
    draw_str(r, h1, cx - str_width(h1, fmd) / 2, y, fmd);
    y += 7 * fmd + 4;
    SDL_SetRenderDrawColor(r, 100, 100, 140, 255);
    const char *h2 = i18n(I18N_HINT_Q_BACK);
    draw_str(r, h2, cx - str_width(h2, fsm) / 2, y, fsm);
}

/* -------------------------------------------------------------------------
 * Menu screen
 * ---------------------------------------------------------------------- */
static void render_menu(SDL_Renderer *r, const GameState *g)
{
    int cx  = s_layout.screen_w / 2;
    int sh  = s_layout.screen_h;
    int fxl = s_layout.font_xl;
    int fmd = s_layout.font_md;
    int fsm = s_layout.font_sm;
    int pad = s_layout.pad;

    /* Title */
    SDL_SetRenderDrawColor(r, 80, 180, 255, 255);
    int title_y = sh * 7 / 100;
    const char *title = i18n(I18N_TITLE);
    draw_str(r, title, cx - str_width(title, fxl) / 2, title_y, fxl);

    /* Player name */
    if (g->player_name[0]) {
        SDL_SetRenderDrawColor(r, 120, 160, 200, 255);
        int sub_y = title_y + 7 * fxl + pad;
        draw_str(r, g->player_name,
                 cx - str_width(g->player_name, fsm) / 2, sub_y, fsm);
    }

    /* Mode list */
    int item_h  = 7 * fmd + pad * 2;
    int list_y  = sh * 24 / 100;
    int max_w = 0;
    for (int i = 0; i < (int)MODE_COUNT; i++) {
        const char *mn = i18n_mode_name((GameMode)i);
        int w = str_width(mn, fmd);
        if (w > max_w) max_w = w;
    }
    int bar_w   = max_w + pad * 4;
    for (int i = 0; i < (int)MODE_COUNT; i++) {
        int item_y = list_y + i * item_h;
        int text_y = item_y + pad / 2;
        const char *mn = i18n_mode_name((GameMode)i);
        if (i == g->menu_cursor) {
            SDL_SetRenderDrawColor(r, 40, 40, 80, 255);
            SDL_Rect bar = { cx - bar_w / 2, item_y, bar_w, item_h };
            SDL_RenderFillRect(r, &bar);
            SDL_SetRenderDrawColor(r, 255, 220, 80, 255);
            draw_char(r, '>', cx - bar_w / 2 + pad / 2, text_y, fmd);
            SDL_SetRenderDrawColor(r, 255, 255, 100, 255);
        } else {
            SDL_SetRenderDrawColor(r, 160, 160, 180, 255);
        }
        int nw = str_width(mn, fmd);
        draw_str(r, mn, cx - nw / 2, text_y, fmd);
    }

    /* Hints */
    int hint_y = list_y + (int)MODE_COUNT * item_h + pad * 2;
    SDL_SetRenderDrawColor(r, 100, 100, 130, 255);
    const char *hint1 = i18n(I18N_MENU_HINT1);
    draw_str(r, hint1, cx - str_width(hint1, fsm) / 2, hint_y, fsm);
    hint_y += 7 * fsm + 3;
    const char *hint2 = i18n(I18N_MENU_HINT2);
    draw_str(r, hint2, cx - str_width(hint2, fsm) / 2, hint_y, fsm);
    hint_y += 7 * fsm + pad * 3;

    /* Separator */
    SDL_SetRenderDrawColor(r, 50, 50, 70, 255);
    int sep_hw = bar_w / 2 + pad * 2;
    SDL_RenderDrawLine(r, cx - sep_hw, hint_y, cx + sep_hw, hint_y);
    hint_y += pad * 2;

    /* High scores */
    SDL_SetRenderDrawColor(r, 160, 160, 190, 255);
    const char *hs_title = i18n(I18N_HIGH_SCORES);
    draw_str(r, hs_title, cx - str_width(hs_title, fmd) / 2, hint_y, fmd);
    hint_y += 7 * fmd + pad;

    int lx    = cx - sep_hw + pad;
    int val_x = cx + pad;

    /* Marathon */
    SDL_SetRenderDrawColor(r, 120, 120, 160, 255);
    draw_str(r, i18n_mode_name(MODE_MARATHON), lx, hint_y, fsm);
    SDL_SetRenderDrawColor(r, 255, 255, 100, 255);
    if (g->hs_marathon > 0) draw_number(r, g->hs_marathon, val_x, hint_y, fsm);
    else draw_str(r, "-", val_x, hint_y, fsm);
    hint_y += 7 * fsm + 2;

    /* Sprint */
    SDL_SetRenderDrawColor(r, 120, 120, 160, 255);
    draw_str(r, i18n_mode_name(MODE_SPRINT), lx, hint_y, fsm);
    SDL_SetRenderDrawColor(r, 100, 255, 100, 255);
    if (g->hs_sprint_ms > 0)
        draw_time(r, (double)g->hs_sprint_ms / 1000.0, val_x, hint_y, fsm);
    else draw_str(r, "-", val_x, hint_y, fsm);
    hint_y += 7 * fsm + 2;

    /* Ultra */
    SDL_SetRenderDrawColor(r, 120, 120, 160, 255);
    draw_str(r, i18n_mode_name(MODE_ULTRA), lx, hint_y, fsm);
    SDL_SetRenderDrawColor(r, 255, 200, 80, 255);
    if (g->hs_ultra > 0) draw_number(r, g->hs_ultra, val_x, hint_y, fsm);
    else draw_str(r, "-", val_x, hint_y, fsm);
    hint_y += 7 * fsm + 2;

    /* Blitz */
    SDL_SetRenderDrawColor(r, 120, 120, 160, 255);
    draw_str(r, i18n_mode_name(MODE_BLITZ), lx, hint_y, fsm);
    SDL_SetRenderDrawColor(r, 255, 160, 80, 255);
    if (g->hs_blitz > 0) draw_number(r, g->hs_blitz, val_x, hint_y, fsm);
    else draw_str(r, "-", val_x, hint_y, fsm);
    hint_y += 7 * fsm + 2;

    /* Cheese */
    SDL_SetRenderDrawColor(r, 120, 120, 160, 255);
    draw_str(r, i18n_mode_name(MODE_CHEESE), lx, hint_y, fsm);
    SDL_SetRenderDrawColor(r, 220, 200, 100, 255);
    if (g->hs_cheese_ms > 0)
        draw_time(r, (double)g->hs_cheese_ms / 1000.0, val_x, hint_y, fsm);
    else draw_str(r, "-", val_x, hint_y, fsm);

    /* Survival */
    hint_y += 7 * fsm + 2;
    SDL_SetRenderDrawColor(r, 120, 120, 160, 255);
    draw_str(r, i18n_mode_name(MODE_SURVIVAL), lx, hint_y, fsm);
    SDL_SetRenderDrawColor(r, 180, 100, 255, 255);
    if (g->hs_survival > 0) draw_number(r, g->hs_survival, val_x, hint_y, fsm);
    else draw_str(r, "-", val_x, hint_y, fsm);
}

/* -------------------------------------------------------------------------
 * Name-entry screen
 * ---------------------------------------------------------------------- */
static void render_name_entry(SDL_Renderer *r)
{
    int cx  = s_layout.screen_w / 2;
    int cy  = s_layout.screen_h / 2;
    int fxl = s_layout.font_xl;
    int fmd = s_layout.font_md;
    int fsm = s_layout.font_sm;
    int pad = s_layout.pad;

    /* Title */
    SDL_SetRenderDrawColor(r, 80, 180, 255, 255);
    const char *t = i18n(I18N_WELCOME);
    draw_str(r, t, cx - str_width(t, fxl) / 2, cy - 7 * fxl * 4, fxl);

    SDL_SetRenderDrawColor(r, 160, 160, 190, 255);
    const char *sub = i18n(I18N_ENTER_YOUR_NAME);
    draw_str(r, sub, cx - str_width(sub, fmd) / 2,
             cy - 7 * fxl * 3 + pad, fmd);

    /* Input box */
    const char *buf = input_text_get();
    int box_w = str_width("XXXXXXXXXXX", fmd) + pad * 4;
    int box_h = 7 * fmd + pad * 2;
    int box_x = cx - box_w / 2;
    int box_y = cy - box_h / 2;
    draw_panel_box(r, box_x, box_y, box_w, box_h);

    /* Text + cursor */
    char display[32];
    SDL_snprintf(display, sizeof(display), "%s_", buf);
    SDL_SetRenderDrawColor(r, 255, 240, 100, 255);
    int tw = str_width(display, fmd);
    draw_str(r, display, cx - tw / 2, box_y + pad, fmd);

    /* Hint */
    SDL_SetRenderDrawColor(r, 100, 100, 130, 255);
    const char *h1 = i18n(I18N_NAME_HINT1);
    draw_str(r, h1, cx - str_width(h1, fsm) / 2, box_y + box_h + pad * 2, fsm);
    const char *h2 = i18n(I18N_NAME_HINT2);
    draw_str(r, h2, cx - str_width(h2, fsm) / 2,
             box_y + box_h + pad * 2 + 7 * fsm + 3, fsm);
}

/* -------------------------------------------------------------------------
 * Settings screen
 * ---------------------------------------------------------------------- */

static const char *RES_LABELS[] = {"480P", "720P", "1080P", "1440P"};

static void render_settings(SDL_Renderer *r, const GameState *g,
                            const GameConfig *cfg)
{
    int cx  = s_layout.screen_w / 2;
    int sh  = s_layout.screen_h;
    int fxl = s_layout.font_xl;
    int fmd = s_layout.font_md;
    int fsm = s_layout.font_sm;
    int pad = s_layout.pad;

    SDL_SetRenderDrawColor(r, 80, 180, 255, 255);
    const char *title = i18n(I18N_SETTINGS);
    draw_str(r, title, cx - str_width(title, fxl) / 2, sh * 8 / 100, fxl);

    /* Items: label, value string */
    typedef struct { const char *label; char val[16]; } Item;
    Item items[12];
    const char *res =
        (cfg->resolution == RES_FULLSCREEN) ? i18n(I18N_RES_FULL) :
        (cfg->resolution >= 0 && cfg->resolution < (int)RES_FULLSCREEN)
            ? RES_LABELS[cfg->resolution] : "-";
    SDL_snprintf(items[0].val, 16, "%s", res);
    SDL_snprintf(items[1].val, 16, "%s", cfg->ghost      ? i18n(I18N_ON) : i18n(I18N_OFF));
    SDL_snprintf(items[2].val, 16, "%s", cfg->grid_lines ? i18n(I18N_ON) : i18n(I18N_OFF));
    SDL_snprintf(items[3].val, 16, "%d",  cfg->sfx_volume);
    SDL_snprintf(items[4].val, 16, "%d",  cfg->bgm_volume);
    SDL_snprintf(items[5].val, 16, "%d",  cfg->das_ms);
    SDL_snprintf(items[6].val, 16, "%d",  cfg->arr_ms);
    SDL_snprintf(items[7].val, 16, "%s",  i18n_language_label(cfg->language));
    SDL_snprintf(items[8].val, 16, "%s",  ">");
    SDL_snprintf(items[9].val, 16, "%s",  ">");
    SDL_snprintf(items[10].val, 16, "%s", ">");
    SDL_snprintf(items[11].val, 16, "%s", ">");
    items[0].label = i18n(I18N_SETTING_RESOLUTION);
    items[1].label = i18n(I18N_SETTING_GHOST);
    items[2].label = i18n(I18N_SETTING_GRID_LINES);
    items[3].label = i18n(I18N_SETTING_SFX_VOL);
    items[4].label = i18n(I18N_SETTING_BGM_VOL);
    items[5].label = i18n(I18N_SETTING_DAS_MS);
    items[6].label = i18n(I18N_SETTING_ARR_MS);
    items[7].label = i18n(I18N_SETTING_LANGUAGE);
    items[8].label = i18n(I18N_EDIT_USER);
    items[9].label = i18n(I18N_RESET_SCORES);
    items[10].label = i18n(I18N_RESET_SETTINGS);
    items[11].label = i18n(I18N_RESET_ALL);

    int n_items = 12;
    int item_h  = 7 * fmd + pad * 2;
    int list_y  = sh * 18 / 100;
    int max_label = 0, max_val = 0;
    for (int i = 0; i < n_items; i++) {
        int lw = str_width(items[i].label, fmd);
        int vw = str_width(items[i].val, fmd);
        if (lw > max_label) max_label = lw;
        if (vw > max_val)   max_val   = vw;
    }
    int gap     = str_width("  ", fmd);
    int label_x = cx - gap - max_label;
    int val_x   = cx + gap;

    for (int i = 0; i < n_items; i++) {
        int iy = list_y + i * item_h;
        int ty = iy + pad / 2;
        if (i == g->settings_cursor) {
            SDL_SetRenderDrawColor(r, 30, 30, 70, 255);
            /* Full-width highlight to avoid cutting long labels */
            SDL_Rect bar = { 0, iy, s_layout.screen_w, item_h };
            SDL_RenderFillRect(r, &bar);
            SDL_SetRenderDrawColor(r, 255, 220, 80, 255);
            {
                int ax = label_x - pad * 2;
                if (ax < pad / 2) ax = pad / 2;
                draw_char(r, '>', ax, ty, fmd);
            }
            SDL_SetRenderDrawColor(r, 255, 255, 255, 255);
        } else {
            SDL_SetRenderDrawColor(r, 140, 140, 170, 255);
        }
        draw_str(r, items[i].label, label_x, ty, fmd);
        SDL_SetRenderDrawColor(r, (i == g->settings_cursor) ? 255 : 200,
                               220, 100, 255);
        draw_str(r, items[i].val, val_x, ty, fmd);
    }

    /* Hints */
    int hy = list_y + n_items * item_h + pad * 3;
    SDL_SetRenderDrawColor(r, 90, 90, 120, 255);
    const char *h1 = i18n(I18N_SETTINGS_HINT1);
    draw_str(r, h1, cx - str_width(h1, fsm) / 2, hy, fsm);
    hy += 7 * fsm + 3;
    const char *h2 = i18n(I18N_SETTINGS_HINT2);
    draw_str(r, h2, cx - str_width(h2, fsm) / 2, hy, fsm);
}

/* -------------------------------------------------------------------------
 * Confirm dialog
 * ---------------------------------------------------------------------- */

static void render_confirm(SDL_Renderer *r, const GameState *g)
{
    int cx  = s_layout.screen_w / 2;
    int cy  = s_layout.screen_h / 2;
    int fxl = s_layout.font_xl;
    int fmd = s_layout.font_md;
    int fsm = s_layout.font_sm;
    int pad = s_layout.pad;

    const char *title = i18n(I18N_CONFIRM);
    const char *msg   = "";
    if (g->confirm_action == 1) msg = i18n(I18N_CONFIRM_RESET_SCORES);
    else if (g->confirm_action == 2) msg = i18n(I18N_CONFIRM_RESET_SETTINGS);
    else if (g->confirm_action == 3) msg = i18n(I18N_CONFIRM_RESET_ALL);

    int bw = SDL_max(str_width(msg, fmd), str_width(title, fxl));
    bw += pad * 10;
    if (bw > s_layout.screen_w - pad * 4) bw = s_layout.screen_w - pad * 4;
    int bh = 7 * fxl + 7 * fmd * 2 + 7 * fsm * 2 + pad * 10;

    int x = cx - bw / 2;
    int y = cy - bh / 2;
    draw_panel_box_colored(r, x, y, bw, bh, (SDL_Color){60, 60, 90, 255},
                           (SDL_Color){10, 10, 18, 255});

    set_color(r, COLOR_ACCENT);
    draw_str(r, title, cx - str_width(title, fxl) / 2, y + pad * 2, fxl);

    set_color(r, COLOR_VALUE);
    draw_str(r, msg, cx - str_width(msg, fmd) / 2, y + pad * 2 + 7 * fxl + pad, fmd);

    int hy = y + bh - pad * 2 - (7 * fsm + 3) * 2;
    set_color(r, COLOR_LABEL);
    const char *h1 = i18n(I18N_CONFIRM_HINT_OK);
    const char *h2 = i18n(I18N_CONFIRM_HINT_CANCEL);
    draw_str(r, h1, cx - str_width(h1, fsm) / 2, hy, fsm);
    draw_str(r, h2, cx - str_width(h2, fsm) / 2, hy + 7 * fsm + 3, fsm);
}

/* -------------------------------------------------------------------------
 * Results screen
 * ---------------------------------------------------------------------- */
static void render_results(SDL_Renderer *r, const GameState *g)
{
    int cx  = s_layout.screen_w / 2;
    int hy  = s_layout.screen_h * 16 / 100;
    int fxl = s_layout.font_xl;
    int fmd = s_layout.font_md;
    int fsm = s_layout.font_sm;
    int pad = s_layout.pad;

    SDL_SetRenderDrawColor(r, 80, 200, 120, 255);
    const char *heading = i18n(I18N_RESULTS);
    draw_str(r, heading, cx - str_width(heading, fxl) / 2, hy, fxl);
    hy += 7 * fxl + pad;

    SDL_SetRenderDrawColor(r, 160, 160, 190, 255);
    const char *mn = (g->mode < MODE_COUNT) ? i18n_mode_name(g->mode) : "-";
    draw_str(r, mn, cx - str_width(mn, fmd) / 2, hy, fmd);
    hy += 7 * fmd + pad * 2;

    const char *lbl_score = i18n(I18N_LABEL_SCORE);
    const char *lbl_lines = i18n(I18N_LABEL_LINES);
    const char *lbl_level = i18n(I18N_LABEL_LEVEL);
    const char *lbl_time  = i18n(I18N_LABEL_TIME);

    int max_label = str_width(lbl_lines, fmd);
    {
        int w = str_width(lbl_level, fmd);
        if (w > max_label) max_label = w;
    }
    if (g->mode != MODE_ZEN) {
        int w = str_width(lbl_score, fmd);
        if (w > max_label) max_label = w;
    }
    if (g->mode == MODE_SPRINT || g->mode == MODE_ULTRA ||
        g->mode == MODE_BLITZ  || g->mode == MODE_CHEESE) {
        int w = str_width(lbl_time, fmd);
        if (w > max_label) max_label = w;
    }

    int lx = cx - max_label - pad * 2;
    int vx = lx + max_label + str_width("  ", fmd);

    /* Score (not for Zen) */
    if (g->mode != MODE_ZEN) {
        SDL_SetRenderDrawColor(r, 180, 180, 200, 255);
        draw_str(r, lbl_score, lx, hy, fmd);
        SDL_SetRenderDrawColor(r, 255, 255, 100, 255);
        draw_number(r, g->score, vx, hy, fmd);
        hy += 7 * fmd + pad;
    }

    SDL_SetRenderDrawColor(r, 180, 180, 200, 255);
    draw_str(r, lbl_lines, lx, hy, fmd);
    SDL_SetRenderDrawColor(r, 140, 200, 255, 255);
    draw_number(r, g->total_lines, vx, hy, fmd);
    hy += 7 * fmd + pad;

    SDL_SetRenderDrawColor(r, 180, 180, 200, 255);
    draw_str(r, lbl_level, lx, hy, fmd);
    SDL_SetRenderDrawColor(r, 140, 200, 255, 255);
    draw_number(r, g->level, vx, hy, fmd);
    hy += 7 * fmd + pad;

    /* Time */
    if (g->mode == MODE_SPRINT || g->mode == MODE_ULTRA ||
        g->mode == MODE_BLITZ  || g->mode == MODE_CHEESE) {
        SDL_SetRenderDrawColor(r, 180, 180, 200, 255);
        draw_str(r, lbl_time, lx, hy, fmd);
        SDL_SetRenderDrawColor(r, 100, 220, 100, 255);
        draw_time(r, g->time_elapsed, vx, hy, fmd);
        hy += 7 * fmd + pad;
    }

    /* New record indicator */
    {
        int is_record = 0;
        if (g->mode == MODE_MARATHON && g->score == g->hs_marathon && g->score > 0)
            is_record = 1;
        if (g->mode == MODE_SPRINT &&
            (int)(g->time_elapsed * 1000) == g->hs_sprint_ms && g->hs_sprint_ms > 0)
            is_record = 1;
        if (g->mode == MODE_ULTRA && g->score == g->hs_ultra && g->score > 0)
            is_record = 1;
        if (g->mode == MODE_BLITZ && g->score == g->hs_blitz && g->score > 0)
            is_record = 1;
        if (g->mode == MODE_SURVIVAL && g->score == g->hs_survival && g->score > 0)
            is_record = 1;
        if (g->mode == MODE_CHEESE &&
            (int)(g->time_elapsed * 1000) == g->hs_cheese_ms && g->hs_cheese_ms > 0)
            is_record = 1;
        if (is_record) {
            hy += pad;
            SDL_SetRenderDrawColor(r, 255, 220, 40, 255);
            const char *rec = i18n(I18N_NEW_BEST);
            draw_str(r, rec, cx - str_width(rec, fmd) / 2, hy, fmd);
            hy += 7 * fmd + pad;
        }
    }

    hy += pad * 2;

    /* Expanded stats separator */
    SDL_SetRenderDrawColor(r, 40, 50, 80, 255);
    int sw2 = str_width(lbl_score, fmd) + pad * 6;
    SDL_RenderDrawLine(r, cx - sw2, hy, cx + sw2, hy);
    hy += pad * 2;

    /* Piece counts */
    static const char *PNAMES[7] = {"I","O","T","S","Z","J","L"};
    SDL_SetRenderDrawColor(r, 100, 110, 140, 255);
    const char *pieces_lbl = i18n(I18N_PIECES);
    draw_str(r, pieces_lbl, lx, hy, fsm);
    int px = lx + str_width(pieces_lbl, fsm) + str_width(" ", fsm);
    for (int i = 0; i < 7; i++) {
        SDL_Color pc = PIECE_COLORS[i + 1];
        SDL_SetRenderDrawColor(r, pc.r, pc.g, pc.b, 255);
        draw_str(r, PNAMES[i], px, hy, fsm);
        px += str_width("X ", fsm);
        SDL_SetRenderDrawColor(r, 200, 200, 220, 255);
        char nbuf[8];
        SDL_snprintf(nbuf, sizeof(nbuf), "%d", g->stat_pieces[i]);
        draw_str(r, nbuf, px, hy, fsm);
        px += str_width("999 ", fsm);
    }
    hy += 7 * fsm + 3;

    /* 4-line clears and T-spins */
    SDL_SetRenderDrawColor(r, 100, 110, 140, 255);
    const char *quad_lbl = i18n(I18N_QUAD);
    draw_str(r, quad_lbl, lx, hy, fsm);
    SDL_SetRenderDrawColor(r, 200, 200, 220, 255);
    draw_number(r, g->stat_quads,
                lx + str_width(quad_lbl, fsm) + str_width(" ", fsm), hy, fsm);

    SDL_SetRenderDrawColor(r, 100, 110, 140, 255);
    {
        int quad_val_x = lx + str_width(quad_lbl, fsm) + str_width(" ", fsm);
        int quad_val_w = str_width("999", fsm);
        int x2 = quad_val_x + quad_val_w + str_width("  ", fsm);
        const char *tspin_lbl = i18n(I18N_TSPIN_SHORT);
        draw_str(r, tspin_lbl, x2, hy, fsm);
        SDL_SetRenderDrawColor(r, 200, 200, 220, 255);
        draw_number(r, g->stat_tspins,
                    x2 + str_width(tspin_lbl, fsm) + str_width(" ", fsm),
                    hy, fsm);
    }
    hy += 7 * fsm + 3;

    /* Max combo */
    SDL_SetRenderDrawColor(r, 100, 110, 140, 255);
    const char *max_combo_lbl = i18n(I18N_MAX_COMBO);
    draw_str(r, max_combo_lbl, lx, hy, fsm);
    SDL_SetRenderDrawColor(r, 255, 180, 40, 255);
    draw_number(r, g->stat_max_combo,
                lx + str_width(max_combo_lbl, fsm) + str_width(" ", fsm), hy, fsm);
    hy += 7 * fsm + 3;

    /* APM */
    if (g->time_elapsed > 0.5) {
        double minutes = g->time_elapsed / 60.0;
        int apm = (int)((double)g->stat_actions / minutes);
        SDL_SetRenderDrawColor(r, 100, 110, 140, 255);
        const char *apm_lbl = i18n(I18N_APM);
        draw_str(r, apm_lbl, lx, hy, fsm);
        SDL_SetRenderDrawColor(r, 160, 220, 255, 255);
        draw_number(r, apm, lx + str_width(apm_lbl, fsm) + str_width(" ", fsm), hy, fsm);
        hy += 7 * fsm + 3;
    }

    hy += pad;
    SDL_SetRenderDrawColor(r, 130, 130, 160, 255);
    const char *hint = i18n(I18N_HINT_RETURN_MENU);
    draw_str(r, hint, cx - str_width(hint, fsm) / 2, hy, fsm);
}

/* -------------------------------------------------------------------------
 * Public interface
 * ---------------------------------------------------------------------- */

void render_set_options(int ghost, int grid_lines)
{
    opt_ghost      = ghost;
    opt_grid_lines = grid_lines;
}

int render_init(SDL_Renderer **renderer, SDL_Window **window, int w, int h)
{
    *window = SDL_CreateWindow("zezyi",
                               SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               w, h, SDL_WINDOW_SHOWN);
    if (!*window) return -1;
    *renderer = SDL_CreateRenderer(*window, -1,
                                   SDL_RENDERER_ACCELERATED |
                                   SDL_RENDERER_PRESENTVSYNC);
    if (!*renderer) { SDL_DestroyWindow(*window); return -1; }

    layout_compute(&s_layout, w, h);
    return 0;
}

void render_game(SDL_Renderer *r, const GameState *g, const GameConfig *cfg)
{
    if (cfg) i18n_set_language(cfg->language);
    else     i18n_set_language(LANG_EN);

    SDL_SetRenderDrawColor(r, 10, 10, 20, 255);
    SDL_RenderClear(r);

    switch (g->app_state) {
        case APP_MENU:
            render_menu(r, g);
            break;
        case APP_NAME_ENTRY:
            render_name_entry(r);
            break;
        case APP_SETTINGS:
            render_settings(r, g, cfg);
            break;
        case APP_CONFIRM:
            render_confirm(r, g);
            break;
        case APP_MODE_INFO:
            render_mode_info(r, g);
            break;
        case APP_PLAYING:
        case APP_PAUSED:
        case APP_GAME_OVER:
            render_playing(r, g);
            break;
        case APP_RESULTS:
            render_results(r, g);
            break;
    }

    SDL_RenderPresent(r);
}

void render_shutdown(SDL_Renderer *renderer, SDL_Window *window)
{
    if (renderer) SDL_DestroyRenderer(renderer);
    if (window)   SDL_DestroyWindow(window);
}
