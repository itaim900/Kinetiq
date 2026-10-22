import '@testing-library/jest-dom';
import { afterAll, afterEach, beforeAll } from 'vitest';
import { resetMockDb } from '../mocks/handlers';
import { server } from '../mocks/server';

beforeAll(() => server.listen({ onUnhandledRequest: 'error' }));
afterEach(() => {
  server.resetHandlers();
  resetMockDb();
});
afterAll(() => server.close());
