# ADR-0005: Build system is CMake, dependencies via vcpkg

**Status:** Accepted · **Date:** 2026-06-24

## Context

C++ has no single standard build/dependency tool. We need reproducible builds on Windows (the dev
machine) and CI, plus several third-party libraries (Drogon, a JSON lib, an HTTP client, a test
framework).

## Options considered

- **CMake + vcpkg** — CMake is the de-facto standard build system; vcpkg (manifest mode via
  `vcpkg.json`) gives declarative, reproducible dependencies and integrates tightly with CMake/MSVC.
- **CMake + Conan** — also strong; slightly different workflow; equally valid.
- **Manual / system packages** — not reproducible; painful on Windows.

## Decision

Use **CMake** with **vcpkg in manifest mode** (`vcpkg.json`).

## Consequences

- **Positive:** reproducible dependencies declared in-repo; first-class Windows/MSVC support;
  industry-standard CMake skills.
- **Negative:** CMake has a learning curve; first-time vcpkg builds can be slow.
- **Note:** exact `CMakeLists.txt` details are best finalized during the toolchain spike, not on paper.
