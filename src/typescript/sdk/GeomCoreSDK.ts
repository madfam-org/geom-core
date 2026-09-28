/**
 * GeomCore SDK - Smart Geometry Operations with Zero-Lag UX
 *
 * This SDK provides intelligent routing between local WASM execution
 * and remote GPU compute for optimal performance.
 *
 * Key Features:
 * - Automatic operation routing (local vs remote)
 * - Precomputation hints for speculative execution
 * - Memory management with LRU eviction
 * - Performance monitoring and slow operation callbacks
 *
 * @module geom-core/sdk
 */

import type { GeometryEngine } from "../bindings/GeometryEngine.js";
import type {
  ShapeHandle,
  OperationResult,
  ComputeLocation,
  ComputeHint,
  ComplexityEstimate,
  MeshData,
  TessellateOptions,
  BoxParams,
  SphereParams,
  CylinderParams,
  ConeParams,
  TorusParams,
  BooleanUnionParams,
  BooleanSubtractParams,
  BooleanIntersectParams,
  ExtrudeParams,
  RevolveParams,
  SweepParams,
  LoftParams,
  FilletParams,
  ChamferParams,
  ShellParams,
  OffsetParams,
  TranslateParams,
  RotateParams,
  ScaleParams,
  MirrorParams,
  ShapeProperties,
} from "../bindings/types.js";

// Vec3 re-exported for SDK consumers
export type { Vec3 } from "../bindings/types.js";

// =============================================================================
// Configuration Types
// =============================================================================

export interface GeomCoreConfig {
  /** Maximum memory usage in bytes before LRU eviction (default: 512MB) */
  maxMemoryBytes?: number;

  /** Threshold in ms for slow operation warnings (default: 100) */
  slowOperationThresholdMs?: number;

  /** Enable remote compute offloading (default: true if remoteEndpoint provided) */
  enableRemoteCompute?: boolean;

  /** Remote compute server endpoint */
  remoteEndpoint?: string;

  /** API key for remote compute (for paid tiers) */
  remoteApiKey?: string;

  /** Complexity threshold for automatic remote routing (0-1, default: 0.7) */
  remoteComplexityThreshold?: number;

  /** Enable speculative precomputation (default: true) */
  enablePrecomputation?: boolean;

  /** WebSocket connection for real-time remote compute */
  useWebSocket?: boolean;
}

export interface RemoteJobStatus {
  jobId: string;
  status: "queued" | "processing" | "completed" | "failed";
  progress?: number;
  result?: ShapeHandle;
  error?: string;
}

export type SlowOperationCallback = (
  operation: string,
  durationMs: number,
) => void;
export type MemoryPressureCallback = (
  usedBytes: number,
  maxBytes: number,
) => void;

// =============================================================================
// Remote Compute Client
// =============================================================================

class RemoteComputeClient {
  private endpoint: string;
  private apiKey?: string;
  private ws?: WebSocket;
  private pendingJobs: Map<
    string,
    {
      resolve: (result: ShapeHandle) => void;
      reject: (error: Error) => void;
    }
  > = new Map();

  constructor(endpoint: string, apiKey?: string, useWebSocket = false) {
    this.endpoint = endpoint;
    this.apiKey = apiKey;

    if (useWebSocket) {
      this.connectWebSocket();
    }
  }

  private connectWebSocket(): void {
    const wsEndpoint = this.endpoint.replace(/^http/, "ws") + "/ws";
    this.ws = new WebSocket(wsEndpoint);

    this.ws.onmessage = (event) => {
      const data = JSON.parse(event.data);
      const pending = this.pendingJobs.get(data.jobId);

      if (pending) {
        if (data.status === "completed") {
          pending.resolve(data.result);
        } else if (data.status === "failed") {
          pending.reject(new Error(data.error));
        }

        if (data.status === "completed" || data.status === "failed") {
          this.pendingJobs.delete(data.jobId);
        }
      }
    };

    this.ws.onerror = (error) => {
      console.error("WebSocket error:", error);
    };
  }

  async executeRemote(
    operation: string,
    params: Record<string, unknown>,
  ): Promise<ShapeHandle> {
    const headers: Record<string, string> = {
      "Content-Type": "application/json",
    };

    if (this.apiKey) {
      headers["Authorization"] = `Bearer ${this.apiKey}`;
    }

    const response = await fetch(`${this.endpoint}/compute`, {
      method: "POST",
      headers,
      body: JSON.stringify({
        operation,
        params,
        timestamp: Date.now(),
      }),
    });

    if (!response.ok) {
      throw new Error(`Remote compute failed: ${response.statusText}`);
    }

    const result = await response.json();
    return result.shape;
  }

