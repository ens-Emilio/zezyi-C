#include <stdio.h>
#include <string.h>
#include "config.h"
#include "rules.h"
#include "board.h"
#include "piece.h"
#include "randomizer.h"
#include "replay.h"

static int g_pass = 0, g_fail = 0;
#define EXPECT(cond, msg) do { \
    if (cond) { g_pass++; } \
    else { g_fail++; printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, msg); } \
} while (0)

int main(void)
{
    printf("\n== Replay ==\n");

    GameState original;
    game_init(&original, 0xDEADBEEF);

    ReplayRecorder rec;
    replay_init(&rec);
    rec.seed = 0xDEADBEEF;
    rec.mode = MODE_MARATHON;

    InputSnapshot in = {0};
    for (int i = 0; i < 300; i++) {
        in.left = in.right = in.down = 0;
        in.rotate_cw = in.rotate_ccw = in.hard_drop = in.hold = 0;
        if (i % 3 == 0)  in.right = 1;
        if (i % 5 == 0)  in.rotate_cw = 1;
        if (i % 7 == 0)  in.hard_drop = 1;

        replay_record(&rec, &in, i);
        game_tick(&original, &in, 1.0 / 60.0);
    }

    EXPECT(replay_save(&rec, "/tmp/test_replay.zyr") == 0, "replay_save succeeds");

    ReplayPlayer player;
    EXPECT(replay_load(&player, "/tmp/test_replay.zyr") == 0, "replay_load succeeds");
    EXPECT(player.seed == 0xDEADBEEF, "seed matches");
    EXPECT(player.frame_count == 300, "frame count matches");

    GameState replay;
    game_init(&replay, player.seed);

    for (int i = 0; i < player.frame_count; i++) {
        InputSnapshot rin = {0};
        replay_get_input(&player, i, &rin);
        game_tick(&replay, &rin, 1.0 / 60.0);
    }

    EXPECT(replay.score == original.score, "score matches");
    EXPECT(replay.level == original.level, "level matches");
    EXPECT(replay.total_lines == original.total_lines, "lines match");
    EXPECT(replay.tick == original.tick, "tick matches");
    EXPECT(memcmp(&replay.board, &original.board, sizeof(Board)) == 0,
           "board state identical");

    printf("\n%d passed, %d failed\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
