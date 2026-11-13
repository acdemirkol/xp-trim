#include <string.h>

#include "core/dsm.h"
#include "test.h"

#define BUF_BYTES (XT_DSM_BLOCKS_CAP * XT_DSM_BLOCK_BYTES)

static void test_blocks_per_command(void)
{
    CHECK_EQ_U64(xt_dsm_blocks_per_command(0), 1);
    CHECK_EQ_U64(xt_dsm_blocks_per_command(1), 1);
    CHECK_EQ_U64(xt_dsm_blocks_per_command(8), 8);
    CHECK_EQ_U64(xt_dsm_blocks_per_command(0xFFFF), XT_DSM_BLOCKS_CAP);
}

static void test_encoding(void)
{
    /* LBA 0x0000123456789ABC, 0x00FF sectors -> little-endian bytes. */
    static const uint8_t expected[8] = { 0xBC, 0x9A, 0x78, 0x56, 0x34, 0x12, 0xFF, 0x00 };
    xt_lba_range r = { UINT64_C(0x123456789ABC), 0xFF };
    xt_dsm_cursor c;
    uint8_t buf[BUF_BYTES];
    uint32_t blocks, entries;
    uint64_t sectors, lba;
    uint16_t cnt;
    size_t i;
    int rest_zero = 1;

    memset(buf, 0xCC, sizeof buf);
    xt_dsm_cursor_init(&c, &r, 1);
    CHECK_RESULT(xt_dsm_pack(&c, buf, 1, &blocks, &entries, &sectors), XT_OK);
    CHECK_EQ_U64(blocks, 1);
    CHECK_EQ_U64(entries, 1);
    CHECK_EQ_U64(sectors, 0xFF);
    CHECK(memcmp(buf, expected, 8) == 0);
    for (i = 8; i < XT_DSM_BLOCK_BYTES; i++)
        if (buf[i] != 0)
            rest_zero = 0;
    CHECK(rest_zero);
    /* Bytes beyond max_blocks are not touched. */
    CHECK(buf[XT_DSM_BLOCK_BYTES] == 0xCC);

    xt_dsm_decode_entry(buf, &lba, &cnt);
    CHECK_EQ_U64(lba, UINT64_C(0x123456789ABC));
    CHECK_EQ_U64(cnt, 0xFF);

    CHECK(xt_dsm_cursor_done(&c));
    CHECK_RESULT(xt_dsm_pack(&c, buf, 1, &blocks, &entries, &sectors), XT_OK);
    CHECK_EQ_U64(blocks, 0);
    CHECK_EQ_U64(entries, 0);
}

static void test_split_65535(void)
{
    /* 65535 * 2 + 10 sectors -> three entries. */
    xt_lba_range r = { 1000, 65535u * 2 + 10 };
    xt_dsm_cursor c;
    uint8_t buf[BUF_BYTES];
    uint32_t blocks, entries;
    uint64_t sectors, lba;
    uint16_t cnt;

    xt_dsm_cursor_init(&c, &r, 1);
    CHECK_RESULT(xt_dsm_pack(&c, buf, 1, &blocks, &entries, &sectors), XT_OK);
    CHECK_EQ_U64(entries, 3);
    CHECK_EQ_U64(sectors, r.count);

    xt_dsm_decode_entry(buf + 0, &lba, &cnt);
    CHECK_EQ_U64(lba, 1000); CHECK_EQ_U64(cnt, 65535);
    xt_dsm_decode_entry(buf + 8, &lba, &cnt);
    CHECK_EQ_U64(lba, 1000 + 65535); CHECK_EQ_U64(cnt, 65535);
    xt_dsm_decode_entry(buf + 16, &lba, &cnt);
    CHECK_EQ_U64(lba, 1000 + 2 * 65535); CHECK_EQ_U64(cnt, 10);
    xt_dsm_decode_entry(buf + 24, &lba, &cnt);
    CHECK_EQ_U64(cnt, 0); /* unused entry */
}

static void test_split_boundary(void)
{
    xt_lba_range exact = { 0, 65535 };
    xt_lba_range one_over = { 0, 65536 };
    xt_dsm_cursor c;
    uint8_t buf[BUF_BYTES];
    uint32_t blocks, entries;
    uint64_t sectors, lba;
    uint16_t cnt;

    xt_dsm_cursor_init(&c, &exact, 1);
    CHECK_RESULT(xt_dsm_pack(&c, buf, 1, &blocks, &entries, &sectors), XT_OK);
    CHECK_EQ_U64(entries, 1);
    xt_dsm_decode_entry(buf, &lba, &cnt);
    CHECK_EQ_U64(cnt, 65535);

    xt_dsm_cursor_init(&c, &one_over, 1);
    CHECK_RESULT(xt_dsm_pack(&c, buf, 1, &blocks, &entries, &sectors), XT_OK);
    CHECK_EQ_U64(entries, 2);
    CHECK_EQ_U64(sectors, 65536);
    xt_dsm_decode_entry(buf, &lba, &cnt);
    CHECK_EQ_U64(lba, 0); CHECK_EQ_U64(cnt, 65535);
    xt_dsm_decode_entry(buf + 8, &lba, &cnt);
    CHECK_EQ_U64(lba, 65535); CHECK_EQ_U64(cnt, 1);
}

