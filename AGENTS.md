# Geom Core Agent Operating Guide

> [!IMPORTANT]
> MADFAM-ENCLII-FIRST-LEGACY-RAW v1: This document contains legacy raw infrastructure command examples.
> Routine production operations must use Enclii web, API, or CLI. Treat raw
> `kubectl`, `helm`, SSH, provider CLI/API, `docker exec`, and direct container
> access as platform bootstrap or documented break-glass only, and record any
> missing Enclii adapter gap.


<!-- MADFAM-AGENTS-CANONICAL v1 -->

This is the canonical instruction file for Claude, Codex, and any other LLM
agent working in this repository. `CLAUDE.md` is kept only as a compatibility
redirect and should not become the source of truth again.

## Required operating doctrine

- Read this file before making repo changes.
- Prefer existing repo conventions, scripts, and docs over introducing new
  patterns.
- Preserve user work and never revert unrelated changes.
- Treat production operations as Enclii-first: use Enclii web, API, or CLI for
  provisioning, deployment, observability, domains, secrets, provider
  operations, scaling, rollback, and remediation.
- Use direct `kubectl`, `helm`, SSH, provider CLIs/APIs, `docker exec`, or
  direct container access only for platform bootstrap or documented break-glass
  emergencies when Enclii is unavailable or lacks an implemented adapter.
- Record any missing Enclii adapter gap instead of normalizing raw production
  access in docs or runbooks.

## Repo entrypoints

- `README.md`
- `ECOSYSTEM.md`
- `docs/`
- `.github/workflows/`

## LLM context files

- `llms.txt` is the compact context index.
- `llms-full.txt` is the durable full-context map and operating contract.
- `AGENTS.md` is canonical for agent instructions.
- `CLAUDE.md` redirects here for Claude compatibility.

## Maintenance

Regenerate or repair these files with
`internal-devops/scripts/sync-agent-docs.py` from the labspace ecosystem.

---

## Legacy CLAUDE.md guidance imported on 2026-05-13

<!-- BEGIN LEGACY_CLAUDE_IMPORT -->

# geom-core - CLAUDE.md

> **High-Performance Geometry Analysis for 3D Printing**

## Overview

**Status**: 🟢 Production Ready (All 8 Milestones Complete)  
**Purpose**: C++ geometry analysis engine with Python and WebAssembly bindings  
**License**: Apache 2.0 (permissive, enterprise-friendly)  
**Package**: `@madfam/geom-core` (npm), `geom-core` (PyPI)

geom-core is the **physics standard** for the MADFAM ecosystem. It provides mesh analysis, printability scoring, and auto-orientation for 3D printing workflows.

---

## Quick Start

### Python
```bash
pip install geom-core
```

```python
import geom_core_py as gc

analyzer = gc.Analyzer()
analyzer.load_stl("model.stl")

print(f"Volume: {analyzer.get_volume()} mm³")
print(f"Watertight: {analyzer.is_watertight()}")

# Printability analysis
analyzer.build_spatial_index()
report = analyzer.get_printability_report(45.0, 0.8)
print(f"Score: {report.score}/100")

# Auto-orientation
result = analyzer.auto_orient()
print(f"Improvement: {result.improvement_percent}%")
```

### WebAssembly (Browser)
```javascript
const geomCore = await import('./geom_core.js');
const analyzer = new geomCore.Analyzer();

const fileData = new Uint8Array(await file.arrayBuffer());
analyzer.loadSTLFromBytes(fileData);

const report = analyzer.getPrintabilityReport(45.0, 0.8);
console.log(`Score: ${report.score}`);

// Zero-copy visualization data
const overhangMap = analyzer.getOverhangMapJS(45.0);  // Uint8Array
```

---

## Project Structure

```
geom-core/
├── src/
│   ├── core/
│   │   ├── vector3.hpp       # 3D vector operations
│   │   ├── matrix3.hpp       # Rotation matrices
│   │   ├── mesh.hpp          # STL parser, volume calculation
│   │   ├── spatial.hpp       # AABB tree, ray casting
│   │   └── analyzer.hpp      # High-level API
│   ├── bindings/
│   │   ├── python/           # pybind11 bindings
│   │   └── wasm/             # Emscripten bindings
│   └── occt/                 # Optional STEP support
├── viewer/                   # Solarpunk 3D viewer (Three.js)
├── tests/
│   ├── test_mesh.py          # Mesh operations
│   ├── test_printability.py  # Printability analysis
│   ├── test_auto_orient.py   # Auto-orientation
│   └── test_step.py          # STEP file loading
├── scripts/
│   ├── build_python.sh       # Build Python bindings
│   ├── build_wasm.sh         # Build WebAssembly
│   └── run_tests.sh          # Run test suite
├── package.json              # npm package (@madfam/geom-core)
└── pyproject.toml            # Python package (geom-core)
```

---

## Development Commands

### Python Build
```bash
# Build from source
./scripts/build_python.sh

# Or install in development mode
pip install -e .

# Run tests
./scripts/run_tests.sh
pytest tests/
```

### WebAssembly Build
```bash
# Requires Emscripten SDK
source ~/emsdk/emsdk_env.sh

./scripts/build_wasm.sh
# Output: dist/wasm/geom_core.js, geom_core.wasm
```

