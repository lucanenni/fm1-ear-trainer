#include "trainer_interval.h"

static const char *const NAMES[TRAINER_INTERVAL_MAX_SEMITONES] = {
    "Minor 2nd", "Major 2nd", "Minor 3rd", "Major 3rd", "Perfect 4th", "Tritone", "Perfect 5th",
    "Minor 6th", "Major 6th", "Minor 7th", "Major 7th", "Octave",
    "Minor 9th", "Major 9th", "Minor 10th", "Major 10th", "Perfect 11th", "Tritone + 8",
    "Perfect 12th", "Minor 13th", "Major 13th", "Minor 14th", "Major 14th", "15th",
};

const char *trainer_interval_name(uint32_t semitones)
{
    if (semitones < TRAINER_INTERVAL_MIN_SEMITONES || semitones > TRAINER_INTERVAL_MAX_SEMITONES)
        return "?";
    return NAMES[semitones - 1u];
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

bool trainer_interval_generate(const trainer_interval_config_t *cfg, trainer_rng_t *rng,
                                trainer_question_t *out)
{
    uint32_t semitones_idx, dir, root_span;
    int8_t root, second;

    if (!pick_from_mask(rng, cfg->semitone_mask, TRAINER_INTERVAL_MAX_SEMITONES, &semitones_idx))
        return false;
    if (!pick_from_mask(rng, cfg->dir_mask, TRAINER_DIR_COUNT, &dir))
        return false;
    if (dir == TRAINER_DIR_MIXED)
        dir = trainer_rng_range(rng, 2) ? TRAINER_DIR_DESC : TRAINER_DIR_ASC;

    root_span = (uint32_t)(cfg->root_max - cfg->root_min) + 1u;
    root = (int8_t)(cfg->root_min + (int)trainer_rng_range(rng, root_span));
    second = (dir == TRAINER_DIR_DESC) ? (int8_t)(root - (int)(semitones_idx + 1u))
                                        : (int8_t)(root + (int)(semitones_idx + 1u));

    out->type = TRAINER_EX_INTERVAL;
    out->events[0].note = root;
    out->events[0].start_step = 0;
    out->events[0].dur_steps = 1;
    out->events[1].note = second;
    out->events[1].start_step = (dir == TRAINER_DIR_HARMONIC) ? 0 : 1;
    out->events[1].dur_steps = 1;
    out->event_count = 2;
    out->answer.interval.semitones = (uint8_t)(semitones_idx + 1u);
    out->answer.interval.dir = (uint8_t)dir;
    out->answer.interval.ref_note = root;
    out->answer.interval.answer_note = second;
    out->item_id = TRAINER_INTERVAL_ITEM_ID(semitones_idx + 1u, dir);
    return true;
}

uint8_t trainer_interval_pc_from_c(uint32_t semitones, trainer_interval_dir_t dir)
{
    uint32_t s = semitones % 12u;
    return (uint8_t)(dir == TRAINER_DIR_DESC ? (12u - s) % 12u : s);
}
