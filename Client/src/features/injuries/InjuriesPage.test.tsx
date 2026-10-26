import { fireEvent, render, screen, within } from '@testing-library/react';
import { http, HttpResponse } from 'msw';
import { beforeEach, describe, expect, it } from 'vitest';
import InjuriesPage from './InjuriesPage';
import { mockConditions, seedInjury } from '../../mocks/handlers';
import { server } from '../../mocks/server';
import { saveToken } from '../../lib/tokenStorage';

describe('InjuriesPage — list', () => {
  beforeEach(() => {
    localStorage.clear();
    saveToken('test-token');
  });

  it('shows an empty state', async () => {
    render(<InjuriesPage />);
    expect(await screen.findByText(/no injuries yet/i)).toBeInTheDocument();
  });

  it('shows a known injury with its catalog muscles and roles', async () => {
    const c = mockConditions[0];
    seedInjury({
      condition: { id: c.id, name: c.name, icd11_code: c.icd11_code },
      body_region: 'thigh',
      injury_type: 'muscle',
      severity: 'moderate',
      muscles: c.muscles,
    });
    render(<InjuriesPage />);

    const card = await screen.findByRole('article', { name: 'Hamstring strain' });
    expect(within(card).getByText(/thigh · moderate/i)).toBeInTheDocument();
    const pills = within(card).getByRole('list', { name: /affected muscles/i });
    expect(within(pills).getByText('Hamstrings')).toHaveClass('pill-primary');
    expect(within(pills).getByText('Glutes')).toHaveClass('pill-secondary');
  });

  it('names an unlisted injury by its region', async () => {
    seedInjury({ body_region: 'lower_back' });
    render(<InjuriesPage />);
    expect(await screen.findByRole('article', { name: 'Other — Lower back' })).toBeInTheDocument();
  });

  it('moves a healed injury into the Healed section and back', async () => {
    seedInjury({ body_region: 'knee' });
    render(<InjuriesPage />);

    fireEvent.click(await screen.findByRole('button', { name: 'Mark healed' }));
    expect(await screen.findByText('Healed (1)')).toBeInTheDocument();
    expect(screen.getByText(/no injuries yet/i)).toBeInTheDocument();

    fireEvent.click(screen.getByRole('button', { name: 'Mark active' }));
    expect(await screen.findByRole('button', { name: 'Mark healed' })).toBeInTheDocument();
    expect(screen.queryByText(/healed \(/i)).not.toBeInTheDocument();
  });

  it('asks for confirmation before deleting', async () => {
    seedInjury({ body_region: 'knee' });
    render(<InjuriesPage />);

    fireEvent.click(await screen.findByRole('button', { name: 'Delete' }));
    expect(screen.getByText('Delete this injury?')).toBeInTheDocument();
    fireEvent.click(screen.getByRole('button', { name: 'Cancel' }));
    expect(screen.getByRole('article', { name: 'Other — Knee' })).toBeInTheDocument();

    fireEvent.click(screen.getByRole('button', { name: 'Delete' }));
    fireEvent.click(screen.getByRole('button', { name: 'Yes, delete' }));
    expect(await screen.findByText(/no injuries yet/i)).toBeInTheDocument();
  });
});

describe('InjuriesPage — add and edit', () => {
  beforeEach(() => {
    localStorage.clear();
    saveToken('test-token');
  });

  it('adds a known condition and shows it with catalog muscles', async () => {
    render(<InjuriesPage />);
    fireEvent.click(await screen.findByRole('button', { name: '+ Add injury' }));
    fireEvent.click(screen.getByRole('button', { name: /hamstring strain/i }));
    fireEvent.click(screen.getByRole('radio', { name: 'Severe' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    const card = await screen.findByRole('article', { name: 'Hamstring strain' });
    expect(within(card).getByText(/thigh · severe/i)).toBeInTheDocument();
    expect(screen.queryByRole('form', { name: 'Add injury' })).not.toBeInTheDocument();
  });

  it('adds an unlisted injury with the chosen muscles', async () => {
    render(<InjuriesPage />);
    fireEvent.click(await screen.findByRole('button', { name: '+ Add injury' }));
    fireEvent.click(screen.getByRole('button', { name: /my injury isn't listed/i }));
    fireEvent.change(screen.getByLabelText('Where is it?'), { target: { value: 'knee' } });
    fireEvent.click(screen.getByRole('radio', { name: 'Joint' }));
    fireEvent.click(screen.getByRole('checkbox', { name: 'Quads' }));
    fireEvent.click(screen.getByRole('radio', { name: 'Mild' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    const card = await screen.findByRole('article', { name: 'Other — Knee' });
    expect(within(card).getByText('Quads')).toBeInTheDocument();
  });

  it('edits severity in place', async () => {
    seedInjury({ body_region: 'knee', severity: 'mild' });
    render(<InjuriesPage />);
    fireEvent.click(await screen.findByRole('button', { name: 'Edit' }));
    expect(screen.getByRole('form', { name: 'Edit injury' })).toBeInTheDocument();
    fireEvent.click(screen.getByRole('radio', { name: 'Severe' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    const card = await screen.findByRole('article', { name: 'Other — Knee' });
    expect(within(card).getByText(/knee · severe/i)).toBeInTheDocument();
  });

  it('cancel closes the form without saving', async () => {
    render(<InjuriesPage />);
    fireEvent.click(await screen.findByRole('button', { name: '+ Add injury' }));
    fireEvent.click(screen.getByRole('button', { name: 'Cancel' }));
    expect(screen.queryByRole('form', { name: 'Add injury' })).not.toBeInTheDocument();
  });

  it('keeps the form open and shows the server error under the field', async () => {
    server.use(
      http.post('/api/v1/users/me/injuries', () =>
        HttpResponse.json(
          { detail: 'One or more fields are invalid.', errors: { severity: ['severity rejected by server'] } },
          { status: 422 },
        ),
      ),
    );
    render(<InjuriesPage />);
    fireEvent.click(await screen.findByRole('button', { name: '+ Add injury' }));
    fireEvent.click(screen.getByRole('button', { name: /hamstring strain/i }));
    fireEvent.click(screen.getByRole('radio', { name: 'Mild' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));

    expect(await screen.findByText('severity rejected by server')).toBeInTheDocument();
    expect(screen.getByRole('form', { name: 'Add injury' })).toBeInTheDocument();
  });
});
