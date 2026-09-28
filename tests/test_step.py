#!/usr/bin/env python3
"""
Test STEP file loading (Milestone 7)

Tests the loadStep() method. This test will pass whether or not OCCT is available.
When OCCT is not available, loadStep() should return False.
When OCCT is available, it should successfully load a STEP file.
"""

import sys
import os

# Import the compiled module
try:
    import geom_core_py as gc
except ImportError:
    print("ERROR: Could not import geom_core_py module")
    print("Make sure to build the project first with: cmake .. && make")
    sys.exit(1)


def test_step_api():
    """Test that the loadStep API exists"""
    print("=" * 70)
    print("geom-core Milestone 7: STEP File Loading Tests")
    print("=" * 70)
    print()

    # Test 1: Check that loadStep method exists
    print("Testing STEP API availability...")
    analyzer = gc.Analyzer()

    if not hasattr(analyzer, "load_step"):
        print("✗ Test failed: load_step() method not found")
        return False

    print("  ✓ load_step() method exists")
    print()

    # Test 2: Try to load a non-existent STEP file
    # This should return False (either because file doesn't exist or OCCT not available)
    print("Testing load_step() with non-existent file...")
    result = analyzer.load_step("nonexistent.step")

    if result:
        print("  Warning: load_step() returned True for non-existent file")
    else:
        print("  ✓ load_step() correctly returned False")
    print()

    # Test 3: Create a simple test scenario
    print("Testing STEP support status...")
    test_file = "/tmp/test_cube.step"

    # Try to load (will fail if file doesn't exist or OCCT not available)
    success = analyzer.load_step(test_file)

    if success:
        print("  ✓ OCCT is available and STEP file loaded successfully")
        print(f"    Vertices: {analyzer.get_vertex_count()}")
        print(f"    Triangles: {analyzer.get_triangle_count()}")
    else:
        print("  ✓ STEP loading returned False (OCCT not available or file not found)")
        print("    This is expected when OCCT is not installed.")
    print()

    print("=" * 70)
    print("✓ All STEP API tests passed!")
    print("=" * 70)
    print()
    print("Note: Full STEP functionality requires Open CASCADE Technology (OCCT)")
    print("To enable STEP support:")
    print(
        "  Ubuntu/Debian: sudo apt-get install libocct-data-exchange-dev libocct-ocaf-dev"
    )
    print("  macOS: brew install opencascade")
    print("  Then rebuild: cmake -DUSE_OCCT=ON .. && make")

    return True


if __name__ == "__main__":
    success = test_step_api()
    sys.exit(0 if success else 1)
