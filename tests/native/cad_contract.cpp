#include "geom-core/cad/Engine.hpp"
#include <cstring>
#include <limits>
#include <stdexcept>
#include <iostream>

namespace madfam::geom::io {
using namespace madfam::geom::cad;
Result<MeshData> readSTLFromMemory(const uint8_t*, size_t);
Result<std::vector<uint8_t>> writeSTLToMemory(const MeshData&, bool);
}
using namespace madfam::geom::cad;
void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }

int main() {
    double scalar = 2;
    require(Result<double>::ok(scalar).value == 2, "lvalue result");
    require(*Result<std::unique_ptr<int>>::ok(std::make_unique<int>(3)).value == 3, "move-only result");
    Engine engine;
    engine.initialize();
    require(!engine.healthCheck().occtAvailable, "this test requires a kernel-free build");
    auto box = engine.makeBox(BoxParams{});
    require(!box.success && box.errorCode == "OCCT_UNAVAILABLE", "no placeholder CAD success");
    require(engine.getShapeCount() == 0, "no placeholder registered");
    ShapeRegistry::instance().recordOperation(1);

    MeshData mesh;
    mesh.positions = {0.123456789f,0,0, 1,0,0, 0,1,0};
    mesh.indices = {0,1,2};
    for (bool binary : {true, false}) {
        auto bytes = madfam::geom::io::writeSTLToMemory(mesh, binary);
        require(bytes.success, "STL writer");
        auto parsed = madfam::geom::io::readSTLFromMemory(bytes.value.data(), bytes.value.size());
        require(parsed.success && parsed.value.triangleCount() == 1, "STL round trip");
        require(parsed.value.positions == mesh.positions, "STL coordinates preserved");
    }
    mesh.positions = {0,0,0, 1e38f,0,0, 0,1e38f,0};
    auto large = madfam::geom::io::writeSTLToMemory(mesh, true);
    require(large.success, "finite large-coordinate STL written");
    auto largeRead = madfam::geom::io::readSTLFromMemory(large.value.data(), large.value.size());
    require(largeRead.success && largeRead.value.normals[2] == 1, "large-coordinate normal stays finite");
    std::vector<uint8_t> hostile(84, 0);
    uint32_t count = std::numeric_limits<uint32_t>::max();
    std::memcpy(hostile.data() + 80, &count, 4);
    require(!madfam::geom::io::readSTLFromMemory(hostile.data(), hostile.size()).success, "oversized count rejected");
    require(!madfam::geom::io::readSTLFromMemory(hostile.data(), 20).success, "truncated header rejected");
    const char* invalid = "solid x\nfacet normal 0 0 1\nouter loop\nvertex invalid\n";
    require(!madfam::geom::io::readSTLFromMemory(reinterpret_cast<const uint8_t*>(invalid), std::strlen(invalid)).success, "malformed ASCII rejected");
    mesh.indices[0] = 999;
    require(!madfam::geom::io::writeSTLToMemory(mesh, true).success, "invalid index rejected");
    mesh.indices[0] = 0;
    mesh.positions[0] = std::numeric_limits<float>::quiet_NaN();
    require(!madfam::geom::io::writeSTLToMemory(mesh, true).success, "non-finite vertex rejected");
    std::cout << "CAD capability and STL contracts passed\n";
}