/* Packs everything and checks the entries cover exactly the input ranges. */
static void test_multi_command_coverage(void)
{
    enum { N = 150 };
    xt_lba_range r[N];
    xt_dsm_cursor c;
    uint8_t buf[BUF_BYTES];
    uint32_t blocks, entries, total_entries = 0, commands = 0;
    uint64_t sectors, total_sectors = 0, expected_sectors = 0;
    size_t i, ri = 0;
    uint64_t next_lba = 0;
    int contiguous = 1;

    for (i = 0; i < N; i++) {
        r[i].lba = 1000000 + (uint64_t)i * 200;
        r[i].count = 100;
        expected_sectors += 100;
    }

    xt_dsm_cursor_init(&c, r, N);
    next_lba = r[0].lba;
    while (!xt_dsm_cursor_done(&c)) {
        uint32_t e;
        CHECK_RESULT(xt_dsm_pack(&c, buf, 1, &blocks, &entries, &sectors), XT_OK);
        if (entries == 0)
            break;
        CHECK_EQ_U64(blocks, 1);
        CHECK(entries <= XT_DSM_ENTRIES_PER_BLOCK);
        for (e = 0; e < entries; e++) {
            uint64_t lba;
            uint16_t cnt;
            xt_dsm_decode_entry(buf + e * 8, &lba, &cnt);
            if (ri >= N || lba != next_lba || cnt != 100)
                contiguous = 0;
            ri++;
            if (ri < N)
                next_lba = r[ri].lba;
        }
        total_entries += entries;
        total_sectors += sectors;
        commands++;
    }
    CHECK(contiguous);
    CHECK_EQ_U64(total_entries, N);
    CHECK_EQ_U64(total_sectors, expected_sectors);
    CHECK_EQ_U64(commands, 3); /* 64 + 64 + 22 */
}

static void test_multi_block(void)
{
    /* 100 ranges in one command of up to 2 blocks -> 2 blocks used. */
    xt_lba_range r[100];
    xt_dsm_cursor c;
    uint8_t buf[BUF_BYTES];
    uint32_t blocks, entries;
    uint64_t sectors;
    size_t i;

    for (i = 0; i < 100; i++) {
        r[i].lba = (uint64_t)i * 10;
        r[i].count = 1;
    }
    xt_dsm_cursor_init(&c, r, 100);
    CHECK_RESULT(xt_dsm_pack(&c, buf, 2, &blocks, &entries, &sectors), XT_OK);
    CHECK_EQ_U64(blocks, 2);
    CHECK_EQ_U64(entries, 100);
    CHECK(xt_dsm_cursor_done(&c));
}

static void test_errors(void)
{
    xt_lba_range r = { XT_LBA48_LIMIT - 5, 10 };
    xt_lba_range z = { 10, 0 };
    xt_dsm_cursor c;
    uint8_t buf[BUF_BYTES];
    uint32_t blocks, entries;
    uint64_t sectors;

    xt_dsm_cursor_init(&c, &r, 1);
    CHECK_RESULT(xt_dsm_pack(&c, buf, 1, &blocks, &entries, &sectors), XT_E_OUT_OF_RANGE);
    CHECK(!xt_dsm_cursor_done(&c));

    xt_dsm_cursor_init(&c, &z, 1);
    CHECK_RESULT(xt_dsm_pack(&c, buf, 1, &blocks, &entries, &sectors), XT_E_INVALID_ARG);

    xt_dsm_cursor_init(&c, &r, 1);
    CHECK_RESULT(xt_dsm_pack(&c, buf, 0, &blocks, &entries, &sectors), XT_E_INVALID_ARG);
    CHECK_RESULT(xt_dsm_pack(&c, buf, XT_DSM_BLOCKS_CAP + 1, &blocks, &entries, &sectors),
                 XT_E_INVALID_ARG);
    CHECK_RESULT(xt_dsm_pack(&c, NULL, 1, &blocks, &entries, &sectors), XT_E_INVALID_ARG);
}

void suite_dsm(void)
{
    test_blocks_per_command();
    test_encoding();
    test_split_65535();
    test_split_boundary();
    test_multi_command_coverage();
    test_multi_block();
    test_errors();
}
