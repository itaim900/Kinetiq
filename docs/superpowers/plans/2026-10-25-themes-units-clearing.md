# Themes, Units, Clearing Values + Ops Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Light/night themes with a monochrome accent, an app-wide Metric | Imperial switch, clearable profile values backed by a profile history, plus the repo rename, Compose fix, `npm audit` review and five ADRs.

**Architecture:** Client: colour tokens as CSS variables switched by `data-theme` on `<html>` (`lib/theme.ts`); units as a React context (`lib/UnitsContext.tsx`) fed by the profile and saved via `PATCH {unit_preference}`; pure `lib/units.ts` conversions; the profile form becomes units-aware. Server: `PATCH /users/me` adopts JSON Merge Patch (`null` = clear) and records every changed profile field through a new `IProfileHistoryRepository` port (migration `0006`).

**Tech Stack:** React 18 + TS + Vite, React Hook Form + Zod, Vitest + MSW; C++20 / Crow / libpqxx / GoogleTest; Docker Compose; `gh` CLI.

**Spec:** [`docs/superpowers/specs/2026-10-25-themes-units-clearing-design.md`](../specs/2026-10-25-themes-units-clearing-design.md)

## Global Constraints
- Server stores **metric only**; conversions happen in the client. 1 in = 2.54 cm; 1 lb = 0.45359237 kg; display and send rounded to 0.1.
- Accent: **white** in night, **near-black** `#1d1b19` in light; errors red (`#c62828` light / `#ff6b6b` night).
- Theme stored in `localStorage` key `kinetiq_theme` (`'light' | 'dark'`); default = device `prefers-color-scheme`.
- Units are app-wide, saved as the account's `unit_preference`; switching units never re-sends unedited height/weight.
- `PATCH /users/me`: absent = unchanged, value = set, `null` = clear. `unit_preference: null` → 422 on `unit_preference`.
- History tracks exactly: `sex`, `birth_date`, `height_cm`, `weight_kg`, `experience_level`, `weekly_workout_goal`, `available_time_minutes`, `medications`.
- Never edit applied migrations; new one is `0006_user_profile_history.sql`.
- Every commit green (hooks). New server `.cpp` files must be added to `Server/CMakeLists.txt`.
- External actions (repo rename) only as described in Task 1.

---

### Task 1: Rename the GitHub repository to `Kinetiq`

**Files:** Modify: `README.md`, `docs/05-roadmap.md`, `docs/CONTEXT.md`, `docs/phase-1-summary.md`

- [ ] **Step 1: Check the current state**
```bash
gh repo view itaim900/KinetiQ --json name,url -q '.name + " " + .url'
git remote get-url origin
```
Expected: `KinetiQ https://github.com/itaim900/KinetiQ`; remote `https://github.com/itaim900/KinetiQ.git`.

- [ ] **Step 2: Rename on GitHub and repoint the local remote**
```bash
gh repo rename Kinetiq --repo itaim900/KinetiQ --yes
git remote set-url origin https://github.com/itaim900/Kinetiq.git
git ls-remote origin HEAD
gh repo view itaim900/Kinetiq --json name -q .name
```
Expected: `ls-remote` prints a commit hash; name `Kinetiq`.

- [ ] **Step 3: Update doc links**
```bash
for f in README.md docs/05-roadmap.md docs/CONTEXT.md docs/phase-1-summary.md; do sed -i 's#itaim900/KinetiQ#itaim900/Kinetiq#g' "$f"; done
git grep -n "KinetiQ" -- ':!docs/superpowers' ':!WORKLOG.md'
```
Expected: no output (dated specs/plans/worklog are history and stay as written).
Image names use `${{ github.repository_owner }}` (`.github/workflows/images.yml`) → unaffected.

- [ ] **Step 4: Commit**
```bash
git add README.md docs/05-roadmap.md docs/CONTEXT.md docs/phase-1-summary.md
git commit -m "chore: GitHub repo renamed to itaim900/Kinetiq; update links"
```

---

### Task 2: Fix `docker compose up -d` after the folder rename

**Files:** Modify: `docker-compose.yml`, `docs/CONTEXT.md`

- [ ] **Step 1: Record the data to verify against later**
```bash
docker exec kinetiq-postgres psql -U kinetiq -d kinetiq -tAc "select (select count(*) from users)||' users, '||(select count(*) from user_injuries)||' injuries, '||(select count(*) from schema_migrations)||' migrations'"
```
Note the output (e.g. `2 users, 1 injuries, 5 migrations`).

- [ ] **Step 2: Give the project a fixed name and the volume an explicit name** — in `docker-compose.yml`
add as the first line:
```yaml
name: kinetiq
```
and replace the bottom `volumes:` block with:
```yaml
volumes:
  kinetiq_pgdata:
    # Fixed name so the project no longer depends on the folder name.
    name: kinetiq_pgdata
```
Also update the comment block at the top if it mentions the folder.

- [ ] **Step 3: Move to the new container + volume without losing data**
```bash
docker stop kinetiq-postgres && docker rm kinetiq-postgres     # old container; data stays in fitapp_kinetiq_pgdata
docker compose create                                          # creates empty kinetiq_pgdata + new container (not started)
docker run --rm -v fitapp_kinetiq_pgdata:/from:ro -v kinetiq_pgdata:/to postgres:16 bash -c "cp -a /from/. /to/"
docker compose start
```
Wait until `docker inspect -f '{{.State.Health.Status}}' kinetiq-postgres` is `healthy`.

- [ ] **Step 4: Verify** — rerun Step 1's query → **identical** output. `docker compose ps` lists
`kinetiq-postgres` as healthy under project `kinetiq`. `docker compose down && docker compose up -d`
works and keeps the data (rerun the query). The old volumes `fitapp_kinetiq_pgdata`,
`fitapp_fitapp_pgdata`, `kinetiq_kinetiq_pgdata` are **left in place** as a backup (backlog: delete later).

- [ ] **Step 5: Update `docs/CONTEXT.md`** — replace the "Repo folder was renamed `Fit App` → `Kinetiq`…"
bullet with: "**Compose project is named `kinetiq`** (fixed `name:` in `docker-compose.yml`); data
volume `kinetiq_pgdata` (copied from the old `fitapp_kinetiq_pgdata`, which is kept as a backup).
`docker compose up -d` works again."

- [ ] **Step 6: Commit**
```bash
git add docker-compose.yml docs/CONTEXT.md
git commit -m "fix(ops): fixed Compose project name + data volume; compose up works after folder rename"
```

---

### Task 3: Review `npm audit`

**Files:** Modify: `Client/package-lock.json` (and `package.json` only if a fix requires it)

- [ ] **Step 1: List** — from `Client/`: `npm audit` and `npm audit --omit=dev`. Note each advisory:
package, severity, whether it's dev-only (Vitest/Vite/jsdom tooling) or shipped to users.
- [ ] **Step 2: Apply safe fixes** — `npm audit fix` (never `--force`). Then `npm test`, `npx tsc --noEmit`,
`npm run build` → all green.
- [ ] **Step 3: Record leftovers** — anything not fixable without a breaking upgrade goes into the
backlog list for Task 12 with: package, severity, dev-only?, and the upgrade it needs.
- [ ] **Step 4: Commit** (only if the lock file changed)
```bash
git add Client/package-lock.json Client/package.json
git commit -m "chore(client): npm audit fix (non-breaking)"
```

---

### Task 4: ADRs 0013–0017

**Files:** Create: `docs/adr/0013-authenticator-function-not-middleware.md`, `0014-load-modify-save-patch.md`, `0015-autocommit-repository-pattern.md`, `0016-injury-conditions-catalog.md`, `0017-merge-patch-clearing-and-profile-history.md`; Modify: `docs/adr/README.md`

- [ ] **Step 1: Write the five ADRs** (same format as 0011: Status/Date, Context, Options considered, Decision, Consequences).

`0013-authenticator-function-not-middleware.md`:
```markdown
# ADR-0013: Authentication is a plain function, not Crow middleware

**Status:** Accepted · **Date:** 2026-07-16 (recorded 2026-10-25)

## Context
Protected routes need the current user from `Authorization: Bearer <jwt>`. HTTP tests drive Crow
in-process via `app.handle_full()` (no socket) — and `handle_full()` does **not** run middleware
(global or route-local); a route declaring middleware even dereferences an uninitialised context.

## Options considered
- **Crow middleware** — idiomatic, but untestable in-process; would force socket-based tests.
- **Socket-based HTTP tests** — real middleware, but slow and flaky (ports, startup).
- **A plain function handlers call** — `authenticate(req, tokens) -> optional<CurrentUser>`.

## Decision
`http::authenticate()` in `http/middleware/Authenticator.h`; every protected handler calls it first
and returns 401 on `nullopt`. No Crow middleware on routes.

## Consequences
- **Positive:** the whole request path (routing → auth → service → DTO) is tested in-process, fast.
- **Negative:** each handler must remember the call (one line); a forgotten call isn't caught by
  the framework — every protected route has a 401 test to compensate.
```

`0014-load-modify-save-patch.md`:
```markdown
# ADR-0014: PATCH = load, modify, validate, save (not dynamic SQL)

**Status:** Accepted · **Date:** 2026-07-17 (recorded 2026-10-25)

## Context
`PATCH /users/me` and injury PATCH update a subset of fields. Two ways: build an `UPDATE … SET`
with only the provided columns, or load the entity, apply the provided fields in C++, validate the
whole result, and save all columns.

## Options considered
- **Dynamic SQL** — touches only changed columns; validation would have to reason about partial data.
- **Load-modify-save** — the domain policy validates a complete entity; repository has one `update`.

## Decision
Load-modify-save in the app service (`UpdateProfile`, `InjuryService::update`), validating the
complete entity with the domain policy before persisting.

## Consequences
- **Positive:** one validation path for create and update; simple repositories; easy fakes.
- **Negative:** last-write-wins between concurrent requests for the same user (accepted for v1);
  load and save are separate statements (see ADR-0015).
```

