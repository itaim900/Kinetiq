-- Migration 0004: injury conditions catalog (reference data).
--
-- Fixed body regions (shared by injuries and exercise contraindications so the
-- safety filter matches), 12 more muscles (28 total) and 17 known injury
-- conditions mapped to the muscles they affect. Medical sources and review
-- notes: docs/superpowers/specs/2026-10-21-injury-conditions-catalog-design.md.
-- Not medical advice; secondary muscles are project judgement pending physio review.

CREATE TABLE body_regions (
    slug TEXT PRIMARY KEY,
    name TEXT NOT NULL UNIQUE
);

INSERT INTO body_regions (slug, name) VALUES
    ('neck', 'Neck'), ('shoulder', 'Shoulder'), ('upper_back', 'Upper back'),
    ('lower_back', 'Lower back'), ('chest', 'Chest'), ('abdomen', 'Abdomen'),
    ('elbow', 'Elbow'), ('wrist_hand', 'Wrist & hand'), ('hip', 'Hip'), ('groin', 'Groin'),
    ('thigh', 'Thigh'), ('knee', 'Knee'), ('lower_leg', 'Lower leg'),
    ('ankle_foot', 'Ankle & foot');

-- Normalise existing free-text regions, then remove rows that still don't
-- match a region (dev/test data only; no real users exist yet).
UPDATE user_injuries SET body_region = lower(replace(trim(body_region), ' ', '_'));
DELETE FROM user_injuries WHERE body_region NOT IN (SELECT slug FROM body_regions);
ALTER TABLE user_injuries
    ADD CONSTRAINT fk_user_injuries_body_region
    FOREIGN KEY (body_region) REFERENCES body_regions(slug);

DELETE FROM exercise_contraindications WHERE body_region NOT IN (SELECT slug FROM body_regions);
ALTER TABLE exercise_contraindications
    ADD CONSTRAINT fk_exercise_contraindications_body_region
    FOREIGN KEY (body_region) REFERENCES body_regions(slug);

INSERT INTO muscles (name, muscle_group) VALUES
    ('Supraspinatus', 'shoulders'), ('Infraspinatus', 'shoulders'),
    ('Teres Minor', 'shoulders'), ('Subscapularis', 'shoulders'),
    ('Forearm Flexors', 'arms'), ('Forearm Extensors', 'arms'),
    ('Tibialis Anterior', 'legs'), ('Adductors', 'legs'), ('Hip Flexors', 'legs'),
    ('Gluteus Medius', 'legs'), ('Levator Scapulae', 'neck'), ('Sternocleidomastoid', 'neck')
ON CONFLICT (name) DO NOTHING;

CREATE TABLE injury_conditions (
    id          BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
    slug        TEXT NOT NULL UNIQUE,
    name        TEXT NOT NULL UNIQUE,
    icd11_code  TEXT,                                   -- WHO ICD-11 MMS; NULL when none found
    body_region TEXT NOT NULL REFERENCES body_regions(slug),
    injury_type TEXT NOT NULL
        CHECK (injury_type IN ('muscle','bone','tendon_ligament','joint','other'))
);

CREATE TABLE injury_condition_muscles (
    condition_id BIGINT NOT NULL REFERENCES injury_conditions(id) ON DELETE CASCADE,
    muscle_id    BIGINT NOT NULL REFERENCES muscles(id),
    role         TEXT   NOT NULL CHECK (role IN ('primary','secondary')),
    PRIMARY KEY (condition_id, muscle_id)
);

