#!/usr/bin/env python3
"""
Differential tests: geom-core vs the yantra4d reference implementation.

geom-core's Analyzer.assess_integrity is a C++ port of yantra4d's
`apps/api/services/engine/mesh_integrity.py`. The reference algorithm is
copied below, verbatim apart from formatting, as the oracle — importing it
would need a yantra4d checkout. Provenance: ported from yantra4d's mesh
integrity assessment (AGPL-3.0, madfam-org/yantra4d @52fbb52a) and relicensed
into this Apache-2.0 repo — MADFAM holds the whole copyright (100 %: no
external contributor, no CLA), the reasoning recorded for the Sim4D ingestion.

Fixtures are generated deterministically with trimesh (no yantra4d, no
cadquery), written as binary STL, then read by both sides: trimesh.load for
the oracle, Analyzer.load_stl for geom-core. Also checks surface area,
centroid, body count, volume and bounding box against trimesh.

Requires numpy and trimesh (CI installs them; they are not runtime deps).
"""

from __future__ import annotations

import math
import os
import sys
import tempfile
from dataclasses import dataclass, field

import numpy as np
import trimesh

build_dir = os.path.join(os.path.dirname(os.path.dirname(__file__)), "build", "python")
if os.path.exists(build_dir) and build_dir not in sys.path:
    sys.path.insert(0, build_dir)

import geom_core_py  # noqa: E402

# ---------------------------------------------------------------------------
# Oracle: yantra4d apps/api/services/engine/mesh_integrity.py @52fbb52a
# ---------------------------------------------------------------------------


@dataclass
class MeshIntegrity:
    """Verdict plus the evidence behind it."""

    watertight: bool
    boundary_edges: int
    euler_number: int | None
    winding_consistent: bool
    volume: float
    nonmanifold_edges: int = 0
    merge_tolerance: float | None = None
    notes: list[str] = field(default_factory=list)

    @property
    def summary(self) -> str:
        if self.watertight:
            tol = (
                f" after merging at {self.merge_tolerance:g}mm"
                if self.merge_tolerance
                else ""
            )
            return f"watertight{tol} (volume {self.volume:.2f})"
        if self.boundary_edges == 0 and self.nonmanifold_edges > 0:
            return (
                f"NOT watertight — no holes, but {self.nonmanifold_edges} "
                f"non-manifold edge(s) shared by more than two faces "
                f"(euler {self.euler_number})"
            )
        return (
            f"NOT watertight — {self.boundary_edges} boundary edge(s), "
            f"{self.nonmanifold_edges} non-manifold edge(s), "
            f"euler {self.euler_number}, winding "
            f"{'consistent' if self.winding_consistent else 'INCONSISTENT'}"
        )


def _edge_face_counts(mesh):
    try:
        edges = np.sort(mesh.edges_sorted, axis=1)
        _, counts = np.unique(edges, axis=0, return_counts=True)
        return int((counts == 1).sum()), int((counts > 2).sum())
    except Exception:
        return -1, -1


