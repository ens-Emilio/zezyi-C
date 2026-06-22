#pragma once
#include "config.h"

#define REPLAY_MAX_FRAMES 36000

typedef uint8_t ReplayFrame;

typedef struct {
    uint32_t    seed;
    GameMode    mode;
    int         frame_count;
    ReplayFrame frames[REPLAY_MAX_FRAMES];
} ReplayRecorder;

typedef struct {
    uint32_t    seed;
    GameMode    mode;
    int         frame_count;
    ReplayFrame frames[REPLAY_MAX_FRAMES];
} ReplayPlayer;

void replay_init(ReplayRecorder *rec);
void replay_record(ReplayRecorder *rec, const InputSnapshot *in, int frame);

int  replay_save(const ReplayRecorder *rec, const char *path);
int  replay_load(ReplayPlayer *player, const char *path);

void replay_get_input(const ReplayPlayer *player, int frame, InputSnapshot *out);
