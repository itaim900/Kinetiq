import { z } from 'zod';
import type { Profile, ProfileUpdate } from '../../api';
import { cmToIn, inToCm, kgToLb, lbToKg, round1, type Units } from '../../lib/units';

// A real calendar date in YYYY-MM-DD (rejects 2001-02-29), like the server.
export function isValidDate(s: string): boolean {
  if (!/^\d{4}-\d{2}-\d{2}$/.test(s)) return false;
  const d = new Date(`${s}T00:00:00Z`);
  return !Number.isNaN(d.getTime()) && d.toISOString().startsWith(s);
}

const HEIGHT_MAX_CM = 300;
const WEIGHT_MAX_KG = 500;

// Empty means "not set"; otherwise the number must pass `ok`.
const blankOr = (ok: (n: number) => boolean) => (s: string) => s.trim() === '' || ok(Number(s));

// What the server will store for a typed height/weight: metric, rounded.
export const heightToCm = (value: number, units: Units) =>
  round1(units === 'imperial' ? inToCm(value) : value);
export const weightToKg = (value: number, units: Units) =>
  round1(units === 'imperial' ? lbToKg(value) : value);

// Same limits as the server's ProfilePolicy, checked in metric, worded in the
// user's units.
export function makeProfileSchema(units: Units) {
  const heightMax =
    units === 'imperial' ? `${round1(cmToIn(HEIGHT_MAX_CM))} in` : `${HEIGHT_MAX_CM} cm`;
  const weightMax =
    units === 'imperial' ? `${round1(kgToLb(WEIGHT_MAX_KG))} lb` : `${WEIGHT_MAX_KG} kg`;
  return z.object({
    height: z
      .string()
      .refine(
        blankOr((n) => heightToCm(n, units) > 0 && heightToCm(n, units) <= HEIGHT_MAX_CM),
        `Height must be more than 0 and at most ${heightMax}`,
      ),
    weight: z
      .string()
      .refine(
        blankOr((n) => weightToKg(n, units) > 0 && weightToKg(n, units) <= WEIGHT_MAX_KG),
        `Weight must be more than 0 and at most ${weightMax}`,
      ),
    birth_date: z
      .string()
      .refine((s) => s === '' || isValidDate(s), 'Enter a real date (YYYY-MM-DD)'),
    sex: z.string(),
    experience_level: z.string(),
    weekly_workout_goal: z
      .string()
      .refine(blankOr((n) => Number.isInteger(n) && n >= 0), 'Use a whole number, 0 or more'),
    available_time_minutes: z
      .string()
      .refine(blankOr((n) => Number.isInteger(n) && n > 0), 'Use a whole number above 0'),
    medications: z.string(),
  });
}

export type ProfileFormValues = z.infer<ReturnType<typeof makeProfileSchema>>;
export type ProfileField = keyof ProfileFormValues;

// Form field -> server field (height/weight are unit-neutral in the form).
const SERVER_NAME: Record<ProfileField, keyof ProfileUpdate> = {
  height: 'height_cm',
  weight: 'weight_kg',
  birth_date: 'birth_date',
  sex: 'sex',
  experience_level: 'experience_level',
  weekly_workout_goal: 'weekly_workout_goal',
  available_time_minutes: 'available_time_minutes',
  medications: 'medications',
};

// Server field -> form field, so a 422 lands under the right input.
export const SERVER_FIELD_TO_FORM: Record<string, ProfileField> = Object.fromEntries(
  Object.entries(SERVER_NAME).map(([form, server]) => [server, form]),
) as Record<string, ProfileField>;

const INTEGER_FIELDS: ProfileField[] = ['weekly_workout_goal', 'available_time_minutes'];

export function profileDefaults(p: Profile, units: Units): ProfileFormValues {
  const s = (v: string | number | null) => (v === null ? '' : String(v));
  return {
    height:
      p.height_cm === null
        ? ''
        : String(units === 'imperial' ? round1(cmToIn(p.height_cm)) : p.height_cm),
    weight:
      p.weight_kg === null
        ? ''
        : String(units === 'imperial' ? round1(kgToLb(p.weight_kg)) : p.weight_kg),
    birth_date: s(p.birth_date),
    sex: s(p.sex),
    experience_level: s(p.experience_level),
    weekly_workout_goal: s(p.weekly_workout_goal),
    available_time_minutes: s(p.available_time_minutes),
    medications: s(p.medications),
  };
}

// Turns the fields the user changed into a PATCH body (always metric).
// Emptying a saved value sends null (clear it) and lists it in `cleared`.
export function toProfileUpdate(
  values: ProfileFormValues,
  changed: ProfileField[],
  saved: Profile,
  units: Units,
): { update: ProfileUpdate; cleared: ProfileField[] } {
  const update: Record<string, string | number | null> = {};
  const cleared: ProfileField[] = [];
  for (const field of changed) {
    const raw = values[field].trim();
    const serverField = SERVER_NAME[field];
    if (raw === '') {
      if (saved[serverField as keyof Profile] !== null) {
        update[serverField] = null; // JSON Merge Patch: null clears it
        cleared.push(field);
      }
      continue;
    }
    if (field === 'height') update[serverField] = heightToCm(Number(raw), units);
    else if (field === 'weight') update[serverField] = weightToKg(Number(raw), units);
    else if (INTEGER_FIELDS.includes(field)) update[serverField] = Number(raw);
    else update[serverField] = raw;
  }
  return { update: update as ProfileUpdate, cleared };
}

const FIELD_LABELS: Record<ProfileField, string> = {
  height: 'height',
  weight: 'weight',
  birth_date: 'birth date',
  sex: 'sex',
  experience_level: 'experience level',
  weekly_workout_goal: 'workouts per week',
  available_time_minutes: 'minutes per session',
  medications: 'medications',
};

// "Weight cleared. Kinetiq will keep using your last saved weight (74 kg) for
// training plans." -- the server keeps the old value in the profile history
// (ADR-0017), so the user is told rather than surprised.
export function clearedMessage(field: ProfileField, saved: Profile, units: Units): string {
  const label = FIELD_LABELS[field];
  const unit =
    field === 'height' ? (units === 'imperial' ? ' in' : ' cm')
    : field === 'weight' ? (units === 'imperial' ? ' lb' : ' kg')
    : '';
  const last = profileDefaults(saved, units)[field];
  const title = label.charAt(0).toUpperCase() + label.slice(1);
  return `${title} cleared. Kinetiq will keep using your last saved ${label} (${last}${unit}) for training plans.`;
}
