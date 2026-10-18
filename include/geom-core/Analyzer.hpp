#pragma once
#include <string>
#include <memory>
#include <optional>
#include "Mesh.hpp"
#include "Integrity.hpp"
#include "Vector3.hpp"
#include "Spatial.hpp"

namespace madfam::geom {

    /**
     * @brief Printability analysis report for 3D printing
     */
    struct PrintabilityReport {
        double overhangArea;         // mm² of surface requiring support
        double overhangPercentage;   // % of total surface area
        int thinWallVertexCount;     // Number of vertices with walls too thin
        double score;                // Overall printability score (0-100)
        double totalSurfaceArea;     // Total mesh surface area (mm²)

        PrintabilityReport()
            : overhangArea(0.0)
            , overhangPercentage(0.0)
            , thinWallVertexCount(0)
            , score(100.0)
            , totalSurfaceArea(0.0) {}
    };

    /**
     * @brief Auto-orientation optimization result (Milestone 5)
     */
    struct OrientationResult {
        Vector3 optimalUpVector;        // The direction that should point "Up"
        double originalOverhangArea;    // Overhang area before optimization (mm²)
        double optimizedOverhangArea;   // Overhang area after optimization (mm²)
        double improvementPercent;      // Percentage improvement (0-100)

        OrientationResult()
            : optimalUpVector(0, 0, 1)
            , originalOverhangArea(0.0)
            , optimizedOverhangArea(0.0)
            , improvementPercent(0.0) {}
    };

    /**
     * @brief High-level geometry analysis interface
     *
     * Provides convenient methods for loading and analyzing 3D models.
     * Wraps the lower-level Mesh class for ease of use in bindings.
     */
    class Analyzer {
    public:
        Analyzer();
        ~Analyzer();

        // ========================================
        // Real Mesh Analysis Methods (Milestone 2)
        // ========================================

        /**
         * @brief Load a mesh from an STL file
         * @param filepath Path to binary STL file
         * @return true if successful, false otherwise
         */
        bool loadSTL(const std::string& filepath);

        /**
         * @brief Load a mesh from STL data in memory (for WASM)
         * @param data Binary STL data as a string
         * @return true if successful, false otherwise
         *
         * In JavaScript/WASM, you can pass a Uint8Array or binary string.
         * Emscripten will automatically convert it to std::string.
         */
        bool loadSTLFromBytes(const std::string& data);

        /**
         * @brief Load a mesh from a STEP file (requires OCCT)
         * @param filepath Path to STEP file (.step or .stp)
         * @param linearDeflection Tessellation precision in mm (default 0.1mm)
         * @param angularDeflection Angular precision in radians (default 0.5)
         * @return true if successful, false otherwise
         *
         * This method requires Open CASCADE Technology (OCCT) to be installed.
         * If compiled without OCCT support, this method will return false and
         * print an error message.
         *
         * The linearDeflection parameter controls the maximum distance between
         * the tessellated mesh and the original CAD surface. Smaller values
         * produce more accurate (but larger) meshes.
         */
        bool loadStep(const std::string& filepath,
                     double linearDeflection = 0.1,
                     double angularDeflection = 0.5);

        /**
         * @brief Calculate the volume of the loaded mesh
         * @return Volume in cubic units (0.0 if no mesh loaded)
         */
        double getVolume() const;

        /**
         * @brief Check if the loaded mesh is watertight (manifold)
         * @return true if watertight, false otherwise
         */
        bool isWatertight() const;

        /**
         * @brief Get bounding box dimensions
         * @return Vector3 containing (width, height, depth); see getBounds()
         */
        Vector3 getBoundingBox() const;

        /**
         * @brief Axis-aligned bounding box: min and max corners plus size
         * @return All zero when no mesh is loaded
         */
        BoundingBox getBounds() const;

        /**
         * @brief Parser message of the last failed loadSTL()/loadSTLFromBytes()
         * @return "" when the last load succeeded (or nothing was loaded)
         */
        std::string getLastError() const;

        /**
         * @brief Total surface area of the loaded mesh (mm² for mm input)
         */
        double getSurfaceArea() const;

        /**
         * @brief Volumetric centroid of the loaded mesh, see Mesh::getCentroid()
         */
        Vector3 getCentroid() const;

        /**
         * @brief Number of connected bodies in the loaded mesh, see Mesh::bodyCount()
         */
        int getBodyCount() const;

