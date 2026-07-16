import { fireEvent, render, screen } from '@testing-library/react';
import { http, HttpResponse } from 'msw';
import { describe, expect, it, vi } from 'vitest';
import { server } from '../../mocks/server';
import LoginForm from './LoginForm';
import type { AuthResult } from '../../api';

function fillAndSubmit(email: string, password: string) {
  fireEvent.change(screen.getByLabelText(/email/i), { target: { value: email } });
  fireEvent.change(screen.getByLabelText(/password/i), { target: { value: password } });
  fireEvent.click(screen.getByRole('button', { name: /log in/i }));
}

describe('LoginForm', () => {
  it('calls onSuccess with the logged-in user and token', async () => {
    const onSuccess = vi.fn<(result: AuthResult) => void>();
    render(<LoginForm onSuccess={onSuccess} />);

    fillAndSubmit('gina@example.com', 'StrongPass1');

    await screen.findByRole('button', { name: /log in/i });
    expect(onSuccess).toHaveBeenCalledWith(
      expect.objectContaining({
        user: expect.objectContaining({ email: 'gina@example.com' }),
        access_token: expect.any(String),
      }),
    );
  });

  // Wrong password and unknown email return the identical message from the
  // backend (no email enumeration) -- one test covers both, since the form
  // can't tell them apart and shouldn't try to.
  it('shows the generic backend error message on invalid credentials', async () => {
    server.use(
      http.post('/api/v1/auth/login', () =>
        HttpResponse.json({ detail: 'Invalid email or password.' }, { status: 401 }),
      ),
    );
    render(<LoginForm onSuccess={vi.fn()} />);

    fillAndSubmit('nobody@example.com', 'WrongPass1');

    expect(await screen.findByText('Invalid email or password.')).toBeInTheDocument();
  });
});
