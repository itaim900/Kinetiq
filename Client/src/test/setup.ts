import '@testing-library/jest-dom';
import { configure } from '@testing-library/react';
import { afterAll, afterEach, beforeAll } from 'vitest';
import { resetMockDb } from '../mocks/handlers';
import { server } from '../mocks/server';

// findBy*/waitFor give up after 1s by default. Some flows (restore session →
// redirect → page loads 4 requests) can need longer when the whole suite runs
// in parallel on this machine, which made a test flaky. 3s is still quick.
configure({ asyncUtilTimeout: 3000 });

beforeAll(() => server.listen({ onUnhandledRequest: 'error' }));
afterEach(() => {
  server.resetHandlers();
  resetMockDb();
});
afterAll(() => server.close());
