# ADR-0003: C++ web framework is Drogon

**Status:** Superseded by [ADR-0011](./0011-web-framework-crow.md) · **Date:** 2026-06-24

> **Superseded:** During the Phase-1 toolchain spike, building Drogon from source on the 8 GB dev
> machine proved impractical (out-of-memory, ~1h builds), and MSYS2 ships no Drogon package. The
> HTTP layer was switched to **Crow**. See [ADR-0011](./0011-web-framework-crow.md).

## Context

We need an HTTP server framework for the C++ backend (routing, request/response, JSON, async I/O).
The author is newer to C++, so framework ergonomics and documentation matter.

## Options considered

- **Drogon** — high-performance async framework; built-in routing, JSON, ORM, DB clients (Postgres),
  filters/middleware. More "batteries included."
- **Crow** — lightweight, header-only, Flask-like; very simple, but you build more yourself (DB, etc.).
- **Pistache / cpp-httplib / Beast** — lower-level or less full-featured for this use case.

## Decision

Use **Drogon**. Its built-in routing, JSON, middleware/filters, and Postgres support reduce the
amount of plumbing to write, which matters most while learning.

## Consequences

- **Positive:** less boilerplate; integrated DB + JSON; good performance; controller/filter model
  maps cleanly onto our inbound-adapter layer.
- **Negative:** smaller community and fewer tutorials than mainstream web frameworks; some Drogon
  idioms to learn.
- **Boundary:** Drogon is confined to the `http/` adapter layer (ADR-0002); the core never imports it.
