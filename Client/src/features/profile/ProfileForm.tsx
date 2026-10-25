import { zodResolver } from '@hookform/resolvers/zod';
import { useState } from 'react';
import { useForm } from 'react-hook-form';
import { ApiError, type Profile, type ProfileUpdate } from '../../api';
import { formatFeetInches, inToCm, type Units } from '../../lib/units';
import {
  SERVER_FIELD_TO_FORM,
  makeProfileSchema,
  profileDefaults,
  toProfileUpdate,
  type ProfileField,
  type ProfileFormValues,
} from './profileSchema';

interface ProfileFormProps {
  profile: Profile;
  units: Units;
  onSave: (update: ProfileUpdate) => Promise<Profile>;
}

export default function ProfileForm({ profile, units, onSave }: ProfileFormProps) {
  const [saved, setSaved] = useState(profile);
  const [status, setStatus] = useState<string | null>(null);
  const {
    register,
    handleSubmit,
    reset,
    setError,
    watch,
    formState: { errors, dirtyFields, isSubmitting },
  } = useForm<ProfileFormValues>({
    resolver: zodResolver(makeProfileSchema(units)),
    defaultValues: profileDefaults(profile, units),
  });

  async function submit(values: ProfileFormValues) {
    setStatus(null);
    // dirtyFields = what the user actually changed since the last save.
    const changed = (Object.keys(dirtyFields) as ProfileField[]).filter((f) => dirtyFields[f]);
    const { update, cleared } = toProfileUpdate(values, changed, saved, units);
    if (cleared.length > 0) {
      for (const field of cleared) {
        setError(field, { message: "Clearing a saved value isn't supported yet." });
      }
      return;
    }
    if (Object.keys(update).length === 0) {
      setStatus('Nothing to save.');
      return;
    }
    try {
      const updated = await onSave(update);
      setSaved(updated);
      reset(profileDefaults(updated, units)); // new baseline: nothing is "changed" any more
      setStatus('Profile saved.');
    } catch (err) {
      if (err instanceof ApiError && Object.keys(err.fieldErrors).length > 0) {
        for (const [field, messages] of Object.entries(err.fieldErrors)) {
          setError(SERVER_FIELD_TO_FORM[field] ?? (field as ProfileField), { message: messages[0] });
        }
      } else {
        setStatus(err instanceof Error ? err.message : String(err));
      }
    }
  }

  const err = (field: ProfileField) =>
    errors[field]?.message ? (
      <p role="alert" className="field-error">
        {errors[field]?.message}
      </p>
    ) : null;
  const heightIn = Number(watch('height'));

  return (
    <form className="profile-form" aria-label="Profile" onSubmit={handleSubmit(submit)} noValidate>
      <fieldset>
        <legend>Body</legend>
        <label htmlFor="height">Height ({units === 'imperial' ? 'in' : 'cm'})</label>
        <input id="height" inputMode="decimal" {...register('height')} />
        {units === 'imperial' && heightIn > 0 && (
          <p className="muted">= {formatFeetInches(inToCm(heightIn))}</p>
        )}
        {err('height')}
        <label htmlFor="weight">Weight ({units === 'imperial' ? 'lb' : 'kg'})</label>
        <input id="weight" inputMode="decimal" {...register('weight')} />
        {err('weight')}
        <label htmlFor="birth_date">Birth date</label>
        <input id="birth_date" type="date" {...register('birth_date')} />
        {err('birth_date')}
        <label htmlFor="sex">Sex</label>
        <select id="sex" {...register('sex')}>
          <option value="">Not set</option>
          <option value="female">Female</option>
          <option value="male">Male</option>
          <option value="other">Other</option>
          <option value="unspecified">Prefer not to say</option>
        </select>
        {err('sex')}
      </fieldset>

      <fieldset>
        <legend>Training</legend>
        <label htmlFor="experience_level">Experience level</label>
        <select id="experience_level" {...register('experience_level')}>
          <option value="">Not set</option>
          <option value="beginner">Beginner</option>
          <option value="intermediate">Intermediate</option>
          <option value="advanced">Advanced</option>
        </select>
        {err('experience_level')}
        <label htmlFor="weekly_workout_goal">Workouts per week</label>
        <input id="weekly_workout_goal" inputMode="numeric" {...register('weekly_workout_goal')} />
        {err('weekly_workout_goal')}
        <label htmlFor="available_time_minutes">Minutes per session</label>
        <input id="available_time_minutes" inputMode="numeric" {...register('available_time_minutes')} />
        {err('available_time_minutes')}
      </fieldset>

      <fieldset>
        <legend>Other</legend>
        <label htmlFor="medications">Medications</label>
        <textarea id="medications" rows={2} {...register('medications')} />
        {err('medications')}
      </fieldset>

      {status && (
        <p role="status" className="muted">
          {status}
        </p>
      )}
      <div className="form-actions">
        <button type="submit" className="primary" disabled={isSubmitting}>
          {isSubmitting ? 'Saving…' : 'Save profile'}
        </button>
      </div>
    </form>
  );
}
