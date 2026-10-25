# Stage C additions — themes, units, clearing values + ops (design)

**Date:** 2026-10-25 · **Status:** Approved (brainstorming, with browser mockups) · **Branch:** `slice-2-profile`
**Extends:** [`2026-10-22-stage-c-frontend-design.md`](2026-10-22-stage-c-frontend-design.md) (built before
its Task 9, so Task 9's browser check covers these too).
**Supersedes:** Slice 2 decision **D3** ("absent = unchanged, no null-clearing in v1") in
[`2026-07-17-slice-2-profile-injuries-design.md`](2026-07-17-slice-2-profile-injuries-design.md).

## Scope
Owner asked for the small/medium items now; large ones go to `docs/backlog.md`:
- **Now:** light theme + night mode, imperial units, clearing saved values (with profile history),
  ops (GitHub repo rename, `docker compose` fix, `npm audit` review, ADRs).
- **Backlog:** forgot password (needs email infra), subscription idea, user-agreement disclosure,
  "delete my data".

## Decisions
| # | Decision |
|---|---|
| T1 | **Light by default when the device is light; night when it's dark** (`prefers-color-scheme`); a ☾/☀ toggle overrides and is remembered in `localStorage`. |
| T2 | **Monochrome accent:** white in night mode, near-black in light mode. Errors stay red in both. |
| T3 | Primary vs secondary muscle pills are distinguished by **fill** (primary filled, secondary outlined), not colour. |
| U1 | **Server always stores metric**; only the UI converts. |
| U2 | **Units switch in the top bar** (`Metric | Imperial`) — whole app, all metric or all imperial; saved to the account (`unit_preference`); optimistic with rollback on failure. The profile form's Units dropdown is removed. |
| U3 | **One box per field:** imperial height = total inches (hint "= 5 ft 10 in"); weight = lb. |
| C1 | **JSON Merge Patch (RFC 7396)** for `PATCH /users/me` and injury `notes`: absent = unchanged, value = set, **`null` = clear**. `unit_preference` can't be cleared (422). |
| C2 | **Profile history** (`user_profile_history`, migration `0006`): every changed profile field is recorded (old/new value, time), so a cleared value's **last known value** is kept for future training plans. |
| C3 | The UI says so when a value is cleared ("Kinetiq will keep using your last saved weight (74 kg) for training plans"); the user agreement must disclose it (backlog). |

## 1. Themes
- **Tokens:** all colours are CSS variables, defined twice — `:root[data-theme="light"]` and
  `:root[data-theme="dark"]`. `app-theme.css` and `auth-theme.css` both use them (the login
  screen follows the theme).

| Token | Light | Night |
|---|---|---|
| `--bg` | `#faf8f5` | `#14161a` |
| `--surface` | `#ffffff` | `#1b1e23` |
| `--surface-2` | `#f1eeea` | `rgba(255,255,255,0.045)` |
| `--ink` | `#1d1b19` | `#f1efec` |
| `--muted` | `#6b655e` | `#a8a29b` |
| `--line` | `#e7e2dc` | `rgba(241,239,236,0.1)` |
| `--accent` | `#1d1b19` | `#ffffff` |
| `--accent-ink` (text on accent) | `#ffffff` | `#14161a` |
| `--accent-soft` | `#ebe7e2` | `rgba(255,255,255,0.12)` |
| `--error` | `#c62828` | `#ff6b6b` |

- **Choosing:** `lib/theme.ts` — `getInitialTheme()` (saved choice, else device preference),
  `applyTheme(t)` (sets `document.documentElement.dataset.theme`, saves it), `toggleTheme()`.
- **No flash:** a tiny inline script in `index.html` applies the theme before React loads.
- **Toggle:** `ThemeToggle` button ("☾ Night" / "☀ Light") in the top bar and on the login screen.
- **Pills:** `.pill-primary` filled (`--accent` bg, `--accent-ink` text); `.pill-secondary`
  outlined (`--line` border, `--muted` text).

## 2. Units
- `lib/units.ts` (pure, tested): `cmToIn`, `inToCm`, `kgToLb`, `lbToKg`, `formatFeetInches(cm)`
  ("5 ft 10 in"). 1 in = 2.54 cm; 1 lb = 0.45359237 kg. Display 1 decimal; values sent to the
  server rounded to 0.1 (cm / kg).
- **State:** the current unit system comes from the user's profile (`unit_preference`); a small
  `UnitsContext` provides `{ units, setUnits }` to the shell and pages. `setUnits` updates the UI
  immediately, then `PATCH {unit_preference}`; on failure it reverts and shows an error.
  The shell loads the profile once on login to know the starting unit.
- **Top bar:** `UnitsToggle` — two buttons `Metric | Imperial` (`aria-pressed` on the active one).
- **Profile form:** labels/inputs follow the units (`Height (cm)`/`Height (in)`, `Weight (kg)`/
  `Weight (lb)`); imperial height shows the `= 5 ft 10 in` hint. Switching units re-displays the
  saved values converted. Edited values are converted to metric before sending. **Only edited
  fields are sent** — switching units alone never re-sends height/weight (no rounding drift).
- **Validation:** convert to metric, apply the same limits (height ≤ 300 cm, weight ≤ 500 kg);
  messages in the active units ("at most 118.1 in", "at most 1102.3 lb").

## 3. Clearing values + history
**Server (TDD):**
- `app::ProfileUpdate` gains `std::set<std::string> clear` (field wire names). `parseProfileUpdate`:
  a key present with JSON `null` → added to `clear` (not parsed as a value).
- `UpdateProfile`: `unit_preference` in `clear` → `ProfileValidationError("unit_preference", …)`
  (422). Otherwise clear those fields, apply set values, validate, save, then record history.
- **History:** port `IProfileHistoryRepository { void record(long long userId, const std::string&
  field, const std::optional<std::string>& oldValue, const std::optional<std::string>& newValue); }`
  + `InMemoryProfileHistoryRepository` fake + `PgProfileHistoryRepository` +
  `AutoCommitPgProfileHistoryRepository`. `UpdateProfile(users, history)` compares before/after
  and records each changed field among the 8 tracked ones (not `unit_preference`). Same value →
  no row. Known limitation (unchanged pattern): profile save and history write are not one atomic
  transaction.
- Migration `0006_user_profile_history.sql`:
  `user_profile_history (id BIGINT identity PK, user_id BIGINT NOT NULL REFERENCES users(id) ON
  DELETE CASCADE, field TEXT NOT NULL CHECK (field IN (8 names)), old_value TEXT, new_value TEXT,
  changed_at TIMESTAMPTZ NOT NULL DEFAULT now())` + index on `(user_id, field, changed_at)`.
- Injuries: `parseUpdateInjury` treats `"notes": null` as clear (empty notes), same as `""` today.
- Reading history back (last-known lookup, weight chart, endpoint) is **not built now** — Phase 3.

**Client:**
- `toProfileUpdate` sends `null` for an emptied saved field (no more `cleared` list / blocking
  message). "Not set" in a dropdown on a saved value = clear.
- After saving with cleared fields, the status names them with the last value, e.g.
  "Weight cleared. Kinetiq will keep using your last saved weight (74 kg) for training plans."

## 4. Ops
- **Repo rename:** `gh repo rename Kinetiq`; `git remote set-url origin https://github.com/itaim900/Kinetiq.git`;
  update doc links `itaim900/KinetiQ` → `itaim900/Kinetiq`. Image names use
  `github.repository_owner` → unaffected.
- **Compose fix:** `docker-compose.yml` gets top-level `name: kinetiq` and the volume an explicit
  `name: fitapp_kinetiq_pgdata` (keeps existing data). Remove the old container
  (`docker rm -f kinetiq-postgres` — data lives in the volume), then `docker compose up -d`;
  verify user/injury counts unchanged. Update `CONTEXT.md` (drop the workaround).
- **`npm audit`:** apply non-breaking fixes; record the rest (with reasons) in the backlog.
- **ADRs 0013–0017:** authenticator function not Crow middleware · load-modify-save PATCH ·
  AutoCommit repository pattern · conditions catalog with locked known conditions · merge-patch
  clearing + profile history.

## Testing
- `theme.ts` (initial theme from storage / device; apply + persist), `ThemeToggle` (switches
  `data-theme`, label flips).
- `units.ts` conversions + round trips; `UnitsToggle` (aria-pressed, PATCH sent, rollback on
  error); profile form in imperial (labels, hint, conversion on save, only edited fields sent,
  limits in imperial).
- Server: DTO null → clear; service clears, rejects clearing `unit_preference`, records history
  only for changed fields; Pg history adapter writes rows; HTTP round trip.
- Client: emptied saved field sends `null` + shows the "last saved" note.
- Task 9 browser check adds: theme toggle (both screens), units switch, clearing a value.

## Build order
1. Ops (rename, compose, audit, ADRs) 2. Themes 3. Units 4. Clearing (server → client)
5. Stage C Task 9 (browser check, docs, backlog) → merge `slice-2-profile` → `main` (owner's go-ahead).
