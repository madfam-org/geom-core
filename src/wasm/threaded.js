// @ts-check
/**
 * @madfam/geom-core/threaded: the opt-in pthread build, for hosts that serve
 * cross-origin-isolated pages (COOP: same-origin, COEP: require-corp). It
 * allocates shared WebAssembly memory and starts a 4-worker pool at load.
 * Where shared memory is unavailable it rejects with GeomCoreThreadingError
 * up front instead of hanging in the pool start-up.
 */
import factory from './geom-core-threaded.js';
import { createLoader, threadedBlocker, GeomCoreThreadingError } from './loader.js';

export { GeomCoreSTLError, GeomCoreThreadingError } from './loader.js';

const load = createLoader(factory);

/** @param {import('./types').GeomCoreOptions} [options] */
export default async function createGeomCoreThreaded(options) {
  const blocker = threadedBlocker();
  if (blocker) throw new GeomCoreThreadingError(blocker);
  return load(options);
}
