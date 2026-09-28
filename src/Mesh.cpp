#include "geom-core/Mesh.hpp"
#include <fstream>
#include <map>
#include <unordered_map>
#include <algorithm>
#include <limits>
#include <cstring>
#include <cstdint>
#include <iostream>
#include <cmath>

namespace madfam::geom {

bool Mesh::loadFromSTL(const std::string& filepath) {
    clear();
    // Read entire file into memory
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open STL file: " << filepath << std::endl;
        return false;
    }

    // Get file size and read into buffer
    std::streamsize size = file.tellg();
    if (size < 84) return false;
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        std::cerr << "Error: Failed to read STL file: " << filepath << std::endl;
        return false;
    }

    file.close();

    // Delegate to buffer-based loader
    return loadFromSTLBuffer(buffer.data(), buffer.size());
}

bool Mesh::loadFromSTLBuffer(const char* buffer, size_t size) {
    // Clear existing data
    clear();

    // Validate minimum size (80-byte header + 4-byte count)
    if (!buffer || size < 84) {
        std::cerr << "Error: STL buffer too small (< 84 bytes)" << std::endl;
        return false;
    }

    // Read 80-byte header (skip it)
    size_t offset = 80;

    // Read triangle count (4 bytes, little-endian unsigned int)
    uint32_t triangleCount;
    std::memcpy(&triangleCount, buffer + offset, 4);
    offset += 4;

    // Divide the available byte count before allocating. Multiplying the
    // uint32 count by 50 first can wrap and admit a tiny malicious buffer.
    if (triangleCount > (size - 84) / 50 ||
        triangleCount > static_cast<size_t>(std::numeric_limits<int>::max()) / 3) {
        std::cerr << "Error: STL triangle count exceeds the buffer or index capacity" << std::endl;
        return false;
    }

    // Map to deduplicate vertices: vertex -> index
    std::map<Vector3, int> vertexMap;
    int nextVertexIndex = 0;

    // Reserve space for efficiency
    faces.reserve(triangleCount);

    // Read each triangle
    for (uint32_t i = 0; i < triangleCount; ++i) {
        // Check if we have enough data left
        if (offset + 50 > size) {
            std::cerr << "Error: Unexpected end of STL buffer at triangle " << i << std::endl;
            return false;
        }

        // Read normal vector (12 bytes) - we'll skip it
        offset += 12;

        // Read 3 vertices (12 bytes each = 3 floats per vertex)
        int indices[3];
        for (int j = 0; j < 3; ++j) {
            float coords[3];
            std::memcpy(coords, buffer + offset, 12);
            offset += 12;

            if (!std::isfinite(coords[0]) || !std::isfinite(coords[1]) ||
                !std::isfinite(coords[2])) {
                clear();
                std::cerr << "Error: STL contains non-finite coordinates" << std::endl;
                return false;
            }
            Vector3 vertex(coords[0], coords[1], coords[2]);

            // Check if vertex already exists in map
            auto it = vertexMap.find(vertex);
            if (it != vertexMap.end()) {
                // Vertex exists, reuse index
                indices[j] = it->second;
            } else {
                // New vertex, add to map and vertices list
                indices[j] = nextVertexIndex;
                vertexMap[vertex] = nextVertexIndex;
                vertices.push_back(vertex);
                nextVertexIndex++;
            }
        }

        // Create triangle from indices
        faces.emplace_back(indices[0], indices[1], indices[2]);

        // Read attribute byte count (2 bytes) - skip it
        offset += 2;
    }

    std::cout << "Loaded STL: " << vertices.size() << " vertices, "
              << faces.size() << " triangles" << std::endl;

    return true;
}

double Mesh::getVolume() const {
    if (faces.empty()) {
        return 0.0;
    }

    double volume = 0.0;

    // For each triangle, calculate signed tetrahedron volume
    // Formula: V = (1/6) * dot(p1, cross(p2, p3))
    for (const auto& face : faces) {
        const Vector3& p1 = vertices[face.v0];
        const Vector3& p2 = vertices[face.v1];
        const Vector3& p3 = vertices[face.v2];

        // Calculate cross product: p2 x p3
        Vector3 crossProduct = p2 % p3;

        // Calculate dot product: p1 · (p2 x p3)
        double dotProduct = p1 * crossProduct;

        // Add signed volume contribution
        volume += dotProduct;
    }

    // Divide by 6 and take absolute value
    return std::abs(volume / 6.0);
}

bool Mesh::isWatertight() const {
    if (faces.empty()) {
        return false;
    }

    // Edge map: (vertex_i, vertex_j) -> count
    // We use min/max to ensure consistent edge representation
    std::map<std::pair<int, int>, int> edgeCount;

    // Iterate over all faces and count edges
    for (const auto& face : faces) {
        // Each triangle has 3 edges
        int edges[3][2] = {
            {face.v0, face.v1},
            {face.v1, face.v2},
            {face.v2, face.v0}
        };

        for (int i = 0; i < 3; ++i) {
            int a = edges[i][0];
            int b = edges[i][1];

            // Normalize edge representation (smaller index first)
            std::pair<int, int> edge = (a < b) ? std::make_pair(a, b) : std::make_pair(b, a);

            // Increment counter
            edgeCount[edge]++;
        }
    }

    // Check if all edges have exactly 2 faces
    for (const auto& entry : edgeCount) {
        if (entry.second != 2) {
            // Found non-manifold edge or boundary edge
            return false;
        }
    }

    return true;
}

Vector3 Mesh::getBoundingBox() const {
    if (vertices.empty()) {
        return Vector3(0, 0, 0);
    }

    double minX = std::numeric_limits<double>::max();
    double minY = std::numeric_limits<double>::max();
    double minZ = std::numeric_limits<double>::max();
    double maxX = std::numeric_limits<double>::lowest();
    double maxY = std::numeric_limits<double>::lowest();
    double maxZ = std::numeric_limits<double>::lowest();

    for (const auto& vertex : vertices) {
        minX = std::min(minX, vertex.x);
        minY = std::min(minY, vertex.y);
        minZ = std::min(minZ, vertex.z);
        maxX = std::max(maxX, vertex.x);
        maxY = std::max(maxY, vertex.y);
        maxZ = std::max(maxZ, vertex.z);
    }

    return Vector3(maxX - minX, maxY - minY, maxZ - minZ);
}

void Mesh::clear() {
    vertices.clear();
    faces.clear();
}

} // namespace madfam::geom
