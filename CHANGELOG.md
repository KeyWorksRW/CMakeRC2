# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

This fork of [vector-of-bool/cmrc](https://github.com/vector-of-bool/cmrc) (CMakeRC) began diverging from the original repository on September 6, 2026.

## [3.0.0] - 2026-09-06

### Added

- **C23 `#embed` support and `string_view` path APIs** (2026-09-06, [#32])
  - `CMakeRC.cmake` now emits a `#embed` directive in generated resource files when the compiler supports it (guarded by `__has_embed`), falling back to the hex-literal array otherwise. Empty files keep the existing zero-byte array behavior.
  - `cmrc.hpp` gains `file::view()`, a `string_view`-based `path_param` type, and heterogeneous (`std::less<>`) map lookups under C++17, avoiding string copies when opening or looking up embedded paths.

- **Base64 fallback encoding for resource embedding** (2026-09-07, [#33])
  - New `CMRC_BASE64` option encodes resources as base64 (~1.33x expansion) instead of the `\xNN` character-literal fallback (~6x expansion), which makes large resources slow to compile. `#embed` still takes precedence when available.
  - Encoded strings are split into chunked literals to stay under MSVC per-literal and post-concatenation caps (C2026) on older toolsets.
  - New `CMRC_DISABLE_EMBED` diagnostic option forces the fallback path on `#embed`-capable compilers for testing the generators.
  - Documented the `#embed` / literal / base64 tradeoffs in the README.

- **`CODEGEN` keyword for `add_custom_command()` calls** (2026-09-06, [#31])
  - On CMake 3.31+ (policy CMP0171 set to NEW), consumers can build the builtin `codegen` target to generate resources without compiling them — useful for linting CI jobs like clang-tidy. (Idea credited to @craigscott-crascit.)

- **Project infrastructure** (2026-09-06)
  - Added a GitHub Actions workflow to check every pull request.
  - Added issue templates.
  - Documented how to use CMakeRC with a shared library ([#14]).
  - Updated LICENSE and README to reflect this fork.

### Fixed

- **Ninja CMP0058 warning** (2026-09-06, [#29], closes [#17])
  - Replaced configure-time `file(GENERATE)` of `cmrc.hpp` with a custom command and `cmrc-base-hdr` target, so the header is a declared build product. Resource libraries now depend on it. Added a `header_regen` regression test covering rebuild after deleting the generated header.

- **Iterator and code-review bugs in `cmrc.hpp`** (2026-09-06, [#30])
  - Fixed pre-increment and post-increment iterator operators, including incorrect return values.
  - Fixed `directory::end()` returning a singular iterator, which caused infinite loops when iterating directories directly on libc++/MSVC.
  - Added missing `<cstdio>` / `<cstdlib>` includes for `CMRC_NO_EXCEPTIONS` builds, and removed dead `<functional>` / `<mutex>` includes.
  - Guarded duplicate-name insertion in `add_file` / `add_subdir`; documented the path contract (forward slashes, no dot segments). Regression tests added for all fixes.

- **CMake minimum version deprecation warnings** (2026-09-06, [#28])
  - Set `cmake_minimum_required(VERSION 3.12...4.0)` — the range form works on CMake 3.12+ and prevents deprecation warnings on CMake 4.x.

### Changed

- Set the fork's version to 3.0.0 so it cannot collide with the original repository's version numbering (2026-09-06).

## [Unreleased]

### Fixed

- CMakeRC module directory is now cached as a `CACHE INTERNAL` variable, so API functions called from the parent project still resolve helper sources when CMakeRC is included via FetchContent (2026-09-06).

[3.0.0]: https://github.com/KeyWorksRW/CMakeRC2/commit/434c30a10469abfb0bc732b3d134219f1cf879f8
[Unreleased]: https://github.com/KeyWorksRW/CMakeRC2/commits/main

<!-- PR links -->
[#14]: https://github.com/KeyWorksRW/CMakeRC2/issues/14
[#17]: https://github.com/KeyWorksRW/CMakeRC2/issues/17
[#28]: https://github.com/KeyWorksRW/CMakeRC2/pull/28
[#29]: https://github.com/KeyWorksRW/CMakeRC2/pull/29
[#30]: https://github.com/KeyWorksRW/CMakeRC2/pull/30
[#31]: https://github.com/KeyWorksRW/CMakeRC2/pull/31
[#32]: https://github.com/KeyWorksRW/CMakeRC2/pull/32
[#33]: https://github.com/KeyWorksRW/CMakeRC2/pull/33
