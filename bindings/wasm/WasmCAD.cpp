/**
 * WasmCAD.cpp - WebAssembly Bindings for geom-core CAD Engine
 * 
 * Exposes the C++ CAD engine to JavaScript via Emscripten embind.
 * Optimized for zero-copy data transfer using typed_memory_view.
 */

#include <emscripten/bind.h>
#include <emscripten/val.h>

#include "geom-core/cad/Engine.hpp"
#include "geom-core/cad/ShapeRegistry.hpp"
#include "geom-core/cad/Types.hpp"

using namespace emscripten;
using namespace madfam::geom::cad;
using madfam::geom::Vector3;

// =============================================================================
// JavaScript Value Converters
// =============================================================================

namespace {

// Convert Vec3 to JS object
val vec3ToJS(const Vector3& v) {
    val obj = val::object();
    obj.set("x", v.x);
    obj.set("y", v.y);
    obj.set("z", v.z);
    return obj;
}

// Convert JS object to Vec3
Vector3 jsToVec3(const val& obj) {
    return Vector3(
        obj["x"].as<double>(),
        obj["y"].as<double>(),
        obj["z"].as<double>()
    );
}

// Convert optional Vec3
std::optional<Vector3> jsToOptionalVec3(const val& obj) {
    if (obj.isUndefined() || obj.isNull()) {
        return std::nullopt;
    }
    return jsToVec3(obj);
}

// Convert BoundingBox to JS
val bboxToJS(const BoundingBox& bbox) {
    val obj = val::object();
    obj.set("min", vec3ToJS(bbox.min));
    obj.set("max", vec3ToJS(bbox.max));
    return obj;
}

// Convert ShapeHandle to JS
val handleToJS(const ShapeHandle& h) {
    val obj = val::object();
    obj.set("id", h.id);
    obj.set("type", static_cast<int>(h.type));
    obj.set("bbox", bboxToJS(h.bbox));
    obj.set("hash", h.hash);
    
    if (h.volume.has_value()) {
        obj.set("volume", h.volume.value());
    }
    if (h.surfaceArea.has_value()) {
        obj.set("surfaceArea", h.surfaceArea.value());
    }
    if (h.centerOfMass.has_value()) {
        obj.set("centerOfMass", vec3ToJS(h.centerOfMass.value()));
    }
    
    return obj;
}

// Convert Result<ShapeHandle> to JS
val resultHandleToJS(const Result<ShapeHandle>& r) {
    val obj = val::object();
    obj.set("success", r.success);
    
    if (r.success) {
        obj.set("value", handleToJS(r.value));
    } else {
        val err = val::object();
        err.set("code", r.errorCode);
        err.set("message", r.errorMessage);
        obj.set("error", err);
    }
    
    obj.set("durationMs", r.durationMs);
    obj.set("memoryUsedBytes", r.memoryUsedBytes);
    obj.set("wasCached", r.wasCached);
    
    return obj;
}

// Convert Result<double> to JS
val resultDoubleToJS(const Result<double>& r) {
    val obj = val::object();
    obj.set("success", r.success);
    
    if (r.success) {
        obj.set("value", r.value);
    } else {
        val err = val::object();
        err.set("code", r.errorCode);
        err.set("message", r.errorMessage);
        obj.set("error", err);
    }
    
    obj.set("durationMs", r.durationMs);
    return obj;
}

// Return JS-owned arrays: the operation result is destroyed after this call.
val meshDataToJS(const MeshData& mesh) {
    val obj = val::object();
    
    // Copy from the temporary view before the C++ result releases its vectors.
    obj.set("positions", val::global("Float32Array").new_(val(typed_memory_view(mesh.positions.size(), mesh.positions.data()))));
    obj.set("normals", val::global("Float32Array").new_(val(typed_memory_view(mesh.normals.size(), mesh.normals.data()))));
    obj.set("indices", val::global("Uint32Array").new_(val(typed_memory_view(mesh.indices.size(), mesh.indices.data()))));
    
    if (!mesh.uvs.empty()) {
        obj.set("uvs", val::global("Float32Array").new_(val(typed_memory_view(mesh.uvs.size(), mesh.uvs.data()))));
    }
    
    obj.set("vertexCount", mesh.vertexCount());
    obj.set("triangleCount", mesh.triangleCount());
    obj.set("byteSize", mesh.byteSize());
    
    return obj;
}

} // anonymous namespace

// =============================================================================
// Wrapper Class for JavaScript API
// =============================================================================

