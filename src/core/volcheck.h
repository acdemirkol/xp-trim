/*
 * Decides whether a volume may be trimmed, from facts gathered by the Win32
 * layer. Pure logic so every refusal rule is unit tested.
 */
#ifndef XPTRIM_CORE_VOLCHECK_H
#define XPTRIM_CORE_VOLCHECK_H

#include <stdint.h>

#include "core/geometry.h"

typedef enum {
    XT_PS_UNKNOWN = 0, /* IOCTL_DISK_GET_PARTITION_INFO_EX failed */
    XT_PS_MBR,
    XT_PS_GPT,
    XT_PS_RAW
} xt_partition_style;

/* MBR partition type of a dynamic (LDM) disk. */
#define XT_PARTITION_LDM 0x42u
/* MBR type flag of a partition in an NT fault-tolerant set (mirror/stripe). */
#define XT_PARTITION_NTFT 0x80u

typedef struct {
    int is_fixed_drive;              /* GetDriveType() == DRIVE_FIXED */
    int fs_is_ntfs;                  /* file system name is exactly "NTFS" */

    uint32_t extent_count;           /* IOCTL_VOLUME_GET_VOLUME_DISK_EXTENTS */
    uint32_t disk_number;
    uint64_t extent_offset;
    uint64_t extent_length;

    xt_partition_style partition_style;
    int partition_info_ok;           /* IOCTL_DISK_GET_PARTITION_INFO succeeded */
    uint8_t partition_type;
    uint64_t partition_offset;
    uint64_t partition_length;

    uint32_t disk_bytes_per_sector;  /* IOCTL_DISK_GET_DRIVE_GEOMETRY */
    uint32_t ntfs_bytes_per_sector;  /* FSCTL_GET_NTFS_VOLUME_DATA */
    uint32_t ntfs_bytes_per_cluster;
    uint64_t ntfs_total_clusters;
    uint64_t ntfs_free_clusters;
} xt_volume_facts;

typedef enum {
    XT_VOL_OK = 0,
    XT_VOL_NOT_FIXED,
    XT_VOL_NOT_NTFS,
    XT_VOL_MULTI_EXTENT,
    XT_VOL_NO_EXTENT,
    XT_VOL_GPT,
    XT_VOL_NOT_MBR,
    XT_VOL_NO_PARTITION_INFO,
    XT_VOL_DYNAMIC,
    XT_VOL_FT_SET,
    XT_VOL_EXTENT_MISMATCH,
    XT_VOL_SECTOR_MISMATCH,
    XT_VOL_BAD_GEOMETRY,
    XT_VOL_FS_EXCEEDS_PARTITION
} xt_volume_verdict;

/*
 * Applies the refusal rules in a fixed order and returns the first that
 * matches. On XT_VOL_OK, *geom (if not NULL) receives the validated geometry,
 * built only from values reported by the system.
 */
xt_volume_verdict xt_volume_evaluate(const xt_volume_facts *f, xt_geometry *geom);

/* Short English reason; never NULL. */
const char *xt_volume_verdict_str(xt_volume_verdict v);

#endif /* XPTRIM_CORE_VOLCHECK_H */
