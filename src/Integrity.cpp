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
 * Why two steps (from the reference): STL stores every triangle with its own
 * copy of each vertex, so a closed solid is only sealed once coincident
 * vertices merge, and whether they merge depends on a floating-point
 * tolerance. Merge at a tolerance relative to the model's own scale, then
 * judge, and report *why* a mesh failed rather than just that it did.
 */
#include "geom-core/Integrity.hpp"
#include "geom-core/Mesh.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <map>
#include <utility>

namespace madfam::geom {

namespace {

using Face = std::array<int, 3>;

struct EdgeStats {
    int boundary = 0;     // edges used by exactly one face
    int nonManifold = 0;  // edges used by three or more faces
    int manifold = 0;     // edges used by exactly two faces
    bool windingConsistent = true;
    int uniqueEdges = 0;
    bool watertight() const { return boundary == 0 && nonManifold == 0; }
};

struct EdgeRecord {
    int count = 0;
    std::pair<int, int> first;   // first directed occurrence
    std::pair<int, int> second;  // second directed occurrence
};

// Same edge order and pairing rule as trimesh: edges are faces[:, [0,1,1,2,2,0]];
// an edge is consistently wound when, for the two faces sharing it, the second
// occurrence starts where the first one ends.
EdgeStats edgeStats(const std::vector<Face>& faces) {
    std::map<std::pair<int, int>, EdgeRecord> edges;
    for (const auto& f : faces) {
        const std::pair<int, int> directed[3] = {
            {f[0], f[1]}, {f[1], f[2]}, {f[2], f[0]}};
        for (const auto& d : directed) {
            const auto key = std::minmax(d.first, d.second);
            EdgeRecord& rec = edges[{key.first, key.second}];
            if (rec.count == 0) rec.first = d;
            if (rec.count == 1) rec.second = d;
            ++rec.count;
        }
    }
    EdgeStats stats;
    stats.uniqueEdges = static_cast<int>(edges.size());
    for (const auto& entry : edges) {
        const EdgeRecord& rec = entry.second;
        if (rec.count == 1) {
            ++stats.boundary;
        } else if (rec.count == 2) {
            ++stats.manifold;
            if (rec.first.second != rec.second.first) stats.windingConsistent = false;
        } else {
            ++stats.nonManifold;
        }
    }
    return stats;
}

int referencedVertexCount(const std::vector<Face>& faces, size_t vertexCount) {
    std::vector<char> seen(vertexCount, 0);
    int count = 0;
    for (const auto& f : faces) {
        for (int id : f) {
            if (!seen[id]) {
                seen[id] = 1;
                ++count;
            }
        }
    }
    return count;
}

// Divergence-theorem volume from the x-component flux, the formula trimesh's
// mass_properties uses (sum(cross_x * (x0 + x1 + x2)) / 6). For a closed
// surface it equals the signed-tetrahedron volume; for an open one both are
// meaningless, and this keeps the number identical to the reference's.
double signedVolume(const std::vector<Vector3>& v, const std::vector<Face>& faces) {
    double sum = 0.0;
    for (const auto& f : faces) {
        const Vector3 cross = (v[f[1]] - v[f[0]]) % (v[f[2]] - v[f[0]]);
        sum += cross.x * (v[f[0]].x + v[f[1]].x + v[f[2]].x);
    }
    return sum / 6.0;
}

int degenerateCount(const std::vector<Vector3>& v, const std::vector<Face>& faces,
                    double extent) {
    const double limit = 1e-12 * extent * extent;  // on twice the area
    int count = 0;
    for (const auto& f : faces) {
        if (f[0] == f[1] || f[1] == f[2] || f[2] == f[0]) {
            ++count;
            continue;
        }
        if (((v[f[1]] - v[f[0]]) % (v[f[2]] - v[f[0]])).length() <= limit) ++count;
    }
    return count;
}

int bodies(const std::vector<Face>& faces, size_t vertexCount) {
    Mesh m;
    std::vector<Triangle> tris;
    tris.reserve(faces.size());
    for (const auto& f : faces) tris.emplace_back(f[0], f[1], f[2]);
    m.setVertices(std::vector<Vector3>(vertexCount));
    m.setTriangles(tris);
    return m.bodyCount(BodyConnectivity::Edge);
}

std::string formatG(double value) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%g", value);
    return buf;
}

std::string formatFixed2(double value) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.2f", value);
    return buf;
}

// Fill the evidence shared by every verdict reached on (vertices, faces).
void describe(MeshIntegrity& r, const std::vector<Vector3>& v,
              const std::vector<Face>& faces, const EdgeStats& stats, double extent) {
    r.eulerNumber = referencedVertexCount(faces, v.size()) - stats.uniqueEdges +
                    static_cast<int>(faces.size());
    r.windingConsistent = stats.windingConsistent;
    r.volume = signedVolume(v, faces);
    r.manifoldEdges = stats.manifold;
    r.degenerateFaces = degenerateCount(v, faces, extent);
    r.bodyCount = bodies(faces, v.size());
    r.inverted = r.watertight && r.windingConsistent && r.volume < 0.0;
}

void appendAdditionNotes(MeshIntegrity& r) {
    if (r.degenerateFaces > 0) {
        r.notes.push_back(std::to_string(r.degenerateFaces) +
                          " degenerate face(s) with zero area or a repeated vertex");
    }
    if (r.inverted) {
        r.notes.push_back("the surface is inside out: winding is consistent but the "
                          "signed volume is negative, so every normal points inward");
    }
}

} // namespace

