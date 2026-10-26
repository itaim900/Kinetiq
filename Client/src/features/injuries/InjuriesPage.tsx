import { useEffect, useState } from 'react';
import {
  addInjury,
  deleteInjury,
  listBodyRegions,
  listConditions,
  listInjuries,
  listMuscles,
  updateInjury,
  type Injury,
} from '../../api';
import InjuryCard from './InjuryCard';
import InjuryForm from './InjuryForm';
import { toAddRequest, toUpdateRequest, type InjuryFormValues } from './injurySchema';
import type { Catalog } from './types';

const message = (e: unknown) => (e instanceof Error ? e.message : String(e));

export default function InjuriesPage() {
  const [injuries, setInjuries] = useState<Injury[]>([]);
  const [catalog, setCatalog] = useState<Catalog | null>(null);
  const [loadError, setLoadError] = useState<string | null>(null);
  const [actionError, setActionError] = useState<string | null>(null);
  // Which inline form is open: none, "add" (top of the list), or editing one card (its id).
  const [editing, setEditing] = useState<'none' | 'add' | number>('none');

  // Load the user's injuries and the reference catalog together, once.
  useEffect(() => {
    Promise.all([listInjuries(), listConditions(), listBodyRegions(), listMuscles()])
      .then(([loaded, conditions, regions, muscles]) => {
        setInjuries(loaded);
        setCatalog({ conditions, regions, muscles });
      })
      .catch((e: unknown) => setLoadError(message(e)));
  }, []);

  const regionName = (slug: string) => catalog?.regions.find((r) => r.slug === slug)?.name ?? slug;
  const replace = (updated: Injury) =>
    setInjuries((list) => list.map((i) => (i.id === updated.id ? updated : i)));

  // saveNew / saveEdit let errors propagate: InjuryForm catches them and shows
  // each message under its field, keeping the form open.
  async function saveNew(values: InjuryFormValues) {
    const created = await addInjury(toAddRequest(values));
    setInjuries((list) => [...list, created]);
    setEditing('none');
  }

  async function saveEdit(original: Injury, values: InjuryFormValues) {
    const request = toUpdateRequest(values, original);
    if (Object.keys(request).length > 0) replace(await updateInjury(original.id, request));
    setEditing('none');
  }

  async function toggleActive(injury: Injury) {
    setActionError(null);
    try {
      replace(await updateInjury(injury.id, { active: !injury.active }));
    } catch (e) {
      setActionError(message(e));
    }
  }

  async function remove(injury: Injury) {
    setActionError(null);
    try {
      await deleteInjury(injury.id);
      setInjuries((list) => list.filter((i) => i.id !== injury.id));
    } catch (e) {
      setActionError(message(e));
    }
  }

  if (loadError) return <p role="alert">Couldn't load injuries: {loadError}</p>;
  if (!catalog) return <p>Loading…</p>;

  const active = injuries.filter((i) => i.active);
  const healed = injuries.filter((i) => !i.active);
  // A card, or — if it's the one being edited — the same form in its place.
  const card = (i: Injury) =>
    editing === i.id ? (
      <InjuryForm
        key={i.id}
        catalog={catalog}
        initial={i}
        onSave={(values) => saveEdit(i, values)}
        onCancel={() => setEditing('none')}
      />
    ) : (
      <InjuryCard
        key={i.id}
        injury={i}
        regionName={regionName(i.body_region)}
        onEdit={() => setEditing(i.id)}
        onToggleActive={() => toggleActive(i)}
        onDelete={() => remove(i)}
      />
    );

  return (
    <section className="page" aria-labelledby="injuries-title">
      <div className="page-head">
        <h2 id="injuries-title">My injuries</h2>
        {editing !== 'add' && (
          <button type="button" className="primary" onClick={() => setEditing('add')}>
            + Add injury
          </button>
        )}
      </div>
      {editing === 'add' && (
        <InjuryForm catalog={catalog} onSave={saveNew} onCancel={() => setEditing('none')} />
      )}
      {actionError && (
        <p role="alert" className="field-error">
          {actionError}
        </p>
      )}
      {active.length === 0 && editing !== 'add' && <p className="muted">No injuries yet.</p>}
      {active.map(card)}
      {healed.length > 0 && (
        <details className="healed">
          <summary>Healed ({healed.length})</summary>
          {healed.map(card)}
        </details>
      )}
    </section>
  );
}
