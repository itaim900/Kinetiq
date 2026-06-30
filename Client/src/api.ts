export interface HealthStatus {
  status: string;
  db: string;
}

// Use an absolute origin so the same code works in the browser (dev proxy
// forwards /api) and under jsdom in tests (where fetch needs an absolute URL).
const apiBase = typeof window !== 'undefined' ? window.location.origin : '';

export async function fetchHealth(): Promise<HealthStatus> {
  const res = await fetch(`${apiBase}/api/v1/health`);
  if (!res.ok) {
    throw new Error(`Health request failed: ${res.status}`);
  }
  return (await res.json()) as HealthStatus;
}
