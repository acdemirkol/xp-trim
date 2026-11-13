#include <string.h>

#include "core/identify.h"
#include "core/volcheck.h"
#include "test.h"
#include "win32/volume.h"

/* A supported XP system volume: MBR, type 0x07, starts at sector 63. */
static xt_volume_facts good(void)
{
    xt_volume_facts f;
    f.is_fixed_drive = 1;
    f.fs_is_ntfs = 1;
    f.extent_count = 1;
    f.disk_number = 0;
    f.extent_offset = 63ull * 512;
    f.extent_length = 1000ull * 4096;
    f.partition_style = XT_PS_MBR;
    f.partition_info_ok = 1;
    f.partition_type = 0x07;
    f.partition_offset = 63ull * 512;
    f.partition_length = 1000ull * 4096;
    f.disk_bytes_per_sector = 512;
    f.ntfs_bytes_per_sector = 512;
    f.ntfs_bytes_per_cluster = 4096;
    f.ntfs_total_clusters = 999; /* NTFS keeps the last sector for the backup boot sector */
    f.ntfs_free_clusters = 500;
    return f;
}

static void test_supported(void)
{
    xt_volume_facts f = good();
    xt_geometry g;
    CHECK_RESULT(xt_volume_evaluate(&f, &g), XT_VOL_OK);
    CHECK_EQ_U64(g.partition_offset_bytes, 63 * 512);
    CHECK_EQ_U64(g.partition_length_bytes, 1000ull * 4096);
    CHECK_EQ_U64(g.bytes_per_sector, 512);
    CHECK_EQ_U64(g.bytes_per_cluster, 4096);
    CHECK_RESULT(xt_volume_evaluate(&f, NULL), XT_VOL_OK);
}

static void test_refusals(void)
{
    xt_volume_facts f;
    xt_geometry g;

    g.bytes_per_sector = 1234; /* sentinel: must stay untouched on refusal */

    f = good(); f.is_fixed_drive = 0;
    CHECK_RESULT(xt_volume_evaluate(&f, &g), XT_VOL_NOT_FIXED);
    f = good(); f.fs_is_ntfs = 0;
    CHECK_RESULT(xt_volume_evaluate(&f, &g), XT_VOL_NOT_NTFS);
    f = good(); f.extent_count = 2;
    CHECK_RESULT(xt_volume_evaluate(&f, &g), XT_VOL_MULTI_EXTENT);
    f = good(); f.extent_count = 0;
    CHECK_RESULT(xt_volume_evaluate(&f, &g), XT_VOL_NO_EXTENT);
    f = good(); f.partition_style = XT_PS_GPT;
    CHECK_RESULT(xt_volume_evaluate(&f, &g), XT_VOL_GPT);
    f = good(); f.partition_style = XT_PS_RAW;
    CHECK_RESULT(xt_volume_evaluate(&f, &g), XT_VOL_NOT_MBR);
    f = good(); f.partition_style = XT_PS_UNKNOWN;
    CHECK_RESULT(xt_volume_evaluate(&f, &g), XT_VOL_NOT_MBR);
    f = good(); f.partition_info_ok = 0;
    CHECK_RESULT(xt_volume_evaluate(&f, &g), XT_VOL_NO_PARTITION_INFO);
    f = good(); f.partition_type = 0x42;
    CHECK_RESULT(xt_volume_evaluate(&f, &g), XT_VOL_DYNAMIC);
    f = good(); f.partition_type = 0x87; /* NTFS in an FT set */
    CHECK_RESULT(xt_volume_evaluate(&f, &g), XT_VOL_FT_SET);
    f = good(); f.extent_offset += 512;
    CHECK_RESULT(xt_volume_evaluate(&f, &g), XT_VOL_EXTENT_MISMATCH);
    f = good(); f.extent_length -= 512;
    CHECK_RESULT(xt_volume_evaluate(&f, &g), XT_VOL_EXTENT_MISMATCH);
    f = good(); f.ntfs_bytes_per_sector = 4096;
    CHECK_RESULT(xt_volume_evaluate(&f, &g), XT_VOL_SECTOR_MISMATCH);
    f = good(); f.disk_bytes_per_sector = 0; f.ntfs_bytes_per_sector = 0;
    CHECK_RESULT(xt_volume_evaluate(&f, &g), XT_VOL_SECTOR_MISMATCH);
    f = good(); f.ntfs_bytes_per_cluster = 1000;
    CHECK_RESULT(xt_volume_evaluate(&f, &g), XT_VOL_BAD_GEOMETRY);
    f = good(); f.ntfs_total_clusters = 1001;
    CHECK_RESULT(xt_volume_evaluate(&f, &g), XT_VOL_FS_EXCEEDS_PARTITION);
    f = good(); f.ntfs_total_clusters = 0;
    CHECK_RESULT(xt_volume_evaluate(&f, &g), XT_VOL_FS_EXCEEDS_PARTITION);
    CHECK_RESULT(xt_volume_evaluate(NULL, &g), XT_VOL_BAD_GEOMETRY);

    CHECK_EQ_U64(g.bytes_per_sector, 1234);
}

