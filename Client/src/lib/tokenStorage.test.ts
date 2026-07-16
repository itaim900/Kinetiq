import { beforeEach, describe, expect, it } from 'vitest';
import { clearToken, getToken, saveToken } from './tokenStorage';

// localStorage, chosen over in-memory-only (2026-07-16 decision): the 24h
// JWT expiry only means "log in once a day" if the token survives a page
// refresh, so it has to persist somewhere.
describe('tokenStorage', () => {
  beforeEach(() => {
    localStorage.clear();
  });

  it('returns null when no token has been saved', () => {
    expect(getToken()).toBeNull();
  });

  it('round-trips a saved token', () => {
    saveToken('my-jwt-token');
    expect(getToken()).toBe('my-jwt-token');
  });

  it('clears the token', () => {
    saveToken('my-jwt-token');
    clearToken();
    expect(getToken()).toBeNull();
  });
});