def assess(mesh, scale_hint=None):
    notes = []

    if not isinstance(mesh, trimesh.Trimesh):
        return MeshIntegrity(
            False,
            -1,
            None,
            False,
            0.0,
            notes=[f"not a triangle mesh: {type(mesh).__name__}"],
        )
    if len(mesh.faces) == 0:
        return MeshIntegrity(False, 0, None, False, 0.0, notes=["mesh has no faces"])

    if mesh.is_watertight:
        return MeshIntegrity(
            True,
            0,
            int(mesh.euler_number),
            bool(mesh.is_winding_consistent),
            float(mesh.volume),
        )

    extent = scale_hint or float(np.max(mesh.extents) if len(mesh.extents) else 1.0)
    for rel in (1e-8, 1e-7, 1e-6, 1e-5):
        tol = max(extent * rel, 1e-9)
        candidate = mesh.copy()
        digits = max(0, round(float(-np.log10(tol))))
        try:
            candidate.merge_vertices(digits_vertex=digits)
        except TypeError:
            candidate.merge_vertices()
        if candidate.is_watertight:
            notes.append(
                f"sealed after merging coincident vertices at {tol:g}mm "
                f"({len(mesh.vertices) - len(candidate.vertices)} merged) — "
                f"STL float precision, not a hole in the model"
            )
            return MeshIntegrity(
                True,
                0,
                int(candidate.euler_number),
                bool(candidate.is_winding_consistent),
                float(candidate.volume),
                merge_tolerance=tol,
                notes=notes,
            )

    boundary, nonmanifold = _edge_face_counts(mesh)
    if boundary > 0:
        notes.append(
            f"{boundary} edge(s) belong to only one face, so the surface "
            f"does not close"
        )
    if nonmanifold > 0:
        notes.append(
            f"{nonmanifold} edge(s) are shared by more than two faces — "
            f"the surface is closed but self-touching, typically "
            f"coincident coplanar faces left by a boolean union"
        )
    if not mesh.is_winding_consistent:
        notes.append(
            "face winding is inconsistent, which can invert normals and "
            "break slicing even where the surface closes"
        )
    return MeshIntegrity(
        False,
        boundary,
        int(mesh.euler_number),
        bool(mesh.is_winding_consistent),
        float(mesh.volume),
        nonmanifold_edges=nonmanifold,
        notes=notes,
    )


# ---------------------------------------------------------------------------
# Oracle for the fields geom-core adds (not in the reference)
# ---------------------------------------------------------------------------


def _judged_mesh(mesh, ref):
    """The mesh the verdict was reached on: merged when a merge sealed it."""
    if not ref.merge_tolerance:
        return mesh
    candidate = mesh.copy()
    candidate.merge_vertices(
        digits_vertex=max(0, round(float(-np.log10(ref.merge_tolerance))))
    )
    return candidate


