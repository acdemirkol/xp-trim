#include <stdlib.h>

#include "core/ranges.h"

static int cmp_lba(const void *a, const void *b)
{
    uint64_t x = ((const xt_lba_range *)a)->lba;
    uint64_t y = ((const xt_lba_range *)b)->lba;
    return (x > y) - (x < y);
}

xt_result xt_ranges_normalize(xt_lba_range *r, size_t *n)
{
    size_t i, w;

    if (n == NULL || (*n > 0 && r == NULL))
        return XT_E_INVALID_ARG;

    for (i = 0; i < *n; i++) {
        if (r[i].count == 0)
            return XT_E_INVALID_ARG;
        if (r[i].lba > UINT64_MAX - r[i].count)
            return XT_E_OVERFLOW;
    }
    if (*n < 2)
        return XT_OK;

    qsort(r, *n, sizeof r[0], cmp_lba);

    w = 0;
    for (i = 1; i < *n; i++) {
        uint64_t prev_end = r[w].lba + r[w].count;
        if (r[i].lba < prev_end)
            return XT_E_OVERLAP;
        if (r[i].lba == prev_end) {
            /* No overflow: r[i].lba + r[i].count was checked above. */
            r[w].count += r[i].count;
        } else {
            r[++w] = r[i];
        }
    }
    *n = w + 1;
    return XT_OK;
}

xt_result xt_ranges_total(const xt_lba_range *r, size_t n, uint64_t *total)
{
    size_t i;
    uint64_t sum = 0;

    if (total == NULL || (n > 0 && r == NULL))
        return XT_E_INVALID_ARG;
    for (i = 0; i < n; i++) {
        if (r[i].count > UINT64_MAX - sum)
            return XT_E_OVERFLOW;
        sum += r[i].count;
    }
    *total = sum;
    return XT_OK;
}
