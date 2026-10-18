import type { GeomCoreModule, GeomCoreOptions } from './types';

export * from './types';
export { GeomCoreSTLError, GeomCoreThreadingError } from './loader';

/**
 * Instantiate the threaded module. Rejects with GeomCoreThreadingError when
 * SharedArrayBuffer is missing or the page is not cross-origin isolated.
 */
declare function createGeomCoreThreaded(options?: GeomCoreOptions): Promise<GeomCoreModule>;
export default createGeomCoreThreaded;
