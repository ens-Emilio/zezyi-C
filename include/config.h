#pragma once
#include <stdint.h>

#define BOARD_W      10
#define BOARD_H      20
#define PIECE_DIM     4
#define QUEUE_CAP    14
#define NEXT_VISIBLE  5
#define CELL_PX      32

/* Lock-delay constants */
#define LOCK_RESET_MAX   15
#define LOCK_DELAY       0.5
#define LINE_CLEAR_TICKS  9   /* ~0.15 s at 60 fps */

/* Audio event bitmask — set by rules.c, dispatched by app.c */
#define AUDIO_EVT_MOVE       (1u <<  0)
#define AUDIO_EVT_ROTATE     (1u <<  1)
#define AUDIO_EVT_WALL_HIT   (1u <<  2)
#define AUDIO_EVT_SOFT_DROP  (1u <<  3)
#define AUDIO_EVT_HARD_DROP  (1u <<  4)
#define AUDIO_EVT_LOCK       (1u <<  5)
#define AUDIO_EVT_SINGLE     (1u <<  6)
#define AUDIO_EVT_DOUBLE     (1u <<  7)
#define AUDIO_EVT_TRIPLE     (1u <<  8)
#define AUDIO_EVT_QUAD       (1u <<  9)
#define AUDIO_EVT_TSPIN      (1u << 10)
#define AUDIO_EVT_B2B        (1u << 11)
#define AUDIO_EVT_ALL_CLEAR  (1u << 12)
#define AUDIO_EVT_LEVEL_UP   (1u << 13)
#define AUDIO_EVT_GAME_OVER  (1u << 14)
#define AUDIO_EVT_HOLD       (1u << 15)

/* Game-mode goals */
#define SPRINT_GOAL   40
#define ULTRA_TIME   120.0
#define BLITZ_TIME    40.0    /* Blitz: 40-second score sprint  */
#define CHEESE_LINES  10      /* Cheese: clear 10 garbage lines */

typedef enum {
    RES_480P = 0,
    RES_720P,
    RES_1080P,
    RES_1440P,
    RES_FULLSCREEN,
    RES_COUNT
} Resolution;

typedef enum {
    LANG_EN = 0,
    LANG_PTBR,
    LANG_ES,
    LANG_COUNT
} Language;

typedef enum {
    CELL_EMPTY = 0,
    CELL_I, CELL_O, CELL_T,
    CELL_S, CELL_Z, CELL_J, CELL_L,
    CELL_GARBAGE   /* grey filler for Cheese garbage rows */
} Cell;

typedef enum {
    PIECE_I = 0,
    PIECE_O, PIECE_T, PIECE_S,
    PIECE_Z, PIECE_J, PIECE_L,
    PIECE_COUNT
} PieceKind;

typedef enum {
    MODE_MARATHON = 0,
    MODE_SPRINT,
    MODE_ULTRA,
    MODE_BLITZ,    /* 40-second best-score sprint   */
    MODE_ZEN,      /* infinite, no game-over        */
    MODE_SURVIVAL, /* speed ramps every 3 lines     */
    MODE_CHEESE,   /* race to clear 10 garbage rows */
    MODE_COUNT
} GameMode;

typedef enum {
    APP_MENU = 0,
    APP_NAME_ENTRY,  /* first-run: player types their name      */
    APP_SETTINGS,    /* settings screen                         */
    APP_CONFIRM,     /* confirm destructive actions             */
    APP_MODE_INFO,   /* per-mode info screen before game starts */
    APP_PLAYING,
    APP_PAUSED,
    APP_GAME_OVER,
    APP_RESULTS
} AppState;

/* Contextual tips — set by rules.c, displayed by render_sdl.c */
typedef enum {
    TIP_NONE = 0,
    TIP_TRY_HOLD,     /* hold slot empty after 5 pieces placed */
    TIP_DANGER,       /* stack reaches top quarter of board    */
    TIP_COMBO_FIRE,   /* combo ≥ 4                             */
    TIP_FIRST_QUAD,   /* player just cleared first 4-line clear */
    TIP_COUNT
} TipId;

