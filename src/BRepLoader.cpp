#ifdef GC_USE_OCCT

#include "BRepLoader.hpp"
#include "geom-core/Mesh.hpp"

// Open CASCADE includes
#include <STEPControl_Reader.hxx>
#include <BRepMesh_IncrementalMesh.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Shape.hxx>
#include <TopoDS_Face.hxx>
#include <TopExp_Explorer.hxx>
#include <Poly_Triangulation.hxx>
#include <BRep_Tool.hxx>
#include <TopLoc_Location.hxx>
#include <gp_Pnt.hxx>
#include <TColgp_Array1OfPnt.hxx>
#include <Poly_Array1OfTriangle.hxx>
#include <Poly_Triangle.hxx>

#include <iostream>
#include <map>
#include <vector>

namespace madfam::geom::brep {

bool loadStepFile(const std::string& filepath,
                  Mesh& outMesh,
                  double linearDeflection,
                  double angularDeflection) {
    
    std::cout << "Loading STEP file: " << filepath << std::endl;
    std::cout << "Linear deflection: " << linearDeflection << " mm" << std::endl;
    
    // ========================================
    // Step 1: Read STEP file
    // ========================================
    STEPControl_Reader reader;
    IFSelect_ReturnStatus status = reader.ReadFile(filepath.c_str());
    
    if (status != IFSelect_RetDone) {
        std::cerr << "Error: Failed to read STEP file: " << filepath << std::endl;
        return false;
    }
    
    std::cout << "STEP file read successfully" << std::endl;
    
    // Transfer roots to internal data structure
    Standard_Integer nbRoots = reader.TransferRoots();
    if (nbRoots == 0) {
        std::cerr << "Error: No roots transferred from STEP file" << std::endl;
        return false;
    }
    
    std::cout << "Transferred " << nbRoots << " roots" << std::endl;
    
    // Get the resulting shape
    TopoDS_Shape shape = reader.OneShape();
    if (shape.IsNull()) {
        std::cerr << "Error: Resulting shape is null" << std::endl;
        return false;
    }
    
    // ========================================
    // Step 2: Tessellate the shape
    // ========================================
    std::cout << "Tessellating shape..." << std::endl;
    
    BRepMesh_IncrementalMesh mesher(shape, linearDeflection, Standard_False, 
                                     angularDeflection, Standard_True);
    
    if (!mesher.IsDone()) {
        std::cerr << "Error: Mesh generation failed" << std::endl;
        return false;
    }
    
    std::cout << "Tessellation complete" << std::endl;
    
    // ========================================
    // Step 3: Extract triangulation data
    // ========================================
    
    // We need to build a unified vertex list and triangle list
    // OCCT provides per-face triangulations with local indices
    
    std::vector<Vector3> vertices;
    std::vector<Triangle> triangles;
    
    // Map from (face_ptr, local_vertex_index) to global vertex index
    // We use a map to deduplicate vertices across faces
    std::map<Vector3, int> vertexMap;
    
    int faceCount = 0;
    int triangleCount = 0;
    
    // Iterate over all faces in the shape
    for (TopExp_Explorer faceExp(shape, TopAbs_FACE); faceExp.More(); faceExp.Next()) {
        const TopoDS_Face& face = TopoDS::Face(faceExp.Current());
        faceCount++;
        
        // Get the triangulation for this face
        TopLoc_Location loc;
        Handle(Poly_Triangulation) triangulation = BRep_Tool::Triangulation(face, loc);
        
        if (triangulation.IsNull()) {
            // Face has no triangulation - skip it
            continue;
        }
        
        // Get transformation matrix for this face
        gp_Trsf transform = loc.Transformation();
        
        // Build local vertex map for this face
        std::vector<int> localToGlobal(triangulation->NbNodes() + 1); // OCCT uses 1-based indexing
        
        // Process vertices
        for (Standard_Integer i = 1; i <= triangulation->NbNodes(); i++) {
            // Indexed access works with current OCCT triangulation storage.
            gp_Pnt pt = triangulation->Node(i);
            
            // Apply transformation
            pt.Transform(transform);
            
            // Convert to our Vector3
            Vector3 vertex(pt.X(), pt.Y(), pt.Z());
            
            // Check if vertex already exists (deduplication)
            auto it = vertexMap.find(vertex);
            int globalIndex;
            
            if (it != vertexMap.end()) {
                // Vertex already exists
                globalIndex = it->second;
            } else {
                // New vertex
                globalIndex = static_cast<int>(vertices.size());
                vertices.push_back(vertex);
                vertexMap[vertex] = globalIndex;
            }
            
            // Map local index to global index
            localToGlobal[i] = globalIndex;
        }
        
        // Process triangles
        for (Standard_Integer i = 1; i <= triangulation->NbTriangles(); i++) {
            const Poly_Triangle tri = triangulation->Triangle(i);
            
            // Get vertex indices (1-based in OCCT)
            Standard_Integer n1, n2, n3;
            tri.Get(n1, n2, n3);
            
            // Check face orientation (reversed if face is flipped)
            bool reversed = (face.Orientation() == TopAbs_REVERSED);
            
            // Convert to global indices and create triangle
            Triangle triangle;
            if (reversed) {
                // Reverse winding order
                triangle.v0 = localToGlobal[n1];
                triangle.v1 = localToGlobal[n3];
                triangle.v2 = localToGlobal[n2];
            } else {
                triangle.v0 = localToGlobal[n1];
                triangle.v1 = localToGlobal[n2];
                triangle.v2 = localToGlobal[n3];
            }
            
            triangles.push_back(triangle);
            triangleCount++;
        }
    }
    
    std::cout << "Extracted " << faceCount << " faces" << std::endl;
    std::cout << "Generated " << vertices.size() << " vertices" << std::endl;
    std::cout << "Generated " << triangles.size() << " triangles" << std::endl;
    
    if (triangles.empty()) {
        std::cerr << "Error: No triangles extracted from STEP file" << std::endl;
        return false;
    }
    
    // ========================================
    // Step 4: Populate the Mesh object
    // ========================================
    
    // Clear existing data
    outMesh.clear();
    
    // Set vertices and triangles
    outMesh.setVertices(vertices);
    outMesh.setTriangles(triangles);
    
    std::cout << "STEP file loaded successfully into mesh" << std::endl;
    std::cout << "Final mesh: " << outMesh.getVertexCount() << " vertices, "
              << outMesh.getTriangleCount() << " triangles" << std::endl;
    
    return true;
}

} // namespace madfam::geom::brep

#endif // GC_USE_OCCT
