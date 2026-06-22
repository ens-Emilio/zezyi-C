#include <stdio.h>
#include <string.h>
#include "replay.h"

void replay_init(ReplayRecorder *rec)
{
    memset(rec, 0, sizeof(*rec));
}

static ReplayFrame pack_input(const InputSnapshot *in)
{
    unsigned int f = 0;
    if (in->left)       f |= (1u << 0);
    if (in->right)      f |= (1u << 1);
    if (in->down)       f |= (1u << 2);
    if (in->rotate_cw)  f |= (1u << 3);
    if (in->rotate_ccw) f |= (1u << 4);
    if (in->hard_drop)  f |= (1u << 5);
    if (in->hold)       f |= (1u << 6);
    return (ReplayFrame)f;
}

static InputSnapshot unpack_input(ReplayFrame f)
{
    InputSnapshot in;
    memset(&in, 0, sizeof(in));
    in.left       = (f & (1u << 0)) ? 1 : 0;
    in.right      = (f & (1u << 1)) ? 1 : 0;
    in.down       = (f & (1u << 2)) ? 1 : 0;
    in.rotate_cw  = (f & (1u << 3)) ? 1 : 0;
    in.rotate_ccw = (f & (1u << 4)) ? 1 : 0;
    in.hard_drop  = (f & (1u << 5)) ? 1 : 0;
    in.hold       = (f & (1u << 6)) ? 1 : 0;
    return in;
}

void replay_record(ReplayRecorder *rec, const InputSnapshot *in, int frame)
{
    if (frame < 0 || frame >= REPLAY_MAX_FRAMES) return;
    rec->frames[frame] = pack_input(in);
    if (frame + 1 > rec->frame_count)
        rec->frame_count = frame + 1;
}

int replay_save(const ReplayRecorder *rec, const char *path)
{
    FILE *f = fopen(path, "wb");
    if (!f) return -1;

    const char magic[4] = { 'Z', 'Y', 'R', '1' };
    fwrite(magic, 1, 4, f);
    fwrite(&rec->seed, sizeof(rec->seed), 1, f);
    fwrite(&rec->mode, sizeof(rec->mode), 1, f);
    fwrite(&rec->frame_count, sizeof(rec->frame_count), 1, f);
    fwrite(rec->frames, sizeof(ReplayFrame), (size_t)rec->frame_count, f);

    fclose(f);
    return 0;
}

int replay_load(ReplayPlayer *player, const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f) return -1;

    char magic[4];
    if (fread(magic, 1, 4, f) != 4 || memcmp(magic, "ZYR1", 4) != 0) {
        fclose(f);
        return -1;
    }

    if (fread(&player->seed, sizeof(player->seed), 1, f) != 1) {
        fclose(f);
        return -1;
    }
    if (fread(&player->mode, sizeof(player->mode), 1, f) != 1) {
        fclose(f);
        return -1;
    }
    if (fread(&player->frame_count, sizeof(player->frame_count), 1, f) != 1) {
        fclose(f);
        return -1;
    }

    if (player->frame_count < 0 || player->frame_count > REPLAY_MAX_FRAMES) {
        fclose(f);
        return -1;
    }

    if (fread(player->frames, sizeof(ReplayFrame),
              (size_t)player->frame_count, f) != (size_t)player->frame_count) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return 0;
}

void replay_get_input(const ReplayPlayer *player, int frame, InputSnapshot *out)
{
    if (frame < 0 || frame >= player->frame_count) {
        memset(out, 0, sizeof(*out));
        return;
    }
    *out = unpack_input(player->frames[frame]);
}
