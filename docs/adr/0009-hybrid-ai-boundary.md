# ADR-0009: Hybrid deterministic + generative boundary

**Status:** Accepted · **Date:** 2026-06-24

## Context

LLMs are creative but can hallucinate and are non-deterministic. Some outputs in this app are
safety- or correctness-critical (injury contraindications, calories, starting weights) and must
never be left to chance.

## Options considered

- **All-generative:** ask the LLM for everything (exercises, numbers, calories). Simple prompt,
  unsafe and unverifiable.
- **All-rule-based:** no LLM; hand-coded plan generation. Safe but rigid and not the project's goal.
- **Hybrid:** deterministic code owns safety and numbers; the LLM owns creative structuring within
  a constrained, validated envelope.

## Decision

Adopt the **hybrid boundary**:

| Owned by deterministic C++ | Owned by the LLM |
|---|---|
| Safety filter (injury contraindications) | Which allowed exercises to pick |
| Starting-weight engine | Ordering and sets/reps/rest (within bounds) |
| Calorie computation (MET) | Natural-language rationale / coaching notes |
| Output validation | |

## Consequences

- **Positive:** unsafe exercises are structurally impossible in output; numbers are reproducible and
  explainable; the LLM still adds genuine value.
- **Negative:** more backend logic than a pure prompt; requires maintaining reference data
  (contraindications, MET values, strength norms).
- **Reference:** validation + self-correction details in `03-llm-port.md`.
