import { fireEvent, render, screen, waitFor } from '@testing-library/react';
import { http, HttpResponse } from 'msw';
import { beforeEach, describe, expect, it } from 'vitest';
import ProfilePage from './ProfilePage';
import { UnitsContext, type UnitsValue } from '../../lib/UnitsContext';
import { server } from '../../mocks/server';
import { saveToken } from '../../lib/tokenStorage';

const metric: UnitsValue = { units: 'metric', setUnits: async () => {}, error: null };
const imperial: UnitsValue = { units: 'imperial', setUnits: async () => {}, error: null };

const renderPage = (units: UnitsValue = metric) =>
  render(
    <UnitsContext.Provider value={units}>
      <ProfilePage />
    </UnitsContext.Provider>,
  );

const profileJson = (overrides: Record<string, unknown> = {}) => ({
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
  ...overrides,
});

describe('ProfilePage', () => {
  beforeEach(() => {
    localStorage.clear();
    saveToken('test-token');
  });

  it('pre-fills the form from the saved profile', async () => {
    renderPage();
    expect(await screen.findByLabelText('Height (cm)')).toHaveValue('178');
    expect(screen.getByLabelText('Experience level')).toHaveValue('intermediate');
  });

  it('sends only the changed field and confirms', async () => {
    let sent: unknown = null;
    server.use(
      http.patch('/api/v1/users/me', async ({ request }) => {
        sent = await request.json();
        return HttpResponse.json(profileJson({ weight_kg: 80 }));
      }),
    );
    renderPage();
    fireEvent.change(await screen.findByLabelText('Weight (kg)'), { target: { value: '80' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save profile' }));

    expect(await screen.findByText('Profile saved.')).toBeInTheDocument();
    expect(sent).toEqual({ weight_kg: 80 });
  });

  it('shows Zod errors and does not send', async () => {
    let calls = 0;
    server.use(
      http.patch('/api/v1/users/me', () => {
        calls++;
        return HttpResponse.json({});
      }),
    );
    renderPage();
    fireEvent.change(await screen.findByLabelText('Height (cm)'), { target: { value: '400' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save profile' }));

    expect(await screen.findByText(/height must be more than 0 and at most 300/i)).toBeInTheDocument();
    expect(calls).toBe(0);
  });

  it('clears a saved value and explains the last saved value is kept', async () => {
    let sent: unknown = null;
    server.use(
      http.patch('/api/v1/users/me', async ({ request }) => {
        sent = await request.json();
        return HttpResponse.json(profileJson({ height_cm: null }));
      }),
    );
    renderPage();
    fireEvent.change(await screen.findByLabelText('Height (cm)'), { target: { value: '' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save profile' }));
    expect(
      await screen.findByText(/height cleared\. kinetiq will keep using your last saved height \(178 cm\)/i),
    ).toBeInTheDocument();
    expect(sent).toEqual({ height_cm: null });
    expect(screen.getByLabelText('Height (cm)')).toHaveValue('');
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
    renderPage();
    fireEvent.change(await screen.findByLabelText('Birth date'), { target: { value: '1995-03-04' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save profile' }));
    await waitFor(() => expect(screen.getByText('birth_date rejected')).toBeInTheDocument());
  });

  it('shows and saves height/weight in imperial', async () => {
    let sent: unknown = null;
    server.use(
      http.patch('/api/v1/users/me', async ({ request }) => {
        sent = await request.json();
        return HttpResponse.json(profileJson({ weight_kg: 81.6, unit_preference: 'imperial' }));
      }),
    );
    renderPage(imperial);
    expect(await screen.findByLabelText('Height (in)')).toHaveValue('70.1');
    expect(screen.getByText('= 5 ft 10 in')).toBeInTheDocument();
    fireEvent.change(screen.getByLabelText('Weight (lb)'), { target: { value: '180' } });
    fireEvent.click(screen.getByRole('button', { name: 'Save profile' }));
    expect(await screen.findByText('Profile saved.')).toBeInTheDocument();
    expect(sent).toEqual({ weight_kg: 81.6 });
  });

  it('has no units dropdown (the top-bar switch replaces it)', async () => {
    renderPage();
    await screen.findByLabelText('Height (cm)');
    expect(screen.queryByLabelText('Units')).not.toBeInTheDocument();
  });
});
