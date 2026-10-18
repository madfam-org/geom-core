#include <emscripten/bind.h>
#include <emscripten/val.h>
#include <cstdint>
#include <type_traits>
#include "geom-core/Analyzer.hpp"
#include "geom-core/Mesh.hpp"
#include "geom-core/Vector3.hpp"

using namespace emscripten;
using namespace madfam::geom;

// ========================================
// Typed Array Wrapper Functions (Milestone 8)
// ========================================

/**
 * @brief Get overhang map as a Uint8Array view (zero-copy)
 *
 * Returns a typed memory view directly into C++ memory.
 * The JS TypedArray shares memory with the C++ vector.
 *
 * @return Uint8Array with size = triangle count
 *         Values: 0 = Safe, 1 = Overhang, 2 = Ground
 */
val getOverhangMapJS(Analyzer& self, double criticalAngleDegrees) {
    const auto& data = self.calculateOverhangMap(criticalAngleDegrees);
    return val(typed_memory_view(data.size(), data.data()));
}

/**
 * @brief Get wall thickness map as a Float32Array view (zero-copy)
 *
 * Returns a typed memory view directly into C++ memory.
 * The JS TypedArray shares memory with the C++ vector.
 *
 * @return Float32Array with size = vertex count
 *         Values: Wall thickness in mm
 */
val getWallThicknessMapJS(Analyzer& self, double maxSearchDistanceMM) {
    const auto& data = self.calculateWallThicknessMap(maxSearchDistanceMM);
    return val(typed_memory_view(data.size(), data.data()));
}

// ========================================
// Mesh data and bounds (0.2.0)
// ========================================

// The vertex and index views below reinterpret the C++ arrays in place.
static_assert(std::is_standard_layout_v<Vector3> && sizeof(Vector3) == 3 * sizeof(double),
              "Vector3 must be three packed doubles for the Float64Array view");
static_assert(std::is_standard_layout_v<Triangle> && sizeof(Triangle) == 3 * sizeof(int32_t) &&
                  sizeof(int) == sizeof(uint32_t),
              "Triangle must be three packed 32-bit indices for the Uint32Array view");

/**
 * @brief Welded vertex coordinates as a zero-copy Float64Array view
 *        [x0, y0, z0, x1, ...] into WASM memory.
 *
 * The view is only valid until the next call that can allocate (WASM memory
 * growth detaches it), the next load, or delete(). getVertices() copies.
 */
val getVerticesViewJS(const Analyzer& self) {
    const auto& v = self.getVertices();
    return val(typed_memory_view(v.size() * 3, reinterpret_cast<const double*>(v.data())));
}

/**
 * @brief Triangle indices into the vertex array as a zero-copy Uint32Array
 *        view [a0, b0, c0, a1, ...]; same lifetime rule as getVerticesView().
 */
val getIndicesViewJS(const Analyzer& self) {
    const auto& f = self.getFaces();
    return val(typed_memory_view(f.size() * 3, reinterpret_cast<const uint32_t*>(f.data())));
}

/** @brief Owned Float64Array copy of getVerticesView(). */
val getVerticesJS(const Analyzer& self) {
    return val::global("Float64Array").new_(getVerticesViewJS(self));
}

/** @brief Owned Uint32Array copy of getIndicesView(). */
val getIndicesJS(const Analyzer& self) {
    return val::global("Uint32Array").new_(getIndicesViewJS(self));
}

val vec3JS(const Vector3& v) {
    val out = val::object();
    out.set("x", v.x);
    out.set("y", v.y);
    out.set("z", v.z);
    return out;
}

/**
 * @brief Bounding box as {min, max, size} plus the 0.1.0 extents x/y/z
 *
 * x, y and z equal size.x/y/z; they are kept so 0.1.0 callers that read the
 * extents keep working, and are deprecated in favour of size.
 */
val getBoundingBoxJS(const Analyzer& self) {
    const BoundingBox b = self.getBounds();
    val out = val::object();
    out.set("min", vec3JS(b.min));
    out.set("max", vec3JS(b.max));
    out.set("size", vec3JS(b.size));
    out.set("x", b.size.x);
    out.set("y", b.size.y);
    out.set("z", b.size.z);
    return out;
}

