/*
 * ATA DATA SET MANAGEMENT (TRIM) payload packing.
 *
 * The payload is a sequence of 512-byte blocks, each holding 64 little-endian
 * 8-byte LBA Range Entries: bits 0..47 = starting LBA, bits 48..63 = number
 * of sectors (1..65535; 0 marks an unused entry). Unused space is zero.
 *
 * Ranges given to the packer must already have passed xt_range_check();
 * the packer only re-checks the 48-bit limit.
 */
#ifndef XPTRIM_CORE_DSM_H
#define XPTRIM_CORE_DSM_H

#include <stddef.h>
#include <stdint.h>

#include "core/geometry.h"
#include "core/result.h"

#define XT_DSM_BLOCK_BYTES       512u
#define XT_DSM_ENTRY_BYTES       8u
#define XT_DSM_ENTRIES_PER_BLOCK (XT_DSM_BLOCK_BYTES / XT_DSM_ENTRY_BYTES)
#define XT_DSM_MAX_ENTRY_SECTORS 0xFFFFu

/* Upper bound on blocks per command we ever use, whatever the drive reports. */
#define XT_DSM_BLOCKS_CAP 8u

/*
 * Blocks per DATA SET MANAGEMENT command from IDENTIFY word 105.
 * 0 (not reported) is treated as 1; values above XT_DSM_BLOCKS_CAP are capped.
 */
uint32_t xt_dsm_blocks_per_command(uint16_t identify_word105);

/* Walks a range list across successive commands. */
typedef struct {
    const xt_lba_range *ranges;
    size_t count;
    size_t index;    /* current range */
    uint64_t offset; /* sectors of the current range already packed */
} xt_dsm_cursor;

void xt_dsm_cursor_init(xt_dsm_cursor *c, const xt_lba_range *ranges, size_t count);
int xt_dsm_cursor_done(const xt_dsm_cursor *c);

/*
 * Packs the next command's payload into buf (max_blocks * 512 bytes, fully
 * overwritten). Ranges longer than 65535 sectors are split across entries.
 *
 * On XT_OK: *blocks_used = 512-byte blocks to send (0 when nothing was left),
 * *entries = entries written, *sectors = sectors covered.
 * On error the cursor is not advanced past the offending range.
 */
xt_result xt_dsm_pack(xt_dsm_cursor *c, uint8_t *buf, uint32_t max_blocks,
                      uint32_t *blocks_used, uint32_t *entries, uint64_t *sectors);

/* Decodes one 8-byte entry (for tests and dry-run output). */
void xt_dsm_decode_entry(const uint8_t *p, uint64_t *lba, uint16_t *count);

#endif /* XPTRIM_CORE_DSM_H */
