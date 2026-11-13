#include <string.h>

#include "core/ata.h"

void xt_ata_identify_taskfile(uint8_t tf[8])
{
    memset(tf, 0, 8);
    tf[XT_TF_SECTOR_COUNT] = 1;
    tf[XT_TF_COMMAND] = XT_ATA_CMD_IDENTIFY;
}

void xt_sat16_identify_cdb(uint8_t cdb[XT_SAT_CDB_LEN])
{
    memset(cdb, 0, XT_SAT_CDB_LEN);
    cdb[0] = XT_SAT_ATA16;
    cdb[1] = 4u << 1;  /* PROTOCOL = 4 (PIO data-in), EXTEND = 0 */
    cdb[2] = 0x0Eu;    /* T_DIR = 1, BYT_BLOK = 1, T_LENGTH = 2 (sector count) */
    cdb[6] = 1;        /* sector count (7:0) */
    cdb[14] = XT_ATA_CMD_IDENTIFY;
}

int xt_ata_status_failed(uint8_t status)
{
    return (status & (XT_ATA_STATUS_ERR | XT_ATA_STATUS_DF | XT_ATA_STATUS_BSY)) != 0;
}
