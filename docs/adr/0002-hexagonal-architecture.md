# ADR-0002: Architecture style is Hexagonal (Ports & Adapters)

**Status:** Accepted · **Date:** 2026-06-24

## Context

We want a test-driven codebase (TDD) and the ability to swap external technologies (LLM provider,
auth scheme, even the web framework) without rewrites. The AI feature in particular must be
testable without calling a paid, slow, non-deterministic external API.

## Options considered

- **Hexagonal (Ports & Adapters):** core logic depends on interfaces; tech plugged in at edges.
- **Classic layered/MVC:** simpler, but controllers/services often bind directly to framework and DB.
- **Big-ball-of-mud / framework-first:** fastest to start, worst to test and evolve.

## Decision

Adopt **Hexagonal Architecture**. The core (`app` + `domain` + `ports`) compiles and is fully
testable with no framework, DB, or network. Adapters (Postgres, Gemini, HTTP) are wired at the
composition root and replaced with fakes in tests.

## Consequences

- **Positive:** fast, free, deterministic unit tests; technology swaps are localized (e.g.
  JWT→sessions touches one adapter); clear dependency direction.
- **Negative:** more interfaces/indirection up front; a learning curve in defining good ports.
- **Enables:** ADR-0006 (auth swap), ADR-0008 (LLM provider swap), the whole test strategy.
