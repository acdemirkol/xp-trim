# xp-trim

[Türkçe](README.tr.md)

SSD TRIM for Windows XP (SP0–SP3, 32-bit).

> **Status: early development. Nothing in this repository sends TRIM yet.**
> Do not use it on a disk whose data you care about.

Windows XP predates TRIM, so SSDs used under XP never learn which blocks are free
and slowly lose write performance. xp-trim fills that gap:

| Component | File | Purpose |
|---|---|---|
| Command line tool | `xptrim.exe` | Inspect volumes and TRIM free space (dry-run by default) |
| Service | `xptrimsvc.exe` | Runs TRIM on a schedule |
| Control Panel applet | `xptrim.cpl` | View status, change settings, run now |
| Notification area icon (optional) | `xptrimtray.exe` | Status next to the clock; double-click opens the applet |

## Safety design

- **Dry-run is the default.** Real TRIM commands are only sent with `--apply`
  (or `apply=1` in the service configuration, set by an administrator).
- Free space is first reserved by a temporary placeholder file; only that file's
  clusters are trimmed, so the file system cannot reuse them mid-operation.
- The partition offset and sector size are always read from the system, never assumed.
- Dynamic disks, RAID/spanned volumes, GPT disks and non-NTFS volumes are refused.
- TRIM is only sent when the drive reports TRIM support (ATA IDENTIFY word 169).

## Usage

Run from an administrator command prompt.

```
xptrim info C:      Show whether volume C: can be trimmed (read-only)
xptrim --version
```

| Exit code | Meaning |
|---|---|
| 0 | Success (`info`: the volume is supported) |
| 2 | Usage error |
| 3 | Volume not supported; the reason is printed |
| 4 | A system query failed; the step and Windows error code are printed |

## Building

Requires [MSYS2](https://www.msys2.org/) with the MINGW32 environment:

```sh
pacman -S --needed mingw-w64-i686-gcc mingw-w64-i686-binutils make
make          # build/xptrim.exe
make test     # unit tests
make check    # verify the binaries only import functions available on XP SP0
```

From cmd or PowerShell, `tools\msys-run.cmd make test` runs the same inside MSYS2.

Binaries are linked statically against XP's `msvcrt.dll`, have PE OS/subsystem
version 5.01 and need no runtime installation.

## License

[MIT](LICENSE)