typedef enum {
    PHASE_FALLING = 0,
    PHASE_LINE_CLEAR_ANIM
} GamePhase;

typedef struct {
    uint8_t cells[BOARD_H][BOARD_W];
} Board;

typedef struct {
    PieceKind kind;
    int rotation, x, y;
} ActivePiece;

typedef struct {
    PieceKind data[QUEUE_CAP];
    int head, tail, count;
    uint32_t rng_state;
} PieceQueue;

typedef struct {
    /* Continuous / DAS+ARR */
    int left, right, down;
    /* Edge-triggered game actions */
    int rotate_cw, rotate_ccw;
    int hard_drop, hold, pause;
    /* Edge-triggered menu/navigation */
    int menu_up;    /* UP arrow                        */
    int menu_down;  /* DOWN arrow (edge, for menus)    */
    int menu_left;  /* LEFT arrow (edge, for settings) */
    int menu_right; /* RIGHT arrow (edge, for settings)*/
    int back;       /* Q — voltar ao menu              */
    int quit;       /* SDL_QUIT only                   */
    int res_cycle;  /* R — cycle resolution (menu)     */
    int settings;   /* S — open settings screen        */
    /* Text-input mode */
    int text_backspace;
    int text_confirm;
} InputSnapshot;

typedef struct {
    Board board;
    ActivePiece current, ghost;
    PieceQueue queue;
    PieceKind hold;
    int hold_used;
    uint64_t tick;
    GameMode mode;

    /* Score / progress */
    int score, level, total_lines;
    int combo;         /* consecutive clear streak (-1 = broken) */
    int b2b;           /* back-to-back flag */

    /* Last-lock decorators (valid until next lock) */
    int tspin;         /* last lock was a T-spin */
    int all_clear;     /* board was empty after last clear */

    /* Timing */
    double gravity_accum, lock_accum;
    double time_elapsed;         /* seconds from game start */
    int touching_ground;
    int lock_reset_count;
    int last_rot;                /* last action was a rotation (T-spin helper) */

    /* Drop scoring */
    int soft_drop_cells;
    int hard_drop_cells;

    /* IRS / IHS buffers */
    int pending_irs;
    int pending_ihs;

    /* App state */
    AppState app_state;
    int game_over;   /* mirrors app_state == APP_GAME_OVER */
    int paused;      /* mirrors app_state == APP_PAUSED */
    int menu_cursor; /* current menu selection (0..MODE_COUNT-1) */

    /* High scores (loaded at startup, saved on new record) */
    int hs_marathon;    /* best score */
    int hs_sprint_ms;   /* best Sprint time in ms (0 = never completed) */
    int hs_ultra;       /* best Ultra score */
    int hs_blitz;       /* best Blitz score */
    int hs_survival;    /* best Survival score */
    int hs_cheese_ms;   /* best Cheese time in ms (0 = never completed) */

    /* Player name (loaded from config) */
    char player_name[32];

    /* Line-clear animation */
    GamePhase game_phase;
    int       clear_timer;    /* counts 0 → LINE_CLEAR_TICKS */
    uint32_t  clearing_rows;  /* bit y set ↔ row y is being cleared */

    /* Settings screen cursor */
    int settings_cursor;

    /* Name-entry return state (e.g., edit user from settings) */
    AppState name_return_state;

    /* Confirm dialog */
    int      confirm_action;
    AppState confirm_return_state;

    /* Contextual tip — set by rules.c, displayed by render_sdl.c */
    TipId tip_id;
    uint8_t tip_shown; /* bitmask of TipId values already triggered */

    /* Per-game statistics */
    int stat_pieces[7];  /* pieces placed per PieceKind  */
    int stat_quads;      /* 4-line clears                */
    int stat_tspins;     /* T-spin clears (any lines)    */
    int stat_max_combo;  /* highest combo streak reached */
    int stat_actions;    /* total inputs (for APM)       */

    /* Audio events — set by rules.c, zeroed + dispatched by app.c each frame */
    uint32_t  audio_events;
} GameState;
