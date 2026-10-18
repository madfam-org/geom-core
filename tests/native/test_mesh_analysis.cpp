// C++ unit tests for mesh scalars and the integrity assessment.
// Zero-dependency harness (the core library is std-only); registered with
// CTest by CMakeLists.txt when GEOM_BUILD_TESTS is ON.
#include "geom-core/Analyzer.hpp"
#include "geom-core/Integrity.hpp"
#include "geom-core/Mesh.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

using madfam::geom::Analyzer;
using madfam::geom::Mesh;
using madfam::geom::MeshIntegrity;
using madfam::geom::Triangle;
using madfam::geom::Vector3;

namespace {

int failures = 0;
int checks = 0;

void check(bool ok, const std::string& what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::fprintf(stderr, "FAIL: %s\n", what.c_str());
    }
}

bool near(double a, double b, double tol = 1e-9) { return std::abs(a - b) <= tol; }

struct Soup {
    std::vector<Vector3> v;
    std::vector<Triangle> f;
};

// Outward-wound axis-aligned box [x0, x0+s]^3, 12 triangles, 8 shared vertices.
Soup box(double x0 = 0.0, double s = 10.0) {
    Soup m;
    for (int i = 0; i < 8; ++i) {
        m.v.emplace_back(x0 + ((i & 1) ? s : 0), (i & 2) ? s : 0, (i & 4) ? s : 0);
    }
    const int faces[12][3] = {{0, 2, 1}, {1, 2, 3}, {4, 5, 6}, {5, 7, 6},
                              {0, 1, 4}, {1, 5, 4}, {2, 6, 3}, {3, 6, 7},
                              {0, 4, 2}, {2, 4, 6}, {1, 3, 5}, {3, 7, 5}};
    for (const auto& t : faces) m.f.emplace_back(t[0], t[1], t[2]);
    return m;
}

Soup concat(const Soup& a, const Soup& b) {
    Soup m = a;
    const int offset = static_cast<int>(a.v.size());
    m.v.insert(m.v.end(), b.v.begin(), b.v.end());
    for (const auto& t : b.f) m.f.emplace_back(t.v0 + offset, t.v1 + offset, t.v2 + offset);
    return m;
}

// Every triangle gets its own vertex copies, as an STL stores them.
Soup unweld(const Soup& a, double jitter = 0.0) {
    Soup m;
    int k = 0;
    for (const auto& t : a.f) {
        for (int id : {t.v0, t.v1, t.v2}) {
            const double d = jitter * ((k % 3) - 1);  // -j, 0, +j
            m.v.emplace_back(a.v[id].x + d, a.v[id].y - d, a.v[id].z + d);
            ++k;
        }
        const int n = static_cast<int>(m.v.size());
        m.f.emplace_back(n - 3, n - 2, n - 1);
    }
    return m;
}

// Merge exactly coincident vertices, as the STL loader does.
Soup weld(const Soup& a) {
    Soup m;
    std::map<Vector3, int> ids;
    std::vector<int> remap(a.v.size());
    for (size_t i = 0; i < a.v.size(); ++i) {
        const auto it = ids.find(a.v[i]);
        if (it != ids.end()) {
            remap[i] = it->second;
        } else {
            remap[i] = static_cast<int>(m.v.size());
            ids.emplace(a.v[i], remap[i]);
            m.v.push_back(a.v[i]);
        }
    }
    for (const auto& t : a.f) m.f.emplace_back(remap[t.v0], remap[t.v1], remap[t.v2]);
    return m;
}

Mesh toMesh(const Soup& s) {
    Mesh m;
    m.setVertices(s.v);
    m.setTriangles(s.f);
    return m;
}

void testScalars() {
    const Mesh cube = toMesh(box());
    check(near(cube.getSurfaceArea(), 600.0), "box surface area 600");
    check(near(cube.getSignedVolume(), 1000.0), "box signed volume +1000");
    const Vector3 c = cube.getCentroid();
    check(near(c.x, 5) && near(c.y, 5) && near(c.z, 5), "box centroid (5,5,5)");
    check(cube.bodyCount() == 1, "box is one body");

    const Mesh two = toMesh(concat(box(0), box(20)));
    check(two.bodyCount() == 2, "two disjoint boxes are two bodies");
    check(near(two.getSurfaceArea(), 1200.0), "two boxes surface area 1200");
    const Vector3 c2 = two.getCentroid();
    check(near(c2.x, 15) && near(c2.y, 5) && near(c2.z, 5), "two-box centroid (15,5,5)");

    Soup flipped = box();
    for (auto& t : flipped.f) std::swap(t.v1, t.v2);
    const Mesh inv = toMesh(flipped);
    check(near(inv.getSignedVolume(), -1000.0), "inside-out box signed volume -1000");
    check(near(inv.getVolume(), 1000.0), "getVolume stays absolute");
    const Vector3 c3 = inv.getCentroid();
    check(near(c3.x, 5) && near(c3.y, 5) && near(c3.z, 5), "centroid is winding-independent");

    const Mesh empty;
    check(empty.getSurfaceArea() == 0.0 && empty.bodyCount() == 0, "empty mesh scalars are 0");
    const Vector3 c4 = empty.getCentroid();
    check(c4.x == 0 && c4.y == 0 && c4.z == 0, "empty mesh centroid is the origin");
}

