#include "competitive.h"

void garbage_queue_init(GarbageQueue *gq) { gq->lines = 0; gq->hole_col = 0; }
void garbage_queue_add(GarbageQueue *gq, int lines, uint32_t rng_seed) {
	uint32_t s = rng_seed ? rng_seed : 1;
	s ^= s << 13; s ^= s >> 17; s ^= s << 5;
	gq->lines += lines;
	gq->hole_col = (int)(s % 10);
}
int garbage_queue_pending(const GarbageQueue *gq) { return gq->lines; }

int competitive_garbage_for_clear(int lines, int tspin, int b2b, int all_clear) {
	int garbage = 0;
	if (tspin) {
		switch (lines) {
			case 0: garbage = 4; break;
			case 1: garbage = 4; break;
			case 2: garbage = 6; break;
			case 3: garbage = 8; break;
			default: break;
		}
	} else {
		switch (lines) {
			case 1: garbage = 0; break;
			case 2: garbage = 1; break;
			case 3: garbage = 2; break;
			case 4: garbage = 4; break;
			default: break;
		}
	}
	if (b2b) garbage *= 2;
	if (all_clear) garbage += 10;
	return garbage;
}

int competitive_tspin_solo_score(int level) { return 400 * (level + 1); }
int competitive_b2b_bonus(int base_score, int b2b) {
	if (b2b) return base_score * 3 / 2;
	return base_score;
}
