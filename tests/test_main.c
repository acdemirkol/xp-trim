#include <wchar.h>

#include "test.h"
#include "version.h"

int test_failures = 0;
int test_checks = 0;

static void suite_build(void)
{
    CHECK(wcslen(XPTRIM_VERSION) > 0);
    CHECK_EQ_U64(sizeof(void *), 4); /* must be a 32-bit build */
}

static void run(const char *name, void (*suite)(void))
{
    int before = test_failures;
    suite();
    printf("  %-10s %s\n", name, test_failures == before ? "ok" : "FAILED");
}

int main(void)
{
    run("build", suite_build);
    run("geometry", suite_geometry);
    run("ranges", suite_ranges);
    run("dsm", suite_dsm);
    run("identify", suite_identify);
    run("volcheck", suite_volcheck);

    printf("%d checks, %d failures\n", test_checks, test_failures);
    return test_failures == 0 ? 0 : 1;
}
