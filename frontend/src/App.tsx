import { useEffect, useState } from 'react';
import { fetchHealth, type HealthStatus } from './api';

export default function App() {
  const [health, setHealth] = useState<HealthStatus | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    fetchHealth()
      .then(setHealth)
      .catch((e: unknown) => setError(e instanceof Error ? e.message : String(e)));
  }, []);

  return (
    <main
      style={{
        fontFamily: 'system-ui, sans-serif',
        padding: '2rem',
        maxWidth: 640,
        margin: '0 auto',
      }}
    >
      <h1>Fit App</h1>
      <p>Walking skeleton — the React frontend talking to the C++ backend.</p>

      <section
        aria-label="backend-health"
        style={{
          marginTop: '1.5rem',
          padding: '1rem 1.25rem',
          border: '1px solid #ddd',
          borderRadius: 12,
        }}
      >
        <h2 style={{ marginTop: 0 }}>Backend health</h2>
        {error && (
          <p role="alert" style={{ color: '#b00020' }}>
            Backend unreachable: {error}
          </p>
        )}
        {!error && !health && <p>Checking…</p>}
        {health && (
          <ul>
            <li>
              API status: <strong>{health.status}</strong>
            </li>
            <li>
              Database: <strong>{health.db}</strong>
            </li>
          </ul>
        )}
      </section>
    </main>
  );
}
