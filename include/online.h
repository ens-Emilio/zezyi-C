#pragma once
#include "config.h"
#include "netcode.h"
#include <stdint.h>

typedef struct {
    NetContext *net;
    int         online_frame;
    uint8_t     opp_input;
    int         opp_input_frame;
    uint64_t    opp_wait_since_ms;
    char        room_id[16];
    char        opponent_name[32];
    char        disconnect_reason[64];
} OnlineCtx;

void online_init(OnlineCtx *o);
void online_disconnect(OnlineCtx *o);

/* Returns 1 if connection succeeded and moved to LOBBY, 0 on failure (state -> DISCONNECT). */
int  online_connect(OnlineCtx *o, const char *player_name, GameState *game);

/* Poll lobby state; handles START / ERROR / net ERROR / back input. */
void online_lobby_tick(OnlineCtx *o, GameState *game, InputSnapshot *input,
                       double *accumulator, uint64_t *prev);

/* Full VS tick: poll, check errors, lockstep advance with 5s timeout, handle back. */
void online_vs_tick(OnlineCtx *o, GameState *game, InputSnapshot *input,
                    double *accumulator, uint64_t *prev);
