# http/

**The inbound HTTP adapter (Crow). Translates HTTP ⇄ use cases.**

- `controllers/` — route handlers (e.g. `AuthController`) that parse requests, call an `app/` service,
  and serialize the result.
- `middleware/` — cross-cutting concerns: JWT auth → `CurrentUser`, error mapping to RFC 9457.
- `dto/` — request/response data transfer objects + (de)serialization.

This layer is a thin translation shell; business logic lives in `app/` and `domain/`.
