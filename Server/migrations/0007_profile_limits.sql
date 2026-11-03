-- 0007: profile limits match the domain ProfilePolicy (spec 2026-11-01):
-- at most 21 workouts a week (3 a day x 7), sessions of 1-300 minutes.
-- Replaces the looser inline checks from 0002 (>= 0 and > 0), keeping the
-- names Postgres gave them.
ALTER TABLE users
  DROP CONSTRAINT users_weekly_workout_goal_check,
  ADD  CONSTRAINT users_weekly_workout_goal_check
       CHECK (weekly_workout_goal IS NULL OR weekly_workout_goal BETWEEN 0 AND 21),
  DROP CONSTRAINT users_available_time_minutes_check,
  ADD  CONSTRAINT users_available_time_minutes_check
       CHECK (available_time_minutes IS NULL OR available_time_minutes BETWEEN 1 AND 300);