  async submitJob(
    operation: string,
    params: Record<string, unknown>,
  ): Promise<string> {
    const headers: Record<string, string> = {
      "Content-Type": "application/json",
    };

    if (this.apiKey) {
      headers["Authorization"] = `Bearer ${this.apiKey}`;
    }

    const response = await fetch(`${this.endpoint}/jobs`, {
      method: "POST",
      headers,
      body: JSON.stringify({
        operation,
        params,
      }),
    });

    if (!response.ok) {
      throw new Error(`Failed to submit job: ${response.statusText}`);
    }

    const result = await response.json();
    return result.jobId;
  }

  async getJobStatus(jobId: string): Promise<RemoteJobStatus> {
    const headers: Record<string, string> = {};

    if (this.apiKey) {
      headers["Authorization"] = `Bearer ${this.apiKey}`;
    }

    const response = await fetch(`${this.endpoint}/jobs/${jobId}`, {
      headers,
    });

    if (!response.ok) {
      throw new Error(`Failed to get job status: ${response.statusText}`);
    }

    return response.json();
  }

  waitForJob(jobId: string): Promise<ShapeHandle> {
    return new Promise((resolve, reject) => {
      if (this.ws) {
        // Use WebSocket for real-time updates
        this.pendingJobs.set(jobId, { resolve, reject });
        this.ws.send(JSON.stringify({ type: "subscribe", jobId }));
      } else {
        // Poll for status
        const poll = async () => {
          const status = await this.getJobStatus(jobId);

          if (status.status === "completed" && status.result) {
            resolve(status.result);
          } else if (status.status === "failed") {
            reject(new Error(status.error || "Job failed"));
          } else {
            setTimeout(poll, 100); // Poll every 100ms
          }
        };

        poll();
      }
    });
  }

  disconnect(): void {
    if (this.ws) {
      this.ws.close();
    }
  }
}

// =============================================================================
// Precomputation Manager
// =============================================================================

class PrecomputationManager {
  private cache: Map<
    string,
    {
      result: ShapeHandle;
      timestamp: number;
    }
  > = new Map();

  private pending: Map<string, Promise<ShapeHandle>> = new Map();
  private maxCacheAge = 30000; // 30 seconds

  generateKey(operation: string, params: Record<string, unknown>): string {
    return `${operation}:${JSON.stringify(params)}`;
  }

  getCached(key: string): ShapeHandle | undefined {
    const cached = this.cache.get(key);

    if (cached && Date.now() - cached.timestamp < this.maxCacheAge) {
      return cached.result;
    }

    if (cached) {
      this.cache.delete(key);
    }

    return undefined;
  }

  setCached(key: string, result: ShapeHandle): void {
    this.cache.set(key, {
      result,
      timestamp: Date.now(),
    });
  }

  setPending(key: string, promise: Promise<ShapeHandle>): void {
    this.pending.set(key, promise);

    promise.finally(() => {
      this.pending.delete(key);
    });
  }

  getPending(key: string): Promise<ShapeHandle> | undefined {
    return this.pending.get(key);
  }

  clear(): void {
    this.cache.clear();
    this.pending.clear();
  }
}

// =============================================================================
// GeomCore SDK
// =============================================================================

export class GeomCoreSDK {
  private engine: GeometryEngine;
  private config: Required<GeomCoreConfig>;
  private remoteClient?: RemoteComputeClient;
  private precomputation: PrecomputationManager;
  private slowOpCallbacks: SlowOperationCallback[] = [];
  private memoryCallbacks: MemoryPressureCallback[] = [];
  private usedMemoryBytes = 0;

