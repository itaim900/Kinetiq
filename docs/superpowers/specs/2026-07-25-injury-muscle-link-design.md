# Slice 2 · Stage B extension — Injury/Muscle Link: Design Spec

> **⚠️ Partly superseded (2026-10-21)** by
> [`2026-10-21-injury-conditions-catalog-design.md`](2026-10-21-injury-conditions-catalog-design.md):
> E2, E4, E5 and the muscle half of E1 are replaced (injuries now link to **several** muscles via a
> conditions catalog). `injury_type` (E1) and migration 0003 were built as described here.

**Status:** Approved design (brainstormed 2026-07-25) — ready for implementation planning
**Extends:** Stage B (`#20` injuries CRUD), already built and merged into branch `slice-2-profile`
(not yet merged to `main`).
**Related:** [`docs/superpowers/specs/2026-07-17-slice-2-profile-injuries-design.md`](./2026-07-17-slice-2-profile-injuries-design.md)
(original Slice 2 spec), [`docs/01-data-model.md`](../../01-data-model.md) (`Muscles`,
`Exercise_Muscles`), [`docs/phase-2-slice-2-profile-injuries.md`](../../phase-2-slice-2-profile-injuries.md)
(living build log)

---

## Goal

Let an injury optionally reference a specific muscle from the catalog (e.g. a hamstring strain
→ the "Hamstrings" muscle), while still supporting injuries that aren't muscle-related at all
(bone, tendon/ligament, joint). Also unblocks Stage C's future injury form with a small read-only
muscle-picker endpoint.

---

## Key finding that shapes this design

The Phase 3 rule-based safety filter is **already designed to key off `User_Injuries.body_region`
as free text**, JOINed against `Exercise_Contraindications.body_region` (also free text) — it was
never wired to the `Muscles` table. So this feature is a **new, parallel** structured relationship
layered on top of injuries, not a replacement of `body_region` or the existing filter mechanism.
`body_region` is untouched by this design.

The `Muscles` table exists in the schema (`0002_core_schema.sql`) but is **completely empty** —
seeding is otherwise planned for Slice 3 (`#22`). This design seeds a small subset now, using the
exact names already documented in `01-data-model.md`'s Phase 2 plan (pectorals, deltoids, biceps,
triceps, lats, traps, rhomboids, erector spinae, rectus abdominis, obliques, glutes, quads,
hamstrings, calves), so Slice 3's full catalog seed stays compatible (`ON CONFLICT DO NOTHING`).

---

## Locked decisions

### E1 — `injury_type` + optional `muscle_id`, not a full anatomy model
Every injury gets a required `injury_type` (`muscle | bone | tendon_ligament | joint | other`) and
an optional `muscle_id` FK, valid only when `injury_type == "muscle"`. Rejected building separate
`Bones`/`Tendons` catalog tables — `injury_type` alone captures "what kind of tissue," and only
muscles get a structured catalog link for now (the common, well-modeled case); bone/tendon/joint
injuries stay described by `body_region` + free-text `notes`. Revisable later if a real need for
structured bone/tendon catalogs emerges.

### E2 — One muscle per injury, no join table
A single nullable `muscle_id` column, not a many-to-many `injury_muscles` table. Covers the
overwhelming majority of real cases ("I strained my hamstring"). Multi-muscle injuries (e.g. a
"rotator cuff" spanning several shoulder muscles) remain fully describable via free-text `notes`;
they just don't get multiple structured links. Revisable later.

### E3 — Seed a small muscle subset now, matching the documented Phase 2 list
16 muscles, grouped by `muscle_group`, inserted via the new migration with
`ON CONFLICT (name) DO NOTHING`. Not the full Slice 3 catalog — just enough for this feature to be
usable end-to-end today, using names Slice 3 can build on without renaming.

### E4 — `muscle_id` is never required, even when `injury_type == "muscle"`
A user can say "it's muscle-related" without knowing exactly which muscle. Whether a *provided*
`muscle_id` is valid is checked by `InjuryService` (via a new `IMuscleRepository`), not by the pure
domain policy (which has no DB access) — a bad id returns a clean `422`, never a raw FK-violation
`500`.

