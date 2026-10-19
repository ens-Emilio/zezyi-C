#pragma once
#include "config.h"

int competitive_garbage_for_clear(int lines, int tspin, int b2b, int all_clear);

typedef struct {
	int lines;
	int hole_col;
} GarbageQueue;

void garbage_queue_init(GarbageQueue *gq);
void garbage_queue_add(GarbageQueue *gq, int lines, uint32_t rng_seed);
int  garbage_queue_pending(const GarbageQueue *gq);

int competitive_tspin_solo_score(int level);
int competitive_b2b_bonus(int base_score, int b2b);
