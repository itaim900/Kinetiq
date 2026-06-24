import { render, screen } from '@testing-library/react';
import { describe, it, expect } from 'vitest';
import App from './App';

describe('App', () => {
  it('renders backend health returned by the API', async () => {
    render(<App />);

    // The MSW handler returns { status: "ok", db: "connected" }.
    expect(await screen.findByText('connected')).toBeInTheDocument();
    expect(screen.getByText('ok')).toBeInTheDocument();
  });
});
