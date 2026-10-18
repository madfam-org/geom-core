/**
 * Engine.cpp - Main CAD Engine Implementation
 * 
 * Central entry point for all geometry operations.
 * Coordinates shape registry, caching, and performance monitoring.
 */

#include "geom-core/cad/Engine.hpp"
#include "geom-core/cad/ShapeRegistry.hpp"

#ifdef GC_USE_OCCT
#include "OCCTShape.hpp"
#include <Standard_Version.hxx>
#endif

#ifndef GEOM_CORE_VERSION
#error "GEOM_CORE_VERSION must be defined by the build (CMake derives it from pyproject.toml)"
#endif

#include <chrono>
#include <sstream>

namespace madfam::geom::cad {

// =============================================================================
// Global Engine Instance
// =============================================================================

static std::unique_ptr<Engine> globalEngine;

Engine& getGlobalEngine() {
    if (!globalEngine) {
        globalEngine = std::make_unique<Engine>();
        globalEngine->initialize();
    }
    return *globalEngine;
}

// =============================================================================
// Lifecycle
// =============================================================================

Engine::Engine() = default;
Engine::~Engine() = default;

bool Engine::initialize() {
    if (initialized_) return true;
    
#ifdef GC_USE_OCCT
    occtAvailable_ = true;
#else
    occtAvailable_ = false;
#endif
    
    initialized_ = true;
    return true;
}

std::string Engine::getVersion() const {
    std::stringstream ss;
    ss << "geom-core v" << GEOM_CORE_VERSION;
    
#ifdef GC_USE_OCCT
    ss << " (OCCT " << OCC_VERSION_COMPLETE << ")";
#else
    ss << " (no OCCT)";
#endif
    
    return ss.str();
}

void Engine::shutdown() {
    disposeAll();
    initialized_ = false;
}

// =============================================================================
// Memory Management
// =============================================================================

bool Engine::disposeShape(const std::string& shapeId) {
    return ShapeRegistry::instance().disposeShape(shapeId);
}

void Engine::disposeAll() {
    ShapeRegistry::instance().disposeAll();
}

size_t Engine::getShapeCount() const {
    return ShapeRegistry::instance().getShapeCount();
}

size_t Engine::getMemoryUsage() const {
    return ShapeRegistry::instance().getEstimatedMemoryBytes();
}

ShapeHandle Engine::getShapeHandle(const std::string& shapeId) const {
    return ShapeRegistry::instance().getHandle(shapeId);
}

std::vector<ShapeHandle> Engine::getAllShapes() const {
    return ShapeRegistry::instance().getAllHandles();
}

// =============================================================================
// Analysis Operations
// =============================================================================

Result<double> Engine::getVolume(const std::string& shapeId) {
    auto start = std::chrono::high_resolution_clock::now();
    
    auto* shape = ShapeRegistry::instance().getShape(shapeId);
    if (!shape) {
        return Result<double>::error("SHAPE_NOT_FOUND", "Shape not found: " + shapeId);
    }
    
    double volume = shape->getVolume();
    
    auto end = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    auto result = Result<double>::ok(volume);
    result.durationMs = durationMs;
    return result;
}

Result<double> Engine::getSurfaceArea(const std::string& shapeId) {
    auto start = std::chrono::high_resolution_clock::now();
    
    auto* shape = ShapeRegistry::instance().getShape(shapeId);
    if (!shape) {
        return Result<double>::error("SHAPE_NOT_FOUND", "Shape not found: " + shapeId);
    }
    
    double area = shape->getSurfaceArea();
    
    auto end = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    auto result = Result<double>::ok(area);
    result.durationMs = durationMs;
    return result;
}

Result<BoundingBox> Engine::getBoundingBox(const std::string& shapeId) {
    auto* shape = ShapeRegistry::instance().getShape(shapeId);
    if (!shape) {
        return Result<BoundingBox>::error("SHAPE_NOT_FOUND", "Shape not found: " + shapeId);
    }
    
    return Result<BoundingBox>::ok(shape->getBoundingBox());
}

Result<Vector3> Engine::getCenterOfMass(const std::string& shapeId) {
    auto start = std::chrono::high_resolution_clock::now();
    
    auto* shape = ShapeRegistry::instance().getShape(shapeId);
    if (!shape) {
        return Result<Vector3>::error("SHAPE_NOT_FOUND", "Shape not found: " + shapeId);
    }
    
    Vector3 com = shape->getCenterOfMass();
    
    auto end = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    auto result = Result<Vector3>::ok(com);
    result.durationMs = durationMs;
    return result;
}

Result<bool> Engine::isWatertight(const std::string& shapeId) {
    auto* shape = ShapeRegistry::instance().getShape(shapeId);
    if (!shape) {
        return Result<bool>::error("SHAPE_NOT_FOUND", "Shape not found: " + shapeId);
    }
    
#ifdef GC_USE_OCCT
    // Check if solid is valid and closed
    const TopoDS_Shape& occtShape = getOCCT(shape);
    BRepCheck_Analyzer analyzer(occtShape);
    bool valid = analyzer.IsValid();
    return Result<bool>::ok(valid);
#else
    return Result<bool>::error("NOT_IMPLEMENTED", "Watertight check requires OCCT");
#endif
}

Result<bool> Engine::isSolid(const std::string& shapeId) {
    auto* shape = ShapeRegistry::instance().getShape(shapeId);
    if (!shape) {
        return Result<bool>::error("SHAPE_NOT_FOUND", "Shape not found: " + shapeId);
    }
    
    return Result<bool>::ok(shape->getType() == ShapeType::Solid);
}

// =============================================================================
// Tessellation
// =============================================================================

Result<MeshData> Engine::tessellate(const std::string& shapeId, const TessellateOptions& options) {
    auto start = std::chrono::high_resolution_clock::now();
    
    auto* shape = ShapeRegistry::instance().getShape(shapeId);
    if (!shape) {
        return Result<MeshData>::error("SHAPE_NOT_FOUND", "Shape not found: " + shapeId);
    }
    
    MeshData mesh = shape->tessellate(options);
    
    auto end = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    auto result = Result<MeshData>::ok(std::move(mesh));
    result.durationMs = durationMs;
    result.memoryUsedBytes = mesh.byteSize();
    
    notifySlowOperation("tessellate", durationMs);
    ShapeRegistry::instance().recordOperation(durationMs);
    
    return result;
}

// =============================================================================
// Copy Operation
// =============================================================================

Result<ShapeHandle> Engine::copy(const std::string& shapeId) {
    auto start = std::chrono::high_resolution_clock::now();
    
    auto* shape = ShapeRegistry::instance().getShape(shapeId);
    if (!shape) {
        return Result<ShapeHandle>::error("SHAPE_NOT_FOUND", "Shape not found: " + shapeId);
    }
    
    auto cloned = shape->clone();
    ShapeType type = shape->getType();
    
    std::string newId = ShapeRegistry::instance().registerShape(std::move(cloned), type);
    ShapeHandle handle = ShapeRegistry::instance().getHandle(newId);
    
    auto end = std::chrono::high_resolution_clock::now();
    double durationMs = std::chrono::duration<double, std::milli>(end - start).count();
    
    auto result = Result<ShapeHandle>::ok(std::move(handle));
    result.durationMs = durationMs;
    return result;
}

// =============================================================================
// Zero-Lag Optimization
// =============================================================================

void Engine::precompute(const PrecomputeHint& hint) {
    // TODO: Implement background precomputation
    // For now, just log the hint
}

void Engine::warmup(const std::vector<std::string>& modules) {
    // Warm up specific modules by triggering lazy initialization
    for (const auto& module : modules) {
        if (module == "boolean") {
            // Trigger boolean module initialization
        } else if (module == "tessellation") {
            // Trigger tessellation module initialization
        }
    }
}

void Engine::prefetch(const std::vector<std::string>& shapeIds) {
    // Touch shapes to bring them into fast cache
    for (const auto& id : shapeIds) {
        ShapeRegistry::instance().getShape(id);
    }
}

ComplexityEstimate Engine::estimateComplexity(
    const std::string& operation,
    const std::vector<std::string>& shapeIds
) const {
    ComplexityEstimate estimate;
    estimate.score = 0.5;
    estimate.estimatedMs = 100;
    estimate.estimatedBytes = 50000;
    estimate.recommendRemote = false;
    
    // Base complexity by operation type
    static const std::unordered_map<std::string, double> opWeights = {
        {"makeBox", 0.05}, {"makeSphere", 0.05}, {"makeCylinder", 0.05},
        {"makeCone", 0.05}, {"makeTorus", 0.05},
        {"translate", 0.02}, {"rotate", 0.02}, {"scale", 0.02}, {"mirror", 0.02},
        {"booleanUnion", 0.4}, {"booleanSubtract", 0.45}, {"booleanIntersect", 0.4},
        {"extrude", 0.2}, {"revolve", 0.25}, {"sweep", 0.5}, {"loft", 0.6},
        {"fillet", 0.5}, {"chamfer", 0.4}, {"shell", 0.6}, {"offset", 0.5},
        {"tessellate", 0.3},
    };
    
    auto it = opWeights.find(operation);
    double baseWeight = (it != opWeights.end()) ? it->second : 0.5;
    
    // Increase complexity with shape count and complexity
    double shapeMultiplier = 1.0;
    for (const auto& id : shapeIds) {
        auto* shape = ShapeRegistry::instance().getShape(id);
        if (shape) {
            // Use memory estimate as proxy for complexity
            size_t bytes = shape->getEstimatedMemoryBytes();
            shapeMultiplier += bytes / 100000.0; // 100KB = +1.0
        }
    }
    
    estimate.score = std::min(1.0, baseWeight * shapeMultiplier);
    
    // Time estimate based on score
    if (estimate.score < 0.1) {
        estimate.estimatedMs = 5;
    } else if (estimate.score < 0.3) {
        estimate.estimatedMs = 50;
    } else if (estimate.score < 0.6) {
        estimate.estimatedMs = 200;
    } else if (estimate.score < 0.8) {
        estimate.estimatedMs = 500;
    } else {
        estimate.estimatedMs = 2000;
        estimate.recommendRemote = true;
    }
    
    estimate.estimatedBytes = shapeIds.size() * 50000 * estimate.score;
    
    return estimate;
}

void Engine::cancelPrecompute(const std::string& operationKey) {
    // TODO: Cancel pending background operation
}

// =============================================================================
// Health & Metrics
// =============================================================================

Engine::HealthStatus Engine::healthCheck() const {
    HealthStatus status;
    status.healthy = initialized_;
    status.occtAvailable = occtAvailable_;
    status.version = getVersion();
    status.shapeCount = getShapeCount();
    status.memoryUsedBytes = getMemoryUsage();
    
    auto stats = ShapeRegistry::instance().getStats();
    if (stats.cacheHits + stats.cacheMisses > 0) {
        status.cacheHitRate = static_cast<double>(stats.cacheHits) / 
                             (stats.cacheHits + stats.cacheMisses);
    } else {
        status.cacheHitRate = 0;
    }
    
    return status;
}

ShapeRegistry::Stats Engine::getStats() const {
    return ShapeRegistry::instance().getStats();
}

void Engine::onSlowOperation(SlowOperationCallback callback, double thresholdMs) {
    slowOpCallbacks_.push_back({std::move(callback), thresholdMs});
}

void Engine::notifySlowOperation(const std::string& op, double durationMs) {
    for (const auto& [callback, threshold] : slowOpCallbacks_) {
        if (durationMs >= threshold) {
            callback(op, durationMs);
        }
    }
}

std::string Engine::generateOperationKey(
    const std::string& op, 
    const std::vector<std::string>& ids
) const {
    std::stringstream ss;
    ss << op;
    for (const auto& id : ids) {
        ss << ":" << id;
    }
    return ss.str();
}

} // namespace madfam::geom::cad
