#pragma once
#include <SDL2/SDL.h>
#include "config.h"
#include "gameconfig.h"

/* Dynamic layout — derived entirely from screen resolution */
typedef struct {
    int screen_w, screen_h;
    int cell_px;   /* board cell size in pixels                      */
    int prev_cell; /* preview/hold piece cell size                    */
    int pad;       /* generic padding unit                            */
    int font_xl;   /* title font scale                                */
    int font_lg;   /* large font scale  (numbers)                     */
    int font_md;   /* medium font scale (labels)                      */
    int font_sm;   /* small font scale  (hints)                       */
    SDL_Rect board;        /* board area on screen                    */
    SDL_Rect left_panel;   /* hold + stats panel (left of board)      */
    SDL_Rect right_panel;  /* next queue panel   (right of board)     */
} Layout;

void layout_compute(Layout *l, int screen_w, int screen_h);
void render_set_layout(const Layout *l);

int  render_init(SDL_Renderer **renderer, SDL_Window **window, int w, int h);
void render_set_options(int ghost, int grid_lines);
void render_game(SDL_Renderer *renderer, const GameState *g, const GameConfig *cfg);
void render_shutdown(SDL_Renderer *renderer, SDL_Window *window);
