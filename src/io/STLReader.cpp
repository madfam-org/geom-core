/** STL readers share the same mesh/result interfaces as the CAD engine. */
#include "geom-core/cad/Types.hpp"
#include <fstream>
#include <sstream>
#include <cstring>
#include <cmath>
#include <limits>

namespace madfam::geom::io {
using namespace madfam::geom::cad;
namespace {
Result<MeshData> invalid(const std::string& message) {
    return Result<MeshData>::error("INVALID_DATA", message);
}

Result<MeshData> readBinary(std::istream& stream, size_t size) {
    char header[80];
    uint32_t count = 0;
    if (!stream.read(header, 80) || !stream.read(reinterpret_cast<char*>(&count), 4))
        return invalid("Truncated STL header");
    // Validate before allocation, using wide arithmetic even on wasm32.
    if (uint64_t(size) != 84ULL + uint64_t(count) * 50ULL ||
        count > std::numeric_limits<uint32_t>::max() / 3)
        return invalid("STL triangle count does not match body size");
    MeshData mesh;
    mesh.positions.reserve(size_t(count) * 9);
    mesh.normals.reserve(size_t(count) * 9);
    mesh.indices.reserve(size_t(count) * 3);
    for (uint32_t i = 0; i < count; ++i) {
        float values[12];
        uint16_t attribute = 0;
        if (!stream.read(reinterpret_cast<char*>(values), sizeof(values)) ||
            !stream.read(reinterpret_cast<char*>(&attribute), 2))
            return invalid("Truncated STL triangle");
        for (float value : values)
            if (!std::isfinite(value)) return invalid("Non-finite STL coordinate");
        for (int v = 0; v < 3; ++v) {
            mesh.positions.insert(mesh.positions.end(), values + 3 + v * 3, values + 6 + v * 3);
            mesh.normals.insert(mesh.normals.end(), values, values + 3);
            mesh.indices.push_back(i * 3 + v);
        }
    }
    return Result<MeshData>::ok(std::move(mesh));
}

Result<MeshData> readASCII(std::istream& stream) {
    MeshData mesh;
    std::string line, word;
    if (!std::getline(stream, line)) return invalid("Empty STL");
    std::istringstream first(line);
    if (!(first >> word) || word != "solid") return invalid("Missing ASCII STL solid header");
    auto expect = [&](const char* wanted) { return bool(stream >> word) && word == wanted; };
    while (stream >> word) {
        if (word == "endsolid") return Result<MeshData>::ok(std::move(mesh));
        if (word != "facet" || !expect("normal")) return invalid("Invalid STL facet");
        float normal[3];
        if (!(stream >> normal[0] >> normal[1] >> normal[2])) return invalid("Invalid facet normal");
        for (float n : normal) if (!std::isfinite(n)) return invalid("Non-finite normal");
        if (!expect("outer") || !expect("loop")) return invalid("Invalid outer loop");
        for (int v = 0; v < 3; ++v) {
            float xyz[3];
            if (!expect("vertex") || !(stream >> xyz[0] >> xyz[1] >> xyz[2]))
                return invalid("Invalid STL vertex");
            for (float n : xyz) if (!std::isfinite(n)) return invalid("Non-finite vertex");
            if (mesh.vertexCount() >= std::numeric_limits<uint32_t>::max())
                return invalid("STL exceeds index capacity");
            mesh.indices.push_back(static_cast<uint32_t>(mesh.vertexCount()));
            mesh.positions.insert(mesh.positions.end(), xyz, xyz + 3);
            mesh.normals.insert(mesh.normals.end(), normal, normal + 3);
        }
        if (!expect("endloop") || !expect("endfacet")) return invalid("Unterminated STL facet");
    }
    return invalid("Missing endsolid");
}

Result<MeshData> readStream(std::istream& stream, size_t size) {
    if (size >= 84) {
        stream.seekg(80);
        uint32_t count = 0;
        stream.read(reinterpret_cast<char*>(&count), 4);
        stream.clear();
        stream.seekg(0);
        if (uint64_t(size) == 84ULL + uint64_t(count) * 50ULL)
            return readBinary(stream, size);
    }
    return readASCII(stream);
}
} // namespace

Result<MeshData> readSTL(const std::string& filepath) {
    std::ifstream file(filepath, std::ios::binary | std::ios::ate);
    if (!file.is_open()) return Result<MeshData>::error("IO_ERROR", "Failed to open file: " + filepath);
    const auto size = file.tellg();
    if (size < 0) return Result<MeshData>::error("IO_ERROR", "Failed to stat STL");
    file.seekg(0);
    return readStream(file, static_cast<size_t>(size));
}

Result<MeshData> readSTLFromMemory(const uint8_t* data, size_t size) {
    if (!data || !size) return invalid("Empty STL buffer");
    // Validate binary lengths before copying into the stream or allocating mesh
    // arrays. ASCII input has no count; its syntax is checked by readASCII.
    if (size < 5 || std::memcmp(data, "solid", 5) != 0) {
        if (size < 84) return invalid("Truncated STL header");
        uint32_t count = 0;
        std::memcpy(&count, data + 80, 4);
        if (uint64_t(size) != 84ULL + uint64_t(count) * 50ULL)
            return invalid("STL triangle count does not match body size");
    }
    std::istringstream stream(std::string(reinterpret_cast<const char*>(data), size), std::ios::binary);
    return readStream(stream, size);
}
} // namespace madfam::geom::io
