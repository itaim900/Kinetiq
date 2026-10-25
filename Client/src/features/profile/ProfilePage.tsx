import { useEffect, useState } from 'react';
import { getProfile, updateProfile, type Profile } from '../../api';
import { useUnits } from '../../lib/UnitsContext';
import ProfileForm from './ProfileForm';

export default function ProfilePage() {
  const { units } = useUnits();
  const [profile, setProfile] = useState<Profile | null>(null);
  const [error, setError] = useState<string | null>(null);

  useEffect(() => {
    getProfile()
      .then(setProfile)
      .catch((e: unknown) => setError(e instanceof Error ? e.message : String(e)));
  }, []);

  return (
    <section className="page" aria-labelledby="profile-title">
      <h2 id="profile-title">Profile</h2>
      {error && <p role="alert">Couldn't load your profile: {error}</p>}
      {!error && !profile && <p>Loading…</p>}
      {/* key={units}: switching units rebuilds the form, so it re-shows the
          saved values converted (unsaved edits are dropped on a switch). */}
      {profile && <ProfileForm key={units} profile={profile} units={units} onSave={updateProfile} />}
    </section>
  );
}
