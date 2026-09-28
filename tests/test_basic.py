#!/usr/bin/env python3
"""
Basic sanity test for geom-core Python bindings.
Tests the core Analyzer class to ensure the build pipeline works.
"""

import sys
import os

# Add the build directory to Python path if not already set
build_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "build", "python")
if os.path.exists(build_dir) and build_dir not in sys.path:
    sys.path.insert(0, build_dir)

try:
    import geom_core_py
except ImportError as e:
    print(f"ERROR: Failed to import geom_core_py: {e}")
    print(f"Make sure to build the project first: ./scripts/build_python.sh")
    print(f"Or set PYTHONPATH to the build directory")
    sys.exit(1)


def test_add():
    """Test the simple add function."""
    print("Testing Analyzer.add()...")
    analyzer = geom_core_py.Analyzer()
    result = analyzer.add(1, 2)
    assert result == 3, f"Expected 3, got {result}"
    print(f"  ✓ add(1, 2) = {result}")


def test_load_data():
    """Test the loadData placeholder function."""
    print("\nTesting Analyzer.load_data()...")
    analyzer = geom_core_py.Analyzer()
    result = analyzer.load_data("test_model.step")
    assert result == True, f"Expected True, got {result}"
    print(f"  ✓ load_data() returned {result}")


def test_mock_volume():
    """Test the getMockVolume function (sphere volume formula)."""
    print("\nTesting Analyzer.get_mock_volume()...")
    analyzer = geom_core_py.Analyzer()

    # Test with radius = 1.0
    # Expected: (4/3) * PI * 1^3 ≈ 4.189
    radius = 1.0
    volume = analyzer.get_mock_volume(radius)
    expected = 4.18879020478639
    tolerance = 0.0001

    assert abs(volume - expected) < tolerance, f"Expected ~{expected}, got {volume}"
    print(f"  ✓ get_mock_volume({radius}) = {volume:.6f}")

    # Test with radius = 2.0
    radius = 2.0
    volume = analyzer.get_mock_volume(radius)
    expected = 33.5103216382911
    assert abs(volume - expected) < tolerance, f"Expected ~{expected}, got {volume}"
    print(f"  ✓ get_mock_volume({radius}) = {volume:.6f}")


def main():
    """Run all tests."""
    print("=" * 50)
    print("geom-core Python Bindings - Basic Tests")
    print("=" * 50)

    try:
        test_add()
        test_load_data()
        test_mock_volume()

        print("\n" + "=" * 50)
        print("✓ All tests passed!")
        print("=" * 50)
        return 0

    except AssertionError as e:
        print(f"\n✗ Test failed: {e}")
        return 1
    except Exception as e:
        print(f"\n✗ Unexpected error: {e}")
        import traceback

        traceback.print_exc()
        return 1


if __name__ == "__main__":
    sys.exit(main())
