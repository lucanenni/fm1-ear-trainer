#include "trainer_degree.h"

/* semitone offsets of the 7 scale degrees from the tonic, within one octave */
static const int8_t MAJOR_STEPS[7] = {0, 2, 4, 5, 7, 9, 11};
static const int8_t MINOR_STEPS[7] = {0, 2, 3, 5, 7, 8, 10};

static const int8_t *steps_of(trainer_scale_type_t s)
{
    return (s == TRAINER_SCALE_NATURAL_MINOR) ? MINOR_STEPS : MAJOR_STEPS;
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

/* the triad built on scale degree d (0-indexed, 0..6), stacking two diatonic thirds; ext[] spans
 * two octaves of the scale (14 entries) so stacking past degree 7 wraps correctly */
static void triad_at(const int8_t *steps, int8_t root, uint32_t d, int8_t out[3])
{
    int8_t ext[14];
    uint32_t i;
    for (i = 0; i < 14u; i++)
        ext[i] = (int8_t)(steps[i % 7u] + 12 * (int8_t)(i / 7u));
    out[0] = (int8_t)(root + ext[d]);
    out[1] = (int8_t)(root + ext[d + 2u]);
    out[2] = (int8_t)(root + ext[d + 4u]);
}

bool trainer_degree_generate(const trainer_degree_config_t *cfg, trainer_rng_t *rng,
                              trainer_question_t *out)
{
    uint32_t scale, cadence, degree_idx, root_span, n, i;
    int8_t root, target;
    const int8_t *steps;

    if (!pick_from_mask(rng, cfg->scale_mask, TRAINER_SCALE_TYPE_COUNT, &scale))
        return false;
    if (!pick_from_mask(rng, cfg->cadence_mask, TRAINER_CADENCE_COUNT, &cadence))
        return false;
    if (!pick_from_mask(rng, cfg->degree_mask, 7u, &degree_idx))
        return false;

    root_span = (uint32_t)(cfg->root_max - cfg->root_min) + 1u;
    root = (int8_t)(cfg->root_min + (int)trainer_rng_range(rng, root_span));
    steps = steps_of((trainer_scale_type_t)scale);
    target = (int8_t)(root + steps[degree_idx]);

    n = 0;
    if (cadence == TRAINER_CADENCE_I_IV_V_I) {
        static const uint32_t DEGREES[4] = {0, 3, 4, 0};
        uint32_t c;
        for (c = 0; c < 4u; c++) {
            int8_t triad[3];
            triad_at(steps, root, DEGREES[c], triad);
            for (i = 0; i < 3u; i++) {
                out->events[n].note = triad[i];
                out->events[n].start_step = (uint8_t)c;
                out->events[n].dur_steps = 1;
                n++;
            }
        }
    } else {
        out->events[n].note = root;
        out->events[n].start_step = 0;
        out->events[n].dur_steps = 4;
        n++;
    }
    out->events[n].note = target;
    out->events[n].start_step = (uint8_t)(out->events[n - 1].start_step + out->events[n - 1].dur_steps);
    out->events[n].dur_steps = 2;
    n++;

    out->type = TRAINER_EX_DEGREE;
    out->event_count = (uint8_t)n;
    out->answer.degree.scale_type = (uint8_t)scale;
    out->answer.degree.degree = (uint8_t)(degree_idx + 1u);
    out->answer.degree.cadence = (uint8_t)cadence;
    out->answer.degree.root_note = root;
    out->answer.degree.target_note = target;
    out->item_id = TRAINER_DEGREE_ITEM_ID(scale, degree_idx + 1u);
    return true;
}

uint8_t trainer_degree_pc_from_c(trainer_scale_type_t scale, uint8_t degree)
{
    return (uint8_t)steps_of(scale)[degree - 1u];
}
