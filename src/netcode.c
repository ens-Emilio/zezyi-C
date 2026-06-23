#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <fcntl.h>
#include "netcode.h"

struct NetContext {
    int      sockfd;
    NetState state;
    char     error[128];
    char     recv_buf[4096];
    int      recv_len;
};

static void set_error(NetContext *nc, const char *msg)
{
    strncpy(nc->error, msg, sizeof(nc->error) - 1);
    nc->error[sizeof(nc->error) - 1] = '\0';
    nc->state = NC_ERROR;
}

static int send_line(NetContext *nc, const char *json)
{
    size_t len = strlen(json);
    if (write(nc->sockfd, json, len) != (ssize_t)len)
        return -1;
    if (write(nc->sockfd, "\n", (size_t)1) != 1)
        return -1;
    return 0;
}

NetContext *net_connect(const char *host, int port)
{
    NetContext *nc = calloc(1, sizeof(*nc));
    if (!nc) return NULL;

    nc->state = NC_CONNECTING;
    nc->sockfd = -1;

    struct hostent *he = gethostbyname(host);
    if (!he) {
        set_error(nc, "host resolution failed");
        free(nc);
        return NULL;
    }

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        set_error(nc, "socket creation failed");
        free(nc);
        return NULL;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], (size_t)he->h_length);

    if (connect(fd, (struct sockaddr *)&addr, (socklen_t)sizeof(addr)) < 0) {
        close(fd);
        set_error(nc, "connection refused");
        free(nc);
        return NULL;
    }

    /* Set socket to non-blocking for net_poll */
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);

    nc->sockfd = fd;
    nc->state = NC_CONNECTED;
    return nc;
}

void net_disconnect(NetContext *nc)
{
    if (!nc) return;
    if (nc->sockfd >= 0) {
        close(nc->sockfd);
        nc->sockfd = -1;
    }
    nc->state = NC_DISCONNECTED;
    free(nc);
}

NetState net_state(const NetContext *nc)
{
    return nc ? nc->state : NC_DISCONNECTED;
}

const char *net_error(const NetContext *nc)
{
    return nc ? nc->error : "no context";
}

int net_send_join(NetContext *nc, const char *player_name)
{
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"join\",\"data\":{\"player_name\":\"%s\"}}",
        player_name);
    return send_line(nc, buf);
}

int net_create_room(NetContext *nc, int private)
{
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"create_room\",\"data\":{\"private\":%s}}",
        private ? "true" : "false");
    return send_line(nc, buf);
}

int net_join_room(NetContext *nc, const char *room_id)
{
    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"join_room\",\"data\":{\"room_id\":\"%s\"}}",
        room_id);
    return send_line(nc, buf);
}

int net_send_ready(NetContext *nc, int ready)
{
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"ready\",\"data\":{\"ready\":%s}}",
        ready ? "true" : "false");
    return send_line(nc, buf);
}

int net_send_input(NetContext *nc, int frame, uint8_t input_bitmask)
{
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"input\",\"data\":{\"frame\":%d,\"input\":%u}}",
        frame, (unsigned)input_bitmask);
    return send_line(nc, buf);
}

int net_send_state(NetContext *nc, int frame, uint32_t hash)
{
    char buf[128];
    snprintf(buf, sizeof(buf),
        "{\"type\":\"state\",\"data\":{\"frame\":%d,\"hash\":%u}}",
        frame, (unsigned)hash);
    return send_line(nc, buf);
}

/* --- Minimal JSON value extractors --- */

/* Locate a key and return a pointer just past the ':' (after whitespace). */
static const char *find_key(const char *json, const char *key)
{
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return NULL;
    p += strlen(pattern);
    p = strchr(p, ':');
    if (!p) return NULL;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

static int json_find_string(const char *json, const char *key,
                           char *out, size_t out_sz)
{
    const char *p = find_key(json, key);
    if (!p || *p != '"') return -1;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < out_sz - 1u) {
        out[i++] = *p++;
    }
    out[i] = '\0';
    return 0;
}

static int json_find_int(const char *json, const char *key, int *out)
{
    const char *p = find_key(json, key);
    if (!p) return -1;
    *out = atoi(p);
    return 0;
}

static int json_find_bool(const char *json, const char *key, int *out)
{
    const char *p = find_key(json, key);
    if (!p) return -1;
    *out = (strncmp(p, "true", 4) == 0) ? 1 : 0;
    return 0;
}

/* Return a pointer to the body of the "data" object (at the opening '{'). */
static const char *json_data_start(const char *json)
{
    const char *p = strstr(json, "\"data\"");
    if (!p) return NULL;
    p += 6; /* strlen("\"data\"") */
    p = strchr(p, ':');
    if (!p) return NULL;
    p++;
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

int net_poll(NetContext *nc, NetMessage *out)
{
    if (!nc || nc->sockfd < 0) return 0;

    memset(out, 0, sizeof(*out));

    /* Try to read more data (non-blocking). */
    ssize_t n = read(nc->sockfd, nc->recv_buf + nc->recv_len,
                     sizeof(nc->recv_buf) - (size_t)nc->recv_len - 1u);
    if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            set_error(nc, "connection lost");
            return 0;
        }
        /* No new data — still check buffer for a complete line below. */
    } else if (n == 0) {
        set_error(nc, "server closed connection");
        return 0;
    } else {
        nc->recv_len += (int)n;
        nc->recv_buf[nc->recv_len] = '\0';
    }

    char *newline = strchr(nc->recv_buf, '\n');
    if (!newline) return 0;

    *newline = '\0';
    const char *line = nc->recv_buf;

    char type_str[32];
    if (json_find_string(line, "type", type_str, sizeof(type_str)) != 0) {
        int consumed = (int)(newline - nc->recv_buf) + 1;
        memmove(nc->recv_buf, newline + 1, (size_t)(nc->recv_len - consumed));
        nc->recv_len -= consumed;
        return 0;
    }

    const char *data = json_data_start(line);
    if (!data) data = "";

    if (strcmp(type_str, "start") == 0) {
        out->type = NC_MSG_START;
        int seed_val = 0;
        json_find_int(data, "seed", &seed_val);
        out->seed = (uint32_t)seed_val;
        json_find_int(data, "mode", &out->mode);
        json_find_string(data, "opponent", out->opponent,
                         sizeof(out->opponent));
    } else if (strcmp(type_str, "frame") == 0) {
        out->type = NC_MSG_FRAME;
        json_find_int(data, "frame", &out->frame);
        int input_val = 0;
        json_find_int(data, "input", &input_val);
        out->input = (uint8_t)input_val;
    } else if (strcmp(type_str, "state_ack") == 0) {
        out->type = NC_MSG_STATE_ACK;
        json_find_int(data, "frame", &out->frame);
        json_find_bool(data, "valid", &out->valid);
    } else if (strcmp(type_str, "end") == 0) {
        out->type = NC_MSG_END;
        json_find_string(data, "winner", out->winner,
                         sizeof(out->winner));
        json_find_string(data, "reason", out->reason,
                         sizeof(out->reason));
    } else if (strcmp(type_str, "error") == 0) {
        out->type = NC_MSG_ERROR;
        json_find_string(data, "message", out->error_msg,
                         sizeof(out->error_msg));
    } else {
        out->type = NC_MSG_NONE;
    }

    int consumed = (int)(newline - nc->recv_buf) + 1;
    memmove(nc->recv_buf, newline + 1, (size_t)(nc->recv_len - consumed));
    nc->recv_len -= consumed;

    return 1;
}
