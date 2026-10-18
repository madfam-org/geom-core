import { describe, expect, it } from 'vitest';
// The wrapper logic shared by the WASM entry points; it does not load WASM.
import { createLoader, GeomCoreSTLError, GeomCoreThreadingError, threadedBlocker } from '../src/wasm/loader.js';

class FakeAnalyzer {
  next = true;
  error = '';
  _loadSTLFromBytes() { return this.next; }
  _loadSTL() { return this.next; }
  getLastError() { return this.error; }
}

const fakeFactory = async (options?: Record<string, unknown>) => ({ Analyzer: FakeAnalyzer, options });

describe('createLoader', () => {
  it('passes a copy of the options to the factory', async () => {
    const options = { locateFile: (p: string) => `/assets/${p}` };
    const mod = (await createLoader(fakeFactory)(options)) as unknown as { options: Record<string, unknown> };
    expect(mod.options).not.toBe(options);
    expect(mod.options.locateFile).toBe(options.locateFile);
  });

  it('loadSTLFromBytes returns true on success and throws GeomCoreSTLError with the parser message', async () => {
    const mod = await createLoader(fakeFactory)();
    const analyzer = new mod.Analyzer() as unknown as FakeAnalyzer & { loadSTLFromBytes(d: Uint8Array): true; loadSTL(p: string): true };
    expect(analyzer.loadSTLFromBytes(new Uint8Array(84))).toBe(true);
    analyzer.next = false;
    analyzer.error = 'STL buffer too small (< 84 bytes)';
    let thrown: unknown;
    try { analyzer.loadSTLFromBytes(new Uint8Array(10)); } catch (e) { thrown = e; }
    expect(thrown).toBeInstanceOf(GeomCoreSTLError);
    expect(thrown).toBeInstanceOf(Error);
    expect((thrown as GeomCoreSTLError).name).toBe('GeomCoreSTLError');
    expect((thrown as GeomCoreSTLError).parserMessage).toBe('STL buffer too small (< 84 bytes)');
    expect((thrown as Error).message).toBe('geom-core could not read the STL: STL buffer too small (< 84 bytes)');
    expect(() => analyzer.loadSTL('/missing.stl')).toThrow(GeomCoreSTLError);
  });

  it('fails visibly when the module has no Analyzer', async () => {
    await expect(createLoader(async () => ({}))()).rejects.toThrow('did not register Analyzer');
  });
});

describe('threadedBlocker', () => {
  const realm = (extra: Record<string, unknown>) => extra as unknown as typeof globalThis;

  it('allows Node and cross-origin-isolated pages', () => {
    expect(threadedBlocker(realm({ SharedArrayBuffer }))).toBeNull();
    expect(threadedBlocker(realm({ SharedArrayBuffer, crossOriginIsolated: true }))).toBeNull();
  });

  it('refuses without SharedArrayBuffer or cross-origin isolation, naming COOP/COEP', () => {
    expect(threadedBlocker(realm({}))).toMatch(/SharedArrayBuffer.*COOP: same-origin, COEP: require-corp/);
    expect(threadedBlocker(realm({ SharedArrayBuffer, crossOriginIsolated: false }))).toMatch(/cross-origin-isolated/);
    expect(new GeomCoreThreadingError('x').name).toBe('GeomCoreThreadingError');
  });
});
