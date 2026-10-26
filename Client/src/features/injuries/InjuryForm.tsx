import { zodResolver } from '@hookform/resolvers/zod';
import { Controller, useForm } from 'react-hook-form';
import { ApiError, type Injury, type InjuryCondition } from '../../api';
import ConditionSearch from './ConditionSearch';
import MuscleSelect from './MuscleSelect';
import {
  INJURY_TYPES,
  SERVER_FIELD_TO_FORM,
  SEVERITIES,
  TYPE_LABELS,
  formDefaults,
  injuryFormSchema,
  type InjuryFormValues,
} from './injurySchema';
import type { Catalog } from './types';

interface InjuryFormProps {
  catalog: Catalog;
  initial?: Injury; // present = edit mode
  onSave: (values: InjuryFormValues) => Promise<void>;
  onCancel: () => void;
}

const capitalize = (s: string) => s.charAt(0).toUpperCase() + s.slice(1);

export default function InjuryForm({ catalog, initial, onSave, onCancel }: InjuryFormProps) {
  const isEdit = initial !== undefined;
  const {
    register,
    control,
    handleSubmit,
    watch,
    setValue,
    setError,
    formState: { errors, isSubmitting },
  } = useForm<InjuryFormValues>({
    resolver: zodResolver(injuryFormSchema),
    defaultValues: formDefaults(initial),
  });

  // watch() re-renders the form when these change, so the right fields show.
  const conditionId = watch('conditionId');
  const unlisted = watch('unlisted');
  const condition = catalog.conditions.find((c) => c.id === conditionId);
  const regionName = (slug: string) => catalog.regions.find((r) => r.slug === slug)?.name ?? slug;

  const chooseCondition = (c: InjuryCondition) => {
    setValue('conditionId', c.id);
    setValue('unlisted', false);
  };
  const chooseUnlisted = () => {
    setValue('conditionId', null);
    setValue('unlisted', true);
  };
  const backToSearch = () => {
    setValue('conditionId', null);
    setValue('unlisted', false);
  };

  // The server has the final say: a 422 names fields, and each message is
  // shown under the matching input.
  async function submit(values: InjuryFormValues) {
    try {
      await onSave(values);
    } catch (err) {
      if (err instanceof ApiError && Object.keys(err.fieldErrors).length > 0) {
        for (const [field, messages] of Object.entries(err.fieldErrors)) {
          const formField = SERVER_FIELD_TO_FORM[field];
          if (formField) setError(formField, { message: messages[0] });
          else setError('root.server', { message: messages[0] });
        }
      } else {
        setError('root.server', { message: err instanceof Error ? err.message : String(err) });
      }
    }
  }

  const fieldError = (message?: string) =>
    message ? (
      <p role="alert" className="field-error">
        {message}
      </p>
    ) : null;
  const chosen = condition !== undefined || unlisted;

  return (
    <form
      className="injury-form"
      aria-label={isEdit ? 'Edit injury' : 'Add injury'}
      onSubmit={handleSubmit(submit)}
      noValidate
    >
      <h3>{isEdit ? 'Edit injury' : 'Add injury'}</h3>

      {!chosen && (
        <ConditionSearch
          conditions={catalog.conditions}
          regionName={regionName}
          onSelect={chooseCondition}
          onUnlisted={chooseUnlisted}
          error={errors.conditionId?.message}
        />
      )}

      {condition && (
        <section className="locked" aria-label="Condition details">
          <p>
            <strong>{condition.name}</strong> · {regionName(condition.body_region)} ·{' '}
            {TYPE_LABELS[condition.injury_type]} ·{' '}
            {condition.icd11_code ? `ICD-11 ${condition.icd11_code}` : 'no ICD-11 code'}
          </p>
          <p className="muted">Affects (from the catalog, can't be edited):</p>
          <ul className="pills" aria-label="Affected muscles">
            {condition.muscles.map((m) => (
              <li key={m.id} className={`pill pill-${m.role}`}>
                {m.name} · {m.role}
              </li>
            ))}
          </ul>
          {isEdit ? (
            <p className="muted">To change the condition, delete this injury and add it again.</p>
          ) : (
            <button type="button" className="link" onClick={backToSearch}>
              Choose a different injury
            </button>
          )}
          <p className="notice">General information, not medical advice.</p>
        </section>
      )}

      {unlisted && (
        <>
          <label htmlFor="injury-region">Where is it?</label>
          <select id="injury-region" {...register('bodyRegion')}>
            <option value="">Choose a body region</option>
            {catalog.regions.map((r) => (
              <option key={r.slug} value={r.slug}>
                {r.name}
              </option>
            ))}
          </select>
          {fieldError(errors.bodyRegion?.message)}

          <fieldset>
            <legend>Type</legend>
            {INJURY_TYPES.map((t) => (
              <label key={t} className="choice">
                <input type="radio" value={t} {...register('injuryType')} />
                {TYPE_LABELS[t]}
              </label>
            ))}
          </fieldset>
          {fieldError(errors.injuryType?.message)}

          <Controller
            control={control}
            name="muscleIds"
            render={({ field }) => (
              <MuscleSelect muscles={catalog.muscles} value={field.value} onChange={field.onChange} />
            )}
          />
          {fieldError(errors.muscleIds?.message)}

          {!isEdit && (
            <button type="button" className="link" onClick={backToSearch}>
              Back to search
            </button>
          )}
        </>
      )}

      {chosen && (
        <>
          <fieldset>
            <legend>Severity</legend>
            {SEVERITIES.map((s) => (
              <label key={s} className="choice">
                <input type="radio" value={s} {...register('severity')} />
                {capitalize(s)}
              </label>
            ))}
          </fieldset>
          {fieldError(errors.severity?.message)}

          <label htmlFor="injury-notes">Notes (optional)</label>
          <textarea id="injury-notes" rows={2} {...register('notes')} />
          {fieldError(errors.notes?.message)}
        </>
      )}

      {fieldError(errors.root?.server?.message)}

      <div className="form-actions">
        {chosen && (
          <button type="submit" className="primary" disabled={isSubmitting}>
            {isSubmitting ? 'Saving…' : 'Save'}
          </button>
        )}
        <button type="button" onClick={onCancel}>
          Cancel
        </button>
      </div>
      <p className="notice">
        General information, not medical advice. Consult a professional for diagnosis and treatment.
      </p>
    </form>
  );
}
