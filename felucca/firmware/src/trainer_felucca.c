/* SPDX-License-Identifier: GPL-3.0-only */
/* The ear-trainer overlay (MENU > TRAINER, menu_items.c MI_TRAINER / ui_menu.c menu_input): while
 * ui.trainer is set, ui_draw_page() calls draw_trainer() instead of the normal UI and ui_input()
 * calls trainer_input() instead of the normal input handling (both touch-points are one `if`
 * each, next to the same thing UBOOT already does). Everything here is new, project-specific code:
 * it only calls the base through trainer_bridge.c (note on/off), panel.c/gfx.c (input labels,
 * text drawing) and plain globals (fm1_in, fm1_ms, ui, panel) that the base already exposes to
 * every page. The actual exercise logic (question generation, answer checking, the session state
 * machine, adaptive stats) is entirely in ../../../trainer/, compiled and tested on its own. */
#include "../../../trainer/trainer_types.h"
#include "../../../trainer/trainer_rng.h"
#include "../../../trainer/trainer_interval.h"
#include "../../../trainer/trainer_chord.h"
#include "../../../trainer/trainer_degree.h"
#include "../../../trainer/trainer_question.h"
#include "../../../trainer/trainer_session.h"
#include "../../../trainer/trainer_stats.h"

/* ---------------------------------------------------------------- levels --- */
/* Predefined levels per exercise (spec 3.5's own example for intervals: "L1 = 3a M, 5a G, 8a; L2
 * = aggiunge 4a G, 6a M"). Chord and degree levels follow the same idea: start with the easiest
 * handful, grow it. Sevenths (spec 3.1.2: "livello avanzato") and the melodic/harmonic extras
 * (spec 3.2) are left for Fase 4. */
static const uint8_t TR_IV_L1[] = {4, 7, 12};                        /* M3, P5, P8 */
static const uint8_t TR_IV_L2[] = {4, 5, 7, 9, 12};                  /* + P4, M6 */
static const uint8_t TR_IV_L3[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12};   /* every interval in an octave */
static const struct { const uint8_t *semitones; uint8_t n; } TR_IV_LEVEL[] = {
    {TR_IV_L1, (uint8_t)NELEM(TR_IV_L1)}, {TR_IV_L2, (uint8_t)NELEM(TR_IV_L2)}, {TR_IV_L3, (uint8_t)NELEM(TR_IV_L3)},
};

static const uint8_t TR_CH_L1[] = {TRAINER_CHORD_MAJOR, TRAINER_CHORD_MINOR};
static const uint8_t TR_CH_L2[] = {TRAINER_CHORD_MAJOR, TRAINER_CHORD_MINOR, TRAINER_CHORD_DIM, TRAINER_CHORD_AUG};
static const uint8_t TR_CH_L3[] = {TRAINER_CHORD_MAJOR, TRAINER_CHORD_MINOR, TRAINER_CHORD_DIM, TRAINER_CHORD_AUG,
                                    TRAINER_CHORD_SUS2, TRAINER_CHORD_SUS4};
static const struct { const uint8_t *quality; uint8_t n; } TR_CH_LEVEL[] = {
    {TR_CH_L1, (uint8_t)NELEM(TR_CH_L1)}, {TR_CH_L2, (uint8_t)NELEM(TR_CH_L2)}, {TR_CH_L3, (uint8_t)NELEM(TR_CH_L3)},
};

static const uint8_t TR_DG_L1[] = {TRAINER_SCALE_MAJOR};
static const uint8_t TR_DG_L2[] = {TRAINER_SCALE_MAJOR, TRAINER_SCALE_NATURAL_MINOR};
static const struct { const uint8_t *scale; uint8_t n; } TR_DG_LEVEL[] = {
    {TR_DG_L1, (uint8_t)NELEM(TR_DG_L1)}, {TR_DG_L2, (uint8_t)NELEM(TR_DG_L2)},
};