  constructor(engine: GeometryEngine, config: GeomCoreConfig = {}) {
    this.engine = engine;
    this.precomputation = new PrecomputationManager();

    this.config = {
      maxMemoryBytes: config.maxMemoryBytes ?? 512 * 1024 * 1024, // 512MB
      slowOperationThresholdMs: config.slowOperationThresholdMs ?? 100,
      enableRemoteCompute:
        config.enableRemoteCompute ?? !!config.remoteEndpoint,
      remoteEndpoint: config.remoteEndpoint ?? "",
      remoteApiKey: config.remoteApiKey ?? "",
      remoteComplexityThreshold: config.remoteComplexityThreshold ?? 0.7,
      enablePrecomputation: config.enablePrecomputation ?? true,
      useWebSocket: config.useWebSocket ?? false,
    };

    if (this.config.enableRemoteCompute && this.config.remoteEndpoint) {
      this.remoteClient = new RemoteComputeClient(
        this.config.remoteEndpoint,
        this.config.remoteApiKey,
        this.config.useWebSocket,
      );
    }
  }

  // ===========================================================================
  // Lifecycle
  // ===========================================================================

  dispose(): void {
    this.engine.disposeAll();
    this.precomputation.clear();
    this.remoteClient?.disconnect();
  }

  getStats(): {
    shapeCount: number;
    usedMemoryBytes: number;
    maxMemoryBytes: number;
    remoteEnabled: boolean;
  } {
    return {
      shapeCount: this.engine.getShapeCount(),
      usedMemoryBytes: this.usedMemoryBytes,
      maxMemoryBytes: this.config.maxMemoryBytes,
      remoteEnabled: this.config.enableRemoteCompute,
    };
  }

  // ===========================================================================
  // Callbacks
  // ===========================================================================

  onSlowOperation(callback: SlowOperationCallback): () => void {
    this.slowOpCallbacks.push(callback);
    return () => {
      const idx = this.slowOpCallbacks.indexOf(callback);
      if (idx >= 0) this.slowOpCallbacks.splice(idx, 1);
    };
  }

  onMemoryPressure(callback: MemoryPressureCallback): () => void {
    this.memoryCallbacks.push(callback);
    return () => {
      const idx = this.memoryCallbacks.indexOf(callback);
      if (idx >= 0) this.memoryCallbacks.splice(idx, 1);
    };
  }

  private notifySlowOperation(operation: string, durationMs: number): void {
    if (durationMs >= this.config.slowOperationThresholdMs) {
      for (const cb of this.slowOpCallbacks) {
        cb(operation, durationMs);
      }
    }
  }

  private checkMemoryPressure(additionalBytes: number): void {
    const newUsed = this.usedMemoryBytes + additionalBytes;

    if (newUsed > this.config.maxMemoryBytes * 0.9) {
      for (const cb of this.memoryCallbacks) {
        cb(newUsed, this.config.maxMemoryBytes);
      }
    }

    if (newUsed > this.config.maxMemoryBytes) {
      // Trigger eviction - this would need integration with the engine
      console.warn("Memory pressure: consider disposing unused shapes");
    }
  }

  // ===========================================================================
  // Smart Operation Routing
  // ===========================================================================

  private async executeWithRouting<T>(
    operation: string,
    params: Record<string, unknown>,
    localExecutor: () => OperationResult<T>,
    hint?: ComputeHint,
  ): Promise<OperationResult<T>> {
    const shapeIds = this.extractShapeIds(params);
    const complexity = this.engine.estimateComplexity(operation, shapeIds);

    // Determine execution location
    let location: ComputeLocation = hint?.preferLocation ?? "auto";

    if (location === "auto") {
      if (
        complexity.score > this.config.remoteComplexityThreshold &&
        this.config.enableRemoteCompute
      ) {
        location = "remote";
      } else {
        location = "local";
      }
    }

    // Check precomputation cache
    if (this.config.enablePrecomputation) {
      const cacheKey = this.precomputation.generateKey(operation, params);
      const cached = this.precomputation.getCached(cacheKey);

      if (cached) {
        return {
          success: true,
          value: cached as T,
          durationMs: 0,
          wasCached: true,
        };
      }

      // Check for pending precomputation
      const pending = this.precomputation.getPending(cacheKey);
      if (pending) {
        const result = await pending;
        return {
          success: true,
          value: result as T,
          durationMs: 0,
          wasCached: true,
        };
      }
    }

    // Execute based on location
    if (location === "remote" && this.remoteClient) {
      try {
        const result = await this.remoteClient.executeRemote(operation, params);
        return {
          success: true,
          value: result as T,
          executedRemotely: true,
        };
      } catch (error) {
        // Fall back to local on remote failure
        console.warn(
          `Remote execution failed, falling back to local: ${error}`,
        );
      }
    }

    // Local execution
    const result = localExecutor();

    if (result.durationMs !== undefined) {
      this.notifySlowOperation(operation, result.durationMs);
    }

    if (result.success && result.memoryUsedBytes) {
      this.checkMemoryPressure(result.memoryUsedBytes);
      this.usedMemoryBytes += result.memoryUsedBytes;
    }

    return result;
  }