/**
 * @brief Mesh integrity verdict as a plain JS object
 *
 * scaleHint <= 0 (or 0) derives the scale from the largest bounding-box
 * dimension, like the C++ default. eulerNumber and mergeTolerance are null
 * when absent; notes is an Array of strings.
 */
val assessIntegrityJS(const Analyzer& self, double scaleHint) {
    const MeshIntegrity r = self.assessIntegrity(
        scaleHint > 0.0 ? std::optional<double>(scaleHint) : std::nullopt);
    val out = val::object();
    out.set("watertight", r.watertight);
    out.set("boundaryEdges", r.boundaryEdges);
    out.set("nonManifoldEdges", r.nonManifoldEdges);
    out.set("manifoldEdges", r.manifoldEdges);
    out.set("eulerNumber", r.eulerNumber ? val(*r.eulerNumber) : val::null());
    out.set("windingConsistent", r.windingConsistent);
    out.set("volume", r.volume);
    out.set("mergeTolerance", r.mergeTolerance ? val(*r.mergeTolerance) : val::null());
    out.set("degenerateFaces", r.degenerateFaces);
    out.set("inverted", r.inverted);
    out.set("bodyCount", r.bodyCount);
    val notes = val::array();
    for (const auto& note : r.notes) notes.call<void>("push", note);
    out.set("notes", notes);
    out.set("summary", r.summary());
    return out;
}

EMSCRIPTEN_BINDINGS(geom_core_module) {
    // PrintabilityReport struct
    value_object<PrintabilityReport>("PrintabilityReport")
        .field("overhangArea", &PrintabilityReport::overhangArea)
        .field("overhangPercentage", &PrintabilityReport::overhangPercentage)
        .field("thinWallVertexCount", &PrintabilityReport::thinWallVertexCount)
        .field("score", &PrintabilityReport::score)
        .field("totalSurfaceArea", &PrintabilityReport::totalSurfaceArea);

    // OrientationResult struct (Milestone 5)
    value_object<OrientationResult>("OrientationResult")
        .field("optimalUpVector", &OrientationResult::optimalUpVector)
        .field("originalOverhangArea", &OrientationResult::originalOverhangArea)
        .field("optimizedOverhangArea", &OrientationResult::optimizedOverhangArea)
        .field("improvementPercent", &OrientationResult::improvementPercent);

    // Vector3 class
    value_object<Vector3>("Vector3")
        .field("x", &Vector3::x)
        .field("y", &Vector3::y)
        .field("z", &Vector3::z);

    // Analyzer class
    class_<Analyzer>("Analyzer")
        .constructor<>()
        // Raw boolean loaders. The package entry points (dist/wasm/index.js,
        // threaded.js) wrap them as loadSTLFromBytes()/loadSTL(), which throw
        // GeomCoreSTLError with getLastError() instead of returning false.
        .function("_loadSTLFromBytes", &Analyzer::loadSTLFromBytes)
        .function("_loadSTL", &Analyzer::loadSTL)
        .function("getLastError", &Analyzer::getLastError)
        .function("getVolume", &Analyzer::getVolume)
        .function("isWatertight", &Analyzer::isWatertight)
        .function("getBoundingBox", &getBoundingBoxJS)
        .function("getVertices", &getVerticesJS)
        .function("getIndices", &getIndicesJS)
        .function("getVerticesView", &getVerticesViewJS)
        .function("getIndicesView", &getIndicesViewJS)
        .function("getVertexCount", &Analyzer::getVertexCount)
        .function("getTriangleCount", &Analyzer::getTriangleCount)
        .function("getSurfaceArea", &Analyzer::getSurfaceArea)
        .function("getCentroid", &Analyzer::getCentroid)
        .function("getBodyCount", &Analyzer::getBodyCount)
        .function("assessIntegrity", &assessIntegrityJS)
        .function("buildSpatialIndex", &Analyzer::buildSpatialIndex)
        .function("getPrintabilityReport", &Analyzer::getPrintabilityReport)
        .function("autoOrient", &Analyzer::autoOrient)
        // Visualization data export (Milestone 8) - Typed Arrays
        .function("getOverhangMapJS", &getOverhangMapJS)
        .function("getWallThicknessMapJS", &getWallThicknessMapJS);
}
