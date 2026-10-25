# Stage C Frontend (Injuries + Profile) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** After login, users manage their injuries (known condition or unlisted) and their profile in a tabbed app shell, with the app renamed to Kinetiq.

**Architecture:** `App.tsx` keeps session restore + login; once logged in it renders a React Router tree: `AppShell` (top bar + tabs) with `/injuries` and `/profile` pages. All authenticated calls go through one `authFetch` helper in `api.ts` (Bearer token, typed `ApiError` with per-field errors, 401 → logout hook). Forms use React Hook Form + Zod; pure "schema + converter" modules hold the rules so they're unit-testable without rendering.

**Tech Stack:** React 18 + TypeScript + Vite; `react-router` 7.18.4, `react-hook-form` 7.90.0, `zod` 3.25.76, `@hookform/resolvers` 5.9.2; Vitest + Testing Library (`fireEvent`) + MSW 2.

**Spec:** [`docs/superpowers/specs/2026-10-22-stage-c-frontend-design.md`](../specs/2026-10-22-stage-c-frontend-design.md)

## Global Constraints
- Display name **Kinetiq** everywhere in UI text and docs; links/names of the GitHub repo `itaim900/KinetiQ` stay unchanged.
- Exact dependency versions (installed with `--save-exact`): `react-router@7.18.4`, `react-hook-form@7.90.0`, `zod@3.25.76`, `@hookform/resolvers@5.9.2`.
- Import router APIs from `'react-router'` (v7 has no separate `react-router-dom` needed).
- Server is the authority: any 422 `errors` field is shown under the matching form field.
- Known condition ⇒ region / type / muscles locked in the UI; edit sends only `severity` / `notes` / `active`.
- Not-medical-advice notice inside the locked condition box and under the injury form.
- Profile: metric only; only changed fields are sent; emptying a saved value shows "Clearing a saved value isn't supported yet".
- Every commit green: the pre-commit hook runs client type-check + tests on any `Client/` change. Run commands from `Client/`.
- Tests use `fireEvent` (no `user-event` dependency). MSW handlers are stateful; `resetMockDb()` runs after each test.

---

### Task 1: Rename KinetiQ → Kinetiq

**Files:**
- Modify: `Client/index.html`, `Client/src/App.tsx`, `Server/src/main.cpp`, `README.md`, `CLAUDE.md`, `scripts/run-dev.ps1`, `docs/CONTEXT.md`, `docs/01-data-model.md`, `docs/02-api-contract.md`, `docs/03-llm-port.md`, `docs/04-architecture.md`, `docs/05-roadmap.md`, `docs/phase-1-summary.md`

