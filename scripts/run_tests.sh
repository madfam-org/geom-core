#!/bin/bash
set -e

echo "========================================"
echo "Running geom-core test suite"
echo "========================================"

# Get the script directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_ROOT"

# Set Python path to include the build directory
export PYTHONPATH="${PROJECT_ROOT}/build/python:${PYTHONPATH}"

# Run all tests
echo ""
echo "Running test_mesh.py..."
python3 tests/test_mesh.py
python3 tests/test_stl_admission.py

echo ""
echo "Running test_printability.py..."
python3 tests/test_printability.py

echo ""
echo "Running test_auto_orient.py..."
python3 tests/test_auto_orient.py

echo ""
echo "Running test_step.py..."
python3 tests/test_step.py
python3 tests/test_step_geometry.py

echo ""
echo "========================================"
echo "Native test scripts completed; skipped capability checks remain unverified."
echo "========================================"
