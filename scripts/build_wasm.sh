#!/bin/bash
#
# build_wasm.sh - Build geom-core WebAssembly modules
#
# Usage:
#   ./scripts/build_wasm.sh           # Release build, unified module
#   ./scripts/build_wasm.sh --debug   # Debug build with assertions
#   ./scripts/build_wasm.sh --split   # Build split modules for lazy loading
#   ./scripts/build_wasm.sh --occt    # Build with OCCT support (requires OCCT WASM)
#

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
PROJECT_ROOT="$( cd "$SCRIPT_DIR/.." && pwd )"

# Default options
BUILD_TYPE="Release"
SPLIT_MODULES="OFF"
WASM_THREADS="ON"
WASM_SIMD="ON"
USE_OCCT="OFF"
OCCT_WASM_ROOT=""

# Parse arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --debug)
            BUILD_TYPE="Debug"
            shift
            ;;
        --split)
            SPLIT_MODULES="ON"
            shift
            ;;
        --no-threads)
            WASM_THREADS="OFF"
            shift
            ;;
        --no-simd)
            WASM_SIMD="OFF"
            shift
            ;;
        --occt)
            USE_OCCT="ON"
            shift
            ;;
        --occt-root)
            OCCT_WASM_ROOT="$2"
            USE_OCCT="ON"
            shift 2
            ;;
        --help)
            echo "Usage: $0 [options]"
            echo ""
            echo "Options:"
            echo "  --debug        Build with debug symbols and assertions"
            echo "  --split        Build split modules for lazy loading"
            echo "  --no-threads   Disable threading (for browsers without SharedArrayBuffer)"
            echo "  --no-simd      Disable SIMD optimizations"
            echo "  --occt         Enable OCCT support"
            echo "  --occt-root    Path to OCCT WASM build directory"
            echo "  --help         Show this help message"
            exit 0
            ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            exit 1
            ;;
    esac
done

echo -e "${BLUE}============================================${NC}"
echo -e "${BLUE}  geom-core WASM Build${NC}"
echo -e "${BLUE}============================================${NC}"
echo ""
echo -e "Build Type:      ${GREEN}$BUILD_TYPE${NC}"
echo -e "Split Modules:   ${GREEN}$SPLIT_MODULES${NC}"
echo -e "Threading:       ${GREEN}$WASM_THREADS${NC}"
echo -e "SIMD:            ${GREEN}$WASM_SIMD${NC}"
echo -e "OCCT Support:    ${GREEN}$USE_OCCT${NC}"
if [[ -n "$OCCT_WASM_ROOT" ]]; then
    echo -e "OCCT Root:       ${GREEN}$OCCT_WASM_ROOT${NC}"
fi
echo ""

# Check for Emscripten
if ! command -v emcc &> /dev/null; then
    echo -e "${RED}Error: Emscripten not found!${NC}"
    echo ""
    echo "Install Emscripten SDK:"
    echo "  git clone https://github.com/emscripten-core/emsdk.git"
    echo "  cd emsdk"
    echo "  ./emsdk install latest"
    echo "  ./emsdk activate latest"
    echo "  source ./emsdk_env.sh"
    exit 1
fi

EMCC_VERSION=$(emcc --version | head -n1)
echo -e "Emscripten:      ${GREEN}$EMCC_VERSION${NC}"
echo ""

# Create build directory
BUILD_DIR="$PROJECT_ROOT/build/wasm"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"

# Configure CMake with Emscripten
echo -e "${YELLOW}Configuring CMake...${NC}"

CMAKE_ARGS=(
    -DCMAKE_TOOLCHAIN_FILE="$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"
    -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
    -DBUILD_WASM_BINDINGS=ON
    -DBUILD_PYTHON_BINDINGS=OFF
    -DBUILD_WASM_THREADS="$WASM_THREADS"
    -DBUILD_WASM_SIMD="$WASM_SIMD"
    -DSPLIT_WASM_MODULES="$SPLIT_MODULES"
    -DUSE_OCCT="$USE_OCCT"
)

if [[ -n "$OCCT_WASM_ROOT" ]]; then
    CMAKE_ARGS+=(-DOCCT_WASM_ROOT="$OCCT_WASM_ROOT")
fi

emcmake cmake "${CMAKE_ARGS[@]}" "$PROJECT_ROOT"

