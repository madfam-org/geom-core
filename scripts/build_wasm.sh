#!/bin/bash
#
# build_wasm.sh - Build geom-core WebAssembly modules
#
# Usage:
#   ./scripts/build_wasm.sh                     # Release: single-threaded + threaded
#   ./scripts/build_wasm.sh --variant single    # only the default (no-threads) build
#   ./scripts/build_wasm.sh --variant threaded  # only the opt-in threaded build
#   ./scripts/build_wasm.sh --debug             # Debug build with assertions
#   ./scripts/build_wasm.sh --split             # Build split modules for lazy loading
#   ./scripts/build_wasm.sh --occt              # Build with OCCT support (requires OCCT WASM)
#
# Output (dist/wasm/, the npm package's WASM half):
#   geom-core.js + geom-core.wasm                    single-threaded ES module (default entry)
#   geom-core-threaded.js + geom-core-threaded.wasm  pthread build (opt-in, needs COOP/COEP)
#   index.js, threaded.js, loader.js + *.d.ts        package entry points (from src/wasm/)
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
VARIANTS="single threaded"
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
        --variant)
            case "$2" in
                single|threaded) VARIANTS="$2" ;;
                both) VARIANTS="single threaded" ;;
                *) echo -e "${RED}Unknown variant: $2 (single|threaded|both)${NC}"; exit 1 ;;
            esac
            shift 2
            ;;
        --no-threads)
            # Kept for 0.1.0 callers: same as --variant single.
            VARIANTS="single"
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
            echo "  --variant V    single | threaded | both (default: both)"
            echo "  --no-threads   Same as --variant single"
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
echo -e "Variants:        ${GREEN}$VARIANTS${NC}"
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

DIST_DIR="$PROJECT_ROOT/dist/wasm"
mkdir -p "$DIST_DIR"
CORES=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

build_variant() {
    local variant="$1" threads base
    if [[ "$variant" == "threaded" ]]; then threads="ON"; base="geom-core-threaded"; else threads="OFF"; base="geom-core"; fi

    local build_dir="$PROJECT_ROOT/build/wasm-$variant"
    mkdir -p "$build_dir"
    cd "$build_dir"

    echo -e "${YELLOW}Configuring CMake ($variant)...${NC}"
    local cmake_args=(
        -DCMAKE_TOOLCHAIN_FILE="$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake"
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
        -DBUILD_WASM_BINDINGS=ON
        -DBUILD_PYTHON_BINDINGS=OFF
        -DBUILD_WASM_THREADS="$threads"
        -DBUILD_WASM_SIMD="$WASM_SIMD"
        -DSPLIT_WASM_MODULES="$SPLIT_MODULES"
        -DUSE_OCCT="$USE_OCCT"
    )
    if [[ -n "$OCCT_WASM_ROOT" ]]; then
        cmake_args+=(-DOCCT_WASM_ROOT="$OCCT_WASM_ROOT")
    fi
    emcmake cmake "${cmake_args[@]}" "$PROJECT_ROOT"

    echo -e "${YELLOW}Building WASM modules ($variant)...${NC}"
    emmake make -j"$CORES"

    echo -e "${YELLOW}Copying $variant output to dist...${NC}"
    if [[ "$SPLIT_MODULES" == "ON" ]]; then
        for module in geom-core-base geom-core-boolean geom-core-analysis; do
            cp -v "$build_dir/wasm/$module.js" "$build_dir/wasm/$module.wasm" "$DIST_DIR/"
        done
        if [[ "$USE_OCCT" == "ON" ]]; then
            cp -v "$build_dir/wasm/geom-core-occt.js" "$build_dir/wasm/geom-core-occt.wasm" "$DIST_DIR/"
        fi
    else
        cp -v "$build_dir/wasm/$base.js" "$build_dir/wasm/$base.wasm" "$DIST_DIR/"
    fi
    if [[ "$threads" == "ON" ]]; then
        # Emscripten >= 3.1.58 folds the pthread worker into the main .js;
        # older releases emit separate *.worker.js files. Copy them if present.
        for worker in "$build_dir/wasm/"*.worker.js "$build_dir/wasm/"*.worker.mjs; do
            if [[ -f "$worker" ]]; then cp -v "$worker" "$DIST_DIR/"; fi
        done
    fi
    cd "$PROJECT_ROOT"
}

for variant in $VARIANTS; do
    build_variant "$variant"
done

# Package entry points and TypeScript declarations (hand-written, typechecked
# by `pnpm typecheck`, see src/wasm/). The Emscripten loaders are ES modules,
# so dist/wasm is marked "type": "module" for Node.
echo ""
echo -e "${YELLOW}Copying entry points and TypeScript declarations...${NC}"
cp -v "$PROJECT_ROOT"/src/wasm/*.js "$PROJECT_ROOT"/src/wasm/*.d.ts "$DIST_DIR/"
printf '{"type":"module"}\n' > "$DIST_DIR/package.json"

# Print summary
echo ""
echo -e "${GREEN}============================================${NC}"
echo -e "${GREEN}  Build Complete!${NC}"
echo -e "${GREEN}============================================${NC}"
echo ""
echo "Output files:"
ls -lh "$DIST_DIR"/*.wasm "$DIST_DIR"/geom-core*.js 2>/dev/null | while read line; do
    echo "  $line"
done

# Calculate total size
TOTAL_SIZE=$(du -sh "$DIST_DIR" | cut -f1)
echo ""
echo -e "Total size: ${GREEN}$TOTAL_SIZE${NC}"
echo ""

# Usage hints
echo -e "${BLUE}Usage in browser (single-threaded, no COOP/COEP needed):${NC}"
echo ""
echo "  import createGeomCore from '@madfam/geom-core/wasm';"
echo "  import wasmUrl from '@madfam/geom-core/wasm-binary?url';  // Vite"
echo ""
echo "  const mod = await createGeomCore({ locateFile: (p) => p.endsWith('.wasm') ? wasmUrl : p });"
echo "  const analyzer = new mod.Analyzer();"
echo "  analyzer.loadSTLFromBytes(bytes);  // throws GeomCoreSTLError on a bad STL"
echo ""
if [[ "$VARIANTS" == *threaded* ]]; then
    echo -e "${YELLOW}Note:${NC} @madfam/geom-core/threaded requires these response headers:"
    echo "  Cross-Origin-Embedder-Policy: require-corp"
    echo "  Cross-Origin-Opener-Policy: same-origin"
    echo ""
fi
