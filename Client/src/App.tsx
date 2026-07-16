import { useEffect, useState } from 'react';
import { fetchHealth, fetchMe, type AuthResult, type AuthUser, type HealthStatus } from './api';
import AuthPage from './features/auth/AuthPage';
import './features/auth/auth-theme.css';
import { clearToken, getToken, saveToken } from './lib/tokenStorage';

export default function App() {
  const [health, setHealth] = useState<HealthStatus | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [currentUser, setCurrentUser] = useState<AuthUser | null>(null);
  const [isRestoringSession, setIsRestoringSession] = useState(true);

  useEffect(() => {
    fetchHealth()
      .then(setHealth)
      .catch((e: unknown) => setError(e instanceof Error ? e.message : String(e)));
  }, []);

  // On load, a stored token might already be expired or otherwise invalid
  // (e.g. the server's JWT_SECRET changed) -- GET /auth/me is the check;
  // failure means "treat it as not logged in," not an error to surface.
  useEffect(() => {
    const token = getToken();
    if (!token) {
      setIsRestoringSession(false);
      return;
    }
    fetchMe(token)
      .then(setCurrentUser)
      .catch(() => clearToken())
      .finally(() => setIsRestoringSession(false));
  }, []);

  function handleAuthenticated(result: AuthResult) {
    saveToken(result.access_token);
    setCurrentUser(result.user);
  }

  function handleLogout() {
    clearToken();
    setCurrentUser(null);
  }

  if (!currentUser) {
    return (
      <div className="auth-backdrop">
        <div className="auth-health-badge" aria-label="backend-health">
          {error && <span role="alert">Backend unreachable</span>}
          {!error && !health && <span>Checking…</span>}
          {health && (
            <>
              <span>
                API: <strong>{health.status}</strong>
              </span>
              <span>
                DB: <strong>{health.db}</strong>
              </span>
            </>
          )}
        </div>
        <div className="auth-card">
          <h1>Fit App</h1>
          <p className="auth-tagline">Plan your training. Track your progress.</p>
          {isRestoringSession ? (
            <p>Checking session…</p>
          ) : (
            <AuthPage onAuthenticated={handleAuthenticated} />
          )}
        </div>
      </div>
    );
  }

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

      <section
        aria-label="auth"
        style={{
          marginTop: '1.5rem',
          padding: '1rem 1.25rem',
          border: '1px solid #ddd',
          borderRadius: 12,
        }}
      >
        <p>
          Logged in as <strong>{currentUser.display_name}</strong> ({currentUser.email})
        </p>
        <button type="button" onClick={handleLogout}>
          Log out
        </button>
      </section>
    </main>
  );
}
