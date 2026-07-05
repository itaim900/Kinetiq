-- Migration 0002: core v1 schema.
--
-- Implements the full data model from docs/01-data-model.md up front (Phase 2
-- builds features against these tables slice by slice). Conventions:
--   * BIGINT identity primary keys.
--   * Enum-like columns are TEXT + CHECK (easy to evolve; see design principle 8).
--   * Canonical SI units only (weight_kg, height_cm).
--   * created_at on every table; updated_at only where rows are mutated in place.
--
-- The whole file runs in one transaction (the Migrator wraps it), so it either
-- applies completely or not at all.

-- Keeps updated_at current on UPDATE without the app having to remember.
CREATE OR REPLACE FUNCTION set_updated_at() RETURNS trigger AS $$
BEGIN
    NEW.updated_at = now();
    RETURN NEW;
END;
$$ LANGUAGE plpgsql;

-- ---------------------------------------------------------------------------
-- Users
-- ---------------------------------------------------------------------------
CREATE TABLE users (
    id                     BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    email                  CITEXT NOT NULL UNIQUE,
    password_hash          TEXT   NOT NULL,                 -- argon2id, never plaintext
    display_name           TEXT,
    sex                    TEXT   CHECK (sex IN ('male','female','other','unspecified')),
    birth_date             DATE,                            -- age derived at query time
    height_cm              NUMERIC(5,1),
    weight_kg              NUMERIC(5,2),
    experience_level       TEXT   CHECK (experience_level IN ('beginner','intermediate','advanced')),
    unit_preference        TEXT   NOT NULL DEFAULT 'metric'
                                  CHECK (unit_preference IN ('metric','imperial')),
    weekly_workout_goal    INT    CHECK (weekly_workout_goal IS NULL OR weekly_workout_goal >= 0),
    available_time_minutes INT    CHECK (available_time_minutes IS NULL OR available_time_minutes > 0),
    preferences            JSONB  NOT NULL DEFAULT '{}'::jsonb,
    medications            TEXT,
    created_at             TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at             TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE TRIGGER users_set_updated_at
    BEFORE UPDATE ON users FOR EACH ROW EXECUTE FUNCTION set_updated_at();

-- ---------------------------------------------------------------------------
-- User_Injuries — drives the rule-based safety filter
-- ---------------------------------------------------------------------------
CREATE TABLE user_injuries (
    id          BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id     BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    body_region TEXT   NOT NULL,                            -- matches exercise_contraindications.body_region
    severity    TEXT   CHECK (severity IN ('mild','moderate','severe')),
    active      BOOLEAN NOT NULL DEFAULT true,
    notes       TEXT,
    created_at  TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX idx_user_injuries_user_id ON user_injuries(user_id);

-- ---------------------------------------------------------------------------
-- Muscles + Equipment (lookup entities)
-- ---------------------------------------------------------------------------
CREATE TABLE muscles (
    id           BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name         TEXT NOT NULL UNIQUE,
    muscle_group TEXT NOT NULL
);

CREATE TABLE equipment (
    id       BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name     TEXT   NOT NULL UNIQUE,
    aliases  TEXT[] NOT NULL DEFAULT '{}',                  -- alt names for CV matching (Phase 5)
    category TEXT
);

-- ---------------------------------------------------------------------------
-- Exercises (core catalogue)
-- ---------------------------------------------------------------------------
CREATE TABLE exercises (
    id               BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    name             TEXT NOT NULL UNIQUE,
    instructions     TEXT,
    media_url        TEXT,
    difficulty_level INT  CHECK (difficulty_level BETWEEN 1 AND 5),
    exercise_type    TEXT,                                  -- strength, cardio, mobility, ...
    met_value        NUMERIC(4,2),                          -- for deterministic calorie math
    created_at       TIMESTAMPTZ NOT NULL DEFAULT now()
);

-- ---------------------------------------------------------------------------
-- Exercise ⨯ (Muscles | Equipment | Contraindications | Strength norms)
-- ---------------------------------------------------------------------------
CREATE TABLE exercise_muscles (
    exercise_id BIGINT NOT NULL REFERENCES exercises(id) ON DELETE CASCADE,
    muscle_id   BIGINT NOT NULL REFERENCES muscles(id)   ON DELETE CASCADE,
    role        TEXT   NOT NULL CHECK (role IN ('primary','secondary')),
    PRIMARY KEY (exercise_id, muscle_id)
);

CREATE TABLE exercise_equipment (
    exercise_id  BIGINT NOT NULL REFERENCES exercises(id) ON DELETE CASCADE,
    equipment_id BIGINT NOT NULL REFERENCES equipment(id) ON DELETE CASCADE,
    role         TEXT   NOT NULL CHECK (role IN ('required','assisted')),
    PRIMARY KEY (exercise_id, equipment_id)
);

CREATE TABLE exercise_contraindications (
    exercise_id BIGINT NOT NULL REFERENCES exercises(id) ON DELETE CASCADE,
    body_region TEXT   NOT NULL,
    PRIMARY KEY (exercise_id, body_region)
);
CREATE INDEX idx_exercise_contra_body_region ON exercise_contraindications(body_region);

CREATE TABLE exercise_strength_norms (
    exercise_id           BIGINT NOT NULL REFERENCES exercises(id) ON DELETE CASCADE,
    sex                   TEXT   NOT NULL CHECK (sex IN ('male','female')),
    experience_level      TEXT   NOT NULL CHECK (experience_level IN ('beginner','intermediate','advanced')),
    bodyweight_multiplier NUMERIC(4,2) NOT NULL,
    PRIMARY KEY (exercise_id, sex, experience_level)
);

-- ---------------------------------------------------------------------------
-- Generation_Requests — LLM audit table (one row per LLM call)
-- ---------------------------------------------------------------------------
CREATE TABLE generation_requests (
    id                BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id           BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    model             TEXT   NOT NULL,
    system_prompt     TEXT,
    user_payload      JSONB,
    raw_response      JSONB,
    parsed_ok         BOOLEAN,
    validation_errors JSONB,
    tokens_input      INT,
    tokens_output     INT,
    cost_usd          NUMERIC(10,6),
    latency_ms        INT,
    status            TEXT CHECK (status IN ('pending','succeeded','failed')),
    created_at        TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX idx_generation_requests_user_id ON generation_requests(user_id);

-- ---------------------------------------------------------------------------
-- Workouts + Workout_Exercises
-- ---------------------------------------------------------------------------
CREATE TABLE workouts (
    id                    BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id               BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    generation_request_id BIGINT REFERENCES generation_requests(id) ON DELETE SET NULL,
    name                  TEXT,
    workout_type          TEXT,
    target_environment    TEXT,
    source                TEXT NOT NULL CHECK (source IN ('ai','manual')),
    status                TEXT NOT NULL DEFAULT 'draft'
                               CHECK (status IN ('draft','active','archived')),
    created_at            TIMESTAMPTZ NOT NULL DEFAULT now(),
    updated_at            TIMESTAMPTZ NOT NULL DEFAULT now()
);
CREATE INDEX idx_workouts_user_id ON workouts(user_id);
CREATE TRIGGER workouts_set_updated_at
    BEFORE UPDATE ON workouts FOR EACH ROW EXECUTE FUNCTION set_updated_at();

CREATE TABLE workout_exercises (
    id              BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    workout_id      BIGINT NOT NULL REFERENCES workouts(id)  ON DELETE CASCADE,
    exercise_id     BIGINT NOT NULL REFERENCES exercises(id),
    order_index     INT    NOT NULL,
    sets            INT    CHECK (sets IS NULL OR sets > 0),
    target_reps_min INT    CHECK (target_reps_min IS NULL OR target_reps_min > 0),
    target_reps_max INT    CHECK (target_reps_max IS NULL OR target_reps_max > 0),
    target_weight_kg NUMERIC(6,2),
    weight_source   TEXT   CHECK (weight_source IN ('body_type_estimate','needs_user_input','user_set')),
    rest_seconds    INT    CHECK (rest_seconds IS NULL OR rest_seconds >= 0),
    CONSTRAINT workout_exercises_reps_order CHECK (
        target_reps_min IS NULL OR target_reps_max IS NULL OR target_reps_min <= target_reps_max),
    UNIQUE (workout_id, order_index)
);
CREATE INDEX idx_workout_exercises_workout_id ON workout_exercises(workout_id);