static uint32_t tr_level_count(trainer_exercise_t ex)
{
    switch (ex) {
    case TRAINER_EX_INTERVAL: return (uint32_t)NELEM(TR_IV_LEVEL);
    case TRAINER_EX_CHORD:    return (uint32_t)NELEM(TR_CH_LEVEL);
    default:                  return (uint32_t)NELEM(TR_DG_LEVEL);
    }
}

/* ----------------------------------------------------------- generation --- */
/* Each of these builds the small list of item_ids the current level allows, lets trainer_stats.h
 * pick one (spec 3.5: wrong-more-often items come up more often), then narrows the real generator's
 * config to that single item so the tested trainer/ code still does the actual construction (root
 * note, event list, item_id) — this file never builds a trainer_question_t by hand. */
static bool tr_generate_interval(uint32_t level, trainer_rng_t *rng, const trainer_stats_t *st, trainer_question_t *out)
{
    uint32_t ids[12u * 3u], n = 0, i, d, pick, chosen;
    trainer_interval_config_t cfg;
    for (i = 0; i < TR_IV_LEVEL[level].n; i++)
        for (d = 0; d < 3u; d++)                                    /* ASC, DESC, HARMONIC (MIXED is never stored) */
            ids[n++] = TRAINER_INTERVAL_ITEM_ID(TR_IV_LEVEL[level].semitones[i], d);
    pick = trainer_stats_pick_weighted(st, ids, n, rng);
    chosen = ids[pick];
    cfg.semitone_mask = TRAINER_SEMITONE_BIT((chosen >> 4) & 0xFFu);
    cfg.dir_mask = TRAINER_DIR_BIT(chosen & 0xFu);
    cfg.root_min = 48; cfg.root_max = 64;                           /* C3..E4: room for a 12-semitone interval */
    return trainer_interval_generate(&cfg, rng, out);
}
static bool tr_generate_chord(uint32_t level, trainer_rng_t *rng, const trainer_stats_t *st, trainer_question_t *out)
{
    uint32_t ids[8], n = 0, i, pick, chosen;
    trainer_chord_config_t cfg;
    for (i = 0; i < TR_CH_LEVEL[level].n; i++)
        ids[n++] = TRAINER_CHORD_ITEM_ID(TR_CH_LEVEL[level].quality[i], TRAINER_VOICING_BLOCK);
    pick = trainer_stats_pick_weighted(st, ids, n, rng);
    chosen = ids[pick];
    cfg.quality_mask = TRAINER_CHORD_QUALITY_BIT((chosen >> 4) & 0xFFu);
    cfg.voicing_mask = TRAINER_VOICING_BIT(chosen & 0xFu);
    cfg.inversion_mask = 1u;                                        /* root position only for the MVP */
    cfg.root_min = 48; cfg.root_max = 60;
    return trainer_chord_generate(&cfg, rng, out);
}
static bool tr_generate_degree(uint32_t level, trainer_rng_t *rng, const trainer_stats_t *st, trainer_question_t *out)
{
    uint32_t ids[14], n = 0, s, d, pick, chosen;
    trainer_degree_config_t cfg;
    for (s = 0; s < TR_DG_LEVEL[level].n; s++)
        for (d = 1; d <= 7u; d++)
            ids[n++] = TRAINER_DEGREE_ITEM_ID(TR_DG_LEVEL[level].scale[s], d);
    pick = trainer_stats_pick_weighted(st, ids, n, rng);
    chosen = ids[pick];
    cfg.scale_mask = TRAINER_SCALE_BIT((chosen >> 4) & 0xFFu);
    cfg.cadence_mask = TRAINER_CADENCE_BIT(TRAINER_CADENCE_I_IV_V_I) | TRAINER_CADENCE_BIT(TRAINER_CADENCE_DRONE);
    cfg.degree_mask = 1u << (chosen & 0xFu);
    cfg.root_min = 48; cfg.root_max = 60;
    return trainer_degree_generate(&cfg, rng, out);
}

