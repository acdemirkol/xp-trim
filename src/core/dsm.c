#include <string.h>

#include "core/dsm.h"

uint32_t xt_dsm_blocks_per_command(uint16_t identify_word105)
{
    if (identify_word105 == 0)
        return 1;
    if (identify_word105 > XT_DSM_BLOCKS_CAP)
        return XT_DSM_BLOCKS_CAP;
    return identify_word105;
}

void xt_dsm_cursor_init(xt_dsm_cursor *c, const xt_lba_range *ranges, size_t count)
{
    c->ranges = ranges;
    c->count = count;
    c->index = 0;
    c->offset = 0;
}

int xt_dsm_cursor_done(const xt_dsm_cursor *c)
{
    return c->index >= c->count;
}

static void put_le64(uint8_t *p, uint64_t v)
{
    int i;
    for (i = 0; i < 8; i++)
        p[i] = (uint8_t)(v >> (8 * i));
}

xt_result xt_dsm_pack(xt_dsm_cursor *c, uint8_t *buf, uint32_t max_blocks,
                      uint32_t *blocks_used, uint32_t *entries, uint64_t *sectors)
{
    uint32_t cap, n = 0;
    uint64_t total = 0;

    if (c == NULL || buf == NULL || blocks_used == NULL || entries == NULL ||
        sectors == NULL || max_blocks == 0 || max_blocks > XT_DSM_BLOCKS_CAP)
        return XT_E_INVALID_ARG;
    if (c->count > 0 && c->ranges == NULL)
        return XT_E_INVALID_ARG;

    memset(buf, 0, (size_t)max_blocks * XT_DSM_BLOCK_BYTES);
    cap = max_blocks * XT_DSM_ENTRIES_PER_BLOCK;

    while (n < cap && c->index < c->count) {
        const xt_lba_range *r = &c->ranges[c->index];
        uint64_t remaining, len, lba;

        if (r->count == 0 || c->offset >= r->count)
            return XT_E_INVALID_ARG;

        remaining = r->count - c->offset;
        len = remaining < XT_DSM_MAX_ENTRY_SECTORS ? remaining : XT_DSM_MAX_ENTRY_SECTORS;
        lba = r->lba + c->offset;
        if (r->lba > UINT64_MAX - c->offset || lba >= XT_LBA48_LIMIT ||
            len > XT_LBA48_LIMIT - lba)
            return XT_E_OUT_OF_RANGE;

        put_le64(buf + (size_t)n * XT_DSM_ENTRY_BYTES, lba | (len << 48));
        n++;
        total += len;

        c->offset += len;
        if (c->offset == r->count) {
            c->index++;
            c->offset = 0;
        }
    }

    *entries = n;
    *sectors = total;
    *blocks_used = (n + XT_DSM_ENTRIES_PER_BLOCK - 1) / XT_DSM_ENTRIES_PER_BLOCK;
    return XT_OK;
}

void xt_dsm_decode_entry(const uint8_t *p, uint64_t *lba, uint16_t *count)
{
    uint64_t v = 0;
    int i;
    for (i = 7; i >= 0; i--)
        v = (v << 8) | p[i];
    *lba = v & (XT_LBA48_LIMIT - 1);
    *count = (uint16_t)(v >> 48);
}
