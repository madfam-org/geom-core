# geom-core

[![CI/CD Pipeline](https://github.com/madfam-org/geom-core/actions/workflows/ci.yml/badge.svg)](https://github.com/madfam-org/geom-core/actions/workflows/ci.yml)
![Python 3.7+](https://img.shields.io/badge/python-3.7+-blue)
![C++17](https://img.shields.io/badge/c++-17-blue)
![License: Apache 2.0](https://img.shields.io/badge/License-Apache_2.0-blue.svg)
![Version 0.2.0](https://img.shields.io/badge/version-0.2.0-blue)

High-performance C++ geometry analysis engine for 3D printing, with Python and WebAssembly bindings.

Native mesh analysis (Python module and kernel-free WASM) is built and tested in
CI. CAD authoring is experimental and needs OCCT; without it every CAD
operation returns an explicit `OCCT_UNAVAILABLE`/`NOT_IMPLEMENTED` error. See
[build and verification boundaries](docs/BUILD-STATUS.md); mesh-analysis test
results do not validate CAD authoring.

## What geom-core is and is not

geom-core is the shared mesh-analysis layer of the MADFAM geometry factories:
**mesh in, numbers out**. One C++17 implementation, shipped as a Python module
and a WASM/TypeScript package.

| geom-core does | geom-core does not |
|---|---|
| Parse binary STL (STEP through optional OCCT) | Generate solids, patterns or sheets (yantra4d, fashion-cabinet, pliego kernels) |
| Watertight/manifold integrity with evidence, volume, surface area, bounding box, centroid, body count | Know a manifest, a contract or conformance rule (hyperobjects-spec) |
| Printability: overhang, wall thickness, auto-orientation | Know a price, a tier or a quote (Cotiza, Dhanam) |
| Per-triangle/per-vertex maps for visualisation | Serve HTTP, hold catalog state or authorise anyone |
| | Browser CSG (manifold-3d) or format conversion (trimesh) |

Consumers integrate it behind a feature flag with their current code as the
differential oracle; see [Adoption plan (R8)](#adoption-plan-r8).

## Features

- **Zero Dependencies**: Core library uses only C++17 standard library for lightweight builds
- **Binary STL Support**: Fast binary STL parser with vertex deduplication
- **STEP File Support**: Optional Open CASCADE Technology (OCCT) integration for CAD file import
- **Mesh Analysis**: Volume calculation, watertight checking, bounding box computation
- **Printability Analysis**: Overhang detection, wall thickness analysis with spatial acceleration
- **Auto-Orientation**: Automatically find optimal mesh orientation to minimize support material
- **Visualization Data Export**: Per-triangle overhang maps and per-vertex wall thickness for real-time visualization
- **Interactive 3D Viewer**: WebGL-based Solarpunk viewer with Three.js and zero-copy WASM integration
- **Cross-Platform**: Python bindings (Linux, macOS, Windows) and WebAssembly for browsers
- **Verification**: Native analysis tests and package checks; separate CAD/WASM build gates

## Installation

geom-core is **not on PyPI or npm yet**. Releases are distributed as GitHub
Release assets (see [Releases and versioning](#releases-and-versioning)).

### Python (from a GitHub Release)

```bash
# Pick the wheel for your platform and Python from the release page
pip install https://github.com/madfam-org/geom-core/releases/download/v0.2.0/<wheel-file>.whl
```

### TypeScript / WASM (from a GitHub Release)

```bash
npm install https://github.com/madfam-org/geom-core/releases/download/v0.2.0/madfam-geom-core-0.2.0.tgz
```

### Build from Source

```bash
git clone https://github.com/madfam-io/geom-core.git
cd geom-core
pip install .
```

### Optional: STEP File Support

To enable STEP file loading (requires Open CASCADE Technology):

**Ubuntu/Debian:**
```bash
sudo apt-get install libocct-data-exchange-dev libocct-ocaf-dev \
  libocct-modeling-data-dev libocct-modeling-algorithms-dev
cd geom-core
cmake -DUSE_OCCT=ON ..
make
```

**macOS:**
```bash
brew install opencascade
cd geom-core
cmake -DUSE_OCCT=ON ..
make
```

## Quick Start

### Python

```python
import geom_core_py as gc

# Load an STL file
analyzer = gc.Analyzer()
analyzer.load_stl("model.stl")

# Or load a STEP file (requires OCCT)
# analyzer.load_step("model.step", linear_deflection=0.1)

# Basic mesh info
print(f"Vertices: {analyzer.get_vertex_count()}")
print(f"Triangles: {analyzer.get_triangle_count()}")
print(f"Volume: {analyzer.get_volume()} mm³")
print(f"Watertight: {analyzer.is_watertight()}")

# Printability analysis
analyzer.build_spatial_index()
report = analyzer.get_printability_report(
    critical_angle_degrees=45.0,
    min_wall_thickness_mm=0.8
)
print(f"Printability Score: {report.score}/100")
print(f"Overhang Area: {report.overhang_area} mm²")
print(f"Thin Wall Vertices: {report.thin_wall_vertex_count}")

# Auto-orientation
result = analyzer.auto_orient()
print(f"Improvement: {result.improvement_percent}%")
print(f"Optimal Up Vector: ({result.optimal_up_vector.x}, "
      f"{result.optimal_up_vector.y}, {result.optimal_up_vector.z})")
```

### WebAssembly

```javascript
import createGeomCore from '@madfam/geom-core/wasm';

const mod = await createGeomCore();
const analyzer = new mod.Analyzer();
analyzer.loadSTLFromBytes(new Uint8Array(await file.arrayBuffer())); // throws GeomCoreSTLError on a bad STL

const { min, max, size } = analyzer.getBoundingBox();
const vertices = analyzer.getVertices();  // Float64Array [x0, y0, z0, ...] (welded)
const indices = analyzer.getIndices();    // Uint32Array  [a0, b0, c0, ...]

const report = analyzer.getPrintabilityReport(45.0, 0.8);
console.log(`Score: ${report.score}`);

// Visualization data (zero-copy typed-array views, see "Typed arrays" below)
const overhangMap = analyzer.getOverhangMapJS(45.0);       // Uint8Array
const wallThickness = analyzer.getWallThicknessMapJS(10.0); // Float32Array
analyzer.delete();
```

## Browser usage

`@madfam/geom-core/wasm` is a **single-threaded** ES-module build
(`USE_PTHREADS=0`, `EXPORT_ES6`, `MODULARIZE`, `ENVIRONMENT=web,worker,node`,
memory growth on). It needs no `SharedArrayBuffer`, starts no workers and
therefore needs **no COOP/COEP headers**: it loads on any page, in a worker
and in Node.

| Export | File | What |
|---|---|---|
| `@madfam/geom-core/wasm` | `dist/wasm/index.js` | default entry: `createGeomCore(options)` + `GeomCoreSTLError` |
| `@madfam/geom-core/wasm-binary` | `dist/wasm/geom-core.wasm` | its `.wasm`, for bundler `?url` imports |
| `@madfam/geom-core/threaded` | `dist/wasm/threaded.js` | opt-in pthread build (below) |
| `@madfam/geom-core/threaded/wasm-binary` | `dist/wasm/geom-core-threaded.wasm` | its `.wasm` |
| `@madfam/geom-core` | `dist/esm`, `dist/cjs` | TypeScript CAD SDK (unchanged) |

The WASM entries are ES modules only (no `require()`; `require.resolve` works).
`createGeomCore(options)` passes Emscripten module options through:
`locateFile(path)` maps `"geom-core.wasm"` to the URL to fetch, `wasmBinary`
supplies the bytes, `print`/`printErr` redirect C++ output.

### Vite

No alias and no server headers are needed:

```ts
import createGeomCore, { GeomCoreSTLError } from '@madfam/geom-core/wasm';
import wasmUrl from '@madfam/geom-core/wasm-binary?url';

const mod = await createGeomCore({
  locateFile: (path) => (path.endsWith('.wasm') ? wasmUrl : path),
});
```

The loader also finds its `.wasm` through `new URL('geom-core.wasm',
import.meta.url)`, which Vite rewrites to the same emitted asset, so
`locateFile` is optional with Vite; pass it when your bundler does not handle
that pattern or the `.wasm` is served from elsewhere. CI builds a Vite app this
way (`tests/wasm/vite-bundle.test.ts`) and checks that it emits the package's
own `.wasm`, no threaded build and no `new Worker(`.

### Errors are thrown, not returned

`loadSTLFromBytes(data)` and `loadSTL(path)` return `true` or throw
`GeomCoreSTLError` (`name === 'GeomCoreSTLError'`, `parserMessage` = the C++
parser's reason, `message` = `geom-core could not read the STL: <reason>`),
leaving no mesh loaded. `getLastError()` returns the same reason. The WASM
build no longer writes these to stderr.

### Typed arrays

| Method | Returns | Ownership |
|---|---|---|
| `getVertices()` | `Float64Array`, `3 × getVertexCount()` | **copy**, yours to keep |
| `getIndices()` | `Uint32Array`, `3 × getTriangleCount()` | **copy**, yours to keep |
| `getVerticesView()`, `getIndicesView()`, `getOverhangMapJS()`, `getWallThicknessMapJS()` | same types | **zero-copy view** into WASM memory |

A view is valid only until the next call that may allocate (memory growth
detaches it), the next load, or `delete()`; `.slice()` it to keep it.
Vertices are welded (exact duplicates merged), so `indices` index into a
shared vertex array, the input manifold-3d and three.js indexed geometry take.

### Threaded variant (`@madfam/geom-core/threaded`)

The same API built with pthreads: shared `WebAssembly.Memory` plus a 4-worker
pool started at load. Browsers allow shared memory only on
**cross-origin-isolated** pages, so the host must send

```
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

(and every subresource must be CORS/CORP-clean). Without `SharedArrayBuffer`,
or with `crossOriginIsolated === false`, `createGeomCoreThreaded()` rejects at
once with `GeomCoreThreadingError` instead of hanging in the pool start-up.
The loader spawns its workers with `new Worker(new URL(..., import.meta.url))`,
the pattern bundlers rewrite. Today no analysis call is parallelised, so the
threaded build buys nothing for mesh analysis; use it only when a host is
already isolated and a future parallel kernel needs it.

### 3D Viewer (Solarpunk Edition)

The project includes a beautiful WebGL-based viewer for real-time analysis visualization:

```bash
# Build WASM module first (requires Emscripten)
cd build-wasm
emcmake cmake .. -DBUILD_WASM=ON -DCMAKE_BUILD_TYPE=Release
emmake make

# Serve the viewer
cd ../viewer
python3 -m http.server 8080

# Open http://localhost:8080 in your browser
```

**Features:**
- 🎨 Color-coded overhang visualization (green = safe, red = needs support, blue = ground)
- 📏 Wall thickness gradient display
- 🔄 Interactive rotation, pan, and zoom controls
- 🌿 Beautiful solarpunk aesthetic with nature-inspired design
- ⚡ Zero-copy memory sharing between WASM and JavaScript

See [viewer/README.md](viewer/README.md) for detailed usage instructions.

## Architecture

### Core Components

- **Vector3**: 3D vector operations (dot product, cross product, normalization)
- **Matrix3**: 3x3 rotation matrices with Rodrigues' formula
- **Mesh**: Binary STL parser, volume calculation, manifold checking
- **Spatial**: AABB tree for ray casting acceleration (Möller-Trumbore intersection)
- **Analyzer**: High-level API wrapping all functionality

### Performance Optimizations

- **Vertex Deduplication**: O(N log N) using `std::map` during STL loading
- **Spatial Acceleration**: AABB tree with BVH for O(log N) ray queries
- **Auto-Orientation**: Tests orientations by rotating test vectors, not mesh vertices (1000x faster)

## Development

### Build Python Bindings

```bash
./scripts/build_python.sh
```

### Build WASM

```bash
# Requires Emscripten SDK (CI: 6.0.10) and Java 21 for Closure
./scripts/build_wasm.sh                     # both variants into dist/wasm/
./scripts/build_wasm.sh --variant single    # default build only (--no-threads is an alias)
pnpm test:wasm                              # Vitest on the single-threaded build (node + jsdom + Vite)
pnpm test:wasm:threaded                     # Node smoke test of the threaded build
```

### Run Tests

```bash
./scripts/run_tests.sh
```

### Run CI Locally

```bash
# Install act (GitHub Actions local runner)
# https://github.com/nektos/act
act -j python-build
```

## Testing

The project includes comprehensive tests for all features:

- `tests/test_mesh.py`: Basic mesh operations (Milestone 2)
- `tests/test_printability.py`: Printability analysis (Milestone 4)
- `tests/test_auto_orient.py`: Auto-orientation (Milestone 5)
- `tests/test_step.py`: STEP file loading API (Milestone 7)
- `tests/test_version.py`: pyproject.toml, CMake, package.json and `index.ts` agree on the version
- `tests/test_integrity_differential.py`: integrity, surface area, centroid and body count against the yantra4d reference and trimesh (needs `numpy` and `trimesh`)
- `tests/native/test_mesh_analysis.cpp`: C++ unit tests, run with `ctest --test-dir build`
- `tests/wasm/*.test.ts` (`pnpm test:wasm`, CI only; needs the Emscripten build): the single-threaded WASM in Node against yantra4d's differential fixtures (closed, offset, inside-out, two-body, sphere and sub-threshold meshes; volume 1e-9 relative, bbox and centroid 1e-9 mm), vertex/index round trips, the three malformed STLs, integrity verdicts, the export map, a jsdom page without `SharedArrayBuffer`, and a Vite app build
- `tests/wasm/threaded-smoke.mjs` (`pnpm test:wasm:threaded`, CI only): the threaded build in Node
- `tests/wasm-loader.test.ts`, `tests/wasm-types.test.ts` (`pnpm test`): entry-point wrapper logic, and the `MeshIntegrity` TypeScript type against the C++ struct

All tests run automatically via GitHub Actions on every push.

## API Reference

### Analyzer Class

The Python module is `geom_core_py` (there is no `geom_core` package and no
Python `Mesh` class): build an `Analyzer`, load a file, then query it.
Module attributes: `__version__`, `has_occt`. Classes: `Analyzer`, `Vector3`,
`BoundingBox`, `MeshIntegrity`, `PrintabilityReport`, `OrientationResult`.

#### Mesh Loading

| C++ (`Analyzer`) | Python (`geom_core_py.Analyzer`) | WASM (`Analyzer`) | Failure |
|---|---|---|---|
| `loadSTL(path)` | `load_stl(path)` | `loadSTL(path)` (virtual FS) | C++/Python: `false` + stderr; WASM: throws `GeomCoreSTLError` |
| `loadSTLFromBytes(data)` | — (no bytes loader) | `loadSTLFromBytes(Uint8Array)` | same |
| `loadStep(path, linDefl, angDefl)` | `load_step(path, linear_deflection=0.1, angular_deflection=0.5)` | — | `false` (needs OCCT; `has_occt`) |
| `getLastError()` | — | `getLastError()` | parser message of the last failed STL load |

Loads print nothing on success (0.1.0 printed a "Loaded STL" line to stdout).

#### Mesh Properties

| C++ (`Analyzer`) | Python (`geom_core_py.Analyzer`) | WASM (`Analyzer`) | Returns |
|---|---|---|---|
| `getVertexCount()` | `get_vertex_count()` | `getVertexCount()` | vertices after exact dedup |
| `getTriangleCount()` | `get_triangle_count()` | `getTriangleCount()` | triangles |
| `getVolume()` | `get_volume()` | `getVolume()` | **absolute** volume, mm³ (an inside-out solid is positive here) |
| `getSignedVolume()` | `get_signed_volume()` | `getSignedVolume()` | signed volume, mm³; negative when inside out |
| `getSurfaceArea()` | `get_surface_area()` | `getSurfaceArea()` | sum of triangle areas, mm² |
| `getBoundingBox()` | `get_bounding_box()` | — | `Vector3` size (width, height, depth) |
| `getBounds()` | `get_bounds()` | `getBoundingBox()` | `{min, max, size}` (WASM also keeps the deprecated 0.1.0 `x`/`y`/`z` = size) |
| — | — | `getVertices()` / `getIndices()` | `Float64Array` / `Uint32Array` copies (views: `getVerticesView()` / `getIndicesView()`) |
| `getCentroid()` | `get_centroid()` | `getCentroid()` | volumetric centroid `Vector3`; origin when \|volume\| < 1e-9 |
| `getBodyCount(mode)` | `get_body_count(mode="edge")` | `getBodyCount(mode = 'edge')` | connected bodies, see below |
| `isWatertight()` | `is_watertight()` | `isWatertight()` | plain edge-count check on the mesh as loaded |
| `assessIntegrity(scaleHint)` | `assess_integrity(scale_hint=None)` | `assessIntegrity(scaleHint)` (`<= 0` = derive) | `MeshIntegrity` |

**Body count connectivity.** `mode="edge"` (the default since 0.2.0) joins
faces that share an edge, like `len(trimesh.Trimesh.split(only_watertight=False))`;
`mode="vertex"` joins faces that share any vertex, which was 0.1.0's only rule
and matches `trimesh.Trimesh.body_count` (a vertex-graph count) on meshes
without unreferenced vertices. Two boxes touching at one corner are 2 bodies
by edge and 1 by vertex; trimesh 5.1.1 reports `split()` = 2 and
`body_count` = 1 for that mesh. `MeshIntegrity.bodyCount` is edge-connected.

The C++ `Mesh` class (not bound in Python or WASM) also exposes `getSurfaceArea()`, `getCentroid()`, `bodyCount()`
and `getSignedVolume()`, and `assessMeshIntegrity(mesh, scaleHint)` /
`Analyzer::assessIntegrity(mesh, scaleHint)` assess any `Mesh`.

#### Mesh integrity (`MeshIntegrity`)

A port of yantra4d's `mesh_integrity.assess` (relicensed; see the header of
`include/geom-core/Integrity.hpp`). It merges coincident vertices at
tolerances relative to the model's scale (`scale_hint`, default the largest
bounding-box dimension; tried at 1e-8, 1e-7, 1e-6, 1e-5 of it) before judging,
so STL float noise is not reported as a hole, and it says which failure it is:

| Field (Python / WASM) | Meaning |
|---|---|
| `watertight` | every edge shared by exactly two faces, as loaded or after the merge |
| `boundary_edges` / `boundaryEdges` | edges used by one face (holes) |
| `nonmanifold_edges` / `nonManifoldEdges` | edges shared by three or more faces (self-touching, e.g. boolean leftovers) |
| `manifold_edges` / `manifoldEdges` | edges shared by exactly two faces |
| `euler_number` / `eulerNumber` | V − E + F, `None`/`null` for an empty mesh |
| `winding_consistent` / `windingConsistent` | neighbours traverse shared edges in opposite directions |
| `volume` | signed volume (trimesh's formula; negative when inside out), so an inverted solid fails `inverted` even though `get_volume()` is positive |
| `merge_tolerance` / `mergeTolerance` | tolerance that sealed the mesh, else `None`/`null` |
| `degenerate_faces` / `degenerateFaces` | zero-area faces or repeated vertex indices |
| `inverted` | watertight, consistently wound, negative volume |
| `body_count` / `bodyCount` | connected bodies, faces sharing an edge |
| `notes`, `summary` | why, in words (the reference's wording, then notes for the added fields) |

`tests/test_integrity_differential.py` checks every reference field, note and
summary against a copy of the yantra4d algorithm on trimesh-generated fixtures.

#### Printability
- `build_spatial_index()`: Build AABB tree for analysis
- `get_printability_report(critical_angle, min_thickness)`: Analyze printability
- `auto_orient(sample_resolution, critical_angle)`: Find optimal orientation

#### Visualization Data Export (Milestone 8)
- C++ only (`calculateOverhangMap`, `calculateWallThicknessMap`; not bound in Python):
- `calculate_overhang_map(critical_angle)`: Get per-triangle overhang classification
  - Returns: `std::vector<uint8_t>` (0=safe, 1=overhang, 2=ground)
- `calculate_wall_thickness_map(max_distance)`: Get per-vertex wall thickness
  - Returns: `std::vector<float>` (wall thickness in mm)
- **WASM Only:**
  - `getOverhangMapJS(critical_angle)`: Returns Uint8Array view (zero-copy)
  - `getWallThicknessMapJS(max_distance)`: Returns Float32Array view (zero-copy)

### PrintabilityReport

- `score`: Overall printability (0-100)
- `overhang_area`: Surface area requiring support (mm²)
- `overhang_percentage`: Percentage of total surface
- `thin_wall_vertex_count`: Number of vertices with thin walls
- `total_surface_area`: Total mesh surface area (mm²)

### OrientationResult

- `optimal_up_vector`: Best orientation direction (Vector3)
- `original_overhang_area`: Before optimization (mm²)
- `optimized_overhang_area`: After optimization (mm²)
- `improvement_percent`: Percentage improvement (0-100)

## Milestones

- ✅ **Milestone 1**: Project scaffold with CMake + Python bindings
- ✅ **Milestone 2**: Mesh analysis (STL parser, volume, watertight)
- ✅ **Milestone 3**: WebAssembly support
- ✅ **Milestone 4**: Printability analysis (overhangs, wall thickness)
- ✅ **Milestone 5**: Auto-orientation algorithm
- ✅ **Milestone 6**: CI/CD pipeline & packaging
- ✅ **Milestone 7**: STEP file support via Open CASCADE Technology
- ✅ **Milestone 8**: Visualization data export & Solarpunk 3D viewer

## Technical Details

### Overhang Detection

Surfaces are considered overhangs if their normal angle from vertical exceeds the critical angle (typically 45°):

```
dot_product = normal · up_vector
if dot_product < -cos(critical_angle):
    surface is overhang
```

### Wall Thickness Analysis

For each vertex, cast a ray inward (along negative normal) and check distance to opposite wall:

```
ray = Ray(vertex + ε·normal, -normal)
hit = spatial_tree.ray_cast(ray, max_distance)
if hit.distance < min_thickness:
    vertex has thin wall
```

### Auto-Orientation

Tests 26 candidate orientations (6 cardinals + 12 edges + 8 corners) by rotating the test vector instead of the mesh:

```
for candidate_up_vector in sphere_samples:
    overhang_area = analyze_overhangs(candidate_up_vector)
    if overhang_area < best:
        best = candidate_up_vector
```

### Visualization Data Export (Milestone 8)

For real-time visualization in WebGL/Three.js, the engine exports granular per-element data:

**Overhang Map** (per triangle):
```cpp
std::vector<uint8_t> overhang_map(triangle_count);
// 0 = safe (self-supporting)
// 1 = overhang (needs support)
// 2 = ground (build platform contact)
```

**Wall Thickness Map** (per vertex):
```cpp
std::vector<float> thickness_map(vertex_count);
// Distance to nearest opposite wall in mm
```

**Zero-Copy WASM Integration:**
```cpp
// C++ side - return memory view directly
emscripten::val getOverhangMapJS(Analyzer& self, double angle) {
    const auto& data = self.calculateOverhangMap(angle);
    return emscripten::val(emscripten::typed_memory_view(data.size(), data.data()));
}

// JavaScript side - no copying, instant access
const overhangMap = analyzer.getOverhangMapJS(45);  // Uint8Array
for (let i = 0; i < triangleCount; i++) {
    applyColor(triangles[i], overhangMap[i]);
}
```

This approach enables real-time visualization of large meshes (100k+ triangles) with zero performance overhead.

## Releases and versioning

**Version source.** `pyproject.toml` (`[project] version`) is the single source
of truth. `CMakeLists.txt` reads it into `PROJECT_VERSION` and passes it to
every target as `GEOM_CORE_VERSION` (`geom_core_py.__version__`, the CAD
engine's `getVersion()`); `setup.py` lets setuptools read it from
`pyproject.toml`. `package.json` and `src/typescript/index.ts` cannot read a
file at runtime, so they carry the same literal and `tests/test_version.py`
fails CI if any of them disagree. To bump: edit those three files together.

**GitHub Releases.** `.github/workflows/release.yml` runs on a `v*` tag (the tag
must equal `v` + the version) and on manual dispatch (a dry run that only keeps
the artefacts on the workflow run). It builds and tests:

- Python wheels plus an sdist: Linux x86_64 for Python 3.8–3.12, built by
  `cibuildwheel` in the `manylinux_2_28` image and repaired by `auditwheel`
  (tags `manylinux_2_27_x86_64.manylinux_2_28_x86_64`: Debian bookworm and
  trixie, Ubuntu 20.04+, RHEL 8+), and macOS `universal2` (arm64 + x86_64) for
  3.10–3.12. Wheels are built **without OCCT** (`has_occt` is `False`; STEP
  loading is unavailable in released wheels). Releases up to 0.2.0 shipped
  plain `linux_x86_64` Linux wheels built on Ubuntu 24.04, which need
  `GLIBCXX_3.4.32` and `GLIBC_2.32` (Debian trixie or newer) and cannot go to
  PyPI.
- The npm tarball `madfam-geom-core-<version>.tgz`: the TypeScript SDK (ESM,
  CJS, types) plus the single-threaded WASM module (`./wasm`, the default) and
  the threaded one (`./threaded`, needs cross-origin isolation) in
  `dist/wasm/`.

and attaches them to the Release with the workflow's own `github.token`.

### Publishing to PyPI/npm (owner)

Not automated: it needs credentials only the owner holds and two decisions —
whether `@madfam/geom-core` goes to `npm.madfam.io` (private) or public npm,
and PyPI trusted publishing for `geom-core`. The Release job builds
`manylinux_2_28` wheels from the release after 0.2.0 on, which PyPI accepts.
Until publishing is set up, consumers pin a Release asset URL.

## Adoption plan (R8)

No repository consumes geom-core yet. The R8 audit
(internal-devops `audits/2026-09-04-geom-core-yantra4d-usage.md`) sets the order:

1. Make it installable and honest: one version, no mock APIs, real CI gates,
   Release artefacts (this repo).
2. Adopt yantra4d's stronger mesh-integrity check and expose surface area,
   centroid and body count (this repo).
3. First consumer: the **yantra4d browser path** (STL parse, bounding box,
   volume and centroid through the WASM package behind a feature flag, with
   yantra4d's own tests as the differential oracle). Then the yantra4d API
   path, hyperobjects-spec mesh gates, Cotiza, Blueprint and jewelry-studio.

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

Apache License 2.0 - see [LICENSE](LICENSE) for details

## Acknowledgments

- Built with [pybind11](https://github.com/pybind/pybind11) for Python bindings
- WASM support via [Emscripten](https://emscripten.org/)
- Inspired by the need for lightweight 3D printing analysis tools

## Support

- Issues: https://github.com/madfam-io/geom-core/issues
- Documentation: https://github.com/madfam-io/geom-core#readme
