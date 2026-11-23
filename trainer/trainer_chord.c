#include "trainer_chord.h"

static const int8_t OFF_MAJOR[] = {0, 4, 7};
static const int8_t OFF_MINOR[] = {0, 3, 7};
static const int8_t OFF_DIM[] = {0, 3, 6};
static const int8_t OFF_AUG[] = {0, 4, 8};
static const int8_t OFF_SUS2[] = {0, 2, 7};
static const int8_t OFF_SUS4[] = {0, 5, 7};
static const int8_t OFF_MAJ7[] = {0, 4, 7, 11};
static const int8_t OFF_DOM7[] = {0, 4, 7, 10};
static const int8_t OFF_MIN7[] = {0, 3, 7, 10};
static const int8_t OFF_M7B5[] = {0, 3, 6, 10};
static const int8_t OFF_DIM7[] = {0, 3, 6, 9};

typedef struct { const int8_t *offsets; uint8_t note_count; const char *name; } chord_def_t;

static const chord_def_t DEFS[TRAINER_CHORD_QUALITY_COUNT] = {
    [TRAINER_CHORD_MAJOR] = {OFF_MAJOR, 3, "Major"},
    [TRAINER_CHORD_MINOR] = {OFF_MINOR, 3, "Minor"},
    [TRAINER_CHORD_DIM] = {OFF_DIM, 3, "Diminished"},
    [TRAINER_CHORD_AUG] = {OFF_AUG, 3, "Augmented"},
    [TRAINER_CHORD_SUS2] = {OFF_SUS2, 3, "Sus2"},
    [TRAINER_CHORD_SUS4] = {OFF_SUS4, 3, "Sus4"},
    [TRAINER_CHORD_MAJ7] = {OFF_MAJ7, 4, "Major 7th"},
    [TRAINER_CHORD_DOM7] = {OFF_DOM7, 4, "Dominant 7th"},
    [TRAINER_CHORD_MIN7] = {OFF_MIN7, 4, "Minor 7th"},
    [TRAINER_CHORD_M7B5] = {OFF_M7B5, 4, "Half-diminished 7th"},
    [TRAINER_CHORD_DIM7] = {OFF_DIM7, 4, "Diminished 7th"},
};

const char *trainer_chord_quality_name(trainer_chord_quality_t q)
{
    return (q < TRAINER_CHORD_QUALITY_COUNT) ? DEFS[q].name : "?";
}

uint8_t trainer_chord_note_count(trainer_chord_quality_t q)
{
    return (q < TRAINER_CHORD_QUALITY_COUNT) ? DEFS[q].note_count : 0u;
}

void trainer_chord_pitch_classes(trainer_chord_quality_t q, int8_t root_note, uint8_t *out_pc,
                                  uint8_t *out_count)
{
    uint8_t n = trainer_chord_note_count(q), i;
    *out_count = n;
    for (i = 0; i < n; i++) {
        int v = (root_note + DEFS[q].offsets[i]) % 12;
        out_pc[i] = (uint8_t)(v < 0 ? v + 12 : v);
    }
}

static bool pick_from_mask(trainer_rng_t *rng, uint32_t mask, uint32_t bit_count, uint32_t *out)
{
    uint32_t enabled[32], n = 0, i;
    for (i = 0; i < bit_count; i++)
        if (mask & (1u << i))
            enabled[n++] = i;
    if (!n)
        return false;
    *out = enabled[trainer_rng_range(rng, n)];
    return true;
}

static void sort_asc(int8_t *a, uint8_t n)
{
    uint8_t i, j;
    for (i = 1; i < n; i++) {
        int8_t v = a[i];
        j = i;
        while (j > 0 && a[j - 1] > v) {
            a[j] = a[j - 1];
            j--;
        }
        a[j] = v;
    }
}

bool trainer_chord_generate(const trainer_chord_config_t *cfg, trainer_rng_t *rng,
                             trainer_question_t *out)
{
    uint32_t quality, voicing, inversion, usable_inv, root_span, i;
    uint8_t notes;
    int8_t root, pitches[4];

    if (!pick_from_mask(rng, cfg->quality_mask, TRAINER_CHORD_QUALITY_COUNT, &quality))
        return false;
    notes = DEFS[quality].note_count;

    usable_inv = cfg->inversion_mask & ((notes < 32) ? ((1u << notes) - 1u) : 0xFFFFFFFFu);
    if (!usable_inv)
        usable_inv = 1u;
    if (!pick_from_mask(rng, usable_inv, notes, &inversion))
        inversion = 0;

    if (!pick_from_mask(rng, cfg->voicing_mask, TRAINER_VOICING_COUNT, &voicing))
        voicing = TRAINER_VOICING_BLOCK;

    root_span = (uint32_t)(cfg->root_max - cfg->root_min) + 1u;
    root = (int8_t)(cfg->root_min + (int)trainer_rng_range(rng, root_span));

    for (i = 0; i < notes; i++) {
        int8_t p = (int8_t)(root + DEFS[quality].offsets[i]);
        if (i < inversion)
            p = (int8_t)(p + 12);
        pitches[i] = p;
    }
    sort_asc(pitches, notes);

    out->type = TRAINER_EX_CHORD;
    out->event_count = notes;
    for (i = 0; i < notes; i++) {
        uint8_t step = (voicing == TRAINER_VOICING_BLOCK) ? 0
                       : (voicing == TRAINER_VOICING_ARPEGGIO_UP) ? (uint8_t)i
                                                                   : (uint8_t)(notes - 1u - i);
        out->events[i].note = pitches[i];
        out->events[i].start_step = step;
        out->events[i].dur_steps = 1;
    }
    out->answer.chord.quality = (uint8_t)quality;
    out->answer.chord.inversion = (uint8_t)inversion;
    out->answer.chord.voicing = (uint8_t)voicing;
    out->answer.chord.root_note = root;
    out->item_id = TRAINER_CHORD_ITEM_ID(quality, voicing);
    return true;
}
