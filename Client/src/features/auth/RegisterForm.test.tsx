import { fireEvent, render, screen } from '@testing-library/react';
import { http, HttpResponse } from 'msw';
import { describe, expect, it, vi } from 'vitest';
import { server } from '../../mocks/server';
import RegisterForm from './RegisterForm';
import type { AuthResult } from '../../api';

function fillAndSubmit(email: string, password: string, displayName: string) {
  fireEvent.change(screen.getByLabelText(/email/i), { target: { value: email } });
  fireEvent.change(screen.getByLabelText(/^password/i), { target: { value: password } });
  fireEvent.change(screen.getByLabelText(/display name/i), { target: { value: displayName } });
  fireEvent.click(screen.getByRole('button', { name: /create account/i }));
}

describe('RegisterForm', () => {
  it('calls onSuccess with the registered user and token', async () => {
    const onSuccess = vi.fn<(result: AuthResult) => void>();
    render(<RegisterForm onSuccess={onSuccess} />);

    fillAndSubmit('dana@example.com', 'StrongPass1', 'Dana');

    await screen.findByRole('button', { name: /create account/i }); // wait a tick
    expect(onSuccess).toHaveBeenCalledWith(
      expect.objectContaining({
        user: expect.objectContaining({ email: 'dana@example.com', display_name: 'Dana' }),
        access_token: expect.any(String),
      }),
    );
  });

  it('shows the backend error message on a weak password', async () => {
    server.use(
      http.post('/api/v1/auth/register', () =>
        HttpResponse.json(
          { detail: 'Password does not meet the strength policy.' },
          { status: 422 },
        ),
      ),
    );
    render(<RegisterForm onSuccess={vi.fn()} />);

    fillAndSubmit('eve@example.com', 'weak', 'Eve');

    expect(
      await screen.findByText('Password does not meet the strength policy.'),
    ).toBeInTheDocument();
  });

  it('shows the backend error message on a duplicate email', async () => {
    server.use(
      http.post('/api/v1/auth/register', () =>
        HttpResponse.json({ detail: 'Email is already registered.' }, { status: 409 }),
      ),
    );
    render(<RegisterForm onSuccess={vi.fn()} />);

    fillAndSubmit('frank@example.com', 'StrongPass1', 'Frank');

    expect(await screen.findByText('Email is already registered.')).toBeInTheDocument();
  });
});
