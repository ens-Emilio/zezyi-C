#pragma once
#include "config.h"

typedef struct {
    int das_ms;          /* delayed auto-shift   (ms)  — default 140 */
    int arr_ms;          /* auto-repeat rate     (ms)  — default 30  */
    int ghost;           /* 1 = show ghost piece        — default 1  */
    int grid_lines;      /* 1 = draw board grid lines   — default 0  */
    int sfx_volume;      /* SFX volume  0..128          — default 100 */
    int bgm_volume;      /* BGM volume  0..128          — default 70  */
    int resolution;      /* Resolution enum 0..4        — default 1 (720p) */
    Language language;   /* UI language                 — default EN */
    char player_name[32]; /* player display name        — default ""  */
} GameConfig;

void gameconfig_defaults(GameConfig *cfg);
void gameconfig_clamp(GameConfig *cfg);
void gameconfig_load(GameConfig *cfg, const char *path);
int  gameconfig_save(const GameConfig *cfg, const char *path);
