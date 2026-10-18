/**
 * The mesh fixtures and tolerances of yantra4d's geom-core differential suite
 * (apps/studio/src/lib/__fixtures__/meshFixtures.js, lane G3), as plain
 * triangle soups, plus a float64 oracle with printEstimator's formulas.
 *
 * Tolerances (G3): every coordinate is float32 and both sides widen it to
 * float64, so differences are summation order only.
 *   volume   1e-9 relative, 1e-12 mm3 absolute floor for zero-volume meshes
 *   bbox     1e-9 mm absolute
 *   centroid 1e-9 mm absolute
 */
export const VOLUME_REL = 1e-9;
export const VOLUME_ABS_FLOOR = 1e-12;
export const LENGTH_ABS = 1e-9;

export interface MeshFixture { name: string; positions: number[]; index?: number[] }

/** Axis-aligned box as 12 outward-wound triangles (flip = inside out). */
function boxTriangles([sx, sy, sz]: number[], [ox, oy, oz] = [0, 0, 0], flip = false): number[] {
  const p = [[0, 0, 0], [sx, 0, 0], [sx, sy, 0], [0, sy, 0], [0, 0, sz], [sx, 0, sz], [sx, sy, sz], [0, sy, sz]]
    .map(([x, y, z]) => [x + ox, y + oy, z + oz]);
  const f = [[0, 2, 1], [0, 3, 2], [4, 5, 6], [4, 6, 7], [0, 1, 5], [0, 5, 4], [1, 2, 6], [1, 6, 5], [2, 3, 7], [2, 7, 6], [3, 0, 4], [3, 4, 7]];
  return f.flatMap((t) => (flip ? [t[0], t[2], t[1]] : t).flatMap((i) => p[i]));
}

/** UV sphere, outward wound, as a triangle soup. */
function sphereTriangles(r: number, [cx, cy, cz]: number[], seg = 48, rings = 24): number[] {
  const pt = (i: number, j: number) => {
    const th = (Math.PI * j) / rings;
    const ph = (2 * Math.PI * i) / seg;
    return [cx + r * Math.sin(th) * Math.cos(ph), cy + r * Math.sin(th) * Math.sin(ph), cz + r * Math.cos(th)];
  };
  const out: number[] = [];
  for (let j = 0; j < rings; j++) {
    for (let i = 0; i < seg; i++) {
      const a = pt(i, j), b = pt(i + 1, j), c = pt(i + 1, j + 1), d = pt(i, j + 1);
      if (j !== 0) out.push(...a, ...d, ...b);
      if (j !== rings - 1) out.push(...b, ...d, ...c);
    }
  }
  return out;
}

const TETRA_UNIT = [
  0, 0, 0, 1, 0, 0, 0, 1, 0,
  0, 0, 0, 1, 0, 0, 0, 0, 1,
  0, 0, 0, 0, 1, 0, 0, 0, 1,
  1, 0, 0, 0, 1, 0, 0, 0, 1,
];

export const MESH_FIXTURES: MeshFixture[] = [
  { name: 'single triangle at origin', positions: [0, 0, 0, 1, 0, 0, 0, 1, 0] },
  { name: 'unit tetrahedron (mixed winding)', positions: TETRA_UNIT },
  {
    name: 'indexed tetrahedron (edge 10)',
    positions: [0, 0, 0, 10, 0, 0, 0, 10, 0, 0, 0, 10],
    index: [0, 1, 2, 0, 1, 3, 0, 2, 3, 1, 2, 3],
  },
  { name: 'closed box 20x20x30 at (-5,-10,0)', positions: boxTriangles([20, 20, 30], [-5, -10, 0]) },
  {
    name: 'offset tetrahedron at (10,10,10)',
    positions: [
      10, 10, 10, 10, 11, 10, 11, 10, 10,
      10, 10, 10, 11, 10, 10, 10, 10, 11,
      10, 10, 10, 10, 10, 11, 10, 11, 10,
      11, 10, 10, 10, 11, 10, 10, 10, 11,
    ],
  },
  { name: 'two coplanar triangles (zero volume)', positions: [0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 1, 1, 0, 0, 1, 0] },
  { name: 'offset box 1x2x3 at (10,-5,7)', positions: boxTriangles([1, 2, 3], [10, -5, 7]) },
  { name: 'inside-out box 1x2x3', positions: boxTriangles([1, 2, 3], [10, -5, 7], true) },
  { name: 'two bodies (unit cubes 5 mm apart)', positions: [...boxTriangles([1, 1, 1]), ...boxTriangles([1, 1, 1], [5, 0, 0])] },
  { name: 'UV sphere r=12.5 at (3.3,-7.1,20.25), 2208 triangles', positions: sphereTriangles(12.5, [3.3, -7.1, 20.25]) },
  { name: 'sub-threshold box (1e-12 mm3, centroid rule)', positions: boxTriangles([1e-4, 1e-4, 1e-4], [0.1, 0.1, 0.1]) },
];