static void test_strings(void)
{
    int v;
    for (v = XT_VOL_OK; v <= XT_VOL_VERDICT_LAST; v++) {
        const char *s = xt_volume_verdict_str((xt_volume_verdict)v);
        CHECK(s[0] != '\0' && strcmp(s, "unknown") != 0);
    }
}

/* Drive matching good(): partition ends at LBA 63 + 8000. */
static xt_identify good_drive(void)
{
    xt_identify id;
    memset(&id, 0, sizeof id);
    id.lba48 = 1;
    id.user_sectors = 63 + 8000;
    id.logical_sector_size = 512;
    id.trim_supported = 1;
    id.dsm_max_blocks = 8;
    return id;
}

static void test_drive(void)
{
    xt_volume_facts f = good();
    xt_geometry g;
    xt_identify id;

    CHECK_RESULT(xt_volume_evaluate(&f, &g), XT_VOL_OK);

    id = good_drive();
    CHECK_RESULT(xt_drive_evaluate(&id, &g), XT_VOL_OK);
    id = good_drive(); id.user_sectors += 1000000;
    CHECK_RESULT(xt_drive_evaluate(&id, &g), XT_VOL_OK);

    id = good_drive(); id.trim_supported = 0;
    CHECK_RESULT(xt_drive_evaluate(&id, &g), XT_VOL_NO_TRIM);
    id = good_drive(); id.lba48 = 0;
    CHECK_RESULT(xt_drive_evaluate(&id, &g), XT_VOL_NO_LBA48);
    id = good_drive(); id.logical_sector_size = 4096;
    CHECK_RESULT(xt_drive_evaluate(&id, &g), XT_VOL_DRIVE_SECTOR_MISMATCH);
    id = good_drive(); id.user_sectors -= 1;
    CHECK_RESULT(xt_drive_evaluate(&id, &g), XT_VOL_PARTITION_BEYOND_DRIVE);

    CHECK_RESULT(xt_drive_evaluate(NULL, &g), XT_VOL_NO_IDENTIFY);
    id = good_drive();
    CHECK_RESULT(xt_drive_evaluate(&id, NULL), XT_VOL_BAD_GEOMETRY);
}

static void test_parse_drive_letter(void)
{
    CHECK(xt_parse_drive_letter(L"C") == L'C');
    CHECK(xt_parse_drive_letter(L"c:") == L'C');
    CHECK(xt_parse_drive_letter(L"D:\\") == L'D');
    CHECK(xt_parse_drive_letter(L"z:") == L'Z');
    CHECK(xt_parse_drive_letter(L"") == 0);
    CHECK(xt_parse_drive_letter(NULL) == 0);
    CHECK(xt_parse_drive_letter(L"1:") == 0);
    CHECK(xt_parse_drive_letter(L"CD") == 0);
    CHECK(xt_parse_drive_letter(L"C:\\x") == 0);
    CHECK(xt_parse_drive_letter(L"C:/") == 0);
    CHECK(xt_parse_drive_letter(L"\\\\.\\C:") == 0);
    CHECK(xt_parse_drive_letter(L"\\\\.\\PhysicalDrive0") == 0);
}

void suite_volcheck(void)
{
    test_supported();
    test_refusals();
    test_strings();
    test_drive();
    test_parse_drive_letter();
}
