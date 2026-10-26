# Slice 2 · Stage C — Frontend: injuries + profile (design)

**Date:** 2026-10-22 · **Status:** Approved (brainstorming, with browser mockups) · **Issue:** `#25`
**Branch:** `slice-2-profile` · **Builds on:** the injury conditions catalog API
([`2026-10-21-injury-conditions-catalog-design.md`](2026-10-21-injury-conditions-catalog-design.md))
and the profile API (`GET`/`PATCH /users/me`, [Slice 2 spec](2026-07-17-slice-2-profile-injuries-design.md) D1–D8).

## Goal
After login, users manage their **injuries** (pick a known condition or describe an unlisted one)
and their **profile**, in the same "Momentum" look as the login screen. Injuries come first: they
are what later workout building and the AI safety filter rely on.

## Decisions
| # | Decision |
|---|---|
| S1 | **Scope:** app shell + injuries screen (built first) + profile form, all in Stage C. |
| S2 | **Layout:** top bar with tabs — `Kinetiq · [Injuries] [Profile] · name · Log out` — one centred column; works on phones. |
| S3 | **Add injury:** one **searchable condition field**; the last option "My injury isn't listed…" swaps in region / type / muscle fields. |
| S4 | **Form placement:** **inline panel** (add opens at the top of the list; edit turns the card into the form). No modal. |
| S5 | **Libraries:** `react-router` (v7) + `react-hook-form` + `zod` + `@hookform/resolvers`; data via plain `fetch` helpers in `api.ts` (no TanStack Query). Exact, mutually compatible versions pinned in the plan. |
| S6 | **Rename** the app's display name **KinetiQ → Kinetiq** (22 occurrences in 13 files, display text only; identifiers are already lowercase). First build step. |
| S7 | **Server stays the authority:** Zod gives instant feedback, but 422 field errors from the server are always shown under the matching field. |
| S8 | **Not medical advice:** a short notice under the injury form and inside the locked condition box. |

## App shell & navigation
```
App.tsx               session restore + login screen (unchanged role)
 └─ AppShell          top bar + <Outlet/> for the current page
     ├─ /injuries     InjuriesPage   ("/" and unknown paths redirect here)
     └─ /profile      ProfilePage
```
- `App.tsx` renders a `BrowserRouter` with the routes once logged in, replacing the walking-skeleton
  view. The health badge stays on the login screen only.
- Tabs are `NavLink`s; the active one gets the coral highlight.
- `app-theme.css`: shares the Momentum colours with `auth-theme.css` via CSS variables.
- **Expired session:** any **401** from an authenticated call logs the user out and the login
  screen shows "Your session expired — please log in again."

## Injuries screen (`/injuries`) — `Client/src/features/injuries/`
| Component | Job |
|---|---|
| `InjuriesPage` | On open, loads injuries + catalog (conditions, body regions, muscles). Shows active injuries, "+ Add injury", and a collapsed **"Healed (n)"** section for inactive ones. |
| `InjuryCard` | Condition name (or "Other — <region>"), region, severity, muscles as pills (primary = coral, secondary = grey). Actions: **Edit · Mark healed · Delete** (Delete asks "Delete this injury?" first). Healed cards show **Mark active** instead of Mark healed. |
| `InjuryForm` | Inline form for **add** and **edit**. |
| `ConditionSearch` | Text input filtering the 17 conditions (by name, case-insensitive); results are buttons; **"My injury isn't listed…"** is always last. |
| `MuscleSelect` | Muscle chips grouped by `muscle_group`, toggle on tap. Unlisted injuries only. |

**`InjuryForm` rules** (mirroring the server):
- **Known condition chosen:** locked box shows its muscles (role), region, type and ICD-11 code (or
  "no ICD-11 code"); only **severity** + **notes** are editable. Sends `{condition_id, severity, notes?}`.
- **Unlisted:** region (dropdown of 14), type (5 buttons), muscles (`MuscleSelect`, optional),
  severity, notes. Sends `{body_region, injury_type, muscle_ids, severity, notes?}`.
