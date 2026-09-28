# Build and verification boundaries

The native Python `Analyzer` contains mesh loading, volume, watertightness,
printability and orientation. Its CMake target uses the core analysis sources
and the existing `PyBindEntry.cpp`. Build with `scripts/build_python.sh` and
verify with `scripts/run_tests.sh`. STEP geometry additionally requires OCCT;
a successful API smoke without OCCT does not verify STEP tessellation. Native
CI sets `GEOM_REQUIRE_OCCT=1` and verifies a generated 10 mm STEP cube: it
must be watertight, have 1,000 mm³ volume, and measure 10 mm on each axis.
The module reports `has_occt`; a missing required capability fails CI.

`python -m build` builds a source distribution and a wheel from that distribution.
The wheel statically links the analysis library so it does not depend on a
checkout-local `libgeom_core`. CI installs it into a clean virtual environment
and imports the extension outside the source tree. Packaging defaults to two
parallel compiler jobs; `CMAKE_BUILD_PARALLEL_LEVEL` overrides that budget.

CAD authoring is a separate, experimental implementation. Native compilation
can be requested with `-DBUILD_NATIVE_CAD=ON`; WASM still builds the CAD sources
and must pass its own CI lane. Neither is validated by the Python analysis
suite. The default kernel-free WASM build now compiles the CAD interfaces but
returns `OCCT_UNAVAILABLE` for primitives, matching the explicit failure behavior
of booleans and features. It never reports a bounding-box placeholder as a solid.
Local threaded WASM execution verifies a real STL cube's 1,000 mm³ volume,
watertightness and 10 mm dimensions, plus malformed-input rejection. CI repeats
these checks and also builds/runs without threads. Closure requires Java 21.
Mesh arrays returned from temporary CAD operation results are copied into
JavaScript-owned arrays; they cannot outlive freed C++ vectors.

This does not validate CAD authoring with OCCT in WASM. The optional OCCT and
split-module targets still reference absent source files and require separate
implementation and verification. Keep the package unreleased until its declared
capabilities and target-specific release gates are satisfied; Python analysis or
kernel-free WASM success is not evidence of browser CAD boolean support.

Python formatting and critical syntax/undefined-name checks fail CI on errors.
The previous C++ formatting placeholder was not a check; C++ compilation is
validated by the actual build jobs.

JavaScript dependencies are installed from `pnpm-lock.yaml` with pnpm 9.15.0;
the incomplete checked-in `node_modules` tree is removed. CI checks TypeScript
and builds the ESM, CommonJS and declaration outputs from a clean install.
The package-entrypoint tests invoke real Node ESM and CommonJS consumers after
the build. This catches unresolved extensionless imports and module-format
metadata failures that TypeScript or Vitest's resolver can hide. These tests
verify loading the SDK, not CAD operations; missing CAD runtime coverage remains
a release limitation.
