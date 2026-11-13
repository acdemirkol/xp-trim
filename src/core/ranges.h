/*
 * Operations on lists of LBA ranges.
 */
#ifndef XPTRIM_CORE_RANGES_H
#define XPTRIM_CORE_RANGES_H

#include <stddef.h>
#include <stdint.h>

#include "core/geometry.h"
#include "core/result.h"

/*
 * Sorts ranges by LBA and merges ranges that touch end-to-start, in place.
 * *n is updated to the new number of ranges.
 *
 * Ranges come from the extents of a single file, so they must never overlap;
 * an overlap means something upstream is wrong and fails with XT_E_OVERLAP.
 * A zero-length range fails with XT_E_INVALID_ARG. On failure the array
 * contents are unspecified (it may already be sorted) and *n is unchanged.
 */
xt_result xt_ranges_normalize(xt_lba_range *r, size_t *n);

/* Sum of all range lengths in sectors, or XT_E_OVERFLOW. */
xt_result xt_ranges_total(const xt_lba_range *r, size_t n, uint64_t *total);

#endif /* XPTRIM_CORE_RANGES_H */
