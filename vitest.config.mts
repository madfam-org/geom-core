import { configDefaults, defineConfig } from 'vitest/config';

// Default suite (`pnpm test`): runs without a WASM build. The suites that
// load the real WASM live in tests/wasm/ and run via `pnpm test:wasm`
// (vitest.wasm.config.mts) after scripts/build_wasm.sh, in CI's WASM job.
export default defineConfig({
  test: {
    exclude: [...configDefaults.exclude, 'tests/wasm/**'],
  },
});
