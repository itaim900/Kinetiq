// localStorage, not in-memory-only (2026-07-16 decision): the backend's
// 24h JWT expiry is meant to mean "log in once a day," which only holds if
// the token survives a page refresh -- in-memory storage would force a
// re-login on every refresh, quietly defeating that choice.
const STORAGE_KEY = 'kinetiq_access_token';

export function saveToken(token: string): void {
  localStorage.setItem(STORAGE_KEY, token);
}

export function getToken(): string | null {
  return localStorage.getItem(STORAGE_KEY);
}

export function clearToken(): void {
  localStorage.removeItem(STORAGE_KEY);
}
