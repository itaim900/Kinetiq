import { z } from 'zod';
import type { Profile, ProfileUpdate } from '../../api';

// A real calendar date in YYYY-MM-DD (rejects 2001-02-29), like the server.
export function isValidDate(s: string): boolean {
  if (!/^\d{4}-\d{2}-\d{2}$/.test(s)) return false;
  const d = new Date(`${s}T00:00:00Z`);
  return !Number.isNaN(d.getTime()) && d.toISOString().startsWith(s);
}

// Empty means "not set"; otherwise the number must pass `ok`.
const blankOr = (ok: (n: number) => boolean) => (s: string) => s.trim() === '' || ok(Number(s));

// Mirrors the server's ProfilePolicy so users get the same answer instantly.
export const profileFormSchema = z.object({
  height_cm: z
    .string()
    .refine(blankOr((n) => n > 0 && n <= 300), 'Height must be more than 0 and at most 300 cm'),
  weight_kg: z
    .string()
    .refine(blankOr((n) => n > 0 && n <= 500), 'Weight must be more than 0 and at most 500 kg'),
  birth_date: z.string().refine((s) => s === '' || isValidDate(s), 'Enter a real date (YYYY-MM-DD)'),
  sex: z.string(),
  experience_level: z.string(),
  weekly_workout_goal: z
    .string()
    .refine(blankOr((n) => Number.isInteger(n) && n >= 0), 'Use a whole number, 0 or more'),
  available_time_minutes: z
    .string()
    .refine(blankOr((n) => Number.isInteger(n) && n > 0), 'Use a whole number above 0'),
  medications: z.string(),
  unit_preference: z.enum(['metric', 'imperial']),
});

export type ProfileFormValues = z.infer<typeof profileFormSchema>;
export type ProfileField = keyof ProfileFormValues;

const NUMBER_FIELDS: ProfileField[] = [
  'height_cm',
  'weight_kg',
  'weekly_workout_goal',
  'available_time_minutes',
];

export function profileDefaults(p: Profile): ProfileFormValues {
  const s = (v: string | number | null) => (v === null ? '' : String(v));
  return {
    height_cm: s(p.height_cm),
    weight_kg: s(p.weight_kg),
    birth_date: s(p.birth_date),
    sex: s(p.sex),
    experience_level: s(p.experience_level),
    weekly_workout_goal: s(p.weekly_workout_goal),
    available_time_minutes: s(p.available_time_minutes),
    medications: s(p.medications),
    unit_preference: p.unit_preference,
  };
}

// Turns the changed fields into a PATCH body. The server can't clear a saved
// value yet (absent = unchanged), so emptied saved values are reported back
// in `cleared` instead of being silently dropped.
export function toProfileUpdate(
  values: ProfileFormValues,
  changed: ProfileField[],
  saved: Profile,
): { update: ProfileUpdate; cleared: ProfileField[] } {
  const update: Record<string, string | number> = {};
  const cleared: ProfileField[] = [];
  for (const field of changed) {
    const raw = values[field].trim();
    if (raw === '') {
      if (saved[field] !== null) cleared.push(field);
      continue;
    }
    update[field] = NUMBER_FIELDS.includes(field) ? Number(raw) : raw;
  }
  return { update: update as ProfileUpdate, cleared };
}
