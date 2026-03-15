#pragma once

typedef enum {
    SFX_MOVE = 0,
    SFX_ROTATE,
    SFX_WALL_HIT,
    SFX_SOFT_DROP,
    SFX_HARD_DROP,
    SFX_LOCK,
    SFX_SINGLE,
    SFX_DOUBLE,
    SFX_TRIPLE,
    SFX_QUAD,
    SFX_TSPIN,
    SFX_B2B,
    SFX_ALL_CLEAR,
    SFX_LEVEL_UP,
    SFX_GAME_OVER,
    SFX_HOLD,
    SFX_COUNT
} SfxId;

int  audio_init(void);
void audio_shutdown(void);
void audio_play(SfxId id);
void audio_play_bgm(const char *path, int loops); /* loops=-1 = infinite */
void audio_stop_bgm(void);
void audio_pause_bgm(void);
void audio_resume_bgm(void);
void audio_set_sfx_volume(int vol);   /* 0..128 */
void audio_set_bgm_volume(int vol);   /* 0..128 */
