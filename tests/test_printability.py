#!/usr/bin/env python3
"""
Test suite for Milestone 4: Printability Analysis

Tests the overhang detection and wall thickness analysis.
"""

import sys
import os
import struct
import tempfile

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


def write_binary_stl_thin_plate(filepath, width=10.0, length=10.0, thickness=0.1):
    """
    Generate a thin rectangular plate (very thin walls).
    This will trigger wall thickness warnings.
    """
    # Define 8 vertices of a thin plate
    half_w = width / 2.0
    half_l = length / 2.0
    half_t = thickness / 2.0

    vertices = [
        (-half_w, -half_l, -half_t),  # 0: bottom-left-bottom
        (half_w, -half_l, -half_t),  # 1: bottom-right-bottom
        (half_w, half_l, -half_t),  # 2: top-right-bottom
        (-half_w, half_l, -half_t),  # 3: top-left-bottom
        (-half_w, -half_l, half_t),  # 4: bottom-left-top
        (half_w, -half_l, half_t),  # 5: bottom-right-top
        (half_w, half_l, half_t),  # 6: top-right-top
        (-half_w, half_l, half_t),  # 7: top-left-top
    ]

    # 12 triangles (same as cube)
    triangles = [
        (0, 2, 1),
        (0, 3, 2),  # Bottom
        (4, 5, 6),
        (4, 6, 7),  # Top
        (0, 4, 7),
        (0, 7, 3),  # Left
        (1, 6, 5),
        (1, 2, 6),  # Right
        (0, 1, 5),
        (0, 5, 4),  # Front
        (3, 6, 2),
        (3, 7, 6),  # Back
    ]

    with open(filepath, "wb") as f:
        header_text = b"Binary STL thin plate for thickness test"
        f.write(header_text + b"\x00" * (80 - len(header_text)))
        f.write(struct.pack("<I", len(triangles)))

        for tri in triangles:
            v0, v1, v2 = vertices[tri[0]], vertices[tri[1]], vertices[tri[2]]
            edge1 = (v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2])
            edge2 = (v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2])
            normal = (
                edge1[1] * edge2[2] - edge1[2] * edge2[1],
                edge1[2] * edge2[0] - edge1[0] * edge2[2],
                edge1[0] * edge2[1] - edge1[1] * edge2[0],
            )
            f.write(struct.pack("<fff", *normal))
            f.write(struct.pack("<fff", *v0))
            f.write(struct.pack("<fff", *v1))
            f.write(struct.pack("<fff", *v2))
            f.write(struct.pack("<H", 0))


def write_binary_stl_inverted_pyramid(filepath, base_size=10.0, height=5.0):
    """
    Generate an inverted pyramid (apex at top, base at bottom).
    All side faces slope downward and outward - severe overhangs!
    """
    half = base_size / 2.0

    vertices = [
        (0, 0, height),  # 0: apex (TOP - point)
        (-half, -half, 0),  # 1: base corner (bottom)
        (half, -half, 0),  # 2: base corner (bottom)
        (half, half, 0),  # 3: base corner (bottom)
        (-half, half, 0),  # 4: base corner (bottom)
    ]

    # Ensure counter-clockwise winding from outside (normals point outward)
    triangles = [
        # Four triangular faces (all overhangs) - face normals point outward and downward
        (0, 2, 1),  # Front face
        (0, 3, 2),  # Right face
        (0, 4, 3),  # Back face
        (0, 1, 4),  # Left face
        # Base (two triangles) - facing downward (also an overhang!)
        (1, 2, 3),  # Base triangle 1 (facing down)
        (1, 3, 4),  # Base triangle 2 (facing down)
    ]

    with open(filepath, "wb") as f:
        header_text = b"Binary STL inverted pyramid for overhang test"
        f.write(header_text + b"\x00" * (80 - len(header_text)))
        f.write(struct.pack("<I", len(triangles)))

        for tri in triangles:
            v0, v1, v2 = vertices[tri[0]], vertices[tri[1]], vertices[tri[2]]
            edge1 = (v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2])
            edge2 = (v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2])
            normal = (
                edge1[1] * edge2[2] - edge1[2] * edge2[1],
                edge1[2] * edge2[0] - edge1[0] * edge2[2],
                edge1[0] * edge2[1] - edge1[1] * edge2[0],
            )
            f.write(struct.pack("<fff", *normal))
            f.write(struct.pack("<fff", *v0))
            f.write(struct.pack("<fff", *v1))
            f.write(struct.pack("<fff", *v2))
            f.write(struct.pack("<H", 0))


def test_thin_wall_detection():
    """Test wall thickness analysis on a very thin plate."""
    print("\nTesting wall thickness detection...")

    with tempfile.NamedTemporaryFile(suffix=".stl", delete=False) as f:
        temp_file = f.name

    try:
        # Create a plate that's only 0.1mm thick
        write_binary_stl_thin_plate(temp_file, width=10.0, length=10.0, thickness=0.1)

        analyzer = geom_core_py.Analyzer()
        success = analyzer.load_stl(temp_file)
        assert success, "Failed to load thin plate"

        print(f"  ✓ Loaded thin plate: {analyzer.get_vertex_count()} vertices")

        # Build spatial index (required for thickness analysis)
        analyzer.build_spatial_index()
        print(f"  ✓ Built spatial index")

        # Analyze with minimum thickness of 0.2mm (plate is 0.1mm)
        report = analyzer.get_printability_report(45.0, 0.2)

        print(f"  ✓ Printability score: {report.score:.1f}/100")
        print(f"  ✓ Thin wall vertices: {report.thin_wall_vertex_count}")
        print(f"  ✓ Overhang area: {report.overhang_area:.2f} mm²")
        print(f"  ✓ Overhang percentage: {report.overhang_percentage:.1f}%")

        # The thin plate should trigger wall thickness warnings
        assert (
            report.thin_wall_vertex_count > 0
        ), f"Expected thin walls to be detected, but got {report.thin_wall_vertex_count}"

        print(f"  ✓ Thin walls correctly detected!")

    finally:
        if os.path.exists(temp_file):
            os.remove(temp_file)


