/*
 * Mesh integrity assessment.
 *
 * Ported from yantra4d's mesh integrity assessment (AGPL-3.0,
 * madfam-org/yantra4d @52fbb52a, apps/api/services/engine/mesh_integrity.py:
 * MeshIntegrity, _edge_face_counts, assess) and relicensed into this
 * Apache-2.0 repo — MADFAM holds the whole copyright (100 %: no external
 * contributor has committed to yantra4d and there is no CLA), the same
 * reasoning recorded for the Sim4D ingestion
 * (yantra4d docs/architecture/sim4d-extraction.md, "Licensing").
 *
 * Added here, beyond the reference: manifoldEdges, degenerateFaces,
 * inverted and bodyCount.
 */
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace madfam::geom {

class Mesh;

/**
 * @brief Verdict plus the evidence behind it.
 *
 * Field semantics follow yantra4d's MeshIntegrity:
 * - watertight: every edge is shared by exactly two faces, either as loaded
 *   or after merging coincident vertices at a scale-relative tolerance.
 * - boundaryEdges / nonManifoldEdges: edges used by one face (holes) and by
 *   three or more faces (self-touching surfaces). Counted on the mesh as
 *   loaded when no merge seals it; 0 / 0 when it is watertight.
 * - eulerNumber: referenced vertices - unique edges + faces (absent for an
 *   empty mesh).
 * - windingConsistent: every edge shared by exactly two faces is traversed
 *   in opposite directions by them.
 * - volume: signed volume (negative for an inside-out mesh).
 * - mergeTolerance: the tolerance (model units) that sealed the mesh, absent
 *   when it was already sealed as loaded or never sealed.
 * Additions:
 * - manifoldEdges: edges shared by exactly two faces.
 * - degenerateFaces: faces with a repeated vertex index or twice-area
 *   <= 1e-12 * extent².
 * - inverted: watertight, consistently wound and negative volume.
 * - bodyCount: connected components of faces sharing a vertex.
 * All counts are taken on the mesh the verdict was reached on (merged when a
 * merge sealed it, otherwise as loaded).
 */
struct MeshIntegrity {
    bool watertight = false;
    int boundaryEdges = 0;
    int nonManifoldEdges = 0;
    int manifoldEdges = 0;
    std::optional<int> eulerNumber;
    bool windingConsistent = false;
    double volume = 0.0;
    std::optional<double> mergeTolerance;
    int degenerateFaces = 0;
    bool inverted = false;
    int bodyCount = 0;
    std::vector<std::string> notes;

    /** One-line verdict that names which failure it is (yantra4d `summary`). */
    std::string summary() const;
};

/**
 * @brief Decide whether a mesh encloses a volume, merging coincident vertices first.
 *
 * @param mesh The mesh to assess (not modified).
 * @param scaleHint Characteristic model size in model units (mm). Absent or
 *        0 derives it from the largest bounding-box dimension, so a 2 mm part
 *        and a 200 mm part are judged on the same relative precision. The
 *        merge is retried at extent * {1e-8, 1e-7, 1e-6, 1e-5} (at least
 *        1e-9), rounding coordinates to round(-log10(tol)) decimal digits
 *        exactly as trimesh.merge_vertices(digits_vertex=...) does.
 */
MeshIntegrity assessMeshIntegrity(const Mesh& mesh,
                                  std::optional<double> scaleHint = std::nullopt);

} // namespace madfam::geom
