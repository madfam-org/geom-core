// Smoke test of the opt-in threaded entry (@madfam/geom-core/threaded) in
// Node, which provides SharedArrayBuffer and worker_threads the way a
// cross-origin-isolated page does. Run after scripts/build_wasm.sh:
//   node tests/wasm/threaded-smoke.mjs
// A plain script, not Vitest: the pthread pool keeps worker threads alive,
// so the process exits explicitly.
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';

const require = createRequire(import.meta.url);
const fixtures = require('./fixtures.cjs');

const watchdog = setTimeout(() => {
  console.error('threaded smoke: module did not initialise within 60 s');
  process.exit(1);
}, 60_000);

const sharedMemories = [];
const NativeMemory = WebAssembly.Memory;
WebAssembly.Memory = new Proxy(NativeMemory, {
  construct(target, args) {
    if (args[0]?.shared) sharedMemories.push(args[0]);
    return Reflect.construct(target, args);
  },
});

try {
  const { default: createThreaded, GeomCoreSTLError } = await import('../../dist/wasm/threaded.js');
  const mod = await createThreaded({ print() {} });
  assert.ok(sharedMemories.length >= 1, 'the threaded build allocates shared WebAssembly memory');

  const analyzer = new mod.Analyzer();
  assert.equal(analyzer.loadSTLFromBytes(fixtures.cube()), true);
  assert.ok(Math.abs(analyzer.getVolume() - 1000) < 1e-9);
  assert.deepEqual(analyzer.getBoundingBox().min, { x: -5, y: -5, z: -5 });
  assert.deepEqual(analyzer.getBoundingBox().max, { x: 5, y: 5, z: 5 });
  assert.equal(analyzer.getIndices().length, 36);
  assert.equal(analyzer.assessIntegrity(0).watertight, true);
  assert.throws(() => analyzer.loadSTLFromBytes(new Uint8Array(10)),
    (e) => e instanceof GeomCoreSTLError && e.parserMessage === 'STL buffer too small (< 84 bytes)');
  analyzer.delete();
  clearTimeout(watchdog);
  console.log('threaded smoke: shared memory, cube volume/bounds/indices/integrity and the throwing loader passed');
  process.exit(0);
} catch (error) {
  console.error(error);
  process.exit(1);
}
