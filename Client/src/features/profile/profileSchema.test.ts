import { describe, expect, it } from 'vitest';
import type { Profile } from '../../api';
import { isValidDate, profileDefaults, profileFormSchema, toProfileUpdate } from './profileSchema';

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

const errorPaths = (overrides: Record<string, string>) => {
  const r = profileFormSchema.safeParse({ ...profileDefaults(saved), ...overrides });
  return r.success ? [] : r.error.issues.map((i) => i.path.join('.'));
};

describe('profileFormSchema (mirrors ProfilePolicy)', () => {
  it('accepts the saved profile', () => expect(errorPaths({})).toEqual([]));

  it('checks height and weight ranges', () => {
    expect(errorPaths({ height_cm: '0' })).toEqual(['height_cm']);
    expect(errorPaths({ height_cm: '301' })).toEqual(['height_cm']);
    expect(errorPaths({ weight_kg: '500.5' })).toEqual(['weight_kg']);
    expect(errorPaths({ weight_kg: 'abc' })).toEqual(['weight_kg']);
  });

  it('needs whole numbers for workouts and minutes', () => {
    expect(errorPaths({ weekly_workout_goal: '2.5' })).toEqual(['weekly_workout_goal']);
    expect(errorPaths({ available_time_minutes: '0' })).toEqual(['available_time_minutes']);
  });

  it('allows empty optional fields', () => expect(errorPaths({ available_time_minutes: '' })).toEqual([]));
});

describe('isValidDate', () => {
  it('accepts real dates only', () => {
    expect(isValidDate('2000-02-29')).toBe(true);
    expect(isValidDate('2001-02-29')).toBe(false);
    expect(isValidDate('01/02/2000')).toBe(false);
  });
});

describe('toProfileUpdate', () => {
  it('sends only changed fields, with numbers as numbers', () => {
    const values = { ...profileDefaults(saved), weight_kg: '80', medications: 'none' };
    expect(toProfileUpdate(values, ['weight_kg', 'medications'], saved)).toEqual({
      update: { weight_kg: 80, medications: 'none' },
      cleared: [],
    });
  });

  it('reports emptied saved values instead of sending them', () => {
    const values = { ...profileDefaults(saved), height_cm: '' };
    expect(toProfileUpdate(values, ['height_cm'], saved)).toEqual({ update: {}, cleared: ['height_cm'] });
  });
});
