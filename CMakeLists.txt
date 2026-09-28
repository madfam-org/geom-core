cmake_minimum_required(VERSION 3.15)
project(geom-core VERSION 2.0.0 LANGUAGES CXX)

# ===========================================================================
# Zero-Lag Geometry Engine
# Unified WASM module with OCCT support for browser + server compute
# ===========================================================================

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build Options
option(BUILD_SHARED_LIBS "Build a shared native analysis library" ON)
option(BUILD_NATIVE_CAD "Build the experimental native CAD authoring sources" OFF)
option(BUILD_PYTHON_BINDINGS "Build Python bindings using pybind11" ON)
option(BUILD_WASM_BINDINGS "Build WebAssembly bindings" OFF)
option(BUILD_WASM_SIMD "Enable SIMD optimizations in WASM" ON)
option(BUILD_WASM_THREADS "Enable multi-threading in WASM" ON)
option(BUILD_GPU_SUPPORT "Build with CUDA/GPU acceleration" OFF)
option(USE_OCCT "Enable Open CASCADE Technology" ON)
option(SPLIT_WASM_MODULES "Build split WASM modules for lazy loading" OFF)

# ===========================================================================
# OCCT Configuration (works for both native and WASM)
# ===========================================================================
set(OCCT_ENABLED FALSE)

if(USE_OCCT)
    if(EMSCRIPTEN)
        # For Emscripten, we need pre-compiled OCCT for WASM
        # Set OCCT_WASM_ROOT to the path containing OCCT WASM build
        if(DEFINED OCCT_WASM_ROOT)
            set(OCCT_ENABLED TRUE)
            set(OpenCASCADE_INCLUDE_DIR "${OCCT_WASM_ROOT}/include")
            set(OpenCASCADE_LIBRARY_DIR "${OCCT_WASM_ROOT}/lib")
            message(STATUS "Using OCCT WASM from: ${OCCT_WASM_ROOT}")
        else()
            message(WARNING "OCCT_WASM_ROOT not set. OCCT support disabled for WASM build.")
            message(WARNING "To enable: cmake -DOCCT_WASM_ROOT=/path/to/occt-wasm ...")
        endif()
    else()
        # Native build: find system OCCT
        find_package(OpenCASCADE QUIET)
        if(OpenCASCADE_FOUND)
            set(OCCT_ENABLED TRUE)
            message(STATUS "OpenCASCADE found: ${OpenCASCADE_VERSION}")
        else()
            find_package(PkgConfig QUIET)
            if(PkgConfig_FOUND)
                pkg_check_modules(OCCT QUIET OCCT)
                if(OCCT_FOUND)
                    set(OCCT_ENABLED TRUE)
                    set(OpenCASCADE_INCLUDE_DIR ${OCCT_INCLUDE_DIRS})
                    set(OpenCASCADE_LIBRARY_DIR ${OCCT_LIBRARY_DIRS})
                endif()
            endif()
        endif()

        if(NOT OCCT_ENABLED)
            message(WARNING "OpenCASCADE not found. Install with:")
            message(WARNING "  Ubuntu: sudo apt-get install libocct-data-exchange-dev")
            message(WARNING "  macOS: brew install opencascade")
        endif()
    endif()
endif()

# ===========================================================================
# Core Source Files
# ===========================================================================

# Base sources (always included)
set(CORE_SOURCES
    src/Analyzer.cpp
    src/Mesh.cpp
    src/Spatial.cpp
)

# CAD operations sources (new unified module)
set(CAD_SOURCES
    src/cad/Engine.cpp
    src/cad/Primitives.cpp
    src/cad/BooleanOps.cpp
    src/cad/Features.cpp
    src/cad/Transforms.cpp
    src/cad/ShapeRegistry.cpp
)

# File I/O sources
set(IO_SOURCES
    src/io/STLReader.cpp
    src/io/STLWriter.cpp
)

