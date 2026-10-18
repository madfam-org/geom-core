/**
 * geom-core WASM module types, shared by the single-threaded default entry
 * (`@madfam/geom-core/wasm`) and the opt-in threaded entry
 * (`@madfam/geom-core/threaded`). Hand-written to match
 * bindings/wasm/WasmEntry.cpp and bindings/wasm/WasmCAD.cpp; shipped as
 * dist/wasm/types.d.ts.
 */

export interface Vec3 {
  x: number;
  y: number;
  z: number;
}

export interface BoundingBox {
  min: Vec3;
  max: Vec3;
}

export interface ShapeHandle {
  id: string;
  type: number;
  bbox: BoundingBox;
  hash: string;
  volume?: number;
  surfaceArea?: number;
  centerOfMass?: Vec3;
}

export interface MeshData {
  positions: Float32Array;
  normals: Float32Array;
  indices: Uint32Array;
  uvs?: Float32Array;
  vertexCount: number;
  triangleCount: number;
  byteSize: number;
}

export interface OperationResult<T> {
  success: boolean;
  value?: T;
  error?: {
    code: string;
    message: string;
  };
  durationMs?: number;
  memoryUsedBytes?: number;
  wasCached?: boolean;
}

export interface ComplexityEstimate {
  score: number;
  estimatedMs: number;
  estimatedBytes: number;
  recommendRemote: boolean;
}

export interface HealthStatus {
  healthy: boolean;
  occtAvailable: boolean;
  version: string;
  shapeCount: number;
  memoryUsedBytes: number;
  cacheHitRate: number;
}

export interface GeomCoreCAD {
  // Lifecycle
  initialize(): boolean;
  isInitialized(): boolean;
  getVersion(): string;
  shutdown(): void;

  // Primitives
  makeBox(params?: { width?: number; height?: number; depth?: number; center?: Vec3 }): OperationResult<ShapeHandle>;
  makeSphere(params?: { radius?: number; center?: Vec3 }): OperationResult<ShapeHandle>;
  makeCylinder(params?: { radius?: number; height?: number; center?: Vec3; axis?: Vec3 }): OperationResult<ShapeHandle>;
  makeCone(params?: { radius1?: number; radius2?: number; height?: number; center?: Vec3; axis?: Vec3 }): OperationResult<ShapeHandle>;
  makeTorus(params?: { majorRadius?: number; minorRadius?: number; center?: Vec3; axis?: Vec3 }): OperationResult<ShapeHandle>;

  // Boolean operations
  booleanUnion(params: { shapeIds: string[] }): OperationResult<ShapeHandle>;
  booleanSubtract(params: { baseId: string; toolIds: string[] }): OperationResult<ShapeHandle>;
  booleanIntersect(params: { shapeIds: string[] }): OperationResult<ShapeHandle>;

  // Transforms
  translate(params: { shapeId: string; offset: Vec3 }): OperationResult<ShapeHandle>;
  rotate(params: { shapeId: string; axisOrigin?: Vec3; axisDirection?: Vec3; angle: number }): OperationResult<ShapeHandle>;
  scale(params: { shapeId: string; center?: Vec3; factor: number }): OperationResult<ShapeHandle>;
  mirror(params: { shapeId: string; planePoint?: Vec3; planeNormal?: Vec3 }): OperationResult<ShapeHandle>;

  // Analysis
  tessellate(shapeId: string, options?: { linearDeflection?: number; angularDeflection?: number; computeNormals?: boolean; computeUVs?: boolean }): OperationResult<MeshData>;
  getVolume(shapeId: string): OperationResult<number>;
  getSurfaceArea(shapeId: string): OperationResult<number>;
  getBoundingBox(shapeId: string): OperationResult<BoundingBox>;
  getCenterOfMass(shapeId: string): OperationResult<Vec3>;

  // Memory management
  disposeShape(shapeId: string): boolean;
  disposeAll(): void;
  getShapeCount(): number;
  getMemoryUsage(): number;
  getShapeHandle(shapeId: string): ShapeHandle;

  // Zero-lag optimization
  estimateComplexity(operation: string, shapeIds: string[]): ComplexityEstimate;
  precompute(operation: string, shapeIds: string[]): void;

  // Health
  healthCheck(): HealthStatus;
}

// Mesh analysis (bindings/wasm/WasmEntry.cpp): mesh in, numbers out.
export interface PrintabilityReport {
  overhangArea: number;
  overhangPercentage: number;
  thinWallVertexCount: number;
  score: number;
  totalSurfaceArea: number;
}

export interface OrientationResult {
  optimalUpVector: Vec3;
  originalOverhangArea: number;
  optimizedOverhangArea: number;
  improvementPercent: number;
}

