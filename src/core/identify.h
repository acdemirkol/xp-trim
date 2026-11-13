/*
 * Parsing of the 512-byte ATA IDENTIFY DEVICE response.
 *
 * Word numbers follow ACS (ATA Command Set); words are little-endian.
 */
#ifndef XPTRIM_CORE_IDENTIFY_H
#define XPTRIM_CORE_IDENTIFY_H

#include <stdint.h>

#include "core/result.h"

#define XT_IDENTIFY_BYTES 512u

typedef struct xt_identify_s {
    char model[41];    /* words 27..46, trimmed, NUL-terminated */
    char serial[21];   /* words 10..19 */
    char firmware[9];  /* words 23..26 */

    int lba48;                   /* word 83 bit 10 (48-bit address feature set) */
    uint64_t user_sectors;       /* words 100..103 if lba48, else 60..61 */
    uint32_t logical_sector_size; /* bytes; from words 106/117..118, else 512 */

    int trim_supported;          /* word 169 bit 0 */
    int deterministic_trim;      /* word 69 bit 14 (DRAT) */
    int zeroes_after_trim;       /* word 69 bit 5 (RZAT) */
    uint16_t dsm_max_blocks;     /* word 105, raw; see xt_dsm_blocks_per_command */
    uint16_t rotation_rate;      /* word 217: 1 = non-rotating (SSD), 0 = not reported */
} xt_identify;

/*
 * Parses an IDENTIFY DEVICE buffer.
 *
 * Fails with XT_E_BAD_DATA if the buffer is all 0x00 or all 0xFF (a driver
 * that reported success without returning data), if word 0 bit 15 says this
 * is not an ATA device,
 * or if the integrity word (255) carries the 0xA5 signature and the checksum
 * does not add up. *out is only written on XT_OK.
 */
xt_result xt_identify_parse(const uint8_t *buf, xt_identify *out);

#endif /* XPTRIM_CORE_IDENTIFY_H */
