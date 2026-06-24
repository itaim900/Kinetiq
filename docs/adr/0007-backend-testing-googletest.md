# ADR-0007: Backend testing with GoogleTest, developed test-first (TDD)

**Status:** Accepted · **Date:** 2026-06-24

## Context

The project is explicitly test-driven: tests are written before implementation. We need a C++ test
framework with good assertions, fixtures, and mocking support.

## Options considered

- **GoogleTest (+ GoogleMock)** — widely used; rich assertions, fixtures; GoogleMock for mocking ports.
- **Catch2** — elegant, header-only, expressive; mocking is a separate concern.
- **doctest** — very fast to compile; minimal.

## Decision

Use **GoogleTest + GoogleMock**. GoogleMock pairs naturally with our ports (ADR-0002) for fast unit
tests; GoogleTest fixtures suit integration tests against Postgres.

## Consequences

- **Positive:** one mature toolchain covers unit + integration; mocking ports is first-class.
- **Negative:** heavier compile/setup than doctest.
- **Workflow:** red → green → refactor; the API contract (`02`) and LLM port (`03`) already enumerate
  the test cases to write first.
