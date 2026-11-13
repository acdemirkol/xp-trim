#include <windows.h>
#include <winioctl.h>
#include <string.h>
#include <wchar.h>

#include "win32/volume.h"

wchar_t xt_parse_drive_letter(const wchar_t *arg)
{
    wchar_t c;

    if (arg == NULL || arg[0] == L'\0')
        return 0;
    c = arg[0];
    if (c >= L'a' && c <= L'z')
        c = (wchar_t)(c - L'a' + L'A');
    if (c < L'A' || c > L'Z')
        return 0;
    if (arg[1] == L'\0')
        return c;
    if (arg[1] != L':')
        return 0;
    if (arg[2] == L'\0' || (arg[2] == L'\\' && arg[3] == L'\0'))
        return c;
    return 0;
}

static int fail(xt_query_status *st, const char *step)
{
    st->error = GetLastError();
    st->step = step;
    return 0;
}

/* Builds L"\\\\.\\PhysicalDrive<n>" without pulling in a printf variant. */
static void physical_drive_path(DWORD n, wchar_t *out, size_t out_len)
{
    static const wchar_t prefix[] = L"\\\\.\\PhysicalDrive";
    wchar_t digits[11];
    size_t nd = 0, i, len = wcslen(prefix);

    do {
        digits[nd++] = (wchar_t)(L'0' + n % 10);
        n /= 10;
    } while (n != 0 && nd < 10);

    if (len + nd + 1 > out_len) {
        out[0] = L'\0';
        return;
    }
    memcpy(out, prefix, len * sizeof(wchar_t));
    for (i = 0; i < nd; i++)
        out[len + i] = digits[nd - 1 - i];
    out[len + nd] = L'\0';
}

static int query_disk(HANDLE vol, xt_volume_facts *f, xt_query_status *st)
{
    VOLUME_DISK_EXTENTS ext;
    PARTITION_INFORMATION_EX pex;
    PARTITION_INFORMATION pi;
    NTFS_VOLUME_DATA_BUFFER nv;
    DISK_GEOMETRY dg;
    DWORD got;
    HANDLE disk;
    wchar_t path[40];

    /* Disk extents: exactly one is accepted later; ERROR_MORE_DATA means >1. */
    memset(&ext, 0, sizeof ext);
    if (DeviceIoControl(vol, IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS, NULL, 0,
                        &ext, sizeof ext, &got, NULL)) {
        f->extent_count = ext.NumberOfDiskExtents;
        if (ext.NumberOfDiskExtents >= 1) {
            f->disk_number = ext.Extents[0].DiskNumber;
            f->extent_offset = (uint64_t)ext.Extents[0].StartingOffset.QuadPart;
            f->extent_length = (uint64_t)ext.Extents[0].ExtentLength.QuadPart;
        }
    } else if (GetLastError() == ERROR_MORE_DATA) {
        f->extent_count = ext.NumberOfDiskExtents > 1 ? ext.NumberOfDiskExtents : 2;
        return 1; /* refused anyway; do not guess which disk to look at */
    } else {
        return fail(st, "IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS");
    }

    /* Partition style; failure leaves XT_PS_UNKNOWN and the volume is refused. */
    if (DeviceIoControl(vol, IOCTL_DISK_GET_PARTITION_INFO_EX, NULL, 0,
                        &pex, sizeof pex, &got, NULL)) {
        switch (pex.PartitionStyle) {
        case PARTITION_STYLE_MBR: f->partition_style = XT_PS_MBR; break;
        case PARTITION_STYLE_GPT: f->partition_style = XT_PS_GPT; break;
        case PARTITION_STYLE_RAW: f->partition_style = XT_PS_RAW; break;
        default:                  f->partition_style = XT_PS_UNKNOWN; break;
        }
    }

    /* The offset used for LBAs comes from here and nowhere else. */
    if (DeviceIoControl(vol, IOCTL_DISK_GET_PARTITION_INFO, NULL, 0,
                        &pi, sizeof pi, &got, NULL)) {
        f->partition_info_ok = 1;
        f->partition_type = pi.PartitionType;
        f->partition_offset = (uint64_t)pi.StartingOffset.QuadPart;
        f->partition_length = (uint64_t)pi.PartitionLength.QuadPart;
    }

    if (!DeviceIoControl(vol, FSCTL_GET_NTFS_VOLUME_DATA, NULL, 0,
                         &nv, sizeof nv, &got, NULL))
        return fail(st, "FSCTL_GET_NTFS_VOLUME_DATA");
    f->ntfs_bytes_per_sector = nv.BytesPerSector;
    f->ntfs_bytes_per_cluster = nv.BytesPerCluster;
    f->ntfs_total_clusters = (uint64_t)nv.TotalClusters.QuadPart;
    f->ntfs_free_clusters = (uint64_t)nv.FreeClusters.QuadPart;

    if (f->extent_count != 1)
        return 1;

    /* Physical disk opened with no access rights: query only. */
    physical_drive_path(f->disk_number, path, sizeof path / sizeof path[0]);
    disk = CreateFileW(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                       OPEN_EXISTING, 0, NULL);
    if (disk == INVALID_HANDLE_VALUE)
        return fail(st, "open physical disk");
    if (!DeviceIoControl(disk, IOCTL_DISK_GET_DRIVE_GEOMETRY, NULL, 0,
                         &dg, sizeof dg, &got, NULL)) {
        fail(st, "IOCTL_DISK_GET_DRIVE_GEOMETRY");
        CloseHandle(disk);
        return 0;
    }
    CloseHandle(disk);
    f->disk_bytes_per_sector = dg.BytesPerSector;
    return 1;
}

int xt_volume_query(wchar_t letter, xt_volume_facts *f, wchar_t *fs_name,
                    DWORD fs_name_len, xt_query_status *st)
{
    wchar_t root[] = L"?:\\";
    wchar_t dev[] = L"\\\\.\\?:";
    HANDLE vol;
    int ok;

    memset(f, 0, sizeof *f);
    st->error = 0;
    st->step = NULL;
    if (fs_name_len > 0)
        fs_name[0] = L'\0';

    if (letter < L'A' || letter > L'Z') {
        SetLastError(ERROR_INVALID_PARAMETER);
        return fail(st, "drive letter");
    }
    root[0] = letter;
    dev[4] = letter;

    f->is_fixed_drive = GetDriveTypeW(root) == DRIVE_FIXED;
    if (!f->is_fixed_drive)
        return 1;

    if (!GetVolumeInformationW(root, NULL, 0, NULL, NULL, NULL, fs_name, fs_name_len))
        return fail(st, "GetVolumeInformation");
    f->fs_is_ntfs = wcscmp(fs_name, L"NTFS") == 0;
    if (!f->fs_is_ntfs)
        return 1;

    vol = CreateFileW(dev, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                      OPEN_EXISTING, 0, NULL);
    if (vol == INVALID_HANDLE_VALUE)
        return fail(st, "open volume");
    ok = query_disk(vol, f, st);
    CloseHandle(vol);
    return ok;
}
