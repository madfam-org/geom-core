"""Verify STEP tessellation against a generated 10 mm cube."""

import os
from pathlib import Path
import sys
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "build" / "python"))
import geom_core_py  # noqa: E402

if os.environ.get("GEOM_REQUIRE_OCCT") == "1" and not geom_core_py.has_occt:
    raise RuntimeError("CI requires an OCCT-enabled build; STEP geometry is unverified")


@unittest.skipUnless(geom_core_py.has_occt, "STEP geometry requires OCCT")
class STEPGeometryTests(unittest.TestCase):
    def test_tessellated_cube(self):
        analyzer = geom_core_py.Analyzer()
        path = Path(__file__).parent / "fixtures" / "cube-10mm.step"
        self.assertTrue(analyzer.load_step(str(path)))
        self.assertTrue(analyzer.is_watertight())
        self.assertAlmostEqual(analyzer.get_volume(), 1000.0, places=5)
        dimensions = analyzer.get_bounding_box()
        for length in (dimensions.x, dimensions.y, dimensions.z):
            self.assertAlmostEqual(length, 10.0, places=5)


if __name__ == "__main__":
    unittest.main()
