#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <SDL2/SDL.h>
#include "app.h"
#include "config.h"
#include "gameconfig.h"
#include "audio.h"
#include "rules.h"
#include "input.h"
#include "render_sdl.h"
#include "time_step.h"
#include "highscore.h"
#include "netcode.h"
#include "online.h"
#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#endif

#define CFG_PATH "zezyi.cfg"

/* Resolution table — index matches Resolution enum (desktop only) */
#ifndef __EMSCRIPTEN__
static const int RES_W[RES_COUNT] = { 854, 1280, 1920, 2560, 0 };
static const int RES_H[RES_COUNT] = { 480,  720, 1080, 1440, 0 };
#endif

#ifndef __EMSCRIPTEN__
static void get_window_size(int res, int *w, int *h)
{
    if (res == RES_FULLSCREEN) {
        SDL_DisplayMode dm;
        if (SDL_GetCurrentDisplayMode(0, &dm) == 0) {
            *w = dm.w;
            *h = dm.h;
        } else {
            *w = 1280; *h = 720;
        }
    } else {
        if (res < 0 || res >= RES_FULLSCREEN) res = RES_720P;
        *w = RES_W[res];
        *h = RES_H[res];
    }
}
#endif

#ifndef __EMSCRIPTEN__
static void apply_resolution(SDL_Window *window, int res, Layout *lay)
{
    int nw, nh;
    get_window_size(res, &nw, &nh);
    if (res == RES_FULLSCREEN) {
        SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
        SDL_GetWindowSize(window, &nw, &nh);
    } else {
        SDL_SetWindowFullscreen(window, 0);
        SDL_SetWindowSize(window, nw, nh);
        SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED);
    }
    layout_compute(lay, nw, nh);
    render_set_layout(lay);
}
#endif

/* -------------------------------------------------------------------------
 * Game-loop helpers
 * ---------------------------------------------------------------------- */

static void start_mode(GameState *game, InputSnapshot *input,
                       double *accumulator, uint64_t *prev)
{
    uint32_t seed = (uint32_t)(time_now_ms() & 0xFFFFFFFFu);
    if (seed == 0) seed = 1;
    game_init_for_mode(game, seed,
                       (GameMode)game->menu_cursor);
    if (accumulator) *accumulator = 0.0;
    *prev        = time_now_ms();
    input->hard_drop  = 0;
    input->rotate_cw  = 0;
    input->rotate_ccw = 0;
    input->menu_up    = 0;
    input->menu_down  = 0;
    input->back       = 0;
}

static void go_to_menu(GameState *game, double *accumulator)
{
    game_init_menu(game);
    *accumulator = 0.0;
}

static void resume_game(GameState *game, double *accumulator, uint64_t *prev)
{
    game->app_state = APP_PLAYING;
    game->paused    = 0;
    *accumulator    = 0.0;
    *prev           = time_now_ms();
}

static const char *bgm_for_mode(GameMode mode)
{
    switch (mode) {
        case MODE_SPRINT:   return "assets/bgm/theme_sprint.ogg";
        case MODE_ULTRA:    return "assets/bgm/theme_ultra.ogg";
        case MODE_BLITZ:    return "assets/bgm/theme_sprint.ogg"; /* reuse */
        case MODE_SURVIVAL: return "assets/bgm/theme_marathon.ogg";
        case MODE_CHEESE:   return "assets/bgm/theme_sprint.ogg"; /* reuse */
        case MODE_ZEN:      return "assets/bgm/theme_menu.ogg";   /* calm   */
        default:            return "assets/bgm/theme_marathon.ogg";
    }
}

