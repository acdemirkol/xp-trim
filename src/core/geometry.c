#include "core/geometry.h"

static int is_pow2(uint32_t v)
{
    return v != 0 && (v & (v - 1)) == 0;
}

xt_result xt_geometry_validate(const xt_geometry *g)
{
    uint32_t bps;

    if (g == NULL)
        return XT_E_INVALID_ARG;

    bps = g->bytes_per_sector;
    if (bps < 512 || !is_pow2(bps))
        return XT_E_INVALID_ARG;
    if (g->bytes_per_cluster < bps || g->bytes_per_cluster % bps != 0)
        return XT_E_INVALID_ARG;
    if (g->partition_length_bytes == 0)
        return XT_E_INVALID_ARG;
    if (g->partition_offset_bytes % bps != 0 || g->partition_length_bytes % bps != 0)
        return XT_E_INVALID_ARG;
    if (g->partition_offset_bytes > UINT64_MAX - g->partition_length_bytes)
        return XT_E_OVERFLOW;
    return XT_OK;
}

xt_result xt_extent_to_lba(const xt_geometry *g, uint64_t lcn,
                           uint64_t clusters, xt_lba_range *out)
{
    xt_result r;
    uint64_t bpc, spc, start_bytes, len_bytes, lba, count;

    r = xt_geometry_validate(g);
    if (r != XT_OK)
        return r;
    if (out == NULL || clusters == 0)
        return XT_E_INVALID_ARG;

    bpc = g->bytes_per_cluster;
    spc = bpc / g->bytes_per_sector;

    if (lcn > UINT64_MAX / bpc || clusters > UINT64_MAX / bpc)
        return XT_E_OVERFLOW;
    start_bytes = lcn * bpc;
    len_bytes = clusters * bpc;
    if (start_bytes > UINT64_MAX - len_bytes)
        return XT_E_OVERFLOW;
    if (start_bytes + len_bytes > g->partition_length_bytes)
        return XT_E_OUT_OF_RANGE;

    /* Cannot overflow: bounded by (offset + length) / sector, checked above. */
    lba = g->partition_offset_bytes / g->bytes_per_sector + lcn * spc;
    count = clusters * spc;

    if (lba >= XT_LBA48_LIMIT || count > XT_LBA48_LIMIT - lba)
        return XT_E_OUT_OF_RANGE;

    out->lba = lba;
    out->count = count;
    return XT_OK;
}

xt_result xt_range_check(const xt_geometry *g, const xt_lba_range *r)
{
    xt_result res;
    uint64_t first, end;

    res = xt_geometry_validate(g);
    if (res != XT_OK)
        return res;
    if (r == NULL || r->count == 0)
        return XT_E_INVALID_ARG;

    first = g->partition_offset_bytes / g->bytes_per_sector;
    end = (g->partition_offset_bytes + g->partition_length_bytes) / g->bytes_per_sector;

    if (r->lba < first || r->lba >= end || r->count > end - r->lba)
        return XT_E_OUT_OF_RANGE;
    if (r->lba >= XT_LBA48_LIMIT || r->count > XT_LBA48_LIMIT - r->lba)
        return XT_E_OUT_OF_RANGE;
    return XT_OK;
}