/* ------------------------------------------------------------------ state --- */
typedef struct { int8_t note; uint32_t on_ms, off_ms; uint8_t on_done, off_done; } tr_sched_ev_t;

#define TR_STEP_MS 320u                /* one abstract step (trainer_event_t) = this long; spec 3.6's tempo,
                                        * fixed for this phase */
#define TR_VEL 100u                    /* spec 3.6: fixed velocity for questions, no hints from dynamics */
#define TR_FEEDBACK_MS 1400u           /* spec 3.4 step 5: "automatically"; a fixed pause while feedback shows */

static trainer_rng_t tr_rng;
static trainer_stats_t tr_stats;
static trainer_session_t tr_sess;
static struct { trainer_exercise_t ex; uint32_t level; } tr_cfg = {TRAINER_EX_INTERVAL, 0};
static uint32_t tr_feedback_t0;
static uint32_t tr_prev_held_mask;
static tr_sched_ev_t tr_sched[TRAINER_MAX_EVENTS];
static uint8_t tr_sched_n;
static uint32_t tr_sched_t0;
static uint8_t tr_playing;

static bool tr_generate(trainer_question_t *out)
{
    switch (tr_cfg.ex) {
    case TRAINER_EX_INTERVAL: return tr_generate_interval(tr_cfg.level, &tr_rng, &tr_stats, out);
    case TRAINER_EX_CHORD:    return tr_generate_chord(tr_cfg.level, &tr_rng, &tr_stats, out);
    default:                  return tr_generate_degree(tr_cfg.level, &tr_rng, &tr_stats, out);
    }
}
static void tr_schedule_play(const trainer_question_t *q)
{
    uint8_t i;
    trainer_bridge_all_off();
    tr_sched_n = q->event_count < TRAINER_MAX_EVENTS ? q->event_count : TRAINER_MAX_EVENTS;
    for (i = 0; i < tr_sched_n; i++) {
        tr_sched[i].note = q->events[i].note;
        tr_sched[i].on_ms = (uint32_t)q->events[i].start_step * TR_STEP_MS;
        tr_sched[i].off_ms = tr_sched[i].on_ms + (uint32_t)q->events[i].dur_steps * TR_STEP_MS - TR_STEP_MS / 4u;
        tr_sched[i].on_done = tr_sched[i].off_done = 0;
    }
    tr_sched_t0 = fm1_ms;
    tr_playing = tr_sched_n != 0;
}
static void tr_sched_poll(void)
{
    uint32_t t, i; uint8_t all_done = 1;
    if (!tr_playing)
        return;
    t = fm1_ms - tr_sched_t0;
    for (i = 0; i < tr_sched_n; i++) {
        if (!tr_sched[i].on_done && t >= tr_sched[i].on_ms) {
            trainer_bridge_note_on((uint32_t)tr_sched[i].note, TR_VEL);
            tr_sched[i].on_done = 1;
        }
        if (tr_sched[i].on_done && !tr_sched[i].off_done && t >= tr_sched[i].off_ms) {
            trainer_bridge_note_off((uint32_t)tr_sched[i].note);
            tr_sched[i].off_done = 1;
        }
        all_done &= tr_sched[i].off_done;
    }
    if (all_done)
        tr_playing = 0;
}
static void tr_next_question(void)
{
    trainer_question_t q;
    if (tr_generate(&q)) {
        trainer_session_set_question(&tr_sess, &q, fm1_ms);
        tr_schedule_play(&tr_sess.current);
    }
    tr_prev_held_mask = 0;
}
static void tr_advance(void)
{
    if (trainer_session_advance(&tr_sess))
        tr_next_question();                 /* DONE is unreachable here: sessions run with total_questions = 0 */
}
static void tr_enter(void)
{
    if (!tr_rng.state)                       /* trainer_rng_t's only field; 0 only before the first seed */
        trainer_rng_seed(&tr_rng, fm1_ms | 1u);
    set_engine_of(&trk[TRAINER_TRACK], ENGI_FM6);     /* spec 3.6: a plain, listenable tone regardless of
                                                        * whatever this track last had (preset 5: a held
                                                        * sine, hostsim.c's own "ANALOG SINE KEY" comment) */
    apply_preset_to(&trk[TRAINER_TRACK], 5u);
    trk[TRAINER_TRACK].p[P_VOICE] = V_POLY;           /* chords (block voicing) need more than one voice */
    trainer_session_start(&tr_sess, tr_cfg.ex, 0u);   /* 0: infinite (spec 3.5's N-question sessions: Fase 4) */
    tr_next_question();
}
static void tr_exit(void)
{
    trainer_bridge_all_off();
    ui.trainer = 0;
    ui.force = 1;
}
static void tr_level_step(int32_t dir)
{
    int32_t lv = (int32_t)tr_cfg.level + (dir > 0 ? 1 : -1);
    uint32_t cnt = tr_level_count(tr_cfg.ex);
    if (lv < 0) lv = 0;
    if (lv >= (int32_t)cnt) lv = (int32_t)cnt - 1;
    tr_cfg.level = (uint32_t)lv;
    tr_next_question();
}
static void tr_switch_exercise(int32_t dir)
{
    int32_t ex = (int32_t)tr_cfg.ex + (dir > 0 ? 1 : -1);
    if (ex < 0) ex = TRAINER_EX_COUNT - 1;
    if (ex >= TRAINER_EX_COUNT) ex = 0;
    tr_cfg.ex = (trainer_exercise_t)ex;
    tr_cfg.level = 0;
    trainer_session_start(&tr_sess, tr_cfg.ex, 0u);     /* a new exercise starts a new session (spec 3.5's summary
                                                          * is per exercise run, not mixed across exercises) */
    tr_next_question();
}
/* Suonando (spec 3.3): the answer is whatever is held on the keybed when every key is let go
 * again — one note for an interval or a degree, a cluster for a chord — read straight from
 * fm1_in.notes (bit k = key k, 0 = F3 .. 26 = G5; the same chromatic mapping seq.c's kb_pitch
 * uses for its F3 base, 53). Octave is always accepted (spec 3.3's option), fixed on for the MVP. */
