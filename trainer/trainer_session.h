/* Session state machine (spec sec. 3.4, 3.5): a fixed or infinite run of questions for one
 * exercise, with the question flow (ask -> optional replays -> answer -> feedback -> next) and
 * the end-of-session summary. Generating questions (trainer_interval.h etc.) and playing them
 * (audio layer) stay outside this module: it only tracks where the session is and the running
 * totals needed for the summary and for trainer_stats.h. */
#ifndef TRAINER_SESSION_H
#define TRAINER_SESSION_H

#include "trainer_types.h"

typedef enum {
    TRAINER_SESSION_ASKING = 0,   /* current question set; can be replayed; awaiting an answer */
    TRAINER_SESSION_FEEDBACK,     /* last answer graded; correct answer can be shown/replayed */
    TRAINER_SESSION_DONE          /* total_questions reached (never, if it was 0: infinite) */
} trainer_session_state_t;

#define TRAINER_SESSION_MAX_TRACKED_ITEMS 80

typedef struct {
    trainer_exercise_t exercise;
    uint32_t total_questions;       /* 0 = infinite */
    uint32_t asked, correct;
    uint32_t replays_total;
    uint32_t current_replays;
    uint32_t current_start_ms;
    uint64_t response_time_sum_ms;
    trainer_session_state_t state;
    trainer_question_t current;
    trainer_eval_t last_result;
    struct { uint32_t item_id, wrong_count; } errors[TRAINER_SESSION_MAX_TRACKED_ITEMS];
    uint32_t error_item_count;
} trainer_session_t;

typedef struct {
    uint32_t asked, correct;
    uint32_t pct_correct_permille;  /* correct * 1000 / asked (0 if asked == 0) */
    uint32_t avg_response_ms;       /* 0 if asked == 0 */
    bool has_worst_item;
    uint32_t worst_item_id, worst_item_wrong_count;
} trainer_session_summary_t;

void trainer_session_start(trainer_session_t *s, trainer_exercise_t ex, uint32_t total_questions);

/* Called once the interface layer has a question ready and is about to start playing it. */
void trainer_session_set_question(trainer_session_t *s, const trainer_question_t *q, uint32_t now_ms);

/* Call when the user presses "listen again"; only meaningful in TRAINER_SESSION_ASKING. */
void trainer_session_replay(trainer_session_t *s);

/* Grades the answer, moves to TRAINER_SESSION_FEEDBACK, updates the running totals. Only valid in
 * TRAINER_SESSION_ASKING. */
trainer_eval_t trainer_session_answer_played(trainer_session_t *s, const int8_t *notes, uint8_t n,
                                              bool accept_any_octave, uint32_t now_ms);
trainer_eval_t trainer_session_answer_chosen(trainer_session_t *s, uint32_t chosen_item_id,
                                              uint32_t now_ms);

/* Checks whether the session is over. If total_questions is reached, moves to
 * TRAINER_SESSION_DONE and returns false. Otherwise returns true and leaves the state as
 * TRAINER_SESSION_FEEDBACK: the interface layer must generate the next question and call
 * trainer_session_set_question(), which moves to TRAINER_SESSION_ASKING. */
bool trainer_session_advance(trainer_session_t *s);

void trainer_session_summary(const trainer_session_t *s, trainer_session_summary_t *out);

#endif