void testIntegrity() {
    const MeshIntegrity closed = madfam::geom::assessMeshIntegrity(toMesh(box()));
    check(closed.watertight, "closed box is watertight");
    check(closed.boundaryEdges == 0 && closed.nonManifoldEdges == 0, "closed box has no bad edges");
    check(closed.manifoldEdges == 18, "closed box has 18 manifold edges");
    check(closed.eulerNumber && *closed.eulerNumber == 2, "closed box euler 2");
    check(closed.windingConsistent && !closed.inverted, "closed box consistent, not inverted");
    check(near(closed.volume, 1000.0) && !closed.mergeTolerance, "closed box volume, no merge");
    check(closed.bodyCount == 1 && closed.degenerateFaces == 0, "closed box: one body, no degenerate");
    check(closed.summary() == "watertight (volume 1000.00)", "closed box summary: " + closed.summary());

    Soup open = box();
    open.f.erase(open.f.begin(), open.f.begin() + 2);  // drop the z=0 face
    const MeshIntegrity holed = madfam::geom::assessMeshIntegrity(toMesh(open));
    check(!holed.watertight && holed.boundaryEdges == 4, "open box: 4 boundary edges");
    check(holed.nonManifoldEdges == 0 && holed.windingConsistent, "open box: manifold, consistent");
    check(holed.eulerNumber && *holed.eulerNumber == 1, "open box euler 1");
    check(holed.summary() ==
              "NOT watertight — 4 boundary edge(s), 0 non-manifold edge(s), euler 1, winding consistent",
          "open box summary: " + holed.summary());
    check(!holed.notes.empty() && holed.notes[0].rfind("4 edge(s) belong to only one face", 0) == 0,
          "open box note names the holes");

    // Unwelded soup with sub-micron noise: closed once vertices merge.
    const MeshIntegrity soup =
        madfam::geom::assessMeshIntegrity(toMesh(unweld(box(), 4e-7)), 10.0);
    check(soup.watertight && soup.mergeTolerance.has_value(), "noisy soup seals after merging");
    check(soup.mergeTolerance && near(*soup.mergeTolerance, 1e-6, 1e-18), "soup sealed at 1e-6");
    check(!soup.notes.empty() && soup.notes[0].find("STL float precision") != std::string::npos,
          "soup note blames float precision");
    check(soup.eulerNumber && *soup.eulerNumber == 2 && soup.bodyCount == 1, "merged soup topology");

    // Two boxes sharing one edge: closed but non-manifold (bow-tie edge).
    Soup a = box(0);
    Soup b = box(10);
    // Shift b in y so it touches a only along the edge x=10, y=10.
    for (auto& p : b.v) p.y += 10.0;
    const MeshIntegrity bowtie = madfam::geom::assessMeshIntegrity(toMesh(weld(concat(a, b))));
    check(bowtie.watertight == false && bowtie.nonManifoldEdges == 1 && bowtie.boundaryEdges == 0,
          "edge-sharing boxes: one non-manifold edge, no holes (" + bowtie.summary() + ")");
    check(bowtie.summary().rfind("NOT watertight — no holes, but 1 non-manifold edge(s)", 0) == 0,
          "non-manifold summary names the failure");

    Soup flipped = box();
    for (auto& t : flipped.f) std::swap(t.v1, t.v2);
    const MeshIntegrity inv = madfam::geom::assessMeshIntegrity(toMesh(flipped));
    check(inv.watertight && inv.windingConsistent && inv.inverted, "inside-out box is inverted");
    check(near(inv.volume, -1000.0), "inside-out volume is negative");

    Soup oneFlipped = box();
    std::swap(oneFlipped.f[0].v1, oneFlipped.f[0].v2);
    const MeshIntegrity mixed = madfam::geom::assessMeshIntegrity(toMesh(oneFlipped));
    check(mixed.watertight && !mixed.windingConsistent && !mixed.inverted,
          "one flipped triangle: closed, inconsistent winding");

    Soup sliver = box();
    sliver.v.emplace_back(20, 0, 0);
    sliver.v.emplace_back(21, 0, 0);
    sliver.v.emplace_back(22, 0, 0);
    sliver.f.emplace_back(8, 9, 10);  // collinear: zero area, separate body
    const MeshIntegrity degenerate = madfam::geom::assessMeshIntegrity(toMesh(sliver));
    check(degenerate.degenerateFaces == 1 && degenerate.bodyCount == 2, "sliver: degenerate, 2 bodies");
    check(!degenerate.watertight && degenerate.boundaryEdges == 3, "sliver: 3 boundary edges");

    const MeshIntegrity none = madfam::geom::assessMeshIntegrity(Mesh());
    check(!none.watertight && !none.eulerNumber && none.notes.size() == 1 &&
              none.notes[0] == "mesh has no faces",
          "empty mesh verdict");
    check(none.summary() ==
              "NOT watertight — 0 boundary edge(s), 0 non-manifold edge(s), euler None, winding INCONSISTENT",
          "empty mesh summary: " + none.summary());

    Analyzer analyzer;
    const MeshIntegrity unloaded = analyzer.assessIntegrity();
    check(!unloaded.watertight && analyzer.getBodyCount() == 0, "analyzer without a mesh");
}

