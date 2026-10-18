import type { GeomCoreModule, GeomCoreOptions } from './types';

/**
 * Thrown by Analyzer.loadSTLFromBytes()/loadSTL() when the input is not a
 * valid binary STL. `parserMessage` is the C++ parser's reason, e.g.
 * "STL buffer too small (< 84 bytes)".
 */
export declare class GeomCoreSTLError extends Error {
  readonly name: 'GeomCoreSTLError';
  readonly parserMessage: string;
  constructor(parserMessage: string);
}

/**
 * Thrown by the threaded entry when the page cannot run shared-memory WASM
 * (no SharedArrayBuffer, or not cross-origin isolated).
 */
export declare class GeomCoreThreadingError extends Error {
  readonly name: 'GeomCoreThreadingError';
  constructor(message: string);
}

/** Raw Emscripten factory signature (dist/wasm/geom-core*.js default export). */
export type GeomCoreFactory = (options?: GeomCoreOptions) => Promise<unknown>;

/** Wrap a raw factory into the package's createGeomCore(). */
export declare function createLoader(
  factory: GeomCoreFactory,
): (options?: GeomCoreOptions) => Promise<GeomCoreModule>;

/** Why this realm cannot run the threaded build, or null when it can. */
export declare function threadedBlocker(realm?: typeof globalThis): string | null;
