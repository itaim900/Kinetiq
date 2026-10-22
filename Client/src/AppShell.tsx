import { NavLink, Outlet } from 'react-router';
import type { AuthUser } from './api';

interface AppShellProps {
  user: AuthUser;
  onLogout: () => void;
}

// The logged-in frame: top bar with tabs, and an <Outlet/> where the router
// renders the current page. NavLink adds aria-current="page" (and our
// "active" class) to the tab whose address is the current one.
export default function AppShell({ user, onLogout }: AppShellProps) {
  const tabClass = ({ isActive }: { isActive: boolean }) => (isActive ? 'app-tab active' : 'app-tab');
  return (
    <div className="app">
      <header className="app-bar">
        <span className="app-logo">Kinetiq</span>
        <nav aria-label="Main" className="app-tabs">
          <NavLink to="/injuries" className={tabClass}>
            Injuries
          </NavLink>
          <NavLink to="/profile" className={tabClass}>
            Profile
          </NavLink>
        </nav>
        <span className="app-user">{user.display_name}</span>
        <button type="button" className="app-logout" onClick={onLogout}>
          Log out
        </button>
      </header>
      <main className="app-main">
        <Outlet />
      </main>
    </div>
  );
}