INSERT INTO injury_conditions (slug, name, icd11_code, body_region, injury_type) VALUES
    ('acl_tear',              'ACL tear',              'NC93.62', 'knee',       'tendon_ligament'),
    ('meniscus_tear',         'Meniscus tear',         'NC93.3',  'knee',       'joint'),
    ('patellar_tendinopathy', 'Patellar tendinopathy', NULL,      'knee',       'tendon_ligament'),
    ('hamstring_strain',      'Hamstring strain',      'NC76.30', 'thigh',      'muscle'),
    ('quadriceps_strain',     'Quadriceps strain',     'NC76.10', 'thigh',      'muscle'),
    ('groin_strain',          'Groin strain',          'NC76.20', 'groin',      'muscle'),
    ('hip_flexor_strain',     'Hip flexor strain',     'NC76.00', 'hip',        'muscle'),
    ('calf_strain',           'Calf strain',           'NC96.10', 'lower_leg',  'muscle'),
    ('shin_splints',          'Shin splints',          NULL,      'lower_leg',  'bone'),
    ('achilles_tendinopathy', 'Achilles tendinopathy', NULL,      'ankle_foot', 'tendon_ligament'),
    ('ankle_sprain',          'Ankle sprain',          'ND14.7',  'ankle_foot', 'tendon_ligament'),
    ('lower_back_strain',     'Lower back strain',     'NB53.5',  'lower_back', 'muscle'),
    ('rotator_cuff_strain',   'Rotator cuff strain',   'NC16.00', 'shoulder',   'tendon_ligament'),
    ('tennis_elbow',          'Tennis elbow',          'FB55.1',  'elbow',      'tendon_ligament'),
    ('golfers_elbow',         'Golfer''s elbow',       'FB55.0',  'elbow',      'tendon_ligament'),
    ('neck_strain',           'Neck strain',           'NA23.4',  'neck',       'muscle'),
    ('pectoral_strain',       'Pectoral strain',       'NB34.0',  'chest',      'muscle');

-- Muscles are looked up by name: a typo silently drops a row, which the test
-- suite catches by pinning the total (34).
INSERT INTO injury_condition_muscles (condition_id, muscle_id, role)
SELECT c.id, m.id, v.role
FROM (VALUES
    ('acl_tear', 'Quads', 'secondary'), ('acl_tear', 'Hamstrings', 'secondary'),
    ('acl_tear', 'Calves', 'secondary'),
    ('meniscus_tear', 'Quads', 'secondary'), ('meniscus_tear', 'Hamstrings', 'secondary'),
    ('patellar_tendinopathy', 'Quads', 'secondary'),
    ('hamstring_strain', 'Hamstrings', 'primary'), ('hamstring_strain', 'Glutes', 'secondary'),
    ('quadriceps_strain', 'Quads', 'primary'), ('quadriceps_strain', 'Hip Flexors', 'secondary'),
    ('groin_strain', 'Adductors', 'primary'),
    ('hip_flexor_strain', 'Hip Flexors', 'primary'), ('hip_flexor_strain', 'Quads', 'secondary'),
    ('calf_strain', 'Calves', 'primary'),
    ('shin_splints', 'Tibialis Anterior', 'secondary'), ('shin_splints', 'Calves', 'secondary'),
    ('achilles_tendinopathy', 'Calves', 'secondary'),
    ('ankle_sprain', 'Calves', 'secondary'), ('ankle_sprain', 'Tibialis Anterior', 'secondary'),
    ('lower_back_strain', 'Erector Spinae', 'primary'),
    ('lower_back_strain', 'Glutes', 'secondary'), ('lower_back_strain', 'Obliques', 'secondary'),
    ('rotator_cuff_strain', 'Supraspinatus', 'primary'),
    ('rotator_cuff_strain', 'Infraspinatus', 'secondary'),
    ('rotator_cuff_strain', 'Teres Minor', 'secondary'),
    ('rotator_cuff_strain', 'Subscapularis', 'secondary'),
    ('rotator_cuff_strain', 'Lateral Deltoid', 'secondary'),
    ('tennis_elbow', 'Forearm Extensors', 'primary'),
    ('golfers_elbow', 'Forearm Flexors', 'primary'),
    ('neck_strain', 'Levator Scapulae', 'primary'),
    ('neck_strain', 'Sternocleidomastoid', 'primary'), ('neck_strain', 'Traps', 'secondary'),
    ('pectoral_strain', 'Pectorals', 'primary'), ('pectoral_strain', 'Anterior Deltoid', 'secondary')
) AS v(slug, muscle, role)
JOIN injury_conditions c ON c.slug = v.slug
JOIN muscles m ON m.name = v.muscle;