/** Non-indexed float32 corner coordinates [x, y, z] per triangle corner. */
export function soup(fixture: MeshFixture): Float32Array {
  const flat = fixture.index
    ? fixture.index.flatMap((i) => fixture.positions.slice(3 * i, 3 * i + 3))
    : fixture.positions;
  return Float32Array.from(flat);
}

/** Binary STL bytes (float32 coordinates, zero normals). */
export function toBinarySTL(fixture: MeshFixture): Uint8Array {
  const corners = soup(fixture);
  const faces = corners.length / 9;
  const buf = new ArrayBuffer(84 + faces * 50);
  const view = new DataView(buf);
  view.setUint32(80, faces, true);
  for (let t = 0; t < faces; t++) {
    for (let k = 0; k < 9; k++) view.setFloat32(84 + t * 50 + 12 + k * 4, corners[9 * t + k], true);
  }
  return new Uint8Array(buf);
}

export interface OracleMetrics {
  triangles: number;
  volume: number;
  min: number[];
  max: number[];
  size: number[];
  centroid: number[];
}

/** printEstimator's formulas in float64 over the float32 coordinates. */
export function oracle(fixture: MeshFixture): OracleMetrics {
  const c = soup(fixture);
  let v = 0;
  const m = [0, 0, 0];
  const min = [Infinity, Infinity, Infinity];
  const max = [-Infinity, -Infinity, -Infinity];
  for (let i = 0; i < c.length; i += 9) {
    const [ax, ay, az, bx, by, bz, cx, cy, cz] = c.subarray(i, i + 9);
    const d = (ax * (by * cz - bz * cy) - ay * (bx * cz - bz * cx) + az * (bx * cy - by * cx)) / 6;
    v += d;
    m[0] += (d * (ax + bx + cx)) / 4;
    m[1] += (d * (ay + by + cy)) / 4;
    m[2] += (d * (az + bz + cz)) / 4;
  }
  for (let i = 0; i < c.length; i++) {
    min[i % 3] = Math.min(min[i % 3], c[i]);
    max[i % 3] = Math.max(max[i % 3], c[i]);
  }
  const centroid = Math.abs(v) < 1e-9 ? [0, 0, 0] : m.map((s) => s / v);
  return { triangles: c.length / 9, volume: Math.abs(v), min, max, size: max.map((x, k) => x - min[k]), centroid };
}

/** The three malformed STLs of the G3 suite, with the parser message each must carry. */
export const MALFORMED_STLS: { name: string; bytes: () => Uint8Array; parserMessage: string }[] = [
  {
    name: 'buffer smaller than the 84-byte header',
    bytes: () => new Uint8Array(10),
    parserMessage: 'STL buffer too small (< 84 bytes)',
  },
  {
    name: 'buffer truncated below its declared face count',
    bytes: () => { const b = new Uint8Array(84 + 10); new DataView(b.buffer).setUint32(80, 1, true); return b; },
    parserMessage: 'STL triangle count exceeds the buffer or index capacity',
  },
  {
    name: 'header claiming more faces than the buffer holds (20M)',
    bytes: () => { const b = new Uint8Array(84); new DataView(b.buffer).setUint32(80, 20_000_000, true); return b; },
    parserMessage: 'STL triangle count exceeds the buffer or index capacity',
  },
];
