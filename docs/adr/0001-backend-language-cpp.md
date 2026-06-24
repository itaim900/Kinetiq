# ADR-0001: Backend language is C++

**Status:** Accepted · **Date:** 2026-06-24

## Context

This is a learning + portfolio project. A primary goal is to deepen systems-level programming
skill and stand out from typical full-stack portfolios. The backend is an HTTP API orchestrating
a database and an external LLM.

## Options considered

- **C++** — maximal learning of memory model, types, build systems; rare and impressive in web backends.
- **Node/TypeScript** — fastest path for web APIs; shares language with the frontend.
- **Go / Python** — strong, conventional web-backend ergonomics; large ecosystems.

## Decision

Use **C++** for the backend.

## Consequences

- **Positive:** strongest learning outcome; distinctive portfolio piece; forces rigorous thinking
  about ownership, error handling, and build tooling.
- **Negative / risks:** web tooling is rougher than mainstream languages; more boilerplate; slower
  initial velocity; smaller community for web-specific problems.
- **Mitigations:** choose a batteries-included framework (ADR-0003), a strong dependency manager
  (ADR-0005), and a hexagonal structure (ADR-0002) so most logic is plain, testable C++ that is
  insulated from framework friction.