`0015-autocommit-repository-pattern.md`:
```markdown
# ADR-0015: Two-layer repositories (Pg… + AutoCommit…)

**Status:** Accepted · **Date:** 2026-07-16 (recorded 2026-10-25)

## Context
Crow routes capture their dependencies once at startup and serve concurrent requests on many
threads. A `pqxx::work` (transaction) is per-operation and must not be shared across threads.
Integration tests, however, want one transaction per test that is rolled back afterwards.

## Options considered
- **One shared connection/transaction** — unsafe across concurrent requests.
- **Thread a transaction through each request** — correct atomicity, but nothing in Crow supports it
  and every service signature would change.
- **Two layers** — `PgXRepository(pqxx::work&)` for SQL; `AutoCommitPgXRepository(connString)`
  opens a connection + transaction per call and delegates.

## Decision
Every repository has both layers. Tests use `PgX` inside `DbTest`'s rolled-back transaction;
`main.cpp` wires `AutoCommitPgX`.

## Consequences
- **Positive:** thread-safe in production; isolated, fast DB tests; same SQL in both.
- **Negative:** multi-step use cases (e.g. profile save + history write) are not one atomic
  transaction — a crash between steps can leave them inconsistent. Accepted for v1; revisit with a
  unit-of-work if it ever matters.
```

`0016-injury-conditions-catalog.md`:
```markdown
# ADR-0016: Injuries via a conditions catalog (known = locked; unknown = user-described)

**Status:** Accepted · **Date:** 2026-10-21 (recorded 2026-10-25) · Supersedes the single
`muscle_id` design (spec 2026-07-25, E2/E4/E5)

## Context
Injuries should reflect their real effect on the body: several muscles, with a role. No open
database maps injuries → affected muscles (SNOMED CT only gives the primary finding site; ICD-11
gives codes, no muscles).

## Options considered
- **User picks muscles freely** — simple, but accuracy depends on the user's anatomy knowledge.
- **Catalog only** — accurate for known conditions, no way to describe anything else.
- **Catalog + unknown fallback** — known conditions are locked; unknown injuries are described
  from fixed lists.

## Decision
`injury_conditions` (17, ICD-11-anchored) mapped to muscles with `primary|secondary` roles.
A known injury references its condition and reads muscles from the catalog (fixing the catalog
fixes every user's injury). Unknown injuries store user-picked muscles in `user_injury_muscles`.
Body region is a fixed list (FK) shared with exercise contraindications.

## Consequences
- **Positive:** trustworthy data for the Phase 3 safety filter; one place to correct medical content.
- **Negative:** the catalog is project-authored (secondary muscles are judgement) and needs physio
  review; the UI must show a not-medical-advice notice.
```

`0017-merge-patch-clearing-and-profile-history.md`:
```markdown
# ADR-0017: JSON Merge Patch for clearing + an append-only profile history

**Status:** Accepted · **Date:** 2026-10-25 · Supersedes Slice 2 decision D3 (no null-clearing)

## Context
Users need to clear saved profile values. The owner also wants the app to keep using the last
known value (e.g. weight) for training after a field is cleared — and to disclose that.

## Options considered
- **Explicit "clear" list in the body** — non-standard.
- **JSON Merge Patch (RFC 7396)** — absent = unchanged, value = set, `null` = clear.
- For keeping old values: **`last_known_*` columns** (one value, no dates) vs an **append-only
  history table** (every change with a timestamp).

## Decision
`PATCH /users/me` follows JSON Merge Patch; `unit_preference` can't be cleared (422). Every changed
profile field is recorded in `user_profile_history` (old value, new value, time) through
`IProfileHistoryRepository`. Injury `notes: null` also clears.

## Consequences
- **Positive:** standard, predictable API; nothing is lost when a value is cleared; history enables
  future progress features (e.g. weight over time).
- **Negative:** cleared data is still retained → must be disclosed in the user agreement and fully
  erased by a future "delete my data" (backlog). History write is not atomic with the profile save
  (ADR-0015).
```

- [ ] **Step 2: Add rows to the index in `docs/adr/README.md`**
```markdown
| [0013](./0013-authenticator-function-not-middleware.md) | Auth is a plain function, not Crow middleware | Accepted |
| [0014](./0014-load-modify-save-patch.md) | PATCH = load-modify-validate-save | Accepted |
| [0015](./0015-autocommit-repository-pattern.md) | Two-layer repositories (Pg + AutoCommit) | Accepted |
| [0016](./0016-injury-conditions-catalog.md) | Injuries via conditions catalog (known = locked) | Accepted |
| [0017](./0017-merge-patch-clearing-and-profile-history.md) | JSON Merge Patch clearing + profile history | Accepted |
```

- [ ] **Step 3: Commit**
```bash
git add docs/adr
git commit -m "docs(adr): 0013-0017 -- authenticator, load-modify-save, AutoCommit repos, conditions catalog, merge-patch + history"
```

---

### Task 5: Light/night themes + monochrome accent

**Files:**
- Create: `Client/src/theme.css`, `Client/src/lib/theme.ts`, `Client/src/lib/theme.test.ts`, `Client/src/ThemeToggle.tsx`, `Client/src/ThemeToggle.test.tsx`
- Modify: `Client/index.html`, `Client/src/main.tsx`, `Client/src/app-theme.css`, `Client/src/features/auth/auth-theme.css`, `Client/src/AppShell.tsx`, `Client/src/App.tsx`

**Interfaces:** Produces `type Theme = 'light' | 'dark'`, `getInitialTheme(): Theme`, `applyTheme(t: Theme, persist?: boolean): void`, `currentTheme(): Theme`, `<ThemeToggle />`.

- [ ] **Step 1: Write the failing tests**

`Client/src/lib/theme.test.ts`:
```ts
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { applyTheme, currentTheme, getInitialTheme } from './theme';

describe('theme', () => {
  beforeEach(() => localStorage.clear());
  afterEach(() => vi.unstubAllGlobals());

  it('uses the saved choice first', () => {
    localStorage.setItem('kinetiq_theme', 'dark');
    expect(getInitialTheme()).toBe('dark');
  });

  it('falls back to the device preference', () => {
    vi.stubGlobal('matchMedia', (q: string) => ({ matches: q.includes('dark') }));
    expect(getInitialTheme()).toBe('dark');
  });

  it('defaults to light when nothing is known', () => {
    vi.stubGlobal('matchMedia', undefined);
    expect(getInitialTheme()).toBe('light');
  });

  it('applies and remembers a theme', () => {
    applyTheme('dark');
    expect(document.documentElement.dataset.theme).toBe('dark');
    expect(currentTheme()).toBe('dark');
    expect(localStorage.getItem('kinetiq_theme')).toBe('dark');
  });
});
```
`Client/src/ThemeToggle.test.tsx`:
```tsx
import { fireEvent, render, screen } from '@testing-library/react';
import { beforeEach, expect, it } from 'vitest';
import ThemeToggle from './ThemeToggle';
import { applyTheme } from './lib/theme';

beforeEach(() => {
  localStorage.clear();
  applyTheme('light', false);
});

it('switches between light and night and says which mode it offers', () => {
  render(<ThemeToggle />);
  fireEvent.click(screen.getByRole('button', { name: /switch to night mode/i }));
  expect(document.documentElement.dataset.theme).toBe('dark');
  expect(screen.getByRole('button', { name: /switch to light mode/i })).toHaveTextContent('☀ Light');
  expect(localStorage.getItem('kinetiq_theme')).toBe('dark');
});
```

- [ ] **Step 2: Run to verify it fails** — `npx vitest run src/lib/theme.test.ts src/ThemeToggle.test.tsx`
Expected: FAIL — `Failed to resolve import "./theme"`.

- [ ] **Step 3: `lib/theme.ts`**
```ts
export type Theme = 'light' | 'dark';
const STORAGE_KEY = 'kinetiq_theme';

// Saved choice wins; otherwise follow the device (prefers-color-scheme).
export function getInitialTheme(): Theme {
  try {
    const saved = localStorage.getItem(STORAGE_KEY);
    if (saved === 'light' || saved === 'dark') return saved;
  } catch {
    // storage unavailable -- fall through
  }
  const prefersDark =
    typeof window.matchMedia === 'function' &&
    window.matchMedia('(prefers-color-scheme: dark)').matches;
  return prefersDark ? 'dark' : 'light';
}

// Sets data-theme on <html> (CSS swaps every colour token) and, by default,
// remembers the choice so it wins over the device setting next time.
export function applyTheme(theme: Theme, persist = true): void {
  document.documentElement.dataset.theme = theme;
  if (persist) {
    try {
      localStorage.setItem(STORAGE_KEY, theme);
    } catch {
      // ignore
    }
  }
}

export function currentTheme(): Theme {
  return document.documentElement.dataset.theme === 'dark' ? 'dark' : 'light';
}
```

- [ ] **Step 4: `ThemeToggle.tsx`**
```tsx
import { useState } from 'react';
import { applyTheme, currentTheme, type Theme } from './lib/theme';

export default function ThemeToggle() {
  const [theme, setTheme] = useState<Theme>(currentTheme());
  const next: Theme = theme === 'dark' ? 'light' : 'dark';
  return (
    <button
      type="button"
      className="theme-toggle"
      aria-label={`Switch to ${next === 'dark' ? 'night' : 'light'} mode`}
      onClick={() => {
        applyTheme(next);
        setTheme(next);
      }}
    >
      {theme === 'dark' ? '☀ Light' : '☾ Night'}
    </button>
  );
}
```

