#include <string.h>
#include <ctype.h>
#include <SDL2/SDL.h>
#include "input.h"

static int g_das_ms = 140;
static int g_arr_ms =  30;

void input_set_timing(int das_ms, int arr_ms)
{
    g_das_ms = das_ms;
    g_arr_ms = arr_ms;
}

/* -------------------------------------------------------------------------
 * Text-input mode
 * ---------------------------------------------------------------------- */

static int  s_text_mode = 0;
static char s_text_buf[32];

void input_start_text(const char *initial)
{
    s_text_mode = 1;
    if (initial && initial[0]) {
        strncpy(s_text_buf, initial, sizeof(s_text_buf) - 1);
        s_text_buf[sizeof(s_text_buf) - 1] = '\0';
    } else {
        s_text_buf[0] = '\0';
    }
    SDL_StartTextInput();
}

void input_stop_text(void)
{
    s_text_mode = 0;
    SDL_StopTextInput();
}

const char *input_text_get(void)
{
    return s_text_buf;
}

/* -------------------------------------------------------------------------
 * DAS/ARR repeat helper
 * ---------------------------------------------------------------------- */

typedef struct {
    int held;
    uint32_t hold_start_ms;
    uint32_t last_repeat_ms;
    int das_fired;
} RepeatKey;

static RepeatKey rk_left, rk_right, rk_down;

static void repeat_update(RepeatKey *rk, int pressed, int *out)
{
    uint32_t now = SDL_GetTicks();
    *out = 0;

    if (!pressed) {
        rk->held      = 0;
        rk->das_fired = 0;
        return;
    }
    if (!rk->held) {
        rk->held           = 1;
        rk->hold_start_ms  = now;
        rk->last_repeat_ms = now;
        rk->das_fired      = 0;
        *out = 1;
        return;
    }
    if (!rk->das_fired) {
        if ((now - rk->hold_start_ms) >= (uint32_t)g_das_ms) {
            rk->das_fired      = 1;
            rk->last_repeat_ms = now;
            *out = 1;
        }
    } else {
        if ((now - rk->last_repeat_ms) >= (uint32_t)g_arr_ms) {
            rk->last_repeat_ms = now;
            *out = 1;
        }
    }
}

/* -------------------------------------------------------------------------
 * input_poll
 * ---------------------------------------------------------------------- */

void input_poll(InputSnapshot *snap)
{
    /* Reset all edge-triggered fields each frame */
    snap->rotate_cw      = 0;
    snap->rotate_ccw     = 0;
    snap->hard_drop      = 0;
    snap->hold           = 0;
    snap->pause          = 0;
    snap->menu_up        = 0;
    snap->menu_down      = 0;
    snap->menu_left      = 0;
    snap->menu_right     = 0;
    snap->back           = 0;
    snap->quit           = 0;
    snap->res_cycle      = 0;
    snap->settings       = 0;
    snap->text_backspace = 0;
    snap->text_confirm   = 0;

    SDL_Event e;
    while (SDL_PollEvent(&e)) {
        if (e.type == SDL_QUIT) {
            snap->quit = 1;
        }

        /* Text-input events (only in text mode) */
        if (s_text_mode && e.type == SDL_TEXTINPUT) {
            const char *ch = e.text.text;
            int len = (int)strlen(s_text_buf);
            for (int i = 0; ch[i] && len < 11; i++) {
                char c = (char)toupper((unsigned char)ch[i]);
                if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
                    s_text_buf[len++] = c;
                    s_text_buf[len]   = '\0';
                }
            }
        }

        if (e.type == SDL_KEYDOWN && e.key.repeat == 0) {
            if (s_text_mode) {
                /* In text mode only handle backspace and confirm */
                if (e.key.keysym.sym == SDLK_BACKSPACE) {
                    int len = (int)strlen(s_text_buf);
                    if (len > 0) s_text_buf[len - 1] = '\0';
                    snap->text_backspace = 1;
                }
                if (e.key.keysym.sym == SDLK_RETURN &&
                    strlen(s_text_buf) > 0)
                    snap->text_confirm = 1;
                /* Cancel text entry */
                if (e.key.keysym.sym == SDLK_ESCAPE ||
                    e.key.keysym.sym == SDLK_q)
                    snap->back = 1;
            } else {
                switch (e.key.keysym.sym) {
                    case SDLK_x:      snap->rotate_cw  = 1; break;
                    case SDLK_UP:     snap->rotate_cw  = 1; /* CW in-game */
                                      snap->menu_up    = 1; break;
                    case SDLK_z:      snap->rotate_ccw = 1; break;
                    case SDLK_SPACE:  snap->hard_drop  = 1; break;
                    case SDLK_c:
                    case SDLK_LSHIFT: snap->hold       = 1; break;
                    case SDLK_p:
                    case SDLK_ESCAPE: snap->pause      = 1; break;
                    case SDLK_DOWN:   snap->menu_down  = 1; break;
                    case SDLK_LEFT:   snap->menu_left  = 1; break;
                    case SDLK_RIGHT:  snap->menu_right = 1; break;
                    case SDLK_q:      snap->back       = 1; break;
                    case SDLK_r:      snap->res_cycle  = 1; break;
                    case SDLK_s:      snap->settings   = 1; break;
                    default: break;
                }
            }
        }
    }

    /* Continuous movement (DAS / ARR) — not active in text mode */
    if (!s_text_mode) {
        const uint8_t *ks = SDL_GetKeyboardState(NULL);
        repeat_update(&rk_left,  ks[SDL_SCANCODE_LEFT],  &snap->left);
        repeat_update(&rk_right, ks[SDL_SCANCODE_RIGHT], &snap->right);
        repeat_update(&rk_down,  ks[SDL_SCANCODE_DOWN],  &snap->down);
    } else {
        snap->left = snap->right = snap->down = 0;
        rk_left.held = rk_right.held = rk_down.held = 0;
        rk_left.das_fired = rk_right.das_fired = rk_down.das_fired = 0;
    }
}