class WasmCADEngine {
public:
    WasmCADEngine() {
        engine_ = &getGlobalEngine();
    }
    
    // Lifecycle
    bool initialize() { return engine_->initialize(); }
    bool isInitialized() const { return engine_->isInitialized(); }
    std::string getVersion() const { return engine_->getVersion(); }
    void shutdown() { engine_->shutdown(); }
    
    // ==========================================================================
    // Primitives
    // ==========================================================================
    
    val makeBox(val params) {
        BoxParams p;
        if (!params.isUndefined()) {
            if (params.hasOwnProperty("width")) p.width = params["width"].as<double>();
            if (params.hasOwnProperty("height")) p.height = params["height"].as<double>();
            if (params.hasOwnProperty("depth")) p.depth = params["depth"].as<double>();
            if (params.hasOwnProperty("center")) p.center = jsToOptionalVec3(params["center"]);
        }
        return resultHandleToJS(engine_->makeBox(p));
    }
    
    val makeSphere(val params) {
        SphereParams p;
        if (!params.isUndefined()) {
            if (params.hasOwnProperty("radius")) p.radius = params["radius"].as<double>();
            if (params.hasOwnProperty("center")) p.center = jsToOptionalVec3(params["center"]);
        }
        return resultHandleToJS(engine_->makeSphere(p));
    }
    
    val makeCylinder(val params) {
        CylinderParams p;
        if (!params.isUndefined()) {
            if (params.hasOwnProperty("radius")) p.radius = params["radius"].as<double>();
            if (params.hasOwnProperty("height")) p.height = params["height"].as<double>();
            if (params.hasOwnProperty("center")) p.center = jsToOptionalVec3(params["center"]);
            if (params.hasOwnProperty("axis")) p.axis = jsToVec3(params["axis"]);
        }
        return resultHandleToJS(engine_->makeCylinder(p));
    }
    
    val makeCone(val params) {
        ConeParams p;
        if (!params.isUndefined()) {
            if (params.hasOwnProperty("radius1")) p.radius1 = params["radius1"].as<double>();
            if (params.hasOwnProperty("radius2")) p.radius2 = params["radius2"].as<double>();
            if (params.hasOwnProperty("height")) p.height = params["height"].as<double>();
            if (params.hasOwnProperty("center")) p.center = jsToOptionalVec3(params["center"]);
            if (params.hasOwnProperty("axis")) p.axis = jsToVec3(params["axis"]);
        }
        return resultHandleToJS(engine_->makeCone(p));
    }
    
    val makeTorus(val params) {
        TorusParams p;
        if (!params.isUndefined()) {
            if (params.hasOwnProperty("majorRadius")) p.majorRadius = params["majorRadius"].as<double>();
            if (params.hasOwnProperty("minorRadius")) p.minorRadius = params["minorRadius"].as<double>();
            if (params.hasOwnProperty("center")) p.center = jsToOptionalVec3(params["center"]);
            if (params.hasOwnProperty("axis")) p.axis = jsToVec3(params["axis"]);
        }
        return resultHandleToJS(engine_->makeTorus(p));
    }
    
    // ==========================================================================
    // Boolean Operations
    // ==========================================================================
    
    val booleanUnion(val params) {
        BooleanUnionParams p;
        if (params.hasOwnProperty("shapeIds")) {
            val arr = params["shapeIds"];
            int len = arr["length"].as<int>();
            for (int i = 0; i < len; ++i) {
                p.shapeIds.push_back(arr[i].as<std::string>());
            }
        }
        return resultHandleToJS(engine_->booleanUnion(p));
    }
    
    val booleanSubtract(val params) {
        BooleanSubtractParams p;
        if (params.hasOwnProperty("baseId")) {
            p.baseId = params["baseId"].as<std::string>();
        }
        if (params.hasOwnProperty("toolIds")) {
            val arr = params["toolIds"];
            int len = arr["length"].as<int>();
            for (int i = 0; i < len; ++i) {
                p.toolIds.push_back(arr[i].as<std::string>());
            }
        }
        return resultHandleToJS(engine_->booleanSubtract(p));
    }
    
    val booleanIntersect(val params) {
        BooleanIntersectParams p;
        if (params.hasOwnProperty("shapeIds")) {
            val arr = params["shapeIds"];
            int len = arr["length"].as<int>();
            for (int i = 0; i < len; ++i) {
                p.shapeIds.push_back(arr[i].as<std::string>());
            }
        }
        return resultHandleToJS(engine_->booleanIntersect(p));
    }
    
