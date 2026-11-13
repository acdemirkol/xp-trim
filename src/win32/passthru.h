/*
 * ATA IDENTIFY DEVICE through the pass-through interfaces Windows XP may offer.
 *
 * Which interface works depends on the service pack and the storage driver,
 * so all are tried at run time, in order, and the first one that returns a
 * valid IDENTIFY buffer wins:
 *
 *   1. IOCTL_ATA_PASS_THROUGH
 *   2. IOCTL_SCSI_PASS_THROUGH with a SAT ATA PASS-THROUGH (16) CDB
 *   3. SMART_RCV_DRIVE_DATA with ID_CMD (identify only; cannot carry TRIM)
 *
 * The only ATA command this module sends is IDENTIFY DEVICE (0xEC), which
 * reads data from the drive and changes nothing. Windows nevertheless
 * requires a read/write handle for these IOCTLs, so the physical disk is
 * opened GENERIC_READ | GENERIC_WRITE and administrator rights are needed.
 */
#ifndef XPTRIM_WIN32_PASSTHRU_H
#define XPTRIM_WIN32_PASSTHRU_H

#include <windows.h>
#include <stdint.h>

#include "core/identify.h"

typedef enum {
    XT_PT_NONE = 0,
    XT_PT_ATA,   /* IOCTL_ATA_PASS_THROUGH */
    XT_PT_SAT,   /* IOCTL_SCSI_PASS_THROUGH + ATA PASS-THROUGH (16) */
    XT_PT_SMART, /* SMART_RCV_DRIVE_DATA */
    XT_PT_COUNT
} xt_pt_method;

/* Application-defined result codes (bit 29 set, never a Win32 error). */
#define XT_PT_E_STATUS   0x20000001u /* ATA status reported an error */
#define XT_PT_E_SCSI     0x20000002u /* SCSI status was not GOOD */
#define XT_PT_E_DRIVER   0x20000003u /* SMART driver status reported an error */
#define XT_PT_E_BAD_DATA 0x20000004u /* returned buffer failed validation */

typedef struct {
    DWORD open_error;              /* non-zero: the disk could not be opened */
    xt_pt_method method;           /* method that succeeded, or XT_PT_NONE */
    DWORD result[XT_PT_COUNT];     /* per method: 0 ok, Win32 error or XT_PT_E_* */
    int tried[XT_PT_COUNT];
} xt_pt_report;

/*
 * Reads and parses IDENTIFY DEVICE from \\.\PhysicalDrive<disk_number>.
 * Returns 1 and fills *id on success. Returns 0 if the disk could not be
 * opened (rep->open_error) or no method produced valid data.
 */
int xt_identify_disk(DWORD disk_number, xt_identify *id, xt_pt_report *rep);

const wchar_t *xt_pt_method_name(xt_pt_method m);

#endif /* XPTRIM_WIN32_PASSTHRU_H */
