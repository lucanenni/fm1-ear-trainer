/* FM-1 Ear Trainer core: shared types.
 * This module and everything under trainer/ is plain C99, with no dependency on Felucca, the
 * FM-1 hardware, or any OS: it must build and run standalone on the host (see tests/). */
#ifndef TRAINER_TYPES_H
#define TRAINER_TYPES_H

#include <stdint.h>
#include <stdbool.h>

/* The three MVP exercise types (spec sec. 3.1). */
typedef enum {
    TRAINER_EX_INTERVAL = 0,
    TRAINER_EX_CHORD,
    TRAINER_EX_DEGREE,
    TRAINER_EX_COUNT
} trainer_exercise_t;

/* A question can need a cadence (up to 4 triads = 12 notes) plus a target note: 16 gives headroom
 * for that and for the melodic-dictation exercise planned after the MVP (spec sec. 3.2). */
#define TRAINER_MAX_EVENTS 16

/* One note of a question, in playing order. `note` is an absolute pitch (e.g. a MIDI note
 * number); the interface/audio layer picks the real tempo and note length. Notes that share
 * `start_step` sound together (a block chord, a harmonic interval); higher `start_step` values
 * come later. `dur_steps` is how many steps the note should be held for (mostly 1; a drone or the
 * final target note may hold longer) — again in the same abstract step unit, not milliseconds. */
typedef struct {
    int8_t note;
    uint8_t start_step;
    uint8_t dur_steps;
} trainer_event_t;

/* A generated question, ready to be played by the interface/audio layer and later checked by
 * trainer_evaluate_played()/trainer_evaluate_chosen() (trainer_question.h). `item_id` is a stable
 * key identifying the thing being tested (e.g. "descending minor 6th"), independent of the
 * specific root note or octave picked for this instance: it is what trainer_stats.h and the
 * session summary (trainer_session.h) key statistics and adaptive selection on. Each exercise
 * generator reserves its own range of item_id values (trainer_interval.h, trainer_chord.h,
 * trainer_degree.h) so one combined stats table can hold all exercise types at once. */
typedef struct {
    trainer_exercise_t type;
    trainer_event_t events[TRAINER_MAX_EVENTS];
    uint8_t event_count;
    uint32_t item_id;
    union {
        struct {
            uint8_t semitones;   /* 1..24: see trainer_interval.h */
            uint8_t dir;         /* trainer_interval_dir_t, resolved (never TRAINER_DIR_MIXED) */
            int8_t ref_note;     /* the first note heard */
            int8_t answer_note;  /* the second note heard: what a "suonando" answer must match */
        } interval;
        struct {
            uint8_t quality;     /* trainer_chord_quality_t */
            uint8_t inversion;
            uint8_t voicing;     /* trainer_chord_voicing_t */
            int8_t root_note;
        } chord;
        struct {
            uint8_t scale_type;  /* trainer_scale_type_t */
            uint8_t degree;      /* 1..7 */
            uint8_t cadence;     /* trainer_cadence_t */
            int8_t root_note;    /* the tonic */
            int8_t target_note;  /* the note the user must name the degree of */
        } degree;
    } answer;
} trainer_question_t;

/* How the user answered a question (spec sec. 3.3). */
typedef enum { TRAINER_ANSWER_PLAYED = 0, TRAINER_ANSWER_CHOSEN } trainer_answer_mode_t;
typedef enum { TRAINER_WRONG = 0, TRAINER_CORRECT = 1 } trainer_eval_t;

#endif
