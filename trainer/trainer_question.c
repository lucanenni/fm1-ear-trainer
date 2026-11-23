#include "trainer_question.h"
#include "trainer_chord.h"

static uint8_t pitch_class(int8_t note)
{
    int v = note % 12;
    return (uint8_t)(v < 0 ? v + 12 : v);
}

static trainer_eval_t eval_interval(const trainer_question_t *q, const int8_t *notes, uint8_t n,
                                     bool accept_any_octave)
{
    if (n != 1)
        return TRAINER_WRONG;
    if (accept_any_octave)
        return pitch_class(notes[0]) == pitch_class(q->answer.interval.answer_note) ? TRAINER_CORRECT
                                                                                      : TRAINER_WRONG;
    return notes[0] == q->answer.interval.answer_note ? TRAINER_CORRECT : TRAINER_WRONG;
}

static bool pc_set_has(const uint8_t *set, uint8_t n, uint8_t pc)
{
    uint8_t i;
    for (i = 0; i < n; i++)
        if (set[i] == pc)
            return true;
    return false;
}

static trainer_eval_t eval_chord(const trainer_question_t *q, const int8_t *notes, uint8_t n)
{
    uint8_t expected[4], expected_n, played[4], played_n = 0, i;
    trainer_chord_pitch_classes((trainer_chord_quality_t)q->answer.chord.quality,
                                 q->answer.chord.root_note, expected, &expected_n);
    if (n == 0 || n > 4)
        return TRAINER_WRONG;
    for (i = 0; i < n; i++) {
        uint8_t pc = pitch_class(notes[i]);
        if (!pc_set_has(played, played_n, pc))
            played[played_n++] = pc;
    }
    if (played_n != expected_n)
        return TRAINER_WRONG;
    for (i = 0; i < played_n; i++)
        if (!pc_set_has(expected, expected_n, played[i]))
            return TRAINER_WRONG;
    return TRAINER_CORRECT;
}

static trainer_eval_t eval_degree(const trainer_question_t *q, const int8_t *notes, uint8_t n)
{
    if (n != 1)
        return TRAINER_WRONG;
    return pitch_class(notes[0]) == pitch_class(q->answer.degree.target_note) ? TRAINER_CORRECT
                                                                                : TRAINER_WRONG;
}

trainer_eval_t trainer_evaluate_played(const trainer_question_t *q, const int8_t *notes,
                                        uint8_t n, bool accept_any_octave)
{
    switch (q->type) {
    case TRAINER_EX_INTERVAL:
        return eval_interval(q, notes, n, accept_any_octave);
    case TRAINER_EX_CHORD:
        return eval_chord(q, notes, n);
    case TRAINER_EX_DEGREE:
        return eval_degree(q, notes, n);
    default:
        return TRAINER_WRONG;
    }
}

trainer_eval_t trainer_evaluate_chosen(const trainer_question_t *q, uint32_t chosen_item_id)
{
    return chosen_item_id == q->item_id ? TRAINER_CORRECT : TRAINER_WRONG;
}
