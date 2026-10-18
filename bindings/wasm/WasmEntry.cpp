#include <emscripten/bind.h>
#include <emscripten/val.h>
#include "geom-core/Analyzer.hpp"
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
        .function("loadSTLFromBytes", &Analyzer::loadSTLFromBytes)
        .function("loadSTL", &Analyzer::loadSTL)
        .function("getVolume", &Analyzer::getVolume)
        .function("isWatertight", &Analyzer::isWatertight)
        .function("getBoundingBox", &Analyzer::getBoundingBox)
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
