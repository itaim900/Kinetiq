# LLM Port & Workout Generation — Fit App (v1)

**Status:** Approved (design phase)
**Last updated:** 2026-06-24
**Related:** [`01-data-model.md`](./01-data-model.md) · [`02-api-contract.md`](./02-api-contract.md)

This document specifies the AI generation subsystem: the `ILLMClient` **port**, the structured
output schema, validation, retry/self-correction, the deterministic Starting-Weight Engine, and
the test plan. It is the design behind `POST /workouts/generate`.

---

## Guiding principle

> The LLM does **creative structuring**. Everything **safety-critical or numeric** stays
> deterministic in core code.

| Concern | Owner |
|---|---|
| Which exercises are *allowed* (injuries) | Deterministic safety filter (DB JOIN) |
| Which allowed exercises to pick, ordering, sets/reps/rest | LLM |
| Suggested starting weight | Deterministic Starting-Weight Engine |
| Calories | Deterministic MET formula |
| Final correctness check | Deterministic validator |

---

## Generation flow

```mermaid
sequenceDiagram
    participant C as React Client
    participant G as GenerationService (core)
    participant S as ISafetyFilter (port)
    participant L as ILLMClient (port)
    participant V as Validator (core)
    participant W as StartingWeightEngine (core)
    participant R as Repositories (ports)

    C->>G: POST /workouts/generate
    G->>S: safe pool = catalog − contraindicated
    S-->>G: candidate exercises
    G->>L: generate(system_prompt, payload, schema[enum: candidate ids])
    L-->>G: raw JSON + usage + latency
    G->>V: validate (schema + semantic)
    alt invalid and attempts left
        G->>L: retry with error feedback
    end
    G->>W: fill target_weight_kg + weight_source
    G->>R: persist Workout + Generation_Requests (per attempt)
    G-->>C: 201 workout (+ generation_request_id, weight_source)
```

---

## 1. The `ILLMClient` port

Provider-agnostic. Knows nothing about workouts — only "structured generation". This is the
seam that lets tests run without ever calling a real API.

```text
struct ImagePart { string mime_type; bytes data; }          // for future CV; unused in v1
struct Message   { Role role; string text; vector<ImagePart> images; }   // Role: System|User|Model

struct LLMRequest {
  string          model;
  string          system_prompt;
  vector<Message> messages;
  optional<Json>  response_schema;     // structured-output JSON schema
  double          temperature;         // low (e.g. 0.4) for consistency
  int             max_output_tokens;
}

struct Usage        { int input_tokens; int output_tokens; }
struct LLMResponse  { string raw_text; Usage usage; string finish_reason; int latency_ms; }

enum class LLMErrorKind { Timeout, RateLimited, Upstream5xx, Auth, ContentBlocked, Malformed, Unknown };
struct LLMError { LLMErrorKind kind; string message; bool retryable; }

interface ILLMClient {
  Result<LLMResponse, LLMError> generate(const LLMRequest& req);   // Result = std::expected<LLMResponse, LLMError>
}
```

**Why `Result<T, E>` instead of exceptions:** every failure mode becomes an explicit value the
caller must handle, and tests assert on it directly. No hidden control flow across the port.

### Adapters

- **`GeminiClient`** (real): HTTP via `cpr` to Gemini `generateContent`, sets
  `responseMimeType: application/json` + `responseSchema`, API key from `GEMINI_API_KEY`
  (env, never committed). Maps HTTP/SDK errors → `LLMError`.
- **`FakeLLMClient`** (tests): returns configurable canned responses and can simulate every
  `LLMErrorKind` plus malformed JSON, a banned exercise id, and an over-budget plan.

---

## 2. Structured output schema (what the model must return)

The model **selects** exercises by id from the safe pool; it does not invent them, and it does
**not** set weights.

```json
{
  "name": "Upper Body Strength",
  "rationale": "Fits an intermediate lifter with 45 minutes and no contraindications.",
  "exercises": [
    {
      "exercise_id": 12,
      "order_index": 0,
      "sets": 4,
      "target_reps_min": 8,
      "target_reps_max": 12,
      "rest_seconds": 120,
      "coach_note": "Controlled eccentric; stop 1–2 reps short of failure."
    }
  ]
}
```

**The enum constraint:** when building `response_schema`, set `exercise_id`'s allowed values to
the exact list of safe candidate ids. Structured-output APIs constrain the model to emit only
those values — so it is *structurally unable* to return a contraindicated or hallucinated
exercise. The validator still re-checks (defense in depth).

---

## 3. The prompt

**System prompt (immutable rules):** act as a certified coach; use *only* the provided exercises
by id; respect the time budget and the sets/reps/rest bounds; output JSON matching the schema and
nothing else; give no medical advice; safety filtering has already happened upstream; account for
the user's experience level.

