#include <stdio.h>
#include "config.h"
#include "competitive.h"

static int g_pass = 0, g_fail = 0;
#define EXPECT(cond, msg) do { if (cond) { g_pass++; } else { g_fail++; printf("  FAIL [%s:%d] %s\n", __FILE__, __LINE__, msg); } } while (0)

int main(void) {
	printf("\n== Competitive Scoring ==\n");
	EXPECT(competitive_garbage_for_clear(0, 1, 0, 0) == 4, "T-spin solo = 4 garbage");
	EXPECT(competitive_garbage_for_clear(1, 1, 0, 0) == 4, "T-spin single = 4 garbage");
	EXPECT(competitive_garbage_for_clear(2, 1, 0, 0) == 6, "T-spin double = 6 garbage");
	EXPECT(competitive_garbage_for_clear(3, 1, 0, 0) == 8, "T-spin triple = 8 garbage");
	EXPECT(competitive_garbage_for_clear(4, 0, 0, 0) == 4, "Quad = 4 garbage");
	EXPECT(competitive_garbage_for_clear(2, 0, 0, 0) == 1, "Double = 1 garbage");
	EXPECT(competitive_garbage_for_clear(1, 0, 0, 0) == 0, "Single = 0 garbage");
	EXPECT(competitive_garbage_for_clear(4, 0, 1, 0) == 8, "B2B Quad = 8 garbage");
	EXPECT(competitive_garbage_for_clear(2, 1, 1, 0) == 12, "B2B T-spin double = 12");
	EXPECT(competitive_garbage_for_clear(4, 0, 0, 1) == 14, "Quad + all clear = 14");
	EXPECT(competitive_tspin_solo_score(0) == 400, "T-spin solo L0 = 400");
	EXPECT(competitive_tspin_solo_score(5) == 2400, "T-spin solo L5 = 2400");
	EXPECT(competitive_b2b_bonus(800, 1) == 1200, "B2B bonus 800 -> 1200");
	EXPECT(competitive_b2b_bonus(800, 0) == 800, "No B2B = same");
	GarbageQueue gq;
	garbage_queue_init(&gq);
	EXPECT(garbage_queue_pending(&gq) == 0, "empty queue = 0");
	garbage_queue_add(&gq, 4, 0xABCDEF);
	EXPECT(garbage_queue_pending(&gq) == 4, "after add = 4");
	garbage_queue_add(&gq, 2, 0x123456);
	EXPECT(garbage_queue_pending(&gq) == 6, "after second add = 6");
	printf("\n%d passed, %d failed\n", g_pass, g_fail);
	return g_fail > 0 ? 1 : 0;
}
