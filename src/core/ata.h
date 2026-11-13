/*
 * Builders for ATA commands sent through pass-through interfaces.
 * Pure byte layout, so the exact bytes are unit tested.
 */
#ifndef XPTRIM_CORE_ATA_H
#define XPTRIM_CORE_ATA_H

#include <stdint.h>

#define XT_ATA_CMD_IDENTIFY 0xECu

/* SCSI ATA PASS-THROUGH (16) operation code (SAT). */
#define XT_SAT_ATA16 0x85u
#define XT_SAT_CDB_LEN 16u

/* Offsets into an 8-byte ATA task file (ATA_PASS_THROUGH_EX.CurrentTaskFile). */
enum {
    XT_TF_FEATURES = 0,
    XT_TF_SECTOR_COUNT = 1,
    XT_TF_LBA_LOW = 2,
    XT_TF_LBA_MID = 3,
    XT_TF_LBA_HIGH = 4,
    XT_TF_DEVICE = 5,
    XT_TF_COMMAND = 6, /* on return: status register */
    XT_TF_RESERVED = 7
};

/* Status register bits. */
#define XT_ATA_STATUS_ERR 0x01u
#define XT_ATA_STATUS_DF  0x20u
#define XT_ATA_STATUS_BSY 0x80u

/* Task file for IDENTIFY DEVICE (PIO data-in, one 512-byte block). */
void xt_ata_identify_taskfile(uint8_t tf[8]);

/*
 * SAT ATA PASS-THROUGH (16) CDB for IDENTIFY DEVICE:
 * protocol 4 (PIO data-in), T_DIR = from device, BYT_BLOK = blocks,
 * T_LENGTH = sector count field, sector count 1, command 0xEC.
 */
void xt_sat16_identify_cdb(uint8_t cdb[XT_SAT_CDB_LEN]);

/* 1 if a returned status register reports an error, fault or busy device. */
int xt_ata_status_failed(uint8_t status);

#endif /* XPTRIM_CORE_ATA_H */