- [ ] **Step 5: Colour tokens — `Client/src/theme.css`**
```css
/* Colour tokens. data-theme on <html> (set by lib/theme.ts and the inline
   script in index.html) picks the set; every other stylesheet uses var(--…). */
:root,
:root[data-theme='light'] {
  --bg: #faf8f5;
  --surface: #ffffff;
  --surface-2: #f1eeea;
  --ink: #1d1b19;
  --muted: #6b655e;
  --line: #e7e2dc;
  --accent: #1d1b19;
  --accent-ink: #ffffff;
  --accent-soft: #ebe7e2;
  --error: #c62828;
  --error-soft: rgba(198, 40, 40, 0.08);
  --glow: #d8d1c8;
  --glow-2: #e9e3db;
  color-scheme: light;
}

:root[data-theme='dark'] {
  --bg: #14161a;
  --surface: #1b1e23;
  --surface-2: rgba(255, 255, 255, 0.045);
  --ink: #f1efec;
  --muted: #a8a29b;
  --line: rgba(241, 239, 236, 0.1);
  --accent: #ffffff;
  --accent-ink: #14161a;
  --accent-soft: rgba(255, 255, 255, 0.12);
  --error: #ff6b6b;
  --error-soft: rgba(255, 107, 107, 0.12);
  --glow: #ffffff;
  --glow-2: #8a8f99;
  color-scheme: dark;
}

body { margin: 0; background: var(--bg); color: var(--ink); }
.theme-toggle { font: inherit; background: none; border: 1px solid var(--line); color: var(--ink); border-radius: 999px; padding: 0.25rem 0.7rem; cursor: pointer; }
```

- [ ] **Step 6: Point the existing stylesheets at the tokens**

`app-theme.css`: replace the `:root { --app-…}` block with
```css
:root {
  --app-bg: var(--bg);
  --app-surface: var(--surface);
  --app-surface-2: var(--surface-2);
  --app-accent: var(--accent);
  --app-accent-soft: var(--accent-soft);
  --app-ink: var(--ink);
  --app-muted: var(--muted);
  --app-line: var(--line);
}
```
and change these rules:
```css
.field-error { color: var(--error); font-size: 0.85rem; margin: 0.25rem 0; }
.app button.primary { background: var(--accent); color: var(--accent-ink); border: none; border-radius: 8px; padding: 0.45rem 1rem; font-weight: 600; cursor: pointer; }
.pill-primary { background: var(--accent); color: var(--accent-ink); border: 1px solid var(--accent); }
.pill-secondary { background: transparent; color: var(--muted); border: 1px solid var(--line); }
.auth-notice { color: var(--ink); }
```
`auth-theme.css`: replace the 8 `--auth-*` declarations at the top of `.auth-backdrop` with
```css
  --auth-bg: var(--bg);
  --auth-bg-2: var(--surface);
  --auth-accent: var(--glow);
  --auth-accent-dim: var(--glow-2);
  --auth-ink: var(--ink);
  --auth-muted: var(--muted);
  --auth-line: var(--line);
  --auth-surface: var(--surface-2);
```
(`--auth-accent` only drives the background glow blobs and a few links/outlines — use `--accent`
for those below.) Then substitute these literal values:
```bash
cd Client/src/features/auth
sed -i 's/color: #1a0d08;/color: var(--accent-ink);/; s/rgba(255, 106, 77, 0.12)/var(--error-soft)/; s/rgba(255, 106, 77, 0.35)/var(--error)/; s/color: #ffb3a1;/color: var(--error);/; s/rgba(255, 255, 255, 0.05)/var(--surface-2)/; s/rgba(241, 239, 236, 0.35)/var(--muted)/; s/rgba(255, 255, 255, 0.07)/var(--surface-2)/' auth-theme.css
sed -i '/button\[type="submit"\] {/,/}/ s/background: var(--auth-accent);/background: var(--accent);/; /:focus/ s/var(--auth-accent)/var(--accent)/; /auth-links button {/,/}/ s/color: var(--auth-accent);/color: var(--ink);/' auth-theme.css
grep -n "auth-accent\|#ff6a4d\|255, 106, 77" auth-theme.css
```
Expected after the grep: `--auth-accent` only in the variable block and the two glow `radial-gradient`s.

- [ ] **Step 7: Apply the theme before React starts**

`Client/index.html` — inside `<head>`, after `<title>`:
```html
    <script>
      // Apply the theme before anything renders (no white flash in night mode).
      (function () {
        var t = 'light';
        try {
          var saved = localStorage.getItem('kinetiq_theme');
          if (saved === 'light' || saved === 'dark') t = saved;
          else if (window.matchMedia && window.matchMedia('(prefers-color-scheme: dark)').matches) t = 'dark';
        } catch (e) {}
        document.documentElement.dataset.theme = t;
      })();
    </script>
```
`Client/src/main.tsx` — add `import './theme.css';` and `import { applyTheme, getInitialTheme } from './lib/theme';`
and call `applyTheme(getInitialTheme(), false);` before `ReactDOM.createRoot(...)`.

- [ ] **Step 8: Put the toggle in the top bar and on the login screen**

`AppShell.tsx`: `import ThemeToggle from './ThemeToggle';` and insert `<ThemeToggle />` right before
`<span className="app-user">`.
`App.tsx`: `import ThemeToggle from './ThemeToggle';` and inside `.auth-health-badge`, after the health
spans, add `<ThemeToggle />`.

- [ ] **Step 9: Run** — `npx vitest run src/lib/theme.test.ts src/ThemeToggle.test.tsx` → PASS;
`npm test`, `npx tsc --noEmit`, `npm run build` → green.

- [ ] **Step 10: Commit**
```bash
git add Client/index.html Client/src/main.tsx Client/src/theme.css Client/src/lib/theme.ts Client/src/lib/theme.test.ts Client/src/ThemeToggle.tsx Client/src/ThemeToggle.test.tsx Client/src/app-theme.css Client/src/features/auth/auth-theme.css Client/src/AppShell.tsx Client/src/App.tsx
git commit -m "feat(client): light/night themes with monochrome accent and toggle (TDD)"
```

---

### Task 6: Unit conversions (`lib/units.ts`)

**Files:** Create: `Client/src/lib/units.ts`, `Client/src/lib/units.test.ts`

**Interfaces:** Produces `type Units = 'metric' | 'imperial'`, `round1(n)`, `cmToIn`, `inToCm`, `kgToLb`, `lbToKg`, `formatFeetInches(cm: number): string`.

- [ ] **Step 1: Failing test** — `units.test.ts`:
```ts
import { describe, expect, it } from 'vitest';
import { cmToIn, formatFeetInches, inToCm, kgToLb, lbToKg, round1 } from './units';

describe('units', () => {
  it('converts length', () => {
    expect(cmToIn(2.54)).toBeCloseTo(1);
    expect(inToCm(70)).toBeCloseTo(177.8);
    expect(round1(cmToIn(178))).toBe(70.1);
  });

  it('converts weight', () => {
    expect(round1(kgToLb(74))).toBe(163.1);
    expect(lbToKg(1)).toBeCloseTo(0.45359237);
    expect(round1(lbToKg(kgToLb(74)))).toBe(74);
  });

  it('formats height as feet and inches', () => {
    expect(formatFeetInches(178)).toBe('5 ft 10 in');
    expect(formatFeetInches(182.6)).toBe('6 ft 0 in'); // 71.9 in rounds to 72 = 6 ft, not "5 ft 12 in"
  });

  it('rounds to one decimal place', () => {
    expect(round1(178.04)).toBe(178);
    expect(round1(178.05)).toBe(178.1);
  });
});
```
- [ ] **Step 2: Run** — FAIL (`Failed to resolve import "./units"`).
- [ ] **Step 3: `units.ts`**
```ts
export type Units = 'metric' | 'imperial';

export const CM_PER_IN = 2.54;
export const KG_PER_LB = 0.45359237;

export const round1 = (n: number) => Math.round(n * 10) / 10;

export const cmToIn = (cm: number) => cm / CM_PER_IN;
export const inToCm = (inches: number) => inches * CM_PER_IN;
export const kgToLb = (kg: number) => kg / KG_PER_LB;
export const lbToKg = (lb: number) => lb * KG_PER_LB;

// "5 ft 10 in" -- rounds total inches first, so it never shows "5 ft 12 in".
export function formatFeetInches(cm: number): string {
  const totalInches = Math.round(cmToIn(cm));
  return `${Math.floor(totalInches / 12)} ft ${totalInches % 12} in`;
}
```
- [ ] **Step 4: Run** — PASS. **Step 5: Commit**
```bash
git add Client/src/lib/units.ts Client/src/lib/units.test.ts
git commit -m "feat(client): metric/imperial conversion helpers (TDD)"
```

---

### Task 7: Units switch in the top bar (saved to the account)

**Files:**
- Create: `Client/src/lib/UnitsContext.tsx`, `Client/src/UnitsToggle.tsx`, `Client/src/UnitsToggle.test.tsx`
- Modify: `Client/src/App.tsx`, `Client/src/AppShell.tsx`

**Interfaces:** Produces `UnitsContext`, `useUnits(): { units: Units; setUnits(u: Units): Promise<void>; error: string | null }`, `<UnitsProvider>` (loads `getProfile().unit_preference`), `<UnitsToggle />`.

