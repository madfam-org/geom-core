import { defineConfig } from 'vitest/config';

// `pnpm test:wasm`: the real single-threaded WASM from dist/wasm/ (build it
// first with ./scripts/build_wasm.sh). Each file picks its environment
// (node or jsdom) with a @vitest-environment comment.
export default defineConfig({
  test: {
    include: ['tests/wasm/**/*.test.ts'],
    testTimeout: 60_000,
    hookTimeout: 60_000,
  },
});
