/*
 * xptrim - command line front end.
 *
 * Only --version and --help exist at this stage. Every command that will touch
 * a disk is added in later milestones and defaults to dry-run.
 */
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

#include "version.h"

enum {
    EXIT_OK    = 0,
    EXIT_USAGE = 2
};

static void print_usage(FILE *out)
{
    fwprintf(out,
             L"Usage: xptrim [--version | --help]\n"
             L"\n"
             L"SSD TRIM tool for Windows XP. No disk commands are implemented yet.\n");
}

int wmain(int argc, wchar_t **argv)
{
    if (argc == 2 && wcscmp(argv[1], L"--version") == 0) {
        wprintf(L"%ls %ls\n", XPTRIM_NAME, XPTRIM_VERSION);
        return EXIT_OK;
    }
    if (argc == 2 && (wcscmp(argv[1], L"--help") == 0 || wcscmp(argv[1], L"/?") == 0)) {
        print_usage(stdout);
        return EXIT_OK;
    }
    print_usage(stderr);
    return EXIT_USAGE;
}
