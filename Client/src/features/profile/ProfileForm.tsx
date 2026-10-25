import { zodResolver } from '@hookform/resolvers/zod';
import { useState } from 'react';
import { useForm } from 'react-hook-form';
import { ApiError, type Profile, type ProfileUpdate } from '../../api';
import {
  profileDefaults,
  profileFormSchema,
  toProfileUpdate,
  type ProfileField,
  type ProfileFormValues,
} from './profileSchema';

interface ProfileFormProps {
  profile: Profile;
  onSave: (update: ProfileUpdate) => Promise<Profile>;
}

export default function ProfileForm({ profile, onSave }: ProfileFormProps) {
  const [saved, setSaved] = useState(profile);
  const [status, setStatus] = useState<string | null>(null);
  const {
    register,
    handleSubmit,
    reset,
    setError,
    formState: { errors, dirtyFields, isSubmitting },
  } = useForm<ProfileFormValues>({
    resolver: zodResolver(profileFormSchema),
    defaultValues: profileDefaults(profile),
  });

  async function submit(values: ProfileFormValues) {
    setStatus(null);
    // dirtyFields = what the user actually changed since the last save.
    const changed = (Object.keys(dirtyFields) as ProfileField[]).filter((f) => dirtyFields[f]);
    const { update, cleared } = toProfileUpdate(values, changed, saved);
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
      reset(profileDefaults(updated)); // new baseline: nothing is "changed" any more
      setStatus('Profile saved.');
    } catch (err) {
      if (err instanceof ApiError && Object.keys(err.fieldErrors).length > 0) {
        for (const [field, messages] of Object.entries(err.fieldErrors)) {
          setError(field as ProfileField, { message: messages[0] });
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

  return (
    <form className="profile-form" aria-label="Profile" onSubmit={handleSubmit(submit)} noValidate>
      <fieldset>
        <legend>Body</legend>
        <label htmlFor="height_cm">Height (cm)</label>
        <input id="height_cm" inputMode="decimal" {...register('height_cm')} />
        {err('height_cm')}
        <label htmlFor="weight_kg">Weight (kg)</label>
        <input id="weight_kg" inputMode="decimal" {...register('weight_kg')} />
        {err('weight_kg')}
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
        <label htmlFor="unit_preference">Units</label>
        <select id="unit_preference" {...register('unit_preference')}>
          <option value="metric">Metric</option>
          <option value="imperial">Imperial (display coming later)</option>
        </select>
        {err('unit_preference')}
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
