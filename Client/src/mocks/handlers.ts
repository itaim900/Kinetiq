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
