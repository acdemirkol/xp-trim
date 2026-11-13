#include "core/ranges.h"
#include "test.h"

static void test_merge_and_sort(void)
{
    xt_lba_range r[] = {
        { 300, 50 },  /* separate */
        { 100, 10 },  /* 100..109 */
        { 110, 20 },  /* adjacent to previous -> 100..129 */
        { 130, 5 },   /* adjacent again -> 100..134 */
        { 200, 1 },   /* separate */
    };
    size_t n = 5;
    uint64_t total;

    CHECK_RESULT(xt_ranges_normalize(r, &n), XT_OK);
    CHECK_EQ_U64(n, 3);
    CHECK_EQ_U64(r[0].lba, 100); CHECK_EQ_U64(r[0].count, 35);
    CHECK_EQ_U64(r[1].lba, 200); CHECK_EQ_U64(r[1].count, 1);
    CHECK_EQ_U64(r[2].lba, 300); CHECK_EQ_U64(r[2].count, 50);

    CHECK_RESULT(xt_ranges_total(r, n, &total), XT_OK);
    CHECK_EQ_U64(total, 86);
}

static void test_gap_of_one_not_merged(void)
{
    xt_lba_range r[] = { { 0, 10 }, { 11, 10 } };
    size_t n = 2;
    CHECK_RESULT(xt_ranges_normalize(r, &n), XT_OK);
    CHECK_EQ_U64(n, 2);
}

static void test_overlap_rejected(void)
{
    xt_lba_range r[] = { { 100, 10 }, { 109, 5 } };
    size_t n = 2;
    CHECK_RESULT(xt_ranges_normalize(r, &n), XT_E_OVERLAP);
    CHECK_EQ_U64(n, 2);

    {
        xt_lba_range d[] = { { 5, 1 }, { 5, 1 } };
        size_t m = 2;
        CHECK_RESULT(xt_ranges_normalize(d, &m), XT_E_OVERLAP);
    }
}

static void test_invalid(void)
{
    xt_lba_range r[] = { { 1, 0 } };
    xt_lba_range big[] = { { UINT64_MAX, 2 } };
    size_t n = 1;
    uint64_t total;

    CHECK_RESULT(xt_ranges_normalize(r, &n), XT_E_INVALID_ARG);
    n = 1;
    CHECK_RESULT(xt_ranges_normalize(big, &n), XT_E_OVERFLOW);
    CHECK_RESULT(xt_ranges_normalize(r, NULL), XT_E_INVALID_ARG);
    n = 1;
    CHECK_RESULT(xt_ranges_normalize(NULL, &n), XT_E_INVALID_ARG);

    n = 0;
    CHECK_RESULT(xt_ranges_normalize(NULL, &n), XT_OK);
    CHECK_EQ_U64(n, 0);
    CHECK_RESULT(xt_ranges_total(NULL, 0, &total), XT_OK);
    CHECK_EQ_U64(total, 0);

    {
        xt_lba_range t[] = { { 0, UINT64_MAX }, { 0, 1 } };
        CHECK_RESULT(xt_ranges_total(t, 2, &total), XT_E_OVERFLOW);
    }
}

void suite_ranges(void)
{
    test_merge_and_sort();
    test_gap_of_one_not_merged();
    test_overlap_rejected();
    test_invalid();
}