static void tr_answer_poll(void)
{
    uint32_t held = fm1_in.notes;
    int8_t ans[27]; uint8_t n, k;
    trainer_eval_t r;
    if (held) { tr_prev_held_mask = held; return; }
    if (!tr_prev_held_mask) return;
    if (tr_sess.state != TRAINER_SESSION_ASKING || tr_playing) { tr_prev_held_mask = 0; return; }
    for (n = 0, k = 0; k < 27u; k++)
        if ((tr_prev_held_mask >> k) & 1u)
            ans[n++] = (int8_t)(53u + k);
    tr_prev_held_mask = 0;
    r = trainer_session_answer_played(&tr_sess, ans, n, true, fm1_ms);
    trainer_stats_record(&tr_stats, tr_sess.current.item_id, r == TRAINER_CORRECT);
    tr_feedback_t0 = fm1_ms;
    if (r != TRAINER_CORRECT)
        tr_schedule_play(&tr_sess.current);             /* spec 3.4 step 4: wrong -> hear the correct answer */
}
static void trainer_input(uint32_t pressed, uint32_t notes)
{
    (void)notes;
    tr_sched_poll();
    tr_answer_poll();
    if ((pressed >> panel.btn[B_HOME]) & 1u) { tr_exit(); return; }
    if ((pressed >> panel.btn[B_PLAY]) & 1u) {          /* spec 3.4 step 2: replay, counted */
        if (tr_sess.state == TRAINER_SESSION_ASKING)
            trainer_session_replay(&tr_sess);
        tr_schedule_play(&tr_sess.current);
    }
    if ((pressed >> panel.btn[B_REC]) & 1u)             /* spec 3.4 step 4: hear/compare the correct answer */
        tr_schedule_play(&tr_sess.current);
    if ((pressed >> panel.btn[B_OCTDN]) & 1u) tr_level_step(-1);
    if ((pressed >> panel.btn[B_OCTUP]) & 1u) tr_level_step(1);
    { int32_t s = panel_enc(EN_SELECT); if (s) tr_switch_exercise(s); }
    if (tr_sess.state == TRAINER_SESSION_FEEDBACK && fm1_ms - tr_feedback_t0 >= TR_FEEDBACK_MS)
        tr_advance();
}