        /**
         * @brief Watertight/manifold verdict with evidence for a mesh
         * @param mesh Mesh to assess
         * @param scaleHint Characteristic size (mm); absent or 0 uses the
         *        largest bounding-box dimension. See assessMeshIntegrity().
         */
        static MeshIntegrity assessIntegrity(const Mesh& mesh,
                                             std::optional<double> scaleHint = std::nullopt);

        /**
         * @brief Watertight/manifold verdict with evidence for the loaded mesh
         *
         * Supersedes isWatertight() for a verdict: it merges coincident
         * vertices at a scale-relative tolerance before judging and reports
         * boundary vs non-manifold edges, winding and orientation.
         */
        MeshIntegrity assessIntegrity(std::optional<double> scaleHint = std::nullopt) const;

        /**
         * @brief Welded vertices of the loaded mesh (empty when none)
         */
        const std::vector<Vector3>& getVertices() const;

        /**
         * @brief Triangles of the loaded mesh as indices into getVertices()
         */
        const std::vector<Triangle>& getFaces() const;

        /**
         * @brief Get number of vertices in the loaded mesh
         */
        size_t getVertexCount() const;

        /**
         * @brief Get number of triangles in the loaded mesh
         */
        size_t getTriangleCount() const;

        // ========================================
        // Printability Analysis (Milestone 4)
        // ========================================

        /**
         * @brief Build spatial acceleration structure for ray queries
         *
         * Call this after loading a mesh and before running printability analysis.
         * Required for wall thickness checks.
         */
        void buildSpatialIndex();

        /**
         * @brief Analyze printability for 3D printing
         *
         * @param criticalAngleDegrees Overhang angle threshold (typically 45°)
         * @param minWallThicknessMM Minimum wall thickness in mm (typically 0.8-2.0)
         * @return Printability report with overhang and thickness analysis
         *
         * The score is calculated as:
         * - Start at 100.0
         * - Subtract (overhangPercentage * 0.5) for support requirements
         * - Subtract (thinWallVertexCount / totalVertices * 50) for thin walls
         */
        PrintabilityReport getPrintabilityReport(double criticalAngleDegrees,
                                                 double minWallThicknessMM);

        // ========================================
        // Auto-Orientation (Milestone 5)
        // ========================================

        /**
         * @brief Find optimal mesh orientation to minimize overhang area
         *
         * @param sampleResolution Number of test orientations (default 26)
         * @param criticalAngleDegrees Overhang angle threshold (default 45°)
         * @return OrientationResult with optimal up vector and improvement metrics
         *
         * Uses discrete sphere sampling to test multiple orientations without
         * rotating the mesh vertices. Tests cardinals + 45° offsets.
         */
        OrientationResult autoOrient(int sampleResolution = 26,
                                     double criticalAngleDegrees = 45.0);

        // ========================================
        // Visualization Data Export (Milestone 8)
        // ========================================

        /**
         * @brief Calculate per-triangle overhang classification map
         *
         * @param criticalAngleDegrees Overhang angle threshold (typically 45°)
         * @return Vector of uint8_t with size = triangle count
         *         Values: 0 = Safe, 1 = Overhang, 2 = Ground-facing
         *
         * This method provides granular data for visualization. Each triangle
         * is classified based on its orientation relative to the build platform.
         * Ground-facing triangles (2) are those pointing nearly straight down
         * and don't need support. Overhangs (1) need support structures.
         */
        const std::vector<uint8_t>& calculateOverhangMap(double criticalAngleDegrees);

        /**
         * @brief Calculate per-vertex wall thickness values
         *
         * @param minWallThicknessMM Minimum acceptable wall thickness (typically 0.8-2.0mm)
         * @return Vector of float with size = vertex count
         *         Values: Distance to nearest opposite wall in mm
         *
         * This method provides wall thickness data for visualization.
         * Requires buildSpatialIndex() to be called first.
         * Returns 0.0 for vertices where no opposite wall is found.
         */
        const std::vector<float>& calculateWallThicknessMap(double maxSearchDistanceMM);

    private:
        std::unique_ptr<Mesh> mesh;
        std::unique_ptr<AABBTree> spatialTree;

        // Cached visualization data (Milestone 8)
        std::vector<uint8_t> overhangMapCache;
        std::vector<float> wallThicknessCache;

        /**
         * @brief Calculate overhang area for a given up vector
         * @param upVector The direction considered "up" for this analysis
         * @param criticalAngleDegrees Overhang angle threshold
         * @param outTotalArea Output parameter for total surface area
         * @return Overhang area in mm²
         */
        double analyzeOverhangs(const Vector3& upVector,
                               double criticalAngleDegrees,
                               double& outTotalArea) const;
    };
}
