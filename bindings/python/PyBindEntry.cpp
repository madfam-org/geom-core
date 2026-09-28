#include <pybind11/pybind11.h>
#include "geom-core/Analyzer.hpp"
#include "geom-core/Vector3.hpp"

namespace py = pybind11;

PYBIND11_MODULE(geom_core_py, m) {
    m.doc() = "geom-core: High-performance geometry analysis library";
#ifdef GC_USE_OCCT
    m.attr("has_occt") = true;
#else
    m.attr("has_occt") = false;
#endif

    // PrintabilityReport struct
    py::class_<madfam::geom::PrintabilityReport>(m, "PrintabilityReport")
        .def(py::init<>())
        .def_readwrite("overhang_area", &madfam::geom::PrintabilityReport::overhangArea,
                      "Surface area requiring support (mm²)")
        .def_readwrite("overhang_percentage", &madfam::geom::PrintabilityReport::overhangPercentage,
                      "Percentage of total surface area that is overhang")
        .def_readwrite("thin_wall_vertex_count", &madfam::geom::PrintabilityReport::thinWallVertexCount,
                      "Number of vertices with walls too thin")
        .def_readwrite("score", &madfam::geom::PrintabilityReport::score,
                      "Overall printability score (0-100)")
        .def_readwrite("total_surface_area", &madfam::geom::PrintabilityReport::totalSurfaceArea,
                      "Total mesh surface area (mm²)")
        .def("__repr__", [](const madfam::geom::PrintabilityReport& r) {
            return "PrintabilityReport(score=" + std::to_string(r.score) +
                   ", overhang_area=" + std::to_string(r.overhangArea) +
                   " mm², overhang_percentage=" + std::to_string(r.overhangPercentage) +
                   "%, thin_wall_vertices=" + std::to_string(r.thinWallVertexCount) + ")";
        });

    // OrientationResult struct (Milestone 5)
    py::class_<madfam::geom::OrientationResult>(m, "OrientationResult")
        .def(py::init<>())
        .def_readwrite("optimal_up_vector", &madfam::geom::OrientationResult::optimalUpVector,
                      "The direction that should point 'Up' for optimal printing")
        .def_readwrite("original_overhang_area", &madfam::geom::OrientationResult::originalOverhangArea,
                      "Overhang area before optimization (mm²)")
        .def_readwrite("optimized_overhang_area", &madfam::geom::OrientationResult::optimizedOverhangArea,
                      "Overhang area after optimization (mm²)")
        .def_readwrite("improvement_percent", &madfam::geom::OrientationResult::improvementPercent,
                      "Percentage improvement (0-100)")
        .def("__repr__", [](const madfam::geom::OrientationResult& r) {
            return "OrientationResult(improvement=" + std::to_string(r.improvementPercent) +
                   "%, original=" + std::to_string(r.originalOverhangArea) +
                   " mm², optimized=" + std::to_string(r.optimizedOverhangArea) + " mm²)";
        });

    // Vector3 class
    py::class_<madfam::geom::Vector3>(m, "Vector3")
        .def(py::init<>())
        .def(py::init<double, double, double>())
        .def_readwrite("x", &madfam::geom::Vector3::x)
        .def_readwrite("y", &madfam::geom::Vector3::y)
        .def_readwrite("z", &madfam::geom::Vector3::z)
        .def("length", &madfam::geom::Vector3::length, "Get vector length/magnitude")
        .def("norm", &madfam::geom::Vector3::norm, "Get vector norm (alias for length)")
        .def("__repr__", [](const madfam::geom::Vector3& v) {
            return "Vector3(" + std::to_string(v.x) + ", " +
                   std::to_string(v.y) + ", " + std::to_string(v.z) + ")";
        });

    // Analyzer class
    py::class_<madfam::geom::Analyzer>(m, "Analyzer")
        .def(py::init<>())

        // Real mesh analysis methods (Milestone 2)
        .def("load_stl", &madfam::geom::Analyzer::loadSTL,
             "Load a mesh from binary STL file",
             py::arg("filepath"))
        .def("load_step", &madfam::geom::Analyzer::loadStep,
             "Load a mesh from STEP file (requires OCCT)",
             py::arg("filepath"),
             py::arg("linear_deflection") = 0.1,
             py::arg("angular_deflection") = 0.5)
        .def("get_volume", &madfam::geom::Analyzer::getVolume,
             "Calculate the volume of the loaded mesh")
        .def("is_watertight", &madfam::geom::Analyzer::isWatertight,
             "Check if the loaded mesh is watertight (manifold)")
        .def("get_bounding_box", &madfam::geom::Analyzer::getBoundingBox,
             "Get bounding box dimensions as Vector3(width, height, depth)")
        .def("get_vertex_count", &madfam::geom::Analyzer::getVertexCount,
             "Get number of vertices in the loaded mesh")
        .def("get_triangle_count", &madfam::geom::Analyzer::getTriangleCount,
             "Get number of triangles in the loaded mesh")

        // Printability analysis (Milestone 4)
        .def("build_spatial_index", &madfam::geom::Analyzer::buildSpatialIndex,
             "Build spatial acceleration structure for ray queries (required for thickness analysis)")
        .def("get_printability_report", &madfam::geom::Analyzer::getPrintabilityReport,
             "Analyze printability for 3D printing",
             py::arg("critical_angle_degrees") = 45.0,
             py::arg("min_wall_thickness_mm") = 0.8)

        // Auto-orientation (Milestone 5)
        .def("auto_orient", &madfam::geom::Analyzer::autoOrient,
             "Find optimal mesh orientation to minimize overhang area",
             py::arg("sample_resolution") = 26,
             py::arg("critical_angle_degrees") = 45.0)

        // Legacy methods (for backward compatibility)
        .def("load_data", &madfam::geom::Analyzer::loadData,
             "Load geometry data (placeholder - deprecated)",
             py::arg("data"))
        .def("get_mock_volume", &madfam::geom::Analyzer::getMockVolume,
             "Calculate mock volume using sphere formula: (4/3)*PI*r^3 (deprecated)",
             py::arg("base_radius"))
        .def("add", &madfam::geom::Analyzer::add,
             "Add two integers (sanity check)",
             py::arg("a"), py::arg("b"));
}
