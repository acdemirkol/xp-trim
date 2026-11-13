#include <stddef.h>

#include "core/identify.h"
#include "core/volcheck.h"

xt_volume_verdict xt_volume_evaluate(const xt_volume_facts *f, xt_geometry *geom)
{
    xt_geometry g;

    if (f == NULL)
        return XT_VOL_BAD_GEOMETRY;

    if (!f->is_fixed_drive)
        return XT_VOL_NOT_FIXED;
    if (!f->fs_is_ntfs)
        return XT_VOL_NOT_NTFS;

    if (f->extent_count == 0)
        return XT_VOL_NO_EXTENT;
    if (f->extent_count > 1)
        return XT_VOL_MULTI_EXTENT;

    if (f->partition_style == XT_PS_GPT)
        return XT_VOL_GPT;
    if (f->partition_style != XT_PS_MBR)
        return XT_VOL_NOT_MBR;
    if (!f->partition_info_ok)
        return XT_VOL_NO_PARTITION_INFO;

    if (f->partition_type == XT_PARTITION_LDM)
        return XT_VOL_DYNAMIC;
    if (f->partition_type & XT_PARTITION_NTFT)
        return XT_VOL_FT_SET;

    /* The partition the volume reports must be the one disk extent it maps to. */
    if (f->partition_offset != f->extent_offset || f->partition_length != f->extent_length)
        return XT_VOL_EXTENT_MISMATCH;

    if (f->disk_bytes_per_sector == 0 ||
        f->disk_bytes_per_sector != f->ntfs_bytes_per_sector)
        return XT_VOL_SECTOR_MISMATCH;

    g.partition_offset_bytes = f->partition_offset;
    g.partition_length_bytes = f->partition_length;
    g.bytes_per_sector = f->disk_bytes_per_sector;
    g.bytes_per_cluster = f->ntfs_bytes_per_cluster;
    if (xt_geometry_validate(&g) != XT_OK)
        return XT_VOL_BAD_GEOMETRY;

    if (f->ntfs_total_clusters == 0 ||
        f->ntfs_total_clusters > g.partition_length_bytes / g.bytes_per_cluster)
        return XT_VOL_FS_EXCEEDS_PARTITION;

    if (geom != NULL)
        *geom = g;
    return XT_VOL_OK;
}

xt_volume_verdict xt_drive_evaluate(const xt_identify *id, const xt_geometry *g)
{
    uint64_t end_lba;

    if (id == NULL)
        return XT_VOL_NO_IDENTIFY;
    if (g == NULL || xt_geometry_validate(g) != XT_OK)
        return XT_VOL_BAD_GEOMETRY;

    if (!id->trim_supported)
        return XT_VOL_NO_TRIM;
    if (!id->lba48)
        return XT_VOL_NO_LBA48;
    if (id->logical_sector_size != g->bytes_per_sector)
        return XT_VOL_DRIVE_SECTOR_MISMATCH;

    /* No overflow: xt_geometry_validate() checked offset + length. */
    end_lba = (g->partition_offset_bytes + g->partition_length_bytes) / g->bytes_per_sector;
    if (end_lba > id->user_sectors)
        return XT_VOL_PARTITION_BEYOND_DRIVE;
    return XT_VOL_OK;
}

const char *xt_volume_verdict_str(xt_volume_verdict v)
{
    switch (v) {
    case XT_VOL_OK:                   return "supported";
    case XT_VOL_NOT_FIXED:            return "not a fixed disk";
    case XT_VOL_NOT_NTFS:             return "file system is not NTFS";
    case XT_VOL_MULTI_EXTENT:         return "volume spans several disk extents (spanned/striped/RAID)";
    case XT_VOL_NO_EXTENT:            return "volume reports no disk extent";
    case XT_VOL_GPT:                  return "GPT disk";
    case XT_VOL_NOT_MBR:              return "partition style is not MBR";
    case XT_VOL_NO_PARTITION_INFO:    return "partition information unavailable";
    case XT_VOL_DYNAMIC:              return "dynamic disk (LDM)";
    case XT_VOL_FT_SET:               return "partition belongs to a fault-tolerant set (RAID)";
    case XT_VOL_EXTENT_MISMATCH:      return "partition and disk extent disagree";
    case XT_VOL_SECTOR_MISMATCH:      return "disk and file system sector sizes disagree";
    case XT_VOL_BAD_GEOMETRY:         return "inconsistent geometry";
    case XT_VOL_FS_EXCEEDS_PARTITION: return "file system larger than its partition";
    case XT_VOL_NO_IDENTIFY:          return "drive identity could not be read (USB bridge or unsupported driver?)";
    case XT_VOL_NO_TRIM:              return "drive does not report TRIM support";
    case XT_VOL_NO_LBA48:             return "drive does not support 48-bit addressing";
    case XT_VOL_DRIVE_SECTOR_MISMATCH: return "drive and disk sector sizes disagree";
    case XT_VOL_PARTITION_BEYOND_DRIVE: return "partition ends beyond the drive's capacity";
    }
    return "unknown";
}
