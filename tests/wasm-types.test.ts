import { readFileSync } from 'node:fs';
import { expect, test } from 'vitest';

const read = (rel: string) => readFileSync(new URL(`../${rel}`, import.meta.url), 'utf8');

function cppFields(): string[] {
  const body = /struct MeshIntegrity \{([\s\S]*?)\n\};/.exec(read('include/geom-core/Integrity.hpp'))?.[1];
  if (!body) throw new Error('struct MeshIntegrity not found in Integrity.hpp');
  const fields = [...body.matchAll(/^\s+[\w:<>,\s]+?\s(\w+)(?:\s*=[^;]*)?;\s*$/gm)].map((m) => m[1]);
  // summary() is a member function; the bindings expose it as a field.
  return [...fields, 'summary'];
}

function tsFields(): string[] {
  const body = /export interface MeshIntegrity \{([\s\S]*?)\n\}/.exec(read('src/wasm/types.d.ts'))?.[1];
  if (!body) throw new Error('interface MeshIntegrity not found in src/wasm/types.d.ts');
  return [...body.matchAll(/^\s+(\w+):/gm)].map((m) => m[1]);
}

test('the MeshIntegrity TS type mirrors the C++ struct field for field', () => {
  const cpp = cppFields();
  expect(cpp).toContain('bodyCount');
  expect(tsFields()).toEqual(cpp);
});

test('the WASM binding sets every MeshIntegrity field it declares', () => {
  const binding = read('bindings/wasm/WasmEntry.cpp');
  for (const field of tsFields()) expect(binding).toContain(`out.set("${field}"`);
});

test('package exports point at the files scripts/build_wasm.sh produces', () => {
  const pkg = JSON.parse(read('package.json'));
  const script = read('scripts/build_wasm.sh');
  expect(pkg.exports['./wasm'].import).toBe('./dist/wasm/index.js');
  expect(pkg.exports['./wasm-binary']).toBe('./dist/wasm/geom-core.wasm');
  expect(pkg.exports['./threaded'].import).toBe('./dist/wasm/threaded.js');
  expect(pkg.exports['./threaded/wasm-binary']).toBe('./dist/wasm/geom-core-threaded.wasm');
  // Entry points come from src/wasm/; the binaries from the two CMake variants.
  for (const entry of ['index.js', 'index.d.ts', 'threaded.js', 'threaded.d.ts', 'loader.js', 'types.d.ts']) {
    expect(read(`src/wasm/${entry}`).length).toBeGreaterThan(0);
  }
  expect(script).toContain('base="geom-core-threaded"');
  expect(script).toContain('base="geom-core"');
  expect(read('CMakeLists.txt')).toContain('"-s EXPORT_ES6=1"');
  expect(read('CMakeLists.txt')).toContain('"-s USE_PTHREADS=0"');
});
