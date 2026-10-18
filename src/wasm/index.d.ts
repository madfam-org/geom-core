import type { GeomCoreModule, GeomCoreOptions } from './types';

export * from './types';
export { GeomCoreSTLError } from './loader';

/** Instantiate the single-threaded module (no SharedArrayBuffer, no workers). */
declare function createGeomCore(options?: GeomCoreOptions): Promise<GeomCoreModule>;
export default createGeomCore;
