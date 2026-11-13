/*
 * Partition geometry and the single place where cluster numbers become LBAs.
 *
 * Every value in xt_geometry must come from the system at run time
 * (IOCTL_DISK_GET_PARTITION_INFO, IOCTL_DISK_GET_DRIVE_GEOMETRY,
 * FSCTL_GET_NTFS_VOLUME_DATA). Nothing here assumes a default.
 */
#ifndef XPTRIM_CORE_GEOMETRY_H
#define XPTRIM_CORE_GEOMETRY_H

#include <stdint.h>

#include "core/result.h"

/* First LBA that a 48-bit ATA command cannot address. */
#define XT_LBA48_LIMIT (UINT64_C(1) << 48)

typedef struct {
    uint64_t partition_offset_bytes; /* PARTITION_INFORMATION.StartingOffset */
    uint64_t partition_length_bytes; /* PARTITION_INFORMATION.PartitionLength */
    uint32_t bytes_per_sector;       /* logical sector size of the disk */
    uint32_t bytes_per_cluster;      /* NTFS cluster size of the volume */
} xt_geometry;

/* A run of logical sectors on the physical disk. */
typedef struct {
    uint64_t lba;
    uint64_t count;
} xt_lba_range;

/*
 * Checks that the geometry is self-consistent: sector size is a power of two
 * >= 512, cluster size is a non-zero multiple of it, the partition is
 * non-empty and sector aligned, and offset + length does not overflow.
 */
xt_result xt_geometry_validate(const xt_geometry *g);

/*
 * Converts a volume extent (starting LCN, length in clusters) into a disk LBA
 * range:
 *
 *   lba   = partition_offset / sector + lcn * (cluster / sector)
 *   count = clusters * (cluster / sector)
 *
 * Fails with XT_E_OUT_OF_RANGE if any part of the extent lies outside the
 * partition or beyond the 48-bit LBA limit. *out is only written on XT_OK.
 */
xt_result xt_extent_to_lba(const xt_geometry *g, uint64_t lcn,
                           uint64_t clusters, xt_lba_range *out);

/*
 * Re-checks an LBA range against the partition immediately before it is
 * sent: count > 0, fully inside the partition, below the 48-bit limit.
 */
xt_result xt_range_check(const xt_geometry *g, const xt_lba_range *r);

#endif /* XPTRIM_CORE_GEOMETRY_H */
