import { describe, expect, it } from 'vitest';
import type { Profile } from '../../api';
import { isValidDate, makeProfileSchema, profileDefaults, toProfileUpdate } from './profileSchema';

const saved: Profile = {
  id: 1,
  email: 'a@example.com',
  display_name: 'A',
  sex: null,
  birth_date: '1990-05-01',
  height_cm: 178,
  weight_kg: 74,
  experience_level: 'intermediate',
  unit_preference: 'metric',
  weekly_workout_goal: 3,
  available_time_minutes: null,
  medications: null,
};

const errorPaths = (units: 'metric' | 'imperial', overrides: Record<string, string>) => {
  const r = makeProfileSchema(units).safeParse({ ...profileDefaults(saved, units), ...overrides });
  return r.success ? [] : r.error.issues.map((i) => i.path.join('.'));
};

describe('makeProfileSchema (mirrors ProfilePolicy)', () => {
  it('accepts the saved profile in both unit systems', () => {
    expect(errorPaths('metric', {})).toEqual([]);
    expect(errorPaths('imperial', {})).toEqual([]);
  });

  it('checks limits in metric', () => {
    expect(errorPaths('metric', { height: '0' })).toEqual(['height']);
    expect(errorPaths('metric', { height: '301' })).toEqual(['height']);
    expect(errorPaths('metric', { weight: '500.5' })).toEqual(['weight']);
    expect(errorPaths('metric', { weight: 'abc' })).toEqual(['weight']);
  });

  it('checks the same limits in imperial', () => {
    expect(errorPaths('imperial', { height: '118.1' })).toEqual([]); // ≈ 300 cm
    expect(errorPaths('imperial', { height: '118.3' })).toEqual(['height']);
    expect(errorPaths('imperial', { weight: '1102.3' })).toEqual([]); // ≈ 500 kg
    expect(errorPaths('imperial', { weight: '1103' })).toEqual(['weight']);
  });

  it('words imperial limits in imperial units', () => {
    const r = makeProfileSchema('imperial').safeParse({ ...profileDefaults(saved, 'imperial'), height: '200' });
    expect(r.success ? '' : r.error.issues[0].message).toMatch(/118\.1 in/);
  });

  it('needs whole numbers for workouts and minutes', () => {
    expect(errorPaths('metric', { weekly_workout_goal: '2.5' })).toEqual(['weekly_workout_goal']);
    expect(errorPaths('metric', { available_time_minutes: '0' })).toEqual(['available_time_minutes']);
  });
});

describe('isValidDate', () => {
  it('accepts real dates only', () => {
    expect(isValidDate('2000-02-29')).toBe(true);
    expect(isValidDate('2001-02-29')).toBe(false);
    expect(isValidDate('01/02/2000')).toBe(false);
  });
});

describe('profileDefaults', () => {
  it('shows saved values in the chosen units', () => {
    expect(profileDefaults(saved, 'metric')).toMatchObject({ height: '178', weight: '74' });
    expect(profileDefaults(saved, 'imperial')).toMatchObject({ height: '70.1', weight: '163.1' });
  });
});

describe('toProfileUpdate', () => {
  it('sends only changed fields, converted to metric', () => {
    const values = { ...profileDefaults(saved, 'imperial'), weight: '180' };
    expect(toProfileUpdate(values, ['weight'], saved, 'imperial')).toEqual({
      update: { weight_kg: 81.6 },
      cleared: [],
    });
  });

  it('never re-sends untouched height/weight', () => {
    const values = profileDefaults(saved, 'imperial');
    expect(toProfileUpdate(values, [], saved, 'imperial')).toEqual({ update: {}, cleared: [] });
  });

  it('reports emptied saved values', () => {
    const values = { ...profileDefaults(saved, 'metric'), height: '' };
    expect(toProfileUpdate(values, ['height'], saved, 'metric')).toEqual({ update: {}, cleared: ['height'] });
  });
});
