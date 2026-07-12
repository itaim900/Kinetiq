# Multi-stage build for the C++ backend.
#   Stage 1 (build): full toolchain + builds libpqxx from source + our server.
#   Stage 2 (runtime): tiny image with just the binary and the few shared libs
#                      it needs. This is what actually ships/runs.
#
# Context for this Dockerfile is the REPO ROOT (so it can see Server/).

# ---------- build stage ----------
FROM ubuntu:24.04 AS build

# libargon2-dev + libssl-dev: Slice 1 (#17) auth deps (argon2 hashing +
# jwt-cpp's OpenSSL-backed signing), now linked into fitapp_server itself.
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        cmake ninja-build g++ git ca-certificates \
        libasio-dev libpq-dev libargon2-dev libssl-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY Server/ ./Server/

# FITAPP_FETCH_LIBPQXX=ON builds libpqxx from source (consistent ABI on Linux).
# Tests are skipped in the image (no GoogleTest dependency, smaller/faster build).
RUN cmake -S Server -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DFITAPP_FETCH_LIBPQXX=ON \
        -DFITAPP_BUILD_TESTS=OFF \
    && cmake --build build --target fitapp_server

# ---------- runtime stage ----------
FROM ubuntu:24.04 AS runtime

# libpqxx is linked statically; at runtime we only need the C client (libpq5)
# and the C++ standard library. libargon2-1 + libssl3: Slice 1 (#17) auth
# deps' shared runtime libs (jwt-cpp itself is header-only, nothing to ship
# for it beyond the OpenSSL lib it already needs).
# NOTE: package names not verified by a local build (this project never
# builds Docker images locally, per the 8GB RAM constraint in CONTEXT.md --
# same situation as the libpqxx/FITAPP_FETCH_LIBPQXX issue). CI is the real
# check; if `apt-get install` fails here, these are the first names to fix
# (Ubuntu sometimes renames runtime SSL packages, e.g. libssl3 -> libssl3t64).
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        libpq5 libstdc++6 libargon2-1 libssl3 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/build/fitapp_server /usr/local/bin/fitapp_server

# Ship the SQL migrations; the server applies pending ones on startup (the app
# owns the schema now, not Postgres's docker-entrypoint-initdb.d).
WORKDIR /app
COPY Server/migrations ./migrations

# Defaults assume the compose network (Postgres reachable as host "postgres").
# BIND_ADDR=0.0.0.0 so the frontend container can reach the server.
# MIGRATIONS_DIR points at the SQL copied above.
# NOTE: no DB_PASSWORD here on purpose — secrets must not be baked into images;
# it is supplied at runtime by docker-compose.prod.yml.
ENV BIND_ADDR=0.0.0.0 \
    DB_HOST=postgres \
    DB_PORT=5432 \
    DB_NAME=fitapp \
    DB_USER=fitapp \
    MIGRATIONS_DIR=/app/migrations

EXPOSE 8080
CMD ["/usr/local/bin/fitapp_server"]