### TypeScript Package
```bash
pnpm install
pnpm build          # Build all (TS + WASM)
pnpm build:ts       # TypeScript only
pnpm build:wasm     # WASM only
pnpm test
```

### 3D Viewer Development
```bash
cd viewer
python3 -m http.server 8080
# Open http://localhost:8080
```

---

## Features

### Mesh Loading
- **Binary STL**: Fast parser with vertex deduplication
- **STEP/STP**: Via Open CASCADE (optional, requires OCCT)

### Mesh Analysis
- Volume calculation (mm³)
- Watertight/manifold checking
- Bounding box computation
- Triangle/vertex counts

### Printability Analysis
- **Overhang Detection**: Surfaces requiring support material
- **Wall Thickness**: Thin wall identification
- **Printability Score**: 0-100 composite rating

### Auto-Orientation
- Tests 26 candidate orientations
- Minimizes support material
- Reports improvement percentage

### Visualization Export
- Per-triangle overhang classification (0=safe, 1=overhang, 2=ground)
- Per-vertex wall thickness map
- Zero-copy WASM integration for real-time rendering

---

## API Reference

### Analyzer Class

```python
# Loading
analyzer.load_stl(filepath)
analyzer.load_stl_from_bytes(data)
analyzer.load_step(filepath, linear_deflection=0.1)  # Requires OCCT

# Properties
analyzer.get_vertex_count()      # int
analyzer.get_triangle_count()    # int
analyzer.get_volume()            # float (mm³)
analyzer.is_watertight()         # bool
analyzer.get_bounding_box()      # Vector3

# Analysis
analyzer.build_spatial_index()   # Required for printability
analyzer.get_printability_report(critical_angle, min_thickness)
analyzer.auto_orient(sample_resolution=26, critical_angle=45.0)

# Visualization (WASM)
analyzer.getOverhangMapJS(critical_angle)      # Uint8Array
analyzer.getWallThicknessMapJS(max_distance)   # Float32Array
```

### PrintabilityReport
```python
report.score                 # 0-100
report.overhang_area         # mm²
report.overhang_percentage   # 0-100
report.thin_wall_vertex_count
report.total_surface_area    # mm²
```

### OrientationResult
```python
result.optimal_up_vector      # Vector3
result.original_overhang_area # mm²
result.optimized_overhang_area
result.improvement_percent    # 0-100
```

---

## Technical Details

### Overhang Detection
```
dot_product = normal · up_vector
if dot_product < -cos(critical_angle):
    surface is overhang (needs support)
```

### Wall Thickness (Ray Casting)
```
ray = Ray(vertex + ε·normal, -normal)
hit = spatial_tree.ray_cast(ray, max_distance)
if hit.distance < min_thickness:
    vertex has thin wall
```

### Auto-Orientation Algorithm
- Tests 26 orientations (6 cardinals + 12 edges + 8 corners)
- Rotates test vector, not mesh (1000x faster)
- Returns orientation minimizing overhang area

---

## STEP File Support

STEP support requires Open CASCADE Technology:

```bash
# Ubuntu/Debian
sudo apt-get install libocct-data-exchange-dev libocct-ocaf-dev

# macOS
brew install opencascade

# Build with OCCT
cmake -DUSE_OCCT=ON ..
make
```

---

## Performance

- **STL Loading**: O(N log N) vertex deduplication
- **Spatial Queries**: O(log N) via AABB tree
- **Memory**: Zero-copy WASM ↔ JavaScript
- **Throughput**: 100k+ triangles at 60fps visualization

---

## Consumers

| Project | Usage |
|---------|-------|
| **Sim4D** | Primary CAD geometry engine |
| **Forj** | Model validation before fabrication |
| **Blueprint Harvester** | Printability scoring pipeline |
| **Cotiza Studio** | Quote accuracy from geometry |

---

## Milestones (All Complete ✅)

1. ✅ Project scaffold (CMake + pybind11)
2. ✅ Mesh analysis (STL, volume, watertight)
3. ✅ WebAssembly support
4. ✅ Printability analysis
5. ✅ Auto-orientation algorithm
6. ✅ CI/CD pipeline & packaging
7. ✅ STEP file support (OCCT)
8. ✅ Visualization export & 3D viewer

---

## Testing

```bash
# All tests
./scripts/run_tests.sh

# Specific test files
pytest tests/test_mesh.py
pytest tests/test_printability.py
pytest tests/test_auto_orient.py
pytest tests/test_step.py  # Requires OCCT
```

---

## Publishing

### PyPI
```bash
python -m build
twine upload dist/*
```

### npm (MADFAM registry)
```bash
pnpm build
pnpm publish
```

---

## Related Resources

- **GitHub**: [madfam-io/geom-core](https://github.com/madfam-io/geom-core)
- **PyPI**: [geom-core](https://pypi.org/project/geom-core/)
- **npm**: `@madfam/geom-core`
- **License**: Apache 2.0

---

*geom-core - The Physics Standard | Zero Dependencies, Maximum Performance*

<!-- END LEGACY_CLAUDE_IMPORT -->
