import { fireEvent, render, screen } from '@testing-library/react';
import { beforeEach, expect, it } from 'vitest';
import ThemeToggle from './ThemeToggle';
import { applyTheme } from './lib/theme';

beforeEach(() => {
  localStorage.clear();
  applyTheme('light', false);
});

it('switches between light and night and says which mode it offers', () => {
  render(<ThemeToggle />);
  fireEvent.click(screen.getByRole('button', { name: /switch to night mode/i }));
  expect(document.documentElement.dataset.theme).toBe('dark');
  expect(screen.getByRole('button', { name: /switch to light mode/i })).toHaveTextContent('☀ Light');
  expect(localStorage.getItem('kinetiq_theme')).toBe('dark');
});
