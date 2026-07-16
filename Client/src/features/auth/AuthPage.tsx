import { useState } from 'react';
import LoginForm from './LoginForm';
import RegisterForm from './RegisterForm';
import type { AuthResult } from '../../api';

interface AuthPageProps {
  onAuthenticated: (result: AuthResult) => void;
}

// "Forgot password" has no backend yet -- no email infra exists (deferred
// 2026-07-12, see docs/02-api-contract.md). Locked decision (2026-07-16):
// visual-only placeholder rather than a dead-end real form, so it doesn't
// look broken.
export default function AuthPage({ onAuthenticated }: AuthPageProps) {
  const [mode, setMode] = useState<'login' | 'register'>('login');
  const [showForgotPasswordNotice, setShowForgotPasswordNotice] = useState(false);

  return (
    <div>
      {mode === 'login' ? (
        <>
          <LoginForm onSuccess={onAuthenticated} />
          <div className="auth-links">
            <p>
              <button type="button" onClick={() => setShowForgotPasswordNotice(true)}>
                Forgot password?
              </button>
            </p>
            {showForgotPasswordNotice && (
              <p className="auth-notice">Password reset is coming soon.</p>
            )}
            <p>
              Don&apos;t have an account?{' '}
              <button type="button" onClick={() => setMode('register')}>
                Create one
              </button>
            </p>
          </div>
        </>
      ) : (
        <>
          <RegisterForm onSuccess={onAuthenticated} />
          <div className="auth-links">
            <p>
              Already have an account?{' '}
              <button type="button" onClick={() => setMode('login')}>
                Log in instead
              </button>
            </p>
          </div>
        </>
      )}
    </div>
  );
}