  private extractShapeIds(params: Record<string, unknown>): string[] {
    const ids: string[] = [];

    const extractId = (value: unknown): void => {
      if (typeof value === "string" && value.startsWith("shape_")) {
        ids.push(value);
      } else if (typeof value === "object" && value !== null) {
        if ("id" in value && typeof (value as any).id === "string") {
          ids.push((value as any).id);
        }
      }
    };

    for (const value of Object.values(params)) {
      if (Array.isArray(value)) {
        value.forEach(extractId);
      } else {
        extractId(value);
      }
    }

    return ids;
  }

  // ===========================================================================
  // Precomputation Hints
  // ===========================================================================

  /**
   * Hint that an operation may be performed soon (e.g., user hovering over tool).
   * The SDK will speculatively execute and cache the result.
   */
  precompute(operation: string, params: Record<string, unknown>): void {
    if (!this.config.enablePrecomputation) return;

    const cacheKey = this.precomputation.generateKey(operation, params);

    // Don't precompute if already cached or pending
    if (this.precomputation.getCached(cacheKey)) return;
    if (this.precomputation.getPending(cacheKey)) return;

    // Get the executor for this operation
    const executor = this.getOperationExecutor(operation, params);
    if (!executor) return;

    const promise = new Promise<ShapeHandle>((resolve, reject) => {
      // Execute in next microtask to not block current frame
      queueMicrotask(() => {
        try {
          const result = executor();
          if (result.success && result.value) {
            this.precomputation.setCached(
              cacheKey,
              result.value as ShapeHandle,
            );
            resolve(result.value as ShapeHandle);
          } else {
            reject(new Error(result.error?.message || "Precomputation failed"));
          }
        } catch (error) {
          reject(error);
        }
      });
    });

    this.precomputation.setPending(cacheKey, promise);
  }

  /**
   * Cancel a precomputation hint (e.g., user moved away from tool).
   */
  cancelPrecompute(operation: string, params: Record<string, unknown>): void {
    // Generate cache key for potential future use with cancellation tokens
    const _cacheKey = this.precomputation.generateKey(operation, params);
    // Note: Can't actually cancel in-flight operations, but we can remove from pending
    // In a real implementation, we'd need cancellation tokens
    void _cacheKey; // Suppress unused variable warning
  }

  private getOperationExecutor(
    operation: string,
    params: Record<string, unknown>,
  ): (() => OperationResult<ShapeHandle>) | null {
    // Map operation names to engine methods
    const executors: Record<string, () => OperationResult<ShapeHandle>> = {
      makeBox: () => this.engine.makeBox(params as unknown as BoxParams),
      makeSphere: () =>
        this.engine.makeSphere(params as unknown as SphereParams),
      makeCylinder: () =>
        this.engine.makeCylinder(params as unknown as CylinderParams),
      makeCone: () => this.engine.makeCone(params as unknown as ConeParams),
      makeTorus: () => this.engine.makeTorus(params as unknown as TorusParams),
      booleanUnion: () =>
        this.engine.booleanUnion(params as unknown as BooleanUnionParams),
      booleanSubtract: () =>
        this.engine.booleanSubtract(params as unknown as BooleanSubtractParams),
      booleanIntersect: () =>
        this.engine.booleanIntersect(
          params as unknown as BooleanIntersectParams,
        ),
      extrude: () => this.engine.extrude(params as unknown as ExtrudeParams),
      revolve: () => this.engine.revolve(params as unknown as RevolveParams),
      fillet: () => this.engine.fillet(params as unknown as FilletParams),
      chamfer: () => this.engine.chamfer(params as unknown as ChamferParams),
    };

    return executors[operation] || null;
  }

  // ===========================================================================
  // Complexity Estimation
  // ===========================================================================

  estimateComplexity(
    operation: string,
    shapeIds: string[],
  ): ComplexityEstimate {
    return this.engine.estimateComplexity(operation, shapeIds);
  }

