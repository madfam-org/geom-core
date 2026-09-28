/**
 * geom-core - Unified Geometry Engine for MADFAM Ecosystem
 *
 * A high-performance B-Rep geometry engine built on OpenCASCADE Technology (OCCT).
 * Designed for zero-lag browser execution with optional remote GPU compute offloading.
 *
 * @packageDocumentation
 * @module geom-core
 *
 * @example
 * ```typescript
 * import { createGeomCoreSDK, createGeometryEngine } from '@madfam/geom-core';
 * import initOCCT from 'opencascade.js';
 *
 * // Initialize WASM module
 * const occt = await initOCCT();
 *
 * // Create engine and SDK
 * const engine = createGeometryEngine(occt);
 * const sdk = createGeomCoreSDK(engine, {
 *   enablePrecomputation: true,
 *   maxMemoryBytes: 512 * 1024 * 1024,
 * });
 *
 * // Create a box
 * const result = await sdk.makeBox({ width: 100, height: 50, depth: 75 });
 * if (result.success) {
 *   console.log('Created shape:', result.value.id);
 *
 *   // Tessellate for Three.js
 *   const mesh = await sdk.tessellate(result.value.id);
 *   // Use mesh.positions, mesh.normals, mesh.indices with Three.js
 * }
 * ```
 */

// =============================================================================
// Core Types
// =============================================================================

export type {
  // Geometry types
  Vec3,
  BoundingBox,
  ShapeType,
  ShapeHandle,
  MeshData,
  TessellateOptions,

  // Parameter types - Primitives
  BoxParams,
  SphereParams,
  CylinderParams,
  ConeParams,
  TorusParams,
  LineParams,
  CircleParams,
  RectangleParams,
  ArcParams,
  PolygonParams,
  EllipseParams,
  PointParams,

  // Parameter types - Booleans
  BooleanUnionParams,
  BooleanSubtractParams,
  BooleanIntersectParams,

  // Parameter types - Features
  ExtrudeParams,
  RevolveParams,
  SweepParams,
  LoftParams,
  FilletParams,
  ChamferParams,
  ShellParams,
  DraftParams,
  OffsetParams,

  // Parameter types - Transforms
  TransformParams,
  TranslateParams,
  RotateParams,
  ScaleParams,
  MirrorParams,
  Matrix4x4Object,

  // Analysis types
  ShapeProperties,

  // Result types
  OperationResult,
  ComplexityEstimate,
  ComputeLocation,
  ComputeHint,

  // I/O types
  ImportFormat,
  ExportFormat,
  ImportParams,
  ExportParams,
  ExportOptions,
  ImportResult,
  ExportResult,

  // Assembly types
  AssemblyParams,
  AssemblyHandle,
  AssemblyPart,
  PartTransform,
  MateType,
  MateParams,
  MateConstraint,
  PatternType,
  PatternParams,
  PatternResult,

  // WASM types (for advanced users)
  WASMModule,
  OCCTShape,
  OCCTHandle,
} from "./bindings/types.js";

export {
  createHandleId,
  resetHandleIdCounter,
  getShapeId,
} from "./bindings/types.js";

// =============================================================================
// Core Engine
// =============================================================================

export {
  GeometryEngine,
  createGeometryEngine,
} from "./bindings/GeometryEngine.js";

// =============================================================================
// SDK
// =============================================================================

export type {
  GeomCoreConfig,
  RemoteJobStatus,
  SlowOperationCallback,
  MemoryPressureCallback,
} from "./sdk/GeomCoreSDK.js";

export {
  GeomCoreSDK,
  createGeomCoreSDK,
  createBrowserSDK,
  createPaidTierSDK,
} from "./sdk/GeomCoreSDK.js";

// =============================================================================
// Version
// =============================================================================

export const VERSION = "0.1.0";
export const OCCT_VERSION = "7.7.0";
