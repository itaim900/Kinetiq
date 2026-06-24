# ADR-0008: LLM provider is Gemini, hidden behind `ILLMClient`

**Status:** Accepted · **Date:** 2026-06-24

## Context

The app generates workouts with an LLM and (later) identifies gym machines from images, so a
**multimodal** model is desirable. We also want to avoid coupling the codebase to one vendor.

## Options considered

- **Google Gemini** — strong multimodal support; structured-output (JSON schema) support; suits the
  future CV feature.
- **OpenAI** — comparable capabilities; equally viable.
- **Local/open models** — cheaper at scale, more ops burden; weaker tooling for this use case.

## Decision

Use **Gemini** for v1, accessed **only** through the `ILLMClient` port (`03`). The concrete
`GeminiClient` is an outbound adapter; tests use `FakeLLMClient`.

## Consequences

- **Positive:** multimodal-ready for Phase 5 CV; structured outputs constrain responses; vendor is
  swappable by writing a new adapter.
- **Negative:** external dependency, per-token cost, network latency/failure modes.
- **Mitigations:** the `Generation_Requests` audit table, retries/self-correction, and cost/token
  caps (`03`). API key via env var, never committed.