  // ===========================================================================
  // 3D Primitives (Always Local - <5ms)
  // ===========================================================================

  async makeBox(
    params?: BoxParams,
    hint?: ComputeHint,
  ): Promise<OperationResult<ShapeHandle>> {
    return this.executeWithRouting(
      "makeBox",
      (params || {}) as Record<string, unknown>,
      () => this.engine.makeBox(params),
      { ...hint, preferLocation: "local" },
    );
  }

  async makeSphere(
    params?: SphereParams,
    hint?: ComputeHint,
  ): Promise<OperationResult<ShapeHandle>> {
    return this.executeWithRouting(
      "makeSphere",
      (params || {}) as Record<string, unknown>,
      () => this.engine.makeSphere(params),
      { ...hint, preferLocation: "local" },
    );
  }

  async makeCylinder(
    params?: CylinderParams,
    hint?: ComputeHint,
  ): Promise<OperationResult<ShapeHandle>> {
    return this.executeWithRouting(
      "makeCylinder",
      (params || {}) as Record<string, unknown>,
      () => this.engine.makeCylinder(params),
      { ...hint, preferLocation: "local" },
    );
  }

  async makeCone(
    params?: ConeParams,
    hint?: ComputeHint,
  ): Promise<OperationResult<ShapeHandle>> {
    return this.executeWithRouting(
      "makeCone",
      (params || {}) as Record<string, unknown>,
      () => this.engine.makeCone(params),
      { ...hint, preferLocation: "local" },
    );
  }

  async makeTorus(
    params?: TorusParams,
    hint?: ComputeHint,
  ): Promise<OperationResult<ShapeHandle>> {
    return this.executeWithRouting(
      "makeTorus",
      (params || {}) as Record<string, unknown>,
      () => this.engine.makeTorus(params),
      { ...hint, preferLocation: "local" },
    );
  }

  // ===========================================================================
  // Boolean Operations (Auto-Routed)
  // ===========================================================================

  async booleanUnion(
    params: BooleanUnionParams,
    hint?: ComputeHint,
  ): Promise<OperationResult<ShapeHandle>> {
    return this.executeWithRouting(
      "booleanUnion",
      params as unknown as Record<string, unknown>,
      () => this.engine.booleanUnion(params),
      hint,
    );
  }

  async booleanSubtract(
    params: BooleanSubtractParams,
    hint?: ComputeHint,
  ): Promise<OperationResult<ShapeHandle>> {
    return this.executeWithRouting(
      "booleanSubtract",
      params as unknown as Record<string, unknown>,
      () => this.engine.booleanSubtract(params),
      hint,
    );
  }

  async booleanIntersect(
    params: BooleanIntersectParams,
    hint?: ComputeHint,
  ): Promise<OperationResult<ShapeHandle>> {
    return this.executeWithRouting(
      "booleanIntersect",
      params as unknown as Record<string, unknown>,
      () => this.engine.booleanIntersect(params),
      hint,
    );
  }

  // ===========================================================================
  // Feature Operations (Auto-Routed)
  // ===========================================================================

  async extrude(
    params: ExtrudeParams,
    hint?: ComputeHint,
  ): Promise<OperationResult<ShapeHandle>> {
    return this.executeWithRouting(
      "extrude",
      params as unknown as Record<string, unknown>,
      () => this.engine.extrude(params),
      hint,
    );
  }

  async revolve(
    params: RevolveParams,
    hint?: ComputeHint,
  ): Promise<OperationResult<ShapeHandle>> {
    return this.executeWithRouting(
      "revolve",
      params as unknown as Record<string, unknown>,
      () => this.engine.revolve(params),
      hint,
    );
  }

  async sweep(
    params: SweepParams,
    hint?: ComputeHint,
  ): Promise<OperationResult<ShapeHandle>> {
    return this.executeWithRouting(
      "sweep",
      params as unknown as Record<string, unknown>,
      () => this.engine.sweep(params),
      hint,
    );
  }

  async loft(
    params: LoftParams,
    hint?: ComputeHint,
  ): Promise<OperationResult<ShapeHandle>> {
    return this.executeWithRouting(
      "loft",
      params as unknown as Record<string, unknown>,
      () => this.engine.loft(params),
      hint,
    );
  }

