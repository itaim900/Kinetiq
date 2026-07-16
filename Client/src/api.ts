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

// Matches the `user` sub-object in AuthDto.h's userToJson -- deliberately
// never has a password/password_hash field.
export interface AuthUser {
  id: number;
  email: string;
  display_name: string;
}

export interface AuthResult {
  user: AuthUser;
  access_token: string;
}

export interface RegisterRequest {
  email: string;
  password: string;
  display_name: string;
}

export interface LoginRequest {
  email: string;
  password: string;
}

// The backend's RFC 9457-shaped error body (see AuthDto.h's errorResponse).
interface ApiErrorBody {
  detail?: string;
}

async function errorMessage(res: Response): Promise<string> {
  try {
    const body = (await res.json()) as ApiErrorBody;
    if (body.detail) return body.detail;
  } catch {
    // Response body wasn't JSON (or was empty) -- fall through.
  }
  return `Request failed: ${res.status}`;
}

async function postJson<TResult>(path: string, payload: unknown): Promise<TResult> {
  const res = await fetch(`${apiBase}${path}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(payload),
  });
  if (!res.ok) {
    throw new Error(await errorMessage(res));
  }
  return (await res.json()) as TResult;
}

export function registerUser(request: RegisterRequest): Promise<AuthResult> {
  return postJson<AuthResult>('/api/v1/auth/register', request);
}

export function loginUser(request: LoginRequest): Promise<AuthResult> {
  return postJson<AuthResult>('/api/v1/auth/login', request);
}

export async function fetchMe(token: string): Promise<AuthUser> {
  const res = await fetch(`${apiBase}/api/v1/auth/me`, {
    headers: { Authorization: `Bearer ${token}` },
  });
  if (!res.ok) {
    throw new Error(await errorMessage(res));
  }
  return (await res.json()) as AuthUser;
}
