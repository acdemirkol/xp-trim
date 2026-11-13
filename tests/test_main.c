#include <wchar.h>

#include "test.h"
#include "version.h"

int test_failures = 0;
int test_checks = 0;

/* Sanity check of the harness itself, replaced by real suites in feat/core. */
static void test_version_string(void)
{
    CHECK(wcslen(XPTRIM_VERSION) > 0);
    CHECK_EQ_U64(sizeof(void *), 4); /* must be a 32-bit build */
}

int main(void)
{
    test_version_string();

    printf("%d checks, %d failures\n", test_checks, test_failures);
    return test_failures == 0 ? 0 : 1;
}
