#include "trainer_rng.h"

void trainer_rng_seed(trainer_rng_t *rng, uint32_t seed)
{
    rng->state = seed ? seed : 1u;   /* xorshift32 is fixed at 0 forever if seeded with 0 */
}

uint32_t trainer_rng_next(trainer_rng_t *rng)
{
    uint32_t x = rng->state;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rng->state = x;
    return x;
}

uint32_t trainer_rng_range(trainer_rng_t *rng, uint32_t n)
{
    return n ? trainer_rng_next(rng) % n : 0u;
}
