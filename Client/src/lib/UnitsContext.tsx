import { createContext, useContext, useEffect, useState, type ReactNode } from 'react';
import { getProfile, updateProfile } from '../api';
import type { Units } from './units';

export interface UnitsValue {
  units: Units;
  setUnits: (units: Units) => Promise<void>;
  error: string | null;
}

// Default (no provider): metric, switching does nothing -- keeps components
// usable in isolated tests.
export const UnitsContext = createContext<UnitsValue>({
  units: 'metric',
  setUnits: async () => {},
  error: null,
});

export const useUnits = () => useContext(UnitsContext);

// App-wide unit system, loaded from the account and saved back to it.
export function UnitsProvider({ children }: { children: ReactNode }) {
  const [units, setUnitsState] = useState<Units>('metric');
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    getProfile()
      .then((p) => setUnitsState(p.unit_preference))
      .catch(() => {
        // keep metric; a 401 is handled globally by authFetch
      });
  }, []);

  // Optimistic: switch now, save in the background, switch back on failure.
  async function setUnits(next: Units) {
    const previous = units;
    setUnitsState(next);
    setError(null);
    try {
      await updateProfile({ unit_preference: next });
    } catch {
      setUnitsState(previous);
      setError("Couldn't save your units — try again.");
    }
  }

  return <UnitsContext.Provider value={{ units, setUnits, error }}>{children}</UnitsContext.Provider>;
}
