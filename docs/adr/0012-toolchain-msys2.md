# ADR-0012: Toolchain is MSYS2 UCRT64 (GCC) with prebuilt packages (supersedes ADR-0005)

**Status:** Accepted · **Date:** 2026-06-24

## Context

[ADR-0005](./0005-build-and-dependencies.md) chose CMake + vcpkg. The Phase-1 spike showed vcpkg's
from-source dependency builds (OpenSSL, libpq, Drogon) are too heavy for the 8 GB dev machine —
they ran out of memory and took ~1 hour. We still want CMake; we need a lighter dependency story.

## Options considered

- **CMake + vcpkg (MSVC)** — reproducible and industry-standard, but compiles big dependencies from
  source; impractical on 8 GB RAM.
- **CMake + Conan** — same from-source concern.
- **MSYS2 UCRT64 + pacman** — GCC 15 toolchain with CMake + Ninja, and **prebuilt** packages for
  `asio`, `nlohmann-json`, `libpqxx`/`libpq`, and `gtest`. Nothing large compiles from source.

## Decision

Build with the **MSYS2 UCRT64** toolchain (GCC, CMake, Ninja). Dependencies come prebuilt via
`pacman`. Crow is vendored as a single header. Configure/build via the `default` CMake preset
(`Server/CMakePresets.json`, Ninja generator).

Required packages:

```
pacman -S --needed mingw-w64-ucrt-x86_64-asio mingw-w64-ucrt-x86_64-nlohmann-json \
                   mingw-w64-ucrt-x86_64-libpqxx mingw-w64-ucrt-x86_64-gtest
```

## Consequences

- **Positive:** near-instant dependency setup; low memory; fast clean builds (~30 s); fits the
  hardware. GCC 15 with good C++20 support.
- **Negative:** MinGW/GCC is less common than MSVC in Windows shops (a learning trade-off); system
  packages are less hermetic than a vcpkg manifest, so versions are pinned via documentation, and
  CI must install the same MSYS2 packages.
- **Operational note:** use `pacman -Syu` (full upgrade), never a bare `-Sy` (partial DB refresh),
  to avoid DLL/version skew — this skew is what broke the native `initdb` during the spike.
- **Postgres:** [ADR-0004](./0004-database-postgresql.md) (Postgres via Docker Compose) still holds;
  the host port is configurable (`POSTGRES_HOST_PORT`, default 5432) to avoid clashing with a
  pre-existing local Postgres.
