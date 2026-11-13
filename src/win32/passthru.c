#include <windows.h>
#include <winioctl.h>
#include <ntddscsi.h>
#include <stddef.h>
#include <string.h>

#include "core/ata.h"
#include "win32/passthru.h"
#include "win32/volume.h"

#define IDENTIFY_TIMEOUT_S 10

typedef struct {
    ATA_PASS_THROUGH_EX apt;
    ULONG pad;
    UCHAR data[XT_IDENTIFY_BYTES];
} ata_buf;

typedef struct {
    SCSI_PASS_THROUGH spt;
    ULONG pad;
    UCHAR sense[32];
    UCHAR data[XT_IDENTIFY_BYTES];
} sat_buf;

typedef union {
    SENDCMDOUTPARAMS out;
    UCHAR raw[sizeof(SENDCMDOUTPARAMS) - 1 + XT_IDENTIFY_BYTES];
} smart_buf;

static DWORD try_ata(HANDLE h, uint8_t *out)
{
    ata_buf b;
    DWORD got = 0;

    memset(&b, 0, sizeof b);
    b.apt.Length = sizeof(ATA_PASS_THROUGH_EX);
    b.apt.AtaFlags = ATA_FLAGS_DRDY_REQUIRED | ATA_FLAGS_DATA_IN;
    b.apt.DataTransferLength = XT_IDENTIFY_BYTES;
    b.apt.TimeOutValue = IDENTIFY_TIMEOUT_S;
    b.apt.DataBufferOffset = offsetof(ata_buf, data);
    xt_ata_identify_taskfile(b.apt.CurrentTaskFile);

    if (!DeviceIoControl(h, IOCTL_ATA_PASS_THROUGH, &b, sizeof b, &b, sizeof b, &got, NULL))
        return GetLastError();
    if (xt_ata_status_failed(b.apt.CurrentTaskFile[XT_TF_COMMAND]))
        return XT_PT_E_STATUS;
    memcpy(out, b.data, XT_IDENTIFY_BYTES);
    return 0;
}

static DWORD try_sat(HANDLE h, uint8_t *out)
{
    sat_buf b;
    DWORD got = 0;

    memset(&b, 0, sizeof b);
    b.spt.Length = sizeof(SCSI_PASS_THROUGH);
    b.spt.CdbLength = XT_SAT_CDB_LEN;
    b.spt.SenseInfoLength = sizeof b.sense;
    b.spt.DataIn = SCSI_IOCTL_DATA_IN;
    b.spt.DataTransferLength = XT_IDENTIFY_BYTES;
    b.spt.TimeOutValue = IDENTIFY_TIMEOUT_S;
    b.spt.DataBufferOffset = offsetof(sat_buf, data);
    b.spt.SenseInfoOffset = offsetof(sat_buf, sense);
    xt_sat16_identify_cdb(b.spt.Cdb);

    if (!DeviceIoControl(h, IOCTL_SCSI_PASS_THROUGH, &b, sizeof b, &b, sizeof b, &got, NULL))
        return GetLastError();
    if (b.spt.ScsiStatus != 0)
        return XT_PT_E_SCSI;
    memcpy(out, b.data, XT_IDENTIFY_BYTES);
    return 0;
}

static DWORD try_smart(HANDLE h, DWORD disk_number, uint8_t *out)
{
    SENDCMDINPARAMS in;
    smart_buf b;
    DWORD got = 0;

    memset(&in, 0, sizeof in);
    memset(&b, 0, sizeof b);
    in.cBufferSize = XT_IDENTIFY_BYTES;
    in.irDriveRegs.bSectorCountReg = 1;
    in.irDriveRegs.bSectorNumberReg = 1;
    in.irDriveRegs.bDriveHeadReg = 0xA0;
    in.irDriveRegs.bCommandReg = ID_CMD;
    in.bDriveNumber = (BYTE)disk_number;

    if (!DeviceIoControl(h, SMART_RCV_DRIVE_DATA, &in, sizeof in - 1, &b, sizeof b, &got, NULL))
        return GetLastError();
    if (b.out.DriverStatus.bDriverError != 0)
        return XT_PT_E_DRIVER;
    memcpy(out, b.out.bBuffer, XT_IDENTIFY_BYTES);
    return 0;
}

int xt_identify_disk(DWORD disk_number, xt_identify *id, xt_pt_report *rep)
{
    wchar_t path[40];
    HANDLE h;
    uint8_t data[XT_IDENTIFY_BYTES];
    int m;

    memset(rep, 0, sizeof *rep);

    xt_physical_drive_path(disk_number, path, sizeof path / sizeof path[0]);
    h = CreateFileW(path, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE,
                    NULL, OPEN_EXISTING, 0, NULL);
    if (h == INVALID_HANDLE_VALUE) {
        rep->open_error = GetLastError();
        return 0;
    }

    for (m = XT_PT_ATA; m < XT_PT_COUNT; m++) {
        DWORD r;

        memset(data, 0, sizeof data);
        switch (m) {
        case XT_PT_ATA: r = try_ata(h, data); break;
        case XT_PT_SAT: r = try_sat(h, data); break;
        default:        r = try_smart(h, disk_number, data); break;
        }
        if (r == 0 && xt_identify_parse(data, id) != XT_OK)
            r = XT_PT_E_BAD_DATA;

        rep->tried[m] = 1;
        rep->result[m] = r;
        if (r == 0) {
            rep->method = (xt_pt_method)m;
            break;
        }
    }

    CloseHandle(h);
    return rep->method != XT_PT_NONE;
}

const wchar_t *xt_pt_method_name(xt_pt_method m)
{
    switch (m) {
    case XT_PT_ATA:   return L"ATA pass-through";
    case XT_PT_SAT:   return L"SCSI pass-through (SAT)";
    case XT_PT_SMART: return L"SMART (identify only)";
    case XT_PT_NONE:
    case XT_PT_COUNT: break;
    }
    return L"none";
}
