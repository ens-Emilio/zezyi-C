#include <stdio.h>
#include <string.h>
#ifdef ZEZYI_NO_MIXER
// Mixer not available — stub audio (no sound, no link dep)
#include "audio.h"
int audio_init(void){return 0;}
void audio_shutdown(void){}
void audio_play(SfxId id){(void)id;}
void audio_play_bgm(const char *a,int b){(void)a;(void)b;}
void audio_stop_bgm(void){}
void audio_pause_bgm(void){}
void audio_resume_bgm(void){}
void audio_set_sfx_volume(int v){(void)v;}
void audio_set_bgm_volume(int v){(void)v;}
#else
#include <SDL2/SDL_mixer.h>
#include "audio.h"

static Mix_Chunk *sfx[SFX_COUNT];
static Mix_Music *bgm        = NULL;
static int        g_init_ok  = 0;

static const char *SFX_PATHS[SFX_COUNT] = {
    "assets/sfx/move.wav",
    "assets/sfx/rotate.wav",
    "assets/sfx/wall.wav",
    "assets/sfx/soft_drop.wav",
    "assets/sfx/hard_drop.wav",
    "assets/sfx/lock.wav",
    "assets/sfx/single.wav",
    "assets/sfx/double.wav",
    "assets/sfx/triple.wav",
    "assets/sfx/quad.wav",
    "assets/sfx/tspin.wav",
    "assets/sfx/b2b.wav",
    "assets/sfx/all_clear.wav",
    "assets/sfx/level_up.wav",
    "assets/sfx/game_over.wav",
    "assets/sfx/hold.wav",
};

int audio_init(void)
{
    if (Mix_OpenAudio(44100, MIX_DEFAULT_FORMAT, 2, 512) < 0) {
        fprintf(stderr, "audio: Mix_OpenAudio failed: %s\n", Mix_GetError());
        return -1;
    }
    Mix_AllocateChannels(16);
    memset(sfx, 0, sizeof(sfx));

    for (int i = 0; i < SFX_COUNT; i++) {
        sfx[i] = Mix_LoadWAV(SFX_PATHS[i]);
        /* Missing files are silently skipped — game still runs */
    }

    g_init_ok = 1;
    return 0;
}

void audio_shutdown(void)
{
    if (!g_init_ok) return;
    for (int i = 0; i < SFX_COUNT; i++) {
        if (sfx[i]) { Mix_FreeChunk(sfx[i]); sfx[i] = NULL; }
    }
    if (bgm) { Mix_FreeMusic(bgm); bgm = NULL; }
    Mix_CloseAudio();
    Mix_Quit();
    g_init_ok = 0;
}

void audio_play(SfxId id)
{
    if (!g_init_ok) return;
    if (id < 0 || id >= SFX_COUNT || !sfx[id]) return;
    Mix_PlayChannel(-1, sfx[id], 0);
}

/* Try loading music with the given path, then with common extension variants */
static Mix_Music *try_load_music(const char *path)
{
    static const char *EXTS[] = {".wav", ".ogg", ".mp3", ".flac", NULL};

    Mix_Music *m = Mix_LoadMUS(path);
    if (m) return m;

    /* Strip extension (if any) and retry with each candidate */
    char buf[512];
    const char *dot = strrchr(path, '.');
    const char *sep = strrchr(path, '/');
    /* Only strip if the dot belongs to the filename, not a path component */
    size_t base = (dot && (!sep || dot > sep))
                  ? (size_t)(dot - path)
                  : strlen(path);
    if (base >= sizeof(buf) - 6) return NULL;

    for (int i = 0; EXTS[i]; i++) {
        memcpy(buf, path, base);
        buf[base] = '\0';
        strncat(buf, EXTS[i], sizeof(buf) - base - 1);
        m = Mix_LoadMUS(buf);
        if (m) return m;
    }
    return NULL;
}

void audio_play_bgm(const char *path, int loops)
{
    if (!g_init_ok) return;
    if (bgm) { Mix_HaltMusic(); Mix_FreeMusic(bgm); bgm = NULL; }
    bgm = try_load_music(path);
    if (!bgm) return; /* Missing file — no BGM, game continues */
    Mix_PlayMusic(bgm, loops);
}

void audio_stop_bgm(void)
{
    if (!g_init_ok) return;
    Mix_HaltMusic();
}

void audio_pause_bgm(void)
{
    if (!g_init_ok) return;
    Mix_PauseMusic();
}

void audio_resume_bgm(void)
{
    if (!g_init_ok) return;
    Mix_ResumeMusic();
}

void audio_set_sfx_volume(int vol)
{
    if (!g_init_ok) return;
    Mix_Volume(-1, vol);
}

void audio_set_bgm_volume(int vol)
{
    if (!g_init_ok) return;
    Mix_VolumeMusic(vol);
}
#endif
