import { useUnits } from './lib/UnitsContext';
import type { Units } from './lib/units';

// Metric | Imperial switch for the whole app (saved to the account).
export default function UnitsToggle() {
  const { units, setUnits, error } = useUnits();
  const option = (value: Units, label: string) => (
    <button
      type="button"
      aria-pressed={units === value}
      className={units === value ? 'units-option active' : 'units-option'}
      onClick={() => {
        if (units !== value) void setUnits(value);
      }}
    >
      {label}
    </button>
  );
  return (
    <div className="units-toggle" role="group" aria-label="Units">
      {option('metric', 'Metric')}
      {option('imperial', 'Imperial')}
      {error && (
        <span role="alert" className="field-error">
          {error}
        </span>
      )}
    </div>
  );
}