// Binary STL bytes (float32 coordinates, zero normals) for a soup.
std::string toSTL(const Soup& m) {
    std::string bytes(84 + m.f.size() * 50, '\0');
    const uint32_t count = static_cast<uint32_t>(m.f.size());
    std::memcpy(&bytes[80], &count, 4);
    size_t offset = 84;
    for (const auto& t : m.f) {
        offset += 12;
        for (int id : {t.v0, t.v1, t.v2}) {
            const float xyz[3] = {static_cast<float>(m.v[id].x), static_cast<float>(m.v[id].y),
                                  static_cast<float>(m.v[id].z)};
            std::memcpy(&bytes[offset], xyz, 12);
            offset += 12;
        }
        offset += 2;
    }
    return bytes;
}

void testBoundsAndLoadErrors() {
    Analyzer analyzer;
    const auto empty = analyzer.getBounds();
    check(empty.min.x == 0 && empty.max.z == 0 && empty.size.y == 0, "bounds without a mesh are zero");

    // Box [12, 22] x [0, 10] x [0, 10]: min/max are corners, size the extents.
    check(analyzer.loadSTLFromBytes(toSTL(box(12.0))), "offset box loads");
    check(analyzer.getLastError().empty(), "no error after a good load");
    const auto b = analyzer.getBounds();
    check(near(b.min.x, 12) && near(b.min.y, 0) && near(b.min.z, 0), "bounds min corner");
    check(near(b.max.x, 22) && near(b.max.y, 10) && near(b.max.z, 10), "bounds max corner");
    check(near(b.size.x, 10) && near(b.size.y, 10) && near(b.size.z, 10), "bounds size");
    const Vector3 extents = analyzer.getBoundingBox();
    check(extents.x == b.size.x && extents.y == b.size.y && extents.z == b.size.z,
          "getBoundingBox() still returns the size");

    check(!analyzer.loadSTLFromBytes(std::string(10, '\0')), "short buffer rejected");
    check(analyzer.getLastError() == "STL buffer too small (< 84 bytes)",
          "short buffer message: " + analyzer.getLastError());
    check(analyzer.getTriangleCount() == 0, "failed load leaves no mesh");

    std::string huge(84, '\0');
    const uint32_t claimed = 20000000;
    std::memcpy(&huge[80], &claimed, 4);
    check(!analyzer.loadSTLFromBytes(huge), "oversized face count rejected");
    check(analyzer.getLastError() == "STL triangle count exceeds the buffer or index capacity",
          "oversized count message: " + analyzer.getLastError());

    check(analyzer.loadSTLFromBytes(toSTL(box())), "reload after a failure");
    check(analyzer.getLastError().empty(), "a good load clears the previous error");

    check(!analyzer.loadSTL("/nonexistent/geom-core-missing.stl"), "missing file rejected");
    check(analyzer.getLastError() == "Could not open STL file: /nonexistent/geom-core-missing.stl",
          "missing file message: " + analyzer.getLastError());
}

} // namespace

int main() {
    testScalars();
    testIntegrity();
    testBoundsAndLoadErrors();
    std::printf("%d/%d checks passed\n", checks - failures, checks);
    return failures == 0 ? 0 : 1;
}