def write_simple_overhang(filepath):
    """
    Create a simple overhang: one triangle facing straight down.
    This is the simplest possible test case.
    """
    # Three vertices forming a horizontal triangle at z=5, facing DOWN
    # To make it face down, we need the normal to point in -Z direction
    # Right-hand rule: edge1 x edge2 gives normal
    # For downward normal, use clockwise winding when viewed from above
    vertices = [
        (0, 0, 5),  # 0
        (10, 0, 5),  # 1
        (5, 10, 5),  # 2
    ]

    # ONE triangle - clockwise winding (viewed from above) = downward normal
    # edge1 = v2-v0 = (5,10,0), edge2 = v1-v0 = (10,0,0)
    # edge1 x edge2 = (0, 0, -100) -> points DOWN!
    triangles = [
        (0, 2, 1),  # Reversed winding for downward normal
    ]

    with open(filepath, "wb") as f:
        header_text = b"Simple downward triangle"
        f.write(header_text + b"\x00" * (80 - len(header_text)))
        f.write(struct.pack("<I", len(triangles)))

        for tri in triangles:
            v0, v1, v2 = vertices[tri[0]], vertices[tri[1]], vertices[tri[2]]
            # Let STL parser calculate the normal
            edge1 = (v1[0] - v0[0], v1[1] - v0[1], v1[2] - v0[2])
            edge2 = (v2[0] - v0[0], v2[1] - v0[1], v2[2] - v0[2])
            # Cross product gives normal
            normal = (
                edge1[1] * edge2[2] - edge1[2] * edge2[1],
                edge1[2] * edge2[0] - edge1[0] * edge2[2],
                edge1[0] * edge2[1] - edge1[1] * edge2[0],
            )
            f.write(struct.pack("<fff", *normal))
            f.write(struct.pack("<fff", *v0))
            f.write(struct.pack("<fff", *v1))
            f.write(struct.pack("<fff", *v2))
            f.write(struct.pack("<H", 0))


def test_overhang_detection():
    """Test overhang detection on a simple downward-facing triangle."""
    print("\nTesting overhang detection...")

    with tempfile.NamedTemporaryFile(suffix=".stl", delete=False) as f:
        temp_file = f.name

    try:
        # Create a simple downward-facing triangle
        write_simple_overhang(temp_file)

        analyzer = geom_core_py.Analyzer()
        success = analyzer.load_stl(temp_file)
        assert success, "Failed to load overhang test"

        print(f"  ✓ Loaded test geometry: {analyzer.get_triangle_count()} triangles")

        # Don't need spatial index for overhang analysis
        # Use 45° critical angle
        report = analyzer.get_printability_report(45.0, 0.8)

        print(f"  ✓ Printability score: {report.score:.1f}/100")
        print(f"  ✓ Total surface area: {report.total_surface_area:.2f} mm²")
        print(f"  ✓ Overhang area: {report.overhang_area:.2f} mm²")
        print(f"  ✓ Overhang percentage: {report.overhang_percentage:.1f}%")

        # A horizontal triangle facing down should be 100% overhang
        # (angle from vertical is 90°, well above 45° threshold)
        assert (
            report.overhang_percentage > 50.0
        ), f"Expected significant overhangs, but got {report.overhang_percentage:.1f}%"

        # Score should be penalized
        assert (
            report.score < 90.0
        ), f"Expected lower score due to overhangs, but got {report.score:.1f}"

        print(f"  ✓ Overhangs correctly detected!")

    finally:
        if os.path.exists(temp_file):
            os.remove(temp_file)


def test_printability_report_structure():
    """Test PrintabilityReport struct fields."""
    print("\nTesting PrintabilityReport structure...")

    report = geom_core_py.PrintabilityReport()

    assert hasattr(report, "overhang_area")
    assert hasattr(report, "overhang_percentage")
    assert hasattr(report, "thin_wall_vertex_count")
    assert hasattr(report, "score")
    assert hasattr(report, "total_surface_area")

    # Default values
    assert report.score == 100.0
    assert report.overhang_area == 0.0
    assert report.thin_wall_vertex_count == 0

    print(f"  ✓ PrintabilityReport struct has all required fields")
    print(f"  ✓ Default score: {report.score}")


def main():
    """Run all printability tests."""
    print("=" * 70)
    print("geom-core Milestone 4: Printability Analysis Tests")
    print("=" * 70)

    try:
        test_printability_report_structure()
        test_overhang_detection()
        test_thin_wall_detection()

        print("\n" + "=" * 70)
        print("✓ All Milestone 4 printability tests passed!")
        print("=" * 70)
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
