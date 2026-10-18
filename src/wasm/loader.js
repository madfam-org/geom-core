// @ts-check
/**
 * Shared logic of the package's WASM entry points (index.js, threaded.js).
 *
 * The Emscripten factories (geom-core.js, geom-core-threaded.js) expose the
 * C++ loaders as _loadSTLFromBytes/_loadSTL, which return false and keep the
 * parser's message in getLastError(). This wrapper installs the public
 * loadSTLFromBytes/loadSTL, which throw GeomCoreSTLError instead, so a bad
 * STL can never be mistaken for an empty mesh. Throwing here, after the C++
 * call has returned, keeps the C++ side free of exceptions (no leaked
 * argument copies on unwind).
 */

export class GeomCoreSTLError extends Error {
  /** @param {string} parserMessage */
  constructor(parserMessage) {
    const reason = parserMessage || 'unknown parser error';
    super(`geom-core could not read the STL: ${reason}`);
    /** @type {'GeomCoreSTLError'} */
    this.name = 'GeomCoreSTLError';
    this.parserMessage = reason;
  }
}

export class GeomCoreThreadingError extends Error {
  /** @param {string} message */
  constructor(message) {
    super(message);
    /** @type {'GeomCoreThreadingError'} */
    this.name = 'GeomCoreThreadingError';
  }
}

/**
 * @typedef {{
 *   _loadSTLFromBytes(data: Uint8Array | string): boolean,
 *   _loadSTL(path: string): boolean,
 *   getLastError(): string,
 *   loadSTLFromBytes?: (data: Uint8Array | string) => true,
 *   loadSTL?: (path: string) => true,
 * }} RawAnalyzer
 */

/**
 * @param {unknown} raw
 * @returns {import('./types').GeomCoreModule}
 */
function wrapModule(raw) {
  const mod = /** @type {{ Analyzer?: { new (): unknown, prototype: RawAnalyzer } }} */ (raw);
  if (!mod || typeof mod.Analyzer !== 'function') {
    throw new Error('geom-core WASM module did not register Analyzer');
  }
  const proto = mod.Analyzer.prototype;
  /** @param {Uint8Array | string} data @returns {true} */
  proto.loadSTLFromBytes = function loadSTLFromBytes(data) {
    if (!this._loadSTLFromBytes(data)) throw new GeomCoreSTLError(this.getLastError());
    return true;
  };
  /** @param {string} path @returns {true} */
  proto.loadSTL = function loadSTL(path) {
    if (!this._loadSTL(path)) throw new GeomCoreSTLError(this.getLastError());
    return true;
  };
  return /** @type {import('./types').GeomCoreModule} */ (/** @type {unknown} */ (mod));
}

/**
 * @param {import('./loader').GeomCoreFactory} factory
 * @returns {(options?: import('./types').GeomCoreOptions) => Promise<import('./types').GeomCoreModule>}
 */
export function createLoader(factory) {
  return async function createGeomCore(options = {}) {
    // Emscripten adopts the options object as its Module; copy it so the
    // caller's object is not mutated and can be reused.
    return wrapModule(await factory({ ...options }));
  };
}

/**
 * @param {typeof globalThis} [realm]
 * @returns {string | null}
 */
export function threadedBlocker(realm = globalThis) {
  if (typeof realm.SharedArrayBuffer !== 'function') {
    return '@madfam/geom-core/threaded needs SharedArrayBuffer, which this environment does not provide; ' +
      'serve the page cross-origin isolated (COOP: same-origin, COEP: require-corp) or use @madfam/geom-core/wasm';
  }
  if (realm.crossOriginIsolated === false) {
    return '@madfam/geom-core/threaded needs a cross-origin-isolated page (COOP: same-origin, COEP: require-corp); ' +
      'this page is not, so use @madfam/geom-core/wasm';
  }
  return null;
}
