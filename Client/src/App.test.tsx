import { fireEvent, render, screen } from '@testing-library/react';
import { http, HttpResponse } from 'msw';
import { beforeEach, describe, expect, it } from 'vitest';
import App from './App';
import { server } from './mocks/server';
import { getToken, saveToken } from './lib/tokenStorage';

describe('App', () => {
  beforeEach(() => {
    localStorage.clear();
  });

  it('renders backend health returned by the API', async () => {
    render(<App />);

    // The MSW handler returns { status: "ok", db: "connected" }.
    expect(await screen.findByText('connected')).toBeInTheDocument();
    expect(screen.getByText('ok')).toBeInTheDocument();
  });

  it('shows the login form when no session token is stored', async () => {
    render(<App />);
    expect(await screen.findByRole('button', { name: /^log in$/i })).toBeInTheDocument();
  });

  it('restores the session and shows the logged-in view when a valid token is stored', async () => {
    saveToken('valid-token');
    server.use(
      http.get('/api/v1/auth/me', () =>
        HttpResponse.json({ id: 1, email: 'ivy@example.com', display_name: 'Ivy' }),
      ),
    );

    render(<App />);

    expect(await screen.findByText(/ivy@example.com/i)).toBeInTheDocument();
    expect(screen.getByText('Ivy')).toBeInTheDocument();
  });

  it('clears an invalid/expired token and falls back to the login form', async () => {
    saveToken('expired-token');
    server.use(
      http.get('/api/v1/auth/me', () =>
        HttpResponse.json({ detail: 'Missing, invalid, or expired token.' }, { status: 401 }),
      ),
    );

    render(<App />);

    expect(await screen.findByRole('button', { name: /^log in$/i })).toBeInTheDocument();
    expect(getToken()).toBeNull();
  });

  it('shows the logged-in view after a successful login, and logs out back to the login form', async () => {
    render(<App />);

    fireEvent.change(await screen.findByLabelText(/email/i), {
      target: { value: 'jill@example.com' },
    });
    fireEvent.change(screen.getByLabelText(/password/i), { target: { value: 'StrongPass1' } });
    fireEvent.click(screen.getByRole('button', { name: /^log in$/i }));

    expect(await screen.findByText(/jill@example.com/i)).toBeInTheDocument();
    expect(getToken()).toBe('fake-jwt-token');

    fireEvent.click(screen.getByRole('button', { name: /log out/i }));

    expect(await screen.findByRole('button', { name: /^log in$/i })).toBeInTheDocument();
    expect(getToken()).toBeNull();
  });
});
