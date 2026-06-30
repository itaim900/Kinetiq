# Multi-stage build for the React frontend.
#   Stage 1 (build): Node compiles the app into static files (dist/).
#   Stage 2 (runtime): nginx serves those static files and proxies /api to the
#                      backend container. This is a PRODUCTION build (no dev
#                      server / hot-reload); for day-to-day coding use `npm run dev`.
#
# Context for this Dockerfile is the Client/ folder.

# ---------- build stage ----------
FROM node:20-alpine AS build
WORKDIR /app
COPY package.json package-lock.json ./
RUN npm ci
COPY . .
RUN npm run build

# ---------- runtime stage ----------
FROM nginx:alpine AS runtime
COPY nginx.conf /etc/nginx/conf.d/default.conf
COPY --from=build /app/dist /usr/share/nginx/html
EXPOSE 80
