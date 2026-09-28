#pragma once

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include <functional>
#include "../Vector3.hpp"

namespace madfam::geom::cad {

// ===========================================================================
// Core Types for Zero-Copy WASM Integration
// ===========================================================================

/**
 * @brief Shape type classification
 */
enum class ShapeType {
    Solid,
    Surface,
    Curve,
    Point,
    Compound,
    Wire,
    Edge,
    Face,
    Shell,
    Unknown
};

/**
 * @brief Bounding box representation
 */
struct BoundingBox {
    Vector3 min{0, 0, 0};
    Vector3 max{0, 0, 0};
    
    Vector3 center() const {
        return Vector3(
            (min.x + max.x) / 2.0,
            (min.y + max.y) / 2.0,
            (min.z + max.z) / 2.0
        );
    }
    
    Vector3 size() const {
        return Vector3(
            max.x - min.x,
            max.y - min.y,
            max.z - min.z
        );
    }
    
    double volume() const {
        auto s = size();
        return s.x * s.y * s.z;
    }
};

/**
 * @brief Opaque shape handle - used for zero-copy between WASM and JS
 * 
 * The internal shape data stays in WASM memory, only metadata crosses the boundary.
 */
struct ShapeHandle {
    std::string id;           // Unique identifier
    ShapeType type;           // Shape classification
    BoundingBox bbox;         // Cached bounding box
    std::string hash;         // Content hash for caching
    
    // Optional computed properties (lazily filled)
    std::optional<double> volume;
    std::optional<double> surfaceArea;
    std::optional<Vector3> centerOfMass;
    
    bool isValid() const { return !id.empty(); }
};

/**
 * @brief Mesh data optimized for zero-copy transfer to Three.js
 * 
 * All arrays are stored contiguously for direct memory view access.
 */
struct MeshData {
    std::vector<float> positions;   // [x,y,z, x,y,z, ...] - vertex positions
    std::vector<float> normals;     // [nx,ny,nz, ...] - vertex normals
    std::vector<uint32_t> indices;  // Triangle indices
    std::vector<float> uvs;         // Optional UV coordinates
    
    // Metadata
    size_t vertexCount() const { return positions.size() / 3; }
    size_t triangleCount() const { return indices.size() / 3; }
    
    // Memory size for metrics
    size_t byteSize() const {
        return positions.size() * sizeof(float) +
               normals.size() * sizeof(float) +
               indices.size() * sizeof(uint32_t) +
               uvs.size() * sizeof(float);
    }
};

/**
 * @brief Tessellation options
 */
struct TessellateOptions {
    double linearDeflection = 0.1;   // Max distance from true surface (mm)
    double angularDeflection = 0.5;  // Max angle between facets (radians)
    bool relative = false;           // Deflection relative to bbox
    bool computeNormals = true;      // Generate smooth normals
    bool computeUVs = false;         // Generate UV coordinates
};

/**
 * @brief Operation result with error handling
 */
template<typename T>
struct Result {
    bool success = false;
    T value{};
    std::string errorCode;
    std::string errorMessage;
    
    // Performance metrics for monitoring
    double durationMs = 0;
    size_t memoryUsedBytes = 0;
    bool wasCached = false;
    
    static Result<T> ok(T val) {
        Result<T> r;
        r.success = true;
        r.value = std::move(val);
        return r;
    }
    
