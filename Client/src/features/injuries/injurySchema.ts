import { z } from 'zod';
import type {
  AddInjuryRequest,
  Injury,
  InjuryType,
  Severity,
  UpdateInjuryRequest,
} from '../../api';

export const SEVERITIES = ['mild', 'moderate', 'severe'] as const;
export const INJURY_TYPES = ['muscle', 'bone', 'tendon_ligament', 'joint', 'other'] as const;

export const TYPE_LABELS: Record<InjuryType, string> = {
  muscle: 'Muscle',
  bone: 'Bone',
  tendon_ligament: 'Tendon / ligament',
  joint: 'Joint',
  other: 'Other',
};

// The form's values. Everything is a plain string/number so inputs can start
// empty; the rules below (mirroring the server) decide what's valid.
export const injuryFormSchema = z
  .object({
    conditionId: z.number().nullable(), // set = known condition
    unlisted: z.boolean(), // true = "My injury isn't listed…"
    bodyRegion: z.string(),
    injuryType: z.string(),
    muscleIds: z.array(z.number()),
    severity: z
      .string()
      .refine((s) => (SEVERITIES as readonly string[]).includes(s), 'Choose a severity'),
    notes: z.string().max(500, 'Notes can be at most 500 characters'),
  })
  .superRefine((v, ctx) => {
    if (!v.unlisted && v.conditionId === null) {
      ctx.addIssue({
        code: z.ZodIssueCode.custom,
        path: ['conditionId'],
        message: "Pick an injury, or choose \"My injury isn't listed…\"",
      });
    }
    if (v.unlisted) {
      if (v.bodyRegion === '') {
        ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['bodyRegion'], message: 'Choose where the injury is' });
      }
      if (!(INJURY_TYPES as readonly string[]).includes(v.injuryType)) {
        ctx.addIssue({ code: z.ZodIssueCode.custom, path: ['injuryType'], message: 'Choose the injury type' });
      }
    }
  });

export type InjuryFormValues = z.infer<typeof injuryFormSchema>;

export function formDefaults(initial?: Injury): InjuryFormValues {
  if (!initial) {
    return {
      conditionId: null,
      unlisted: false,
      bodyRegion: '',
      injuryType: '',
      muscleIds: [],
      severity: '',
      notes: '',
    };
  }
  return {
    conditionId: initial.condition?.id ?? null,
    unlisted: initial.condition === null,
    bodyRegion: initial.body_region,
    injuryType: initial.injury_type,
    muscleIds: initial.muscles.map((m) => m.id),
    severity: initial.severity,
    notes: initial.notes ?? '',
  };
}

// Builds the POST body: the known-condition shape or the unlisted shape.
export function toAddRequest(v: InjuryFormValues): AddInjuryRequest {
  const notes = v.notes.trim();
  const withNotes = notes ? { notes } : {};
  if (!v.unlisted && v.conditionId !== null) {
    return { condition_id: v.conditionId, severity: v.severity as Severity, ...withNotes };
  }
  return {
    body_region: v.bodyRegion,
    injury_type: v.injuryType as InjuryType,
    muscle_ids: v.muscleIds,
    severity: v.severity as Severity,
    ...withNotes,
  };
}

const sameIds = (a: number[], b: number[]) => {
  const x = [...a].sort((p, q) => p - q);
  const y = [...b].sort((p, q) => p - q);
  return x.length === y.length && x.every((id, i) => id === y[i]);
};

// Builds the PATCH body with only the fields that changed. A known condition's
// region/type/muscles are locked, so they are never sent for one.
export function toUpdateRequest(v: InjuryFormValues, initial: Injury): UpdateInjuryRequest {
  const request: UpdateInjuryRequest = {};
  if (v.severity !== initial.severity) request.severity = v.severity as Severity;
  const notes = v.notes.trim();
  if (notes !== (initial.notes ?? '')) request.notes = notes;
  if (initial.condition === null) {
    if (v.bodyRegion !== initial.body_region) request.body_region = v.bodyRegion;
    if (v.injuryType !== initial.injury_type) request.injury_type = v.injuryType as InjuryType;
    const savedIds = initial.muscles.map((m) => m.id);
    if (!sameIds(v.muscleIds, savedIds)) request.muscle_ids = v.muscleIds;
  }
  return request;
}

// Server field name → form field name, so a 422 lands under the right input.
export const SERVER_FIELD_TO_FORM: Record<string, keyof InjuryFormValues> = {
  condition_id: 'conditionId',
  body_region: 'bodyRegion',
  injury_type: 'injuryType',
  muscle_ids: 'muscleIds',
  severity: 'severity',
  notes: 'notes',
};
