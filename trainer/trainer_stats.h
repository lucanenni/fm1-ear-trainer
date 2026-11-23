/* Persistent per-item statistics and adaptive item selection (spec sec. 3.5). This module never
 * touches flash or any hardware: it only serializes to/from a flat byte buffer; the firmware's own
 * storage layer (see docs/RECON.md sec. 3) is responsible for writing that buffer to flash
 * robustly across power loss, the same way it already does for its own settings. */
#ifndef TRAINER_STATS_H
#define TRAINER_STATS_H

#include <stddef.h>
#include "trainer_types.h"
#include "trainer_rng.h"

/* Max distinct item_ids tracked: interval (24 semitones x 3 resolved directions = 72) + chord (11
 * qualities x 3 voicings = 33) + degree (2 scale types x 7 degrees = 14) = 119, rounded up. */
#define TRAINER_STATS_MAX_ITEMS 128

typedef struct { uint32_t item_id, attempts, correct; } trainer_item_stat_t;

typedef struct {
    trainer_item_stat_t items[TRAINER_STATS_MAX_ITEMS];
    uint32_t count;
} trainer_stats_t;

void trainer_stats_init(trainer_stats_t *st);

/* Adds one attempt at item_id (creating its entry if new; silently ignored if the table is full
 * and the item is unseen — practically unreachable given TRAINER_STATS_MAX_ITEMS above). */
void trainer_stats_record(trainer_stats_t *st, uint32_t item_id, bool correct);

/* Adaptive weight, documented scheme (spec 3.5: "pesatura semplice, documentata"): an item never
 * attempted has the baseline weight 1; one that is wrong `wrong` times out of `attempts` gets
 * weight 1 + TRAINER_ADAPTIVE_WRONG_BOOST * wrong / attempts (integer division), so it is
 * boosted towards (1 + TRAINER_ADAPTIVE_WRONG_BOOST) the more often it is missed, but never
 * excluded (weight is always >= 1): every enabled item can still come up. */
#define TRAINER_ADAPTIVE_WRONG_BOOST 4u
uint32_t trainer_stats_weight(const trainer_stats_t *st, uint32_t item_id);

/* Weighted pick among item_ids[0..count), using trainer_stats_weight() as each one's share.
 * count must be > 0. Returns the index into item_ids that was picked. */
uint32_t trainer_stats_pick_weighted(const trainer_stats_t *st, const uint32_t *item_ids,
                                      uint32_t count, trainer_rng_t *rng);

/* Flat, versioned serialization for the firmware's flash storage layer to write/read as an opaque
 * blob (little-endian, architecture-independent). Returns the number of bytes written/needed;
 * 0 if buf_size is too small for serialize, or the buffer is not recognized for deserialize
 * (st is left as trainer_stats_init() on a failed deserialize). */
size_t trainer_stats_serialize(const trainer_stats_t *st, uint8_t *buf, size_t buf_size);
bool trainer_stats_deserialize(trainer_stats_t *st, const uint8_t *buf, size_t buf_size);

#endif