- **Editing a known injury:** condition shown locked with "Delete and re-add to change the
  condition"; PATCH sends only `severity` / `notes`. **Editing an unlisted injury:** PATCH sends the
  changed fields (`muscle_ids` replaces the set).
- **Zod:** severity required; notes ≤ 500 chars; region + type required when unlisted.
- Mark healed / Mark active = `PATCH {active:false|true}`; Delete = `DELETE`.

## Profile screen (`/profile`) — `Client/src/features/profile/`
`ProfilePage` loads `GET /users/me`; `ProfileForm` is pre-filled and grouped:

| Group | Fields |
|---|---|
| Body | `height_cm`, `weight_kg`, `birth_date`, `sex` |
| Training | `experience_level`, `weekly_workout_goal`, `available_time_minutes` |
| Other | `medications`, `unit_preference` |

**Zod mirrors `ProfilePolicy`:** height 0 < h ≤ 300; weight 0 < w ≤ 500; `birth_date` a valid
`YYYY-MM-DD`; sex ∈ male/female/other/unspecified; experience ∈ beginner/intermediate/advanced;
`weekly_workout_goal` ≥ 0 (integer); `available_time_minutes` > 0 (integer); unit ∈ metric/imperial.
- **Save** sends only **changed** fields (React Hook Form dirty fields) as `PATCH`; success message;
  server 422s shown under the field.
- **Clearing a saved value** isn't supported by the server (D3, absent = unchanged): emptying such a
  field shows "Clearing a saved value isn't supported yet" instead of silently doing nothing.
- **Metric only**; the unit dropdown notes that imperial display comes later.
- Display name / email are account data, not on this form.

## API layer — `Client/src/api.ts`
- `authFetch(path, options)`: adds `Authorization: Bearer <token>`, JSON body/headers; non-2xx →
  throws `ApiError { status, detail, fieldErrors: Record<string, string[]> }` (from the RFC 9457
  body); on **401** also notifies an `onUnauthorized` listener that `App` registers.
- Functions: `getProfile`, `updateProfile`, `listInjuries`, `addInjury`, `updateInjury`,
  `deleteInjury`, `listBodyRegions`, `listMuscles`, `listConditions`.
- Types mirror the server JSON exactly (`Injury`, `ConditionRef | null`, `InjuryMuscle`,
  `InjuryCondition`, `BodyRegion`, `Muscle`, `Profile`, `AddInjuryRequest` as a union of the two shapes).

## Testing
Vitest + Testing Library + **MSW** (same as the login tests). MSW handlers for all 9 endpoints use a
small catalog matching the server tests (hamstring strain, 3 regions, 4 muscles).
- Shell: tabs switch pages; active tab highlighted; unknown path → injuries; 401 → logged out with message.
- Injuries: list + healed section; add known condition (locked muscles, correct payload); add
  unlisted (fields appear, correct payload); Zod errors before sending; server 422 under the field;
  edit; mark healed / active; delete with confirmation.
- Profile: pre-filled; Zod errors; only changed fields sent; clearing a saved value blocked; 422 shown.
- The git hooks run the client suite on every client commit and on push.

**Live verification (end):** Playwright drives a real browser against the real server + Postgres —
log in, add ACL tear, add an unlisted wrist injury, edit, heal, delete, save profile — then the test
user is deleted.

## Build order
1. Rename KinetiQ → Kinetiq (S6).
2. Libraries + `api.ts` (`authFetch`, types, functions) + MSW handlers.
3. `AppShell` + routing + 401 logout.
4. Injuries: `InjuryCard` + list → `ConditionSearch` + `InjuryForm` (known) → unlisted + `MuscleSelect` → edit / heal / delete.
5. Profile form.
6. Browser verification + docs (`CONTEXT.md`, `WORKLOG.md`, slice build log, `docs/backlog.md`) →
   merge `slice-2-profile` → `main` (ends Slice 2).

## Deferred — later, not never
Collected in a new `docs/backlog.md` (single list of future ideas across the project), including:
imperial display · editing display name / email · several conditions per injury · left/right side
· muscle search box · offline support · TanStack Query · renaming the GitHub repository (optional;
GitHub redirects old links).