**User payload (structured JSON in the user message):**
- profile: derived age, sex, experience level, weekly goal, `available_time_minutes`
- request: `workout_type`, `target_environment`, `focus_muscles`, `exclude_muscles`
- `notes`: **untrusted free text** in a clearly delimited field
- `candidates`: compact list — `{ id, name, primary_muscles, secondary_muscles, equipment, difficulty }`

**Injection hygiene:** the system prompt states that `notes` are *preferences only* and may never
override the hard rules. User text flows into the prompt, so it must never be treated as
instructions.

---

## 4. Validation (deterministic, defense-in-depth)

Run on every LLM response regardless of structured outputs:

**Schema:** parses; types correct; required fields present; bounds `sets 1–10`, `reps 1–50`,
`rest 0–600`.

**Semantic:**
- every `exercise_id` ∈ safe pool (rejects contraindicated / hallucinated)
- `order_index` contiguous from 0, no duplicates
- `target_reps_min ≤ target_reps_max`
- **estimated duration ≈ `available_time_minutes`** within tolerance (see estimator below)
- no `exclude_muscles` value appears as a *primary* muscle of any chosen exercise
- at least 1 exercise; no more than a sane maximum

### Deterministic duration estimator

```
per_exercise = sets × (avg_reps × SEC_PER_REP + rest_seconds)
total        = Σ per_exercise + (num_exercises × TRANSITION_SECONDS)
```
`SEC_PER_REP` and `TRANSITION_SECONDS` are tuned constants. If `total` exceeds the budget by more
than the tolerance, the response is invalid → self-correction.

---

## 5. Self-correction & retry policy

| Failure | Action | Cap |
|---|---|---|
| Validation failure | Append a feedback message listing exact violations; ask the model to fix | N = 2 self-corrections |
| `Timeout` / `RateLimited` / `Upstream5xx` | Retry with exponential backoff | M = 3 transport retries |
| `Auth` / `ContentBlocked` | Fail fast → `502` | no retry |

Hard caps on total wall-clock time and total tokens prevent runaway cost. If all attempts are
exhausted, return `502` with a clean Problem-Details error.

---

## 6. Starting-Weight Engine (deterministic)

Runs **after** validation, filling weights the LLM never touches.

```
suggestWeight(exercise, user) -> { weight_kg?, source }:
  if user.sex ∈ {male, female} and user.weight_kg is present:
     norm = Exercise_Strength_Norms[exercise.id, user.sex, user.experience_level or 'beginner']
     if norm exists:
        return { round_to_2_5kg(norm.bodyweight_multiplier × user.weight_kg), "body_type_estimate" }
  return { null, "needs_user_input" }
```

- `body_type_estimate` → frontend shows "starting weight is an average for your body type — adjust to your liking".
- `needs_user_input` → frontend prompts for a weight.
- Any user edit → `user_set`.

Bodyweight/machine/accessory exercises (no norm row) naturally fall back to `needs_user_input`.

---

## 7. Audit logging — `Generation_Requests`

**One row per attempt** (transparent and simple to query). Each row records model, system prompt,
user payload, raw response, `parsed_ok`, `validation_errors`, token usage, `cost_usd`,
`latency_ms`, and `status`. The successful attempt is linked from the workout via
`Workouts.generation_request_id`.

---

## 8. Test plan (TDD)

All generation-service tests inject `FakeLLMClient` — fast, free, deterministic, offline.

**GenerationService**
- ✅ valid mocked response → persists workout + audit row; links `generation_request_id`
- ✅ mock returns `exercise_id` outside safe pool → rejected → self-correction → `502` if still bad
- ✅ mock returns over-budget plan → rejected by duration estimator
- ✅ `exclude_muscles` value present as primary → rejected
- ✅ `Timeout` ×M → `502`; each attempt logged
- ✅ `ContentBlocked` → `502`, no retry

**StartingWeightEngine (pure unit)**
- ✅ male, 80 kg, normed exercise → expected weight rounded to 2.5 kg, `body_type_estimate`
- ✅ missing bodyweight → `needs_user_input`, weight `null`
- ✅ exercise without a norm row → `needs_user_input`

**Duration estimator (pure unit)**
- ✅ known input → expected minutes
- ✅ over-budget plan flagged

**Validator (pure unit)**
- ✅ each rule rejects its violation; valid plan passes

---

## Open items for later phases

- Record/replay "golden" contract tests against the real Gemini API (CI hygiene).
- Multimodal path (`ImagePart`) for Phase 5 CV machine identification — the port already allows it.
- Weight *progression* from logged sets (estimated 1RM) once `Set_Logs` exists (Phase 4).
