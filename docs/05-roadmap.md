# Roadmap & Milestones — Fit App

**Status:** Approved (design phase)
**Last updated:** 2026-06-24

This roadmap breaks the project into phases with concrete deliverables and a **Definition of Done
(DoD)** per phase. v1 ships at the end of **Phase 3**. Phases 4–5 are designed-for but deferred.

Work items are tracked as **GitHub Issues** grouped under **Milestones** (one milestone per phase).

| Phase | Milestone | Goal | Status |
|---|---|---|---|
| 0 | Design & Planning | Lock data model, contracts, architecture, decisions | In progress |
| 1 | Walking Skeleton | Prove the toolchain end-to-end (React → C++ → Postgres) | Planned |
| 2 | Core Domain | Auth, profile, exercise catalog, manual workouts (TDD) | Planned |
| 3 | AI Generation (v1) | Safe, validated, AI-generated workouts | Planned |
| 4 | Feedback Loop | History, set logs, RPE-driven adaptation, calories | Deferred |
| 5 | Advanced | CV machine ID, variety algorithm, friend updates, true agent | Deferred |

---

## Phase 0 — Design & Planning

**Goal:** decide *what* and *how* before coding.

**Done so far:** data model (`01`), API contract (`02`), LLM port (`03`), architecture (`04`), ADRs.

**Remaining deliverables:**
- Test strategy doc (TDD workflow, pyramid, tooling, CI)
- Frontend design doc (pages, components, state, UX)
- Seed-data plan (muscles, equipment, exercises, contraindications, MET, strength norms)
- OpenAPI spec (`docs/openapi.yaml`)
- Project conventions (formatting, linting, commit/branch conventions)

**DoD:** every remaining design artifact above is written and committed; the team (you) can start
Phase 1 without open architectural questions.

---

## Phase 1 — Walking Skeleton

**Goal:** the thinnest possible end-to-end slice that exercises the whole toolchain, so future
work is just "filling in." No business logic.

**Deliverables:**
- [x] Backend builds with CMake + Ninja + MSYS2 GCC + Crow ("hello") — see ADR-0011/0012
- [x] `docker-compose.yml` runs Postgres
- [x] First migration applied; backend connects to the DB
- [x] `GET /api/v1/health` returns app + DB status
- [x] GoogleTest wired with one passing test
- [x] React + TypeScript + Vite scaffold
- [x] A page that calls `/health` and renders the result (via the dev proxy)
- [x] Vitest + Testing Library + MSW wired with one passing test
- [ ] **GitHub Actions CI** builds and tests both backend and frontend *(do this first)*
- [ ] **Containerize** backend + frontend + DB into a single `docker compose up` *(after CI)*

**DoD:** a developer can clone the repo, run one or two documented commands (or just
`docker compose up`), and see the React page display live data from the C++ backend backed by
Postgres; CI is green.

---

## Phase 2 — Core Domain

**Goal:** all non-AI functionality, built test-first.

**Deliverables (backend, each TDD):**
- Migrations for all v1 tables
- Repository ports + in-memory fakes
- Password hashing (argon2) + JWT issue/verify + auth middleware → `CurrentUser`
- Auth endpoints: register / login / me
- Profile endpoints: `GET`/`PATCH /users/me`
- Injuries CRUD
- Exercise catalog: `GET /exercises` (filters + pagination), `GET /exercises/{id}`
- Seed script: muscles, equipment, exercises, contraindications
- Manual workouts CRUD

**Deliverables (frontend):**
- Auth flow (register/login, in-memory token handling)
- Profile + injuries forms (React Hook Form + Zod)
- Exercise browser
- Manual workout builder

**DoD:** a user can register, log in, manage their profile and injuries, browse the seeded exercise
catalog, and build/save a workout by hand — all covered by passing unit/integration/contract tests.

---

## Phase 3 — AI Generation (v1 release)

**Goal:** the headline feature — safe, validated, AI-generated workouts.

**Deliverables (each TDD unless noted):**
- `ILLMClient` port + `FakeLLMClient`
- Deterministic safety filter (injury contraindication JOIN)
- Deterministic duration estimator
- Starting-weight engine + strength-norm seed data
- Output schema + validator + self-correction loop
- `GenerationService` orchestration
- `GeminiClient` adapter (real LLM)
- `POST /workouts/generate` endpoint + `Generation_Requests` auditing (contract test)
- Calorie engine (MET)
- Frontend: generate-workout flow + result view with `weight_source` messaging
- v1 demo polish + README usage/screenshots

**DoD:** an authenticated user submits goals/constraints and receives a generated, contraindication-
free workout whose total duration fits their time budget, with body-type starting-weight suggestions
where data allows; every AI call is audited; the full flow is demoable and tested.

---

## Phases 4–5 (deferred — designed-for)

- **Phase 4 — Feedback Loop:** `Workout_History`, `Set_Logs`, RPE capture, plan adaptation,
  historical calories, **friend workout updates** (motivational feed).
- **Phase 5 — Advanced:** CV gym-machine identification, muscle-variety/coverage algorithm,
  function-calling "true agent" upgrade, analytics dashboard.

---

## How to use this with GitHub

- Each phase is a **Milestone**; each deliverable is an **Issue** under it.
- Track progress on the [Issues](https://github.com/itaim900/Fit-App/issues) and
  [Milestones](https://github.com/itaim900/Fit-App/milestones) pages.
- Recommended flow: one branch per issue → PR → review → merge → close issue.
