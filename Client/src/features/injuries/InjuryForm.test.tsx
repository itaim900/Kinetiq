import { fireEvent, render, screen, waitFor, within } from '@testing-library/react';
import { describe, expect, it, vi } from 'vitest';
import { ApiError, type Injury } from '../../api';
import { mockConditions, mockMuscles, mockRegions } from '../../mocks/handlers';
import InjuryForm from './InjuryForm';
import type { InjuryFormValues } from './injurySchema';
import type { Catalog } from './types';

const catalog: Catalog = { conditions: mockConditions, regions: mockRegions, muscles: mockMuscles };

function renderForm(
  options: { initial?: Injury; onSave?: (values: InjuryFormValues) => Promise<void> } = {},
) {
  const onSave = options.onSave ?? vi.fn().mockResolvedValue(undefined);
  const onCancel = vi.fn();
  render(
    <InjuryForm catalog={catalog} initial={options.initial} onSave={onSave} onCancel={onCancel} />,
  );
  return { onSave, onCancel };
}

describe('InjuryForm — add', () => {
  it('filters conditions as you type and always offers "not listed"', () => {
    renderForm();
    fireEvent.change(screen.getByLabelText("What's the injury?"), { target: { value: 'ham' } });
    expect(screen.getByRole('button', { name: /hamstring strain/i })).toBeInTheDocument();
    expect(screen.queryByRole('button', { name: /patellar/i })).not.toBeInTheDocument();
    expect(screen.getByRole('button', { name: /my injury isn't listed/i })).toBeInTheDocument();
  });

  it('shows a known condition locked, then saves condition + severity', async () => {
    const { onSave } = renderForm();
    fireEvent.click(screen.getByRole('button', { name: /hamstring strain/i }));

    const details = screen.getByRole('region', { name: 'Condition details' });
    expect(within(details).getByText(/ICD-11 NC76.30/)).toBeInTheDocument();
    expect(within(details).getByText('Hamstrings · primary')).toBeInTheDocument();
    expect(within(details).getByText('Glutes · secondary')).toBeInTheDocument();
    expect(within(details).getByText(/not medical advice/i)).toBeInTheDocument();
    expect(screen.queryByLabelText('Where is it?')).not.toBeInTheDocument();

    fireEvent.click(screen.getByRole('radio', { name: 'Moderate' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    await waitFor(() =>
      expect(onSave).toHaveBeenCalledWith(
        expect.objectContaining({ conditionId: 1, unlisted: false, severity: 'moderate' }),
      ),
    );
  });

  it('says "no ICD-11 code" when a condition has none', () => {
    renderForm();
    fireEvent.click(screen.getByRole('button', { name: /patellar tendinopathy/i }));
    expect(screen.getByText(/no ICD-11 code/)).toBeInTheDocument();
  });

  it('requires a severity before saving', async () => {
    const { onSave } = renderForm();
    fireEvent.click(screen.getByRole('button', { name: /hamstring strain/i }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    expect(await screen.findByText('Choose a severity')).toBeInTheDocument();
    expect(onSave).not.toHaveBeenCalled();
  });

  it('asks for region and type when the injury is not listed', async () => {
    renderForm();
    fireEvent.click(screen.getByRole('button', { name: /my injury isn't listed/i }));
    fireEvent.click(screen.getByRole('radio', { name: 'Mild' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    expect(await screen.findByText('Choose where the injury is')).toBeInTheDocument();
    expect(screen.getByText('Choose the injury type')).toBeInTheDocument();
  });

  it('saves an unlisted injury with region, type and muscles', async () => {
    const { onSave } = renderForm();
    fireEvent.click(screen.getByRole('button', { name: /my injury isn't listed/i }));
    fireEvent.change(screen.getByLabelText('Where is it?'), { target: { value: 'thigh' } });
    fireEvent.click(screen.getByRole('radio', { name: 'Joint' }));
    fireEvent.click(screen.getByRole('checkbox', { name: 'Quads' }));
    fireEvent.click(screen.getByRole('radio', { name: 'Mild' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    await waitFor(() =>
      expect(onSave).toHaveBeenCalledWith(
        expect.objectContaining({
          unlisted: true,
          bodyRegion: 'thigh',
          injuryType: 'joint',
          muscleIds: [2],
          severity: 'mild',
        }),
      ),
    );
  });

  it('shows a server field error under the field', async () => {
    const onSave = vi
      .fn()
      .mockRejectedValue(new ApiError(422, 'invalid', { severity: ['severity is not allowed'] }));
    renderForm({ onSave });
    fireEvent.click(screen.getByRole('button', { name: /hamstring strain/i }));
    fireEvent.click(screen.getByRole('radio', { name: 'Mild' }));
    fireEvent.click(screen.getByRole('button', { name: 'Save' }));
    expect(await screen.findByText('severity is not allowed')).toBeInTheDocument();
  });
});

describe('InjuryForm — edit', () => {
  const saved: Injury = {
    id: 7,
    user_id: 1,
    condition: { id: 1, name: 'Hamstring strain', icd11_code: 'NC76.30' },
    body_region: 'thigh',
    injury_type: 'muscle',
    severity: 'moderate',
    muscles: mockConditions[0].muscles,
    active: true,
    notes: null,
    created_at: '2026-10-22T10:00:00Z',
  };

  it('keeps a known condition locked and explains how to change it', () => {
    renderForm({ initial: saved });
    expect(screen.getByRole('form', { name: 'Edit injury' })).toBeInTheDocument();
    expect(screen.queryByLabelText("What's the injury?")).not.toBeInTheDocument();
    expect(screen.getByText(/delete this injury and add it again/i)).toBeInTheDocument();
    expect(screen.getByRole('radio', { name: 'Moderate' })).toBeChecked();
  });
});
