import { afterEach, beforeEach, describe, expect, it, vi } from 'vitest';
import { applyTheme, currentTheme, getInitialTheme } from './theme';

describe('theme', () => {
  beforeEach(() => localStorage.clear());
  afterEach(() => vi.unstubAllGlobals());

  it('uses the saved choice first', () => {
    localStorage.setItem('kinetiq_theme', 'dark');
    expect(getInitialTheme()).toBe('dark');
  });

  it('falls back to the device preference', () => {
    vi.stubGlobal('matchMedia', (q: string) => ({ matches: q.includes('dark') }));
    expect(getInitialTheme()).toBe('dark');
  });

  it('defaults to light when nothing is known', () => {
    vi.stubGlobal('matchMedia', undefined);
    expect(getInitialTheme()).toBe('light');
  });

  it('applies and remembers a theme', () => {
    applyTheme('dark');
    expect(document.documentElement.dataset.theme).toBe('dark');
    expect(currentTheme()).toBe('dark');
    expect(localStorage.getItem('kinetiq_theme')).toBe('dark');
  });
});