- [ ] **Step 1: Failing test** — `UnitsToggle.test.tsx`:
```tsx
import { fireEvent, render, screen } from '@testing-library/react';
import { http, HttpResponse } from 'msw';
import { beforeEach, describe, expect, it } from 'vitest';
import UnitsToggle from './UnitsToggle';
import { UnitsProvider } from './lib/UnitsContext';
import { setMockProfile } from './mocks/handlers';
import { server } from './mocks/server';
import { saveToken } from './lib/tokenStorage';

const renderToggle = () =>
  render(
    <UnitsProvider>
      <UnitsToggle />
    </UnitsProvider>,
  );

describe('UnitsToggle', () => {
  beforeEach(() => {
    localStorage.clear();
    saveToken('test-token');
  });

  it("starts from the account's saved units", async () => {
    setMockProfile({ unit_preference: 'imperial' });
    renderToggle();
    expect(await screen.findByRole('button', { name: 'Imperial', pressed: true })).toBeInTheDocument();
  });

  it('switches and saves to the account', async () => {
    let sent: unknown = null;
    server.use(
      http.patch('/api/v1/users/me', async ({ request }) => {
        sent = await request.json();
        return HttpResponse.json({});
      }),
    );
    renderToggle();
    await screen.findByRole('button', { name: 'Metric', pressed: true });
    fireEvent.click(screen.getByRole('button', { name: 'Imperial' }));
    expect(await screen.findByRole('button', { name: 'Imperial', pressed: true })).toBeInTheDocument();
    expect(sent).toEqual({ unit_preference: 'imperial' });
  });

  it('switches back and explains when saving fails', async () => {
    server.use(http.patch('/api/v1/users/me', () => HttpResponse.json({ detail: 'down' }, { status: 500 })));
    renderToggle();
    await screen.findByRole('button', { name: 'Metric', pressed: true });
    fireEvent.click(screen.getByRole('button', { name: 'Imperial' }));
    expect(await screen.findByText(/couldn't save your units/i)).toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Metric', pressed: true })).toBeInTheDocument();
  });
});
```
- [ ] **Step 2: Run** — FAIL (missing modules).
- [ ] **Step 3: `lib/UnitsContext.tsx`**
```tsx
import { createContext, useContext, useEffect, useState, type ReactNode } from 'react';
import { getProfile, updateProfile } from '../api';
import type { Units } from './units';

export interface UnitsValue {
  units: Units;
  setUnits: (units: Units) => Promise<void>;
  error: string | null;
}

// Default (no provider): metric, switching does nothing -- keeps components
// usable in isolated tests.
export const UnitsContext = createContext<UnitsValue>({
  units: 'metric',
  setUnits: async () => {},
  error: null,
});

export const useUnits = () => useContext(UnitsContext);

// App-wide unit system, loaded from the account and saved back to it.
export function UnitsProvider({ children }: { children: ReactNode }) {
  const [units, setUnitsState] = useState<Units>('metric');
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    getProfile()
      .then((p) => setUnitsState(p.unit_preference))
      .catch(() => {
        // keep metric; a 401 is handled globally by authFetch
      });
  }, []);

  // Optimistic: switch now, save in the background, switch back on failure.
  async function setUnits(next: Units) {
    const previous = units;
    setUnitsState(next);
    setError(null);
    try {
      await updateProfile({ unit_preference: next });
    } catch {
      setUnitsState(previous);
      setError("Couldn't save your units — try again.");
    }
  }

  return <UnitsContext.Provider value={{ units, setUnits, error }}>{children}</UnitsContext.Provider>;
}
```
- [ ] **Step 4: `UnitsToggle.tsx`**
```tsx
import { useUnits } from './lib/UnitsContext';
import type { Units } from './lib/units';

export default function UnitsToggle() {
  const { units, setUnits, error } = useUnits();
  const option = (value: Units, label: string) => (
    <button
      type="button"
      aria-pressed={units === value}
      className={units === value ? 'units-option active' : 'units-option'}
      onClick={() => {
        if (units !== value) void setUnits(value);
      }}
    >
      {label}
    </button>
  );
  return (
    <div className="units-toggle" role="group" aria-label="Units">
      {option('metric', 'Metric')}
      {option('imperial', 'Imperial')}
      {error && (
        <span role="alert" className="field-error">
          {error}
        </span>
      )}
    </div>
  );
}
```
Append to `app-theme.css`:
```css
.units-toggle { display: inline-flex; gap: 0; align-items: center; }
.app .units-option { border-radius: 0; padding: 0.25rem 0.6rem; }
.app .units-option:first-child { border-radius: 999px 0 0 999px; }
.app .units-option:nth-child(2) { border-radius: 0 999px 999px 0; }
.app .units-option.active { background: var(--accent); color: var(--accent-ink); }
```
- [ ] **Step 5: Wire it** — `App.tsx`: `import { UnitsProvider } from './lib/UnitsContext';` and wrap
`<Routes>…</Routes>` with `<UnitsProvider>…</UnitsProvider>` (inside `<BrowserRouter>`).
`AppShell.tsx`: `import UnitsToggle from './UnitsToggle';` and insert `<UnitsToggle />` before `<ThemeToggle />`.
- [ ] **Step 6: Run** — `npx vitest run src/UnitsToggle.test.tsx` → PASS; `npm test`, `npx tsc --noEmit` → green.
- [ ] **Step 7: Commit**
```bash
git add Client/src/lib/UnitsContext.tsx Client/src/UnitsToggle.tsx Client/src/UnitsToggle.test.tsx Client/src/App.tsx Client/src/AppShell.tsx Client/src/app-theme.css
git commit -m "feat(client): app-wide Metric | Imperial switch saved to the account (TDD)"
```

---

### Task 8: Profile form in the chosen units

**Files:** Modify: `Client/src/features/profile/profileSchema.ts`, `profileSchema.test.ts`, `ProfileForm.tsx`, `ProfilePage.tsx`, `ProfilePage.test.tsx`

