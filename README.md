# Fit App

An AI-powered web application that helps people create personalized workout plans.

## Overview

Fit App uses an AI agent to generate workout plans tailored to a user's goals,
experience level, available equipment, and schedule.

- **Backend:** C++
- **Frontend:** React
- **AI:** LLM-based agent for personalized plan generation

> This is a learning + portfolio project. Architecture and scope are evolving.

## Project structure

```
.
├── backend/    # C++ API server
├── frontend/   # React web app
└── docs/       # Design docs, architecture notes, decisions
```

## Design docs

The project is being designed before implementation. See [`docs/`](./docs):

- [`01-data-model.md`](./docs/01-data-model.md) — database schema, ER diagram, design principles
- [`02-api-contract.md`](./docs/02-api-contract.md) — REST API contract (contract-first, with test cases)
- [`03-llm-port.md`](./docs/03-llm-port.md) — AI subsystem: LLM port, validation, weight engine
- [`04-architecture.md`](./docs/04-architecture.md) — system architecture (Ports & Adapters), folder layout
- [`05-roadmap.md`](./docs/05-roadmap.md) — phased roadmap, milestones, definitions of done
- [`adr/`](./docs/adr/README.md) — Architecture Decision Records

## Status

Design phase — architecture and contracts defined; implementation not yet started.
