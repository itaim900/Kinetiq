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
    server.use(
      http.patch('/api/v1/users/me', () => {
        calls++;
        return HttpResponse.json({});
      }),
    );
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