/**
 * Mirror of the C++ `madfam::geom::MeshIntegrity` struct
 * (include/geom-core/Integrity.hpp), a port of yantra4d's MeshIntegrity.
 * tests/wasm-types.test.ts fails when a C++ field has no counterpart here.
 */
export interface MeshIntegrity {
  watertight: boolean;
  boundaryEdges: number;
  nonManifoldEdges: number;
  manifoldEdges: number;
  /** V - E + F of the judged mesh; null for an empty mesh. */
  eulerNumber: number | null;
  windingConsistent: boolean;
  /** Signed: negative when the surface is inside out. */
  volume: number;
  /** Tolerance that sealed the mesh; null when it was closed as loaded. */
  mergeTolerance: number | null;
  degenerateFaces: number;
  inverted: boolean;
  bodyCount: number;
  notes: string[];
  /** MeshIntegrity::summary(): one-line verdict naming the failure. */
  summary: string;
}

/**
 * Axis-aligned bounding box of the loaded mesh (mm for mm input).
 * All zero when no mesh is loaded.
 */
export interface MeshBoundingBox {
  min: Vec3;
  max: Vec3;
  /** max - min: width, height, depth. */
  size: Vec3;
  /** @deprecated 0.1.0 shape: equals size.x. */
  x: number;
  /** @deprecated 0.1.0 shape: equals size.y. */
  y: number;
  /** @deprecated 0.1.0 shape: equals size.z. */
  z: number;
}

export interface Analyzer {
  /**
   * Load a binary STL from memory. Returns true; throws GeomCoreSTLError
   * (with the parser's message) when the bytes are not a valid binary STL,
   * leaving no mesh loaded.
   */
  loadSTLFromBytes(data: Uint8Array | string): true;
  /** Load a binary STL from the module's virtual file system; throws like loadSTLFromBytes. */
  loadSTL(path: string): true;
  /** Parser message of the last failed load, "" after a successful one. */
  getLastError(): string;
  getVolume(): number;
  isWatertight(): boolean;
  getBoundingBox(): MeshBoundingBox;
  getVertexCount(): number;
  getTriangleCount(): number;
  /**
   * Welded vertex coordinates [x0, y0, z0, x1, ...] as an owned copy
   * (length 3 * getVertexCount()).
   */
  getVertices(): Float64Array;
  /**
   * Triangle corner indices into getVertices() [a0, b0, c0, a1, ...] as an
   * owned copy (length 3 * getTriangleCount()).
   */
  getIndices(): Uint32Array;
  /**
   * Zero-copy view of the same data as getVertices(), straight into WASM
   * memory. Valid only until the next call that may allocate (memory growth
   * detaches it), the next load, or delete(); copy it (.slice()) to keep it.
   */
  getVerticesView(): Float64Array;
  /** Zero-copy view of getIndices(); same lifetime rule as getVerticesView(). */
  getIndicesView(): Uint32Array;
  getSurfaceArea(): number;
  /** Volumetric centroid; origin when |volume| < 1e-9. */
  getCentroid(): Vec3;
  getBodyCount(): number;
  /** scaleHint <= 0 derives the scale from the largest bounding-box dimension. */
  assessIntegrity(scaleHint: number): MeshIntegrity;
  buildSpatialIndex(): void;
  getPrintabilityReport(criticalAngleDegrees: number, minWallThicknessMM: number): PrintabilityReport;
  autoOrient(sampleResolution: number, criticalAngleDegrees: number): OrientationResult;
  /** Zero-copy view (same lifetime rule as getVerticesView()). */
  getOverhangMapJS(criticalAngleDegrees: number): Uint8Array;
  /** Zero-copy view (same lifetime rule as getVerticesView()). */
  getWallThicknessMapJS(maxSearchDistanceMM: number): Float32Array;
  delete(): void;
}

/** The module the package entry points resolve to. */
export interface GeomCoreModule {
  Analyzer: new () => Analyzer;
  GeomCoreCAD: new () => GeomCoreCAD;
}

/**
 * Emscripten module options the entry points pass through to the factory.
 * Bundlers: pass locateFile (or wasmBinary) so the loader finds the .wasm
 * your bundler emitted, e.g. Vite's `@madfam/geom-core/wasm-binary?url`.
 */
export interface GeomCoreOptions {
  /** Map a runtime file name ("geom-core.wasm") to the URL or path to fetch. */
  locateFile?: (path: string, scriptDirectory: string) => string;
  /** The .wasm bytes, when the caller already has them. */
  wasmBinary?: ArrayBuffer | Uint8Array;
  /** stdout of the C++ code (load summaries). Defaults to console.log. */
  print?: (text: string) => void;
  /** stderr of the C++ code. Defaults to console.error. */
  printErr?: (text: string) => void;
  [option: string]: unknown;
}
