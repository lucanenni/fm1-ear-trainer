#include "trainer_stats.h"
#include <string.h>

#define TRAINER_STATS_MAGIC 0x54535431u   /* "TST1" */

void trainer_stats_init(trainer_stats_t *st)
{
    memset(st, 0, sizeof *st);
}

static trainer_item_stat_t *find(trainer_stats_t *st, uint32_t item_id)
{
    uint32_t i;
    for (i = 0; i < st->count; i++)
        if (st->items[i].item_id == item_id)
            return &st->items[i];
    return NULL;
}

static const trainer_item_stat_t *find_c(const trainer_stats_t *st, uint32_t item_id)
{
    uint32_t i;
    for (i = 0; i < st->count; i++)
        if (st->items[i].item_id == item_id)
            return &st->items[i];
    return NULL;
}

void trainer_stats_record(trainer_stats_t *st, uint32_t item_id, bool correct)
{
    trainer_item_stat_t *it = find(st, item_id);
    if (!it) {
        if (st->count >= TRAINER_STATS_MAX_ITEMS)
            return;
        it = &st->items[st->count++];
        it->item_id = item_id;
        it->attempts = it->correct = 0;
    }
    it->attempts++;
    if (correct)
        it->correct++;
}

uint32_t trainer_stats_weight(const trainer_stats_t *st, uint32_t item_id)
{
    const trainer_item_stat_t *it = find_c(st, item_id);
    uint32_t attempts = it ? it->attempts : 0u;
    uint32_t wrong = it ? (it->attempts - it->correct) : 0u;
    uint32_t denom = attempts ? attempts : 1u;
    return 1u + (TRAINER_ADAPTIVE_WRONG_BOOST * wrong) / denom;
}

uint32_t trainer_stats_pick_weighted(const trainer_stats_t *st, const uint32_t *item_ids,
                                      uint32_t count, trainer_rng_t *rng)
{
    uint32_t total = 0, i, r, acc = 0;
    if (!count)
        return 0;
    for (i = 0; i < count; i++)
        total += trainer_stats_weight(st, item_ids[i]);
    r = trainer_rng_range(rng, total);
    for (i = 0; i < count; i++) {
        acc += trainer_stats_weight(st, item_ids[i]);
        if (r < acc)
            return i;
    }
    return count - 1u;
}

static void put_u32(uint8_t *b, uint32_t v)
{
    b[0] = (uint8_t)(v & 0xFF);
    b[1] = (uint8_t)((v >> 8) & 0xFF);
    b[2] = (uint8_t)((v >> 16) & 0xFF);
    b[3] = (uint8_t)((v >> 24) & 0xFF);
}

static uint32_t get_u32(const uint8_t *b)
{
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8) | ((uint32_t)b[2] << 16) | ((uint32_t)b[3] << 24);
}

size_t trainer_stats_serialize(const trainer_stats_t *st, uint8_t *buf, size_t buf_size)
{
    size_t needed = 8u + (size_t)st->count * 12u;
    uint32_t i;
    if (buf_size < needed)
        return 0;
    put_u32(buf, TRAINER_STATS_MAGIC);
    put_u32(buf + 4, st->count);
    for (i = 0; i < st->count; i++) {
        uint8_t *p = buf + 8u + (size_t)i * 12u;
        put_u32(p, st->items[i].item_id);
        put_u32(p + 4, st->items[i].attempts);
        put_u32(p + 8, st->items[i].correct);
    }
    return needed;
}

bool trainer_stats_deserialize(trainer_stats_t *st, const uint8_t *buf, size_t buf_size)
{
    uint32_t count, i;
    if (buf_size < 8u || get_u32(buf) != TRAINER_STATS_MAGIC)
        return false;
    count = get_u32(buf + 4);
    if (count > TRAINER_STATS_MAX_ITEMS || buf_size < 8u + (size_t)count * 12u)
        return false;
    trainer_stats_init(st);
    st->count = count;
    for (i = 0; i < count; i++) {
        const uint8_t *p = buf + 8u + (size_t)i * 12u;
        st->items[i].item_id = get_u32(p);
        st->items[i].attempts = get_u32(p + 4);
        st->items[i].correct = get_u32(p + 8);
    }
    return true;
}
