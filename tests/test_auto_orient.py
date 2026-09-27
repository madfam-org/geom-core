#!/usr/bin/env python3
"""
Test auto-orientation optimization (Milestone 5)

Goal: The system should identify that a cylinder lying on its side
should be stood upright to minimize support structures.
"""

import sys
import os
import struct
import math

# Import the compiled module
try:
    import geom_core_py as gc
except ImportError:
    print("ERROR: Could not import geom_core_py module")
    print("Make sure to build the project first with: cmake .. && make")
    sys.exit(1)


def write_binary_stl_sideways_cylinder(filepath, radius=5.0, height=20.0, segments=16):
    """
    Create a cylinder lying on its side (along X-axis).

    When standing upright (Y-axis up), this cylinder has minimal overhangs.
    When lying down (Z-axis up), the sides are severe overhangs.

    Args:
        radius: Cylinder radius (mm)
        height: Cylinder height (mm)
        segments: Number of radial segments
    """
    triangles = []

    # Generate cylinder vertices lying along X-axis
    # Bottom cap at x=0, top cap at x=height
    # Circle in YZ plane

    # Bottom cap center: (0, 0, 0)
    # Top cap center: (height, 0, 0)

    for i in range(segments):
        angle1 = (2.0 * math.pi * i) / segments
        angle2 = (2.0 * math.pi * (i + 1)) / segments

        y1 = radius * math.cos(angle1)
        z1 = radius * math.sin(angle1)
        y2 = radius * math.cos(angle2)
        z2 = radius * math.sin(angle2)

        # Bottom cap (at x=0)
        # Triangle from center to edge (normal pointing in -X direction)
        triangles.append(
            {
                "normal": (-1, 0, 0),
                "vertices": [
                    (0, 0, 0),  # Center
                    (0, y2, z2),  # Edge point 2
                    (0, y1, z1),  # Edge point 1
                ],
            }
        )

        # Top cap (at x=height)
        # Triangle from center to edge (normal pointing in +X direction)
        triangles.append(
            {
                "normal": (1, 0, 0),
                "vertices": [
                    (height, 0, 0),  # Center
                    (height, y1, z1),  # Edge point 1
                    (height, y2, z2),  # Edge point 2
                ],
            }
        )

        # Side face (two triangles per segment)
        # Calculate outward normal (perpendicular to X-axis)
        normal_y = math.cos((angle1 + angle2) / 2.0)
        normal_z = math.sin((angle1 + angle2) / 2.0)

        # First triangle of side quad
        triangles.append(
            {
                "normal": (0, normal_y, normal_z),
                "vertices": [
                    (0, y1, z1),  # Bottom edge 1
                    (height, y1, z1),  # Top edge 1
                    (0, y2, z2),  # Bottom edge 2
                ],
            }
        )

        # Second triangle of side quad
        triangles.append(
            {
                "normal": (0, normal_y, normal_z),
                "vertices": [
                    (0, y2, z2),  # Bottom edge 2
                    (height, y1, z1),  # Top edge 1
                    (height, y2, z2),  # Top edge 2
                ],
            }
        )

    # Write binary STL
    with open(filepath, "wb") as f:
        # Header (80 bytes)
        header_text = b"Sideways cylinder for auto-orient test"
        f.write(header_text + b"\x00" * (80 - len(header_text)))

        # Triangle count
        f.write(struct.pack("<I", len(triangles)))

        # Triangles
        for tri in triangles:
            # Normal vector (3 floats)
            f.write(struct.pack("<fff", *tri["normal"]))

            # Vertices (3 vertices × 3 floats each)
            for vertex in tri["vertices"]:
                f.write(struct.pack("<fff", *vertex))

            # Attribute byte count (uint16, always 0)
            f.write(struct.pack("<H", 0))

    print(f"Created sideways cylinder: {len(triangles)} triangles")


def test_auto_orientation():
    """Test that auto-orientation stands up a sideways cylinder"""

    print("=" * 70)
    print("geom-core Milestone 5: Auto-Orientation Test")
    print("=" * 70)
    print()

    # Create test geometry
    temp_file = "/tmp/test_sideways_cylinder.stl"
    write_binary_stl_sideways_cylinder(temp_file, radius=5.0, height=20.0, segments=16)

    try:
        # Load mesh
        analyzer = gc.Analyzer()
        success = analyzer.load_stl(temp_file)

        if not success:
            print("✗ Test failed: Could not load STL file")
            return False

        print(
            f"Loaded sideways cylinder: {analyzer.get_vertex_count()} vertices, "
            f"{analyzer.get_triangle_count()} triangles"
        )
        print()

        # Test 1: Verify original orientation has poor printability
        print("Testing original orientation (cylinder lying on side, Z-up)...")
        original_report = analyzer.get_printability_report(
            critical_angle_degrees=45.0, min_wall_thickness_mm=0.8
        )

        print(f"  Original overhang area: {original_report.overhang_area:.2f} mm²")
        print(
            f"  Original overhang percentage: {original_report.overhang_percentage:.1f}%"
        )
        print(f"  Original score: {original_report.score:.1f}/100")
        print()

        # Sideways cylinder should have significant overhangs (the curved sides)
        if original_report.overhang_percentage < 10.0:
            print(
                f"✗ Test failed: Expected significant overhangs for sideways cylinder, "
                f"got {original_report.overhang_percentage:.1f}%"
            )
            return False

        print("  ✓ Sideways cylinder has expected overhangs")
        print()

        # Test 2: Run auto-orientation
        print("Running auto-orientation optimization...")
        result = analyzer.auto_orient(sample_resolution=26, critical_angle_degrees=45.0)
        print()

        # Test 3: Verify improvement
        print("Analyzing optimization result:")
        print(f"  Original overhang: {result.original_overhang_area:.2f} mm²")
        print(f"  Optimized overhang: {result.optimized_overhang_area:.2f} mm²")
        print(f"  Improvement: {result.improvement_percent:.1f}%")
        print(
            f"  Optimal up vector: ({result.optimal_up_vector.x:.3f}, "
            f"{result.optimal_up_vector.y:.3f}, {result.optimal_up_vector.z:.3f})"
        )
        print()

        # For a cylinder, there are multiple optimal orientations
        # The key is that we found one that significantly reduces overhangs
        up_vec = result.optimal_up_vector
        print(
            f"  ✓ Found optimal orientation: ({up_vec.x:.3f}, {up_vec.y:.3f}, {up_vec.z:.3f})"
        )

        # The optimized overhang should be significantly less than original
        if result.optimized_overhang_area >= result.original_overhang_area:
            print(f"✗ Test failed: Expected optimization to reduce overhang area")
            print(f"  Original: {result.original_overhang_area:.2f} mm²")
            print(f"  Optimized: {result.optimized_overhang_area:.2f} mm²")
            return False

        print(f"  ✓ Overhang area reduced by {result.improvement_percent:.1f}%")

        # Expect at least 20% improvement for this test case
        if result.improvement_percent < 20.0:
            print(
                f"✗ Test failed: Expected at least 20% improvement, got {result.improvement_percent:.1f}%"
            )
            return False

        print(f"  ✓ Significant improvement achieved")
        print()

        print("=" * 70)
        print("✓ All auto-orientation tests passed!")
        print("=" * 70)

        return True

    finally:
        # Clean up
        if os.path.exists(temp_file):
            os.remove(temp_file)


if __name__ == "__main__":
    success = test_auto_orientation()
    sys.exit(0 if success else 1)
