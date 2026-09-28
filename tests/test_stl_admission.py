"""Malformed STL admission and analysis-cache regression checks."""

import os
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "build" / "python"))
import geom_core_py  # noqa: E402
from test_mesh import write_binary_stl_cube  # noqa: E402


class STLAdmissionTests(unittest.TestCase):
    @unittest.skipUnless(sys.platform.startswith("linux"), "RLIMIT_AS requires Linux")
    def test_overflow_count_is_rejected_before_allocation(self):
        # Limit the child so a regressed reserve() cannot exhaust the test host.
        with tempfile.NamedTemporaryFile(suffix=".stl") as mesh:
            mesh.write(bytes(80) + struct.pack("<I", 0x80000000))
            mesh.flush()
            result = subprocess.run(
                [
                    sys.executable,
                    "-c",
                    "import resource,geom_core_py,sys; "
                    "resource.setrlimit(resource.RLIMIT_AS,(512*1024*1024,512*1024*1024)); "
                    "assert geom_core_py.Analyzer().load_stl(sys.argv[1]) is False",
                    mesh.name,
                ],
                env={**os.environ, "PYTHONPATH": sys.path[0]},
                timeout=20,
                capture_output=True,
                text=True,
            )
            self.assertEqual(result.returncode, 0, result.stderr)

    def test_non_finite_coordinates_leave_no_partial_mesh(self):
        for value in (float("nan"), float("inf"), float("-inf")):
            with tempfile.NamedTemporaryFile(suffix=".stl") as mesh:
                triangle = struct.pack("<12fH", *([0.0] * 3 + [value] + [0.0] * 8), 0)
                mesh.write(bytes(80) + struct.pack("<I", 1) + triangle)
                mesh.flush()
                analyzer = geom_core_py.Analyzer()
                self.assertFalse(analyzer.load_stl(mesh.name))
                self.assertEqual(analyzer.get_vertex_count(), 0)
                self.assertEqual(analyzer.get_triangle_count(), 0)

    def test_failed_file_load_clears_previous_mesh(self):
        with tempfile.TemporaryDirectory() as folder:
            path = str(Path(folder) / "cube.stl")
            write_binary_stl_cube(path)
            analyzer = geom_core_py.Analyzer()
            self.assertTrue(analyzer.load_stl(path))
            analyzer.build_spatial_index()
            self.assertFalse(analyzer.load_stl(str(Path(folder) / "missing.stl")))
            self.assertEqual(analyzer.get_vertex_count(), 0)
            self.assertEqual(analyzer.get_triangle_count(), 0)


if __name__ == "__main__":
    unittest.main()