/* ------------------------------------------------------------------ draw --- */
static const char *const TR_EX_NAME[TRAINER_EX_COUNT] = {"INTERVAL", "CHORD", "DEGREE"};
static const char *const TR_EX_HINT[TRAINER_EX_COUNT] = {
    "PLAY THE 2ND NOTE, ANY OCTAVE",
    "PLAY ALL CHORD NOTES TOGETHER",
    "PLAY THE TARGET NOTE",
};

static uint32_t tr_fmt_u32(char *b, uint32_t v)
{
    char tmp[10]; uint32_t n = 0, i;
    if (!v) { b[0] = '0'; return 1; }
    while (v && n < NELEM(tmp)) { tmp[n++] = (char)('0' + v % 10u); v /= 10u; }
    for (i = 0; i < n; i++) b[i] = tmp[n - 1u - i];
    return n;
}
static const char *tr_answer_name(const trainer_question_t *q, char *buf)
{
    static const char *const DEG[7] = {"1", "2", "3", "4", "5", "6", "7"};
    switch (q->type) {
    case TRAINER_EX_INTERVAL: return trainer_interval_name(q->answer.interval.semitones);
    case TRAINER_EX_CHORD: return trainer_chord_quality_name((trainer_chord_quality_t)q->answer.chord.quality);
    default: {
        const char *d = DEG[q->answer.degree.degree - 1u];
        buf[0] = 'D'; buf[1] = 'E'; buf[2] = 'G'; buf[3] = 'R'; buf[4] = 'E'; buf[5] = 'E'; buf[6] = ' ';
        buf[7] = d[0]; buf[8] = 0;
        return buf;
    }
    }
}
static void draw_trainer(void)
{
    char buf[32];
    uint32_t len;
    lcd_fill(0, 0, 240, 240, T_BG);
    draw_text_box(0, 10, 240, &AF_M, TR_EX_NAME[tr_cfg.ex], T_TEXT, 1);
    buf[0] = 'L'; buf[1] = (char)('1' + tr_cfg.level); buf[2] = 0;
    draw_text_box(0, 30, 240, &AF_S, buf, T_MID, 1);
    if (tr_sess.state == TRAINER_SESSION_FEEDBACK) {
        int ok = tr_sess.last_result == TRAINER_CORRECT;
        draw_text_box(0, 84, 240, &AF_L, ok ? "CORRECT" : "WRONG", ok ? T_OK : T_WARN, 1);
        draw_text_box(0, 126, 240, &AF_M, tr_answer_name(&tr_sess.current, buf), T_THEME, 1);
    } else {
        draw_text_box(0, 100, 240, &AF_M, tr_playing ? "LISTEN..." : "YOUR ANSWER", T_TEXT, 1);
        if (!tr_playing)
            draw_text_box(0, 130, 240, &AF_S, TR_EX_HINT[tr_cfg.ex], T_MID, 1);
    }
    len = tr_fmt_u32(buf, tr_sess.correct);
    buf[len++] = '/';
    len += tr_fmt_u32(buf + len, tr_sess.asked);
    buf[len] = 0;
    draw_text_box(0, 198, 240, &AF_S, buf, T_MID, 1);
    draw_text_box(0, 218, 240, &AF_S, "PLAY=AGAIN  REC=ANSWER  HOME=EXIT", T_DIM, 1);
    lcd_sync();
}
