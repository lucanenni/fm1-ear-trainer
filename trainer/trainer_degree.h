/* Scale degree in context exercise (spec sec. 3.1.3): the synth establishes a key (a I-IV-V-I
 * cadence, or a tonic drone), then plays one note; the user names its scale degree (1..7). */
#ifndef TRAINER_DEGREE_H
#define TRAINER_DEGREE_H

#include "trainer_types.h"
#include "trainer_rng.h"

typedef enum {
    TRAINER_SCALE_MAJOR = 0,
    TRAINER_SCALE_NATURAL_MINOR,   /* "poi minore e cromatismi": chromatic degrees are a later addition */
    TRAINER_SCALE_TYPE_COUNT
} trainer_scale_type_t;
#define TRAINER_SCALE_BIT(s) (1u << (uint32_t)(s))

typedef enum {
    TRAINER_CADENCE_I_IV_V_I = 0,
    TRAINER_CADENCE_DRONE,
    TRAINER_CADENCE_COUNT
} trainer_cadence_t;
#define TRAINER_CADENCE_BIT(c) (1u << (uint32_t)(c))

typedef struct {
    uint32_t scale_mask;     /* TRAINER_SCALE_BIT(...); at least one required */
    uint32_t cadence_mask;   /* TRAINER_CADENCE_BIT(...); at least one required */
    uint32_t degree_mask;    /* bit (degree - 1), degree 1..7; at least one required */
    int8_t root_min, root_max;
} trainer_degree_config_t;

/* item_id layout: 0x3000 | (scale_type << 4) | (degree - 1) */
#define TRAINER_DEGREE_ITEM_ID(scale_type, degree) (0x3000u | ((uint32_t)(scale_type) << 4) | ((uint32_t)(degree) - 1u))

bool trainer_degree_generate(const trainer_degree_config_t *cfg, trainer_rng_t *rng,
                              trainer_question_t *out);

/* The scale degree's pitch class with the tonic on C (0..11), ignoring the question's actual
 * (randomly chosen) tonic: what a "suonando" answer must match, so answering tests recognizing
 * the degree itself rather than recalling the tonic that happened to play. */
uint8_t trainer_degree_pc_from_c(trainer_scale_type_t scale, uint8_t degree);

#endif