# OCCT-dependent sources
if(OCCT_ENABLED)
    list(APPEND CORE_SOURCES src/BRepLoader.cpp)
    list(APPEND IO_SOURCES
        src/io/STEPReader.cpp
        src/io/STEPWriter.cpp
        src/io/IGESReader.cpp
    )
    list(APPEND CAD_SOURCES
        src/cad/OCCTPrimitives.cpp
        src/cad/OCCTBoolean.cpp
        src/cad/OCCTFeatures.cpp
    )
endif()

# GPU sources (optional)
if(BUILD_GPU_SUPPORT AND NOT EMSCRIPTEN)
    enable_language(CUDA)
    set(GPU_SOURCES
        src/gpu/BooleanGPU.cu
        src/gpu/MeshingGPU.cu
        src/gpu/VoxelGrid.cu
    )
endif()

# ===========================================================================
# Native Library Build
# ===========================================================================
if(NOT EMSCRIPTEN)
    # The Python Analyzer exposes mesh analysis only. CAD authoring is built
    # by the separate WASM target or explicit experimental native opt-in.
    add_library(geom_core_lib
        ${CORE_SOURCES}
        ${GPU_SOURCES}
    )

    if(BUILD_NATIVE_CAD)
        target_sources(geom_core_lib PRIVATE ${CAD_SOURCES} ${IO_SOURCES})
    endif()

    target_include_directories(geom_core_lib
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>
    )

    # OCCT linking for native build
    if(OCCT_ENABLED)
        target_compile_definitions(geom_core_lib PRIVATE GC_USE_OCCT)
        target_include_directories(geom_core_lib PRIVATE ${OpenCASCADE_INCLUDE_DIR})

        set(OCCT_LIBS
            TKernel TKMath TKBRep TKTopAlgo TKMesh TKPrim TKGeomAlgo
            TKG3d TKG2d TKGeomBase TKShHealing TKFillet TKOffset TKBool
            TKBO TKFeat TKXSBase
        )
        # OCCT 7.8+ consolidated the data-exchange toolkits. Homebrew ships
        # these names; older Linux distributions still ship the TKSTEP family.
        find_library(GEOM_OCCT_MODERN_STEP NAMES TKDESTEP
            HINTS ${OpenCASCADE_LIBRARY_DIR})
        if(GEOM_OCCT_MODERN_STEP)
            list(APPEND OCCT_LIBS TKDESTEP TKDESTL TKDEIGES)
        else()
            list(APPEND OCCT_LIBS TKSTEP TKSTEPBase TKSTEPAttr TKSTEP209 TKSTL TKIGES)
        endif()

        foreach(lib ${OCCT_LIBS})
            find_library(${lib}_PATH NAMES ${lib}
                HINTS ${OpenCASCADE_LIBRARY_DIR})
            if(${lib}_PATH)
                target_link_libraries(geom_core_lib PRIVATE ${${lib}_PATH})
            else()
                target_link_libraries(geom_core_lib PRIVATE ${lib})
            endif()
        endforeach()
    endif()

    # CUDA linking
    if(BUILD_GPU_SUPPORT)
        find_package(CUDAToolkit REQUIRED)
        target_link_libraries(geom_core_lib PRIVATE CUDA::cudart)
        target_compile_definitions(geom_core_lib PRIVATE GC_USE_CUDA)
    endif()

    set_target_properties(geom_core_lib PROPERTIES
        POSITION_INDEPENDENT_CODE ON
        OUTPUT_NAME "geom_core"
        VERSION ${PROJECT_VERSION}
        SOVERSION 2
    )
endif()