static void dispatch_audio(const GameState *g)
{
    uint32_t ev = g->audio_events;
    if (!ev) return;

    if      (ev & AUDIO_EVT_TSPIN)   audio_play(SFX_TSPIN);
    else if (ev & AUDIO_EVT_QUAD)    audio_play(SFX_QUAD);
    else if (ev & AUDIO_EVT_TRIPLE)  audio_play(SFX_TRIPLE);
    else if (ev & AUDIO_EVT_DOUBLE)  audio_play(SFX_DOUBLE);
    else if (ev & AUDIO_EVT_SINGLE)  audio_play(SFX_SINGLE);
    else if (ev & AUDIO_EVT_LOCK)    audio_play(SFX_LOCK);

    if (ev & AUDIO_EVT_B2B)          audio_play(SFX_B2B);
    if (ev & AUDIO_EVT_ALL_CLEAR)    audio_play(SFX_ALL_CLEAR);
    if (ev & AUDIO_EVT_LEVEL_UP)     audio_play(SFX_LEVEL_UP);
    if (ev & AUDIO_EVT_HARD_DROP)    audio_play(SFX_HARD_DROP);
    if (ev & AUDIO_EVT_HOLD)         audio_play(SFX_HOLD);
    if (ev & AUDIO_EVT_GAME_OVER)    audio_play(SFX_GAME_OVER);

    if (!(ev & (AUDIO_EVT_LOCK | AUDIO_EVT_HARD_DROP))) {
        if (ev & AUDIO_EVT_ROTATE)    audio_play(SFX_ROTATE);
        if (ev & AUDIO_EVT_MOVE)      audio_play(SFX_MOVE);
        if (ev & AUDIO_EVT_SOFT_DROP) audio_play(SFX_SOFT_DROP);
    }
}

/* -------------------------------------------------------------------------
 * Settings helpers
 * ---------------------------------------------------------------------- */
#define SETTINGS_COUNT 12
#define SETTING_FIRST_ACTION 8
#define SETTING_EDIT_USER 8
#define SETTING_RESET_SCORES 9
#define SETTING_RESET_SETTINGS 10
#define SETTING_RESET_ALL 11

#define CONFIRM_NONE 0
#define CONFIRM_RESET_SCORES 1
#define CONFIRM_RESET_SETTINGS 2
#define CONFIRM_RESET_ALL 3

static void reset_scores(GameState *g)
{
    g->hs_marathon  = 0;
    g->hs_sprint_ms = 0;
    g->hs_ultra     = 0;
    g->hs_blitz     = 0;
    g->hs_survival  = 0;
    g->hs_cheese_ms = 0;
    hs_save(g);
}

static void reset_settings(GameConfig *cfg)
{
    char     pn[32];
    Language lang = cfg->language;
    strncpy(pn, cfg->player_name, sizeof(pn) - 1);
    pn[sizeof(pn) - 1] = '\0';

    gameconfig_defaults(cfg);
    strncpy(cfg->player_name, pn, sizeof(cfg->player_name) - 1);
    cfg->player_name[sizeof(cfg->player_name) - 1] = '\0';
    cfg->language = lang;
}

static void enter_confirm(GameState *g, int action, AppState return_state)
{
    g->confirm_action = action;
    g->confirm_return_state = return_state;
    g->app_state = APP_CONFIRM;
}

static void settings_change(GameConfig *cfg, int cursor, int dir)
{
    /* dir: +1 = right/increase, -1 = left/decrease */
    switch (cursor) {
        case 0: /* resolution */
            cfg->resolution = (cfg->resolution + dir + (int)RES_COUNT)
                              % (int)RES_COUNT;
            break;
        case 1: cfg->ghost      = !cfg->ghost;      break;
        case 2: cfg->grid_lines = !cfg->grid_lines;  break;
        case 3:
            cfg->sfx_volume += dir * 10;
            if (cfg->sfx_volume < 0)   cfg->sfx_volume = 0;
            if (cfg->sfx_volume > 128) cfg->sfx_volume = 128;
            break;
        case 4:
            cfg->bgm_volume += dir * 10;
            if (cfg->bgm_volume < 0)   cfg->bgm_volume = 0;
            if (cfg->bgm_volume > 128) cfg->bgm_volume = 128;
            break;
        case 5:
            cfg->das_ms += dir * 10;
            if (cfg->das_ms < 50)  cfg->das_ms = 50;
            if (cfg->das_ms > 300) cfg->das_ms = 300;
            break;
        case 6:
            cfg->arr_ms += dir * 5;
            if (cfg->arr_ms < 5)  cfg->arr_ms = 5;
            if (cfg->arr_ms > 100) cfg->arr_ms = 100;
            break;
        case 7: /* language */
            cfg->language = (Language)(((int)cfg->language + dir +
                            (int)LANG_COUNT) % (int)LANG_COUNT);
            break;
        default: break;
    }
}

