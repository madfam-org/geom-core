# Changelog

All notable changes to geom-core. Versions follow [SemVer](https://semver.org/)
(pre-1.0: a minor bump may break); `pyproject.toml` is the single version
source. Release assets are attached to the GitHub Release of each `v*` tag.

## 0.2.0

First consumers (yantra4d Studio in the browser, hyperobjects-spec keystone
in Python) could not use 0.1.0 as shipped; this release fixes what they hit.

### Breaking

- **The default WASM build is single-threaded.** `@madfam/geom-core/wasm` is
  now built with `USE_PTHREADS=0`, `EXPORT_ES6=1`, `MODULARIZE=1`,
  `ENVIRONMENT=web,worker,node` and memory growth: no `SharedArrayBuffer`, no
  worker pool, no COOP/COEP needed. 0.1.0's threaded build allocated shared
  memory and started 4 pthread workers with `new Worker(_scriptName)` at
  load, which fails on pages that are not cross-origin isolated and inside
  bundler chunks. The threaded build moved to the opt-in
  `@madfam/geom-core/threaded` entry.
- **The WASM entries are ES modules.** `@madfam/geom-core/wasm` and
  `/threaded` resolve to hand-written wrappers (`dist/wasm/index.js`,
  `threaded.js`) over the Emscripten factories; `require()` of them no longer
  works (`require.resolve` does). The factory's default export is
  `createGeomCore(options)` as before.
- **`loadSTLFromBytes()` / `loadSTL()` throw** `GeomCoreSTLError` (with
  `parserMessage`) instead of returning `false`; they return `true` on
  success. The WASM build no longer writes parser errors to stderr.
- **WASM `getBoundingBox()` shape widened** to `{min, max, size}`. The 0.1.0
  extents `x`/`y`/`z` are still present (equal to `size`) and deprecated.
- **Body count is edge-connected by default** (faces sharing an edge; like
  `trimesh.split()` on manifold meshes, but unlike it an edge used by three
  or more faces also joins them, see "Open: the body-count definition"
  below); 0.1.0 joined faces sharing any vertex. Affects
  `Mesh::bodyCount()`, `Analyzer::getBodyCount()`, Python `get_body_count()`,
  WASM `getBodyCount()` and `MeshIntegrity.bodyCount`. Pass `mode="vertex"`
  (C++ `BodyConnectivity::Vertex`) for the old rule. Two boxes touching at one
  corner: 2 bodies by edge, 1 by vertex.
- **Loads print nothing on success** (0.1.0 printed "Loaded STL: ..." and the
  STEP loader's progress to stdout). Native failures still go to stderr.

### Open: the body-count definition (owner ruling pending)

0.2.0 changed the default body count from vertex-connected to edge-connected
(see Breaking). Which rule the geometry factories mean by "body count" is an
owner ruling. It is the gate before any consumer makes geom-core its default
mesh oracle; until then the default stays `edge` and nothing here changes.

The question was put as "edge vs vertex". Measured against trimesh, the edge
side is really two rules, because trimesh joins faces only across an edge that
exactly two faces use. All three rules, written out:

- **Edge-connected (E).** Two triangles belong to the same body when a chain
  of triangles joins them in which each triangle shares a whole edge (both of
  its endpoints) with the next, however many triangles use that edge.
  - This is the geom-core 0.2.0 default: `Mesh::bodyCount(BodyConnectivity::Edge)`,
    `Analyzer::getBodyCount()`, Python `get_body_count()` (`mode="edge"`), WASM
    `getBodyCount()` and `MeshIntegrity.bodyCount` (`src/Mesh.cpp` unions
    every pair of faces on an edge).
- **Manifold-edge-connected (E2).** The same, except that an edge joins
  triangles only when exactly two triangles use it. An edge shared by three or
  more triangles, which is a non-manifold seam, joins nothing. These are the
  connected components of trimesh's `face_adjacency`. Every consumer's trimesh
  path counts this way today:
  - trimesh `mesh.split(only_watertight=False)` and
    `trimesh.graph.connected_components(mesh.face_adjacency)`;
  - yantra4d `TrimeshOracle.body_count`;
  - Blueprint `TrimeshOracle.body_count`;
  - hyperobjects-spec `y4d_spec.geometry._bodies`, which feeds the
    `body-count` rule that checks each manifest's declared
    `geometry.body_count`.
- **Vertex-connected (V).** Two triangles belong to the same body when a chain
  joins them in which each triangle shares at least one vertex with the next.
  These are the connected components of the vertex graph.
  - This was geom-core 0.1.0's only rule. In 0.2.0 it is
    `BodyConnectivity::Vertex`, Python `get_body_count(mode="vertex")` and WASM
    `getBodyCount("vertex")`.
  - trimesh's `Trimesh.body_count` property counts this way, and so does
    Blueprint's `vertex_joined_body_count`, which only classifies parity
    records.

**Where they differ.** Measured with the geom-core 0.2.0 Python wheel and
trimesh 4.x, after `merge_vertices()` (geom-core also welds exactly
coincident vertices on load):

| Case | E (geom-core 0.2.0) | E2 (trimesh paths) | V |
|---|---|---|---|
| Two boxes sharing one corner | 2 | 2 | 1 |
| Two triangles meeting at one vertex (a bow tie) | 2 | 2 | 1 |
| Two boxes sharing one edge (four faces on it) | 1 | 2 | 1 |
| Two boxes sharing a whole face (coincident faces) | 1 | 4 | 1 |
| Two separate boxes | 2 | 2 | 2 |

Under every rule the shared corner, edge or face is a non-manifold feature,
and `assessIntegrity()` reports it. The rule decides only how many parts the
count reports. A solid mesh is manifold and gives the same count under all
three rules.

**What each answer changes.**
- *E2.* geom-core adds it (for example `BodyConnectivity::ManifoldEdge`) and
  makes it the default. Every trimesh path keeps its numbers, and the
  `MESH_ORACLE=both` soaks then expect no body-count mismatches at all.
- *E (today's default).* The trimesh paths, including the hyperobjects-spec
  `body-count` rule, would have to move to geom-core's count. Until they do,
  the soaks will report a body-count mismatch on every render with a
  non-manifold edge or face contact (geom-core counts fewer).
  - Blueprint's classifier would file the edge case under
    `body_count_vertex_contact`: its test is only "fewer, and equal to the
    vertex-joined count", so the label reads as a vertex contact when the
    contact is an edge.
- *V.* geom-core's default goes back to vertex, and the hyperobjects-spec
  rule and the declared counts would move to V. Every vertex, edge or face
  contact then becomes one body.

Of the two `MESH_ORACLE=both` soaks prepared on 2026-10-19, only Blueprint's
compares body counts; so does the hyperobjects-spec oracle. yantra4d's API
seam compares quote volume, area and bounding box only.

The owner records the ruling by appending a dated block to internal-devops
`decisions/2026-10-18-geom-core-shared-mesh-analysis-layer.md`, pointing at
this section.

### Added

- Exports `@madfam/geom-core/wasm-binary` and
  `@madfam/geom-core/threaded/wasm-binary` (the `.wasm` files, for bundler
  `?url` imports) and `./package.json`.
- `@madfam/geom-core/threaded` rejects with `GeomCoreThreadingError` when
  `SharedArrayBuffer` is missing or `crossOriginIsolated === false`.
- `Mesh::getBounds()` / `Analyzer::getBounds()` (`BoundingBox{min, max, size}`);
  Python `BoundingBox` and `Analyzer.get_bounds()` (additive;
  `get_bounding_box()` still returns the size).
- `Analyzer::getSignedVolume()`; Python `get_signed_volume()`; WASM
  `getSignedVolume()`. `get_volume()` stays absolute and is documented so.
- `Analyzer::getVertices()` / `getFaces()`; WASM `getVertices()` /
  `getIndices()` (owned `Float64Array` / `Uint32Array` copies) and
  `getVerticesView()` / `getIndicesView()` (zero-copy views).
- `Mesh::getLastError()` / `Analyzer::getLastError()` (WASM too).
- `BodyConnectivity` and the `mode` argument of the body counts.
- TypeScript declarations for the WASM entries live in `src/wasm/` and are
  typechecked (`pnpm typecheck`); `MeshIntegrity` is tested against the C++
  struct field for field.
- Python 3.12 in the CI and release matrices.

### Fixed

- macOS release wheels tagged `universal2` held only arm64 code and failed to
  import on Intel; `setup.py` now maps `ARCHFLAGS` to
  `CMAKE_OSX_ARCHITECTURES`, `release.yml` builds arm64 + x86_64 and fails when
  a wheel lacks an architecture its tag names.
- `loadSTL()` of a file under 84 bytes now reports why.

### CI and release

- `ci.yml`'s WASM job builds both variants, runs `pnpm test:wasm` (Vitest:
  Node, jsdom without `SharedArrayBuffer`, a Vite app build) and
  `pnpm test:wasm:threaded`, and writes a size table to the job summary.
- `release.yml` packs both variants into the npm tarball and checks its
  WASM file list.

## 0.1.0

First release: C++17 mesh analysis core, Python module `geom_core_py`,
threaded Emscripten WASM + TypeScript package `@madfam/geom-core`, integrity
assessment ported from yantra4d, GitHub Release assets. Its assets stay on
the v0.1.0 release page.
