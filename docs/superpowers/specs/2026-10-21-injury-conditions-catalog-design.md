# Injury conditions catalog: multi-muscle injuries (design)

**Date:** 2026-10-21 · **Status:** Approved (brainstorming) · **Branch:** `slice-2-profile`
**Supersedes** in [`2026-07-25-injury-muscle-link-design.md`](2026-07-25-injury-muscle-link-design.md):
E2 (one muscle per injury), E4 (optional single `muscle_id`), E5 (type-change cascade) and the
muscle half of E1. E3 (muscle seed names) and E6 (a muscles endpoint) carry forward, enlarged.
The July plan's Tasks 3–8 are replaced by a new plan.

## Goal
Users single out their injuries so their workouts can be built around them. An injury should
reflect its real effect on the body: **several muscles**, with a role. For **known conditions** the
app supplies that mapping (and the user can't alter it); **unknown injuries** are described by the
user from fixed lists.

## Decisions
| # | Decision |
|---|---|
| C1 | **Known conditions come from a catalog** (`injury_conditions`); each maps to several muscles with a role: `primary` = the injured tissue itself, `secondary` = involved / to be protected. |
| C2 | **Known condition = locked.** The condition fixes `body_region`, `injury_type` and muscles. The user supplies only `severity`, `notes`, `active`. Sending a locked field → 422. Changing the condition = delete + re-add. |
| C3 | **Unknown injury** (no condition): user supplies `body_region`, `injury_type`, optional `muscle_ids`, `severity`, `notes`. Muscles allowed for **any** `injury_type` (the July "muscle_id only when type = muscle" rule is dropped: e.g. a ligament injury still involves muscles). |
| C4 | **Body region is a fixed list** of 14 (`body_regions` table, FK from `user_injuries` and `exercise_contraindications`), so the Phase 3 safety filter matches reliably. |
| C5 | **Muscle catalog grows 16 → 28** (existing names unchanged). |
| C6 | **Approach 1 storage:** known injuries reference the condition and read its muscles at query time (a catalog fix corrects every user's injury). Unknown injuries store the user's picks in `user_injury_muscles`. |
| C7 | **New migration `0004`** — `0003` has already run on dev + test DBs and is never edited. |
| C8 | **Conditions carry an `icd11_code`** (nullable) from WHO ICD-11; primary muscles are cross-checked against SNOMED CT "finding site" where available (reference only — no SNOMED/ICD data is imported). Secondary muscles are our clinical judgement and are labelled as such. |
| C9 | **Not medical advice:** wherever conditions appear, the UI shows a "general information, not medical advice — consult a professional" notice (implemented in Stage C). |

## Data model — migration `0004_injury_conditions_catalog.sql`
1. `body_regions (slug TEXT PK, name TEXT NOT NULL)` + 14 rows (below).
2. Normalise `user_injuries.body_region` (`lower`, spaces → `_`); **delete rows whose region still
   isn't in `body_regions`** (dev/test data only — no real users exist); then add FK
   `user_injuries.body_region → body_regions(slug)`. Same FK on `exercise_contraindications.body_region`
   (table is empty; same delete-invalid step for safety).
3. Drop 0003's `chk_muscle_id_requires_muscle_type`, `idx_user_injuries_muscle_id`, column `muscle_id`.
4. Insert 12 muscles (`ON CONFLICT (name) DO NOTHING`).
5. `injury_conditions (id BIGINT identity PK, slug TEXT UNIQUE NOT NULL, name TEXT UNIQUE NOT NULL,
   icd11_code TEXT NULL, body_region TEXT NOT NULL FK, injury_type TEXT NOT NULL CHECK 5 values)`.
6. `injury_condition_muscles (condition_id FK ON DELETE CASCADE, muscle_id FK, role TEXT CHECK
   ('primary','secondary'), PK (condition_id, muscle_id))`.
7. `user_injuries.condition_id BIGINT NULL REFERENCES injury_conditions(id)` + index.
8. `user_injury_muscles (injury_id FK ON DELETE CASCADE, muscle_id FK, PK (injury_id, muscle_id))`.
9. Seed the 17 conditions and their muscles (inserts look muscles up by name).

For known injuries, the service **copies** the condition's `body_region` and `injury_type` onto the
`user_injuries` row, so the safety filter keeps one simple JOIN on `user_injuries.body_region`.
A condition's region/type is anatomically fixed (an ACL is always in the knee); muscles — the
part most likely to be refined — are never copied. If a catalog correction ever changes a
condition's region/type, that migration also `UPDATE`s the copies.
"`user_injury_muscles` only for unknown injuries" is a cross-table rule (no SQL `CHECK`); enforced
by `InjuryService`.

### Body regions (14)
`neck` Neck · `shoulder` Shoulder · `upper_back` Upper back · `lower_back` Lower back · `chest` Chest ·
`abdomen` Abdomen · `elbow` Elbow · `wrist_hand` Wrist & hand · `hip` Hip · `groin` Groin ·
`thigh` Thigh · `knee` Knee · `lower_leg` Lower leg · `ankle_foot` Ankle & foot

### New muscles (12; group in brackets)
Supraspinatus, Infraspinatus, Teres Minor, Subscapularis (shoulders) · Forearm Flexors,
Forearm Extensors (arms) · Tibialis Anterior, Adductors, Hip Flexors, Gluteus Medius (legs) ·
Levator Scapulae, Sternocleidomastoid (neck)

### Conditions catalog (17) and sources
ICD-11 codes are from the WHO ICD-11 MMS as shown by third-party mirrors (Find-A-Code, AutoICD,
World of Taxonomy), releases v2024-01…v2026-01; **re-check each in the WHO browser
(icd.who.int/browse) before any clinical use.** "Inferred" = chosen by body site, not named in
the source.

| slug | Name | Region | Type | ICD-11 | Primary | Secondary |
|---|---|---|---|---|---|---|
| `acl_tear` | ACL tear | knee | tendon_ligament | NC93.62 (ACL rupture) | – | Quads, Hamstrings, Calves |
| `meniscus_tear` | Meniscus tear | knee | joint | NC93.3 | – | Quads, Hamstrings |
| `patellar_tendinopathy` | Patellar tendinopathy | knee | tendon_ligament | — not found | – | Quads |
| `hamstring_strain` | Hamstring strain | thigh | muscle | NC76.30 | Hamstrings | Glutes |
| `quadriceps_strain` | Quadriceps strain | thigh | muscle | NC76.10 | Quads | Hip Flexors |
| `groin_strain` | Groin strain | groin | muscle | NC76.20 (adductor) | Adductors | – |
| `hip_flexor_strain` | Hip flexor strain | hip | muscle | NC76.00 (hip muscle; inferred) | Hip Flexors | Quads |
| `calf_strain` | Calf strain | lower_leg | muscle | NC96.10 (posterior lower-leg group; inferred) | Calves | – |
| `shin_splints` | Shin splints | lower_leg | bone | — not found | – | Tibialis Anterior, Calves |
| `achilles_tendinopathy` | Achilles tendinopathy | ankle_foot | tendon_ligament | — not found (NC96.00 is acute Achilles *strain*) | – | Calves |
| `ankle_sprain` | Ankle sprain | ankle_foot | tendon_ligament | ND14.7 | – | Calves, Tibialis Anterior |
| `lower_back_strain` | Lower back strain | lower_back | muscle | NB53.5 | Erector Spinae | Glutes, Obliques |
| `rotator_cuff_strain` | Rotator cuff strain | shoulder | tendon_ligament | NC16.00 | Supraspinatus | Infraspinatus, Teres Minor, Subscapularis, Lateral Deltoid |
| `tennis_elbow` | Tennis elbow | elbow | tendon_ligament | FB55.1 | Forearm Extensors | – |
| `golfers_elbow` | Golfer's elbow | elbow | tendon_ligament | FB55.0 | Forearm Flexors | – |
| `neck_strain` | Neck strain | neck | muscle | NA23.4 | Levator Scapulae, Sternocleidomastoid | Traps |
| `pectoral_strain` | Pectoral strain | chest | muscle | NB34.0 (thorax muscle; inferred) | Pectorals | Anterior Deltoid |

SNOMED CT cross-check (reference only): *Low back strain* (300956001) → finding site *muscle of
lower back* — consistent with Erector Spinae as primary. Other primaries follow directly from the
condition's name (e.g. hamstring strain → Hamstrings). SNOMED's own data needs sanity checks too:
*Strain of flexor muscle of hip* (429722003) lists a *joint* as its finding site.
**Secondary muscles are project judgement** (rehab common sense), pending review by a physio.

## Domain
- `Injury`: `std::optional<ConditionRef> condition` (`{id, name, icd11Code?}`),
  `std::vector<InjuryMuscle> muscles` (`{muscleId, name, role}`); `muscleId` removed.
  `bodyRegion` stays `std::string` (now a slug).
- New: `BodyRegion {slug, name}`, `InjuryCondition {id, slug, name, icd11Code?, bodyRegion,
  injuryType, muscles}`; `Muscle` exists.
- `InjuryPolicy`: severity + type enum + `body_region` non-empty; cross-field muscle rule removed.
  Existence of region/muscles/condition is checked by the service (domain has no DB).

## Port — `ports::ICatalogRepository` (read-only reference data)
```cpp
virtual std::vector<domain::BodyRegion> listBodyRegions() = 0;
virtual bool bodyRegionExists(const std::string& slug) = 0;
virtual std::vector<domain::Muscle> listMuscles() = 0;
virtual std::optional<domain::Muscle> findMuscle(long long id) = 0;
virtual std::vector<domain::InjuryCondition> listConditions() = 0;
virtual std::optional<domain::InjuryCondition> findCondition(long long id) = 0;
```
One port for all reference data (replaces the July plan's `IMuscleRepository`): one fake
(`InMemoryCatalogRepository`, pre-seeded with a few regions/muscles and one condition), one
`PgCatalogRepository` + `AutoCommitPgCatalogRepository` (same two-layer pattern as users/injuries).

## Injury repository
`PgInjuryRepository` writes `condition_id` and, for unknown injuries, `user_injury_muscles`
(update replaces the set). On read it fills `condition` (JOIN) and `muscles` from the right
source: `injury_condition_muscles` when `condition_id` is set, else `user_injury_muscles` (role
`primary`). Muscles are ordered primary first, then by name. The service never sees two sources.

## Service — `InjuryService(IInjuryRepository&, ICatalogRepository&)`
`AddInjuryRequest { userId, optional conditionId, optional bodyRegion, optional injuryType,
optional<vector<long long>> muscleIds, severity, optional notes }`.
`UpdateInjuryFields` gains `optional<vector<long long>> muscleIds` (`bodyRegion`/`injuryType`
already optional).

- **Add, known:** `conditionId` set → any of `body_region`/`injury_type`/`muscle_ids` present →
  422 on that field ("cannot be set for a known condition"); condition not found → 422
  `condition_id`; copy region/type/muscles/ref from catalog.
- **Add, unknown:** missing `body_region`/`injury_type` → 422 on that field; region not in catalog
  → 422 `body_region`; any muscle id not in catalog → 422 `muscle_ids`; duplicate ids are
  de-duplicated; muscles get role `primary`.
- **Update:** ownership as today (other user's → 404). Known injury + any locked field present →
  422 on that field. Unknown: same existence checks; `muscle_ids` replaces the set (`[]` clears).
- Then `requireValidInjury`, persist.

## HTTP
- New `CatalogController`: `GET /api/v1/body-regions`, `GET /api/v1/muscles`,
  `GET /api/v1/injury-conditions` (each condition with `icd11_code`, region, type, muscles+roles).
  All 🔒. (`GET /muscles/{id}` from the July plan is dropped — nothing needs it.)
- `POST /users/me/injuries`: either `{condition_id, severity, notes?}` or
  `{body_region, injury_type, muscle_ids?, severity, notes?}`.
- `PATCH /users/me/injuries/{id}`: as in Service.
- Injury JSON: `{id, condition: {id, name, icd11_code} | null, body_region, injury_type, severity,
  muscles: [{id, name, role}], active, notes, created_at}`. (`user_id` stays as today.)
- Errors: RFC 9457 as today; 422 names the field; 404 ownership; 401 unauthenticated.

## Testing
Red → green at every level:
- **Domain:** policy without the removed rule; ligament-type injury with muscles is valid.
- **Fake catalog:** lookups; `findCondition` returns muscles with roles.
- **DB (`DbTest`):** 0004 ran — 14 regions, 28 muscles, 17 conditions; every condition has ≥ 1
  muscle and a valid region; FK rejects an unknown region; known injury reads catalog muscles;
  unknown injury reads its own muscles; deleting an injury cascades its muscle rows.
- **Service:** both shapes; every locked-field / mixed-shape case → 422 with the right field;
  unknown ids → 422; dedupe; ownership 404.
- **HTTP:** three catalog endpoints; create/list/patch for both shapes; 401.
- **Beyond tests:** `psql` check of 0004 (DB tests *skip* on a broken migration); live `curl` run;
  pre-push hook enforces 0 skipped.

## Documentation (at the end of implementation)
- `docs/02-api-contract.md`: new endpoints + injury request/response shapes.
- `docs/01-data-model.md`: new tables, body-region FK, 28 muscles.
- `docs/phase-2-slice-2-profile-injuries.md`: build-log entry.
- `docs/CONTEXT.md` (Slice 2 status + next steps) and `WORKLOG.md`.
- July spec + plan marked superseded (done with this spec).

## Out of scope (YAGNI)
Several conditions on one injury (add two injuries) · left/right side (use notes) · user-added
catalog conditions · Hebrew names · bulk SNOMED/ICD import · `GET /muscles/{id}`.
