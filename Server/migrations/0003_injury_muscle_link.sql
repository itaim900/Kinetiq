-- Migration 0003: injury/muscle link.
--
-- Adds injury_type (required) + muscle_id (nullable, valid only for
-- injury_type = 'muscle') to user_injuries, and seeds a common-injury subset
-- of the Muscles catalog. body_region and the Phase 3 safety filter mechanism
-- are unaffected -- this is additive structure. See
-- docs/superpowers/specs/2026-07-25-injury-muscle-link-design.md.

ALTER TABLE user_injuries
    ADD COLUMN injury_type TEXT NOT NULL DEFAULT 'other'
        CHECK (injury_type IN ('muscle','bone','tendon_ligament','joint','other')),
    ADD COLUMN muscle_id BIGINT REFERENCES muscles(id);

-- Enforce the pairing: a muscle link only makes sense for muscle-type injuries.
ALTER TABLE user_injuries
    ADD CONSTRAINT chk_muscle_id_requires_muscle_type
    CHECK (muscle_id IS NULL OR injury_type = 'muscle');

CREATE INDEX idx_user_injuries_muscle_id ON user_injuries(muscle_id);

-- Seed a common-injury subset. Names match 01-data-model.md's documented
-- Phase 2 seed list, so Slice 3's full catalog seed can safely re-run
-- against these (ON CONFLICT DO NOTHING).
INSERT INTO muscles (name, muscle_group) VALUES
    ('Pectorals', 'chest'), ('Anterior Deltoid', 'shoulders'),
    ('Lateral Deltoid', 'shoulders'), ('Posterior Deltoid', 'shoulders'),
    ('Biceps', 'arms'), ('Triceps', 'arms'), ('Lats', 'back'), ('Traps', 'back'),
    ('Rhomboids', 'back'), ('Erector Spinae', 'back'), ('Rectus Abdominis', 'core'),
    ('Obliques', 'core'), ('Glutes', 'legs'), ('Quads', 'legs'),
    ('Hamstrings', 'legs'), ('Calves', 'legs')
ON CONFLICT (name) DO NOTHING;
