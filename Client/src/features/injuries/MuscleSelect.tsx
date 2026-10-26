import type { Muscle } from '../../api';

interface MuscleSelectProps {
  muscles: Muscle[];
  value: number[];
  onChange: (ids: number[]) => void;
}

// Muscles as tappable chips (real checkboxes underneath, for accessibility),
// grouped by muscle group.
export default function MuscleSelect({ muscles, value, onChange }: MuscleSelectProps) {
  const groups = [...new Set(muscles.map((m) => m.muscle_group))];
  const toggle = (id: number) =>
    onChange(value.includes(id) ? value.filter((v) => v !== id) : [...value, id]);

  return (
    <fieldset className="muscle-select">
      <legend>Muscles involved (optional)</legend>
      {groups.map((group) => (
        <div key={group} role="group" aria-label={group}>
          <span className="muscle-group-name">{group}</span>
          {muscles
            .filter((m) => m.muscle_group === group)
            .map((m) => (
              <label key={m.id} className={value.includes(m.id) ? 'chip chip-on' : 'chip'}>
                <input type="checkbox" checked={value.includes(m.id)} onChange={() => toggle(m.id)} />
                {m.name}
              </label>
            ))}
        </div>
      ))}
    </fieldset>
  );
}
