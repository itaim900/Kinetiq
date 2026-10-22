import { getToken } from './lib/tokenStorage';

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
  errors?: Record<string, string[]>;
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

// ---------------------------------------------------------------------------
// Authenticated API (Stage C). Types mirror the server's JSON exactly.

export type Severity = 'mild' | 'moderate' | 'severe';
export type InjuryType = 'muscle' | 'bone' | 'tendon_ligament' | 'joint' | 'other';

export interface BodyRegion {
  slug: string;
  name: string;
}

export interface Muscle {
  id: number;
  name: string;
  muscle_group: string;
}

export interface InjuryMuscle {
  id: number;
  name: string;
  role: 'primary' | 'secondary';
}

export interface InjuryCondition {
  id: number;
  slug: string;
  name: string;
  icd11_code: string | null;
  body_region: string;
  injury_type: InjuryType;
  muscles: InjuryMuscle[];
}

export interface ConditionRef {
  id: number;
  name: string;
  icd11_code: string | null;
}

export interface Injury {
  id: number;
  user_id: number;
  condition: ConditionRef | null; // null = unlisted injury
  body_region: string;
  injury_type: InjuryType;
  severity: Severity;
  muscles: InjuryMuscle[];
  active: boolean;
  notes: string | null;
  created_at: string;
}

// The two request shapes the server accepts (known condition vs unlisted).
export type AddInjuryRequest =
  | { condition_id: number; severity: Severity; notes?: string }
  | {
      body_region: string;
      injury_type: InjuryType;
      muscle_ids: number[];
      severity: Severity;
      notes?: string;
    };

export interface UpdateInjuryRequest {
  body_region?: string;
  injury_type?: InjuryType;
  muscle_ids?: number[];
  severity?: Severity;
  notes?: string;
  active?: boolean;
}

export interface Profile {
  id: number;
  email: string;
  display_name: string;
  sex: string | null;
  birth_date: string | null;
  height_cm: number | null;
  weight_kg: number | null;
  experience_level: string | null;
  unit_preference: 'metric' | 'imperial';
  weekly_workout_goal: number | null;
  available_time_minutes: number | null;
  medications: string | null;
}

export type ProfileUpdate = Partial<Omit<Profile, 'id' | 'email' | 'display_name'>>;

// A failed request: HTTP status, the server's `detail`, and its per-field
// `errors` (RFC 9457 body from the C++ server) so forms can show each message
// under the right input.
export class ApiError extends Error {
  readonly status: number;
  readonly fieldErrors: Record<string, string[]>;

  constructor(status: number, message: string, fieldErrors: Record<string, string[]> = {}) {
    super(message);
    this.name = 'ApiError';
    this.status = status;
    this.fieldErrors = fieldErrors;
  }
}

// App registers this so ANY authenticated call that gets a 401 (expired
// 24h token) logs the user out in one place.
let unauthorizedHandler: (() => void) | null = null;

export function setUnauthorizedHandler(handler: (() => void) | null): void {
  unauthorizedHandler = handler;
}

async function toApiError(res: Response): Promise<ApiError> {
  let body: ApiErrorBody = {};
  try {
    body = (await res.json()) as ApiErrorBody;
  } catch {
    // Not JSON (or empty) -- keep the defaults.
  }
  return new ApiError(res.status, body.detail ?? `Request failed: ${res.status}`, body.errors ?? {});
}

export async function authFetch<T>(
  path: string,
  options: { method?: string; body?: unknown } = {},
): Promise<T> {
  const token = getToken();
  const headers: Record<string, string> = {};
  if (token) headers.Authorization = `Bearer ${token}`;
  if (options.body !== undefined) headers['Content-Type'] = 'application/json';

  const res = await fetch(`${apiBase}${path}`, {
    method: options.method ?? 'GET',
    headers,
    body: options.body !== undefined ? JSON.stringify(options.body) : undefined,
  });
  if (res.status === 401) unauthorizedHandler?.();
  if (!res.ok) throw await toApiError(res);
  if (res.status === 204) return undefined as T;
  return (await res.json()) as T;
}

export const getProfile = () => authFetch<Profile>('/api/v1/users/me');
export const updateProfile = (update: ProfileUpdate) =>
  authFetch<Profile>('/api/v1/users/me', { method: 'PATCH', body: update });

export const listInjuries = () => authFetch<Injury[]>('/api/v1/users/me/injuries');
export const addInjury = (request: AddInjuryRequest) =>
  authFetch<Injury>('/api/v1/users/me/injuries', { method: 'POST', body: request });
export const updateInjury = (id: number, request: UpdateInjuryRequest) =>
  authFetch<Injury>(`/api/v1/users/me/injuries/${id}`, { method: 'PATCH', body: request });
export const deleteInjury = (id: number) =>
  authFetch<void>(`/api/v1/users/me/injuries/${id}`, { method: 'DELETE' });

export const listBodyRegions = () => authFetch<BodyRegion[]>('/api/v1/body-regions');
export const listMuscles = () => authFetch<Muscle[]>('/api/v1/muscles');
export const listConditions = () => authFetch<InjuryCondition[]>('/api/v1/injury-conditions');
