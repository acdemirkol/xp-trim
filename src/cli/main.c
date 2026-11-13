/*
 * xptrim - command line front end.
 *
 * Commands so far are read-only. Every command that will touch a disk is
 * added in later milestones and defaults to dry-run.
 *
 * Exit codes:
 *   0  success (for `info`: the volume is supported)
 *   2  usage error
 *   3  volume not supported (reason printed)
 *   4  system error (a Win32 query failed; step and error code printed)
 */
#include <windows.h>
#include <stdio.h>
#include <wchar.h>

#include "core/volcheck.h"
#include "version.h"
#include "win32/volume.h"

enum {
    EXIT_OK          = 0,
    EXIT_USAGE       = 2,
    EXIT_UNSUPPORTED = 3,
    EXIT_SYSTEM      = 4
};

static void print_usage(FILE *out)
{
    fwprintf(out,
             L"Usage:\n"
             L"  xptrim info <drive>     Show whether a volume can be trimmed (read-only)\n"
             L"  xptrim --version\n"
             L"  xptrim --help\n"
             L"\n"
             L"<drive> is a drive letter such as C: .\n");
}

static const wchar_t *style_name(xt_partition_style s)
{
    switch (s) {
    case XT_PS_MBR: return L"MBR";
    case XT_PS_GPT: return L"GPT";
    case XT_PS_RAW: return L"RAW";
    case XT_PS_UNKNOWN: break;
    }
    return L"unknown";
}

static int cmd_info(const wchar_t *arg)
{
    wchar_t letter = xt_parse_drive_letter(arg);
    wchar_t fs[MAX_PATH + 1];
    xt_volume_facts f;
    xt_query_status st;
    xt_geometry g;
    xt_volume_verdict v;

    if (letter == 0) {
        fwprintf(stderr, L"xptrim: invalid drive '%ls'\n", arg);
        return EXIT_USAGE;
    }

    if (!xt_volume_query(letter, &f, fs, MAX_PATH + 1, &st)) {
        fwprintf(stderr, L"xptrim: %c: %hs failed, error %lu\n", letter, st.step,
                 (unsigned long)st.error);
        if (st.error == ERROR_ACCESS_DENIED)
            fwprintf(stderr, L"xptrim: run as an administrator\n");
        return EXIT_SYSTEM;
    }

    v = xt_volume_evaluate(&f, &g);

    wprintf(L"Volume %c:\n", letter);
    wprintf(L"  Drive type        %ls\n", f.is_fixed_drive ? L"fixed" : L"not fixed");
    if (f.is_fixed_drive)
        wprintf(L"  File system       %ls\n", fs[0] ? fs : L"(none)");
    if (f.fs_is_ntfs) {
        wprintf(L"  Disk extents      %lu\n", (unsigned long)f.extent_count);
        if (f.extent_count == 1)
            wprintf(L"  Physical disk     \\\\.\\PhysicalDrive%lu\n",
                    (unsigned long)f.disk_number);
        wprintf(L"  Partition style   %ls\n", style_name(f.partition_style));
        if (f.partition_info_ok) {
            wprintf(L"  Partition type    0x%02X\n", (unsigned)f.partition_type);
            wprintf(L"  Partition offset  %llu bytes\n",
                    (unsigned long long)f.partition_offset);
            wprintf(L"  Partition length  %llu bytes\n",
                    (unsigned long long)f.partition_length);
        }
        if (f.disk_bytes_per_sector)
            wprintf(L"  Sector size       %lu bytes\n", (unsigned long)f.disk_bytes_per_sector);
        wprintf(L"  Cluster size      %lu bytes\n", (unsigned long)f.ntfs_bytes_per_cluster);
        wprintf(L"  Clusters          %llu total, %llu free\n",
                (unsigned long long)f.ntfs_total_clusters,
                (unsigned long long)f.ntfs_free_clusters);
        if (v == XT_VOL_OK)
            wprintf(L"  First LBA         %llu\n",
                    (unsigned long long)(g.partition_offset_bytes / g.bytes_per_sector));
    }

    if (v == XT_VOL_OK) {
        wprintf(L"  Verdict           supported (TRIM support of the drive not checked yet)\n");
        return EXIT_OK;
    }
    wprintf(L"  Verdict           NOT SUPPORTED: %hs\n", xt_volume_verdict_str(v));
    return EXIT_UNSUPPORTED;
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
    if (argc == 3 && wcscmp(argv[1], L"info") == 0)
        return cmd_info(argv[2]);

    print_usage(stderr);
    return EXIT_USAGE;
}
