import { useEffect, useState } from 'react';
import { BrowserRouter, Navigate, Route, Routes } from 'react-router';
import {
  fetchHealth,
  fetchMe,
  setUnauthorizedHandler,
  type AuthResult,
  type AuthUser,
  type HealthStatus,
} from './api';
import AppShell from './AppShell';
import ThemeToggle from './ThemeToggle';
import { UnitsProvider } from './lib/UnitsContext';
import AuthPage from './features/auth/AuthPage';
import InjuriesPage from './features/injuries/InjuriesPage';
import ProfilePage from './features/profile/ProfilePage';
import './features/auth/auth-theme.css';
import './app-theme.css';
import { clearToken, getToken, saveToken } from './lib/tokenStorage';

export default function App() {
  const [health, setHealth] = useState<HealthStatus | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [currentUser, setCurrentUser] = useState<AuthUser | null>(null);
  const [isRestoringSession, setIsRestoringSession] = useState(true);
  const [notice, setNotice] = useState<string | null>(null);

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

  // Any authenticated call that gets a 401 (24h token expired) lands here.
  useEffect(() => {
    setUnauthorizedHandler(() => {
      clearToken();
      setCurrentUser(null);
      setNotice('Your session expired — please log in again.');
    });
    return () => setUnauthorizedHandler(null);
  }, []);

  function handleAuthenticated(result: AuthResult) {
    setNotice(null);
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
          <ThemeToggle />
        </div>
        <div className="auth-card">
          <h1>Kinetiq</h1>
          <p className="auth-tagline">Plan your training. Track your progress.</p>
          {notice && (
            <p className="auth-notice" role="status">
              {notice}
            </p>
          )}
          {isRestoringSession ? (
            <p>Checking session…</p>
          ) : (
            <AuthPage onAuthenticated={handleAuthenticated} />
          )}
        </div>
      </div>
    );
  }

  // Logged in: the router decides which page fills the shell's <Outlet/>.
  return (
    <BrowserRouter>
      <UnitsProvider>
        <Routes>
          <Route element={<AppShell user={currentUser} onLogout={handleLogout} />}>
            <Route path="/injuries" element={<InjuriesPage />} />
            <Route path="/profile" element={<ProfilePage />} />
            <Route path="*" element={<Navigate to="/injuries" replace />} />
          </Route>
        </Routes>
      </UnitsProvider>
    </BrowserRouter>
  );
}
