-- Migration 0006: append-only history of profile changes.
--
-- Every changed profile field is recorded (old -> new, when), so a cleared
-- value's last known value is kept for future training plans (ADR-0017).
-- unit_preference is a display setting and isn't tracked. Deleting a user
-- deletes their history (future "delete my data").

CREATE TABLE user_profile_history (
    id         BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    user_id    BIGINT NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    field      TEXT   NOT NULL CHECK (field IN ('sex','birth_date','height_cm','weight_kg',
                      'experience_level','weekly_workout_goal','available_time_minutes','medications')),
    old_value  TEXT,
    new_value  TEXT,
    changed_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX idx_user_profile_history_lookup ON user_profile_history(user_id, field, changed_at);
