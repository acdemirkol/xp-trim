#include <string.h>

#include "core/ata.h"
#include "test.h"

static void test_identify_cdb(void)
{
    static const uint8_t expected[16] = {
        0x85, 0x08, 0x0E, 0x00, 0x00, 0x00, 0x01, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xEC, 0x00
    };
    uint8_t cdb[XT_SAT_CDB_LEN];

    memset(cdb, 0xAA, sizeof cdb);
    xt_sat16_identify_cdb(cdb);
    CHECK(memcmp(cdb, expected, sizeof expected) == 0);
}

static void test_identify_taskfile(void)
{
    static const uint8_t expected[8] = { 0, 1, 0, 0, 0, 0, 0xEC, 0 };
    uint8_t tf[8];

    memset(tf, 0xAA, sizeof tf);
    xt_ata_identify_taskfile(tf);
    CHECK(memcmp(tf, expected, sizeof expected) == 0);
}

static void test_status(void)
{
    CHECK(!xt_ata_status_failed(0x50)); /* DRDY | DSC */
    CHECK(!xt_ata_status_failed(0x40)); /* DRDY */
    CHECK(xt_ata_status_failed(0x51));  /* ERR */
    CHECK(xt_ata_status_failed(0x70));  /* DF */
    CHECK(xt_ata_status_failed(0x80));  /* BSY */
}

void suite_ata(void)
{
    test_identify_cdb();
    test_identify_taskfile();
    test_status();
}
