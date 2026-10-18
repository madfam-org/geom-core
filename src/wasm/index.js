// @ts-check
/**
 * @madfam/geom-core/wasm: the default, single-threaded WASM build.
 * No SharedArrayBuffer, no workers, no COOP/COEP: loads in any browser page,
 * worker or Node.
 */
import factory from './geom-core.js';
import { createLoader } from './loader.js';

export { GeomCoreSTLError } from './loader.js';

/** Instantiate the module; options pass through to Emscripten (locateFile, wasmBinary, ...). */
const createGeomCore = createLoader(factory);
export default createGeomCore;
