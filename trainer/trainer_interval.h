/* Interval exercise (spec sec. 3.1.1): ascending, descending, harmonic or mixed intervals from a
 * random starting note, drawn from a configurable set (minor 2nd .. octave, optionally .. 15th). */
#ifndef TRAINER_INTERVAL_H
#define TRAINER_INTERVAL_H

#include "trainer_types.h"
#include "trainer_rng.h"

#define TRAINER_INTERVAL_MIN_SEMITONES 1u
#define TRAINER_INTERVAL_MAX_SEMITONES 24u   /* 1..12 = minor 2nd..octave, 13..24 = the same + an octave */

/* bit (semitones - 1) set => that interval (1..24 semitones) is enabled */
#define TRAINER_SEMITONE_BIT(semitones) (1u << ((uint32_t)(semitones) - 1u))

typedef enum {
    TRAINER_DIR_ASC = 0,
    TRAINER_DIR_DESC,
    TRAINER_DIR_HARMONIC,
    TRAINER_DIR_MIXED,      /* generator resolves this to ASC or DESC per question, 50/50 */
    TRAINER_DIR_COUNT
} trainer_interval_dir_t;
#define TRAINER_DIR_BIT(d) (1u << (uint32_t)(d))

typedef struct {
    uint32_t semitone_mask;      /* TRAINER_SEMITONE_BIT(...) OR'd together; at least one required */
    uint32_t dir_mask;           /* TRAINER_DIR_BIT(...) OR'd together; at least one required */
    int8_t root_min, root_max;   /* inclusive range for the first note; leave room for the largest
                                   * enabled interval (the generator does not clamp or reject this) */
} trainer_interval_config_t;

/* "Minor 2nd" .. "15th"; "?" if semitones is out of range. */
const char *trainer_interval_name(uint32_t semitones);

/* item_id layout for this exercise: 0x1000 | (semitones << 4) | resolved_dir (0..2) */
#define TRAINER_INTERVAL_ITEM_ID(semitones, dir) (0x1000u | ((uint32_t)(semitones) << 4) | (uint32_t)(dir))

/* Fills *out with a new question drawn from cfg using rng. Returns false (leaving *out untouched)
 * if cfg enables no interval or no direction. */
bool trainer_interval_generate(const trainer_interval_config_t *cfg, trainer_rng_t *rng,
                                trainer_question_t *out);

/* The interval's second note transposed to start on C (pitch class 0..11), ignoring the
 * question's actual (randomly chosen) root: what a "suonando" answer must match, so answering
 * tests recognizing the interval itself rather than recalling the root that happened to play. */
uint8_t trainer_interval_pc_from_c(uint32_t semitones, trainer_interval_dir_t dir);

#endif
