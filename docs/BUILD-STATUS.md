# Build and verification boundaries

The native Python `Analyzer` contains mesh loading, volume, watertightness,
printability and orientation. Its CMake target uses the core analysis sources
and the existing `PyBindEntry.cpp`. Build with `scripts/build_python.sh` and
verify with `scripts/run_tests.sh`. STEP geometry additionally requires OCCT;
a successful API smoke without OCCT does not verify STEP tessellation.

`python -m build` builds a source distribution and a wheel from that distribution.
The wheel statically links the analysis library so it does not depend on a
checkout-local `libgeom_core`. CI installs it into a clean virtual environment
and imports the extension outside the source tree. Packaging defaults to two
parallel compiler jobs; `CMAKE_BUILD_PARALLEL_LEVEL` overrides that budget.

CAD authoring is a separate, experimental implementation. Native compilation
can be requested with `-DBUILD_NATIVE_CAD=ON`; WASM still builds the CAD sources
and must pass its own CI lane. Neither is validated by the Python analysis
suite. At the current baseline, missing OCCT wrappers, incomplete shape
implementations and inconsistent binding interfaces block that lane. Do not
publish or deploy a new CAD/WASM package based on Python-only success, or mask
these compiler failures with placeholder geometry or skipped build checks.

Python formatting and critical syntax/undefined-name checks fail CI on errors.
The previous C++ formatting placeholder was not a check; C++ compilation is
validated by the actual build jobs.

JavaScript dependencies are installed from `pnpm-lock.yaml` with pnpm 9.15.0;
the incomplete checked-in `node_modules` tree is removed. CI checks TypeScript
and builds the ESM, CommonJS and declaration outputs from a clean install.
There are currently no authored JavaScript test files, so `pnpm test` correctly
fails with "No test files found". A successful TypeScript build is not a runtime
CAD test, and that missing coverage remains a release limitation.
