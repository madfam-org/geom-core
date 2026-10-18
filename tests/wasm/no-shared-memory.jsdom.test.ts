// @vitest-environment jsdom
/**
 * The default entry loads on a page that is NOT cross-origin isolated:
 * SharedArrayBuffer is removed, Worker is a trap, and WebAssembly.Memory is
 * watched for shared allocations. The threaded entry must refuse up front
 * in the same realm instead of hanging in its pthread pool start-up.
 */
import { fileURLToPath } from 'node:url';
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import fixtures from './fixtures.cjs';

type Realm = typeof globalThis & { SharedArrayBuffer?: unknown; Worker?: unknown };
const realm = globalThis as Realm;
const saved = { SharedArrayBuffer: realm.SharedArrayBuffer, Worker: realm.Worker, Memory: WebAssembly.Memory };
const workersStarted: unknown[] = [];
const sharedMemories: unknown[] = [];

beforeAll(() => {
  delete realm.SharedArrayBuffer;
  realm.Worker = class TrapWorker {
    constructor(...args: unknown[]) {
      workersStarted.push(args);
      throw new Error('this page cannot start workers');
    }
  };
  WebAssembly.Memory = new Proxy(saved.Memory, {
    construct(target, args) {
      if ((args[0] as WebAssembly.MemoryDescriptor | undefined)?.shared) sharedMemories.push(args[0]);
      return Reflect.construct(target, args);
    },
  });
});

afterAll(() => {
  realm.SharedArrayBuffer = saved.SharedArrayBuffer;
  realm.Worker = saved.Worker;
  WebAssembly.Memory = saved.Memory;
});

describe('a page without cross-origin isolation', () => {
  it('looks like one: a window, no SharedArrayBuffer, not isolated', () => {
    expect(typeof window).toBe('object');
    expect(typeof realm.SharedArrayBuffer).toBe('undefined');
    expect((globalThis as { crossOriginIsolated?: boolean }).crossOriginIsolated).not.toBe(true);
  });

  it('loads @madfam/geom-core/wasm through locateFile and analyses a mesh', async () => {
    const { default: createGeomCore } = await import('../../dist/wasm/index.js');
    const wasmPath = fileURLToPath(new URL('../../dist/wasm/geom-core.wasm', import.meta.url));
    const located: string[] = [];
    const mod = await createGeomCore({
      print: () => {},
      locateFile: (path: string) => {
        located.push(path);
        return path.endsWith('.wasm') ? wasmPath : path;
      },
    });
    expect(located).toContain('geom-core.wasm');
    const analyzer = new mod.Analyzer();
    expect(analyzer.loadSTLFromBytes(fixtures.cube())).toBe(true);
    expect(analyzer.getVolume()).toBeCloseTo(1000, 9);
    expect(analyzer.getBoundingBox().min).toEqual({ x: -5, y: -5, z: -5 });
    expect(analyzer.getVertices().length).toBe(24);
    analyzer.delete();
    expect(workersStarted).toHaveLength(0);
    expect(sharedMemories).toHaveLength(0);
  });

  it('@madfam/geom-core/threaded rejects with GeomCoreThreadingError, starting no worker', async () => {
    const { default: createThreaded, GeomCoreThreadingError } = await import('../../dist/wasm/threaded.js');
    await expect(createThreaded()).rejects.toBeInstanceOf(GeomCoreThreadingError);
    await expect(createThreaded()).rejects.toThrow(/COOP: same-origin, COEP: require-corp/);
    expect(workersStarted).toHaveLength(0);
    expect(sharedMemories).toHaveLength(0);
  });
});
