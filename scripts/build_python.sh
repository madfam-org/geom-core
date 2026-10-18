#!/bin/bash
set -e

echo "========================================"
echo "Building geom-core Python bindings"
echo "========================================"

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_ROOT"

# Create build directory
mkdir -p build
cd build

# Configure CMake
echo "Configuring CMake..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_PYTHON_BINDINGS=ON \
    -DBUILD_WASM_BINDINGS=OFF

# Build
echo "Building..."
make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)

echo "========================================"
echo "Build complete!"
echo "Python module: build/python/geom_core_py.*"
echo "========================================"

# List built files (fails the build if no module was produced)
ls -lh python/geom_core_py.*
