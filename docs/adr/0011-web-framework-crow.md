# ADR-0011: C++ web framework is Crow (supersedes ADR-0003)

**Status:** Accepted · **Date:** 2026-06-24

## Context

[ADR-0003](./0003-cpp-web-framework-drogon.md) chose Drogon. During the Phase-1 walking-skeleton
spike we hit hard constraints on the actual dev machine (8 GB RAM):

- Drogon is not packaged for MSYS2, and building it via vcpkg compiles **OpenSSL → libpq → trantor →
  Drogon** from source. On 8 GB RAM this caused an `OutOfMemoryException` and ~1-hour builds.
- Our architecture ([ADR-0002](./0002-hexagonal-architecture.md)) already puts persistence behind
  **repository ports** using `libpqxx`, so Drogon's main differentiator — its built-in ORM — is not
  actually needed.

## Options considered

- **Keep Drogon, build from source** — honors ADR-0003 but slow and memory-fragile on this hardware.
- **MSVC + vcpkg, slimmed deps** — still compiles OpenSSL/libpq from source; heavy.
- **Crow (header-only)** — single vendored header; its only real dependency (`asio`) plus
  `nlohmann-json` and `libpqxx` are all available **prebuilt** from MSYS2.

## Decision

Use **Crow** as the HTTP layer. JSON via **nlohmann-json**; Postgres access via **libpqxx** behind
repository ports. Crow is vendored as a single header at `backend/third_party/crow/crow_all.h`.

## Consequences

- **Positive:** dependency setup drops from ~1 hour to minutes with low memory use; fits the dev
  machine; fewer moving parts. Fits the hexagonal design cleanly.
- **Negative:** Crow has a smaller feature set than Drogon (no built-in ORM, fewer middleware
  helpers) — we implement what we need ourselves. Smaller ecosystem.
- **Boundary:** Crow stays confined to the `http/` adapter layer; the core never imports it, so this
  swap touched only that thin layer.
- **Validation:** spike proved `GET /api/v1/health` end-to-end (React → Vite proxy → Crow → libpqxx
  → Postgres) returning `{"status":"ok","db":"connected"}`.
