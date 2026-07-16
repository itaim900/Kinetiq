import { fireEvent, render, screen } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';
import AuthPage from './AuthPage';
import type { AuthResult } from '../../api';

describe('AuthPage', () => {
  it('shows the login form by default', () => {
    render(<AuthPage onAuthenticated={vi.fn()} />);
    expect(screen.getByRole('button', { name: /log in/i })).toBeInTheDocument();
    expect(screen.queryByRole('button', { name: /create account/i })).not.toBeInTheDocument();
  });

  it('switches to the register form and back', () => {
    render(<AuthPage onAuthenticated={vi.fn()} />);

    fireEvent.click(screen.getByRole('button', { name: /create one/i }));
    expect(screen.getByRole('button', { name: /create account/i })).toBeInTheDocument();
    expect(screen.queryByRole('button', { name: /^log in$/i })).not.toBeInTheDocument();

    fireEvent.click(screen.getByRole('button', { name: /log in instead/i }));
    expect(screen.getByRole('button', { name: /^log in$/i })).toBeInTheDocument();
  });

  // Locked decision (2026-07-16): no backend exists for this yet -- clicking
  // shows a placeholder message instead of attempting a real request.
  it('shows a "coming soon" message for forgot password instead of doing anything', () => {
    render(<AuthPage onAuthenticated={vi.fn()} />);

    fireEvent.click(screen.getByRole('button', { name: /forgot password/i }));

    expect(screen.getByText(/coming soon/i)).toBeInTheDocument();
  });

  it('calls onAuthenticated when the login form succeeds', async () => {
    const onAuthenticated = vi.fn<(result: AuthResult) => void>();
    render(<AuthPage onAuthenticated={onAuthenticated} />);

    fireEvent.change(screen.getByLabelText(/email/i), {
      target: { value: 'hank@example.com' },
    });
    fireEvent.change(screen.getByLabelText(/password/i), { target: { value: 'StrongPass1' } });
    fireEvent.click(screen.getByRole('button', { name: /log in/i }));

    await screen.findByRole('button', { name: /log in/i });
    expect(onAuthenticated).toHaveBeenCalled();
  });
});
