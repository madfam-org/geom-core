#include "geom-core/Analyzer.hpp"
#include <iostream>
#include <cmath>

#ifdef GC_USE_OCCT
#include "BRepLoader.hpp"
#endif

namespace madfam::geom {

// Constructor
Analyzer::Analyzer() : mesh(std::make_unique<Mesh>()) {}

// Destructor
Analyzer::~Analyzer() = default;

// ========================================
// Real Mesh Analysis Methods (Milestone 2)
// ========================================

bool Analyzer::loadSTL(const std::string& filepath) {
    spatialTree.reset();
    if (!mesh) {
        mesh = std::make_unique<Mesh>();
    }
    return mesh->loadFromSTL(filepath);
}

bool Analyzer::loadSTLFromBytes(const std::string& data) {
    spatialTree.reset();
    if (!mesh) {
        mesh = std::make_unique<Mesh>();
    }
    return mesh->loadFromSTLBuffer(data.data(), data.size());
}

bool Analyzer::loadStep(const std::string& filepath,
                       double linearDeflection,
                       double angularDeflection) {
    spatialTree.reset();
    if (mesh) mesh->clear();
#ifdef GC_USE_OCCT
    if (!mesh) {
        mesh = std::make_unique<Mesh>();
    }

    // Use the BRepLoader to load and tessellate the STEP file
    bool success = brep::loadStepFile(filepath, *mesh, linearDeflection, angularDeflection);

    if (success) {
        std::cout << "Successfully loaded STEP file: " << filepath << std::endl;
        std::cout << "  Vertices: " << mesh->getVertexCount() << std::endl;
        std::cout << "  Triangles: " << mesh->getTriangleCount() << std::endl;
    } else {
        std::cerr << "Failed to load STEP file: " << filepath << std::endl;
    }

    return success;
#else
    std::cerr << "Error: geom-core was compiled without Open CASCADE support." << std::endl;
    std::cerr << "STEP file loading is not available." << std::endl;
    std::cerr << "To enable STEP support, rebuild with: cmake -DUSE_OCCT=ON .." << std::endl;
    (void)filepath;  // Suppress unused parameter warning
    (void)linearDeflection;
    (void)angularDeflection;
    return false;
#endif
}

double Analyzer::getVolume() const {
    if (!mesh) return 0.0;
    return mesh->getVolume();
}

bool Analyzer::isWatertight() const {
    if (!mesh) return false;
    return mesh->isWatertight();
}

Vector3 Analyzer::getBoundingBox() const {
    if (!mesh) return Vector3(0, 0, 0);
    return mesh->getBoundingBox();
}

size_t Analyzer::getVertexCount() const {
    if (!mesh) return 0;
    return mesh->getVertexCount();
}

size_t Analyzer::getTriangleCount() const {
    if (!mesh) return 0;
    return mesh->getTriangleCount();
}

// ========================================
// Printability Analysis (Milestone 4)
// ========================================

void Analyzer::buildSpatialIndex() {
    if (!mesh || mesh->getVertexCount() == 0) {
        std::cerr << "Error: Cannot build spatial index - no mesh loaded" << std::endl;
        return;
    }

    spatialTree = std::make_unique<AABBTree>();
    spatialTree->build(mesh->getVertices(), mesh->getFaces());
    std::cout << "Built spatial index for " << mesh->getTriangleCount() << " triangles" << std::endl;
}

PrintabilityReport Analyzer::getPrintabilityReport(double criticalAngleDegrees,
                                                    double minWallThicknessMM) {
    PrintabilityReport report;

    if (!mesh || mesh->getVertexCount() == 0) {
        std::cerr << "Error: No mesh loaded" << std::endl;
        return report;
    }

    const auto& vertices = mesh->getVertices();
    const auto& faces = mesh->getFaces();

    // ==================================
    // 1. Overhang Analysis
    // ==================================

    Vector3 upVector(0, 0, 1); // Z-up coordinate system
    double totalSurfaceArea = 0.0;
    double overhangArea = analyzeOverhangs(upVector, criticalAngleDegrees, totalSurfaceArea);

    report.totalSurfaceArea = totalSurfaceArea;
    report.overhangArea = overhangArea;
    report.overhangPercentage = (totalSurfaceArea > 0.0)
        ? (overhangArea / totalSurfaceArea * 100.0)
        : 0.0;

    // ==================================
    // 2. Wall Thickness Analysis
    // ==================================

    if (spatialTree && spatialTree->isBuilt()) {
        int thinWallCount = 0;

        // Sample vertices for thickness check (checking every vertex can be slow)
        // For large meshes, sample every N vertices
        size_t sampleRate = (vertices.size() > 10000) ? 10 : 1;

        for (size_t i = 0; i < vertices.size(); i += sampleRate) {
            const Vector3& vertex = vertices[i];

            // Compute vertex normal (average of adjacent face normals)
            Vector3 vertexNormal(0, 0, 0);
            int faceCount = 0;

            for (const auto& face : faces) {
                if (face.v0 == static_cast<int>(i) ||
                    face.v1 == static_cast<int>(i) ||
                    face.v2 == static_cast<int>(i)) {
                    const Vector3& v0 = vertices[face.v0];
                    const Vector3& v1 = vertices[face.v1];
                    const Vector3& v2 = vertices[face.v2];
                    vertexNormal = vertexNormal + calculateTriangleNormal(v0, v1, v2);
                    faceCount++;
                }
            }

            if (faceCount > 0) {
                vertexNormal = vertexNormal.normalized();

                // Cast ray inward (negative normal direction)
                const double epsilon = 0.001; // Offset to avoid self-intersection
                Ray ray(vertex + vertexNormal * epsilon, vertexNormal * -1.0);

                // Cast ray to find opposite wall
                RayHit hit = spatialTree->rayCast(ray, minWallThicknessMM * 10.0); // Search up to 10x min thickness

                if (hit.hit && hit.distance < minWallThicknessMM) {
                    thinWallCount++;
                }
            }
        }

        report.thinWallVertexCount = thinWallCount;
    } else {
        std::cerr << "Warning: Spatial index not built - skipping wall thickness analysis" << std::endl;
        std::cerr << "Call buildSpatialIndex() before getPrintabilityReport()" << std::endl;
    }

    // ==================================
    // 3. Calculate Overall Score
    // ==================================

    double score = 100.0;

    // Penalty for overhangs (up to 50 points)
    score -= std::min(report.overhangPercentage * 0.5, 50.0);

    // Penalty for thin walls (up to 50 points)
    if (vertices.size() > 0) {
        double thinWallRatio = static_cast<double>(report.thinWallVertexCount) / vertices.size();
        score -= std::min(thinWallRatio * 50.0, 50.0);
    }

    report.score = std::max(0.0, score);

    return report;
}

// ========================================
// Auto-Orientation (Milestone 5)
// ========================================

OrientationResult Analyzer::autoOrient(int sampleResolution, double criticalAngleDegrees) {
    OrientationResult result;

    if (!mesh || mesh->getVertexCount() == 0) {
        std::cerr << "Error: No mesh loaded for auto-orientation" << std::endl;
        return result;
    }

    // Calculate original overhang area (with Z-up)
    Vector3 originalUpVector(0, 0, 1);
    double originalTotalArea = 0.0;
    result.originalOverhangArea = analyzeOverhangs(originalUpVector, criticalAngleDegrees, originalTotalArea);

    // Generate candidate up vectors (sphere sampling)
    std::vector<Vector3> candidates;

    // 6 cardinal directions (±X, ±Y, ±Z)
    candidates.push_back(Vector3(1, 0, 0));
    candidates.push_back(Vector3(-1, 0, 0));
    candidates.push_back(Vector3(0, 1, 0));
    candidates.push_back(Vector3(0, -1, 0));
    candidates.push_back(Vector3(0, 0, 1));
    candidates.push_back(Vector3(0, 0, -1));

    // 12 edge directions (45° between two axes)
    const double inv_sqrt2 = 1.0 / std::sqrt(2.0);
    candidates.push_back(Vector3(inv_sqrt2, inv_sqrt2, 0).normalized());
    candidates.push_back(Vector3(inv_sqrt2, -inv_sqrt2, 0).normalized());
    candidates.push_back(Vector3(-inv_sqrt2, inv_sqrt2, 0).normalized());
    candidates.push_back(Vector3(-inv_sqrt2, -inv_sqrt2, 0).normalized());

    candidates.push_back(Vector3(inv_sqrt2, 0, inv_sqrt2).normalized());
    candidates.push_back(Vector3(inv_sqrt2, 0, -inv_sqrt2).normalized());
    candidates.push_back(Vector3(-inv_sqrt2, 0, inv_sqrt2).normalized());
    candidates.push_back(Vector3(-inv_sqrt2, 0, -inv_sqrt2).normalized());

    candidates.push_back(Vector3(0, inv_sqrt2, inv_sqrt2).normalized());
    candidates.push_back(Vector3(0, inv_sqrt2, -inv_sqrt2).normalized());
    candidates.push_back(Vector3(0, -inv_sqrt2, inv_sqrt2).normalized());
    candidates.push_back(Vector3(0, -inv_sqrt2, -inv_sqrt2).normalized());

    // 8 corner directions (±X, ±Y, ±Z all non-zero)
    const double inv_sqrt3 = 1.0 / std::sqrt(3.0);
    candidates.push_back(Vector3(inv_sqrt3, inv_sqrt3, inv_sqrt3).normalized());
    candidates.push_back(Vector3(inv_sqrt3, inv_sqrt3, -inv_sqrt3).normalized());
    candidates.push_back(Vector3(inv_sqrt3, -inv_sqrt3, inv_sqrt3).normalized());
    candidates.push_back(Vector3(inv_sqrt3, -inv_sqrt3, -inv_sqrt3).normalized());
    candidates.push_back(Vector3(-inv_sqrt3, inv_sqrt3, inv_sqrt3).normalized());
    candidates.push_back(Vector3(-inv_sqrt3, inv_sqrt3, -inv_sqrt3).normalized());
    candidates.push_back(Vector3(-inv_sqrt3, -inv_sqrt3, inv_sqrt3).normalized());
    candidates.push_back(Vector3(-inv_sqrt3, -inv_sqrt3, -inv_sqrt3).normalized());

    // Search for best orientation
    double bestOverhangArea = result.originalOverhangArea;
    Vector3 bestUpVector = originalUpVector;

    std::cout << "Testing " << candidates.size() << " orientations..." << std::endl;

    for (const auto& candidate : candidates) {
        double totalArea = 0.0;
        double overhangArea = analyzeOverhangs(candidate, criticalAngleDegrees, totalArea);

        if (overhangArea < bestOverhangArea) {
            bestOverhangArea = overhangArea;
            bestUpVector = candidate;
        }
    }

    // Fill result
    result.optimalUpVector = bestUpVector;
    result.optimizedOverhangArea = bestOverhangArea;

    if (result.originalOverhangArea > 0.0) {
        result.improvementPercent =
            ((result.originalOverhangArea - result.optimizedOverhangArea) /
             result.originalOverhangArea) * 100.0;
    } else {
        result.improvementPercent = 0.0;
    }

    std::cout << "Original overhang: " << result.originalOverhangArea << " mm²" << std::endl;
    std::cout << "Optimized overhang: " << result.optimizedOverhangArea << " mm²" << std::endl;
    std::cout << "Improvement: " << result.improvementPercent << "%" << std::endl;
    std::cout << "Optimal up vector: (" << bestUpVector.x << ", "
              << bestUpVector.y << ", " << bestUpVector.z << ")" << std::endl;

    return result;
}

// ========================================
// Visualization Data Export (Milestone 8)
// ========================================

const std::vector<uint8_t>& Analyzer::calculateOverhangMap(double criticalAngleDegrees) {
    // Clear and resize cache
    overhangMapCache.clear();

    if (!mesh || mesh->getVertexCount() == 0) {
        std::cerr << "Error: No mesh loaded for overhang map" << std::endl;
        return overhangMapCache;
    }

    const auto& vertices = mesh->getVertices();
    const auto& faces = mesh->getFaces();

    // Resize to triangle count
    overhangMapCache.resize(faces.size());

    // Z-up coordinate system
    Vector3 upVector(0, 0, 1);

    const double PI = 3.14159265358979323846;
    const double criticalAngleRad = criticalAngleDegrees * PI / 180.0;
    const double cosThreshold = std::cos(criticalAngleRad);

    // Ground-facing threshold (triangles pointing straight down, < -0.95)
    const double groundThreshold = -0.95;

    for (size_t i = 0; i < faces.size(); ++i) {
        const auto& face = faces[i];
        const Vector3& v0 = vertices[face.v0];
        const Vector3& v1 = vertices[face.v1];
        const Vector3& v2 = vertices[face.v2];

        // Calculate face normal
        Vector3 normal = calculateTriangleNormal(v0, v1, v2);

        // Dot product with up vector
        double dotProduct = normal * upVector;

        // Classify triangle
        if (dotProduct < groundThreshold) {
            // Triangle pointing straight down (build platform contact)
            overhangMapCache[i] = 2;
        } else if (dotProduct < -cosThreshold) {
            // Overhang requiring support
            overhangMapCache[i] = 1;
        } else {
            // Safe angle (self-supporting)
            overhangMapCache[i] = 0;
        }
    }

    std::cout << "Generated overhang map for " << faces.size() << " triangles" << std::endl;
    return overhangMapCache;
}

const std::vector<float>& Analyzer::calculateWallThicknessMap(double maxSearchDistanceMM) {
    // Clear and resize cache
    wallThicknessCache.clear();

    if (!mesh || mesh->getVertexCount() == 0) {
        std::cerr << "Error: No mesh loaded for wall thickness map" << std::endl;
        return wallThicknessCache;
    }

    if (!spatialTree || !spatialTree->isBuilt()) {
        std::cerr << "Error: Spatial index not built - call buildSpatialIndex() first" << std::endl;
        return wallThicknessCache;
    }

    const auto& vertices = mesh->getVertices();
    const auto& faces = mesh->getFaces();

    // Resize to vertex count
    wallThicknessCache.resize(vertices.size(), 0.0f);

    std::cout << "Calculating wall thickness for " << vertices.size() << " vertices..." << std::endl;

    // For each vertex, compute average normal and cast ray inward
    for (size_t i = 0; i < vertices.size(); ++i) {
        const Vector3& vertex = vertices[i];

        // Compute vertex normal (average of adjacent face normals)
        Vector3 vertexNormal(0, 0, 0);
        int faceCount = 0;

        for (const auto& face : faces) {
            if (face.v0 == static_cast<int>(i) ||
                face.v1 == static_cast<int>(i) ||
                face.v2 == static_cast<int>(i)) {
                const Vector3& v0 = vertices[face.v0];
                const Vector3& v1 = vertices[face.v1];
                const Vector3& v2 = vertices[face.v2];
                vertexNormal = vertexNormal + calculateTriangleNormal(v0, v1, v2);
                faceCount++;
            }
        }

        if (faceCount > 0) {
            vertexNormal = vertexNormal.normalized();

            // Cast ray inward (negative normal direction)
            const double epsilon = 0.001; // Offset to avoid self-intersection
            Ray ray(vertex + vertexNormal * epsilon, vertexNormal * -1.0);

            // Cast ray to find opposite wall
            RayHit hit = spatialTree->rayCast(ray, maxSearchDistanceMM);

            if (hit.hit) {
                wallThicknessCache[i] = static_cast<float>(hit.distance);
            } else {
                // No opposite wall found within search distance
                wallThicknessCache[i] = static_cast<float>(maxSearchDistanceMM);
            }
        }
    }

    std::cout << "Wall thickness calculation complete" << std::endl;
    return wallThicknessCache;
}

// ========================================
// Private Helper Methods
// ========================================

double Analyzer::analyzeOverhangs(const Vector3& upVector,
                                  double criticalAngleDegrees,
                                  double& outTotalArea) const {
    if (!mesh || mesh->getVertexCount() == 0) {
        outTotalArea = 0.0;
        return 0.0;
    }

    const double PI = 3.14159265358979323846;
    const double criticalAngleRad = criticalAngleDegrees * PI / 180.0;
    const double cosThreshold = std::cos(criticalAngleRad);

    const auto& vertices = mesh->getVertices();
    const auto& faces = mesh->getFaces();

    double totalSurfaceArea = 0.0;
    double overhangArea = 0.0;

    for (const auto& face : faces) {
        const Vector3& v0 = vertices[face.v0];
        const Vector3& v1 = vertices[face.v1];
        const Vector3& v2 = vertices[face.v2];

        // Calculate face normal
        Vector3 normal = calculateTriangleNormal(v0, v1, v2);

        // Calculate face area
        double area = calculateTriangleArea(v0, v1, v2);
        totalSurfaceArea += area;

        // Check if this is an overhang (facing downward)
        // Dot product with up vector: negative means facing down
        double dotProduct = normal * upVector;

        // If dot < -cos(angle), it's an overhang
        if (dotProduct < -cosThreshold) {
            overhangArea += area;
        }
    }

    outTotalArea = totalSurfaceArea;
    return overhangArea;
}

// ========================================
// Legacy Methods (for backward compatibility)
// ========================================

bool Analyzer::loadData(const std::string& data) {
    std::cout << "Loading data: " << data << std::endl;
    return true;
}

double Analyzer::getMockVolume(double base_radius) {
    // Volume of sphere: (4/3) * PI * r^3
    const double PI = 3.14159265358979323846;
    return (4.0 / 3.0) * PI * std::pow(base_radius, 3);
}

int Analyzer::add(int a, int b) {
    return a + b;
}

} // namespace madfam::geom