  async fillet(
    params: FilletParams,
    hint?: ComputeHint,
  ): Promise<OperationResult<ShapeHandle>> {
    return this.executeWithRouting(
      "fillet",
      params as unknown as Record<string, unknown>,
      () => this.engine.fillet(params),
      hint,
    );
  }

  async chamfer(
    params: ChamferParams,
    hint?: ComputeHint,
  ): Promise<OperationResult<ShapeHandle>> {
    return this.executeWithRouting(
      "chamfer",
      params as unknown as Record<string, unknown>,
      () => this.engine.chamfer(params),
      hint,
    );
  }

  async shell(
    params: ShellParams,
    hint?: ComputeHint,
  ): Promise<OperationResult<ShapeHandle>> {
    return this.executeWithRouting(
      "shell",
      params as unknown as Record<string, unknown>,
      () => this.engine.shell(params),
      hint,
    );
  }

  async offset(
    params: OffsetParams,
    hint?: ComputeHint,
  ): Promise<OperationResult<ShapeHandle>> {
    return this.executeWithRouting(
      "offset",
      params as unknown as Record<string, unknown>,
      () => this.engine.offset(params),
      hint,
    );
  }

  // ===========================================================================
  // Transforms (Always Local - <2ms)
  // ===========================================================================

  async translate(
    params: TranslateParams,
  ): Promise<OperationResult<ShapeHandle>> {
    return this.engine.translate(params);
  }

  async rotate(params: RotateParams): Promise<OperationResult<ShapeHandle>> {
    return this.engine.rotate(params);
  }

  async scale(params: ScaleParams): Promise<OperationResult<ShapeHandle>> {
    return this.engine.scale(params);
  }

  async mirror(params: MirrorParams): Promise<OperationResult<ShapeHandle>> {
    return this.engine.mirror(params);
  }

  // ===========================================================================
  // Analysis (Local with Remote fallback for large meshes)
  // ===========================================================================

  async tessellate(
    shapeId: string,
    options?: TessellateOptions,
    _hint?: ComputeHint,
  ): Promise<OperationResult<MeshData>> {
    // Tessellation is always local for responsiveness
    // Note: _hint parameter reserved for future remote tessellation support
    return this.engine.tessellate(shapeId, options);
  }

  async getProperties(
    shapeId: string,
  ): Promise<OperationResult<ShapeProperties>> {
    return this.engine.getProperties(shapeId);
  }

  async getVolume(shapeId: string): Promise<OperationResult<number>> {
    return this.engine.getVolume(shapeId);
  }

  async getSurfaceArea(shapeId: string): Promise<OperationResult<number>> {
    return this.engine.getSurfaceArea(shapeId);
  }

  // ===========================================================================
  // Memory Management
  // ===========================================================================

  disposeShape(id: string): boolean {
    const success = this.engine.disposeShape(id);
    if (success) {
      // Estimate freed memory (would need actual tracking in production)
      this.usedMemoryBytes = Math.max(0, this.usedMemoryBytes - 50000);
    }
    return success;
  }

  disposeAll(): void {
    this.engine.disposeAll();
    this.usedMemoryBytes = 0;
    this.precomputation.clear();
  }
}

// =============================================================================
// Factory Functions
// =============================================================================

export function createGeomCoreSDK(
  engine: GeometryEngine,
  config?: GeomCoreConfig,
): GeomCoreSDK {
  return new GeomCoreSDK(engine, config);
}

/**
 * Create SDK configured for browser use (local WASM only)
 */
export function createBrowserSDK(engine: GeometryEngine): GeomCoreSDK {
  return new GeomCoreSDK(engine, {
    enableRemoteCompute: false,
    enablePrecomputation: true,
    maxMemoryBytes: 256 * 1024 * 1024, // 256MB for browser
    slowOperationThresholdMs: 16, // Target 60fps
  });
}

/**
 * Create SDK configured for paid tier with remote GPU compute
 */
export function createPaidTierSDK(
  engine: GeometryEngine,
  remoteEndpoint: string,
  apiKey: string,
): GeomCoreSDK {
  return new GeomCoreSDK(engine, {
    enableRemoteCompute: true,
    remoteEndpoint,
    remoteApiKey: apiKey,
    useWebSocket: true,
    enablePrecomputation: true,
    maxMemoryBytes: 512 * 1024 * 1024, // 512MB
    remoteComplexityThreshold: 0.5, // More aggressive offloading for paid users
  });
}
