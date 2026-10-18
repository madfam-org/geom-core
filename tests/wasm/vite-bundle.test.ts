// @vitest-environment node
/**
 * A Vite app build that consumes the package the way the Yantra4D Studio
 * does: the default entry plus `@madfam/geom-core/wasm-binary?url`, with no
 * alias. The bundle must emit the package's own .wasm as an asset and must
 * not pull in the threaded build or start workers.
 */
import { mkdirSync, mkdtempSync, readdirSync, readFileSync, rmSync, symlinkSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import { build } from 'vite';
import { afterAll, expect, test } from 'vitest';

const root = fileURLToPath(new URL('../..', import.meta.url));
const app = mkdtempSync(join(tmpdir(), 'geom-core-vite-'));
afterAll(() => rmSync(app, { recursive: true, force: true }));

test('Vite bundles the single-threaded entry and emits its .wasm via ?url', async () => {
  mkdirSync(join(app, 'node_modules', '@madfam'), { recursive: true });
  symlinkSync(root, join(app, 'node_modules', '@madfam', 'geom-core'), 'dir');
  writeFileSync(join(app, 'index.html'), '<!doctype html><script type="module" src="/main.js"></script>\n');
  writeFileSync(join(app, 'main.js'), [
    "import createGeomCore from '@madfam/geom-core/wasm';",
    "import wasmUrl from '@madfam/geom-core/wasm-binary?url';",
    'export const load = () => createGeomCore({ locateFile: (p) => (p.endsWith(".wasm") ? wasmUrl : p) });',
    'window.geomCoreLoad = load;',
    '',
  ].join('\n'));

  await build({
    root: app,
    configFile: false,
    logLevel: 'error',
    build: { outDir: join(app, 'out'), assetsInlineLimit: 0, emptyOutDir: true },
  });

  const assets = readdirSync(join(app, 'out', 'assets'));
  const wasmAssets = assets.filter((f) => f.endsWith('.wasm'));
  const ownWasm = readFileSync(join(root, 'dist', 'wasm', 'geom-core.wasm'));
  expect(wasmAssets.length).toBeGreaterThanOrEqual(1);
  for (const f of wasmAssets) {
    expect(f.startsWith('geom-core-threaded')).toBe(false);
    expect(readFileSync(join(app, 'out', 'assets', f)).equals(ownWasm)).toBe(true);
  }
  const js = assets.filter((f) => f.endsWith('.js')).map((f) => readFileSync(join(app, 'out', 'assets', f), 'utf8')).join('\n');
  expect(js).not.toMatch(/new Worker\(/);
  expect(js).toContain(wasmAssets[0]);
});
