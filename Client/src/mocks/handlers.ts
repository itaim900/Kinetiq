import { http, HttpResponse } from 'msw';

// Default (happy-path) handlers. Individual tests override these with
// server.use(...) to exercise error cases -- see api.auth.test.ts.
export const handlers = [
  http.get('/api/v1/health', () =>
    HttpResponse.json({ status: 'ok', db: 'connected' }),
  ),

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
];
