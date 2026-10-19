#pragma once
#include "config.h"

typedef enum {
    NC_DISCONNECTED = 0,
    NC_CONNECTING,
    NC_CONNECTED,
    NC_IN_ROOM,
    NC_IN_MATCH,
    NC_ERROR
} NetState;

typedef struct NetContext NetContext;

NetContext *net_connect(const char *host, int port);
void net_disconnect(NetContext *nc);
NetState net_state(const NetContext *nc);
const char *net_error(const NetContext *nc);

int net_send_join(NetContext *nc, const char *player_name);
int net_create_room(NetContext *nc, int private);
int net_join_room(NetContext *nc, const char *room_id);
int net_send_ready(NetContext *nc, int ready);
int net_send_input(NetContext *nc, int frame, uint8_t input_bitmask);
int net_send_state(NetContext *nc, int frame, uint32_t hash);
int net_send_garbage(NetContext *nc, int frame, int lines, int hole_col);

typedef enum {
    NC_MSG_NONE = 0,
    NC_MSG_ROOM_STATE,
    NC_MSG_START,
    NC_MSG_FRAME,
    NC_MSG_STATE_ACK,
    NC_MSG_END,
    NC_MSG_ERROR,
    NC_MSG_GARBAGE,
    NC_MSG_RANKED_MATCH,
    NC_MSG_MATCH_RESULT,
    NC_MSG_LEADERBOARD,
    NC_MSG_TOURNAMENT_STATE,
    NC_MSG_SPECTATE_FRAME
} NetMsgType;

typedef struct {
    NetMsgType type;
    char room_id[16];
    char player_names[2][32];
    int  player_ready[2];
    int  player_count;
    uint32_t seed;
    int     mode;
    char    opponent[32];
    int     frame;
    uint8_t input;
    int     valid;
    char    winner[32];
    char    reason[32];
    char    error_msg[128];
    int     garbage_lines;
    int     garbage_hole_col;
} NetMessage;

int net_poll(NetContext *nc, NetMessage *out);
