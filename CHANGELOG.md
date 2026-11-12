# Changelog

Bu projedeki tüm önemli değişiklikler bu dosyada belgelenir.

Biçim [Keep a Changelog](https://keepachangelog.com/tr-TR/1.1.0/) esas alınır,
sürümleme [Semantic Versioning](https://semver.org/lang/tr/) kurallarına uyar.

## [Unreleased]

### Added
- Build system (GNU Make, MSYS2 MINGW32) producing XP-compatible 32-bit binaries.
- `xptrim --version` and `xptrim --help`.
- `make check`: fails the build if a binary imports a function not available on
  Windows XP SP0 or its PE header does not target Windows 5.01.
- Continuous integration on GitHub Actions.
- MIT license and English/Turkish README.
