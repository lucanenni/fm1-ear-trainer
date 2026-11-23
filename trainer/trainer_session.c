#include "trainer_session.h"
#include "trainer_question.h"
#include <string.h>

void trainer_session_start(trainer_session_t *s, trainer_exercise_t ex, uint32_t total_questions)
{
    memset(s, 0, sizeof *s);
    s->exercise = ex;
    s->total_questions = total_questions;
    s->state = TRAINER_SESSION_DONE;   /* until trainer_session_set_question() is called */
}

void trainer_session_set_question(trainer_session_t *s, const trainer_question_t *q, uint32_t now_ms)
{
    s->current = *q;
    s->current_replays = 0;
    s->current_start_ms = now_ms;
    s->state = TRAINER_SESSION_ASKING;
}

void trainer_session_replay(trainer_session_t *s)
{
    if (s->state == TRAINER_SESSION_ASKING) {
        s->current_replays++;
        s->replays_total++;
    }
}

static void record_wrong(trainer_session_t *s, uint32_t item_id)
{
    uint32_t i;
    for (i = 0; i < s->error_item_count; i++)
        if (s->errors[i].item_id == item_id) {
            s->errors[i].wrong_count++;
            return;
        }
    if (s->error_item_count < TRAINER_SESSION_MAX_TRACKED_ITEMS) {
        s->errors[s->error_item_count].item_id = item_id;
        s->errors[s->error_item_count].wrong_count = 1;
        s->error_item_count++;
    }
}

static trainer_eval_t finish_answer(trainer_session_t *s, trainer_eval_t result, uint32_t now_ms)
{
    s->asked++;
    if (result == TRAINER_CORRECT)
        s->correct++;
    else
        record_wrong(s, s->current.item_id);
    s->response_time_sum_ms += (now_ms >= s->current_start_ms) ? (now_ms - s->current_start_ms) : 0u;
    s->last_result = result;
    s->state = TRAINER_SESSION_FEEDBACK;
    return result;
}

trainer_eval_t trainer_session_answer_played(trainer_session_t *s, const int8_t *notes, uint8_t n,
                                              bool accept_any_octave, uint32_t now_ms)
{
    if (s->state != TRAINER_SESSION_ASKING)
        return TRAINER_WRONG;
    return finish_answer(s, trainer_evaluate_played(&s->current, notes, n, accept_any_octave), now_ms);
}

trainer_eval_t trainer_session_answer_chosen(trainer_session_t *s, uint32_t chosen_item_id,
                                              uint32_t now_ms)
{
    if (s->state != TRAINER_SESSION_ASKING)
        return TRAINER_WRONG;
    return finish_answer(s, trainer_evaluate_chosen(&s->current, chosen_item_id), now_ms);
}

bool trainer_session_advance(trainer_session_t *s)
{
    if (s->state != TRAINER_SESSION_FEEDBACK)
        return s->state != TRAINER_SESSION_DONE;
    if (s->total_questions && s->asked >= s->total_questions) {
        s->state = TRAINER_SESSION_DONE;
        return false;
    }
    return true;   /* still FEEDBACK until the interface layer calls trainer_session_set_question() */
}

void trainer_session_summary(const trainer_session_t *s, trainer_session_summary_t *out)
{
    uint32_t i;
    memset(out, 0, sizeof *out);
    out->asked = s->asked;
    out->correct = s->correct;
    if (s->asked) {
        out->pct_correct_permille = s->correct * 1000u / s->asked;
        out->avg_response_ms = (uint32_t)(s->response_time_sum_ms / s->asked);
    }
    for (i = 0; i < s->error_item_count; i++)
        if (!out->has_worst_item || s->errors[i].wrong_count > out->worst_item_wrong_count) {
            out->has_worst_item = true;
            out->worst_item_id = s->errors[i].item_id;
            out->worst_item_wrong_count = s->errors[i].wrong_count;
        }
}
