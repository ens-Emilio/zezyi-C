#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "online.h"
#include "rules.h"
#include "time_step.h"

static void set_disconnect(OnlineCtx *o, const char *msg) {
    snprintf(o->disconnect_reason, sizeof(o->disconnect_reason),
             "%.63s", msg && msg[0] ? msg : "disconnected");
}

void online_init(OnlineCtx *o) {
    memset(o, 0, sizeof(*o));
}

void online_disconnect(OnlineCtx *o) {
    if (o->net) {
        net_disconnect(o->net);
        o->net = NULL;
    }
}

static void go_to_menu(GameState *game, double *accumulator) {
    game_init_menu(game);
    *accumulator = 0.0;
}

int online_connect(OnlineCtx *o, const char *player_name, GameState *game) {
    const char *host = getenv("ZEZYI_HOST");
    if (!host || !host[0]) host = "localhost";
    const char *port_s = getenv("ZEZYI_PORT");
    int port = port_s ? atoi(port_s) : 8080;
    if (port <= 0 || port > 65535) port = 8080;
    const char *pname = (player_name && player_name[0]) ? player_name : "PLAYER";

    o->net = net_connect(host, port);
    if (!o->net) {
        snprintf(o->disconnect_reason, sizeof(o->disconnect_reason),
                 "offline: %s:%d", host, port);
        game->app_state = APP_ONLINE_DISCONNECT;
        return 0;
    }
    net_send_join(o->net, pname);
    net_create_room(o->net, 0);
    game->app_state = APP_ONLINE_LOBBY;
    return 1;
}

void online_lobby_tick(OnlineCtx *o, GameState *game, InputSnapshot *input,
                       double *accumulator, uint64_t *prev) {
    NetMessage msg;
    while (net_poll(o->net, &msg)) {
        if (msg.type == NC_MSG_START) {
            game_init_for_mode(game, msg.seed, MODE_MARATHON);
            strncpy(o->opponent_name, msg.opponent, sizeof(o->opponent_name) - 1);
            o->opponent_name[sizeof(o->opponent_name) - 1] = '\0';
            o->online_frame = 0;
            o->opp_input = 0;
            o->opp_input_frame = -1;
            o->opp_wait_since_ms = 0;
            game->app_state = APP_ONLINE_VS;
            *accumulator = 0.0;
            *prev = time_now_ms();
            return;
        }
        if (msg.type == NC_MSG_ERROR) {
            set_disconnect(o, msg.error_msg);
            online_disconnect(o);
            game->app_state = APP_ONLINE_DISCONNECT;
            return;
        }
    }
    if (o->net && net_state(o->net) == NC_ERROR) {
        set_disconnect(o, net_error(o->net));
        online_disconnect(o);
        game->app_state = APP_ONLINE_DISCONNECT;
        return;
    }
    if (input->back) {
        online_disconnect(o);
        game->app_state = APP_MENU;
    }
}

void online_vs_tick(OnlineCtx *o, GameState *game, InputSnapshot *input,
                    double *accumulator, uint64_t *prev) {
    (void)prev;

    NetMessage msg;
    while (net_poll(o->net, &msg)) {
        if (msg.type == NC_MSG_FRAME) {
            o->opp_input = msg.input;
            o->opp_input_frame = msg.frame;
        } else if (msg.type == NC_MSG_END) {
            game->app_state = APP_ONLINE_RESULT;
            return;
        } else if (msg.type == NC_MSG_ERROR) {
            set_disconnect(o, msg.error_msg);
            online_disconnect(o);
            game->app_state = APP_ONLINE_DISCONNECT;
            return;
        }
    }

    if (o->net && net_state(o->net) == NC_ERROR) {
        set_disconnect(o, net_error(o->net));
        online_disconnect(o);
        game->app_state = APP_ONLINE_DISCONNECT;
        return;
    }

    if (game->app_state == APP_ONLINE_VS &&
        o->net &&
        *accumulator >= (1.0 / 60.0)) {

        uint8_t local_input = 0;
        if (input->left)       local_input |= (1u << 0);
        if (input->right)      local_input |= (1u << 1);
        if (input->down)       local_input |= (1u << 2);
        if (input->rotate_cw)  local_input |= (1u << 3);
        if (input->rotate_ccw) local_input |= (1u << 4);
        if (input->hard_drop)  local_input |= (1u << 5);
        if (input->hold)       local_input |= (1u << 6);

        net_send_input(o->net, o->online_frame, local_input);

        if (o->opp_input_frame >= o->online_frame) {
            game_tick(game, input, 1.0 / 60.0);
            *accumulator -= 1.0 / 60.0;
            o->online_frame++;
            o->opp_wait_since_ms = 0;

            if (o->online_frame % 60 == 0) {
                uint32_t hash = (uint32_t)game->score ^
                                (uint32_t)game->total_lines ^
                                (uint32_t)game->tick;
                net_send_state(o->net, o->online_frame, hash);
            }
        } else {
            if (o->opp_wait_since_ms == 0)
                o->opp_wait_since_ms = time_now_ms();
            else if (time_now_ms() - o->opp_wait_since_ms > 5000) {
                set_disconnect(o, "timeout waiting for opponent");
                online_disconnect(o);
                game->app_state = APP_ONLINE_DISCONNECT;
                return;
            }
        }
    }

    if (input->back) {
        online_disconnect(o);
        go_to_menu(game, accumulator);
    }
}
