/* Chord quality exercise (spec sec. 3.1.2): triads, then (advanced level) sevenths, in root
 * position or an inversion, played as a block or arpeggiated. */
#ifndef TRAINER_CHORD_H
#define TRAINER_CHORD_H

#include "trainer_types.h"
#include "trainer_rng.h"

typedef enum {
    TRAINER_CHORD_MAJOR = 0,
    TRAINER_CHORD_MINOR,
    TRAINER_CHORD_DIM,
    TRAINER_CHORD_AUG,
    TRAINER_CHORD_SUS2,
    TRAINER_CHORD_SUS4,
    TRAINER_CHORD_MAJ7,     /* advanced (spec: "livello avanzato") */
    TRAINER_CHORD_DOM7,
    TRAINER_CHORD_MIN7,
    TRAINER_CHORD_M7B5,
    TRAINER_CHORD_DIM7,
    TRAINER_CHORD_QUALITY_COUNT
} trainer_chord_quality_t;
#define TRAINER_CHORD_QUALITY_BIT(q) (1u << (uint32_t)(q))

typedef enum {
    TRAINER_VOICING_BLOCK = 0,
    TRAINER_VOICING_ARPEGGIO_UP,
    TRAINER_VOICING_ARPEGGIO_DOWN,
    TRAINER_VOICING_COUNT
} trainer_chord_voicing_t;
#define TRAINER_VOICING_BIT(v) (1u << (uint32_t)(v))

typedef struct {
    uint32_t quality_mask;    /* TRAINER_CHORD_QUALITY_BIT(...) OR'd together; at least one required */
    uint32_t voicing_mask;    /* TRAINER_VOICING_BIT(...); 0 defaults to block only */
    uint32_t inversion_mask;  /* bit k = inversion k allowed; bits beyond a quality's note count
                                * are ignored for that quality; 0 defaults to root position only */
    int8_t root_min, root_max;
} trainer_chord_config_t;

const char *trainer_chord_quality_name(trainer_chord_quality_t q);
uint8_t trainer_chord_note_count(trainer_chord_quality_t q);   /* 3 (triad) or 4 (seventh) */

/* The chord's pitch classes (0..11, root included), for evaluating a played answer regardless of
 * octave or voicing (trainer_question.h). out_pc must hold at least 4 entries. */
void trainer_chord_pitch_classes(trainer_chord_quality_t q, int8_t root_note, uint8_t *out_pc,
                                  uint8_t *out_count);

/* item_id layout: 0x2000 | (quality << 4) | voicing (inversion and root are not part of the key:
 * stats group by quality + voicing, spec 3.5's "6a minore discendente"-style per-item tracking) */
#define TRAINER_CHORD_ITEM_ID(quality, voicing) (0x2000u | ((uint32_t)(quality) << 4) | (uint32_t)(voicing))

bool trainer_chord_generate(const trainer_chord_config_t *cfg, trainer_rng_t *rng,
                             trainer_question_t *out);

#endif
