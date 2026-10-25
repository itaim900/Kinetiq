import { useState } from 'react';
import type { Injury } from '../../api';

interface InjuryCardProps {
  injury: Injury;
  regionName: string;
  onEdit?: () => void;
  onToggleActive: () => void;
  onDelete: () => void;
}

export default function InjuryCard({
  injury,
  regionName,
  onEdit,
  onToggleActive,
  onDelete,
}: InjuryCardProps) {
  const [confirmingDelete, setConfirmingDelete] = useState(false);
  const title = injury.condition ? injury.condition.name : `Other — ${regionName}`;

  return (
    <article className="injury-card" aria-label={title}>
      <h3>{title}</h3>
      <p className="muted">
        {regionName} · {injury.severity}
        {injury.notes ? ` · ${injury.notes}` : ''}
      </p>
      {injury.muscles.length > 0 && (
        <ul className="pills" aria-label="Affected muscles">
          {injury.muscles.map((m) => (
            <li key={m.id} className={`pill pill-${m.role}`}>
              {m.name}
            </li>
          ))}
        </ul>
      )}
      {confirmingDelete ? (
        <div className="card-actions">
          <span>Delete this injury?</span>
          <button type="button" onClick={onDelete}>
            Yes, delete
          </button>
          <button type="button" onClick={() => setConfirmingDelete(false)}>
            Cancel
          </button>
        </div>
      ) : (
        <div className="card-actions">
          {onEdit && (
            <button type="button" onClick={onEdit}>
              Edit
            </button>
          )}
          <button type="button" onClick={onToggleActive}>
            {injury.active ? 'Mark healed' : 'Mark active'}
          </button>
          <button type="button" onClick={() => setConfirmingDelete(true)}>
            Delete
          </button>
        </div>
      )}
    </article>
  );
}
