import type { GeomCoreOptions } from './types';

/**
 * Raw Emscripten factory (MODULARIZE + EXPORT_ES6). Its Analyzer exposes the
 * boolean _loadSTLFromBytes/_loadSTL; import the package entry point instead
 * of this file to get the throwing loaders.
 */
declare function GeomCoreFactory(options?: GeomCoreOptions): Promise<unknown>;
export default GeomCoreFactory;