    static Result<T> error(const std::string& code, const std::string& msg) {
        Result<T> r;
        r.success = false;
        r.errorCode = code;
        r.errorMessage = msg;
        return r;
    }
};

// ===========================================================================
// Primitive Parameters
// ===========================================================================

struct BoxParams {
    double width = 100;
    double height = 100;
    double depth = 100;
    std::optional<Vector3> center;
};

struct SphereParams {
    double radius = 50;
    std::optional<Vector3> center;
};

struct CylinderParams {
    double radius = 50;
    double height = 100;
    std::optional<Vector3> center;
    Vector3 axis{0, 0, 1};
};

struct ConeParams {
    double radius1 = 50;  // Bottom radius
    double radius2 = 0;   // Top radius (0 = pointed cone)
    double height = 100;
    std::optional<Vector3> center;
    Vector3 axis{0, 0, 1};
};

struct TorusParams {
    double majorRadius = 50;  // Ring radius
    double minorRadius = 20;  // Tube radius
    std::optional<Vector3> center;
    Vector3 axis{0, 0, 1};
};

// ===========================================================================
// Boolean Operation Parameters
// ===========================================================================

struct BooleanUnionParams {
    std::vector<std::string> shapeIds;  // At least 2 shapes
};

struct BooleanSubtractParams {
    std::string baseId;                  // Shape to subtract from
    std::vector<std::string> toolIds;    // Shapes to subtract
};

struct BooleanIntersectParams {
    std::vector<std::string> shapeIds;  // At least 2 shapes
};

// ===========================================================================
// Feature Operation Parameters
// ===========================================================================

struct ExtrudeParams {
    std::string profileId;   // Wire or face to extrude
    Vector3 direction{0, 0, 1};
    double distance = 100;
    double draftAngle = 0;   // Optional draft angle (radians)
};

struct RevolveParams {
    std::string profileId;   // Wire or face to revolve
    Vector3 axisOrigin{0, 0, 0};
    Vector3 axisDirection{0, 0, 1};
    double angle = 6.283185307;  // 2π = full revolution
};

struct SweepParams {
    std::string profileId;   // Cross-section
    std::string pathId;      // Sweep path (wire)
    bool frenet = true;      // Use Frenet frame
};

struct LoftParams {
    std::vector<std::string> profileIds;  // At least 2 profiles
    bool ruled = false;      // Ruled surface vs smooth
    bool closed = false;     // Close the loft
};

struct FilletParams {
    std::string shapeId;
    double radius = 5;
    std::vector<std::string> edgeIds;  // Empty = all edges
};

struct ChamferParams {
    std::string shapeId;
    double distance = 5;
    std::vector<std::string> edgeIds;  // Empty = all edges
};

struct ShellParams {
    std::string shapeId;
    double thickness = 2;
    std::vector<std::string> faceIdsToRemove;  // Faces to open
};

struct OffsetParams {
    std::string shapeId;
    double distance = 1;     // Positive = outward
    bool joinArcs = true;    // Arc vs intersection join
};

// ===========================================================================
// Transform Parameters
// ===========================================================================

struct TranslateParams {
    std::string shapeId;
    Vector3 offset{0, 0, 0};
};

struct RotateParams {
    std::string shapeId;
    Vector3 axisOrigin{0, 0, 0};
    Vector3 axisDirection{0, 0, 1};
    double angle = 0;  // Radians
};

struct ScaleParams {
    std::string shapeId;
    Vector3 center{0, 0, 0};
    double factor = 1.0;
    // Or non-uniform:
    std::optional<Vector3> factors;  // {scaleX, scaleY, scaleZ}
};

struct MirrorParams {
    std::string shapeId;
    Vector3 planePoint{0, 0, 0};
    Vector3 planeNormal{1, 0, 0};  // Default: YZ plane (mirror in X)
};

// 4x4 transformation matrix (row-major)
struct Matrix4x4 {
    double m[16] = {
        1, 0, 0, 0,
        0, 1, 0, 0,
        0, 0, 1, 0,
        0, 0, 0, 1
    };
};

struct MatrixTransformParams {
    std::string shapeId;
    Matrix4x4 matrix;
};

// ===========================================================================
// Compute Hints for Zero-Lag Optimization
// ===========================================================================

/**
 * @brief Hint for precomputation (speculative execution)
 */
struct PrecomputeHint {
    std::string operation;
    std::vector<std::string> shapeIds;
    std::optional<std::string> expectedResultId;  // For caching
};

/**
 * @brief Operation complexity estimate
 */
struct ComplexityEstimate {
    double score;           // 0-1, higher = more complex
    size_t estimatedMs;     // Expected duration
    size_t estimatedBytes;  // Expected memory usage
    bool recommendRemote;   // Should offload to server?
};

} // namespace madfam::geom::cad
