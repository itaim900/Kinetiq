import { describe, expect, it } from 'vitest';
import type { Injury } from '../../api';
import {
  formDefaults,
  injuryFormSchema,
  toAddRequest,
  toUpdateRequest,
  type InjuryFormValues,
} from './injurySchema';

const known: InjuryFormValues = {
  ...formDefaults(),
  conditionId: 1,
  severity: 'moderate',
};
const unlisted: InjuryFormValues = {
  ...formDefaults(),
  unlisted: true,
  bodyRegion: 'thigh',
  injuryType: 'joint',
  muscleIds: [2, 3],
  severity: 'mild',
};

function issuePaths(values: InjuryFormValues): string[] {
  const result = injuryFormSchema.safeParse(values);
  return result.success ? [] : result.error.issues.map((i) => i.path.join('.'));
}

const savedKnown: Injury = {
  id: 7,
  user_id: 1,
  condition: { id: 1, name: 'Hamstring strain', icd11_code: 'NC76.30' },
  body_region: 'thigh',
  injury_type: 'muscle',
  severity: 'moderate',
  muscles: [{ id: 1, name: 'Hamstrings', role: 'primary' }],
  active: true,
  notes: null,
  created_at: '2026-10-22T10:00:00Z',
};
const savedUnlisted: Injury = {
  ...savedKnown,
  condition: null,
  body_region: 'thigh',
  injury_type: 'joint',
  muscles: [
    { id: 2, name: 'Quads', role: 'primary' },
    { id: 3, name: 'Calves', role: 'primary' },
  ],
};

describe('injuryFormSchema', () => {
  it('accepts a known condition with a severity', () => {
    expect(issuePaths(known)).toEqual([]);
  });

  it('requires a severity', () => {
    expect(issuePaths({ ...known, severity: '' })).toEqual(['severity']);
  });

  it('requires a condition unless the injury is unlisted', () => {
    expect(issuePaths({ ...known, conditionId: null })).toEqual(['conditionId']);
  });

  it('requires region and type for an unlisted injury', () => {
    expect(issuePaths({ ...unlisted, bodyRegion: '', injuryType: '' }).sort()).toEqual([
      'bodyRegion',
      'injuryType',
    ]);
  });

  it('limits notes to 500 characters', () => {
    expect(issuePaths({ ...known, notes: 'x'.repeat(501) })).toEqual(['notes']);
  });
});

describe('toAddRequest', () => {
  it('sends only condition, severity and (trimmed) notes for a known condition', () => {
    expect(toAddRequest(known)).toEqual({ condition_id: 1, severity: 'moderate' });
    expect(toAddRequest({ ...known, notes: '  left leg ' })).toEqual({
      condition_id: 1,
      severity: 'moderate',
      notes: 'left leg',
    });
  });

  it('sends region, type and muscles for an unlisted injury', () => {
    expect(toAddRequest(unlisted)).toEqual({
      body_region: 'thigh',
      injury_type: 'joint',
      muscle_ids: [2, 3],
      severity: 'mild',
    });
  });
});

describe('toUpdateRequest', () => {
  it('sends only what changed', () => {
    expect(toUpdateRequest(formDefaults(savedKnown), savedKnown)).toEqual({});
    expect(toUpdateRequest({ ...formDefaults(savedKnown), severity: 'severe' }, savedKnown)).toEqual({
      severity: 'severe',
    });
  });

  it('never sends locked fields for a known condition', () => {
    const values = { ...formDefaults(savedKnown), bodyRegion: 'knee', muscleIds: [9] };
    expect(toUpdateRequest(values, savedKnown)).toEqual({});
  });

  it('sends region, type and the full muscle set for an unlisted injury', () => {
    const values = { ...formDefaults(savedUnlisted), injuryType: 'bone', muscleIds: [3] };
    expect(toUpdateRequest(values, savedUnlisted)).toEqual({ injury_type: 'bone', muscle_ids: [3] });
    // Same muscles in another order = no change.
    expect(toUpdateRequest({ ...formDefaults(savedUnlisted), muscleIds: [3, 2] }, savedUnlisted)).toEqual({});
  });

  it('can clear notes', () => {
    const saved = { ...savedKnown, notes: 'old' };
    expect(toUpdateRequest({ ...formDefaults(saved), notes: '' }, saved)).toEqual({ notes: '' });
  });
});

describe('formDefaults', () => {
  it('pre-fills an edit form from a saved injury', () => {
    expect(formDefaults(savedUnlisted)).toEqual({
      conditionId: null,
      unlisted: true,
      bodyRegion: 'thigh',
      injuryType: 'joint',
      muscleIds: [2, 3],
      severity: 'moderate',
      notes: '',
    });
  });
});
