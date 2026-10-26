# Backlog — future ideas (later, not never)

One list of everything deliberately deferred, so nothing gets lost in old specs or chats.
Pull from here when planning a slice; turn an item into a GitHub issue when it becomes real work.
Each item notes **why it waits** where that isn't obvious.

## Product & business (owner)
- **Subscription:** $5.99/month, **refunded after 10 verified workouts** in the period. Needs
  payments (e.g. Stripe), automatic refunds, and a **workout verification system** that tracks real
  gym progress — depends on Slice 4 (workouts) existing first. Large; plan separately.
- **User agreement / privacy policy:** must disclose that cleared profile values are kept in
  `user_profile_history` and may still be used for training plans (ADR-0017), plus the
  not-medical-advice position of the conditions catalog.

## Accounts & auth
- **Forgot password** (deferred 2026-10-25 as large): email provider, reset tokens with expiry
  (single use), two screens, no hint whether an email exists. The login screen already shows a
  placeholder link.
- **"Delete my data" / account deletion** — real erasure (account, injuries, profile history;
  cascades already in place). Needs re-authentication.
- Google OAuth sign-in · email verification · login rate-limiting
- Edit display name / email (account data, not profile)
- Session hardening: refresh tokens / httpOnly cookies instead of a 24h token in `localStorage`

## Profile
- `preferences` (jsonb) — deferred since Slice 2
- Reading the profile history back: "last known value" lookup (Phase 3 training engine), a
  weight-over-time chart
- Keep unsaved edits when switching units mid-edit (today the form rebuilds and drops them)

## Injuries & catalog
- **Physio review** of the conditions catalog — secondary muscles are project judgement
- Re-check the ICD-11 codes in the WHO browser; find codes for patellar tendinopathy, shin splints,
  Achilles tendinopathy (left NULL)
- Several conditions on one injury · left/right side · Hebrew names
- Users suggesting new catalog conditions · a muscle search box if the muscle list grows

## Frontend
- **Upgrade the dev toolchain: Vite 5 → 8, Vitest 2 → 5** — clears the 6 remaining dev-only
  `npm audit` advisories (incl. esbuild letting websites read the dev server). Until then, don't
  browse untrusted sites while `npm run dev` is running. Production deps: 0 advisories.
- Add a favicon (only console error in the Stage C browser check: `favicon.ico` 404)
- Polish: "Choose a different injury" link in the injury form sits too close to the pills and
  doesn't look like a link
- TanStack Query (caching / refetching) when screens multiply · offline support

## Ops & dev experience
- Delete the old Docker volumes kept as a backup after the Compose fix (`fitapp_kinetiq_pgdata`,
  `fitapp_fitapp_pgdata`, `kinetiq_kinetiq_pgdata`) once confident in `kinetiq_pgdata`
- Profile save + history write in one transaction (unit of work) — ADR-0015 trade-off
- Other clones of the repo (Cowork, other machines): `git remote set-url origin
  https://github.com/itaim900/Kinetiq.git`

## Owner's ideas
- (add yours here)
