import { http, HttpResponse } from 'msw';
import { describe, expect, it } from 'vitest';
import { loginUser, registerUser } from './api';
import { server } from './mocks/server';

describe('registerUser', () => {
  it('returns the user and access token on success', async () => {
    const result = await registerUser({
      email: 'alice@example.com',
      password: 'StrongPass1',
      display_name: 'Alice',
    });

    expect(result.user.email).toBe('alice@example.com');
    expect(result.user.display_name).toBe('Alice');
    expect(result.access_token).toBeTruthy();
  });

  it('throws the backend detail message on a weak password (422)', async () => {
    server.use(
      http.post('/api/v1/auth/register', () =>
        HttpResponse.json(
          {
            type: 'validation_error',
            title: 'Validation Failed',
            status: 422,
            detail: 'Password does not meet the strength policy.',
          },
          { status: 422 },
        ),
      ),
    );

    await expect(
      registerUser({ email: 'bob@example.com', password: 'weak', display_name: 'Bob' }),
    ).rejects.toThrow('Password does not meet the strength policy.');
  });

  it('throws the backend detail message on a duplicate email (409)', async () => {
    server.use(
      http.post('/api/v1/auth/register', () =>
        HttpResponse.json(
          {
            type: 'conflict',
            title: 'Conflict',
            status: 409,
            detail: 'Email is already registered.',
          },
          { status: 409 },
        ),
      ),
    );

    await expect(
      registerUser({ email: 'carol@example.com', password: 'StrongPass1', display_name: 'Carol' }),
    ).rejects.toThrow('Email is already registered.');
  });
});

describe('loginUser', () => {
  it('returns the user and access token on success', async () => {
    const result = await loginUser({ email: 'alice@example.com', password: 'StrongPass1' });

    expect(result.user.email).toBe('alice@example.com');
    expect(result.access_token).toBeTruthy();
  });

  it('throws the backend detail message on invalid credentials (401)', async () => {
    server.use(
      http.post('/api/v1/auth/login', () =>
        HttpResponse.json(
          {
            type: 'invalid_credentials',
            title: 'Unauthorized',
            status: 401,
            detail: 'Invalid email or password.',
          },
          { status: 401 },
        ),
      ),
    );

    await expect(
      loginUser({ email: 'nobody@example.com', password: 'Whatever1' }),
    ).rejects.toThrow('Invalid email or password.');
  });
});
