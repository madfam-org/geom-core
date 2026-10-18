#pragma once
#include "Vector3.hpp"
#include <vector>
#include <string>

namespace madfam::geom {

/**
 * @brief Triangle face defined by 3 vertex indices
 */
struct Triangle {
    int v0, v1, v2;

    Triangle() : v0(0), v1(0), v2(0) {}
    Triangle(int a, int b, int c) : v0(a), v1(b), v2(c) {}
};

/**
 * @brief Axis-aligned bounding box of a mesh
 *
 * min and max are the corner coordinates; size = max - min (width, height,
 * depth). All three are (0, 0, 0) for an empty mesh.
 */
struct BoundingBox {
    Vector3 min;
    Vector3 max;
    Vector3 size;
};

/**
 * @brief Which faces count as connected when counting bodies
 *
 * Edge: faces sharing an edge (the default; matches
 * len(trimesh.Trimesh.split(only_watertight=False))). Vertex: faces sharing
 * any vertex (geom-core 0.1.0's rule; matches trimesh.Trimesh.body_count on
 * meshes without unreferenced vertices). Two boxes touching at one corner are
 * 2 bodies by edge and 1 by vertex.
 */
enum class BodyConnectivity { Edge, Vertex };

/**
 * @brief Triangular mesh data structure
 *
 * Stores vertices and faces for efficient topology analysis.
 * Supports loading from STL files and computing geometric properties.
 */
class Mesh {
public:
    Mesh() = default;
    ~Mesh() = default;

    /**
     * @brief Load mesh from binary STL file
     * @param filepath Path to the binary STL file
     * @return true if successful, false otherwise
     *
     * Automatically deduplicates vertices to create a proper connected mesh.
     */
    bool loadFromSTL(const std::string& filepath);

    /**
     * @brief Load mesh from binary STL data in memory
     * @param buffer Pointer to binary STL data
     * @param size Size of the buffer in bytes
     * @return true if successful, false otherwise
     *
     * Automatically deduplicates vertices to create a proper connected mesh.
     * This method is used for WASM where there's no file system.
     */
    bool loadFromSTLBuffer(const char* buffer, size_t size);

    /**
     * @brief Calculate the volume of the mesh using signed tetrahedron method
     * @return Volume in cubic units (mm³ if input is in mm)
     *
     * Formula: For each triangle (p1, p2, p3), compute:
     *   V += (1/6) * dot(p1, cross(p2, p3))
     */
    double getVolume() const;

    /**
     * @brief Check if the mesh is watertight (manifold)
     * @return true if every edge is shared by exactly 2 faces
     *
     * A manifold mesh has no holes or non-manifold edges.
     * This is a requirement for most 3D printing applications.
     */
    bool isWatertight() const;

    /**
     * @brief Get bounding box dimensions
     * @return Vector3 containing (width, height, depth); same as getBounds().size
     */
    Vector3 getBoundingBox() const;

    /**
     * @brief Axis-aligned bounding box with its corners
     * @return min, max and size (max - min); all zero when the mesh is empty
     */
    BoundingBox getBounds() const;

    /**
     * @brief Why the last loadFromSTL()/loadFromSTLBuffer() call failed
     * @return The parser message, or "" when the last load succeeded
     *
     * Native builds also print the message to stderr (as before); WASM
     * builds do not, and the JS binding throws it instead.
     */
    const std::string& getLastError() const { return lastError; }

    /**
     * @brief Signed volume from the signed-tetrahedron sum
     * @return Positive for outward (counter-clockwise) winding, negative for
     *         an inside-out mesh; meaningful only for a closed surface.
     */
    double getSignedVolume() const;

    /**
     * @brief Total surface area: the sum of triangle areas
     * @return Area in square units (mm² if input is in mm); 0 when empty
     */
    double getSurfaceArea() const;

    /**
     * @brief Volumetric centroid (centre of mass of a uniform solid)
     * @return Centroid from signed tetrahedra against the origin, which is
     *         independent of the winding direction. Returns (0, 0, 0) when
     *         |signed volume| < 1e-9 (empty or open/flat meshes), matching
     *         yantra4d's printEstimator.computeCentroid.
     */
    Vector3 getCentroid() const;

    /**
     * @brief Number of connected bodies
     * @param mode Edge (default): triangles sharing an edge are connected;
     *        Vertex: triangles sharing any vertex are. See BodyConnectivity.
     * @return Connected components of triangles (unreferenced vertices are
     *         not counted); 0 when empty
     */
    int bodyCount(BodyConnectivity mode = BodyConnectivity::Edge) const;

    /**
     * @brief Get the number of vertices in the mesh
     */
    size_t getVertexCount() const { return vertices.size(); }

    /**
     * @brief Get the number of triangles in the mesh
     */
    size_t getTriangleCount() const { return faces.size(); }

    /**
     * @brief Clear all mesh data
     */
    void clear();

    /**
     * @brief Get const reference to vertex array (for spatial indexing)
     */
    const std::vector<Vector3>& getVertices() const { return vertices; }

    /**
     * @brief Get const reference to face array (for spatial indexing)
     */
    const std::vector<Triangle>& getFaces() const { return faces; }

    /**
     * @brief Set vertices directly (for STEP loader and other importers)
     * @param verts Vector of vertices to set
     */
    void setVertices(const std::vector<Vector3>& verts) { vertices = verts; }

    /**
     * @brief Set triangles directly (for STEP loader and other importers)
     * @param tris Vector of triangles to set
     */
    void setTriangles(const std::vector<Triangle>& tris) { faces = tris; }

private:
    std::vector<Vector3> vertices;
    std::vector<Triangle> faces;
    std::string lastError;

    // Record a load failure: clears the mesh, stores the message, returns false.
    bool failLoad(const std::string& message);
};

} // namespace madfam::geom
