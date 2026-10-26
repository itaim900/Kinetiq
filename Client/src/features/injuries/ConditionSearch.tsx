import { useState } from 'react';
import type { InjuryCondition } from '../../api';

interface ConditionSearchProps {
  conditions: InjuryCondition[];
  regionName: (slug: string) => string;
  onSelect: (condition: InjuryCondition) => void;
  onUnlisted: () => void;
  error?: string;
}

// Type to filter the known conditions; "My injury isn't listed…" is always last.
export default function ConditionSearch({
  conditions,
  regionName,
  onSelect,
  onUnlisted,
  error,
}: ConditionSearchProps) {
  const [query, setQuery] = useState('');
  const q = query.trim().toLowerCase();
  const matches = conditions.filter((c) => c.name.toLowerCase().includes(q));

  return (
    <div className="condition-search">
      <label htmlFor="condition-query">What's the injury?</label>
      <input
        id="condition-query"
        type="search"
        autoComplete="off"
        placeholder="Start typing, e.g. hamstring"
        value={query}
        onChange={(e) => setQuery(e.target.value)}
      />
      <ul className="condition-results" aria-label="Matching conditions">
        {matches.map((c) => (
          <li key={c.id}>
            <button type="button" onClick={() => onSelect(c)}>
              {c.name} <span className="muted">· {regionName(c.body_region)}</span>
            </button>
          </li>
        ))}
        <li>
          <button type="button" className="unlisted" onClick={onUnlisted}>
            My injury isn't listed…
          </button>
        </li>
      </ul>
      {error && (
        <p role="alert" className="field-error">
          {error}
        </p>
      )}
    </div>
  );
}
