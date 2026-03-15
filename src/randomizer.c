#include "randomizer.h"

/* xorshift32 PRNG */
static uint32_t xorshift32(uint32_t *state)
{
    uint32_t x = *state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *state = x;
    return x;
}

/* Fill next 7-bag using Fisher-Yates shuffle */
static void refill_bag(PieceQueue *q)
{
    PieceKind bag[7];
    for (int i = 0; i < 7; i++)
        bag[i] = (PieceKind)i;

    for (int i = 6; i > 0; i--) {
        int j = (int)(xorshift32(&q->rng_state) % (uint32_t)(i + 1));
        PieceKind tmp = bag[i];
        bag[i] = bag[j];
        bag[j] = tmp;
    }

    for (int i = 0; i < 7; i++) {
        q->data[q->tail] = bag[i];
        q->tail = (q->tail + 1) % QUEUE_CAP;
        q->count++;
    }
}

void queue_init(PieceQueue *q, uint32_t seed)
{
    q->head      = 0;
    q->tail      = 0;
    q->count     = 0;
    q->rng_state = (seed == 0) ? 1u : seed;
    /* Pre-fill two bags so the preview queue is always ready */
    refill_bag(q);
    refill_bag(q);
}

PieceKind queue_pop(PieceQueue *q)
{
    if (q->count < 7)
        refill_bag(q);

    PieceKind k = q->data[q->head];
    q->head  = (q->head + 1) % QUEUE_CAP;
    q->count--;
    return k;
}

PieceKind queue_peek(const PieceQueue *q, int index)
{
    return q->data[(q->head + index) % QUEUE_CAP];
}
