/* Host-only unit tests for trainer/. No framework: plain asserts, a pass/fail counter, exit code
 * 0 iff everything passed. Run with trainer/tests/run_tests.sh. */
#include <stdio.h>
#include <string.h>
#include "../trainer_rng.h"
#include "../trainer_interval.h"
#include "../trainer_chord.h"
#include "../trainer_degree.h"
#include "../trainer_question.h"
#include "../trainer_session.h"
#include "../trainer_stats.h"

static int g_fail = 0, g_count = 0;
#define CHECK(cond) do { \
        g_count++; \
        if (!(cond)) { g_fail++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); } \
    } while (0)

static void test_rng(void)
{
    trainer_rng_t a, b;
    uint32_t i;
    trainer_rng_seed(&a, 42);
    trainer_rng_seed(&b, 42);
    for (i = 0; i < 100; i++)
        CHECK(trainer_rng_next(&a) == trainer_rng_next(&b));

    trainer_rng_seed(&a, 1);
    trainer_rng_seed(&b, 2);
    CHECK(trainer_rng_next(&a) != trainer_rng_next(&b));

    trainer_rng_seed(&a, 7);
    for (i = 0; i < 1000; i++)
        CHECK(trainer_rng_range(&a, 5) < 5);
}

static void test_interval_names(void)
{
    CHECK(strcmp(trainer_interval_name(1), "Minor 2nd") == 0);
    CHECK(strcmp(trainer_interval_name(12), "Octave") == 0);
    CHECK(strcmp(trainer_interval_name(24), "15th") == 0);
    CHECK(strcmp(trainer_interval_name(0), "?") == 0);
    CHECK(strcmp(trainer_interval_name(25), "?") == 0);
}

static void test_interval_generate(void)
{
    trainer_interval_config_t cfg;
    trainer_rng_t rng;
    trainer_question_t q;
    int i;

    memset(&cfg, 0, sizeof cfg);
    cfg.semitone_mask = TRAINER_SEMITONE_BIT(9);   /* major 6th only */
    cfg.dir_mask = TRAINER_DIR_BIT(TRAINER_DIR_ASC);
    cfg.root_min = cfg.root_max = 60;
    trainer_rng_seed(&rng, 1);

    for (i = 0; i < 20; i++) {
        CHECK(trainer_interval_generate(&cfg, &rng, &q));
        CHECK(q.type == TRAINER_EX_INTERVAL);
        CHECK(q.event_count == 2);
        CHECK(q.answer.interval.semitones == 9);
        CHECK(q.answer.interval.dir == TRAINER_DIR_ASC);
        CHECK(q.events[0].note == 60);
        CHECK(q.events[1].note == 69);
        CHECK(q.events[0].start_step == 0 && q.events[1].start_step == 1);
        CHECK(q.item_id == TRAINER_INTERVAL_ITEM_ID(9, TRAINER_DIR_ASC));
    }

    cfg.dir_mask = TRAINER_DIR_BIT(TRAINER_DIR_DESC);
    CHECK(trainer_interval_generate(&cfg, &rng, &q));
    CHECK(q.events[1].note == 51);

    cfg.dir_mask = TRAINER_DIR_BIT(TRAINER_DIR_HARMONIC);
    CHECK(trainer_interval_generate(&cfg, &rng, &q));
    CHECK(q.events[0].start_step == 0 && q.events[1].start_step == 0);

    cfg.dir_mask = TRAINER_DIR_BIT(TRAINER_DIR_MIXED);
    for (i = 0; i < 30; i++) {
        CHECK(trainer_interval_generate(&cfg, &rng, &q));
        CHECK(q.answer.interval.dir == TRAINER_DIR_ASC || q.answer.interval.dir == TRAINER_DIR_DESC);
    }

    cfg.semitone_mask = 0;
    CHECK(!trainer_interval_generate(&cfg, &rng, &q));
}

static void test_interval_evaluate(void)
{
    trainer_question_t q;
    int8_t note;
    memset(&q, 0, sizeof q);
    q.type = TRAINER_EX_INTERVAL;
    q.answer.interval.ref_note = 60;
    q.answer.interval.answer_note = 69;

    note = 69;
    CHECK(trainer_evaluate_played(&q, &note, 1, false) == TRAINER_CORRECT);
    note = 68;
    CHECK(trainer_evaluate_played(&q, &note, 1, false) == TRAINER_WRONG);
    note = 69 + 12;
    CHECK(trainer_evaluate_played(&q, &note, 1, false) == TRAINER_WRONG);
    CHECK(trainer_evaluate_played(&q, &note, 1, true) == TRAINER_CORRECT);

    q.item_id = 0xABCD;
    CHECK(trainer_evaluate_chosen(&q, 0xABCD) == TRAINER_CORRECT);
    CHECK(trainer_evaluate_chosen(&q, 0x1234) == TRAINER_WRONG);
}