### E5 — Update cascade: changing `injury_type` away from `"muscle"` auto-clears `muscle_id`
Consistent with the existing "absent = unchanged" update semantics (no explicit null-clearing in
v1, per the original spec's D3): `UpdateInjuryFields.muscleId` is a plain optional — present sets
it, absent leaves it alone. But if an update changes `injuryType` away from `"muscle"`, the service
force-clears `muscleId` regardless of the payload, because the DB's `CHECK` constraint would
otherwise reject the row. This is the service maintaining an invariant, not a null-clearing feature.
**Known gap, deliberately accepted:** there's no way yet to un-link a muscle while *keeping*
`injury_type == "muscle"` — same category of limitation as the profile's no-null-clearing gap.

### E6 — Add a read-only `GET /muscles` (+ `GET /muscles/{id}`) now
Rather than backend-only. Needed anyway to validate `muscle_id` cleanly, and this also unblocks
Stage C's injury form (it needs *something* to populate a muscle picker) without a follow-up slice.
🔒 like the rest of this API, matching existing convention even for shared reference data.

---

## Data model

### Migration `0003_injury_muscle_link.sql`

```sql
ALTER TABLE user_injuries
    ADD COLUMN injury_type TEXT NOT NULL DEFAULT 'other'
        CHECK (injury_type IN ('muscle','bone','tendon_ligament','joint','other')),
    ADD COLUMN muscle_id BIGINT REFERENCES muscles(id);

-- Enforce the pairing: a muscle link only makes sense for muscle-type injuries.
ALTER TABLE user_injuries
    ADD CONSTRAINT chk_muscle_id_requires_muscle_type
    CHECK (muscle_id IS NULL OR injury_type = 'muscle');

CREATE INDEX idx_user_injuries_muscle_id ON user_injuries(muscle_id);

-- Seed a common-injury subset (names match 01-data-model.md's documented Phase 2
-- list; Slice 3's full catalog seed can safely re-run against these).
INSERT INTO muscles (name, muscle_group) VALUES
    ('Pectorals', 'chest'), ('Anterior Deltoid', 'shoulders'),
    ('Lateral Deltoid', 'shoulders'), ('Posterior Deltoid', 'shoulders'),
    ('Biceps', 'arms'), ('Triceps', 'arms'), ('Lats', 'back'), ('Traps', 'back'),
    ('Rhomboids', 'back'), ('Erector Spinae', 'back'), ('Rectus Abdominis', 'core'),
    ('Obliques', 'core'), ('Glutes', 'legs'), ('Quads', 'legs'),
    ('Hamstrings', 'legs'), ('Calves', 'legs')
ON CONFLICT (name) DO NOTHING;
```

`injury_type DEFAULT 'other'` at the DB level is a safety net for the `ALTER TABLE` only — the API
itself requires `injury_type` explicitly on every create (validated by `InjuryPolicy`, same as
`severity` already is); there is no implicit "falls back to other" behavior at the application
layer.

### Domain types

```cpp
// domain/Injury.h — grown
struct Injury {
    long long id = 0;
    long long userId = 0;
    std::string bodyRegion;
    std::string severity;
    std::string injuryType;              // NEW: muscle|bone|tendon_ligament|joint|other (required)
    std::optional<long long> muscleId;   // NEW: nullable; valid only if injuryType == "muscle"
    bool active = true;
    std::optional<std::string> notes;
    std::string createdAt;
};

// domain/Muscle.h — NEW
struct Muscle {
    long long id = 0;
    std::string name;
    std::string muscleGroup;
};
```

### `InjuryPolicy` additions (pure, no DB access)

- `injury_type` required; must be one of the 5 enum values.
- **Cross-field rule:** if `muscleId` has a value, `injuryType` must be `"muscle"` — else reject
  with field `"muscle_id"`.
- Existence of a provided `muscle_id` is **not** checked here (no DB access in domain code) — that's
  `InjuryService`'s job.

---

## Repository, service, and HTTP layers

### `IMuscleRepository` — new, read-only port

```cpp
// ports/IMuscleRepository.h
class IMuscleRepository {
public:
    virtual ~IMuscleRepository() = default;
    virtual std::vector<domain::Muscle> list() = 0;
    virtual std::optional<domain::Muscle> findById(long long id) = 0;
};
```

Implementations, mirroring the existing `IUserRepository`/`IInjuryRepository` shape:
- `testsupport::InMemoryMuscleRepository` — fake, pre-seeded with a couple of test muscles.
- `db::PgMuscleRepository` — real SQL, takes `pqxx::work&` (composes with `DbTest` rollback).
- `db::AutoCommitPgMuscleRepository` — production wrapper, connection-per-call. Kept for
  consistency with the other `AutoCommit*` repos even though muscles are read-only, since it's
  wired into `main.cpp`'s route-capture-by-reference lifetime the same way.

### `InjuryService` changes

Constructor becomes `InjuryService(IInjuryRepository&, IMuscleRepository&)`.

`add()` and `update()` gain, after domain validation:
- **If** the resulting `injuryType == "muscle"` **and** `muscleId` is set → `muscles_.findById(muscleId)`;
  if absent, throw `InjuryValidationError("muscle_id", "muscle not found")` → HTTP `422`.

`list()` and `remove()` are unchanged (no muscle repo interaction needed).

**Update cascade (E5):** if the fields being applied change `injuryType` away from `"muscle"`, the
service sets the resulting `muscleId` to `nullopt` regardless of what was in the update payload,
before running domain validation and persisting.

### HTTP layer

- `InjuryDto` grows to parse/serialize `injury_type` and `muscle_id` on create, update, and
  response bodies. Responses include `muscle_id` (nullable) but **not** a denormalized
  `muscle_name` — the client fetches the muscle list once for its picker and joins locally, keeping
  `InjuryService.list()` free of per-row muscle lookups.
- New **`MuscleDto`** (`muscleToJson`, `muscleListToJson`) + **`MuscleController`**
  (`registerMuscleRoutes`):
  - `GET /api/v1/muscles` 🔒 → `200`, full list (~16 rows, no pagination).
  - `GET /api/v1/muscles/{id}` 🔒 → `200` detail, `404` if unknown.

---

## Impact on already-shipped Stage B code

`injury_type` becoming required means every existing Stage B file that constructs an `Injury` or
calls `InjuryService`/`registerInjuryRoutes` needs a small, mechanical update (not a rewrite) —
normal "ports grow as new use cases demand it" churn, the same pattern Slice 1 saw when
`ChangePassword` extended `IUserRepository`:

| File | Change needed |
|---|---|
| `test_injury_policy.cpp` | `validInjury()` helper gains `injuryType = "other"`; new cases for the enum + cross-field rule |
| `test_injury_repository.cpp`, `test_pg_injury_repository.cpp`, `test_auto_commit_pg_injury_repository.cpp` | `make()` helpers / direct construction gain `injuryType` |
| `test_injury_service.cpp` | `InjuryService` now constructed with `(injuryRepo, muscleRepo)`; `addReq()` gains `injuryType` |
| `test_injury_controller.cpp` | JSON request bodies gain `"injury_type"`; test fixture constructs a fake `IMuscleRepository` too |
| `main.cpp` | `registerInjuryRoutes` call gets the muscle repo; new `AutoCommitPgMuscleRepository` + `registerMuscleRoutes` |

---

## Testing strategy

- **Domain:** `InjuryPolicy` enum + cross-field cases (muscle_id set with non-muscle type → reject).
- **Repository:** `IMuscleRepository` contract tests (fake + real Postgres), same shape as the
  injury repo tests.
- **Service:** `InjuryService` — muscle-type + valid `muscle_id` succeeds; muscle-type + unknown
  `muscle_id` → `422`; non-muscle type + `muscle_id` set → `422`; changing type away from `"muscle"`
  auto-clears `muscle_id`.
- **HTTP:** `InjuryController` cases updated/extended for the new fields; new `MuscleController`
  tests (list, get-by-id, unknown-id `404`).
- Each stage ends with a **curl live-verify** against the real server + Postgres, matching Stage
  A/B discipline.

---

## Staging

| Stage | Steps |
|---|---|
| **B-Ext1** | Migration (`0003_injury_muscle_link.sql`) + `domain::Muscle` + grown `Injury` + `InjuryPolicy` cross-field rule (TDD) |
| **B-Ext2** | `IMuscleRepository` + fake + `PgMuscleRepository` + `AutoCommitPgMuscleRepository` (TDD, mirrors Stage B3) |
| **B-Ext3** | Update `InjuryService` (muscle validation, type/id cascade) + retrofit existing Stage B tests (TDD) |
| **B-Ext4** | `MuscleDto`/`MuscleController` (`GET /muscles`, `GET /muscles/{id}`) + extend `InjuryDto`/`InjuryController` for the new fields (TDD) |
| **B-Ext5** | Wire `main.cpp` + live-verify (create muscle-type injury, bad `muscle_id` → 422, non-muscle + `muscle_id` → 422, `GET /muscles`) |

Same branch (`slice-2-profile`, still unmerged), same TDD/verify discipline as Stages A and B.

---

## Deferrals (documented, not accidental)

- **No structured `Bones`/`Tendons` catalogs** — `injury_type` alone distinguishes tissue kind for
  non-muscle injuries; `body_region` + `notes` remain their only description (E1).
- **No multi-muscle injuries** — single `muscle_id`, no join table (E2).
- **No explicit "clear the muscle while staying muscle-type"** — same category as the profile's
  no-null-clearing gap (E5).
- **No `muscle_name` denormalized onto injury responses** — client joins locally against the
  fetched muscle list.
