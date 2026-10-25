import { useEffect, useState } from 'react';
import {
  deleteInjury,
  listBodyRegions,
  listConditions,
  listInjuries,
  listMuscles,
  updateInjury,
  type Injury,
} from '../../api';
import InjuryCard from './InjuryCard';
import type { Catalog } from './types';

const message = (e: unknown) => (e instanceof Error ? e.message : String(e));

export default function InjuriesPage() {
  const [injuries, setInjuries] = useState<Injury[]>([]);
  const [catalog, setCatalog] = useState<Catalog | null>(null);
  const [loadError, setLoadError] = useState<string | null>(null);
  const [actionError, setActionError] = useState<string | null>(null);

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
  const card = (i: Injury) => (
    <InjuryCard
      key={i.id}
      injury={i}
      regionName={regionName(i.body_region)}
      onToggleActive={() => toggleActive(i)}
      onDelete={() => remove(i)}
    />
  );

  return (
    <section className="page" aria-labelledby="injuries-title">
      <div className="page-head">
        <h2 id="injuries-title">My injuries</h2>
      </div>
      {actionError && (
        <p role="alert" className="field-error">
          {actionError}
        </p>
      )}
      {active.length === 0 && <p className="muted">No injuries yet.</p>}
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
