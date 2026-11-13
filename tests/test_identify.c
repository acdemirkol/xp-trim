#include <string.h>

#include "core/identify.h"
#include "test.h"

static void set_word(uint8_t *buf, unsigned i, uint16_t v)
{
    buf[2 * i] = (uint8_t)(v & 0xFF);
    buf[2 * i + 1] = (uint8_t)(v >> 8);
}

/* ATA strings: two characters per word, first character in the high byte. */
static void set_string(uint8_t *buf, unsigned first, unsigned nwords, const char *s)
{
    unsigned w;
    size_t len = strlen(s);
    for (w = 0; w < nwords; w++) {
        char hi = (2 * w < len) ? s[2 * w] : ' ';
        char lo = (2 * w + 1 < len) ? s[2 * w + 1] : ' ';
        set_word(buf, first + w, (uint16_t)(((uint8_t)hi << 8) | (uint8_t)lo));
    }
}

static void set_checksum(uint8_t *buf)
{
    unsigned i;
    uint8_t sum = 0;
    buf[510] = 0xA5;
    for (i = 0; i < 511; i++)
        sum = (uint8_t)(sum + buf[i]);
    buf[511] = (uint8_t)(0x100 - sum);
}

/* A typical SATA SSD: LBA48, TRIM, DRAT+RZAT, 8 DSM blocks, non-rotating. */
static void make_ssd(uint8_t *buf)
{
    memset(buf, 0, XT_IDENTIFY_BYTES);
    set_word(buf, 0, 0x0040);
    set_string(buf, 10, 10, "    S1234567890");
    set_string(buf, 23, 4, "FW01");
    set_string(buf, 27, 20, "Example SSD 120GB");
    set_word(buf, 60, 0xFFFF); /* 28-bit capacity saturates */
    set_word(buf, 61, 0x0FFF);
    set_word(buf, 69, 0x4020); /* DRAT | RZAT */
    set_word(buf, 83, 0x4400); /* valid | 48-bit */
    set_word(buf, 100, 0x1000); /* 48-bit capacity 0x0DF91000 sectors */
    set_word(buf, 101, 0x0DF9);
    set_word(buf, 102, 0x0000);
    set_word(buf, 103, 0x0000);
    set_word(buf, 105, 8);
    set_word(buf, 106, 0x4000); /* valid, 512-byte logical sectors */
    set_word(buf, 169, 0x0001);
    set_word(buf, 217, 0x0001);
    set_checksum(buf);
}

static void test_ssd(void)
{
    uint8_t buf[XT_IDENTIFY_BYTES];
    xt_identify id;

    make_ssd(buf);
    CHECK_RESULT(xt_identify_parse(buf, &id), XT_OK);
    CHECK(strcmp(id.model, "Example SSD 120GB") == 0);
    CHECK(strcmp(id.serial, "S1234567890") == 0);
    CHECK(strcmp(id.firmware, "FW01") == 0);
    CHECK(id.lba48);
    CHECK_EQ_U64(id.user_sectors, UINT64_C(0x0DF91000));
    CHECK_EQ_U64(id.logical_sector_size, 512);
    CHECK(id.trim_supported);
    CHECK(id.deterministic_trim);
    CHECK(id.zeroes_after_trim);
    CHECK_EQ_U64(id.dsm_max_blocks, 8);
    CHECK_EQ_U64(id.rotation_rate, 1);
}

static void test_hdd_no_trim(void)
{
    uint8_t buf[XT_IDENTIFY_BYTES];
    xt_identify id;

    make_ssd(buf);
    set_word(buf, 169, 0);
    set_word(buf, 69, 0);
    set_word(buf, 105, 0);
    set_word(buf, 217, 7200);
    set_checksum(buf);
    CHECK_RESULT(xt_identify_parse(buf, &id), XT_OK);
    CHECK(!id.trim_supported);
    CHECK(!id.deterministic_trim);
    CHECK_EQ_U64(id.dsm_max_blocks, 0);
    CHECK_EQ_U64(id.rotation_rate, 7200);
}