static void test_chord_generate_and_evaluate(void)
{
    trainer_chord_config_t cfg;
    trainer_rng_t rng;
    trainer_question_t q;
    int8_t played[4];

    memset(&cfg, 0, sizeof cfg);
    cfg.quality_mask = TRAINER_CHORD_QUALITY_BIT(TRAINER_CHORD_MAJOR);
    cfg.voicing_mask = TRAINER_VOICING_BIT(TRAINER_VOICING_BLOCK);
    cfg.root_min = cfg.root_max = 60;
    trainer_rng_seed(&rng, 5);

    CHECK(trainer_chord_generate(&cfg, &rng, &q));
    CHECK(q.event_count == 3);
    CHECK(q.events[0].note == 60 && q.events[1].note == 64 && q.events[2].note == 67);
    CHECK(q.events[0].start_step == 0 && q.events[1].start_step == 0 && q.events[2].start_step == 0);
    CHECK(q.answer.chord.inversion == 0);

    played[0] = 67; played[1] = 60 + 12; played[2] = 64;
    CHECK(trainer_evaluate_played(&q, played, 3, false) == TRAINER_CORRECT);
    played[2] = 65;
    CHECK(trainer_evaluate_played(&q, played, 3, false) == TRAINER_WRONG);

    cfg.inversion_mask = 1u << 1;   /* first inversion only */
    CHECK(trainer_chord_generate(&cfg, &rng, &q));
    CHECK(q.answer.chord.inversion == 1);
    CHECK(q.events[0].note == 64 && q.events[1].note == 67 && q.events[2].note == 72);

    cfg.voicing_mask = TRAINER_VOICING_BIT(TRAINER_VOICING_ARPEGGIO_UP);
    cfg.inversion_mask = 1u;
    CHECK(trainer_chord_generate(&cfg, &rng, &q));
    CHECK(q.events[0].start_step == 0 && q.events[1].start_step == 1 && q.events[2].start_step == 2);

    cfg.quality_mask = TRAINER_CHORD_QUALITY_BIT(TRAINER_CHORD_DOM7);
    CHECK(trainer_chord_generate(&cfg, &rng, &q));
    CHECK(q.event_count == 4);
    CHECK(q.item_id == TRAINER_CHORD_ITEM_ID(TRAINER_CHORD_DOM7, TRAINER_VOICING_ARPEGGIO_UP));
}

static void test_degree_generate_and_evaluate(void)
{
    trainer_degree_config_t cfg;
    trainer_rng_t rng;
    trainer_question_t q;
    int8_t note;

    memset(&cfg, 0, sizeof cfg);
    cfg.scale_mask = TRAINER_SCALE_BIT(TRAINER_SCALE_MAJOR);
    cfg.cadence_mask = TRAINER_CADENCE_BIT(TRAINER_CADENCE_I_IV_V_I);
    cfg.degree_mask = 1u << (5 - 1);   /* degree 5 only */
    cfg.root_min = cfg.root_max = 60;
    trainer_rng_seed(&rng, 3);

    CHECK(trainer_degree_generate(&cfg, &rng, &q));
    CHECK(q.event_count == 13);
    CHECK(q.answer.degree.degree == 5);
    CHECK(q.answer.degree.target_note == 60 + 7);
    /* I */
    CHECK(q.events[0].note == 60 && q.events[1].note == 64 && q.events[2].note == 67);
    /* IV */
    CHECK(q.events[3].note == 65 && q.events[4].note == 69 && q.events[5].note == 72);
    /* V */
    CHECK(q.events[6].note == 67 && q.events[7].note == 71 && q.events[8].note == 74);
    /* I again */
    CHECK(q.events[9].note == 60 && q.events[10].note == 64 && q.events[11].note == 67);
    CHECK(q.events[12].note == 67 && q.events[12].start_step == 4);

    note = 67 + 24;
    CHECK(trainer_evaluate_played(&q, &note, 1, false) == TRAINER_CORRECT);
    note = 66;
    CHECK(trainer_evaluate_played(&q, &note, 1, false) == TRAINER_WRONG);

    cfg.cadence_mask = TRAINER_CADENCE_BIT(TRAINER_CADENCE_DRONE);
    CHECK(trainer_degree_generate(&cfg, &rng, &q));
    CHECK(q.event_count == 2);
    CHECK(q.events[0].note == 60 && q.events[0].dur_steps == 4);
    CHECK(q.events[1].note == 67 && q.events[1].start_step == 4);

    cfg.scale_mask = TRAINER_SCALE_BIT(TRAINER_SCALE_NATURAL_MINOR);
    CHECK(trainer_degree_generate(&cfg, &rng, &q));
    CHECK(q.answer.degree.target_note == 60 + 7);   /* natural minor's 5th degree is also +7 */
    CHECK(q.item_id == TRAINER_DEGREE_ITEM_ID(TRAINER_SCALE_NATURAL_MINOR, 5));
}

