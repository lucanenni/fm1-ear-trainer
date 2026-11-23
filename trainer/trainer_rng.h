/* Seedable, deterministic PRNG for the trainer core. Not cryptographic: the only requirement is
 * that a given seed always reproduces the same sequence of questions, for reproducible tests and
 * for a "practice this exact run again" feature if ever wanted (spec sec. 4). */
#ifndef TRAINER_RNG_H
#define TRAINER_RNG_H

#include <stdint.h>

typedef struct { uint32_t state; } trainer_rng_t;

void trainer_rng_seed(trainer_rng_t *rng, uint32_t seed);

/* Raw 32-bit xorshift output. */
uint32_t trainer_rng_next(trainer_rng_t *rng);

/* Uniform in [0, n). n must be > 0. Plain modulo: the small, fixed-size sets this core draws
 * from (intervals, chord qualities, scale degrees) make the bias negligible. */
uint32_t trainer_rng_range(trainer_rng_t *rng, uint32_t n);

#endif