static void test_no_lba48(void)
{
    uint8_t buf[XT_IDENTIFY_BYTES];
    xt_identify id;

    make_ssd(buf);
    set_word(buf, 83, 0x4000);   /* valid, 48-bit not supported */
    set_word(buf, 60, 0x5678);
    set_word(buf, 61, 0x0123);
    set_checksum(buf);
    CHECK_RESULT(xt_identify_parse(buf, &id), XT_OK);
    CHECK(!id.lba48);
    CHECK_EQ_U64(id.user_sectors, UINT64_C(0x01235678));

    /* Word 83 with an invalid signature is ignored even if bit 10 is set. */
    set_word(buf, 83, 0x0400);
    set_checksum(buf);
    CHECK_RESULT(xt_identify_parse(buf, &id), XT_OK);
    CHECK(!id.lba48);
}

static void test_logical_sector_size(void)
{
    uint8_t buf[XT_IDENTIFY_BYTES];
    xt_identify id;

    make_ssd(buf);
    set_word(buf, 106, 0x5000); /* valid | logical sector > 256 words */
    set_word(buf, 117, 2048);   /* 2048 words = 4096 bytes */
    set_word(buf, 118, 0);
    set_checksum(buf);
    CHECK_RESULT(xt_identify_parse(buf, &id), XT_OK);
    CHECK_EQ_U64(id.logical_sector_size, 4096);

    /* Bit 12 set but nonsense size. */
    set_word(buf, 117, 0);
    set_checksum(buf);
    CHECK_RESULT(xt_identify_parse(buf, &id), XT_E_BAD_DATA);

    /* Word 106 not valid: bit 12 ignored, default 512. */
    set_word(buf, 106, 0x1000);
    set_checksum(buf);
    CHECK_RESULT(xt_identify_parse(buf, &id), XT_OK);
    CHECK_EQ_U64(id.logical_sector_size, 512);
}

static void test_checksum(void)
{
    uint8_t buf[XT_IDENTIFY_BYTES];
    xt_identify id;

    memset(&id, 0x5A, sizeof id);
    make_ssd(buf);
    buf[200] ^= 0x01; /* corrupt one byte */
    CHECK_RESULT(xt_identify_parse(buf, &id), XT_E_BAD_DATA);
    CHECK_EQ_U64(id.dsm_max_blocks, 0x5A5A); /* untouched */

    /* Without the 0xA5 signature there is no checksum to verify. */
    buf[510] = 0;
    buf[511] = 0;
    CHECK_RESULT(xt_identify_parse(buf, &id), XT_OK);
}

static void test_not_ata(void)
{
    uint8_t buf[XT_IDENTIFY_BYTES];
    xt_identify id;

    make_ssd(buf);
    set_word(buf, 0, 0x8580); /* ATAPI */
    set_checksum(buf);
    CHECK_RESULT(xt_identify_parse(buf, &id), XT_E_BAD_DATA);
    CHECK_RESULT(xt_identify_parse(NULL, &id), XT_E_INVALID_ARG);
    CHECK_RESULT(xt_identify_parse(buf, NULL), XT_E_INVALID_ARG);
}

static void test_strings(void)
{
    uint8_t buf[XT_IDENTIFY_BYTES];
    xt_identify id;

    make_ssd(buf);
    /* All spaces -> empty; control characters -> '?'. */
    set_string(buf, 10, 10, "");
    set_word(buf, 23, (uint16_t)(('A' << 8) | 0x07));
    set_string(buf, 27, 20, "0123456789012345678901234567890123456789");
    set_checksum(buf);
    CHECK_RESULT(xt_identify_parse(buf, &id), XT_OK);
    CHECK(strcmp(id.serial, "") == 0);
    CHECK(strcmp(id.firmware, "A?01") == 0); /* words 24..26 still "01  " */
    CHECK(strlen(id.model) == 40);
}

static void test_empty_buffers(void)
{
    uint8_t buf[XT_IDENTIFY_BYTES];
    xt_identify id;

    memset(buf, 0x00, sizeof buf);
    CHECK_RESULT(xt_identify_parse(buf, &id), XT_E_BAD_DATA);
    memset(buf, 0xFF, sizeof buf);
    CHECK_RESULT(xt_identify_parse(buf, &id), XT_E_BAD_DATA);
}

void suite_identify(void)
{
    test_empty_buffers();
    test_ssd();
    test_hdd_no_trim();
    test_no_lba48();
    test_logical_sector_size();
    test_checksum();
    test_not_ata();
    test_strings();
}