**Interfaces:** Produces: headings and `<title>` read "Kinetiq" (Task 3's tests rely on `Kinetiq`).

- [ ] **Step 1: Write the failing check** — from the repo root:
```bash
git grep -n "KinetiQ" -- Client Server/src README.md CLAUDE.md scripts docs/CONTEXT.md docs/0*.md docs/phase-1-summary.md | grep -v "itaim900/KinetiQ"
```
Expected: 20+ lines (the occurrences to rename).

- [ ] **Step 2: Rename (skipping repo links)**
```bash
for f in Client/index.html Client/src/App.tsx Server/src/main.cpp README.md CLAUDE.md scripts/run-dev.ps1 docs/CONTEXT.md docs/01-data-model.md docs/02-api-contract.md docs/03-llm-port.md docs/04-architecture.md docs/05-roadmap.md docs/phase-1-summary.md; do
  sed -i '/itaim900\/KinetiQ/!s/KinetiQ/Kinetiq/g' "$f"
done
```

- [ ] **Step 3: Verify**
Re-run Step 1's command → no output. `git grep -n "KinetiQ"` → only `itaim900/KinetiQ` links and dated specs/plans/worklog (history, left as is).
From `Client/`: `npm test` → all pass. From `Server/`: `cmake --build --preset default; echo exit=$?` → `exit=0`.

- [ ] **Step 4: Commit** (hook runs server + client checks)
```bash
git add -A Client/index.html Client/src/App.tsx Server/src/main.cpp README.md CLAUDE.md scripts/run-dev.ps1 docs/CONTEXT.md docs/0*.md docs/phase-1-summary.md
git commit -m "chore: rename app display name KinetiQ -> Kinetiq"
```

---

### Task 2: Dependencies, `authFetch` + API functions, stateful MSW handlers

**Files:**
- Modify: `Client/package.json` (via npm), `Client/src/api.ts`, `Client/src/mocks/handlers.ts`, `Client/src/test/setup.ts`
- Create: `Client/src/api.injuries.test.ts`

**Interfaces:**
- Produces (in `api.ts`):
  - Types `Severity`, `InjuryType`, `BodyRegion`, `Muscle`, `InjuryMuscle`, `InjuryCondition`, `ConditionRef`, `Injury`, `AddInjuryRequest`, `UpdateInjuryRequest`, `Profile`, `ProfileUpdate`.
  - `class ApiError extends Error { status: number; fieldErrors: Record<string, string[]> }`
  - `setUnauthorizedHandler(handler: (() => void) | null): void`
  - `authFetch<T>(path: string, options?: { method?: string; body?: unknown }): Promise<T>`
  - `getProfile(): Promise<Profile>`, `updateProfile(u: ProfileUpdate): Promise<Profile>`, `listInjuries(): Promise<Injury[]>`, `addInjury(r: AddInjuryRequest): Promise<Injury>`, `updateInjury(id: number, r: UpdateInjuryRequest): Promise<Injury>`, `deleteInjury(id: number): Promise<void>`, `listBodyRegions(): Promise<BodyRegion[]>`, `listMuscles(): Promise<Muscle[]>`, `listConditions(): Promise<InjuryCondition[]>`
- Produces (in `mocks/handlers.ts`): `mockRegions`, `mockMuscles`, `mockConditions`, `resetMockDb()`, `seedInjury(overrides?: Partial<Injury>): Injury`, `setMockProfile(p: Partial<Profile>)`.

- [ ] **Step 1: Install dependencies**
```bash
npm install --save-exact react-router@7.18.4 react-hook-form@7.90.0 zod@3.25.76 @hookform/resolvers@5.9.2
```
Expected: `package.json` lists the four exact versions; `npm test` still passes.

- [ ] **Step 2: Write the failing test** — `Client/src/api.injuries.test.ts`:
```ts
import { http, HttpResponse } from 'msw';
import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import {
  ApiError,
  addInjury,
  deleteInjury,
  listConditions,
  listInjuries,
  setUnauthorizedHandler,
  updateInjury,
} from './api';
import { server } from './mocks/server';
import { saveToken } from './lib/tokenStorage';

describe('authenticated API calls', () => {
  beforeEach(() => {
    localStorage.clear();
    saveToken('test-token');
  });
  afterEach(() => setUnauthorizedHandler(null));

  it('sends the saved token as a Bearer header', async () => {
    let auth: string | null = null;
    server.use(
      http.get('/api/v1/users/me/injuries', ({ request }) => {
        auth = request.headers.get('Authorization');
        return HttpResponse.json([]);
      }),
    );
    await listInjuries();
    expect(auth).toBe('Bearer test-token');
  });

  it('turns a 422 into an ApiError with field errors', async () => {
    server.use(
      http.post('/api/v1/users/me/injuries', () =>
        HttpResponse.json(
          { detail: 'One or more fields are invalid.', errors: { body_region: ['bad region'] } },
          { status: 422 },
        ),
      ),
    );
    const err = await addInjury({
      body_region: 'nowhere',
      injury_type: 'joint',
      muscle_ids: [],
      severity: 'mild',
    }).catch((e: unknown) => e);
    expect(err).toBeInstanceOf(ApiError);
    expect((err as ApiError).status).toBe(422);
    expect((err as ApiError).fieldErrors).toEqual({ body_region: ['bad region'] });
  });

  it('calls the unauthorized handler on a 401', async () => {
    const onUnauthorized = vi.fn();
    setUnauthorizedHandler(onUnauthorized);
    server.use(
      http.get('/api/v1/users/me/injuries', () =>
        HttpResponse.json({ detail: 'expired' }, { status: 401 }),
      ),
    );
    await expect(listInjuries()).rejects.toBeInstanceOf(ApiError);
    expect(onUnauthorized).toHaveBeenCalledOnce();
  });

  it('adds a known condition and gets its catalog muscles back', async () => {
    const created = await addInjury({ condition_id: 1, severity: 'moderate' });
    expect(created.condition?.name).toBe('Hamstring strain');
    expect(created.body_region).toBe('thigh');
    expect(created.muscles.map((m) => m.role)).toEqual(['primary', 'secondary']);
  });

  it('updates and deletes (204 returns nothing)', async () => {
    const created = await addInjury({
      body_region: 'knee',
      injury_type: 'joint',
      muscle_ids: [2],
      severity: 'mild',
    });
    const updated = await updateInjury(created.id, { active: false });
    expect(updated.active).toBe(false);
    await expect(deleteInjury(created.id)).resolves.toBeUndefined();
    expect(await listInjuries()).toEqual([]);
  });

  it('lists the catalog conditions', async () => {
    const conditions = await listConditions();
    expect(conditions.map((c) => c.name)).toEqual(['Hamstring strain', 'Patellar tendinopathy']);
  });
});
```

- [ ] **Step 3: Run to verify it fails**

Run: `npx vitest run src/api.injuries.test.ts`
Expected: FAIL — `"ApiError" is not exported by "src/api.ts"` (or similar missing-export error).

- [ ] **Step 4: Extend `api.ts`**

At the top of `Client/src/api.ts` add `import { getToken } from './lib/tokenStorage';`.
Replace the existing `interface ApiErrorBody { detail?: string; }` with:
```ts
interface ApiErrorBody {
  detail?: string;
  errors?: Record<string, string[]>;
}
```
Append to the end of the file:
```ts
// ---------------------------------------------------------------------------
// Authenticated API (Stage C). Types mirror the server's JSON exactly.

export type Severity = 'mild' | 'moderate' | 'severe';
export type InjuryType = 'muscle' | 'bone' | 'tendon_ligament' | 'joint' | 'other';

export interface BodyRegion {
  slug: string;
  name: string;
}

export interface Muscle {
  id: number;
  name: string;
  muscle_group: string;
}

export interface InjuryMuscle {
  id: number;
  name: string;
  role: 'primary' | 'secondary';
}

export interface InjuryCondition {
  id: number;
  slug: string;
  name: string;
  icd11_code: string | null;
  body_region: string;
  injury_type: InjuryType;
  muscles: InjuryMuscle[];
}

export interface ConditionRef {
  id: number;
  name: string;
  icd11_code: string | null;
}

export interface Injury {
  id: number;
  user_id: number;
  condition: ConditionRef | null; // null = unlisted injury
  body_region: string;
  injury_type: InjuryType;
  severity: Severity;
  muscles: InjuryMuscle[];
  active: boolean;
  notes: string | null;
  created_at: string;
}

// The two request shapes the server accepts (known condition vs unlisted).
export type AddInjuryRequest =
  | { condition_id: number; severity: Severity; notes?: string }
  | {
      body_region: string;
      injury_type: InjuryType;
      muscle_ids: number[];
      severity: Severity;
      notes?: string;
    };

export interface UpdateInjuryRequest {
  body_region?: string;
  injury_type?: InjuryType;
  muscle_ids?: number[];
  severity?: Severity;
  notes?: string;
  active?: boolean;
}

export interface Profile {
  id: number;
  email: string;
  display_name: string;
  sex: string | null;
  birth_date: string | null;
  height_cm: number | null;
  weight_kg: number | null;
  experience_level: string | null;
  unit_preference: 'metric' | 'imperial';
  weekly_workout_goal: number | null;
  available_time_minutes: number | null;
  medications: string | null;
}

export type ProfileUpdate = Partial<Omit<Profile, 'id' | 'email' | 'display_name'>>;

// A failed request: HTTP status, the server's `detail`, and its per-field
// `errors` (RFC 9457 body from the C++ server) so forms can show each message
// under the right input.
export class ApiError extends Error {
  readonly status: number;
  readonly fieldErrors: Record<string, string[]>;

  constructor(status: number, message: string, fieldErrors: Record<string, string[]> = {}) {
    super(message);
    this.name = 'ApiError';
    this.status = status;
    this.fieldErrors = fieldErrors;
  }
}

// App registers this so ANY authenticated call that gets a 401 (expired
// 24h token) logs the user out in one place.
let unauthorizedHandler: (() => void) | null = null;

export function setUnauthorizedHandler(handler: (() => void) | null): void {
  unauthorizedHandler = handler;
}

async function toApiError(res: Response): Promise<ApiError> {
  let body: ApiErrorBody = {};
  try {
    body = (await res.json()) as ApiErrorBody;
  } catch {
    // Not JSON (or empty) -- keep the defaults.
  }
  return new ApiError(res.status, body.detail ?? `Request failed: ${res.status}`, body.errors ?? {});
}

export async function authFetch<T>(
  path: string,
  options: { method?: string; body?: unknown } = {},
): Promise<T> {
  const token = getToken();
  const headers: Record<string, string> = {};
  if (token) headers.Authorization = `Bearer ${token}`;
  if (options.body !== undefined) headers['Content-Type'] = 'application/json';

  const res = await fetch(`${apiBase}${path}`, {
    method: options.method ?? 'GET',
    headers,
    body: options.body !== undefined ? JSON.stringify(options.body) : undefined,
  });
  if (res.status === 401) unauthorizedHandler?.();
  if (!res.ok) throw await toApiError(res);
  if (res.status === 204) return undefined as T;
  return (await res.json()) as T;
}

export const getProfile = () => authFetch<Profile>('/api/v1/users/me');
export const updateProfile = (update: ProfileUpdate) =>
  authFetch<Profile>('/api/v1/users/me', { method: 'PATCH', body: update });

export const listInjuries = () => authFetch<Injury[]>('/api/v1/users/me/injuries');
export const addInjury = (request: AddInjuryRequest) =>
  authFetch<Injury>('/api/v1/users/me/injuries', { method: 'POST', body: request });
export const updateInjury = (id: number, request: UpdateInjuryRequest) =>
  authFetch<Injury>(`/api/v1/users/me/injuries/${id}`, { method: 'PATCH', body: request });
export const deleteInjury = (id: number) =>
  authFetch<void>(`/api/v1/users/me/injuries/${id}`, { method: 'DELETE' });

export const listBodyRegions = () => authFetch<BodyRegion[]>('/api/v1/body-regions');
export const listMuscles = () => authFetch<Muscle[]>('/api/v1/muscles');
export const listConditions = () => authFetch<InjuryCondition[]>('/api/v1/injury-conditions');
```

- [ ] **Step 5: Replace `Client/src/mocks/handlers.ts`**
```ts
import { http, HttpResponse } from 'msw';
import type {
  BodyRegion,
  Injury,
  InjuryCondition,
  InjuryMuscle,
  InjuryType,
  Muscle,
  Profile,
  Severity,
} from '../api';

// A tiny catalog matching the server tests' in-memory fake.
export const mockRegions: BodyRegion[] = [
  { slug: 'knee', name: 'Knee' },
  { slug: 'lower_back', name: 'Lower back' },
  { slug: 'thigh', name: 'Thigh' },
];

export const mockMuscles: Muscle[] = [
  { id: 1, name: 'Hamstrings', muscle_group: 'legs' },
  { id: 2, name: 'Quads', muscle_group: 'legs' },
  { id: 3, name: 'Calves', muscle_group: 'legs' },
  { id: 4, name: 'Glutes', muscle_group: 'legs' },
  { id: 5, name: 'Erector Spinae', muscle_group: 'back' },
];

export const mockConditions: InjuryCondition[] = [
  {
    id: 1,
    slug: 'hamstring_strain',
    name: 'Hamstring strain',
    icd11_code: 'NC76.30',
    body_region: 'thigh',
    injury_type: 'muscle',
    muscles: [
      { id: 1, name: 'Hamstrings', role: 'primary' },
      { id: 4, name: 'Glutes', role: 'secondary' },
    ],
  },
  {
    id: 2,
    slug: 'patellar_tendinopathy',
    name: 'Patellar tendinopathy',
    icd11_code: null,
    body_region: 'knee',
    injury_type: 'tendon_ligament',
    muscles: [{ id: 2, name: 'Quads', role: 'secondary' }],
  },
];

const defaultProfile: Profile = {
  id: 1,
  email: 'test@example.com',
  display_name: 'Test User',
  sex: null,
  birth_date: null,
  height_cm: 178,
  weight_kg: 74,
  experience_level: 'intermediate',
  unit_preference: 'metric',
  weekly_workout_goal: 3,
  available_time_minutes: null,
  medications: null,
};

// In-memory "database" so add/edit/delete behave like the real server.
// Reset after every test (src/test/setup.ts).
let injuries: Injury[] = [];
let nextId = 1;
let profile: Profile = { ...defaultProfile };

export function resetMockDb(): void {
  injuries = [];
  nextId = 1;
  profile = { ...defaultProfile };
}

export function seedInjury(overrides: Partial<Injury> = {}): Injury {
  const injury: Injury = {
    id: nextId++,
    user_id: 1,
    condition: null,
    body_region: 'lower_back',
    injury_type: 'other',
    severity: 'mild',
    muscles: [],
    active: true,
    notes: null,
    created_at: '2026-10-22T10:00:00Z',
    ...overrides,
  };
  injuries.push(injury);
  return injury;
}

export function setMockProfile(overrides: Partial<Profile>): void {
  profile = { ...profile, ...overrides };
}

function musclesFor(ids: number[]): InjuryMuscle[] {
  return mockMuscles
    .filter((m) => ids.includes(m.id))
    .map((m) => ({ id: m.id, name: m.name, role: 'primary' as const }));
}

// Default (happy-path) handlers. Individual tests override these with
// server.use(...) to exercise error cases.
export const handlers = [
  http.get('/api/v1/health', () => HttpResponse.json({ status: 'ok', db: 'connected' })),

  http.post('/api/v1/auth/register', async ({ request }) => {
    const body = (await request.json()) as { email: string; display_name: string };
    return HttpResponse.json(
      {
        user: { id: 1, email: body.email, display_name: body.display_name },
        access_token: 'fake-jwt-token',
      },
      { status: 201 },
    );
  }),

  http.post('/api/v1/auth/login', async ({ request }) => {
    const body = (await request.json()) as { email: string };
    return HttpResponse.json({
      user: { id: 1, email: body.email, display_name: 'Test User' },
      access_token: 'fake-jwt-token',
    });
  }),

  http.get('/api/v1/auth/me', () =>
    HttpResponse.json({ id: 1, email: 'test@example.com', display_name: 'Test User' }),
  ),

  http.get('/api/v1/users/me', () => HttpResponse.json(profile)),
  http.patch('/api/v1/users/me', async ({ request }) => {
    profile = { ...profile, ...((await request.json()) as Partial<Profile>) };
    return HttpResponse.json(profile);
  }),

  http.get('/api/v1/body-regions', () => HttpResponse.json(mockRegions)),
  http.get('/api/v1/muscles', () => HttpResponse.json(mockMuscles)),
  http.get('/api/v1/injury-conditions', () => HttpResponse.json(mockConditions)),

  http.get('/api/v1/users/me/injuries', () => HttpResponse.json(injuries)),

  http.post('/api/v1/users/me/injuries', async ({ request }) => {
    const body = (await request.json()) as Record<string, unknown>;
    const base = {
      severity: body.severity as Severity,
      notes: (body.notes as string | undefined) ?? null,
    };
    if (typeof body.condition_id === 'number') {
      const c = mockConditions.find((x) => x.id === body.condition_id);
      if (!c) {
        return HttpResponse.json(
          {
            detail: 'One or more fields are invalid.',
            errors: { condition_id: ['condition_id does not match a known condition'] },
          },
          { status: 422 },
        );
      }
      return HttpResponse.json(
        seedInjury({
          ...base,
          condition: { id: c.id, name: c.name, icd11_code: c.icd11_code },
          body_region: c.body_region,
          injury_type: c.injury_type,
          muscles: c.muscles,
        }),
        { status: 201 },
      );
    }
    return HttpResponse.json(
      seedInjury({
        ...base,
        body_region: body.body_region as string,
        injury_type: body.injury_type as InjuryType,
        muscles: musclesFor((body.muscle_ids as number[] | undefined) ?? []),
      }),
      { status: 201 },
    );
  }),

  http.patch('/api/v1/users/me/injuries/:id', async ({ params, request }) => {
    const injury = injuries.find((i) => i.id === Number(params.id));
    if (!injury) return HttpResponse.json({ detail: 'Injury not found.' }, { status: 404 });
    const body = (await request.json()) as Record<string, unknown>;
    if (body.severity !== undefined) injury.severity = body.severity as Severity;
    if (body.notes !== undefined) injury.notes = body.notes as string;
    if (body.active !== undefined) injury.active = body.active as boolean;
    if (body.body_region !== undefined) injury.body_region = body.body_region as string;
    if (body.injury_type !== undefined) injury.injury_type = body.injury_type as InjuryType;
    if (body.muscle_ids !== undefined) injury.muscles = musclesFor(body.muscle_ids as number[]);
    return HttpResponse.json(injury);
  }),

  http.delete('/api/v1/users/me/injuries/:id', ({ params }) => {
    injuries = injuries.filter((i) => i.id !== Number(params.id));
    return new HttpResponse(null, { status: 204 });
  }),
];
```

- [ ] **Step 6: Reset the mock DB after each test** — in `Client/src/test/setup.ts` change the import
line to `import { server } from '../mocks/server';` + `import { resetMockDb } from '../mocks/handlers';`
and the `afterEach` to:
```ts
afterEach(() => {
  server.resetHandlers();
  resetMockDb();
});
```

- [ ] **Step 7: Run to verify it passes**

Run: `npx vitest run src/api.injuries.test.ts` → 6 PASS. Then `npm test` → all pass; `npx tsc --noEmit` → no errors.

- [ ] **Step 8: Commit**
```bash
git add Client/package.json Client/package-lock.json Client/src/api.ts Client/src/mocks/handlers.ts Client/src/test/setup.ts Client/src/api.injuries.test.ts
git commit -m "feat(client): authFetch + typed injuries/profile/catalog API + stateful MSW (TDD)"
```

---

### Task 3: App shell, routing, session-expired logout

**Files:**
- Create: `Client/src/AppShell.tsx`, `Client/src/app-theme.css`
- Create (stubs, replaced in Tasks 4 and 8): `Client/src/features/injuries/InjuriesPage.tsx`, `Client/src/features/profile/ProfilePage.tsx`
- Modify: `Client/src/App.tsx`, `Client/src/App.test.tsx`

**Interfaces:**
- Consumes: `setUnauthorizedHandler` (Task 2).
- Produces: `AppShell({ user: AuthUser; onLogout: () => void })`; routes `/injuries` (heading "My injuries"), `/profile` (heading "Profile"); unknown paths → `/injuries`; on 401 the login screen shows "Your session expired — please log in again."

- [ ] **Step 1: Write the failing tests** — in `Client/src/App.test.tsx`:
1. Add `window.history.pushState({}, '', '/');` as the first line of the `beforeEach`.
2. In `'restores the session and shows the logged-in view when a valid token is stored'`, replace the
   two `expect` lines with:
```tsx
    expect(await screen.findByText('Ivy')).toBeInTheDocument();
    expect(screen.getByRole('link', { name: 'Injuries' })).toHaveAttribute('aria-current', 'page');
```
3. In `'shows the logged-in view after a successful login, and logs out back to the login form'`,
   replace `expect(await screen.findByText(/jill@example.com/i)).toBeInTheDocument();` with
   `expect(await screen.findByRole('heading', { name: 'My injuries' })).toBeInTheDocument();`
4. Append inside the `describe`:
```tsx
  it('switches pages with the tabs and highlights the active one', async () => {
    saveToken('valid-token');
    render(<App />);

    const injuriesTab = await screen.findByRole('link', { name: 'Injuries' });
    expect(injuriesTab).toHaveAttribute('aria-current', 'page');

    fireEvent.click(screen.getByRole('link', { name: 'Profile' }));
    expect(await screen.findByRole('heading', { name: 'Profile' })).toBeInTheDocument();
    expect(screen.getByRole('link', { name: 'Profile' })).toHaveAttribute('aria-current', 'page');
    expect(window.location.pathname).toBe('/profile');
  });

  it('sends unknown paths to the injuries page', async () => {
    window.history.pushState({}, '', '/no-such-page');
    saveToken('valid-token');
    render(<App />);
    expect(await screen.findByRole('heading', { name: 'My injuries' })).toBeInTheDocument();
  });

  it('shows the app name Kinetiq in the top bar', async () => {
    saveToken('valid-token');
    render(<App />);
    // Wait for the logged-in shell (the login card also says "Kinetiq" while
    // the session restores), then check the top-bar logo specifically.
    expect(await screen.findByRole('navigation', { name: 'Main' })).toBeInTheDocument();
    expect(screen.getByText('Kinetiq', { selector: '.app-logo' })).toBeInTheDocument();
  });
```

- [ ] **Step 2: Run to verify it fails**

Run: `npx vitest run src/App.test.tsx`
Expected: FAIL — no link named "Injuries" (the shell doesn't exist yet).

- [ ] **Step 3: Create the page stubs**

`Client/src/features/injuries/InjuriesPage.tsx` (replaced in Task 4):
```tsx
export default function InjuriesPage() {
  return (
    <section className="page" aria-labelledby="injuries-title">
      <h2 id="injuries-title">My injuries</h2>
    </section>
  );
}
```
`Client/src/features/profile/ProfilePage.tsx` (replaced in Task 8):
```tsx
export default function ProfilePage() {
  return (
    <section className="page" aria-labelledby="profile-title">
      <h2 id="profile-title">Profile</h2>
    </section>
  );
}
```

- [ ] **Step 4: Create `AppShell.tsx`**
```tsx
import { NavLink, Outlet } from 'react-router';
import type { AuthUser } from './api';

interface AppShellProps {
  user: AuthUser;
  onLogout: () => void;
}

// The logged-in frame: top bar with tabs, and an <Outlet/> where the router
// renders the current page. NavLink adds aria-current="page" (and our
// "active" class) to the tab whose address is the current one.
export default function AppShell({ user, onLogout }: AppShellProps) {
  const tabClass = ({ isActive }: { isActive: boolean }) => (isActive ? 'app-tab active' : 'app-tab');
  return (
    <div className="app">
      <header className="app-bar">
        <span className="app-logo">Kinetiq</span>
        <nav aria-label="Main" className="app-tabs">
          <NavLink to="/injuries" className={tabClass}>
            Injuries
          </NavLink>
          <NavLink to="/profile" className={tabClass}>
            Profile
          </NavLink>
        </nav>
        <span className="app-user">{user.display_name}</span>
        <button type="button" className="app-logout" onClick={onLogout}>
          Log out
        </button>
      </header>
      <main className="app-main">
        <Outlet />
      </main>
    </div>
  );
}
```

- [ ] **Step 5: Create `app-theme.css`**
```css
/* Logged-in app theme: same "Momentum" palette as auth-theme.css. */
:root {
  --app-bg: #14161a;
  --app-surface: #1b1e23;
  --app-surface-2: rgba(255, 255, 255, 0.045);
  --app-accent: #ff6a4d;
  --app-accent-soft: rgba(255, 106, 77, 0.14);
  --app-ink: #f1efec;
  --app-muted: #a8a29b;
  --app-line: rgba(241, 239, 236, 0.1);
}

.app {
  min-height: 100vh;
  background: var(--app-bg);
  color: var(--app-ink);
  font-family: system-ui, sans-serif;
}

.app-bar {
  display: flex;
  align-items: center;
  gap: 1rem;
  padding: 0.75rem 1.25rem;
  border-bottom: 1px solid var(--app-line);
}
.app-logo { font-weight: 700; color: var(--app-accent); }
.app-tabs { display: flex; gap: 0.25rem; flex: 1; }
.app-tab { color: var(--app-muted); text-decoration: none; padding: 0.35rem 0.75rem; border-radius: 8px; }
.app-tab.active { color: var(--app-accent); background: var(--app-accent-soft); }
.app-user { color: var(--app-muted); }
.app-logout, .app button { font: inherit; }
.app-logout { background: none; border: 1px solid var(--app-line); color: var(--app-ink); border-radius: 8px; padding: 0.3rem 0.7rem; cursor: pointer; }

.app-main { max-width: 640px; margin: 0 auto; padding: 1.5rem 1rem 3rem; }
.page-head { display: flex; justify-content: space-between; align-items: center; }
.muted { color: var(--app-muted); }
.notice { color: var(--app-muted); font-size: 0.85rem; border-left: 2px solid var(--app-accent); padding-left: 0.5rem; }
.field-error { color: #ff8a78; font-size: 0.85rem; margin: 0.25rem 0; }

.app button.primary { background: var(--app-accent); color: #1a0d08; border: none; border-radius: 8px; padding: 0.45rem 1rem; font-weight: 600; cursor: pointer; }
.app button { background: var(--app-surface-2); color: var(--app-ink); border: 1px solid var(--app-line); border-radius: 8px; padding: 0.35rem 0.75rem; cursor: pointer; }
.app button.link { background: none; border: none; color: var(--app-accent); padding: 0; }

.injury-card, .injury-form, .profile-form fieldset {
  background: var(--app-surface);
  border: 1px solid var(--app-line);
  border-radius: 12px;
  padding: 0.9rem 1rem;
  margin: 0.75rem 0;
}
.injury-card h3, .injury-form h3 { margin: 0 0 0.25rem; }
.card-actions { display: flex; gap: 0.5rem; align-items: center; margin-top: 0.6rem; }

.pills { list-style: none; padding: 0; margin: 0.4rem 0 0; display: flex; flex-wrap: wrap; gap: 0.3rem; }
.pill { border-radius: 999px; padding: 0.1rem 0.6rem; font-size: 0.8rem; }
.pill-primary { background: var(--app-accent-soft); color: var(--app-accent); }
.pill-secondary { background: var(--app-surface-2); color: var(--app-muted); }

.locked { border: 1px dashed var(--app-line); border-radius: 10px; padding: 0.6rem 0.8rem; margin: 0.5rem 0; }
.condition-results { list-style: none; padding: 0; margin: 0.4rem 0; display: grid; gap: 0.25rem; }
.condition-results button { width: 100%; text-align: left; }
.condition-results .unlisted { color: var(--app-accent); font-style: italic; }

.app input, .app select, .app textarea {
  width: 100%;
  box-sizing: border-box;
  background: var(--app-surface-2);
  color: var(--app-ink);
  border: 1px solid var(--app-line);
  border-radius: 8px;
  padding: 0.45rem 0.6rem;
  font: inherit;
}
.app input[type='radio'], .app input[type='checkbox'] { width: auto; margin-right: 0.35rem; }
.app fieldset { border: none; padding: 0; margin: 0.6rem 0; }
.app legend, .app label { display: block; color: var(--app-muted); margin: 0.5rem 0 0.25rem; }
.choice { display: inline-flex !important; align-items: center; margin-right: 0.8rem !important; color: var(--app-ink) !important; }
.chip { display: inline-flex !important; align-items: center; border: 1px solid var(--app-line); border-radius: 999px; padding: 0.1rem 0.6rem; margin: 0.15rem 0.25rem 0 0 !important; color: var(--app-ink) !important; }
.chip-on { border-color: var(--app-accent); color: var(--app-accent) !important; }
.muscle-group-name { display: block; color: var(--app-muted); font-size: 0.8rem; text-transform: capitalize; margin-top: 0.4rem; }
.form-actions { display: flex; gap: 0.5rem; margin-top: 0.8rem; }
.healed summary { cursor: pointer; color: var(--app-muted); margin-top: 1rem; }
.auth-notice { color: var(--auth-accent); }
```

- [ ] **Step 6: Rewrite the logged-in part of `App.tsx`**

Replace the imports with:
```tsx
import { useEffect, useState } from 'react';
import { BrowserRouter, Navigate, Route, Routes } from 'react-router';
import {
  fetchHealth,
  fetchMe,
  setUnauthorizedHandler,
  type AuthResult,
  type AuthUser,
  type HealthStatus,
} from './api';
import AppShell from './AppShell';
import AuthPage from './features/auth/AuthPage';
import InjuriesPage from './features/injuries/InjuriesPage';
import ProfilePage from './features/profile/ProfilePage';
import './features/auth/auth-theme.css';
import './app-theme.css';
import { clearToken, getToken, saveToken } from './lib/tokenStorage';
```
After the `isRestoringSession` state line add:
```tsx
  const [notice, setNotice] = useState<string | null>(null);

  // Any authenticated call that gets a 401 (24h token expired) lands here.
  useEffect(() => {
    setUnauthorizedHandler(() => {
      clearToken();
      setCurrentUser(null);
      setNotice('Your session expired — please log in again.');
    });
    return () => setUnauthorizedHandler(null);
  }, []);
```
In `handleAuthenticated`, add `setNotice(null);` as the first line.
In the login branch, directly above `{isRestoringSession ? (`, add:
```tsx
          {notice && (
            <p className="auth-notice" role="status">
              {notice}
            </p>
          )}
```
Replace the whole final `return ( <main ...> ... </main> );` (the walking-skeleton view) with:
```tsx
  return (
    <BrowserRouter>
      <Routes>
        <Route element={<AppShell user={currentUser} onLogout={handleLogout} />}>
          <Route path="/injuries" element={<InjuriesPage />} />
          <Route path="/profile" element={<ProfilePage />} />
          <Route path="*" element={<Navigate to="/injuries" replace />} />
        </Route>
      </Routes>
    </BrowserRouter>
  );
```

- [ ] **Step 7: Run to verify it passes**

Run: `npx vitest run src/App.test.tsx` → all PASS. Then `npm test` and `npx tsc --noEmit` → clean.

- [ ] **Step 8: Commit**
```bash
git add Client/src/AppShell.tsx Client/src/app-theme.css Client/src/features/injuries/InjuriesPage.tsx Client/src/features/profile/ProfilePage.tsx Client/src/App.tsx Client/src/App.test.tsx
git commit -m "feat(client): app shell with Injuries/Profile tabs, routing, session-expired logout (TDD)"
```

---

### Task 4: Injuries list — cards, healed section, mark healed, delete

**Files:**
- Create: `Client/src/features/injuries/types.ts`, `Client/src/features/injuries/InjuryCard.tsx`, `Client/src/features/injuries/InjuriesPage.test.tsx`
- Modify: `Client/src/features/injuries/InjuriesPage.tsx` (replace stub), `Client/src/App.test.tsx` (401 test)

**Interfaces:**
- Consumes: `listInjuries`, `listConditions`, `listBodyRegions`, `listMuscles`, `updateInjury`, `deleteInjury` (Task 2).
- Produces: `interface Catalog { conditions: InjuryCondition[]; regions: BodyRegion[]; muscles: Muscle[] }` (in `types.ts`); `InjuryCard({ injury, regionName, onEdit?, onToggleActive, onDelete })`.

- [ ] **Step 1: Write the failing tests** — `Client/src/features/injuries/InjuriesPage.test.tsx`:
```tsx
import { fireEvent, render, screen, within } from '@testing-library/react';
import { beforeEach, describe, expect, it } from 'vitest';
import InjuriesPage from './InjuriesPage';
import { mockConditions, seedInjury } from '../../mocks/handlers';
import { saveToken } from '../../lib/tokenStorage';

describe('InjuriesPage — list', () => {
  beforeEach(() => {
    localStorage.clear();
    saveToken('test-token');
  });

  it('shows an empty state', async () => {
    render(<InjuriesPage />);
    expect(await screen.findByText(/no injuries yet/i)).toBeInTheDocument();
  });

  it('shows a known injury with its catalog muscles and roles', async () => {
    const c = mockConditions[0];
    seedInjury({
      condition: { id: c.id, name: c.name, icd11_code: c.icd11_code },
      body_region: 'thigh',
      injury_type: 'muscle',
      severity: 'moderate',
      muscles: c.muscles,
    });
    render(<InjuriesPage />);

    const card = await screen.findByRole('article', { name: 'Hamstring strain' });
    expect(within(card).getByText(/thigh · moderate/i)).toBeInTheDocument();
    const pills = within(card).getByRole('list', { name: /affected muscles/i });
    expect(within(pills).getByText('Hamstrings')).toHaveClass('pill-primary');
    expect(within(pills).getByText('Glutes')).toHaveClass('pill-secondary');
  });

  it('names an unlisted injury by its region', async () => {
    seedInjury({ body_region: 'lower_back' });
    render(<InjuriesPage />);
    expect(await screen.findByRole('article', { name: 'Other — Lower back' })).toBeInTheDocument();
  });

  it('moves a healed injury into the Healed section and back', async () => {
    seedInjury({ body_region: 'knee' });
    render(<InjuriesPage />);

    fireEvent.click(await screen.findByRole('button', { name: 'Mark healed' }));
    expect(await screen.findByText('Healed (1)')).toBeInTheDocument();
    expect(screen.getByText(/no injuries yet/i)).toBeInTheDocument();

    fireEvent.click(screen.getByRole('button', { name: 'Mark active' }));
    expect(await screen.findByRole('button', { name: 'Mark healed' })).toBeInTheDocument();
    expect(screen.queryByText(/healed \(/i)).not.toBeInTheDocument();
  });

  it('asks for confirmation before deleting', async () => {
    seedInjury({ body_region: 'knee' });
    render(<InjuriesPage />);

    fireEvent.click(await screen.findByRole('button', { name: 'Delete' }));
    expect(screen.getByText('Delete this injury?')).toBeInTheDocument();
    fireEvent.click(screen.getByRole('button', { name: 'Cancel' }));
    expect(screen.getByRole('article', { name: 'Other — Knee' })).toBeInTheDocument();

    fireEvent.click(screen.getByRole('button', { name: 'Delete' }));
    fireEvent.click(screen.getByRole('button', { name: 'Yes, delete' }));
    expect(await screen.findByText(/no injuries yet/i)).toBeInTheDocument();
  });
});
```
Append to `Client/src/App.test.tsx` inside the `describe`:
```tsx
  it('logs out with a message when the session has expired', async () => {
    saveToken('stale-token');
    server.use(
      http.get('/api/v1/users/me/injuries', () =>
        HttpResponse.json({ detail: 'Missing, invalid, or expired token.' }, { status: 401 }),
      ),
    );
    render(<App />);

    expect(await screen.findByText(/your session expired/i)).toBeInTheDocument();
    expect(screen.getByRole('button', { name: /^log in$/i })).toBeInTheDocument();
    expect(getToken()).toBeNull();
  });
```

- [ ] **Step 2: Run to verify it fails**

Run: `npx vitest run src/features/injuries src/App.test.tsx`
Expected: FAIL — "no injuries yet" not found (stub page), session-expired message not found.

- [ ] **Step 3: Create `types.ts`**
```ts
import type { BodyRegion, InjuryCondition, Muscle } from '../../api';

// Reference data the injuries screen loads once and passes down.
export interface Catalog {
  conditions: InjuryCondition[];
  regions: BodyRegion[];
  muscles: Muscle[];
}
```

- [ ] **Step 4: Create `InjuryCard.tsx`**
```tsx
import { useState } from 'react';
import type { Injury } from '../../api';

interface InjuryCardProps {
  injury: Injury;
  regionName: string;
  onEdit?: () => void;
  onToggleActive: () => void;
  onDelete: () => void;
}

export default function InjuryCard({
  injury,
  regionName,
  onEdit,
  onToggleActive,
  onDelete,
}: InjuryCardProps) {
  const [confirmingDelete, setConfirmingDelete] = useState(false);
  const title = injury.condition ? injury.condition.name : `Other — ${regionName}`;

  return (
    <article className="injury-card" aria-label={title}>
      <h3>{title}</h3>
      <p className="muted">
        {regionName} · {injury.severity}
        {injury.notes ? ` · ${injury.notes}` : ''}
      </p>
      {injury.muscles.length > 0 && (
        <ul className="pills" aria-label="Affected muscles">
          {injury.muscles.map((m) => (
            <li key={m.id} className={`pill pill-${m.role}`}>
              {m.name}
            </li>
          ))}
        </ul>
      )}
      {confirmingDelete ? (
        <div className="card-actions">
          <span>Delete this injury?</span>
          <button type="button" onClick={onDelete}>
            Yes, delete
          </button>
          <button type="button" onClick={() => setConfirmingDelete(false)}>
            Cancel
          </button>
        </div>
      ) : (
        <div className="card-actions">
          {onEdit && (
            <button type="button" onClick={onEdit}>
              Edit
            </button>
          )}
          <button type="button" onClick={onToggleActive}>
            {injury.active ? 'Mark healed' : 'Mark active'}
          </button>
          <button type="button" onClick={() => setConfirmingDelete(true)}>
            Delete
          </button>
        </div>
      )}
    </article>
  );
}
```

- [ ] **Step 5: Replace `InjuriesPage.tsx`**
```tsx
import { useEffect, useState } from 'react';
import {
  deleteInjury,
  listBodyRegions,
  listConditions,
  listInjuries,
  listMuscles,
  updateInjury,
  type Injury,
} from '../../api';
import InjuryCard from './InjuryCard';
import type { Catalog } from './types';

const message = (e: unknown) => (e instanceof Error ? e.message : String(e));

export default function InjuriesPage() {
  const [injuries, setInjuries] = useState<Injury[]>([]);
  const [catalog, setCatalog] = useState<Catalog | null>(null);
  const [loadError, setLoadError] = useState<string | null>(null);
  const [actionError, setActionError] = useState<string | null>(null);

  // Load the user's injuries and the reference catalog together, once.
  useEffect(() => {
    Promise.all([listInjuries(), listConditions(), listBodyRegions(), listMuscles()])
      .then(([loaded, conditions, regions, muscles]) => {
        setInjuries(loaded);
        setCatalog({ conditions, regions, muscles });
      })
      .catch((e: unknown) => setLoadError(message(e)));
  }, []);

  const regionName = (slug: string) => catalog?.regions.find((r) => r.slug === slug)?.name ?? slug;
  const replace = (updated: Injury) =>
    setInjuries((list) => list.map((i) => (i.id === updated.id ? updated : i)));

  async function toggleActive(injury: Injury) {
    setActionError(null);
    try {
      replace(await updateInjury(injury.id, { active: !injury.active }));
    } catch (e) {
      setActionError(message(e));
    }
  }

  async function remove(injury: Injury) {
    setActionError(null);
    try {
      await deleteInjury(injury.id);
      setInjuries((list) => list.filter((i) => i.id !== injury.id));
    } catch (e) {
      setActionError(message(e));
    }
  }

  if (loadError) return <p role="alert">Couldn't load injuries: {loadError}</p>;
  if (!catalog) return <p>Loading…</p>;

  const active = injuries.filter((i) => i.active);
  const healed = injuries.filter((i) => !i.active);
  const card = (i: Injury) => (
    <InjuryCard
      key={i.id}
      injury={i}
      regionName={regionName(i.body_region)}
      onToggleActive={() => toggleActive(i)}
      onDelete={() => remove(i)}
    />
  );

  return (
    <section className="page" aria-labelledby="injuries-title">
      <div className="page-head">
        <h2 id="injuries-title">My injuries</h2>
      </div>
      {actionError && (
        <p role="alert" className="field-error">
          {actionError}
        </p>
      )}
      {active.length === 0 && <p className="muted">No injuries yet.</p>}
      {active.map(card)}
      {healed.length > 0 && (
        <details className="healed">
          <summary>Healed ({healed.length})</summary>
          {healed.map(card)}
        </details>
      )}
    </section>
  );
}
```

- [ ] **Step 6: Run to verify it passes**

Run: `npx vitest run src/features/injuries src/App.test.tsx` → all PASS. `npm test`, `npx tsc --noEmit` → clean.

- [ ] **Step 7: Commit**
```bash
git add Client/src/features/injuries Client/src/App.test.tsx
git commit -m "feat(client): injuries list with cards, healed section, mark healed, delete confirm (TDD)"
```

---

### Task 5: Injury form rules — Zod schema + request converters (pure)

**Files:**
- Create: `Client/src/features/injuries/injurySchema.ts`, `Client/src/features/injuries/injurySchema.test.ts`

**Interfaces:**
- Consumes: `Injury`, `AddInjuryRequest`, `UpdateInjuryRequest`, `Severity`, `InjuryType` (Task 2).
- Produces:
  - `SEVERITIES`, `INJURY_TYPES`, `TYPE_LABELS: Record<InjuryType, string>`
  - `injuryFormSchema`; `type InjuryFormValues = { conditionId: number | null; unlisted: boolean; bodyRegion: string; injuryType: string; muscleIds: number[]; severity: string; notes: string }`
  - `formDefaults(initial?: Injury): InjuryFormValues`
  - `toAddRequest(v: InjuryFormValues): AddInjuryRequest`
  - `toUpdateRequest(v: InjuryFormValues, initial: Injury): UpdateInjuryRequest`
  - `SERVER_FIELD_TO_FORM: Record<string, keyof InjuryFormValues>`

- [ ] **Step 1: Write the failing test** — `injurySchema.test.ts`:
```ts
import { describe, expect, it } from 'vitest';
import type { Injury } from '../../api';
import {
  formDefaults,
  injuryFormSchema,
  toAddRequest,
  toUpdateRequest,
  type InjuryFormValues,
} from './injurySchema';

const known: InjuryFormValues = {
  ...formDefaults(),
  conditionId: 1,
  severity: 'moderate',
};
const unlisted: InjuryFormValues = {
  ...formDefaults(),
  unlisted: true,
  bodyRegion: 'thigh',
  injuryType: 'joint',
  muscleIds: [2, 3],
  severity: 'mild',
};

function issuePaths(values: InjuryFormValues): string[] {
  const result = injuryFormSchema.safeParse(values);
  return result.success ? [] : result.error.issues.map((i) => i.path.join('.'));
}

const savedKnown: Injury = {
  id: 7,
  user_id: 1,
  condition: { id: 1, name: 'Hamstring strain', icd11_code: 'NC76.30' },
  body_region: 'thigh',
  injury_type: 'muscle',
  severity: 'moderate',
  muscles: [{ id: 1, name: 'Hamstrings', role: 'primary' }],
  active: true,
  notes: null,
  created_at: '2026-10-22T10:00:00Z',
};
const savedUnlisted: Injury = {
  ...savedKnown,
  condition: null,
  body_region: 'thigh',
  injury_type: 'joint',
  muscles: [
    { id: 2, name: 'Quads', role: 'primary' },
    { id: 3, name: 'Calves', role: 'primary' },
  ],
};

describe('injuryFormSchema', () => {
  it('accepts a known condition with a severity', () => {
    expect(issuePaths(known)).toEqual([]);
  });

  it('requires a severity', () => {
    expect(issuePaths({ ...known, severity: '' })).toEqual(['severity']);
  });

  it('requires a condition unless the injury is unlisted', () => {
    expect(issuePaths({ ...known, conditionId: null })).toEqual(['conditionId']);
  });

  it('requires region and type for an unlisted injury', () => {
    expect(issuePaths({ ...unlisted, bodyRegion: '', injuryType: '' }).sort()).toEqual([
      'bodyRegion',
      'injuryType',
    ]);
  });

  it('limits notes to 500 characters', () => {
    expect(issuePaths({ ...known, notes: 'x'.repeat(501) })).toEqual(['notes']);
  });
});

describe('toAddRequest', () => {
  it('sends only condition, severity and (trimmed) notes for a known condition', () => {
    expect(toAddRequest(known)).toEqual({ condition_id: 1, severity: 'moderate' });
    expect(toAddRequest({ ...known, notes: '  left leg ' })).toEqual({
      condition_id: 1,
      severity: 'moderate',
      notes: 'left leg',
    });
  });

  it('sends region, type and muscles for an unlisted injury', () => {
    expect(toAddRequest(unlisted)).toEqual({
      body_region: 'thigh',
      injury_type: 'joint',
      muscle_ids: [2, 3],
      severity: 'mild',
    });
  });
});

describe('toUpdateRequest', () => {
  it('sends only what changed', () => {
    expect(toUpdateRequest(formDefaults(savedKnown), savedKnown)).toEqual({});
    expect(toUpdateRequest({ ...formDefaults(savedKnown), severity: 'severe' }, savedKnown)).toEqual({
      severity: 'severe',
    });
  });

  it('never sends locked fields for a known condition', () => {
    const values = { ...formDefaults(savedKnown), bodyRegion: 'knee', muscleIds: [9] };
    expect(toUpdateRequest(values, savedKnown)).toEqual({});
  });

  it('sends region, type and the full muscle set for an unlisted injury', () => {
    const values = { ...formDefaults(savedUnlisted), injuryType: 'bone', muscleIds: [3] };
    expect(toUpdateRequest(values, savedUnlisted)).toEqual({ injury_type: 'bone', muscle_ids: [3] });
    // Same muscles in another order = no change.
    expect(toUpdateRequest({ ...formDefaults(savedUnlisted), muscleIds: [3, 2] }, savedUnlisted)).toEqual({});
  });

  it('can clear notes', () => {
    const saved = { ...savedKnown, notes: 'old' };
    expect(toUpdateRequest({ ...formDefaults(saved), notes: '' }, saved)).toEqual({ notes: '' });
  });
});

describe('formDefaults', () => {
  it('pre-fills an edit form from a saved injury', () => {
    expect(formDefaults(savedUnlisted)).toEqual({
      conditionId: null,
      unlisted: true,
      bodyRegion: 'thigh',
      injuryType: 'joint',
      muscleIds: [2, 3],
      severity: 'moderate',
      notes: '',
    });
  });
});
```

- [ ] **Step 2: Run to verify it fails**

Run: `npx vitest run src/features/injuries/injurySchema.test.ts`
Expected: FAIL — `Failed to resolve import "./injurySchema"`.

- [ ] **Step 3: Write `injurySchema.ts`**
```ts
import { z } from 'zod';
import type {
  AddInjuryRequest,
  Injury,
  InjuryType,
  Severity,
  UpdateInjuryRequest,
} from '../../api';

export const SEVERITIES = ['mild', 'moderate', 'severe'] as const;
export const INJURY_TYPES = ['muscle', 'bone', 'tendon_ligament', 'joint', 'other'] as const;

export const TYPE_LABELS: Record<InjuryType, string> = {
  muscle: 'Muscle',
  bone: 'Bone',
  tendon_ligament: 'Tendon / ligament',
  joint: 'Joint',
  other: 'Other',
};

// The form's values. Everything is a plain string/number so inputs can start
// empty; the rules below (mirroring the server) decide what's valid.
export const injuryFormSchema = z
  .object({
    conditionId: z.number().nullable(), // set = known condition
    unlisted: z.boolean(), // true = "My injury isn't listed…"
    bodyRegion: z.string(),
    injuryType: z.string(),
    muscleIds: z.array(z.number()),
    severity: z
      .string()
      .refine((s) => (SEVERITIES as readonly string[]).includes(s), 'Choose a severity'),
    notes: z.string().max(500, 'Notes can be at most 500 characters'),
  })
  .superRefine((v, ctx) => {
    if (!v.unlisted && v.conditionId === null) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['conditionId'],
        message: "Pick an injury, or choose \"My injury isn't listed…\"",
      });
    }
    if (v.unlisted) {
      if (v.bodyRegion === '') {
        ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['bodyRegion'], message: 'Choose where the injury is' });
      }
      if (!(INJURY_TYPES as readonly string[]).includes(v.injuryType)) {
        ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['injuryType'], message: 'Choose the injury type' });
      }
    }
  });

export type InjuryFormValues = z.infer<typeof injuryFormSchema>;

export function formDefaults(initial?: Injury): InjuryFormValues {
  if (!initial) {
    return {
      conditionId: null,
      unlisted: false,
      bodyRegion: '',
      injuryType: '',
      muscleIds: [],
      severity: '',
      notes: '',
    };
  }
  return {
    conditionId: initial.condition?.id ?? null,
    unlisted: initial.condition === null,
    bodyRegion: initial.body_region,
    injuryType: initial.injury_type,
    muscleIds: initial.muscles.map((m) => m.id),
    severity: initial.severity,
    notes: initial.notes ?? '',
  };
}

// Builds the POST body: the known-condition shape or the unlisted shape.
export function toAddRequest(v: InjuryFormValues): AddInjuryRequest {
  const notes = v.notes.trim();
  const withNotes = notes ? { notes } : {};
  if (!v.unlisted && v.conditionId !== null) {
    return { condition_id: v.conditionId, severity: v.severity as Severity, ...withNotes };
  }
  return {
    body_region: v.bodyRegion,
    injury_type: v.injuryType as InjuryType,
    muscle_ids: v.muscleIds,
    severity: v.severity as Severity,
    ...withNotes,
  };
}

const sameIds = (a: number[], b: number[]) =>
  a.length === b.length && [...a].sort().every((id, i) => id === [...b].sort()[i]);

// Builds the PATCH body with only the fields that changed. A known condition's
// region/type/muscles are locked, so they are never sent for one.
export function toUpdateRequest(v: InjuryFormValues, initial: Injury): UpdateInjuryRequest {
  const request: UpdateInjuryRequest = {};
  if (v.severity !== initial.severity) request.severity = v.severity as Severity;
  const notes = v.notes.trim();
  if (notes !== (initial.notes ?? '')) request.notes = notes;
  if (initial.condition === null) {
    if (v.bodyRegion !== initial.body_region) request.body_region = v.bodyRegion;
    if (v.injuryType !== initial.injury_type) request.injury_type = v.injuryType as InjuryType;
    const savedIds = initial.muscles.map((m) => m.id);
    if (!sameIds(v.muscleIds, savedIds)) request.muscle_ids = v.muscleIds;
  }
  return request;
}

// Server field name → form field name, so a 422 lands under the right input.
export const SERVER_FIELD_TO_FORM: Record<string, keyof InjuryFormValues> = {
  condition_id: 'conditionId',
  body_region: 'bodyRegion',
  injury_type: 'injuryType',
  muscle_ids: 'muscleIds',
  severity: 'severity',
  notes: 'notes',
};
```

- [ ] **Step 4: Run to verify it passes**

Run: `npx vitest run src/features/injuries/injurySchema.test.ts` → all PASS. `npx tsc --noEmit` → clean.

- [ ] **Step 5: Commit**
```bash
git add Client/src/features/injuries/injurySchema.ts Client/src/features/injuries/injurySchema.test.ts
git commit -m "feat(client): injury form Zod schema + add/update request converters (TDD)"
```

---

### Task 6: `ConditionSearch`, `MuscleSelect`, `InjuryForm`

**Files:**
- Create: `Client/src/features/injuries/ConditionSearch.tsx`, `MuscleSelect.tsx`, `InjuryForm.tsx`, `InjuryForm.test.tsx`

**Interfaces:**
- Consumes: Task 5 exports; `Catalog` (Task 4); `ApiError` (Task 2).
- Produces:
  - `ConditionSearch({ conditions, regionName, onSelect(c), onUnlisted(), error? })`
  - `MuscleSelect({ muscles, value: number[], onChange(ids: number[]) })`
  - `InjuryForm({ catalog: Catalog; initial?: Injury; onSave(values: InjuryFormValues): Promise<void>; onCancel(): void })` — form `aria-label` is "Add injury" or "Edit injury".

- [ ] **Step 1: Write the failing test** — `InjuryForm.test.tsx`:
```tsx
import { fireEvent, render, screen, waitFor, within } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';
import { ApiError, type Injury } from '../../api';
import { mockConditions, mockMuscles, mockRegions } from '../../mocks/handlers';
import InjuryForm from './InjuryForm';
import type { Catalog } from './types';

const catalog: Catalog = { conditions: mockConditions, regions: mockRegions, muscles: mockMuscles };

function renderForm(props: Partial<Parameters<typeof InjuryForm>[0]> = {}) {
  const onSave = vi.fn().mockResolvedValue(undefined);
  const onCancel = vi.fn();
  render(<InjuryForm catalog={catalog} onSave={onSave} onCancel={onCancel} {...props} />);
  return { onSave: (props.onSave as typeof onSave) ?? onSave, onCancel };
}

describe('InjuryForm — add', () => {
  it('filters conditions as you type and always offers "not listed"', () => {
    renderForm();
    fireEvent.change(screen.getByLabelText("What's the injury?"), { target: { value: 'ham' } });
    expect(screen.getByRole('button', { name: /hamstring strain/i })).toBeInTheDocument();
    expect(screen.queryByRole('button', { name: /patellar/i })).not.toBeInTheDocument();
    expect(screen.getByRole('button', { name: /my injury isn't listed/i })).toBeInTheDocument();
  });

  it('shows a known condition locked, then saves condition + severity', async () => {
    const { onSave } = renderForm();
    fireEvent.click(screen.getByRole('button', { name: /hamstring strain/i }));

    const details = screen.getByRole('region', { name: 'Condition details' });
    expect(within(details).getByText(/ICD-11 NC76.30/)).toBeInTheDocument();
    expect(within(details).getByText('Hamstrings · primary')).toBeInTheDocument();
    expect(within(details).getByText('Glutes · secondary')).toBeInTheDocument();
    expect(within(details).getByText(/not medical advice/i)).toBeInTheDocument();
    expect(screen.queryByLabelText('Where is it?')).not.toBeInTheDocument();

    fireEvent.click(screen.getByRole('radio', { name: 'Moderate' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    await waitFor(() =>
      expect(onSave).toHaveBeenCalledWith(
        expect.objectContaining({ conditionId: 1, unlisted: false, severity: 'moderate' }),
      ),
    );
  });

  it('says "no ICD-11 code" when a condition has none', () => {
    renderForm();
    fireEvent.click(screen.getByRole('button', { name: /patellar tendinopathy/i }));
    expect(screen.getByText(/no ICD-11 code/)).toBeInTheDocument();
  });

  it('requires a severity before saving', async () => {
    const { onSave } = renderForm();
    fireEvent.click(screen.getByRole('button', { name: /hamstring strain/i }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    expect(await screen.findByText('Choose a severity')).toBeInTheDocument();
    expect(onSave).not.toHaveBeenCalled();
  });

  it('asks for region and type when the injury is not listed', async () => {
    renderForm();
    fireEvent.click(screen.getByRole('button', { name: /my injury isn't listed/i }));
    fireEvent.click(screen.getByRole('radio', { name: 'Mild' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    expect(await screen.findByText('Choose where the injury is')).toBeInTheDocument();
    expect(screen.getByText('Choose the injury type')).toBeInTheDocument();
  });

  it('saves an unlisted injury with region, type and muscles', async () => {
    const { onSave } = renderForm();
    fireEvent.click(screen.getByRole('button', { name: /my injury isn't listed/i }));
    fireEvent.change(screen.getByLabelText('Where is it?'), { target: { value: 'thigh' } });
    fireEvent.click(screen.getByRole('radio', { name: 'Joint' }));
    fireEvent.click(screen.getByRole('checkbox', { name: 'Quads' }));
    fireEvent.click(screen.getByRole('radio', { name: 'Mild' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    await waitFor(() =>
      expect(onSave).toHaveBeenCalledWith(
        expect.objectContaining({
          unlisted: true,
          bodyRegion: 'thigh',
          injuryType: 'joint',
          muscleIds: [2],
          severity: 'mild',
        }),
      ),
    );
  });

  it('shows a server field error under the field', async () => {
    const onSave = vi
      .fn()
      .mockRejectedValue(new ApiError(422, 'invalid', { severity: ['severity is not allowed'] }));
    renderForm({ onSave });
    fireEvent.click(screen.getByRole('button', { name: /hamstring strain/i }));
    fireEvent.click(screen.getByRole('radio', { name: 'Mild' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    expect(await screen.findByText('severity is not allowed')).toBeInTheDocument();
  });
});

describe('InjuryForm — edit', () => {
  const saved: Injury = {
    id: 7,
    user_id: 1,
    condition: { id: 1, name: 'Hamstring strain', icd11_code: 'NC76.30' },
    body_region: 'thigh',
    injury_type: 'muscle',
    severity: 'moderate',
    muscles: mockConditions[0].muscles,
    active: true,
    notes: null,
    created_at: '2026-10-22T10:00:00Z',
  };

  it('keeps a known condition locked and explains how to change it', () => {
    renderForm({ initial: saved });
    expect(screen.getByRole('form', { name: 'Edit injury' })).toBeInTheDocument();
    expect(screen.queryByLabelText("What's the injury?")).not.toBeInTheDocument();
    expect(screen.getByText(/delete this injury and add it again/i)).toBeInTheDocument();
    expect(screen.getByRole('radio', { name: 'Moderate' })).toBeChecked();
  });
});
```

- [ ] **Step 2: Run to verify it fails**

Run: `npx vitest run src/features/injuries/InjuryForm.test.tsx`
Expected: FAIL — `Failed to resolve import "./InjuryForm"`.

- [ ] **Step 3: Write `ConditionSearch.tsx`**
```tsx
import { useState } from 'react';
import type { InjuryCondition } from '../../api';

interface ConditionSearchProps {
  conditions: InjuryCondition[];
  regionName: (slug: string) => string;
  onSelect: (condition: InjuryCondition) => void;
  onUnlisted: () => void;
  error?: string;
}

// Type to filter the known conditions; "My injury isn't listed…" is always last.
export default function ConditionSearch({
  conditions,
  regionName,
  onSelect,
  onUnlisted,
  error,
}: ConditionSearchProps) {
  const [query, setQuery] = useState('');
  const q = query.trim().toLowerCase();
  const matches = conditions.filter((c) => c.name.toLowerCase().includes(q));

  return (
    <div className="condition-search">
      <label htmlFor="condition-query">What's the injury?</label>
      <input
        id="condition-query"
        type="search"
        autoComplete="off"
        placeholder="Start typing, e.g. hamstring"
        value={query}
        onChange={(e) => setQuery(e.target.value)}
      />
      <ul className="condition-results" aria-label="Matching conditions">
        {matches.map((c) => (
          <li key={c.id}>
            <button type="button" onClick={() => onSelect(c)}>
              {c.name} <span className="muted">· {regionName(c.body_region)}</span>
            </button>
          </li>
        ))}
        <li>
          <button type="button" className="unlisted" onClick={onUnlisted}>
            My injury isn't listed…
          </button>
        </li>
      </ul>
      {error && (
        <p role="alert" className="field-error">
          {error}
        </p>
      )}
    </div>
  );
}
```

- [ ] **Step 4: Write `MuscleSelect.tsx`**
```tsx
import type { Muscle } from '../../api';

interface MuscleSelectProps {
  muscles: Muscle[];
  value: number[];
  onChange: (ids: number[]) => void;
}

// Muscles as tappable chips (real checkboxes underneath, for accessibility),
// grouped by muscle group.
export default function MuscleSelect({ muscles, value, onChange }: MuscleSelectProps) {
  const groups = [...new Set(muscles.map((m) => m.muscle_group))];
  const toggle = (id: number) =>
    onChange(value.includes(id) ? value.filter((v) => v !== id) : [...value, id]);

  return (
    <fieldset className="muscle-select">
      <legend>Muscles involved (optional)</legend>
      {groups.map((group) => (
        <div key={group} role="group" aria-label={group}>
          <span className="muscle-group-name">{group}</span>
          {muscles
            .filter((m) => m.muscle_group === group)
            .map((m) => (
              <label key={m.id} className={value.includes(m.id) ? 'chip chip-on' : 'chip'}>
                <input type="checkbox" checked={value.includes(m.id)} onChange={() => toggle(m.id)} />
                {m.name}
              </label>
            ))}
        </div>
      ))}
    </fieldset>
  );
}
```

- [ ] **Step 5: Write `InjuryForm.tsx`**
```tsx
import { zodResolver } from '@hookform/resolvers/zod';
import { Controller, useForm } from 'react-hook-form';
import { ApiError, type Injury, type InjuryCondition, type InjuryType } from '../../api';
import ConditionSearch from './ConditionSearch';
import MuscleSelect from './MuscleSelect';
import {
  INJURY_TYPES,
  SERVER_FIELD_TO_FORM,
  SEVERITIES,
  TYPE_LABELS,
  formDefaults,
  injuryFormSchema,
  type InjuryFormValues,
} from './injurySchema';
import type { Catalog } from './types';

interface InjuryFormProps {
  catalog: Catalog;
  initial?: Injury; // present = edit mode
  onSave: (values: InjuryFormValues) => Promise<void>;
  onCancel: () => void;
}

const capitalize = (s: string) => s.charAt(0).toUpperCase() + s.slice(1);

export default function InjuryForm({ catalog, initial, onSave, onCancel }: InjuryFormProps) {
  const isEdit = initial !== undefined;
  const {
    register,
    control,
    handleSubmit,
    watch,
    setValue,
    setError,
    formState: { errors, isSubmitting },
  } = useForm<InjuryFormValues>({
    resolver: zodResolver(injuryFormSchema),
    defaultValues: formDefaults(initial),
  });

  const conditionId = watch('conditionId');
  const unlisted = watch('unlisted');
  const condition = catalog.conditions.find((c) => c.id === conditionId);
  const regionName = (slug: string) => catalog.regions.find((r) => r.slug === slug)?.name ?? slug;

  const chooseCondition = (c: InjuryCondition) => {
    setValue('conditionId', c.id);
    setValue('unlisted', false);
  };
  const chooseUnlisted = () => {
    setValue('conditionId', null);
    setValue('unlisted', true);
  };
  const backToSearch = () => {
    setValue('conditionId', null);
    setValue('unlisted', false);
  };

  // The server has the final say: a 422 names fields, and each message is
  // shown under the matching input.
  async function submit(values: InjuryFormValues) {
    try {
      await onSave(values);
    } catch (err) {
      if (err instanceof ApiError && Object.keys(err.fieldErrors).length > 0) {
        for (const [field, messages] of Object.entries(err.fieldErrors)) {
          const formField = SERVER_FIELD_TO_FORM[field];
          if (formField) setError(formField, { message: messages[0] });
          else setError('root.server', { message: messages[0] });
        }
      } else {
        setError('root.server', { message: err instanceof Error ? err.message : String(err) });
      }
    }
  }

  const fieldError = (message?: string) =>
    message ? (
      <p role="alert" className="field-error">
        {message}
      </p>
    ) : null;
  const chosen = condition !== undefined || unlisted;

  return (
    <form
      className="injury-form"
      aria-label={isEdit ? 'Edit injury' : 'Add injury'}
      onSubmit={handleSubmit(submit)}
      noValidate
    >
      <h3>{isEdit ? 'Edit injury' : 'Add injury'}</h3>

      {!chosen && (
        <ConditionSearch
          conditions={catalog.conditions}
          regionName={regionName}
          onSelect={chooseCondition}
          onUnlisted={chooseUnlisted}
          error={errors.conditionId?.message}
        />
      )}

      {condition && (
        <section className="locked" aria-label="Condition details">
          <p>
            <strong>{condition.name}</strong> · {regionName(condition.body_region)} ·{' '}
            {TYPE_LABELS[condition.injury_type]} ·{' '}
            {condition.icd11_code ? `ICD-11 ${condition.icd11_code}` : 'no ICD-11 code'}
          </p>
          <p className="muted">Affects (from the catalog, can't be edited):</p>
          <ul className="pills" aria-label="Affected muscles">
            {condition.muscles.map((m) => (
              <li key={m.id} className={`pill pill-${m.role}`}>
                {m.name} · {m.role}
              </li>
            ))}
          </ul>
          {isEdit ? (
            <p className="muted">To change the condition, delete this injury and add it again.</p>
          ) : (
            <button type="button" className="link" onClick={backToSearch}>
              Choose a different injury
            </button>
          )}
          <p className="notice">General information, not medical advice.</p>
        </section>
      )}

      {unlisted && (
        <>
          <label htmlFor="injury-region">Where is it?</label>
          <select id="injury-region" {...register('bodyRegion')}>
            <option value="">Choose a body region</option>
            {catalog.regions.map((r) => (
              <option key={r.slug} value={r.slug}>
                {r.name}
              </option>
            ))}
          </select>
          {fieldError(errors.bodyRegion?.message)}

          <fieldset>
            <legend>Type</legend>
            {INJURY_TYPES.map((t: InjuryType) => (
              <label key={t} className="choice">
                <input type="radio" value={t} {...register('injuryType')} />
                {TYPE_LABELS[t]}
              </label>
            ))}
          </fieldset>
          {fieldError(errors.injuryType?.message)}

          <Controller
            control={control}
            name="muscleIds"
            render={({ field }) => (
              <MuscleSelect muscles={catalog.muscles} value={field.value} onChange={field.onChange} />
            )}
          />
          {fieldError(errors.muscleIds?.message)}

          {!isEdit && (
            <button type="button" className="link" onClick={backToSearch}>
              Back to search
            </button>
          )}
        </>
      )}

      {chosen && (
        <>
          <fieldset>
            <legend>Severity</legend>
            {SEVERITIES.map((s) => (
              <label key={s} className="choice">
                <input type="radio" value={s} {...register('severity')} />
                {capitalize(s)}
              </label>
            ))}
          </fieldset>
          {fieldError(errors.severity?.message)}

          <label htmlFor="injury-notes">Notes (optional)</label>
          <textarea id="injury-notes" rows={2} {...register('notes')} />
          {fieldError(errors.notes?.message)}
        </>
      )}

      {fieldError(errors.root?.server?.message)}

      <div className="form-actions">
        {chosen && (
          <button type="submit" className="primary" disabled={isSubmitting}>
            {isSubmitting ? 'Saving…' : 'Save'}
          </button>
        )}
        <button type="button" onClick={onCancel}>
          Cancel
        </button>
      </div>
      <p className="notice">
        General information, not medical advice. Consult a professional for diagnosis and treatment.
      </p>
    </form>
  );
}
```

- [ ] **Step 6: Run to verify it passes**

Run: `npx vitest run src/features/injuries/InjuryForm.test.tsx` → all PASS. `npm test`, `npx tsc --noEmit` → clean.

- [ ] **Step 7: Commit**
```bash
git add Client/src/features/injuries/ConditionSearch.tsx Client/src/features/injuries/MuscleSelect.tsx Client/src/features/injuries/InjuryForm.tsx Client/src/features/injuries/InjuryForm.test.tsx
git commit -m "feat(client): injury form with condition search, locked catalog muscles, unlisted path (TDD)"
```

---

### Task 7: Add and edit injuries on the page

**Files:**
- Modify: `Client/src/features/injuries/InjuriesPage.tsx`, `Client/src/features/injuries/InjuriesPage.test.tsx`

**Interfaces:**
- Consumes: `InjuryForm` (Task 6), `toAddRequest` / `toUpdateRequest` (Task 5), `addInjury` / `updateInjury` (Task 2).

- [ ] **Step 1: Write the failing tests** — append to `InjuriesPage.test.tsx` (add `http, HttpResponse`
from `'msw'` and `server` from `'../../mocks/server'` to the imports):
```tsx
describe('InjuriesPage — add and edit', () => {
  beforeEach(() => {
    localStorage.clear();
    saveToken('test-token');
  });

  it('adds a known condition and shows it with catalog muscles', async () => {
    render(<InjuriesPage />);
    fireEvent.click(await screen.findByRole('button', { name: '+ Add injury' }));
    fireEvent.click(screen.getByRole('button', { name: /hamstring strain/i }));
    fireEvent.click(screen.getByRole('radio', { name: 'Severe' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    const card = await screen.findByRole('article', { name: 'Hamstring strain' });
    expect(within(card).getByText(/thigh · severe/i)).toBeInTheDocument();
    expect(screen.queryByRole('form', { name: 'Add injury' })).not.toBeInTheDocument();
  });

  it('adds an unlisted injury with the chosen muscles', async () => {
    render(<InjuriesPage />);
    fireEvent.click(await screen.findByRole('button', { name: '+ Add injury' }));
    fireEvent.click(screen.getByRole('button', { name: /my injury isn't listed/i }));
    fireEvent.change(screen.getByLabelText('Where is it?'), { target: { value: 'knee' } });
    fireEvent.click(screen.getByRole('radio', { name: 'Joint' }));
    fireEvent.click(screen.getByRole('checkbox', { name: 'Quads' }));
    fireEvent.click(screen.getByRole('radio', { name: 'Mild' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    const card = await screen.findByRole('article', { name: 'Other — Knee' });
    expect(within(card).getByText('Quads')).toBeInTheDocument();
  });

  it('edits severity in place', async () => {
    seedInjury({ body_region: 'knee', severity: 'mild' });
    render(<InjuriesPage />);
    fireEvent.click(await screen.findByRole('button', { name: 'Edit' }));
    expect(screen.getByRole('form', { name: 'Edit injury' })).toBeInTheDocument();
    fireEvent.click(screen.getByRole('radio', { name: 'Severe' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    const card = await screen.findByRole('article', { name: 'Other — Knee' });
    expect(within(card).getByText(/knee · severe/i)).toBeInTheDocument();
  });

  it('cancel closes the form without saving', async () => {
    render(<InjuriesPage />);
    fireEvent.click(await screen.findByRole('button', { name: '+ Add injury' }));
    fireEvent.click(screen.getByRole('button', { name: 'Cancel' }));
    expect(screen.queryByRole('form', { name: 'Add injury' })).not.toBeInTheDocument();
  });

  it('keeps the form open and shows the server error under the field', async () => {
    server.use(
      http.post('/api/v1/users/me/injuries', () =>
        HttpResponse.json(
          { detail: 'One or more fields are invalid.', errors: { severity: ['severity rejected by server'] } },
          { status: 422 },
        ),
      ),
    );
    render(<InjuriesPage />);
    fireEvent.click(await screen.findByRole('button', { name: '+ Add injury' }));
    fireEvent.click(screen.getByRole('button', { name: /hamstring strain/i }));
    fireEvent.click(screen.getByRole('radio', { name: 'Mild' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    expect(await screen.findByText('severity rejected by server')).toBeInTheDocument();
    expect(screen.getByRole('form', { name: 'Add injury' })).toBeInTheDocument();
  });
});
```

- [ ] **Step 2: Run to verify it fails**

Run: `npx vitest run src/features/injuries/InjuriesPage.test.tsx`
Expected: FAIL — no button "+ Add injury".

- [ ] **Step 3: Wire add/edit into `InjuriesPage.tsx`**

Add to the imports: `addInjury` (from `'../../api'`), `import InjuryForm from './InjuryForm';` and
`import { toAddRequest, toUpdateRequest, type InjuryFormValues } from './injurySchema';`.
After the `actionError` state add:
```tsx
  // Which inline form is open: none, "add" (top of the list), or editing one card.
  const [editing, setEditing] = useState<'none' | 'add' | number>('none');

  async function saveNew(values: InjuryFormValues) {
    const created = await addInjury(toAddRequest(values));
    setInjuries((list) => [...list, created]);
    setEditing('none');
  }

  async function saveEdit(original: Injury, values: InjuryFormValues) {
    const request = toUpdateRequest(values, original);
    if (Object.keys(request).length > 0) replace(await updateInjury(original.id, request));
    setEditing('none');
  }
```
(`saveNew` / `saveEdit` let errors propagate: `InjuryForm` shows them.)
Replace the `card` helper with:
```tsx
  const card = (i: Injury) =>
    editing === i.id ? (
      <InjuryForm
        key={i.id}
        catalog={catalog}
        initial={i}
        onSave={(values) => saveEdit(i, values)}
        onCancel={() => setEditing('none')}
      />
    ) : (
      <InjuryCard
        key={i.id}
        injury={i}
        regionName={regionName(i.body_region)}
        onEdit={() => setEditing(i.id)}
        onToggleActive={() => toggleActive(i)}
        onDelete={() => remove(i)}
      />
    );
```
Replace the `page-head` div with:
```tsx
      <div className="page-head">
        <h2 id="injuries-title">My injuries</h2>
        {editing !== 'add' && (
          <button type="button" className="primary" onClick={() => setEditing('add')}>
            + Add injury
          </button>
        )}
      </div>
      {editing === 'add' && (
        <InjuryForm catalog={catalog} onSave={saveNew} onCancel={() => setEditing('none')} />
      )}
```

- [ ] **Step 4: Run to verify it passes**

Run: `npx vitest run src/features/injuries` → all PASS. `npm test`, `npx tsc --noEmit` → clean.

- [ ] **Step 5: Commit**
```bash
git add Client/src/features/injuries/InjuriesPage.tsx Client/src/features/injuries/InjuriesPage.test.tsx
git commit -m "feat(client): add and edit injuries inline on the injuries page (TDD)"
```

---

### Task 8: Profile form

**Files:**
- Create: `Client/src/features/profile/profileSchema.ts`, `profileSchema.test.ts`, `ProfileForm.tsx`, `ProfilePage.test.tsx`
- Modify: `Client/src/features/profile/ProfilePage.tsx` (replace stub)

**Interfaces:**
- Consumes: `getProfile`, `updateProfile`, `Profile`, `ProfileUpdate`, `ApiError` (Task 2); `setMockProfile` (Task 2).
- Produces: `profileFormSchema`, `ProfileFormValues`, `ProfileField`, `profileDefaults(p)`, `toProfileUpdate(values, changed, saved): { update: ProfileUpdate; cleared: ProfileField[] }`, `isValidDate(s)`; `ProfileForm({ profile, onSave(update): Promise<Profile> })`.

- [ ] **Step 1: Write the failing tests**

`profileSchema.test.ts`:
```ts
import { describe, expect, it } from 'vitest';
import type { Profile } from '../../api';
import { isValidDate, profileDefaults, profileFormSchema, toProfileUpdate } from './profileSchema';

const saved: Profile = {
  id: 1,
  email: 'a@example.com',
  display_name: 'A',
  sex: null,
  birth_date: '1990-05-01',
  height_cm: 178,
  weight_kg: 74,
  experience_level: 'intermediate',
  unit_preference: 'metric',
  weekly_workout_goal: 3,
  available_time_minutes: null,
  medications: null,
};

const errorPaths = (overrides: Record<string, string>) => {
  const r = profileFormSchema.safeParse({ ...profileDefaults(saved), ...overrides });
  return r.success ? [] : r.error.issues.map((i) => i.path.join('.'));
};

describe('profileFormSchema (mirrors ProfilePolicy)', () => {
  it('accepts the saved profile', () => expect(errorPaths({})).toEqual([]));
  it('checks height and weight ranges', () => {
    expect(errorPaths({ height_cm: '0' })).toEqual(['height_cm']);
    expect(errorPaths({ height_cm: '301' })).toEqual(['height_cm']);
    expect(errorPaths({ weight_kg: '500.5' })).toEqual(['weight_kg']);
    expect(errorPaths({ weight_kg: 'abc' })).toEqual(['weight_kg']);
  });
  it('needs whole numbers for workouts and minutes', () => {
    expect(errorPaths({ weekly_workout_goal: '2.5' })).toEqual(['weekly_workout_goal']);
    expect(errorPaths({ available_time_minutes: '0' })).toEqual(['available_time_minutes']);
  });
  it('allows empty optional fields', () => expect(errorPaths({ available_time_minutes: '' })).toEqual([]));
});

describe('isValidDate', () => {
  it('accepts real dates only', () => {
    expect(isValidDate('2000-02-29')).toBe(true);
    expect(isValidDate('2001-02-29')).toBe(false);
    expect(isValidDate('01/02/2000')).toBe(false);
  });
});

describe('toProfileUpdate', () => {
  it('sends only changed fields, with numbers as numbers', () => {
    const values = { ...profileDefaults(saved), weight_kg: '80', medications: 'none' };
    expect(toProfileUpdate(values, ['weight_kg', 'medications'], saved)).toEqual({
      update: { weight_kg: 80, medications: 'none' },
      cleared: [],
    });
  });
  it('reports emptied saved values instead of sending them', () => {
    const values = { ...profileDefaults(saved), height_cm: '' };
    expect(toProfileUpdate(values, ['height_cm'], saved)).toEqual({ update: {}, cleared: ['height_cm'] });
  });
});
```

`ProfilePage.test.tsx`:
```tsx
import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import { http, HttpResponse } from 'msw';
import { beforeEach, describe, expect, it } from 'vitest';
import ProfilePage from './ProfilePage';
import { server } from '../../mocks/server';
import { saveToken } from '../../lib/tokenStorage';

describe('ProfilePage', () => {
  beforeEach(() => {
    localStorage.clear();
    saveToken('test-token');
  });

  it('pre-fills the form from the saved profile', async () => {
    render(<ProfilePage />);
    expect(await screen.findByLabelText('Height (cm)')).toHaveValue('178');
    expect(screen.getByLabelText('Experience level')).toHaveValue('intermediate');
  });

  it('sends only the changed field and confirms', async () => {
    let sent: unknown = null;
    server.use(
      http.patch('/api/v1/users/me', async ({ request }) => {
        sent = await request.json();
        return HttpResponse.json({
          id: 1, email: 'test@example.com', display_name: 'Test User', sex: null, birth_date: null,
          height_cm: 178, weight_kg: 80, experience_level: 'intermediate', unit_preference: 'metric',
          weekly_workout_goal: 3, available_time_minutes: null, medications: null,
        });
      }),
    );
    render(<ProfilePage />);
    fireEvent.change(await screen.findByLabelText('Weight (kg)'), { target: { value: '80' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save profile' }));

    expect(await screen.findByText('Profile saved.')).toBeInTheDocument();
    expect(sent).toEqual({ weight_kg: 80 });
  });

  it('shows Zod errors and does not send', async () => {
    let calls = 0;
    server.use(http.patch('/api/v1/users/me', () => { calls++; return HttpResponse.json({}); }));
    render(<ProfilePage />);
    fireEvent.change(await screen.findByLabelText('Height (cm)'), { target: { value: '400' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save profile' }));

    expect(await screen.findByText(/height must be more than 0 and at most 300/i)).toBeInTheDocument();
    expect(calls).toBe(0);
  });

  it('blocks clearing a saved value', async () => {
    render(<ProfilePage />);
    fireEvent.change(await screen.findByLabelText('Height (cm)'), { target: { value: '' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save profile' }));
    expect(await screen.findByText(/clearing a saved value isn't supported yet/i)).toBeInTheDocument();
  });

  it('shows a server field error under the field', async () => {
    server.use(
      http.patch('/api/v1/users/me', () =>
        HttpResponse.json(
          { detail: 'One or more fields are invalid.', errors: { birth_date: ['birth_date rejected'] } },
          { status: 422 },
        ),
      ),
    );
    render(<ProfilePage />);
    fireEvent.change(await screen.findByLabelText('Birth date'), { target: { value: '1995-03-04' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save profile' }));
    await waitFor(() => expect(screen.getByText('birth_date rejected')).toBeInTheDocument());
  });
});
```

- [ ] **Step 2: Run to verify it fails**

Run: `npx vitest run src/features/profile`
Expected: FAIL — `Failed to resolve import "./profileSchema"`; no "Height (cm)" field.

- [ ] **Step 3: Write `profileSchema.ts`**
```ts
import { z } from 'zod';
import type { Profile, ProfileUpdate } from '../../api';

// A real calendar date in YYYY-MM-DD (rejects 2001-02-29), like the server.
export function isValidDate(s: string): boolean {
  if (!/^\d{4}-\d{2}-\d{2}$/.test(s)) return false;
  const d = new Date(`${s}T00:00:00Z`);
  return !Number.isNaN(d.getTime()) && d.toISOString().startsWith(s);
}

// Empty means "not set"; otherwise the number must pass `ok`.
const blankOr = (ok: (n: number) => boolean) => (s: string) => s.trim() === '' || ok(Number(s));

// Mirrors the server's ProfilePolicy so users get the same answer instantly.
export const profileFormSchema = z.object({
  height_cm: z.string().refine(blankOr((n) => n > 0 && n <= 300), 'Height must be more than 0 and at most 300 cm'),
  weight_kg: z.string().refine(blankOr((n) => n > 0 && n <= 500), 'Weight must be more than 0 and at most 500 kg'),
  birth_date: z.string().refine((s) => s === '' || isValidDate(s), 'Enter a real date (YYYY-MM-DD)'),
  sex: z.string(),
  experience_level: z.string(),
  weekly_workout_goal: z.string().refine(blankOr((n) => Number.isInteger(n) && n >= 0), 'Use a whole number, 0 or more'),
  available_time_minutes: z.string().refine(blankOr((n) => Number.isInteger(n) && n > 0), 'Use a whole number above 0'),
  medications: z.string(),
  unit_preference: z.enum(['metric', 'imperial']),
});

export type ProfileFormValues = z.infer<typeof profileFormSchema>;
export type ProfileField = keyof ProfileFormValues;

const NUMBER_FIELDS: ProfileField[] = ['height_cm', 'weight_kg', 'weekly_workout_goal', 'available_time_minutes'];

export function profileDefaults(p: Profile): ProfileFormValues {
  const s = (v: string | number | null) => (v === null ? '' : String(v));
  return {
    height_cm: s(p.height_cm),
    weight_kg: s(p.weight_kg),
    birth_date: s(p.birth_date),
    sex: s(p.sex),
    experience_level: s(p.experience_level),
    weekly_workout_goal: s(p.weekly_workout_goal),
    available_time_minutes: s(p.available_time_minutes),
    medications: s(p.medications),
    unit_preference: p.unit_preference,
  };
}

// Turns the changed fields into a PATCH body. The server can't clear a saved
// value yet (absent = unchanged), so emptied saved values are reported back
// in `cleared` instead of being silently dropped.
export function toProfileUpdate(
  values: ProfileFormValues,
  changed: ProfileField[],
  saved: Profile,
): { update: ProfileUpdate; cleared: ProfileField[] } {
  const update: Record<string, string | number> = {};
  const cleared: ProfileField[] = [];
  for (const field of changed) {
    const raw = values[field].trim();
    if (raw === '') {
      if (saved[field] !== null) cleared.push(field);
      continue;
    }
    update[field] = NUMBER_FIELDS.includes(field) ? Number(raw) : raw;
  }
  return { update: update as ProfileUpdate, cleared };
}
```

- [ ] **Step 4: Write `ProfileForm.tsx`**
```tsx
import { zodResolver } from '@hookform/resolvers/zod';
import { useState } from 'react';
import { useForm } from 'react-hook-form';
import { ApiError, type Profile, type ProfileUpdate } from '../../api';
import {
  profileDefaults,
  profileFormSchema,
  toProfileUpdate,
  type ProfileField,
  type ProfileFormValues,
} from './profileSchema';

interface ProfileFormProps {
  profile: Profile;
  onSave: (update: ProfileUpdate) => Promise<Profile>;
}

export default function ProfileForm({ profile, onSave }: ProfileFormProps) {
  const [saved, setSaved] = useState(profile);
  const [status, setStatus] = useState<string | null>(null);
  const {
    register,
    handleSubmit,
    reset,
    setError,
    formState: { errors, dirtyFields, isSubmitting },
  } = useForm<ProfileFormValues>({
    resolver: zodResolver(profileFormSchema),
    defaultValues: profileDefaults(profile),
  });

  async function submit(values: ProfileFormValues) {
    setStatus(null);
    const changed = (Object.keys(dirtyFields) as ProfileField[]).filter((f) => dirtyFields[f]);
    const { update, cleared } = toProfileUpdate(values, changed, saved);
    if (cleared.length > 0) {
      for (const field of cleared) {
        setError(field, { message: "Clearing a saved value isn't supported yet." });
      }
      return;
    }
    if (Object.keys(update).length === 0) {
      setStatus('Nothing to save.');
      return;
    }
    try {
      const updated = await onSave(update);
      setSaved(updated);
      reset(profileDefaults(updated)); // new baseline: nothing is "changed" any more
      setStatus('Profile saved.');
    } catch (err) {
      if (err instanceof ApiError && Object.keys(err.fieldErrors).length > 0) {
        for (const [field, messages] of Object.entries(err.fieldErrors)) {
          setError(field as ProfileField, { message: messages[0] });
        }
      } else {
        setStatus(err instanceof Error ? err.message : String(err));
      }
    }
  }

  const err = (field: ProfileField) =>
    errors[field]?.message ? (
      <p role="alert" className="field-error">
        {errors[field]?.message}
      </p>
    ) : null;

  return (
    <form className="profile-form" aria-label="Profile" onSubmit={handleSubmit(submit)} noValidate>
      <fieldset>
        <legend>Body</legend>
        <label htmlFor="height_cm">Height (cm)</label>
        <input id="height_cm" inputMode="decimal" {...register('height_cm')} />
        {err('height_cm')}
        <label htmlFor="weight_kg">Weight (kg)</label>
        <input id="weight_kg" inputMode="decimal" {...register('weight_kg')} />
        {err('weight_kg')}
        <label htmlFor="birth_date">Birth date</label>
        <input id="birth_date" type="date" {...register('birth_date')} />
        {err('birth_date')}
        <label htmlFor="sex">Sex</label>
        <select id="sex" {...register('sex')}>
          <option value="">Not set</option>
          <option value="female">Female</option>
          <option value="male">Male</option>
          <option value="other">Other</option>
          <option value="unspecified">Prefer not to say</option>
        </select>
        {err('sex')}
      </fieldset>

      <fieldset>
        <legend>Training</legend>
        <label htmlFor="experience_level">Experience level</label>
        <select id="experience_level" {...register('experience_level')}>
          <option value="">Not set</option>
          <option value="beginner">Beginner</option>
          <option value="intermediate">Intermediate</option>
          <option value="advanced">Advanced</option>
        </select>
        {err('experience_level')}
        <label htmlFor="weekly_workout_goal">Workouts per week</label>
        <input id="weekly_workout_goal" inputMode="numeric" {...register('weekly_workout_goal')} />
        {err('weekly_workout_goal')}
        <label htmlFor="available_time_minutes">Minutes per session</label>
        <input id="available_time_minutes" inputMode="numeric" {...register('available_time_minutes')} />
        {err('available_time_minutes')}
      </fieldset>

      <fieldset>
        <legend>Other</legend>
        <label htmlFor="medications">Medications</label>
        <textarea id="medications" rows={2} {...register('medications')} />
        {err('medications')}
        <label htmlFor="unit_preference">Units</label>
        <select id="unit_preference" {...register('unit_preference')}>
          <option value="metric">Metric</option>
          <option value="imperial">Imperial (display coming later)</option>
        </select>
        {err('unit_preference')}
      </fieldset>

      {status && (
        <p role="status" className="muted">
          {status}
        </p>
      )}
      <div className="form-actions">
        <button type="submit" className="primary" disabled={isSubmitting}>
          {isSubmitting ? 'Saving…' : 'Save profile'}
        </button>
      </div>
    </form>
  );
}
```

- [ ] **Step 5: Replace `ProfilePage.tsx`**
```tsx
import { useEffect, useState } from 'react';
import { getProfile, updateProfile, type Profile } from '../../api';
import ProfileForm from './ProfileForm';

export default function ProfilePage() {
  const [profile, setProfile] = useState<Profile | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    getProfile()
      .then(setProfile)
      .catch((e: unknown) => setError(e instanceof Error ? e.message : String(e)));
  }, []);

  return (
    <section className="page" aria-labelledby="profile-title">
      <h2 id="profile-title">Profile</h2>
      {error && <p role="alert">Couldn't load your profile: {error}</p>}
      {!error && !profile && <p>Loading…</p>}
      {profile && <ProfileForm profile={profile} onSave={updateProfile} />}
    </section>
  );
}
```

- [ ] **Step 6: Run to verify it passes**

Run: `npx vitest run src/features/profile` → all PASS. `npm test`, `npx tsc --noEmit`, `npm run build` → clean.

- [ ] **Step 7: Commit**
```bash
git add Client/src/features/profile
git commit -m "feat(client): profile form mirroring ProfilePolicy, sends only changed fields (TDD)"
```

---

### Task 9: Browser verification, docs, backlog, finish the slice

**Files:**
- Create: `docs/backlog.md`
- Modify: `docs/CONTEXT.md`, `WORKLOG.md`, `docs/phase-2-slice-2-profile-injuries.md`, `CLAUDE.md` (Client architecture)

- [ ] **Step 1: Run the real app** — Postgres up (`docker start kinetiq-postgres`), then
`./scripts/run-dev.ps1 -Target server` and `./scripts/run-dev.ps1 -Target client` (background).

- [ ] **Step 2: Verify in a real browser (Playwright MCP tools)** against `http://localhost:5173`:
1. Register `stagec-verify@example.com` / `StrongPass1` → lands on **Injuries** tab (highlighted), top bar shows "Kinetiq".
2. **+ Add injury** → type "acl" → pick **ACL tear** → locked box shows Quads / Hamstrings / Calves (secondary), `ICD-11 NC93.62`, notice → Severe → Save → card appears.
3. **+ Add injury** → "My injury isn't listed…" → Wrist & hand, Joint, tick Forearm Flexors, Mild → Save → card "Other — Wrist & hand" with the pill.
4. Edit the ACL card → severity Moderate → Save → card updated. Mark the wrist injury healed → "Healed (1)". Delete it (confirm).
5. **Profile** tab → URL `/profile` → set height 180, weight 75, experience Intermediate → Save → "Profile saved." → refresh page → still on `/profile`, values kept.
6. Browser back button → returns to `/injuries`.
7. **Themes:** toggle ☾/☀ on the login screen and in the app; reload → the choice is kept; both
   themes readable (pills: primary filled, secondary outlined).
8. **Units:** switch Metric → Imperial → Profile shows `in` / `lb` and the "= 5 ft 10 in" hint;
   reload → still imperial (saved to the account).
9. **Clearing:** empty the weight, save → note names the last saved weight; DB row in
   `user_profile_history` with the old value.
10. Take a screenshot of each tab in both themes; check the console has no errors.
Then delete the test user:
`docker exec kinetiq-postgres psql -U kinetiq -d kinetiq -c "delete from users where email='stagec-verify@example.com'"`. Stop the server + client.

- [ ] **Step 3: Write `docs/backlog.md`**
```markdown
# Backlog — future ideas (later, not never)

One list of things deliberately deferred, so nothing is lost. Pull from here when planning a slice.

## Accounts & auth
- Google OAuth sign-in
- "Forgot password" reset (needs email infrastructure)
- Account deletion (re-auth + cascade-delete design)
- Login rate-limiting; email verification
- Edit display name / email
- Refresh tokens / httpOnly-cookie session hardening

## Profile
- Imperial display (store metric, convert in the UI)
- Clearing a saved profile value (server "absent = unchanged" today)
- `preferences` (jsonb) — deferred from Slice 2

## Injuries & catalog
- Physio review of the conditions catalog (secondary muscles are project judgement)
- Re-check ICD-11 codes in the WHO browser; find codes for patellar tendinopathy, shin splints, Achilles tendinopathy
- Several conditions on one injury; left/right side
- Users suggesting new catalog conditions; Hebrew names
- Muscle search box (if the muscle list grows)

## Frontend
- TanStack Query (caching / refetching) when screens multiply
- Offline support
- Upgrade the dev toolchain: Vite 5 → 8, Vitest 2 → 5 (clears the 6 remaining dev-only `npm audit`
  advisories, incl. esbuild dev-server exposure — until then, don't browse untrusted sites while
  `npm run dev` runs)

## Ops
- Delete the old Docker volumes kept as a backup (`fitapp_kinetiq_pgdata`, `fitapp_fitapp_pgdata`,
  `kinetiq_kinetiq_pgdata`) once confident in `kinetiq_pgdata`
- ADRs for Phase 2 decisions (authenticator-not-middleware, load-modify-save, …)

## Product & legal (owner, 2026-10-25)
- **Forgot password** (email infrastructure: provider, reset tokens with expiry, two screens)
- **Subscription idea:** $5.99/month, refunded after 10 verified workouts — needs payments
  (e.g. Stripe), refunds, and **workout verification** (gym progress tracking); depends on Slice 4
- **User agreement / privacy policy:** disclose that cleared profile values are kept in history and
  may still be used for training plans (ADR-0017)
- **"Delete my data"** — real erasure (account + profile history), ties into account deletion

## Owner's ideas
- (add yours here)
```

- [ ] **Step 4: Update docs**
- `docs/phase-2-slice-2-profile-injuries.md`: add `### Stage C — Frontend (#25) (2026-10-22)` under the build log (what was built per task, decisions S1–S8, browser verification results) and update "What's next" to Slice 3.
- `docs/CONTEXT.md`: Slice 2 = **COMPLETE**; Stage C bullet ✅ with test counts; next = **Slice 3 — Exercises + seed (#21, #22, #26)**; docs map gains the Stage C spec/plan and `backlog.md`.
- `CLAUDE.md` "Client architecture": add `react-router` routes (`/injuries`, `/profile`) inside `AppShell`; all authed calls via `authFetch` (401 → logout); forms = React Hook Form + Zod with pure `*Schema.ts` modules (rules + request converters); MSW handlers are stateful (`resetMockDb`).
- `WORKLOG.md`: dated entry at the top.

- [ ] **Step 5: Commit**
```bash
git add docs/backlog.md docs/CONTEXT.md WORKLOG.md docs/phase-2-slice-2-profile-injuries.md CLAUDE.md
git commit -m "docs: Stage C complete -- build log, context, backlog, client architecture"
```

- [ ] **Step 6: Finish the slice** — use superpowers:finishing-a-development-branch: full checks
pass, then **ask the owner** before merging `slice-2-profile` → `main` (merge + push, or PR).
