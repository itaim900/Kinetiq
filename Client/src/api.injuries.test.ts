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
