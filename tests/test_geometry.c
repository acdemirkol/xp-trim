#include "core/geometry.h"
#include "test.h"

/* Classic XP layout: partition at sector 63, 512-byte sectors, 4 KB clusters. */
static xt_geometry xp_default(void)
{
    xt_geometry g;
    g.partition_offset_bytes = 63ull * 512;
    g.partition_length_bytes = 1000ull * 4096; /* 1000 clusters */
    g.bytes_per_sector = 512;
    g.bytes_per_cluster = 4096;
    return g;
}

static void test_validate(void)
{
    xt_geometry g = xp_default();
    CHECK_RESULT(xt_geometry_validate(&g), XT_OK);
    CHECK_RESULT(xt_geometry_validate(NULL), XT_E_INVALID_ARG);

    g = xp_default(); g.bytes_per_sector = 0;
    CHECK_RESULT(xt_geometry_validate(&g), XT_E_INVALID_ARG);
    g = xp_default(); g.bytes_per_sector = 256;
    CHECK_RESULT(xt_geometry_validate(&g), XT_E_INVALID_ARG);
    g = xp_default(); g.bytes_per_sector = 520;
    CHECK_RESULT(xt_geometry_validate(&g), XT_E_INVALID_ARG);
    g = xp_default(); g.bytes_per_cluster = 256;
    CHECK_RESULT(xt_geometry_validate(&g), XT_E_INVALID_ARG);
    g = xp_default(); g.bytes_per_cluster = 4096 + 512 + 1;
    CHECK_RESULT(xt_geometry_validate(&g), XT_E_INVALID_ARG);
    g = xp_default(); g.partition_length_bytes = 0;
    CHECK_RESULT(xt_geometry_validate(&g), XT_E_INVALID_ARG);
    g = xp_default(); g.partition_offset_bytes = 63ull * 512 + 1;
    CHECK_RESULT(xt_geometry_validate(&g), XT_E_INVALID_ARG);
    g = xp_default(); g.partition_offset_bytes = UINT64_MAX - 511;
    CHECK_RESULT(xt_geometry_validate(&g), XT_E_OVERFLOW);
}

static void test_formula(void)
{
    xt_geometry g = xp_default();
    xt_lba_range r;

    /* LBA = 63 + LCN * 8 */
    CHECK_RESULT(xt_extent_to_lba(&g, 0, 1, &r), XT_OK);
    CHECK_EQ_U64(r.lba, 63);
    CHECK_EQ_U64(r.count, 8);

    CHECK_RESULT(xt_extent_to_lba(&g, 10, 5, &r), XT_OK);
    CHECK_EQ_U64(r.lba, 63 + 80);
    CHECK_EQ_U64(r.count, 40);

    /* Last cluster of the partition is allowed. */
    CHECK_RESULT(xt_extent_to_lba(&g, 999, 1, &r), XT_OK);
    CHECK_EQ_U64(r.lba, 63 + 999 * 8);
    CHECK_EQ_U64(r.count, 8);

    /* The whole partition in one extent. */
    CHECK_RESULT(xt_extent_to_lba(&g, 0, 1000, &r), XT_OK);
    CHECK_EQ_U64(r.count, 8000);
}

static void test_aligned_partition(void)
{
    /* 1 MiB-aligned partition (as created by newer partitioners). */
    xt_geometry g = xp_default();
    xt_lba_range r;
    g.partition_offset_bytes = 1048576;
    CHECK_RESULT(xt_extent_to_lba(&g, 3, 2, &r), XT_OK);
    CHECK_EQ_U64(r.lba, 2048 + 24);
    CHECK_EQ_U64(r.count, 16);
}

static void test_4k_sectors(void)
{
    /* 4096-byte logical sectors: one sector per 4 KB cluster. */
    xt_geometry g;
    xt_lba_range r;
    g.partition_offset_bytes = 256ull * 4096;
    g.partition_length_bytes = 100ull * 65536;
    g.bytes_per_sector = 4096;
    g.bytes_per_cluster = 65536;
    CHECK_RESULT(xt_extent_to_lba(&g, 7, 3, &r), XT_OK);
    CHECK_EQ_U64(r.lba, 256 + 7 * 16);
    CHECK_EQ_U64(r.count, 48);
}

