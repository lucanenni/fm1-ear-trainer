/* Evaluating a user's answer against a generated question (spec sec. 3.3, 3.4). */
#ifndef TRAINER_QUESTION_H
#define TRAINER_QUESTION_H

#include "trainer_types.h"

/* "Suonando": the user played `notes[0..n)` on the keyboard.
 * - interval: compares the played note against the interval transposed to start on C (exactly,
 *   or by pitch class if accept_any_octave), not the question's actual random-root note — this
 *   tests recognizing the interval, not recalling the root that happened to play; extra or
 *   missing notes are wrong.
 * - chord: compares the played notes' pitch classes (any octave, any order, duplicates ignored)
 *   against the chord's defining pitch classes; accept_any_octave is ignored (always true).
 * - degree: compares the played note's pitch class against the degree with the tonic on C
 *   (always any octave: a scale degree has no octave of its own), not the question's actual
 *   random-tonic note; accept_any_octave is ignored. */
trainer_eval_t trainer_evaluate_played(const trainer_question_t *q, const int8_t *notes,
                                        uint8_t n, bool accept_any_octave);

/* "Scegliendo": the user picked the option labelled with `chosen_item_id` (the same id space as
 * trainer_question_t.item_id — the interface layer must label its keys/list with real item_ids,
 * e.g. from trainer_interval.h's TRAINER_INTERVAL_ITEM_ID). */
trainer_eval_t trainer_evaluate_chosen(const trainer_question_t *q, uint32_t chosen_item_id);

#endif
