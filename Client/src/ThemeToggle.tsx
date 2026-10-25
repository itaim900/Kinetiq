import { useState } from 'react';
import { applyTheme, currentTheme, type Theme } from './lib/theme';

// ☾ Night / ☀ Light button. The label shows the mode you'd switch TO.
export default function ThemeToggle() {
  const [theme, setTheme] = useState<Theme>(currentTheme());
  const next: Theme = theme === 'dark' ? 'light' : 'dark';
  return (
    <button
      type="button"
      className="theme-toggle"
      aria-label={`Switch to ${next === 'dark' ? 'night' : 'light'} mode`}
      onClick={() => {
        applyTheme(next);
        setTheme(next);
      }}
    >
      {theme === 'dark' ? '☀ Light' : '☾ Night'}
    </button>
  );
}