def _body_count(mesh):
    """Connected components of faces sharing a vertex (trimesh's body_count
    needs scipy; this plain union-find is the independent oracle)."""
    parent = list(range(len(mesh.vertices)))

    def root(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i

    for a, b, c in mesh.faces:
        for other in (b, c):
            ra, rb = root(a), root(other)
            if ra != rb:
                parent[rb] = ra
    referenced = set(int(i) for i in np.unique(mesh.faces))
    return len({root(i) for i in referenced})


def _additions(mesh, ref, scale_hint=None):
    judged = _judged_mesh(mesh, ref)
    edges = np.sort(judged.edges_sorted, axis=1)
    _, counts = np.unique(edges, axis=0, return_counts=True)
    extent = scale_hint or float(np.max(mesh.extents))
    faces = judged.faces
    repeated = (
        (faces[:, 0] == faces[:, 1])
        | (faces[:, 1] == faces[:, 2])
        | (faces[:, 2] == faces[:, 0])
    )
    flat = judged.area_faces * 2.0 <= 1e-12 * extent * extent
    return {
        "manifold_edges": int((counts == 2).sum()),
        "degenerate_faces": int((repeated | flat).sum()),
        "inverted": bool(ref.watertight and ref.winding_consistent and ref.volume < 0),
        "body_count": _body_count(judged),
    }


# ---------------------------------------------------------------------------
# Fixtures (deterministic, trimesh only)
# ---------------------------------------------------------------------------


def _box(extent=20.0):
    return trimesh.creation.box(extents=(extent, extent, extent))


def _soup(mesh, jitter=0.0, seed=0):
    """Loose triangles, optionally with per-copy noise (an STL's float noise)."""
    tris = mesh.vertices[mesh.faces].reshape(-1, 3)
    if jitter:
        tris = tris + np.random.default_rng(seed).uniform(-jitter, jitter, tris.shape)
    return trimesh.Trimesh(
        vertices=tris,
        faces=np.arange(len(tris)).reshape(-1, 3),
        process=False,
    )


def _raw(vertices, faces):
    return trimesh.Trimesh(vertices=vertices, faces=faces, process=False)


def fixtures():
    box = _box()

    open_box = box.copy()
    open_box.update_faces(open_box.face_normals[:, 2] < 0.5)  # drop the +z side

    other = _box()
    other.apply_translation((50, 0, 0))
    two_body = trimesh.util.concatenate([box, other])

    # Box plus a zero-area triangle on an existing edge: one non-manifold edge,
    # two boundary edges, one degenerate face.
    v = np.vstack([box.vertices, (box.vertices[0] + box.vertices[1]) / 2.0])
    on_edge = _raw(v, np.vstack([box.faces, [[0, 1, len(v) - 1]]]))

    # Box plus a collinear sliver far away: degenerate, separate body.
    sliver_v = np.vstack([box.vertices, [[40, 0, 0], [41, 0, 0], [42, 0, 0]]])
    sliver = _raw(sliver_v, np.vstack([box.faces, [[8, 9, 10]]]))

    flipped_all = _raw(box.vertices, box.faces[:, ::-1])
    one_flipped_faces = box.faces.copy()
    one_flipped_faces[0] = one_flipped_faces[0][::-1]
    flipped_one = _raw(box.vertices, one_flipped_faces)

    a = trimesh.creation.box(extents=(10, 10, 10))
    b = trimesh.creation.box(extents=(10, 10, 10))
    b.apply_translation((10, 0, 0))
    sharing_face = trimesh.util.concatenate([a, b])  # gridfinity's signature

    c = trimesh.creation.box(extents=(10, 10, 10))
    c.apply_translation((10, 10, 0))
    sharing_edge = trimesh.util.concatenate([a, c])  # bow-tie edge

    return {
        "closed_box": box,
        "open_box": open_box,
        "two_body": two_body,
        "degenerate_on_edge": on_edge,
        "degenerate_sliver": sliver,
        "flipped_all": flipped_all,
        "flipped_one": flipped_one,
        "sharing_face": sharing_face,
        "sharing_edge": sharing_edge,
        "icosphere": trimesh.creation.icosphere(subdivisions=3, radius=7.5),
        "cylinder": trimesh.creation.cylinder(radius=3.0, height=12.0, sections=48),
        "torus": trimesh.creation.torus(major_radius=10.0, minor_radius=3.0),
        "tiny_box": _box(0.5),
        "large_box": _box(500.0),
        "soup_jitter_3e-6": _soup(box, jitter=3e-6),
        "soup_jitter_1e-3": _soup(box, jitter=1e-3),
        "tiny_soup_jitter": _soup(_box(2.0), jitter=4e-7, seed=1),
    }


# ---------------------------------------------------------------------------
# Comparison
# ---------------------------------------------------------------------------


def _close(a, b, rel=1e-9, abs_tol=1e-6):
    return math.isclose(a, b, rel_tol=rel, abs_tol=abs_tol)


def compare(name, mesh, workdir, scale_hint=None):
    path = os.path.join(workdir, f"{name}.stl")
    mesh.export(path, file_type="stl")

    loaded = trimesh.load(path, file_type="stl", force="mesh")
    ref = assess(loaded, scale_hint=scale_hint)
    extra = _additions(loaded, ref, scale_hint)

    analyzer = geom_core_py.Analyzer()
    assert analyzer.load_stl(path), name
    got = analyzer.assess_integrity(scale_hint)

    where = f"{name}: geom-core {got.summary!r} vs reference {ref.summary!r}"
    assert analyzer.get_triangle_count() == len(loaded.faces), where
    assert got.watertight == ref.watertight, where
    assert got.boundary_edges == ref.boundary_edges, where
    assert got.nonmanifold_edges == ref.nonmanifold_edges, where
    assert got.euler_number == ref.euler_number, where
    assert got.winding_consistent == ref.winding_consistent, where
    assert _close(got.volume, ref.volume), f"{where}: {got.volume} vs {ref.volume}"
    if ref.merge_tolerance is None:
        assert got.merge_tolerance is None, where
    else:
        assert _close(got.merge_tolerance, ref.merge_tolerance, abs_tol=0.0), where
    assert list(got.notes[: len(ref.notes)]) == ref.notes, f"{where}: {got.notes}"
    assert got.summary == ref.summary, where
    for key, value in extra.items():
        assert (
            getattr(got, key) == value
        ), f"{where}: {key} {getattr(got, key)} != {value}"

    # Scalars on the loaded mesh against trimesh.
    assert _close(analyzer.get_surface_area(), float(loaded.area)), name
    assert analyzer.get_body_count() == _body_count(loaded), name
    bbox = analyzer.get_bounding_box()
    assert np.allclose(
        [bbox.x, bbox.y, bbox.z], loaded.extents, rtol=1e-9, atol=1e-9
    ), name
    # Volume and centroid are only meaningful for a closed, consistently wound
    # surface; otherwise the origin-tetrahedron and trimesh flux integrals
    # disagree by design.
    # A soup sealed only by merging still has per-copy noise in the loaded
    # mesh, so its integrals agree to the noise level, not to rounding.
    solid = ref.watertight and ref.winding_consistent
    rel, atol = (1e-9, 1e-6) if ref.merge_tolerance is None else (1e-6, 1e-4)
    if solid:
        assert _close(analyzer.get_volume(), abs(ref.volume), rel=rel), name
    if solid and abs(ref.volume) > 1e-9:
        c = analyzer.get_centroid()
        assert np.allclose(
            [c.x, c.y, c.z], loaded.center_mass, rtol=rel, atol=atol
        ), f"{name}: centroid {c} vs {loaded.center_mass}"
    return got, ref


def test_differential_against_yantra4d_reference():
    with tempfile.TemporaryDirectory() as workdir:
        seen_watertight = seen_open = seen_merge = seen_nonmanifold = 0
        for name, mesh in fixtures().items():
            got, ref = compare(name, mesh, workdir)
            seen_watertight += ref.watertight
            seen_open += ref.boundary_edges > 0
            seen_merge += ref.merge_tolerance is not None
            seen_nonmanifold += ref.nonmanifold_edges > 0
            print(f"  ✓ {name}: {got.summary}")
        # The fixture set must exercise every branch of the reference.
        assert seen_watertight and seen_open and seen_merge and seen_nonmanifold


def test_scale_hint_matches_reference():
    with tempfile.TemporaryDirectory() as workdir:
        soup = _soup(_box(200.0), jitter=2e-5, seed=2)
        for hint in (None, 200.0, 2.0, 0.0):
            got, ref = compare(f"scale_hint_{hint}", soup, workdir, scale_hint=hint)
            print(f"  ✓ scale_hint={hint}: {got.summary}")


def test_expected_verdicts():
    """Spot-check the fixtures mean what their names say (on both sides)."""
    with tempfile.TemporaryDirectory() as workdir:
        f = fixtures()
        got, _ = compare("closed_box", f["closed_box"], workdir)
        assert got.watertight and got.euler_number == 2 and got.body_count == 1
        got, _ = compare("open_box", f["open_box"], workdir)
        assert not got.watertight and got.boundary_edges == 4
        got, _ = compare("two_body", f["two_body"], workdir)
        assert got.watertight and got.body_count == 2
        got, _ = compare("flipped_all", f["flipped_all"], workdir)
        assert got.inverted and got.volume < 0
        got, _ = compare("flipped_one", f["flipped_one"], workdir)
        assert got.watertight and not got.winding_consistent and not got.inverted
        got, _ = compare("sharing_face", f["sharing_face"], workdir)
        assert got.boundary_edges == 0 and got.nonmanifold_edges > 0
        got, _ = compare("degenerate_sliver", f["degenerate_sliver"], workdir)
        assert got.degenerate_faces == 1 and got.body_count == 2
        got, _ = compare("torus", f["torus"], workdir)
        assert got.watertight and got.euler_number == 0
        got, _ = compare("soup_jitter_3e-6", f["soup_jitter_3e-6"], workdir)
        assert got.watertight and got.merge_tolerance is not None


def main():
    print("geom-core integrity: differential tests against the yantra4d reference")
    test_differential_against_yantra4d_reference()
    test_scale_hint_matches_reference()
    test_expected_verdicts()
    print("✓ All differential integrity tests passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
