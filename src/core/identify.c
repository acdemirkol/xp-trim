#include <string.h>

#include "core/identify.h"

static uint16_t word(const uint8_t *buf, unsigned i)
{
    return (uint16_t)(buf[2 * i] | (buf[2 * i + 1] << 8));
}

/* Word i..i+nwords-1, high byte first within each word; trims spaces. */
static void ata_string(const uint8_t *buf, unsigned first, unsigned nwords,
                       char *out, size_t out_size)
{
    size_t len = 0, start = 0, i;
    unsigned w;

    for (w = 0; w < nwords && len + 2 < out_size; w++) {
        uint16_t v = word(buf, first + w);
        out[len++] = (char)(v >> 8);
        out[len++] = (char)(v & 0xFF);
    }
    for (i = 0; i < len; i++) {
        unsigned char ch = (unsigned char)out[i];
        if (ch < 0x20 || ch > 0x7E)
            out[i] = (ch == 0) ? ' ' : '?';
    }
    while (len > 0 && out[len - 1] == ' ')
        len--;
    while (start < len && out[start] == ' ')
        start++;
    memmove(out, out + start, len - start);
    out[len - start] = '\0';
}

/* Words 83/86/106 are only meaningful when bits 15:14 are 01. */
static int word_valid(uint16_t v)
{
    return (v & 0xC000u) == 0x4000u;
}

xt_result xt_identify_parse(const uint8_t *buf, xt_identify *out)
{
    xt_identify id;
    uint16_t w83, w106;

    if (buf == NULL || out == NULL)
        return XT_E_INVALID_ARG;

    {
        /* All-zero or all-0xFF: a driver that "succeeded" without data. */
        unsigned i, zeros = 0, ones = 0;
        for (i = 0; i < XT_IDENTIFY_BYTES; i++) {
            zeros += buf[i] == 0x00;
            ones += buf[i] == 0xFF;
        }
        if (zeros == XT_IDENTIFY_BYTES || ones == XT_IDENTIFY_BYTES)
            return XT_E_BAD_DATA;
    }

    if (word(buf, 0) & 0x8000u)
        return XT_E_BAD_DATA; /* ATAPI or not an ATA device */

    if ((word(buf, 255) & 0xFFu) == 0xA5u) {
        unsigned i;
        uint8_t sum = 0;
        for (i = 0; i < XT_IDENTIFY_BYTES; i++)
            sum = (uint8_t)(sum + buf[i]);
        if (sum != 0)
            return XT_E_BAD_DATA;
    }

    memset(&id, 0, sizeof id);
    ata_string(buf, 27, 20, id.model, sizeof id.model);
    ata_string(buf, 10, 10, id.serial, sizeof id.serial);
    ata_string(buf, 23, 4, id.firmware, sizeof id.firmware);

    w83 = word(buf, 83);
    id.lba48 = word_valid(w83) && (w83 & 0x0400u);
    if (id.lba48) {
        id.user_sectors = (uint64_t)word(buf, 100) |
                          ((uint64_t)word(buf, 101) << 16) |
                          ((uint64_t)word(buf, 102) << 32) |
                          ((uint64_t)word(buf, 103) << 48);
    } else {
        id.user_sectors = (uint64_t)word(buf, 60) | ((uint64_t)word(buf, 61) << 16);
    }

    id.logical_sector_size = 512;
    w106 = word(buf, 106);
    if (word_valid(w106) && (w106 & 0x1000u)) {
        /* Words 117..118: logical sector size in 16-bit words. */
        uint32_t words = (uint32_t)word(buf, 117) | ((uint32_t)word(buf, 118) << 16);
        if (words >= 256 && words <= 0x7FFFFFFFu)
            id.logical_sector_size = words * 2;
        else
            return XT_E_BAD_DATA;
    }

    id.trim_supported = (word(buf, 169) & 0x0001u) != 0;
    id.deterministic_trim = (word(buf, 69) & 0x4000u) != 0;
    id.zeroes_after_trim = (word(buf, 69) & 0x0020u) != 0;
    id.dsm_max_blocks = word(buf, 105);
    id.rotation_rate = word(buf, 217);

    *out = id;
    return XT_OK;
}
