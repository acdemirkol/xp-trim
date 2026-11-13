/*
 * Read-only queries that gather xt_volume_facts for a drive letter.
 *
 * Nothing here writes: the volume is opened with GENERIC_READ (needed by
 * IOCTL_DISK_GET_PARTITION_INFO and FSCTL_GET_NTFS_VOLUME_DATA) and the
 * physical disk with no access rights at all (IOCTL_DISK_GET_DRIVE_GEOMETRY
 * is FILE_ANY_ACCESS).
 */
#ifndef XPTRIM_WIN32_VOLUME_H
#define XPTRIM_WIN32_VOLUME_H

#include <windows.h>

#include "core/volcheck.h"

typedef struct {
    DWORD error;          /* Win32 error of the failed step, 0 on success */
    const char *step;     /* name of the failed step, NULL on success */
} xt_query_status;

/* Accepts "C", "C:" or "C:\" (any case); returns the upper-case letter or 0. */
wchar_t xt_parse_drive_letter(const wchar_t *arg);

/*
 * Fills *f for drive `letter` (A-Z). Returns 1 on success. Returns 0 if a
 * query that every supported volume must answer failed (e.g. access denied);
 * *st then names the step. Queries that legitimately fail on unsupported
 * volumes (partition info on GPT) are recorded in *f instead.
 *
 * If the drive is not fixed or not NTFS, the device is not opened at all
 * and the remaining fields stay zero.
 */
int xt_volume_query(wchar_t letter, xt_volume_facts *f, wchar_t *fs_name,
                    DWORD fs_name_len, xt_query_status *st);

#endif /* XPTRIM_WIN32_VOLUME_H */