/* -------------------------------------------------------------------------
 * Static app context (needed for Emscripten main loop callback)
 * ---------------------------------------------------------------------- */

typedef struct {
    GameConfig    cfg;
    GameState     game;
    InputSnapshot input;
    SDL_Window   *window;
    SDL_Renderer *renderer;
    Layout        lay;
    double        accumulator;
    uint64_t      prev;
    int           running;
    OnlineCtx     online;
} AppCtx;

static AppCtx s_ctx;

/* Forward declaration */
static void main_loop_iter(void);

/* -------------------------------------------------------------------------
 * app_run — init + launch main loop
 * ---------------------------------------------------------------------- */

int app_run(void)
{
    AppCtx *ctx = &s_ctx;

    gameconfig_load(&ctx->cfg, CFG_PATH);

    /* Force X11 on Linux — Wayland SDL2 driver drops keyboard events */
    setenv("SDL_VIDEODRIVER", "x11", 0);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    int win_w, win_h;
#ifdef __EMSCRIPTEN__
    /* Fixed canvas size on web — ignore resolution setting */
    win_w = 1280;
    win_h = 720;
#else
    get_window_size(ctx->cfg.resolution, &win_w, &win_h);
#endif

    if (render_init(&ctx->renderer, &ctx->window, win_w, win_h) != 0) {
        fprintf(stderr, "render_init failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

#ifndef __EMSCRIPTEN__
    if (ctx->cfg.resolution == RES_FULLSCREEN)
        SDL_SetWindowFullscreen(ctx->window, SDL_WINDOW_FULLSCREEN_DESKTOP);
#endif

    layout_compute(&ctx->lay, win_w, win_h);
    render_set_layout(&ctx->lay);

    input_set_timing(ctx->cfg.das_ms, ctx->cfg.arr_ms);
    render_set_options(ctx->cfg.ghost, ctx->cfg.grid_lines);

    audio_init();
    audio_set_sfx_volume(ctx->cfg.sfx_volume);
    audio_set_bgm_volume(ctx->cfg.bgm_volume);

    game_init_menu(&ctx->game);

    /* Copy player name from config into game state for display */
    strncpy(ctx->game.player_name, ctx->cfg.player_name,
            sizeof(ctx->game.player_name) - 1);
    ctx->game.player_name[sizeof(ctx->game.player_name) - 1] = '\0';
    hs_load(&ctx->game);

    /* First-run: no player name → go to name entry */
    if (!ctx->cfg.player_name[0]) {
        ctx->game.app_state = APP_NAME_ENTRY;
        input_start_text("");
        audio_play_bgm("assets/bgm/theme_menu.ogg", -1);
    } else {
        audio_play_bgm("assets/bgm/theme_menu.ogg", -1);
    }
    ctx->accumulator = 0.0;
    ctx->prev        = time_now_ms();
    ctx->running     = 1;
    online_init(&ctx->online);

#ifdef __EMSCRIPTEN__
    emscripten_set_main_loop(main_loop_iter, 0, 1);
    return 0; /* unreachable — emscripten_set_main_loop does not return */
#else
    while (ctx->running && !ctx->input.quit) {
        main_loop_iter();
    }
    online_disconnect(&ctx->online);
    audio_shutdown();
    render_shutdown(ctx->renderer, ctx->window);
    SDL_Quit();
    return 0;
#endif
}

/* -------------------------------------------------------------------------
 * main_loop_iter — one frame of the game loop
 * ---------------------------------------------------------------------- */

static void main_loop_iter(void)
{
    AppCtx *ctx = &s_ctx;

    uint64_t curr       = time_now_ms();
    double   frame_time = (double)(curr - ctx->prev) / 1000.0;
    ctx->prev = curr;
    if (frame_time > 0.25) frame_time = 0.25;
    ctx->accumulator += frame_time;

    input_poll(&ctx->input);

    /* Pause when window loses focus during gameplay (desktop only) */
#ifndef __EMSCRIPTEN__
    if (ctx->game.app_state == APP_PLAYING) {
        uint32_t wflags = SDL_GetWindowFlags(ctx->window);
        if ((wflags & SDL_WINDOW_INPUT_FOCUS) == 0 && (wflags & SDL_WINDOW_SHOWN)) {
            ctx->game.app_state = APP_PAUSED;
            ctx->game.paused    = 1;
            ctx->accumulator    = 0.0;
            goto after_switch;
        }
    }
#endif

    if (ctx->input.quit) {
        ctx->running = 0;
#ifdef __EMSCRIPTEN__
        audio_shutdown();
        render_shutdown(ctx->renderer, ctx->window);
        SDL_Quit();
        emscripten_cancel_main_loop();
#endif
        return;
    }

    AppState prev_state = ctx->game.app_state;

    switch (ctx->game.app_state) {
        /* ---- Name Entry -------------------------------------------- */
        case APP_NAME_ENTRY:
            if (ctx->input.back) {
                input_stop_text();
                ctx->game.app_state = ctx->game.name_return_state;
                if (ctx->game.app_state == 0) ctx->game.app_state = APP_MENU;
                ctx->game.name_return_state = APP_MENU;
                break;
            }
            if (ctx->input.text_confirm) {
                const char *name = input_text_get();
                strncpy(ctx->cfg.player_name, name,
                        sizeof(ctx->cfg.player_name) - 1);
                ctx->cfg.player_name[sizeof(ctx->cfg.player_name) - 1] = '\0';
                strncpy(ctx->game.player_name, name,
                        sizeof(ctx->game.player_name) - 1);
                ctx->game.player_name[sizeof(ctx->game.player_name) - 1] = '\0';
                gameconfig_save(&ctx->cfg, CFG_PATH);
                input_stop_text();
                hs_load(&ctx->game);
                ctx->game.app_state = ctx->game.name_return_state;
                if (ctx->game.app_state == 0) ctx->game.app_state = APP_MENU;
                ctx->game.name_return_state = APP_MENU;
            }
            break;

        /* ---- Main Menu -------------------------------------------- */
        case APP_MENU:
            if (ctx->input.back) { ctx->running = 0; break; }

            if (ctx->input.menu_down)
                ctx->game.menu_cursor =
                    (ctx->game.menu_cursor + 1) % (int)MODE_COUNT;
            if (ctx->input.menu_up)
                ctx->game.menu_cursor =
                    (ctx->game.menu_cursor + (int)MODE_COUNT - 1)
                    % (int)MODE_COUNT;

            /* SPACE → mode info screen (preview before game starts) */
            if (ctx->input.hard_drop) {
                ctx->game.app_state = APP_MODE_INFO;
                break;
            }

            /* Open settings */
            if (ctx->input.settings) {
                ctx->game.app_state = APP_SETTINGS;
                ctx->game.settings_cursor = 0;
                break;
            }

            /* Open online menu */
            if (ctx->input.rotate_cw) {
                ctx->game.app_state = APP_ONLINE_MENU;
                break;
            }

#ifndef __EMSCRIPTEN__
            /* Resolution cycling (desktop only) */
            if (ctx->input.res_cycle) {
                ctx->cfg.resolution =
                    (ctx->cfg.resolution + 1) % (int)RES_COUNT;
                apply_resolution(ctx->window, ctx->cfg.resolution, &ctx->lay);
            }
#endif
            break;

        /* ---- Settings -------------------------------------------- */
        case APP_SETTINGS: {
            if (ctx->input.menu_up)
                ctx->game.settings_cursor =
                    (ctx->game.settings_cursor + SETTINGS_COUNT - 1)
                    % SETTINGS_COUNT;
            if (ctx->input.menu_down)
                ctx->game.settings_cursor =
                    (ctx->game.settings_cursor + 1) % SETTINGS_COUNT;

            if (ctx->game.settings_cursor < SETTING_FIRST_ACTION) {
                if (ctx->input.menu_left)
                    settings_change(&ctx->cfg, ctx->game.settings_cursor, -1);
                if (ctx->input.menu_right || ctx->input.hard_drop)
                    settings_change(&ctx->cfg, ctx->game.settings_cursor, +1);
            } else if (ctx->input.hard_drop) {
                if (ctx->game.settings_cursor == SETTING_EDIT_USER) {
                    ctx->game.name_return_state = APP_SETTINGS;
                    input_start_text(ctx->cfg.player_name);
                    ctx->game.app_state = APP_NAME_ENTRY;
                    break;
                }
                if (ctx->game.settings_cursor == SETTING_RESET_SCORES) {
                    enter_confirm(&ctx->game, CONFIRM_RESET_SCORES,
                                  APP_SETTINGS);
                    break;
                }
                if (ctx->game.settings_cursor == SETTING_RESET_SETTINGS) {
                    enter_confirm(&ctx->game, CONFIRM_RESET_SETTINGS,
                                  APP_SETTINGS);
                    break;
                }
                if (ctx->game.settings_cursor == SETTING_RESET_ALL) {
                    enter_confirm(&ctx->game, CONFIRM_RESET_ALL, APP_SETTINGS);
                    break;
                }
            }

            /* Apply live changes */
            render_set_options(ctx->cfg.ghost, ctx->cfg.grid_lines);
            input_set_timing(ctx->cfg.das_ms, ctx->cfg.arr_ms);
            audio_set_sfx_volume(ctx->cfg.sfx_volume);
            audio_set_bgm_volume(ctx->cfg.bgm_volume);
#ifndef __EMSCRIPTEN__
            if (ctx->game.settings_cursor == 0)
                apply_resolution(ctx->window, ctx->cfg.resolution, &ctx->lay);
#endif

            if (ctx->input.back) {
                gameconfig_save(&ctx->cfg, CFG_PATH);
                ctx->game.app_state = APP_MENU;
            }
            break;
        }

        /* ---- Confirm ------------------------------------------------ */
        case APP_CONFIRM:
            if (ctx->input.back) {
                ctx->game.confirm_action = CONFIRM_NONE;
                ctx->game.app_state = ctx->game.confirm_return_state;
                break;
            }
            if (ctx->input.hard_drop) {
                if (ctx->game.confirm_action == CONFIRM_RESET_SCORES) {
                    reset_scores(&ctx->game);
                } else if (ctx->game.confirm_action == CONFIRM_RESET_SETTINGS) {
                    reset_settings(&ctx->cfg);
                    gameconfig_save(&ctx->cfg, CFG_PATH);
                    render_set_options(ctx->cfg.ghost, ctx->cfg.grid_lines);
                    input_set_timing(ctx->cfg.das_ms, ctx->cfg.arr_ms);
                    audio_set_sfx_volume(ctx->cfg.sfx_volume);
                    audio_set_bgm_volume(ctx->cfg.bgm_volume);
#ifndef __EMSCRIPTEN__
                    apply_resolution(ctx->window, ctx->cfg.resolution,
                                     &ctx->lay);
#endif
                } else if (ctx->game.confirm_action == CONFIRM_RESET_ALL) {
                    reset_scores(&ctx->game);
                    reset_settings(&ctx->cfg);
                    ctx->cfg.player_name[0] = '\0';
                    gameconfig_save(&ctx->cfg, CFG_PATH);
                    render_set_options(ctx->cfg.ghost, ctx->cfg.grid_lines);
                    input_set_timing(ctx->cfg.das_ms, ctx->cfg.arr_ms);
                    audio_set_sfx_volume(ctx->cfg.sfx_volume);
                    audio_set_bgm_volume(ctx->cfg.bgm_volume);
#ifndef __EMSCRIPTEN__
                    apply_resolution(ctx->window, ctx->cfg.resolution,
                                     &ctx->lay);
#endif
                    ctx->game.player_name[0] = '\0';
                    ctx->game.name_return_state = APP_MENU;
                    input_start_text("");
                    ctx->game.confirm_action = CONFIRM_NONE;
                    ctx->game.app_state = APP_NAME_ENTRY;
                    break;
                }
                ctx->game.confirm_action = CONFIRM_NONE;
                ctx->game.app_state = ctx->game.confirm_return_state;
            }
            break;

        /* ---- Mode Info -------------------------------------------- */
        case APP_MODE_INFO:
            if (ctx->input.hard_drop)
                start_mode(&ctx->game, &ctx->input,
                           &ctx->accumulator, &ctx->prev);
            if (ctx->input.back)
                ctx->game.app_state = APP_MENU;
            break;

        /* ---- Playing ---------------------------------------------- */
        case APP_PLAYING:
            if (ctx->input.pause) {
                ctx->game.app_state = APP_PAUSED;
                ctx->game.paused    = 1;
                ctx->accumulator    = 0.0;
            }
            if (ctx->input.back)
                go_to_menu(&ctx->game, &ctx->accumulator);
            break;

        /* ---- Paused ----------------------------------------------- */
        case APP_PAUSED:
            if (ctx->input.pause || ctx->input.hard_drop)
                resume_game(&ctx->game, &ctx->accumulator, &ctx->prev);
            if (ctx->input.back)
                go_to_menu(&ctx->game, &ctx->accumulator);
            break;

        case APP_GAME_OVER:
            if (ctx->input.hard_drop) {
                uint32_t seed = (uint32_t)(time_now_ms() & 0xFFFFFFFFu);
                if (seed == 0) seed = 1;
                game_init_for_mode(&ctx->game, seed,
                                   ctx->game.mode);
                ctx->accumulator = 0.0;
                ctx->prev        = time_now_ms();
            }
            if (ctx->input.back)
                go_to_menu(&ctx->game, &ctx->accumulator);
            break;

        /* ---- Results ---------------------------------------------- */
        case APP_RESULTS:
            if (ctx->input.hard_drop || ctx->input.back)
                go_to_menu(&ctx->game, &ctx->accumulator);
            break;

        /* ---- Online Menu ----------------------------------------- */
        case APP_ONLINE_MENU:
            if (ctx->input.back) {
                ctx->game.app_state = APP_MENU;
                break;
            }
            if (ctx->input.hard_drop) {
                online_connect(&ctx->online, ctx->cfg.player_name, &ctx->game);
                break;
            }
            break;

        case APP_ONLINE_LOBBY:
            online_lobby_tick(&ctx->online, &ctx->game, &ctx->input,
                              &ctx->accumulator, &ctx->prev);
            break;

        case APP_ONLINE_VS:
            online_vs_tick(&ctx->online, &ctx->game, &ctx->input,
                           &ctx->accumulator, &ctx->prev);
            break;

        case APP_ONLINE_RESULT:
            if (ctx->input.hard_drop || ctx->input.back) {
                online_disconnect(&ctx->online);
                go_to_menu(&ctx->game, &ctx->accumulator);
            }
            break;
        case APP_ONLINE_DISCONNECT:
            if (ctx->input.hard_drop || ctx->input.back) {
                online_disconnect(&ctx->online);
                ctx->online.disconnect_reason[0] = '\0';
                go_to_menu(&ctx->game, &ctx->accumulator);
            }
            break;
    }

after_switch:
    ctx->game.audio_events = 0;
    while (ctx->accumulator >= (1.0 / 60.0) &&
           ctx->game.app_state == APP_PLAYING) {
        game_tick(&ctx->game, &ctx->input, 1.0 / 60.0);
        ctx->accumulator -= 1.0 / 60.0;
    }
    if (ctx->game.app_state != APP_PLAYING)
        ctx->accumulator = 0.0;

    dispatch_audio(&ctx->game);

    /* State-transition audio */
    if (ctx->game.app_state != prev_state) {
        switch (ctx->game.app_state) {
            case APP_NAME_ENTRY:
            case APP_SETTINGS:
            case APP_CONFIRM:
            case APP_MODE_INFO:
            case APP_MENU:
            case APP_ONLINE_MENU:
            case APP_ONLINE_LOBBY:
                audio_play_bgm("assets/bgm/theme_menu.ogg", -1);
                break;
            case APP_PLAYING:
            case APP_ONLINE_VS:
                if (prev_state == APP_PAUSED)
                    audio_resume_bgm();
                else
                    audio_play_bgm(bgm_for_mode(ctx->game.mode), -1);
                break;
            case APP_PAUSED:
                audio_pause_bgm();
                break;
            case APP_GAME_OVER:
            case APP_RESULTS:
            case APP_ONLINE_RESULT:
            case APP_ONLINE_DISCONNECT:
                audio_stop_bgm();
                break;
        }
    }

    render_game(ctx->renderer, &ctx->game, &ctx->cfg);

    /* On web, quit means stop the loop cleanly */
#ifdef __EMSCRIPTEN__
    if (!ctx->running) {
        audio_shutdown();
        render_shutdown(ctx->renderer, ctx->window);
        SDL_Quit();
        emscripten_cancel_main_loop();
    }
#endif
}