# ===========================================================================
# WebAssembly Build (Optimized for Zero-Lag)
# ===========================================================================
if(BUILD_WASM_BINDINGS)
    # Base WASM link flags for zero-lag performance
    set(WASM_BASE_FLAGS
        "-lembind"
        "-s MODULARIZE=1"
        "-s ALLOW_MEMORY_GROWTH=1"
        "-s INITIAL_MEMORY=67108864"      # 64MB initial
        "-s MAXIMUM_MEMORY=1073741824"    # 1GB max
        "-s WASM_BIGINT=1"
        "-s EXPORTED_RUNTIME_METHODS=['ccall','cwrap','getValue','setValue']"
        "-s EXPORTED_FUNCTIONS=['_malloc','_free']"
        "--bind"
    )

    # Environment flags
    list(APPEND WASM_BASE_FLAGS "-s ENVIRONMENT=web,worker")

    # Threading support (requires SharedArrayBuffer)
    if(BUILD_WASM_THREADS)
        list(APPEND WASM_BASE_FLAGS
            "-s USE_PTHREADS=1"
            "-s PTHREAD_POOL_SIZE=navigator.hardwareConcurrency||4"
            "-s PROXY_TO_PTHREAD=1"  # Keep main thread responsive
        )
    endif()

    # SIMD support (90%+ browser coverage)
    if(BUILD_WASM_SIMD)
        list(APPEND WASM_BASE_FLAGS "-msimd128")
        add_compile_options(-msimd128)
    endif()

    # Optimization flags
    list(APPEND WASM_BASE_FLAGS
        "-O3"
        "-flto"
        "--closure 1"
        "-s MALLOC=emmalloc"  # Faster allocator
        "-s ASSERTIONS=0"      # Disable for production
    )

    if(SPLIT_WASM_MODULES)
        # =====================================================
        # Split Module Build (for lazy loading)
        # =====================================================

        # Base module: Primitives, transforms, basic analysis (~2MB)
        add_executable(geom_core_base
            src/Analyzer.cpp
            src/Mesh.cpp
            src/Spatial.cpp
            src/cad/Primitives.cpp
            src/cad/Transforms.cpp
            src/cad/ShapeRegistry.cpp
            src/io/STLReader.cpp
            bindings/wasm/WasmBase.cpp
        )
        target_include_directories(geom_core_base PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        string(REPLACE ";" " " WASM_BASE_FLAGS_STR "${WASM_BASE_FLAGS}")
        set_target_properties(geom_core_base PROPERTIES
            LINK_FLAGS "${WASM_BASE_FLAGS_STR} -s EXPORT_NAME='GeomCoreBase'"
            OUTPUT_NAME "geom-core-base"
            SUFFIX ".js"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm"
        )

        # Boolean module: Boolean operations (~3MB)
        add_executable(geom_core_boolean
            src/cad/BooleanOps.cpp
            bindings/wasm/WasmBoolean.cpp
        )
        target_include_directories(geom_core_boolean PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set_target_properties(geom_core_boolean PROPERTIES
            LINK_FLAGS "${WASM_BASE_FLAGS_STR} -s EXPORT_NAME='GeomCoreBoolean'"
            OUTPUT_NAME "geom-core-boolean"
            SUFFIX ".js"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm"
        )

        # OCCT module: STEP/IGES, advanced modeling (~15MB)
        if(OCCT_ENABLED)
            add_executable(geom_core_occt
                src/BRepLoader.cpp
                src/cad/OCCTPrimitives.cpp
                src/cad/OCCTBoolean.cpp
                src/cad/OCCTFeatures.cpp
                src/io/STEPReader.cpp
                src/io/STEPWriter.cpp
                bindings/wasm/WasmOCCT.cpp
            )
            target_include_directories(geom_core_occt PRIVATE
                ${CMAKE_CURRENT_SOURCE_DIR}/include
                ${OpenCASCADE_INCLUDE_DIR}
            )
            target_compile_definitions(geom_core_occt PRIVATE GC_USE_OCCT)
            # Link OCCT static libraries for WASM
            set_target_properties(geom_core_occt PROPERTIES
                LINK_FLAGS "${WASM_BASE_FLAGS_STR} -s EXPORT_NAME='GeomCoreOCCT' -L${OpenCASCADE_LIBRARY_DIR}"
                OUTPUT_NAME "geom-core-occt"
                SUFFIX ".js"
                RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm"
            )
        endif()

        # Analysis module: Printability, mesh analysis (~2MB)
        add_executable(geom_core_analysis
            src/Analyzer.cpp
            src/Spatial.cpp
            bindings/wasm/WasmAnalysis.cpp
        )
        target_include_directories(geom_core_analysis PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
        set_target_properties(geom_core_analysis PROPERTIES
            LINK_FLAGS "${WASM_BASE_FLAGS_STR} -s EXPORT_NAME='GeomCoreAnalysis'"
            OUTPUT_NAME "geom-core-analysis"
            SUFFIX ".js"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm"
        )

    else()
        # =====================================================
        # Unified Module Build (simpler, larger initial load)
        # =====================================================

        set(WASM_SOURCES
            ${CORE_SOURCES}
            ${CAD_SOURCES}
            ${IO_SOURCES}
            bindings/wasm/WasmEntry.cpp
            bindings/wasm/WasmCAD.cpp
        )

        add_executable(geom_core_wasm ${WASM_SOURCES})

        target_include_directories(geom_core_wasm PRIVATE
            ${CMAKE_CURRENT_SOURCE_DIR}/include
        )

        if(OCCT_ENABLED)
            target_compile_definitions(geom_core_wasm PRIVATE GC_USE_OCCT)
            target_include_directories(geom_core_wasm PRIVATE ${OpenCASCADE_INCLUDE_DIR})
            # Note: OCCT libs must be pre-compiled for Emscripten
        endif()

        string(REPLACE ";" " " WASM_FLAGS_STR "${WASM_BASE_FLAGS}")
        set_target_properties(geom_core_wasm PROPERTIES
            LINK_FLAGS "${WASM_FLAGS_STR} -s EXPORT_NAME='GeomCore'"
            OUTPUT_NAME "geom-core"
            SUFFIX ".js"
            RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm"
        )
    endif()

    message(STATUS "WASM build configured:")
    message(STATUS "  Threading: ${BUILD_WASM_THREADS}")
    message(STATUS "  SIMD: ${BUILD_WASM_SIMD}")
    message(STATUS "  Split modules: ${SPLIT_WASM_MODULES}")
endif()

# ===========================================================================
# Python Bindings
# ===========================================================================
if(BUILD_PYTHON_BINDINGS AND NOT EMSCRIPTEN)
    include(FetchContent)
    FetchContent_Declare(
        pybind11
        GIT_REPOSITORY https://github.com/pybind/pybind11.git
        GIT_TAG v2.11.1
    )
    FetchContent_MakeAvailable(pybind11)

    pybind11_add_module(geom_core_py
        bindings/python/PyBindEntry.cpp
    )

    target_link_libraries(geom_core_py PRIVATE geom_core_lib)
    if(OCCT_ENABLED)
        target_compile_definitions(geom_core_py PRIVATE GC_USE_OCCT)
    endif()

    set_target_properties(geom_core_py PROPERTIES
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/python"
    )

    message(STATUS "Python bindings enabled")
endif()

# ===========================================================================
# Summary
# ===========================================================================
message(STATUS "")
message(STATUS "===========================================")
message(STATUS "geom-core v${PROJECT_VERSION} Configuration")
message(STATUS "===========================================")
message(STATUS "Build Type:           ${CMAKE_BUILD_TYPE}")
message(STATUS "C++ Standard:         ${CMAKE_CXX_STANDARD}")
message(STATUS "OCCT Support:         ${OCCT_ENABLED}")
message(STATUS "Python Bindings:      ${BUILD_PYTHON_BINDINGS}")
message(STATUS "WASM Bindings:        ${BUILD_WASM_BINDINGS}")
if(BUILD_WASM_BINDINGS)
    message(STATUS "  WASM Threading:     ${BUILD_WASM_THREADS}")
    message(STATUS "  WASM SIMD:          ${BUILD_WASM_SIMD}")
    message(STATUS "  Split Modules:      ${SPLIT_WASM_MODULES}")
endif()
message(STATUS "GPU Support:          ${BUILD_GPU_SUPPORT}")
message(STATUS "===========================================")
