/* SPDX-License-Identifier: GPL-3.0-only */
/* Ear trainer: voice.c's trk_note_on / trk_note_off / trk_all_off are static, so trainer_felucca.c
 * (also unity-built into this same translation unit, right after this file) cannot reach them
 * directly without this small bridge. It is the only place the Trainer touches the base's private
 * track state; everything else in trainer_felucca.c only calls these three functions and the
 * pure-C trainer/ core (../../../trainer/). The Trainer plays on one dedicated track so the
 * normal tracks (0..NTRK-2) are left alone while it is open. */
#define TRAINER_TRACK (NTRK - 1u)

static void trainer_bridge_note_on(uint32_t note, uint32_t vel)
{
    trk_note_on(&trk[TRAINER_TRACK], note, vel);
}
static void trainer_bridge_note_off(uint32_t note)
{
    trk_note_off(&trk[TRAINER_TRACK], note);
}
static void trainer_bridge_all_off(void)
{
    trk_all_off(&trk[TRAINER_TRACK]);
}
