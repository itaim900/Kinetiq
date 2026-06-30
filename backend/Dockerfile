# Multi-stage build for the C++ backend.
#   Stage 1 (build): full toolchain + builds libpqxx from source + our server.
#   Stage 2 (runtime): tiny image with just the binary and the few shared libs
#                      it needs. This is what actually ships/runs.
#
# Context for this Dockerfile is the REPO ROOT (so it can see backend/).

# ---------- build stage ----------
FROM ubuntu:24.04 AS build

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        cmake ninja-build g++ git ca-certificates \
        libasio-dev libpq-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY backend/ ./backend/

# FITAPP_FETCH_LIBPQXX=ON builds libpqxx from source (consistent ABI on Linux).
# Tests are skipped in the image (no GoogleTest dependency, smaller/faster build).
RUN cmake -S backend -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DFITAPP_FETCH_LIBPQXX=ON \
        -DFITAPP_BUILD_TESTS=OFF \
    && cmake --build build --target fitapp_server

# ---------- runtime stage ----------
FROM ubuntu:24.04 AS runtime

# libpqxx is linked statically; at runtime we only need the C client (libpq5)
# and the C++ standard library.
RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        libpq5 libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=build /src/build/fitapp_server /usr/local/bin/fitapp_server

# Defaults assume the compose network (Postgres reachable as host "postgres").
# BIND_ADDR=0.0.0.0 so the frontend container can reach the server.
ENV BIND_ADDR=0.0.0.0 \
    DB_HOST=postgres \
    DB_PORT=5432 \
    DB_NAME=fitapp \
    DB_USER=fitapp \
    DB_PASSWORD=fitapp

EXPOSE 8080
CMD ["/usr/local/bin/fitapp_server"]
