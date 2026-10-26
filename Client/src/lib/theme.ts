export type Theme = 'light' | 'dark';
const STORAGE_KEY = 'kinetiq_theme';

// Saved choice wins; otherwise follow the device (prefers-color-scheme).
export function getInitialTheme(): Theme {
  try {
    const saved = localStorage.getItem(STORAGE_KEY);
    if (saved === 'light' || saved === 'dark') return saved;
  } catch {
    // storage unavailable -- fall through
  }
  const prefersDark =
    typeof window.matchMedia === 'function' &&
    window.matchMedia('(prefers-color-scheme: dark)').matches;
  return prefersDark ? 'dark' : 'light';
}

// Sets data-theme on <html> (CSS swaps every colour token) and, by default,
// remembers the choice so it wins over the device setting next time.
export function applyTheme(theme: Theme, persist = true): void {
  document.documentElement.dataset.theme = theme;
  if (persist) {
    try {
      localStorage.setItem(STORAGE_KEY, theme);
    } catch {
      // ignore
    }
  }
}

export function currentTheme(): Theme {
  return document.documentElement.dataset.theme === 'dark' ? 'dark' : 'light';
}