static void test_session_fixed(void)
{
    trainer_session_t s;
    trainer_interval_config_t cfg;
    trainer_rng_t rng;
    trainer_question_t q;
    trainer_session_summary_t sum;
    int8_t note;
    uint32_t t = 0;

    memset(&cfg, 0, sizeof cfg);
    cfg.semitone_mask = TRAINER_SEMITONE_BIT(7);
    cfg.dir_mask = TRAINER_DIR_BIT(TRAINER_DIR_ASC);
    cfg.root_min = cfg.root_max = 60;
    trainer_rng_seed(&rng, 9);

    trainer_session_start(&s, TRAINER_EX_INTERVAL, 3);
    CHECK(trainer_interval_generate(&cfg, &rng, &q));
    trainer_session_set_question(&s, &q, t);
    CHECK(s.state == TRAINER_SESSION_ASKING);

    trainer_session_replay(&s);
    trainer_session_replay(&s);
    CHECK(s.current_replays == 2 && s.replays_total == 2);

    note = (int8_t)(60 + 7);
    t += 1000;
    CHECK(trainer_session_answer_played(&s, &note, 1, false, t) == TRAINER_CORRECT);
    CHECK(s.state == TRAINER_SESSION_FEEDBACK && s.asked == 1 && s.correct == 1);
    CHECK(trainer_session_advance(&s));

    CHECK(trainer_interval_generate(&cfg, &rng, &q));
    t += 500;
    trainer_session_set_question(&s, &q, t);
    note = (int8_t)(60 + 7 + 1);   /* wrong */
    t += 2000;
    CHECK(trainer_session_answer_played(&s, &note, 1, false, t) == TRAINER_WRONG);
    CHECK(trainer_session_advance(&s));

    CHECK(trainer_interval_generate(&cfg, &rng, &q));
    t += 100;
    trainer_session_set_question(&s, &q, t);
    CHECK(trainer_session_answer_chosen(&s, q.item_id, t + 1500) == TRAINER_CORRECT);
    CHECK(!trainer_session_advance(&s));
    CHECK(s.state == TRAINER_SESSION_DONE);

    trainer_session_summary(&s, &sum);
    CHECK(sum.asked == 3 && sum.correct == 2);
    CHECK(sum.pct_correct_permille == 666);
    CHECK(sum.has_worst_item && sum.worst_item_wrong_count == 1);
}

static void test_session_infinite(void)
{
    trainer_session_t s;
    trainer_question_t q;
    int i;
    memset(&q, 0, sizeof q);
    q.type = TRAINER_EX_DEGREE;

    trainer_session_start(&s, TRAINER_EX_DEGREE, 0);
    for (i = 0; i < 10; i++) {
        trainer_session_set_question(&s, &q, (uint32_t)i * 100u);
        CHECK(trainer_session_answer_chosen(&s, q.item_id + 1, (uint32_t)i * 100u + 50u) == TRAINER_WRONG);
        CHECK(trainer_session_advance(&s));
        CHECK(s.state != TRAINER_SESSION_DONE);
    }
    CHECK(s.asked == 10);
}

static void test_stats_weight_and_pick(void)
{
    trainer_stats_t st;
    trainer_rng_t rng;
    uint32_t ids[2] = {100, 200};
    uint32_t counts[2] = {0, 0};
    uint32_t i;

    trainer_stats_init(&st);
    CHECK(trainer_stats_weight(&st, 100) == 1);

    trainer_stats_record(&st, 100, true);
    trainer_stats_record(&st, 100, true);
    CHECK(trainer_stats_weight(&st, 100) == 1);   /* always correct: baseline */

    trainer_stats_record(&st, 200, false);
    trainer_stats_record(&st, 200, false);
    trainer_stats_record(&st, 200, true);
    /* 2 wrong / 3 attempts: 1 + 4*2/3 (int div) = 1 + 2 = 3 */
    CHECK(trainer_stats_weight(&st, 200) == 3);

    trainer_rng_seed(&rng, 123);
    for (i = 0; i < 4000; i++)
        counts[trainer_stats_pick_weighted(&st, ids, 2, &rng)]++;
    CHECK(counts[1] > counts[0]);   /* item 200 (weight 3) should come up more than item 100 (weight 1) */
}

static void test_stats_serialize_roundtrip(void)
{
    trainer_stats_t a, b;
    uint8_t buf[64];
    size_t n;

    trainer_stats_init(&a);
    trainer_stats_record(&a, 10, true);
    trainer_stats_record(&a, 10, false);
    trainer_stats_record(&a, 20, true);

    n = trainer_stats_serialize(&a, buf, sizeof buf);
    CHECK(n == 8u + 2u * 12u);
    CHECK(trainer_stats_serialize(&a, buf, n - 1u) == 0);

    CHECK(trainer_stats_deserialize(&b, buf, n));
    CHECK(b.count == a.count);
    CHECK(trainer_stats_weight(&b, 10) == trainer_stats_weight(&a, 10));
    CHECK(trainer_stats_weight(&b, 20) == trainer_stats_weight(&a, 20));

    buf[0] ^= 0xFFu;
    CHECK(!trainer_stats_deserialize(&b, buf, n));
}

int main(void)
{
    test_rng();
    test_interval_names();
    test_interval_generate();
    test_interval_evaluate();
    test_chord_generate_and_evaluate();
    test_degree_generate_and_evaluate();
    test_session_fixed();
    test_session_infinite();
    test_stats_weight_and_pick();
    test_stats_serialize_roundtrip();

    printf("%d/%d checks passed\n", g_count - g_fail, g_count);
    return g_fail ? 1 : 0;
}
