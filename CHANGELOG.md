# Changelog

All notable changes to geom-core. Versions follow [SemVer](https://semver.org/)
(pre-1.0: a minor bump may break); `pyproject.toml` is the single version
source. Release assets are attached to the GitHub Release of each `v*` tag.

## Unreleased

### CI and release

- **Linux wheels are `manylinux_2_28`.** `release.yml` builds the Linux
  x86_64 wheels (cp38–cp312) with `cibuildwheel` in the `manylinux_2_28`
  image and repairs them with `auditwheel`, so they carry the
  `manylinux_2_28_x86_64` tag (and `manylinux_2_27_x86_64`, which auditwheel
  adds) and install on Debian bookworm and trixie, Ubuntu 20.04+ and RHEL 8+;
  the repaired extension references at most `GLIBCXX_3.4.21` and
  `GLIBC_2.14`. 0.1.0 and 0.2.0 shipped plain `linux_x86_64` wheels
  built on Ubuntu 24.04 that need `GLIBCXX_3.4.32` and `GLIBC_2.32`, i.e. a
  Debian trixie or newer base. The job fails unless all five wheels carry the
  manylinux tag, and it runs `scripts/run_tests.sh` against each installed
  wheel. macOS wheels are unchanged (`universal2`, py3.10–3.12).

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
- **Body count is edge-connected by default** (faces sharing an edge, like
  `trimesh.split()`); 0.1.0 joined faces sharing any vertex. Affects
  `Mesh::bodyCount()`, `Analyzer::getBodyCount()`, Python `get_body_count()`,
  WASM `getBodyCount()` and `MeshIntegrity.bodyCount`. Pass `mode="vertex"`
  (C++ `BodyConnectivity::Vertex`) for the old rule. Two boxes touching at one
  corner: 2 bodies by edge, 1 by vertex.
- **Loads print nothing on success** (0.1.0 printed "Loaded STL: ..." and the
  STEP loader's progress to stdout). Native failures still go to stderr.

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
