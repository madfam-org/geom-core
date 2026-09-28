#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
cad_test_dir=$(mktemp -d)
trap 'rm -rf "$cad_test_dir"' EXIT
"${CXX:-c++}" -std=c++17 -pthread -Iinclude \
    tests/native/cad_contract.cpp src/cad/Engine.cpp src/cad/Primitives.cpp \
    src/cad/BooleanOps.cpp src/cad/Features.cpp src/cad/Transforms.cpp \
    src/cad/ShapeRegistry.cpp src/io/STLReader.cpp src/io/STLWriter.cpp \
    -o "$cad_test_dir/cad-contract"
"$cad_test_dir/cad-contract"