    // ==========================================================================
    // Transforms
    // ==========================================================================
    
    val translate(val params) {
        TranslateParams p;
        p.shapeId = params["shapeId"].as<std::string>();
        p.offset = jsToVec3(params["offset"]);
        return resultHandleToJS(engine_->translate(p));
    }
    
    val rotate(val params) {
        RotateParams p;
        p.shapeId = params["shapeId"].as<std::string>();
        if (params.hasOwnProperty("axisOrigin")) {
            p.axisOrigin = jsToVec3(params["axisOrigin"]);
        }
        if (params.hasOwnProperty("axisDirection")) {
            p.axisDirection = jsToVec3(params["axisDirection"]);
        }
        p.angle = params["angle"].as<double>();
        return resultHandleToJS(engine_->rotate(p));
    }
    
    val scale(val params) {
        ScaleParams p;
        p.shapeId = params["shapeId"].as<std::string>();
        if (params.hasOwnProperty("center")) {
            p.center = jsToVec3(params["center"]);
        }
        p.factor = params["factor"].as<double>();
        return resultHandleToJS(engine_->scale(p));
    }
    
    val mirror(val params) {
        MirrorParams p;
        p.shapeId = params["shapeId"].as<std::string>();
        if (params.hasOwnProperty("planePoint")) {
            p.planePoint = jsToVec3(params["planePoint"]);
        }
        if (params.hasOwnProperty("planeNormal")) {
            p.planeNormal = jsToVec3(params["planeNormal"]);
        }
        return resultHandleToJS(engine_->mirror(p));
    }
    
    // ==========================================================================
    // Analysis
    // ==========================================================================
    
    val tessellate(std::string shapeId, val options) {
        TessellateOptions opts;
        if (!options.isUndefined()) {
            if (options.hasOwnProperty("linearDeflection")) {
                opts.linearDeflection = options["linearDeflection"].as<double>();
            }
            if (options.hasOwnProperty("angularDeflection")) {
                opts.angularDeflection = options["angularDeflection"].as<double>();
            }
            if (options.hasOwnProperty("relative")) {
                opts.relative = options["relative"].as<bool>();
            }
            if (options.hasOwnProperty("computeNormals")) {
                opts.computeNormals = options["computeNormals"].as<bool>();
            }
            if (options.hasOwnProperty("computeUVs")) {
                opts.computeUVs = options["computeUVs"].as<bool>();
            }
        }
        
        auto result = engine_->tessellate(shapeId, opts);
        
        val obj = val::object();
        obj.set("success", result.success);
        
        if (result.success) {
            obj.set("value", meshDataToJS(result.value));
        } else {
            val err = val::object();
            err.set("code", result.errorCode);
            err.set("message", result.errorMessage);
            obj.set("error", err);
        }
        
        obj.set("durationMs", result.durationMs);
        obj.set("memoryUsedBytes", result.memoryUsedBytes);
        
        return obj;
    }
    
    val getVolume(std::string shapeId) {
        return resultDoubleToJS(engine_->getVolume(shapeId));
    }
    
    val getSurfaceArea(std::string shapeId) {
        return resultDoubleToJS(engine_->getSurfaceArea(shapeId));
    }
    
    val getBoundingBox(std::string shapeId) {
        auto result = engine_->getBoundingBox(shapeId);
        val obj = val::object();
        obj.set("success", result.success);
        
        if (result.success) {
            obj.set("value", bboxToJS(result.value));
        } else {
            val err = val::object();
            err.set("code", result.errorCode);
            err.set("message", result.errorMessage);
            obj.set("error", err);
        }
        
        return obj;
    }
    
    val getCenterOfMass(std::string shapeId) {
        auto result = engine_->getCenterOfMass(shapeId);
        val obj = val::object();
        obj.set("success", result.success);
        
        if (result.success) {
            obj.set("value", vec3ToJS(result.value));
        } else {
            val err = val::object();
            err.set("code", result.errorCode);
            err.set("message", result.errorMessage);
            obj.set("error", err);
        }
        
        return obj;
    }
    
    // ==========================================================================
    // Memory Management
    // ==========================================================================
    
    bool disposeShape(std::string shapeId) {
        return engine_->disposeShape(shapeId);
    }
    
    void disposeAll() {
        engine_->disposeAll();
    }
    