static void test_cluster_equals_sector(void)
{
    xt_geometry g = xp_default();
    xt_lba_range r;
    g.bytes_per_cluster = 512;
    g.partition_length_bytes = 1000ull * 512;
    CHECK_RESULT(xt_extent_to_lba(&g, 999, 1, &r), XT_OK);
    CHECK_EQ_U64(r.lba, 63 + 999);
    CHECK_EQ_U64(r.count, 1);
}

static void test_out_of_partition(void)
{
    xt_geometry g = xp_default();
    xt_lba_range r = { 12345, 678 };

    /* One cluster past the end. */
    CHECK_RESULT(xt_extent_to_lba(&g, 1000, 1, &r), XT_E_OUT_OF_RANGE);
    /* Starts inside, ends one cluster past the end. */
    CHECK_RESULT(xt_extent_to_lba(&g, 999, 2, &r), XT_E_OUT_OF_RANGE);
    CHECK_RESULT(xt_extent_to_lba(&g, 0, 1001, &r), XT_E_OUT_OF_RANGE);
    /* Output untouched on failure. */
    CHECK_EQ_U64(r.lba, 12345);
    CHECK_EQ_U64(r.count, 678);

    CHECK_RESULT(xt_extent_to_lba(&g, 0, 0, &r), XT_E_INVALID_ARG);
    CHECK_RESULT(xt_extent_to_lba(&g, 0, 1, NULL), XT_E_INVALID_ARG);
}

static void test_overflow(void)
{
    xt_geometry g = xp_default();
    xt_lba_range r;
    CHECK_RESULT(xt_extent_to_lba(&g, UINT64_MAX, 1, &r), XT_E_OVERFLOW);
    CHECK_RESULT(xt_extent_to_lba(&g, 0, UINT64_MAX, &r), XT_E_OVERFLOW);
    CHECK_RESULT(xt_extent_to_lba(&g, UINT64_MAX / 4096, UINT64_MAX / 4096, &r),
                 XT_E_OVERFLOW);
}

static void test_lba48_limit(void)
{
    /* Partition ending exactly at the 48-bit limit: last sector is usable. */
    xt_geometry g;
    xt_lba_range r;
    g.bytes_per_sector = 512;
    g.bytes_per_cluster = 512;
    g.partition_offset_bytes = (XT_LBA48_LIMIT - 10) * 512;
    g.partition_length_bytes = 20ull * 512; /* runs 10 sectors past the limit */
    CHECK_RESULT(xt_extent_to_lba(&g, 9, 1, &r), XT_OK);
    CHECK_EQ_U64(r.lba, XT_LBA48_LIMIT - 1);
    CHECK_RESULT(xt_extent_to_lba(&g, 10, 1, &r), XT_E_OUT_OF_RANGE);
    CHECK_RESULT(xt_extent_to_lba(&g, 9, 2, &r), XT_E_OUT_OF_RANGE);
}

static void test_range_check(void)
{
    xt_geometry g = xp_default(); /* LBAs 63 .. 63+8000-1 */
    xt_lba_range r;

    r.lba = 63; r.count = 8000;
    CHECK_RESULT(xt_range_check(&g, &r), XT_OK);
    r.lba = 62; r.count = 1;
    CHECK_RESULT(xt_range_check(&g, &r), XT_E_OUT_OF_RANGE);
    r.lba = 63 + 8000; r.count = 1;
    CHECK_RESULT(xt_range_check(&g, &r), XT_E_OUT_OF_RANGE);
    r.lba = 63 + 7999; r.count = 2;
    CHECK_RESULT(xt_range_check(&g, &r), XT_E_OUT_OF_RANGE);
    r.lba = 63 + 7999; r.count = 1;
    CHECK_RESULT(xt_range_check(&g, &r), XT_OK);
    r.lba = 100; r.count = UINT64_MAX;
    CHECK_RESULT(xt_range_check(&g, &r), XT_E_OUT_OF_RANGE);
    r.lba = 100; r.count = 0;
    CHECK_RESULT(xt_range_check(&g, &r), XT_E_INVALID_ARG);
    CHECK_RESULT(xt_range_check(&g, NULL), XT_E_INVALID_ARG);
}

void suite_geometry(void)
{
    test_validate();
    test_formula();
    test_aligned_partition();
    test_4k_sectors();
    test_cluster_equals_sector();
    test_out_of_partition();
    test_overflow();
    test_lba48_limit();
    test_range_check();
}