std::string MeshIntegrity::summary() const {
    const std::string euler = eulerNumber ? std::to_string(*eulerNumber) : "None";
    if (watertight) {
        const std::string tol = (mergeTolerance && *mergeTolerance != 0.0)
            ? " after merging at " + formatG(*mergeTolerance) + "mm"
            : "";
        return "watertight" + tol + " (volume " + formatFixed2(volume) + ")";
    }
    // Name which of the two failures this is. "Not watertight" alone sends
    // someone looking for a hole even when the surface is closed.
    if (boundaryEdges == 0 && nonManifoldEdges > 0) {
        return "NOT watertight — no holes, but " + std::to_string(nonManifoldEdges) +
               " non-manifold edge(s) shared by more than two faces (euler " + euler + ")";
    }
    return "NOT watertight — " + std::to_string(boundaryEdges) + " boundary edge(s), " +
           std::to_string(nonManifoldEdges) + " non-manifold edge(s), euler " + euler +
           ", winding " + (windingConsistent ? "consistent" : "INCONSISTENT");
}

MeshIntegrity assessMeshIntegrity(const Mesh& mesh, std::optional<double> scaleHint) {
    MeshIntegrity r;
    const auto& vertices = mesh.getVertices();
    std::vector<Face> faces;
    faces.reserve(mesh.getFaces().size());
    for (const auto& t : mesh.getFaces()) faces.push_back({t.v0, t.v1, t.v2});

    if (faces.empty()) {
        r.notes.push_back("mesh has no faces");
        return r;
    }

    // Python truthiness of `scale_hint or extent`: absent or 0 -> extent.
    double extent = 1.0;
    if (scaleHint && *scaleHint != 0.0) {
        extent = *scaleHint;
    } else if (!vertices.empty()) {
        const Vector3 dims = mesh.getBoundingBox();
        extent = std::max({dims.x, dims.y, dims.z});
    }

    const EdgeStats loaded = edgeStats(faces);
    if (loaded.watertight()) {
        r.watertight = true;
        describe(r, vertices, faces, loaded, extent);
        appendAdditionNotes(r);
        return r;
    }

    // Not sealed as loaded. Retry the merge at tolerances scaled to the model,
    // coarsest last, and stop at the first that closes it.
    std::vector<char> referenced(vertices.size(), 0);
    for (const auto& f : faces) for (int id : f) referenced[id] = 1;

    for (double rel : {1e-8, 1e-7, 1e-6, 1e-5}) {
        const double tol = std::max(extent * rel, 1e-9);
        // round() of a float in Python 3 rounds half to even, as nearbyint does.
        const int digits = std::max(0, static_cast<int>(std::nearbyint(-std::log10(tol))));
        double scale = 1.0;
        for (int i = 0; i < digits; ++i) scale *= 10.0;  // exact power of ten

        std::map<std::array<long long, 3>, int> unique;
        std::vector<int> remap(vertices.size(), -1);
        std::vector<Vector3> merged;
        for (size_t i = 0; i < vertices.size(); ++i) {
            if (!referenced[i]) continue;
            const Vector3& p = vertices[i];
            const std::array<long long, 3> key = {
                static_cast<long long>(std::nearbyint(p.x * scale)),
                static_cast<long long>(std::nearbyint(p.y * scale)),
                static_cast<long long>(std::nearbyint(p.z * scale))};
            const auto it = unique.find(key);
            if (it != unique.end()) {
                remap[i] = it->second;
            } else {
                const int id = static_cast<int>(merged.size());
                unique.emplace(key, id);
                remap[i] = id;
                merged.push_back(p);  // first occurrence keeps its coordinates
            }
        }
        std::vector<Face> candidate;
        candidate.reserve(faces.size());
        for (const auto& f : faces) candidate.push_back({remap[f[0]], remap[f[1]], remap[f[2]]});

        const EdgeStats sealed = edgeStats(candidate);
        if (sealed.watertight()) {
            r.watertight = true;
            r.mergeTolerance = tol;
            r.notes.push_back(
                "sealed after merging coincident vertices at " + formatG(tol) + "mm (" +
                std::to_string(static_cast<long long>(vertices.size()) -
                               static_cast<long long>(merged.size())) +
                " merged) — STL float precision, not a hole in the model");
            describe(r, merged, candidate, sealed, extent);
            appendAdditionNotes(r);
            return r;
        }
    }

    // Genuinely not closed-manifold. Say which failure it is.
    r.boundaryEdges = loaded.boundary;
    r.nonManifoldEdges = loaded.nonManifold;
    describe(r, vertices, faces, loaded, extent);
    if (r.boundaryEdges > 0) {
        r.notes.push_back(std::to_string(r.boundaryEdges) +
                          " edge(s) belong to only one face, so the surface does not close");
    }
    if (r.nonManifoldEdges > 0) {
        r.notes.push_back(std::to_string(r.nonManifoldEdges) +
                          " edge(s) are shared by more than two faces — the surface is "
                          "closed but self-touching, typically coincident coplanar faces "
                          "left by a boolean union");
    }
    if (!r.windingConsistent) {
        r.notes.push_back("face winding is inconsistent, which can invert normals and "
                          "break slicing even where the surface closes");
    }
    appendAdditionNotes(r);
    return r;
}

} // namespace madfam::geom