**Interfaces:**
- Consumes: `Units`, `round1`, `cmToIn`, `inToCm`, `kgToLb`, `lbToKg`, `formatFeetInches` (Task 6); `useUnits` (Task 7).
- Produces (replacing Task 8 of the Stage C plan's versions):
  `makeProfileSchema(units)`, `ProfileFormValues` with keys `height, weight, birth_date, sex, experience_level, weekly_workout_goal, available_time_minutes, medications`,
  `profileDefaults(p, units)`, `toProfileUpdate(values, changed, saved, units): { update: ProfileUpdate; cleared: ProfileField[] }`,
  `SERVER_FIELD_TO_FORM`, `ProfileForm({ profile, units, onSave })`.

- [ ] **Step 1: Rewrite the tests**

Replace `profileSchema.test.ts` with:
```ts
import { describe, expect, it } from 'vitest';
import type { Profile } from '../../api';
import { isValidDate, makeProfileSchema, profileDefaults, toProfileUpdate } from './profileSchema';

const saved: Profile = {
  id: 1, email: 'a@example.com', display_name: 'A', sex: null, birth_date: '1990-05-01',
  height_cm: 178, weight_kg: 74, experience_level: 'intermediate', unit_preference: 'metric',
  weekly_workout_goal: 3, available_time_minutes: null, medications: null,
};

const errorPaths = (units: 'metric' | 'imperial', overrides: Record<string, string>) => {
  const r = makeProfileSchema(units).safeParse({ ...profileDefaults(saved, units), ...overrides });
  return r.success ? [] : r.error.issues.map((i) => i.path.join('.'));
};

describe('makeProfileSchema (mirrors ProfilePolicy)', () => {
  it('accepts the saved profile in both unit systems', () => {
    expect(errorPaths('metric', {})).toEqual([]);
    expect(errorPaths('imperial', {})).toEqual([]);
  });
  it('checks limits in metric', () => {
    expect(errorPaths('metric', { height: '0' })).toEqual(['height']);
    expect(errorPaths('metric', { height: '301' })).toEqual(['height']);
    expect(errorPaths('metric', { weight: '500.5' })).toEqual(['weight']);
    expect(errorPaths('metric', { weight: 'abc' })).toEqual(['weight']);
  });
  it('checks the same limits in imperial', () => {
    expect(errorPaths('imperial', { height: '118.1' })).toEqual([]); // ≈ 300 cm
    expect(errorPaths('imperial', { height: '118.3' })).toEqual(['height']);
    expect(errorPaths('imperial', { weight: '1102.3' })).toEqual([]); // ≈ 500 kg
    expect(errorPaths('imperial', { weight: '1103' })).toEqual(['weight']);
  });
  it('words imperial limits in imperial units', () => {
    const r = makeProfileSchema('imperial').safeParse({ ...profileDefaults(saved, 'imperial'), height: '200' });
    expect(r.success ? '' : r.error.issues[0].message).toMatch(/118\.1 in/);
  });
  it('needs whole numbers for workouts and minutes', () => {
    expect(errorPaths('metric', { weekly_workout_goal: '2.5' })).toEqual(['weekly_workout_goal']);
    expect(errorPaths('metric', { available_time_minutes: '0' })).toEqual(['available_time_minutes']);
  });
});

describe('isValidDate', () => {
  it('accepts real dates only', () => {
    expect(isValidDate('2000-02-29')).toBe(true);
    expect(isValidDate('2001-02-29')).toBe(false);
    expect(isValidDate('01/02/2000')).toBe(false);
  });
});

describe('profileDefaults', () => {
  it('shows saved values in the chosen units', () => {
    expect(profileDefaults(saved, 'metric')).toMatchObject({ height: '178', weight: '74' });
    expect(profileDefaults(saved, 'imperial')).toMatchObject({ height: '70.1', weight: '163.1' });
  });
});

describe('toProfileUpdate', () => {
  it('sends only changed fields, converted to metric', () => {
    const values = { ...profileDefaults(saved, 'imperial'), weight: '180' };
    expect(toProfileUpdate(values, ['weight'], saved, 'imperial')).toEqual({
      update: { weight_kg: 81.6 },
      cleared: [],
    });
  });
  it('never re-sends untouched height/weight', () => {
    const values = profileDefaults(saved, 'imperial');
    expect(toProfileUpdate(values, [], saved, 'imperial')).toEqual({ update: {}, cleared: [] });
  });
  it('reports emptied saved values', () => {
    const values = { ...profileDefaults(saved, 'metric'), height: '' };
    expect(toProfileUpdate(values, ['height'], saved, 'metric')).toEqual({ update: {}, cleared: ['height'] });
  });
});
```
In `ProfilePage.test.tsx`: wrap every `render(<ProfilePage />)` as
`render(<UnitsContext.Provider value={metric}><ProfilePage /></UnitsContext.Provider>)` with
```tsx
import { UnitsContext, type UnitsValue } from '../../lib/UnitsContext';
const metric: UnitsValue = { units: 'metric', setUnits: async () => {}, error: null };
const imperial: UnitsValue = { units: 'imperial', setUnits: async () => {}, error: null };
```
and append:
```tsx
  it('shows and saves height/weight in imperial', async () => {
    let sent: unknown = null;
    server.use(
      http.patch('/api/v1/users/me', async ({ request }) => {
        sent = await request.json();
        return HttpResponse.json({
          id: 1, email: 'test@example.com', display_name: 'Test User', sex: null, birth_date: null,
          height_cm: 178, weight_kg: 81.6, experience_level: 'intermediate', unit_preference: 'imperial',
          weekly_workout_goal: 3, available_time_minutes: null, medications: null,
        });
      }),
    );
    render(<UnitsContext.Provider value={imperial}><ProfilePage /></UnitsContext.Provider>);
    expect(await screen.findByLabelText('Height (in)')).toHaveValue('70.1');
    expect(screen.getByText('= 5 ft 10 in')).toBeInTheDocument();
    fireEvent.change(screen.getByLabelText('Weight (lb)'), { target: { value: '180' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save profile' }));
    expect(await screen.findByText('Profile saved.')).toBeInTheDocument();
    expect(sent).toEqual({ weight_kg: 81.6 });
  });

  it('has no units dropdown (the top-bar switch replaces it)', async () => {
    render(<UnitsContext.Provider value={metric}><ProfilePage /></UnitsContext.Provider>);
    await screen.findByLabelText('Height (cm)');
    expect(screen.queryByLabelText('Units')).not.toBeInTheDocument();
  });
```

- [ ] **Step 2: Run** — `npx vitest run src/features/profile` → FAIL (`makeProfileSchema` not exported).

- [ ] **Step 3: Replace `profileSchema.ts`**
```ts
import { z } from 'zod';
import type { Profile, ProfileUpdate } from '../../api';
import { cmToIn, inToCm, kgToLb, lbToKg, round1, type Units } from '../../lib/units';

// A real calendar date in YYYY-MM-DD (rejects 2001-02-29), like the server.
export function isValidDate(s: string): boolean {
  if (!/^\d{4}-\d{2}-\d{2}$/.test(s)) return false;
  const d = new Date(`${s}T00:00:00Z`);
  return !Number.isNaN(d.getTime()) && d.toISOString().startsWith(s);
}

const HEIGHT_MAX_CM = 300;
const WEIGHT_MAX_KG = 500;

// Empty means "not set"; otherwise the number must pass `ok`.
const blankOr = (ok: (n: number) => boolean) => (s: string) => s.trim() === '' || ok(Number(s));

// What the server will store for a typed height/weight, in metric, rounded.
export const heightToCm = (value: number, units: Units) => round1(units === 'imperial' ? inToCm(value) : value);
export const weightToKg = (value: number, units: Units) => round1(units === 'imperial' ? lbToKg(value) : value);

// Same limits as the server's ProfilePolicy, checked in metric, worded in the user's units.
export function makeProfileSchema(units: Units) {
  const heightMax = units === 'imperial' ? `${round1(cmToIn(HEIGHT_MAX_CM))} in` : `${HEIGHT_MAX_CM} cm`;
  const weightMax = units === 'imperial' ? `${round1(kgToLb(WEIGHT_MAX_KG))} lb` : `${WEIGHT_MAX_KG} kg`;
  return z.object({
    height: z.string().refine(
      blankOr((n) => heightToCm(n, units) > 0 && heightToCm(n, units) <= HEIGHT_MAX_CM),
      `Height must be more than 0 and at most ${heightMax}`,
    ),
    weight: z.string().refine(
      blankOr((n) => weightToKg(n, units) > 0 && weightToKg(n, units) <= WEIGHT_MAX_KG),
      `Weight must be more than 0 and at most ${weightMax}`,
    ),
    birth_date: z.string().refine((s) => s === '' || isValidDate(s), 'Enter a real date (YYYY-MM-DD)'),
    sex: z.string(),
    experience_level: z.string(),
    weekly_workout_goal: z.string().refine(blankOr((n) => Number.isInteger(n) && n >= 0), 'Use a whole number, 0 or more'),
    available_time_minutes: z.string().refine(blankOr((n) => Number.isInteger(n) && n > 0), 'Use a whole number above 0'),
    medications: z.string(),
  });
}

export type ProfileFormValues = z.infer<ReturnType<typeof makeProfileSchema>>;
export type ProfileField = keyof ProfileFormValues;

// Form field → server field (height/weight are unit-neutral in the form).
const SERVER_NAME: Record<ProfileField, keyof ProfileUpdate> = {
  height: 'height_cm',
  weight: 'weight_kg',
  birth_date: 'birth_date',
  sex: 'sex',
  experience_level: 'experience_level',
  weekly_workout_goal: 'weekly_workout_goal',
  available_time_minutes: 'available_time_minutes',
  medications: 'medications',
};

// Server field → form field, so a 422 lands under the right input.
export const SERVER_FIELD_TO_FORM: Record<string, ProfileField> = Object.fromEntries(
  Object.entries(SERVER_NAME).map(([form, server]) => [server, form]),
) as Record<string, ProfileField>;

const INTEGER_FIELDS: ProfileField[] = ['weekly_workout_goal', 'available_time_minutes'];

export function profileDefaults(p: Profile, units: Units): ProfileFormValues {
  const s = (v: string | number | null) => (v === null ? '' : String(v));
  return {
    height: p.height_cm === null ? '' : String(units === 'imperial' ? round1(cmToIn(p.height_cm)) : p.height_cm),
    weight: p.weight_kg === null ? '' : String(units === 'imperial' ? round1(kgToLb(p.weight_kg)) : p.weight_kg),
    birth_date: s(p.birth_date),
    sex: s(p.sex),
    experience_level: s(p.experience_level),
    weekly_workout_goal: s(p.weekly_workout_goal),
    available_time_minutes: s(p.available_time_minutes),
    medications: s(p.medications),
  };
}

// Turns the fields the user changed into a PATCH body (metric). Emptied
// saved values are reported in `cleared`.
export function toProfileUpdate(
  values: ProfileFormValues,
  changed: ProfileField[],
  saved: Profile,
  units: Units,
): { update: ProfileUpdate; cleared: ProfileField[] } {
  const update: Record<string, string | number | null> = {};
  const cleared: ProfileField[] = [];
  for (const field of changed) {
    const raw = values[field].trim();
    const serverField = SERVER_NAME[field];
    if (raw === '') {
      if (saved[serverField as keyof Profile] !== null) cleared.push(field);
      continue;
    }
    if (field === 'height') update[serverField] = heightToCm(Number(raw), units);
    else if (field === 'weight') update[serverField] = weightToKg(Number(raw), units);
    else if (INTEGER_FIELDS.includes(field)) update[serverField] = Number(raw);
    else update[serverField] = raw;
  }
  return { update: update as ProfileUpdate, cleared };
}
```

- [ ] **Step 4: Update `ProfileForm.tsx`**
- Props become `{ profile: Profile; units: Units; onSave: (update: ProfileUpdate) => Promise<Profile> }`.
- `useForm({ resolver: zodResolver(makeProfileSchema(units)), defaultValues: profileDefaults(profile, units) })`;
  add `watch` to the destructured `useForm` result.
- `toProfileUpdate(values, changed, saved, units)`; after save `reset(profileDefaults(updated, units))`.
- Server field errors: `setError(SERVER_FIELD_TO_FORM[field] ?? (field as ProfileField), …)`.
- Height/weight inputs:
```tsx
        <label htmlFor="height">Height ({units === 'imperial' ? 'in' : 'cm'})</label>
        <input id="height" inputMode="decimal" {...register('height')} />
        {units === 'imperial' && Number(watch('height')) > 0 && (
          <p className="muted">= {formatFeetInches(inToCm(Number(watch('height'))))}</p>
        )}
        {err('height')}
        <label htmlFor="weight">Weight ({units === 'imperial' ? 'lb' : 'kg'})</label>
        <input id="weight" inputMode="decimal" {...register('weight')} />
        {err('weight')}
```
- Delete the Units `<label>`/`<select>` block and its `err('unit_preference')`.
- Imports: `formatFeetInches`, `inToCm`, `type Units` from `'../../lib/units'`; `makeProfileSchema`,
  `SERVER_FIELD_TO_FORM` from `'./profileSchema'` (drop `profileFormSchema`).

`ProfilePage.tsx`: `import { useUnits } from '../../lib/UnitsContext';`, `const { units } = useUnits();`
and render `<ProfileForm key={units} profile={profile} units={units} onSave={updateProfile} />`
(the `key` remounts the form when units change, so it re-displays the saved values converted).

- [ ] **Step 5: Run** — `npx vitest run src/features/profile` → PASS; `npm test`, `npx tsc --noEmit` → green.
- [ ] **Step 6: Commit**
```bash
git add Client/src/features/profile
git commit -m "feat(client): profile form shows and saves height/weight in the chosen units (TDD)"
```

---

### Task 9: Server — JSON Merge Patch clearing

**Files:**
- Modify: `Server/src/app/UpdateProfile.h`, `Server/src/http/dto/ProfileDto.h`, `Server/src/http/dto/InjuryDto.h`, `Server/src/app/InjuryService.h`
- Modify tests: `Server/tests/test_update_profile.cpp`, `Server/tests/test_user_controller.cpp`, `Server/tests/test_injury_service.cpp`, `Server/tests/test_injury_controller.cpp`

**Interfaces:** Produces `ProfileUpdate::clear` (`std::set<std::string>` of wire names).

- [ ] **Step 1: Failing tests**

Append to `test_update_profile.cpp`:
```cpp
TEST(UpdateProfile, ClearField_EmptiesIt) {
    InMemoryUserRepository repo;
    const long long id = seedUser(repo);  // experience_level = "beginner"

    ProfileUpdate update;
    update.clear.insert("experience_level");
    UpdateProfile useCase(repo);
    const User result = useCase.execute(id, update);

    EXPECT_FALSE(result.experienceLevel.has_value());
    EXPECT_FALSE(repo.findById(id)->experienceLevel.has_value());  // persisted
}

TEST(UpdateProfile, ClearUnitPreference_Throws_OnThatField) {
    InMemoryUserRepository repo;
    const long long id = seedUser(repo);
    ProfileUpdate update;
    update.clear.insert("unit_preference");
    UpdateProfile useCase(repo);
    try {
        useCase.execute(id, update);
        FAIL() << "expected ProfileValidationError";
    } catch (const ProfileValidationError& e) {
        EXPECT_EQ(e.field(), "unit_preference");
    }
}
```
Append to `test_user_controller.cpp` (follow the file's existing helpers `UserApi` /
`registerAndGetToken` and `client.send(crow::HTTPMethod::Patch, ...)` pattern):
```cpp
TEST(UserControllerTest, PatchMe_NullClearsField_Returns200WithNull) {
    UserApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "clear@example.com");
    client.send(crow::HTTPMethod::Patch, "/api/v1/users/me", R"({"weight_kg":80})",
                {{"Authorization", "Bearer " + token}});

    const auto r = client.send(crow::HTTPMethod::Patch, "/api/v1/users/me", R"({"weight_kg":null})",
                               {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 200);
    const auto body = crow::json::load(r.body);
    ASSERT_TRUE(body);
    EXPECT_EQ(body["weight_kg"].t(), crow::json::type::Null);
}

TEST(UserControllerTest, PatchMe_NullUnitPreference_Returns422) {
    UserApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "clear-unit@example.com");
    const auto r = client.send(crow::HTTPMethod::Patch, "/api/v1/users/me",
                               R"({"unit_preference":null})", {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 422);
    EXPECT_TRUE(crow::json::load(r.body)["errors"].has("unit_preference"));
}
```
Append to `test_injury_service.cpp`:
```cpp
TEST(InjuryService, Update_EmptyNotes_ClearsThem) {
    Fixture f;
    auto req = unknownReq(1, "lower_back", "mild");
    req.notes = "old";
    const Injury created = f.svc.add(req);
    UpdateInjuryFields fields;
    fields.notes = "";
    EXPECT_FALSE(f.svc.update(1, created.id, fields).notes.has_value());
}
```
Append to `test_injury_controller.cpp`:
```cpp
TEST(InjuryControllerTest, Patch_NotesNull_ClearsNotes) {
    InjuryApi api;
    kinetiq::testing::HttpClient client(api.app);
    const std::string token = registerAndGetToken(client, "inj-notes@example.com");
    const auto created = crow::json::load(
        client
            .post("/api/v1/users/me/injuries",
                  R"({"body_region":"lower_back","severity":"mild","injury_type":"other","notes":"old"})",
                  {{"Authorization", "Bearer " + token}})
            .body);
    const auto r = client.send(crow::HTTPMethod::Patch,
                               "/api/v1/users/me/injuries/" + std::to_string(created["id"].i()),
                               R"({"notes":null})", {{"Authorization", "Bearer " + token}});
    EXPECT_EQ(r.code, 200);
    EXPECT_EQ(crow::json::load(r.body)["notes"].t(), crow::json::type::Null);
}
```
- [ ] **Step 2: Run** — build `kinetiq_tests` → FAIL (`ProfileUpdate` has no member `clear`).
- [ ] **Step 3: Implement**

`UpdateProfile.h`: add `#include <set>`; in `ProfileUpdate` add
```cpp
    // JSON Merge Patch: wire names of fields sent as null -> clear them.
    std::set<std::string> clear;
```
update its comment (absent = unchanged, value = set, null = clear; D3 superseded by ADR-0017), and
in `execute`, right after `kinetiq::domain::User user = *found;`:
```cpp
        if (update.clear.count("unit_preference")) {
            throw kinetiq::domain::ProfileValidationError(
                "unit_preference", "unit_preference can't be cleared (it must be metric or imperial)");
        }
        if (update.clear.count("sex")) user.sex.reset();
        if (update.clear.count("birth_date")) user.birthDate.reset();
        if (update.clear.count("height_cm")) user.heightCm.reset();
        if (update.clear.count("weight_kg")) user.weightKg.reset();
        if (update.clear.count("experience_level")) user.experienceLevel.reset();
        if (update.clear.count("weekly_workout_goal")) user.weeklyWorkoutGoal.reset();
        if (update.clear.count("available_time_minutes")) user.availableTimeMinutes.reset();
        if (update.clear.count("medications")) user.medications.reset();
```
`ProfileDto.h` — replace `parseProfileUpdate`'s body with:
```cpp
    kinetiq::app::ProfileUpdate u;
    // JSON Merge Patch: a key sent as null means "clear this field".
    const auto isNull = [&body](const char* key) {
        return body.has(key) && body[key].t() == crow::json::type::Null;
    };
    for (const char* key : {"sex", "birth_date", "height_cm", "weight_kg", "experience_level",
                            "unit_preference", "weekly_workout_goal", "available_time_minutes",
                            "medications"}) {
        if (isNull(key)) u.clear.insert(key);
    }
    auto present = [&](const char* key) { return body.has(key) && !isNull(key); };
    if (present("sex")) u.sex = std::string(body["sex"].s());
    if (present("birth_date")) u.birthDate = std::string(body["birth_date"].s());
    if (present("height_cm")) u.heightCm = body["height_cm"].d();
    if (present("weight_kg")) u.weightKg = body["weight_kg"].d();
    if (present("experience_level")) u.experienceLevel = std::string(body["experience_level"].s());
    if (present("unit_preference")) u.unitPreference = std::string(body["unit_preference"].s());
    if (present("weekly_workout_goal"))
        u.weeklyWorkoutGoal = static_cast<int>(body["weekly_workout_goal"].i());
    if (present("available_time_minutes"))
        u.availableTimeMinutes = static_cast<int>(body["available_time_minutes"].i());
    if (present("medications")) u.medications = std::string(body["medications"].s());
    return u;
```
(and update the function's comment).
`InjuryDto.h` `parseUpdateInjury`: replace the `notes` line with
```cpp
    if (body.has("notes")) {
        fields.notes = body["notes"].t() == crow::json::type::Null ? std::string()
                                                                    : std::string(body["notes"].s());
    }
```
`InjuryService.h` `update`: replace `if (fields.notes) injury.notes = fields.notes;` with
```cpp
        // Empty (or JSON null) notes clear them.
        if (fields.notes) {
            injury.notes = fields.notes->empty() ? std::nullopt : fields.notes;
        }
```
- [ ] **Step 4: Run** — build + `./build/kinetiq_tests.exe` → all pass, 0 skipped.
- [ ] **Step 5: Commit**
```bash
git add Server/src/app/UpdateProfile.h Server/src/http/dto/ProfileDto.h Server/src/http/dto/InjuryDto.h Server/src/app/InjuryService.h Server/tests/test_update_profile.cpp Server/tests/test_user_controller.cpp Server/tests/test_injury_service.cpp Server/tests/test_injury_controller.cpp
git commit -m "feat(server): JSON Merge Patch clearing for profile + injury notes (TDD)"
```

---

### Task 10: Server — profile history (migration 0006)

**Files:**
- Create: `Server/migrations/0006_user_profile_history.sql`, `Server/src/ports/IProfileHistoryRepository.h`, `Server/tests/support/InMemoryProfileHistoryRepository.h`, `Server/src/adapters/db/PgProfileHistoryRepository.h/.cpp`, `Server/src/adapters/db/AutoCommitPgProfileHistoryRepository.h/.cpp`, `Server/tests/test_pg_profile_history_repository.cpp`, `Server/tests/test_auto_commit_pg_profile_history_repository.cpp`
- Modify: `Server/src/app/UpdateProfile.h`, `Server/src/http/controllers/UserController.h/.cpp`, `Server/src/main.cpp`, `Server/CMakeLists.txt`, `Server/tests/test_update_profile.cpp`, `Server/tests/test_user_controller.cpp`

**Interfaces:**
- Produces `ports::IProfileHistoryRepository::record(long long userId, const std::string& field, const std::optional<std::string>& oldValue, const std::optional<std::string>& newValue)`;
  `UpdateProfile(IUserRepository&, IProfileHistoryRepository&)`;
  `registerUserRoutes(app, users, history, tokens)`;
  `testsupport::InMemoryProfileHistoryRepository` with public `std::vector<HistoryEntry> entries`.

- [ ] **Step 1: Failing tests**

In `test_update_profile.cpp`: `#include "support/InMemoryProfileHistoryRepository.h"`,
`using kinetiq::testsupport::InMemoryProfileHistoryRepository;`; every
`UpdateProfile useCase(repo);` becomes `InMemoryProfileHistoryRepository history; UpdateProfile useCase(repo, history);`.
Append:
```cpp
TEST(UpdateProfile, RecordsOnlyChangedFields) {
    InMemoryUserRepository repo;
    InMemoryProfileHistoryRepository history;
    const long long id = seedUser(repo);  // experience_level = "beginner"

    ProfileUpdate update;
    update.weightKg = 72.5;
    update.experienceLevel = "beginner";  // same value -> no row
    UpdateProfile(repo, history).execute(id, update);

    ASSERT_EQ(history.entries.size(), 1u);
    EXPECT_EQ(history.entries[0].userId, id);
    EXPECT_EQ(history.entries[0].field, "weight_kg");
    EXPECT_FALSE(history.entries[0].oldValue.has_value());
    EXPECT_EQ(history.entries[0].newValue, "72.5");
}

TEST(UpdateProfile, ClearingRecordsTheOldValue) {
    InMemoryUserRepository repo;
    InMemoryProfileHistoryRepository history;
    const long long id = seedUser(repo);

    ProfileUpdate update;
    update.clear.insert("experience_level");
    UpdateProfile(repo, history).execute(id, update);

    ASSERT_EQ(history.entries.size(), 1u);
    EXPECT_EQ(history.entries[0].field, "experience_level");
    EXPECT_EQ(history.entries[0].oldValue, "beginner");
    EXPECT_FALSE(history.entries[0].newValue.has_value());
}

TEST(UpdateProfile, UnitPreferenceIsNotTracked) {
    InMemoryUserRepository repo;
    InMemoryProfileHistoryRepository history;
    const long long id = seedUser(repo);
    ProfileUpdate update;
    update.unitPreference = "imperial";
    UpdateProfile(repo, history).execute(id, update);
    EXPECT_TRUE(history.entries.empty());
}
```
`test_user_controller.cpp`: `#include "support/InMemoryProfileHistoryRepository.h"`, add member
`kinetiq::testsupport::InMemoryProfileHistoryRepository history;` to `UserApi` and register with
`kinetiq::http::registerUserRoutes(app, users, history, tokens);`.

`test_pg_profile_history_repository.cpp`:
```cpp
// Profile history: migration 0006 + PgProfileHistoryRepository (DbTest rollback).

#include "support/TestDb.h"

#include <gtest/gtest.h>

#include "adapters/db/PgProfileHistoryRepository.h"

#include <pqxx/pqxx>

using kinetiq::db::PgProfileHistoryRepository;

namespace {
long long seedUser(pqxx::work& tx, const std::string& email) {
    return tx.exec("INSERT INTO users (email, password_hash) VALUES ($1, 'x') RETURNING id",
                   pqxx::params{email})[0][0].as<long long>();
}
}  // namespace

TEST_F(DbTest, PgProfileHistory_RecordsRow) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    const long long userId = seedUser(tx(), "hist@example.com");
    PgProfileHistoryRepository history(tx());
    history.record(userId, "weight_kg", std::string("74"), std::nullopt);

    const auto rows = tx().exec(
        "SELECT field, old_value, new_value, changed_at FROM user_profile_history WHERE user_id = $1",
        pqxx::params{userId});
    ASSERT_EQ(rows.size(), 1u);
    EXPECT_EQ(rows[0]["field"].as<std::string>(), "weight_kg");
    EXPECT_EQ(rows[0]["old_value"].as<std::string>(), "74");
    EXPECT_TRUE(rows[0]["new_value"].is_null());
    EXPECT_FALSE(rows[0]["changed_at"].is_null());
}

TEST_F(DbTest, PgProfileHistory_RejectsUntrackedField) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    const long long userId = seedUser(tx(), "hist-bad@example.com");
    PgProfileHistoryRepository history(tx());
    EXPECT_THROW(history.record(userId, "unit_preference", std::nullopt, std::string("metric")),
                 pqxx::sql_error);
}

TEST_F(DbTest, PgProfileHistory_DeletedWithUser) {
    if (!available()) GTEST_SKIP() << "no test database reachable";
    const long long userId = seedUser(tx(), "hist-cascade@example.com");
    PgProfileHistoryRepository(tx()).record(userId, "sex", std::nullopt, std::string("female"));
    tx().exec("DELETE FROM users WHERE id = $1", pqxx::params{userId});
    EXPECT_EQ(tx().exec("SELECT count(*) FROM user_profile_history WHERE user_id = $1",
                        pqxx::params{userId})[0][0].as<long long>(),
              0);
}
```
`test_auto_commit_pg_profile_history_repository.cpp`:
```cpp
// AutoCommitPgProfileHistoryRepository -- the one wired into main.cpp. Cleans
// up by deleting its user (history cascades).

#include "support/TestDb.h"

#include <gtest/gtest.h>

#include "adapters/db/AutoCommitPgProfileHistoryRepository.h"

#include <pqxx/pqxx>

using kinetiq::db::AutoCommitPgProfileHistoryRepository;

TEST(AutoCommitPgProfileHistoryRepositoryTest, RecordsOverOwnConnection) {
    if (!kinetiq::testing::ensureTestDatabaseReady()) GTEST_SKIP() << "no test database reachable";
    long long userId = 0;
    {
        pqxx::connection conn(kinetiq::testing::testConnString());
        pqxx::work tx(conn);
        userId = tx.exec("INSERT INTO users (email, password_hash) VALUES ('hist-auto@example.com','x') RETURNING id")[0][0].as<long long>();
        tx.commit();
    }
    AutoCommitPgProfileHistoryRepository history(kinetiq::testing::testConnString());
    history.record(userId, "height_cm", std::nullopt, std::string("178"));

    pqxx::connection conn(kinetiq::testing::testConnString());
    pqxx::work tx(conn);
    EXPECT_EQ(tx.exec("SELECT count(*) FROM user_profile_history WHERE user_id = $1",
                      pqxx::params{userId})[0][0].as<long long>(),
              1);
    tx.exec("DELETE FROM users WHERE id = $1", pqxx::params{userId});
    tx.commit();
}
```
CMake: add `src/adapters/db/PgProfileHistoryRepository.cpp` and
`src/adapters/db/AutoCommitPgProfileHistoryRepository.cpp` to **both** targets; add the two test files
to `kinetiq_tests`.

- [ ] **Step 2: Run** — build → FAIL (missing headers / `UpdateProfile` constructor).

- [ ] **Step 3: Migration** — `Server/migrations/0006_user_profile_history.sql`:
```sql
-- Migration 0006: append-only history of profile changes.
--
-- Every changed profile field is recorded (old -> new, when), so a cleared
-- value's last known value is kept for future training plans (ADR-0017).
-- unit_preference is a display setting and isn't tracked. Deleting a user
-- deletes their history (future "delete my data").

CREATE TABLE user_profile_history (
    id         BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id    BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    field      TEXT   NOT NULL CHECK (field IN ('sex','birth_date','height_cm','weight_kg',
                      'experience_level','weekly_workout_goal','available_time_minutes','medications')),
    old_value  TEXT,
    new_value  TEXT,
    changed_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX idx_user_profile_history_lookup ON user_profile_history(user_id, field, changed_at);
```

- [ ] **Step 4: Port, fake, adapters**

`Server/src/ports/IProfileHistoryRepository.h`:
```cpp
#pragma once

#include <optional>
#include <string>

namespace kinetiq::ports {

// Append-only log of profile field changes (ADR-0017). Values are text so one
// table fits every field; nullopt = "not set".
class IProfileHistoryRepository {
public:
    virtual ~IProfileHistoryRepository() = default;
    virtual void record(long long userId, const std::string& field,
                        const std::optional<std::string>& oldValue,
                        const std::optional<std::string>& newValue) = 0;
};

}  // namespace kinetiq::ports
```
`Server/tests/support/InMemoryProfileHistoryRepository.h`:
```cpp
#pragma once

#include "ports/IProfileHistoryRepository.h"

#include <optional>
#include <string>
#include <vector>

namespace kinetiq::testsupport {

struct HistoryEntry {
    long long userId = 0;
    std::string field;
    std::optional<std::string> oldValue;
    std::optional<std::string> newValue;
};

// Fake: records entries in a public vector for tests to inspect.
class InMemoryProfileHistoryRepository : public kinetiq::ports::IProfileHistoryRepository {
public:
    void record(long long userId, const std::string& field,
                const std::optional<std::string>& oldValue,
                const std::optional<std::string>& newValue) override {
        entries.push_back({userId, field, oldValue, newValue});
    }
    std::vector<HistoryEntry> entries;
};

}  // namespace kinetiq::testsupport
```
`PgProfileHistoryRepository.h`:
```cpp
#pragma once

#include "ports/IProfileHistoryRepository.h"

#include <pqxx/pqxx>

namespace kinetiq::db {

class PgProfileHistoryRepository : public kinetiq::ports::IProfileHistoryRepository {
public:
    explicit PgProfileHistoryRepository(pqxx::work& tx) : tx_(tx) {}
    void record(long long userId, const std::string& field,
                const std::optional<std::string>& oldValue,
                const std::optional<std::string>& newValue) override;

private:
    pqxx::work& tx_;
};

}  // namespace kinetiq::db
```
`PgProfileHistoryRepository.cpp`:
```cpp
#include "adapters/db/PgProfileHistoryRepository.h"

namespace kinetiq::db {

void PgProfileHistoryRepository::record(long long userId, const std::string& field,
                                        const std::optional<std::string>& oldValue,
                                        const std::optional<std::string>& newValue) {
    tx_.exec(
        "INSERT INTO user_profile_history (user_id, field, old_value, new_value) "
        "VALUES ($1, $2, $3, $4)",
        pqxx::params{userId, field, oldValue, newValue});
}

}  // namespace kinetiq::db
```
`AutoCommitPgProfileHistoryRepository.h`:
```cpp
#pragma once

#include "ports/IProfileHistoryRepository.h"

#include <string>

namespace kinetiq::db {

// Production adapter: one connection + transaction per call (ADR-0015).
class AutoCommitPgProfileHistoryRepository : public kinetiq::ports::IProfileHistoryRepository {
public:
    explicit AutoCommitPgProfileHistoryRepository(std::string connString)
        : connString_(std::move(connString)) {}
    void record(long long userId, const std::string& field,
                const std::optional<std::string>& oldValue,
                const std::optional<std::string>& newValue) override;

private:
    std::string connString_;
};

}  // namespace kinetiq::db
```
`AutoCommitPgProfileHistoryRepository.cpp`:
```cpp
#include "adapters/db/AutoCommitPgProfileHistoryRepository.h"

#include "adapters/db/PgProfileHistoryRepository.h"

#include <pqxx/pqxx>

namespace kinetiq::db {

void AutoCommitPgProfileHistoryRepository::record(long long userId, const std::string& field,
                                                  const std::optional<std::string>& oldValue,
                                                  const std::optional<std::string>& newValue) {
    pqxx::connection conn(connString_);
    pqxx::work tx(conn);
    PgProfileHistoryRepository(tx).record(userId, field, oldValue, newValue);
    tx.commit();
}

}  // namespace kinetiq::db
```

- [ ] **Step 5: Record history in `UpdateProfile`** — add includes `ports/IProfileHistoryRepository.h`,
`<map>`, `<sstream>`; add above the class:
```cpp
namespace detail {
inline std::optional<std::string> asText(const std::optional<std::string>& v) { return v; }
inline std::optional<std::string> asText(const std::optional<double>& v) {
    if (!v) return std::nullopt;
    std::ostringstream os;
    os << *v;  // 72.5 -> "72.5" (not "72.500000")
    return os.str();
}
inline std::optional<std::string> asText(const std::optional<int>& v) {
    return v ? std::optional<std::string>(std::to_string(*v)) : std::nullopt;
}
// The 8 tracked fields, keyed by wire name (unit_preference deliberately excluded).
inline std::map<std::string, std::optional<std::string>> trackedValues(const kinetiq::domain::User& u) {
    return {{"sex", asText(u.sex)},
            {"birth_date", asText(u.birthDate)},
            {"height_cm", asText(u.heightCm)},
            {"weight_kg", asText(u.weightKg)},
            {"experience_level", asText(u.experienceLevel)},
            {"weekly_workout_goal", asText(u.weeklyWorkoutGoal)},
            {"available_time_minutes", asText(u.availableTimeMinutes)},
            {"medications", asText(u.medications)}};
}
}  // namespace detail
```
Constructor: `UpdateProfile(kinetiq::ports::IUserRepository& users, kinetiq::ports::IProfileHistoryRepository& history) : users_(users), history_(history) {}`
+ member `kinetiq::ports::IProfileHistoryRepository& history_;`.
In `execute`: after `User user = *found;` add `const auto before = detail::trackedValues(user);`;
after `users_.update(user);` add
```cpp
        // Record each tracked field that actually changed (ADR-0017). Not atomic
        // with the save above (ADR-0015).
        for (const auto& [field, after] : detail::trackedValues(user)) {
            const auto& old = before.at(field);
            if (old != after) history_.record(userId, field, old, after);
        }
```

- [ ] **Step 6: Wire it** — `UserController.h/.cpp`: add `#include "ports/IProfileHistoryRepository.h"`,
signature `registerUserRoutes(crow::SimpleApp& app, kinetiq::ports::IUserRepository& users,
kinetiq::ports::IProfileHistoryRepository& history, kinetiq::ports::ITokenService& tokens)`; lambda capture
`[&users, &history, &tokens]`; `kinetiq::app::UpdateProfile updateProfile(users, history);`.
`main.cpp`: `#include "adapters/db/AutoCommitPgProfileHistoryRepository.h"`, after the `users` line
`kinetiq::db::AutoCommitPgProfileHistoryRepository history(buildConnString());`, and
`kinetiq::http::registerUserRoutes(app, users, history, tokens);`.

- [ ] **Step 7: Run + verify the migration**
Build; `./build/kinetiq_tests.exe 2>&1 | grep -E "testdb|^\[  (PASSED|FAILED|SKIPPED)  ?\] [0-9]"` →
all pass, 0 skipped. Then
`docker exec kinetiq-postgres psql -U kinetiq -d kinetiq_test -c "\d user_profile_history"` → table + CHECK + index.

- [ ] **Step 8: Commit**
```bash
git add Server/migrations/0006_user_profile_history.sql Server/src/ports/IProfileHistoryRepository.h Server/tests/support/InMemoryProfileHistoryRepository.h Server/src/adapters/db/PgProfileHistoryRepository.h Server/src/adapters/db/PgProfileHistoryRepository.cpp Server/src/adapters/db/AutoCommitPgProfileHistoryRepository.h Server/src/adapters/db/AutoCommitPgProfileHistoryRepository.cpp Server/src/app/UpdateProfile.h Server/src/http/controllers/UserController.h Server/src/http/controllers/UserController.cpp Server/src/main.cpp Server/CMakeLists.txt Server/tests/test_update_profile.cpp Server/tests/test_user_controller.cpp Server/tests/test_pg_profile_history_repository.cpp Server/tests/test_auto_commit_pg_profile_history_repository.cpp
git commit -m "feat(server): profile history -- every changed field recorded, migration 0006 (TDD)"
```

---

### Task 11: Client — clearing values + "last saved" note

**Files:** Modify: `Client/src/features/profile/profileSchema.ts`, `profileSchema.test.ts`, `ProfileForm.tsx`, `ProfilePage.test.tsx`, `Client/src/api.ts` (type only)

**Interfaces:** `toProfileUpdate` now puts `null` in `update` for emptied saved fields (still listing them in `cleared`); new `clearedMessage(field, saved, units): string`.

- [ ] **Step 1: Failing tests**
In `profileSchema.test.ts`, replace the `'reports emptied saved values'` test with:
```ts
  it('sends null for emptied saved values and lists them', () => {
    const values = { ...profileDefaults(saved, 'metric'), height: '' };
    expect(toProfileUpdate(values, ['height'], saved, 'metric')).toEqual({
      update: { height_cm: null },
      cleared: ['height'],
    });
  });

  it('ignores emptying a field that was never set', () => {
    const values = { ...profileDefaults(saved, 'metric'), available_time_minutes: '' };
    expect(toProfileUpdate(values, ['available_time_minutes'], saved, 'metric')).toEqual({ update: {}, cleared: [] });
  });
```
and add (import `clearedMessage`):
```ts
describe('clearedMessage', () => {
  it('names the last saved value in the chosen units', () => {
    expect(clearedMessage('weight', saved, 'metric')).toBe(
      'Weight cleared. Kinetiq will keep using your last saved weight (74 kg) for training plans.',
    );
    expect(clearedMessage('weight', saved, 'imperial')).toContain('(163.1 lb)');
  });
});
```
In `ProfilePage.test.tsx`, replace `'blocks clearing a saved value'` with:
```tsx
  it('clears a saved value and explains the last saved value is kept', async () => {
    let sent: unknown = null;
    server.use(
      http.patch('/api/v1/users/me', async ({ request }) => {
        sent = await request.json();
        return HttpResponse.json({
          id: 1, email: 'test@example.com', display_name: 'Test User', sex: null, birth_date: null,
          height_cm: null, weight_kg: 74, experience_level: 'intermediate', unit_preference: 'metric',
          weekly_workout_goal: 3, available_time_minutes: null, medications: null,
        });
      }),
    );
    render(<UnitsContext.Provider value={metric}><ProfilePage /></UnitsContext.Provider>);
    fireEvent.change(await screen.findByLabelText('Height (cm)'), { target: { value: '' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save profile' }));
    expect(
      await screen.findByText(/height cleared\. kinetiq will keep using your last saved height \(178 cm\)/i),
    ).toBeInTheDocument();
    expect(sent).toEqual({ height_cm: null });
    expect(screen.getByLabelText('Height (cm)')).toHaveValue('');
  });
```
- [ ] **Step 2: Run** — FAIL.
- [ ] **Step 3: Implement**
`api.ts`: the `Profile` fields are already `| null`, so `ProfileUpdate` (a `Partial`) accepts `null` — no change needed; confirm `tsc` agrees.
`profileSchema.ts` — in `toProfileUpdate`, change the empty branch to:
```ts
    if (raw === '') {
      if (saved[serverField as keyof Profile] !== null) {
        update[serverField] = null; // JSON Merge Patch: null clears it
        cleared.push(field);
      }
      continue;
    }
```
and append:
```ts
const FIELD_LABELS: Record<ProfileField, string> = {
  height: 'height',
  weight: 'weight',
  birth_date: 'birth date',
  sex: 'sex',
  experience_level: 'experience level',
  weekly_workout_goal: 'workouts per week',
  available_time_minutes: 'minutes per session',
  medications: 'medications',
};

// "Weight cleared. Kinetiq will keep using your last saved weight (74 kg) …"
export function clearedMessage(field: ProfileField, saved: Profile, units: Units): string {
  const label = FIELD_LABELS[field];
  const suffix =
    field === 'height' ? (units === 'imperial' ? ' in' : ' cm') : field === 'weight' ? (units === 'imperial' ? ' lb' : ' kg') : '';
  const last = profileDefaults(saved, units)[field];
  return `${label.charAt(0).toUpperCase() + label.slice(1)} cleared. Kinetiq will keep using your last saved ${label} (${last}${suffix}) for training plans.`;
}
```
`ProfileForm.tsx` — remove the `if (cleared.length > 0) { … return; }` block; after a successful save
set the status to the cleared messages when there are any:
```tsx
      const updated = await onSave(update);
      const notes = cleared.map((f) => clearedMessage(f, saved, units));
      setSaved(updated);
      reset(profileDefaults(updated, units));
      setStatus(notes.length > 0 ? notes.join(' ') : 'Profile saved.');
```
(import `clearedMessage`).
- [ ] **Step 4: Run** — `npx vitest run src/features/profile` → PASS; `npm test`, `npx tsc --noEmit` → green.
- [ ] **Step 5: Commit**
```bash
git add Client/src/features/profile Client/src/api.ts
git commit -m "feat(client): clear saved profile values (null) and say the last value is kept (TDD)"
```

---

### Task 12: Docs + hand over to Stage C Task 9

**Files:** Modify: `docs/02-api-contract.md`, `docs/01-data-model.md`, `docs/superpowers/specs/2026-07-17-slice-2-profile-injuries-design.md`, `docs/superpowers/plans/2026-10-22-stage-c-frontend.md`, `CLAUDE.md`

- [ ] **Step 1: API contract** — under `### PATCH /users/me`, add: "JSON Merge Patch (RFC 7396):
absent = unchanged, value = set, **`null` = clear**; `unit_preference` can't be cleared (`422`).
Every changed field except `unit_preference` is recorded in `user_profile_history` (ADR-0017)."
Add test cases `✅ {"weight_kg": null} clears it → 200 with null` and `❌ {"unit_preference": null} → 422`.
Under injuries PATCH: "`notes: null` (or `""`) clears notes."
- [ ] **Step 2: Data model** — add `### User_Profile_History (migration 0006)` (columns, CHECK list,
cascade, purpose: last known values for training; disclose in user agreement).
- [ ] **Step 3: July spec** — under D3 add "⚠️ Superseded 2026-10-25 by ADR-0017 (null clears)."
- [ ] **Step 4: Stage C plan Task 9** — in Step 2's browser checklist add: "toggle ☾/☀ on the login
screen and in the app (and reload: choice kept); switch Metric → Imperial (profile shows in / lb,
choice survives reload); clear weight → note names the last saved weight". In Step 3's backlog add:
forgot password; subscription idea ($5.99/month, refund after 10 verified workouts — payments,
refunds, workout verification, depends on Slice 4); user agreement must disclose that cleared
values are kept; "delete my data" (erases history); delete old Docker volumes
(`fitapp_kinetiq_pgdata`, `fitapp_fitapp_pgdata`, `kinetiq_kinetiq_pgdata`); `npm audit` leftovers
from Task 3.
- [ ] **Step 5: CLAUDE.md** — Client architecture: theme tokens in `theme.css` + `lib/theme.ts`
(`data-theme` on `<html>`); `UnitsProvider`/`useUnits` (metric storage, client-side conversion via
`lib/units.ts`). Server: `PATCH /users/me` is JSON Merge Patch; profile history port.
- [ ] **Step 6: Commit**
```bash
git add docs CLAUDE.md
git commit -m "docs: merge-patch clearing, profile history, themes/units; extend Stage C Task 9 checklist"
```
Then continue with **Stage C plan Task 9** (browser verification of everything, docs, backlog, merge).
