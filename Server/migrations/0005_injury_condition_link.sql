-- Migration 0005: link injuries to the conditions catalog; several muscles per injury.
--
-- Replaces 0003's single muscle_id (decision E2, superseded 2026-10-21). Known
-- injuries reference a condition and read its muscles from the catalog;
-- unknown injuries keep the user's own picks in user_injury_muscles.

ALTER TABLE user_injuries DROP CONSTRAINT chk_muscle_id_requires_muscle_type;
DROP INDEX idx_user_injuries_muscle_id;
ALTER TABLE user_injuries DROP COLUMN muscle_id;

ALTER TABLE user_injuries ADD COLUMN condition_id BIGINT REFERENCES injury_conditions(id);
CREATE INDEX idx_user_injuries_condition_id ON user_injuries(condition_id);

-- Only for unknown injuries (condition_id IS NULL) -- enforced by InjuryService,
-- since a CHECK can't look at another table.
CREATE TABLE user_injury_muscles (
    injury_id BIGINT NOT NULL REFERENCES user_injuries(id) ON DELETE CASCADE,
    muscle_id BIGINT NOT NULL REFERENCES muscles(id),
    PRIMARY KEY (injury_id, muscle_id)
);
