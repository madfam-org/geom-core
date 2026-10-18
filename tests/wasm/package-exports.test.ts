// @vitest-environment node
/**
 * The export map as a real Node consumer resolves it (Vitest's resolver
 * would hide mistakes): every WASM subpath resolves to a built file, and the
 * default entry loads its own .wasm with no options.
 */
import { execFileSync } from 'node:child_process';
import { existsSync, readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { expect, test } from 'vitest';

const root = fileURLToPath(new URL('../..', import.meta.url));
const node = (code: string, type: 'module' | 'commonjs' = 'module') =>
  execFileSync(process.execPath, [`--input-type=${type}`, '-e', code], { cwd: root, encoding: 'utf8' }).trim();

test('WASM subpaths resolve to built files', () => {
  const resolved = JSON.parse(node(`console.log(JSON.stringify(Object.fromEntries(
    ["wasm", "wasm-binary", "threaded", "threaded/wasm-binary"].map((s) => [s, import.meta.resolve("@madfam/geom-core/" + s)]))))`));
  expect(resolved.wasm).toMatch(/\/dist\/wasm\/index\.js$/);
  expect(resolved['wasm-binary']).toMatch(/\/dist\/wasm\/geom-core\.wasm$/);
  expect(resolved.threaded).toMatch(/\/dist\/wasm\/threaded\.js$/);
  expect(resolved['threaded/wasm-binary']).toMatch(/\/dist\/wasm\/geom-core-threaded\.wasm$/);
  for (const url of Object.values(resolved) as string[]) expect(existsSync(fileURLToPath(url))).toBe(true);
  const wasm = readFileSync(fileURLToPath(resolved['wasm-binary']));
  expect(WebAssembly.validate(wasm)).toBe(true);
});

test('require.resolve finds the entry (bundler aliases rely on it)', () => {
  expect(node('console.log(require.resolve("@madfam/geom-core/wasm"))', 'commonjs')).toMatch(/dist\/wasm\/index\.js$/);
});

test('import "@madfam/geom-core/wasm" loads its own .wasm without options', () => {
  const out = node(`
    import createGeomCore from "@madfam/geom-core/wasm";
    const mod = await createGeomCore({ print() {} });
    const a = new mod.Analyzer();
    const b = new Uint8Array(84 + 50);
    new DataView(b.buffer).setUint32(80, 1, true);
    [[0,0,0],[1,0,0],[0,1,0]].flat().forEach((v, k) => new DataView(b.buffer).setFloat32(84 + 12 + 4 * k, v, true));
    a.loadSTLFromBytes(b);
    console.log(a.getTriangleCount(), JSON.stringify(a.getBoundingBox().max), typeof SharedArrayBuffer);
  `);
  expect(out).toBe('1 {"x":1,"y":1,"z":0} function');
});