    int getShapeCount() {
        return static_cast<int>(engine_->getShapeCount());
    }
    
    int getMemoryUsage() {
        return static_cast<int>(engine_->getMemoryUsage());
    }
    
    val getShapeHandle(std::string shapeId) {
        return handleToJS(engine_->getShapeHandle(shapeId));
    }
    
    // ==========================================================================
    // Zero-Lag Optimization
    // ==========================================================================
    
    val estimateComplexity(std::string operation, val shapeIdsArray) {
        std::vector<std::string> shapeIds;
        int len = shapeIdsArray["length"].as<int>();
        for (int i = 0; i < len; ++i) {
            shapeIds.push_back(shapeIdsArray[i].as<std::string>());
        }
        
        auto estimate = engine_->estimateComplexity(operation, shapeIds);
        
        val obj = val::object();
        obj.set("score", estimate.score);
        obj.set("estimatedMs", estimate.estimatedMs);
        obj.set("estimatedBytes", estimate.estimatedBytes);
        obj.set("recommendRemote", estimate.recommendRemote);
        return obj;
    }
    
    void precompute(std::string operation, val shapeIdsArray) {
        PrecomputeHint hint;
        hint.operation = operation;
        
        int len = shapeIdsArray["length"].as<int>();
        for (int i = 0; i < len; ++i) {
            hint.shapeIds.push_back(shapeIdsArray[i].as<std::string>());
        }
        
        engine_->precompute(hint);
    }
    
    // ==========================================================================
    // Health Check
    // ==========================================================================
    
    val healthCheck() {
        auto status = engine_->healthCheck();
        
        val obj = val::object();
        obj.set("healthy", status.healthy);
        obj.set("occtAvailable", status.occtAvailable);
        obj.set("version", status.version);
        obj.set("shapeCount", static_cast<int>(status.shapeCount));
        obj.set("memoryUsedBytes", static_cast<int>(status.memoryUsedBytes));
        obj.set("cacheHitRate", status.cacheHitRate);
        return obj;
    }
    
private:
    Engine* engine_;
};

// =============================================================================
// Emscripten Bindings
// =============================================================================

EMSCRIPTEN_BINDINGS(geom_core_cad) {
    class_<WasmCADEngine>("GeomCoreCAD")
        .constructor<>()
        
        // Lifecycle
        .function("initialize", &WasmCADEngine::initialize)
        .function("isInitialized", &WasmCADEngine::isInitialized)
        .function("getVersion", &WasmCADEngine::getVersion)
        .function("shutdown", &WasmCADEngine::shutdown)
        
        // Primitives
        .function("makeBox", &WasmCADEngine::makeBox)
        .function("makeSphere", &WasmCADEngine::makeSphere)
        .function("makeCylinder", &WasmCADEngine::makeCylinder)
        .function("makeCone", &WasmCADEngine::makeCone)
        .function("makeTorus", &WasmCADEngine::makeTorus)
        
        // Boolean operations
        .function("booleanUnion", &WasmCADEngine::booleanUnion)
        .function("booleanSubtract", &WasmCADEngine::booleanSubtract)
        .function("booleanIntersect", &WasmCADEngine::booleanIntersect)
        
        // Transforms
        .function("translate", &WasmCADEngine::translate)
        .function("rotate", &WasmCADEngine::rotate)
        .function("scale", &WasmCADEngine::scale)
        .function("mirror", &WasmCADEngine::mirror)
        
        // Analysis
        .function("tessellate", &WasmCADEngine::tessellate)
        .function("getVolume", &WasmCADEngine::getVolume)
        .function("getSurfaceArea", &WasmCADEngine::getSurfaceArea)
        .function("getBoundingBox", &WasmCADEngine::getBoundingBox)
        .function("getCenterOfMass", &WasmCADEngine::getCenterOfMass)
        
        // Memory management
        .function("disposeShape", &WasmCADEngine::disposeShape)
        .function("disposeAll", &WasmCADEngine::disposeAll)
        .function("getShapeCount", &WasmCADEngine::getShapeCount)
        .function("getMemoryUsage", &WasmCADEngine::getMemoryUsage)
        .function("getShapeHandle", &WasmCADEngine::getShapeHandle)
        
        // Zero-lag optimization
        .function("estimateComplexity", &WasmCADEngine::estimateComplexity)
        .function("precompute", &WasmCADEngine::precompute)
        
        // Health
        .function("healthCheck", &WasmCADEngine::healthCheck)
        ;
}
