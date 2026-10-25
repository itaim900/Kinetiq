import { fireEvent, render, screen } from '@testing-library/react';
import { http, HttpResponse } from 'msw';
import { beforeEach, describe, expect, it } from 'vitest';
import UnitsToggle from './UnitsToggle';
import { UnitsProvider } from './lib/UnitsContext';
import { setMockProfile } from './mocks/handlers';
import { server } from './mocks/server';
import { saveToken } from './lib/tokenStorage';

const renderToggle = () =>
  render(
    <UnitsProvider>
      <UnitsToggle />
    </UnitsProvider>,
  );

describe('UnitsToggle', () => {
  beforeEach(() => {
    localStorage.clear();
    saveToken('test-token');
  });

  it("starts from the account's saved units", async () => {
    setMockProfile({ unit_preference: 'imperial' });
    renderToggle();
    expect(await screen.findByRole('button', { name: 'Imperial', pressed: true })).toBeInTheDocument();
  });

  it('switches and saves to the account', async () => {
    let sent: unknown = null;
    server.use(
      http.patch('/api/v1/users/me', async ({ request }) => {
        sent = await request.json();
        return HttpResponse.json({});
      }),
    );
    renderToggle();
    await screen.findByRole('button', { name: 'Metric', pressed: true });
    fireEvent.click(screen.getByRole('button', { name: 'Imperial' }));
    expect(await screen.findByRole('button', { name: 'Imperial', pressed: true })).toBeInTheDocument();
    expect(sent).toEqual({ unit_preference: 'imperial' });
  });

  it('switches back and explains when saving fails', async () => {
    server.use(http.patch('/api/v1/users/me', () => HttpResponse.json({ detail: 'down' }, { status: 500 })));
    renderToggle();
    await screen.findByRole('button', { name: 'Metric', pressed: true });
    fireEvent.click(screen.getByRole('button', { name: 'Imperial' }));
    expect(await screen.findByText(/couldn't save your units/i)).toBeInTheDocument();
    expect(screen.getByRole('button', { name: 'Metric', pressed: true })).toBeInTheDocument();
  });
});
