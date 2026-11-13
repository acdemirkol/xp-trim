# Changelog

Bu projedeki tüm önemli değişiklikler bu dosyada belgelenir.

Biçim [Keep a Changelog](https://keepachangelog.com/tr-TR/1.1.0/) esas alınır,
sürümleme [Semantic Versioning](https://semver.org/lang/tr/) kurallarına uyar.

## [Unreleased]

### Added
- Build system (GNU Make, MSYS2 MINGW32) producing XP-compatible 32-bit binaries.
- `xptrim --version` and `xptrim --help`.
- `xptrim info <drive>`: read-only report of a volume's partition, sector and
  cluster geometry, and whether it is supported. Refuses non-fixed drives,
  non-NTFS file systems, spanned/striped volumes, GPT and dynamic disks,
  fault-tolerant (RAID) partitions and inconsistent geometry.
- `xptrim info` reads the drive's ATA IDENTIFY data (via ATA pass-through,
  SCSI/SAT pass-through or SMART, whichever works) and reports model,
  firmware, capacity, sector size, TRIM support, deterministic TRIM and
  blocks per DATA SET MANAGEMENT command. Drives without TRIM or 48-bit
  addressing, with a different sector size, or smaller than the partition
  are refused.
- `make check`: fails the build if a binary imports a function not available on
  Windows XP SP0 or its PE header does not target Windows 5.01.
- Continuous integration on GitHub Actions.
- MIT license and English/Turkish README.