# Build
echo ""
echo -e "${YELLOW}Building WASM modules...${NC}"

# Use available cores
CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
emmake make -j"$CORES"

# Copy output to dist
echo ""
echo -e "${YELLOW}Copying to dist...${NC}"

DIST_DIR="$PROJECT_ROOT/dist/wasm"
mkdir -p "$DIST_DIR"

if [[ "$SPLIT_MODULES" == "ON" ]]; then
    cp -v "$BUILD_DIR/wasm/geom-core-base.js" "$DIST_DIR/"
    cp -v "$BUILD_DIR/wasm/geom-core-base.wasm" "$DIST_DIR/"
    cp -v "$BUILD_DIR/wasm/geom-core-boolean.js" "$DIST_DIR/"
    cp -v "$BUILD_DIR/wasm/geom-core-boolean.wasm" "$DIST_DIR/"
    cp -v "$BUILD_DIR/wasm/geom-core-analysis.js" "$DIST_DIR/"
    cp -v "$BUILD_DIR/wasm/geom-core-analysis.wasm" "$DIST_DIR/"

    if [[ "$USE_OCCT" == "ON" ]]; then
        cp -v "$BUILD_DIR/wasm/geom-core-occt.js" "$DIST_DIR/"
        cp -v "$BUILD_DIR/wasm/geom-core-occt.wasm" "$DIST_DIR/"
    fi

    if [[ "$WASM_THREADS" == "ON" ]]; then
        # Emscripten >= 3.1.58 folds the pthread worker into the main .js;
        # older releases emit separate *.worker.js files. Copy them if present.
        for worker in "$BUILD_DIR/wasm/"*.worker.js; do
            if [[ -f "$worker" ]]; then cp -v "$worker" "$DIST_DIR/"; fi
        done
    fi
else
    cp -v "$BUILD_DIR/wasm/geom-core.js" "$DIST_DIR/"
    cp -v "$BUILD_DIR/wasm/geom-core.wasm" "$DIST_DIR/"

    if [[ "$WASM_THREADS" == "ON" ]]; then
        # Emscripten >= 3.1.58 folds the pthread worker into geom-core.js;
        # older releases emit a separate geom-core.worker.js. Copy it if present.
        if [[ -f "$BUILD_DIR/wasm/geom-core.worker.js" ]]; then
            cp -v "$BUILD_DIR/wasm/geom-core.worker.js" "$DIST_DIR/"
        fi
    fi
fi

# Create TypeScript declarations
echo ""
echo -e "${YELLOW}Creating TypeScript declarations...${NC}"

cat > "$DIST_DIR/geom-core.d.ts" << 'EOF'
/**
 * geom-core WASM Module Type Definitions
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

export interface GeomCoreModule {
  GeomCoreCAD: new () => GeomCoreCAD;
}

declare function GeomCore(options?: { locateFile?: (path: string) => string }): Promise<GeomCoreModule>;

export default GeomCore;
EOF

# Print summary
echo ""
echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}  Build Complete!${NC}"
echo -e "${GREEN}============================================${NC}"
echo ""
echo "Output files:"
ls -lh "$DIST_DIR"/*.{js,wasm} 2>/dev/null | while read line; do
    echo "  $line"
done

# Calculate total size
TOTAL_SIZE=$(du -sh "$DIST_DIR" | cut -f1)
echo ""
echo -e "Total size: ${GREEN}$TOTAL_SIZE${NC}"
echo ""

# Usage hints
echo -e "${BLUE}Usage in browser:${NC}"
echo ""
echo "  import GeomCore from '@madfam/geom-core/wasm';"
echo ""
echo "  const module = await GeomCore();"
echo "  const engine = new module.GeomCoreCAD();"
echo "  engine.initialize();"
echo ""
echo "  const box = engine.makeBox({ width: 100, height: 50, depth: 75 });"
echo "  if (box.success) {"
echo "    const mesh = engine.tessellate(box.value.id);"
echo "    // Use mesh.positions, mesh.normals, mesh.indices with Three.js"
echo "  }"
echo ""

if [[ "$WASM_THREADS" == "ON" ]]; then
    echo -e "${YELLOW}Note:${NC} Threading requires these response headers:"
    echo "  Cross-Origin-Embedder-Policy: require-corp"
    echo "  Cross-Origin-Opener-Policy: same-origin"
    echo ""
fi
