# ADR-0010: Frontend stack is React + TypeScript + Vite

**Status:** Accepted · **Date:** 2026-06-24

## Context

The author is new to React and wants to learn modern, industry-standard frontend practices while
building a clean, maintainable SPA that consumes the REST API.

## Options considered

- **React + TypeScript + Vite** with a focused ecosystem.
- **Plain React + JavaScript** — less to learn initially, but misses the value of typed contracts.
- **Other frameworks (Vue/Svelte/Angular)** — fine, but React has the largest learning/portfolio value.

## Decision

Use **React + TypeScript + Vite**, with:

| Concern | Choice | Why |
|---|---|---|
| Build/dev server | **Vite** | Fast, modern, simple |
| Language | **TypeScript** | Typed API contracts; fewer runtime bugs; resume value |
| Routing | **React Router** | Standard SPA routing |
| Server state | **TanStack Query** | Caching, loading/error states for API data |
| Forms + validation | **React Hook Form + Zod** | Ergonomic forms; Zod schemas mirror the API |
| UI | **Tailwind CSS + shadcn/ui** | Fast, modern, accessible components |
| Charts | **Recharts** | Progress/volume dashboards |
| Testing | **Vitest + Testing Library + MSW** | Unit/component tests; MSW mocks the API contract |

## Consequences

- **Positive:** modern, highly employable stack; TypeScript + Zod keep the frontend honest against
  the API contract; MSW enables frontend development against a mocked backend.
- **Negative:** several libraries to learn at once for a React newcomer.
- **Mitigation:** introduce libraries phase-by-phase rather than all at once.
