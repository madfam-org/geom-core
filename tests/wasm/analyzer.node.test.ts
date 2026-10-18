// @vitest-environment node
/**
 * The real single-threaded WASM (dist/wasm/, built by scripts/build_wasm.sh)
 * through the package's default entry point, loading its own .wasm.
 */
import { afterAll, beforeAll, describe, expect, it } from 'vitest';
import createGeomCore, { GeomCoreSTLError } from '../../dist/wasm/index.js';
import type { Analyzer, GeomCoreModule, MeshIntegrity } from '../../dist/wasm/index.js';
import fixtures from './fixtures.cjs';
import {
  LENGTH_ABS, MALFORMED_STLS, MESH_FIXTURES, VOLUME_ABS_FLOOR, VOLUME_REL, oracle, soup, toBinarySTL,
} from './meshFixtures';

let mod: GeomCoreModule;
const worst = { volumeRel: 0, lengthAbs: 0 };

beforeAll(async () => {
  mod = await createGeomCore({ print: () => {} });
});

afterAll(() => {
  console.info(`[geom-core WASM vs oracle] worst volume rel err ${worst.volumeRel.toExponential(2)}, worst length abs err ${worst.lengthAbs.toExponential(2)} mm`);
});

function withAnalyzer<T>(bytes: Uint8Array, fn: (a: Analyzer) => T): T {
  const analyzer = new mod.Analyzer();
  try {
    expect(analyzer.loadSTLFromBytes(bytes)).toBe(true);
    return fn(analyzer);
  } finally {
    analyzer.delete();
  }
}

const expectVolume = (wasm: number, ref: number) => {
  const err = Math.abs(wasm - ref);
  worst.volumeRel = Math.max(worst.volumeRel, ref === 0 ? err : err / Math.abs(ref));
  expect(err).toBeLessThanOrEqual(Math.max(VOLUME_REL * Math.abs(ref), VOLUME_ABS_FLOOR));
};
const expectLength = (wasm: number, ref: number) => {
  const err = Math.abs(wasm - ref);
  worst.lengthAbs = Math.max(worst.lengthAbs, err);
  expect(err).toBeLessThanOrEqual(LENGTH_ABS);
};
const xyz = (v: { x: number; y: number; z: number }) => [v.x, v.y, v.z];

describe('G3 fixtures: volume, bbox min/max/size, centroid within G3 tolerances', () => {
  for (const fixture of MESH_FIXTURES) {
    it(fixture.name, () => {
      const ref = oracle(fixture);
      withAnalyzer(toBinarySTL(fixture), (a) => {
        expect(a.getTriangleCount()).toBe(ref.triangles);
        expectVolume(a.getVolume(), ref.volume);
        const box = a.getBoundingBox();
        xyz(box.min).forEach((v, k) => expectLength(v, ref.min[k]));
        xyz(box.max).forEach((v, k) => expectLength(v, ref.max[k]));
        xyz(box.size).forEach((v, k) => expectLength(v, ref.size[k]));
        // The 0.1.0 extents stay as deprecated aliases of size.
        expect([box.x, box.y, box.z]).toEqual(xyz(box.size));
        xyz(a.getCentroid()).forEach((v, k) => expectLength(v, ref.centroid[k]));
      });
    });
  }
});

describe('vertex and index access', () => {
  for (const fixture of MESH_FIXTURES) {
    it(`rebuilds the exact triangle soup: ${fixture.name}`, () => {
      const corners = soup(fixture);
      withAnalyzer(toBinarySTL(fixture), (a) => {
        const vertices = a.getVertices();
        const indices = a.getIndices();
        expect(vertices).toBeInstanceOf(Float64Array);
        expect(indices).toBeInstanceOf(Uint32Array);
        expect(vertices.length).toBe(3 * a.getVertexCount());
        expect(indices.length).toBe(3 * a.getTriangleCount());
        const rebuilt = Array.from(indices, (i) => [vertices[3 * i], vertices[3 * i + 1], vertices[3 * i + 2]]).flat();
        expect(rebuilt).toEqual(Array.from(corners));
        // Zero-copy views hold the same data.
        expect(Array.from(a.getVerticesView())).toEqual(Array.from(vertices));
        expect(Array.from(a.getIndicesView())).toEqual(Array.from(indices));
      });
    });
  }

  it('welds shared corners: a closed box has 8 vertices and 12 triangles', () => {
    withAnalyzer(toBinarySTL(MESH_FIXTURES[3]), (a) => {
      expect(a.getVertices().length).toBe(24);
      expect(a.getIndices().length).toBe(36);
    });
  });

  it('getVertices() is an owned copy that outlives the analyzer; the view is not owned', () => {
    const analyzer = new mod.Analyzer();
    analyzer.loadSTLFromBytes(toBinarySTL(MESH_FIXTURES[6]));
    const copy = analyzer.getVertices();
    const view = analyzer.getVerticesView();
    const snapshot = Array.from(copy);
    expect(copy.buffer).not.toBe(view.buffer);
    analyzer.delete();
    expect(Array.from(copy)).toEqual(snapshot);
  });

  it('an empty analyzer returns empty arrays and a zero bounding box', () => {
    const analyzer = new mod.Analyzer();
    expect(analyzer.getVertices().length).toBe(0);
    expect(analyzer.getIndices().length).toBe(0);
    const box = analyzer.getBoundingBox();
    expect([...xyz(box.min), ...xyz(box.max), ...xyz(box.size)]).toEqual(new Array(9).fill(0));
    analyzer.delete();
  });
});

describe('malformed STLs throw GeomCoreSTLError with the parser message', () => {
  for (const bad of MALFORMED_STLS) {
    it(bad.name, () => {
      const analyzer = new mod.Analyzer();
      analyzer.loadSTLFromBytes(toBinarySTL(MESH_FIXTURES[3]));
      let thrown: unknown;
      try {
        analyzer.loadSTLFromBytes(bad.bytes());
      } catch (e) {
        thrown = e;
      }
      expect(thrown).toBeInstanceOf(GeomCoreSTLError);
      expect(thrown).toBeInstanceOf(Error);
      expect((thrown as GeomCoreSTLError).parserMessage).toBe(bad.parserMessage);
      // Same wording the Studio's wrapper matches (/could not read the STL/).
      expect((thrown as Error).message).toBe(`geom-core could not read the STL: ${bad.parserMessage}`);
      expect(analyzer.getLastError()).toBe(bad.parserMessage);
      expect(analyzer.getTriangleCount()).toBe(0);
      // A good load afterwards clears the error.
      analyzer.loadSTLFromBytes(toBinarySTL(MESH_FIXTURES[3]));
      expect(analyzer.getLastError()).toBe('');
      analyzer.delete();
    });
  }

  it('non-finite coordinates', () => {
    const bytes = toBinarySTL(MESH_FIXTURES[0]);
    new DataView(bytes.buffer).setFloat32(84 + 12, Number.NaN, true);
    const analyzer = new mod.Analyzer();
    expect(() => analyzer.loadSTLFromBytes(bytes)).toThrow('geom-core could not read the STL: STL contains non-finite coordinates');
    analyzer.delete();
  });

  it('loadSTL on a missing virtual file', () => {
    const analyzer = new mod.Analyzer();
    expect(() => analyzer.loadSTL('/missing.stl')).toThrow('geom-core could not read the STL: Could not open STL file: /missing.stl');
    analyzer.delete();
  });
});

describe('body count connectivity and signed volume (0.2.0)', () => {
  // Two 10 mm boxes touching at the single corner (10, 10, 10).
  const corner = { name: 'corner-touching boxes', positions: [] as number[] };
  const boxA = MESH_FIXTURES[3].positions.map((v, i) => (i % 3 === 0 ? v + 5 : i % 3 === 1 ? v + 10 : v)); // 20x20x30 at origin
  corner.positions = [...boxA, ...boxA.map((v, i) => v + [20, 20, 30][i % 3])];

  it('edge connectivity is the default; vertex joins the shared corner', () => {
    withAnalyzer(toBinarySTL(corner), (a) => {
      expect(a.getVertexCount()).toBe(15);
      expect(a.getBodyCount()).toBe(2);
      expect(a.getBodyCount('edge')).toBe(2);
      expect(a.getBodyCount('vertex')).toBe(1);
      expect(a.assessIntegrity(0).bodyCount).toBe(2);
      expect(() => a.getBodyCount('face' as 'edge')).toThrow(TypeError);
    });
  });

  it('getSignedVolume() is negative for the inside-out box; getVolume() stays absolute', () => {
    const inverted = MESH_FIXTURES.find((f) => f.name.startsWith('inside-out'))!;
    withAnalyzer(toBinarySTL(inverted), (a) => {
      expectVolume(a.getSignedVolume(), -6);
      expectVolume(a.getVolume(), 6);
      expect(a.assessIntegrity(0).inverted).toBe(true);
    });
  });
});

// Carried over from the 0.1.0 runtime check (tests/wasm/runtime.cjs).
describe('integrity, surface area, centroid, body count (0.1.0 runtime checks)', () => {
  const assess = (name: keyof typeof fixtures) => withAnalyzer(fixtures[name](), (a) => ({
    integrity: a.assessIntegrity(0) as MeshIntegrity,
    area: a.getSurfaceArea(),
    centroid: a.getCentroid(),
    bodies: a.getBodyCount(),
    watertight: a.isWatertight(),
    volume: a.getVolume(),
    vertexCount: a.getVertexCount(),
  }));

  it('closed 10 mm cube', () => {
    const cube = assess('cube');
    expect(cube.vertexCount).toBe(8);
    expect(cube.watertight).toBe(true);
    expect(cube.volume).toBeCloseTo(1000, 9);
    expect(cube.integrity).toMatchObject({ watertight: true, eulerNumber: 2, mergeTolerance: null, inverted: false, bodyCount: 1, notes: [] });
    expect(cube.integrity.summary).toBe('watertight (volume 1000.00)');
    expect(cube.area).toBeCloseTo(600, 9);
    xyz(cube.centroid).forEach((v) => expect(Math.abs(v)).toBeLessThanOrEqual(1e-9));
  });

  it('open cube, two bodies, inside out, one flipped, sliver', () => {
    const open = assess('openCube').integrity;
    expect(open).toMatchObject({ watertight: false, boundaryEdges: 4, nonManifoldEdges: 0, eulerNumber: 1 });
    expect(open.summary.startsWith('NOT watertight — 4 boundary edge(s)')).toBe(true);
    const two = assess('twoCubes');
    expect(two.integrity.bodyCount).toBe(2);
    expect(two.bodies).toBe(2);
    expect(two.area).toBeCloseTo(1200, 9);
    expect(two.centroid.x).toBeCloseTo(10, 9);
    const inverted = assess('insideOutCube').integrity;
    expect(inverted.inverted).toBe(true);
    expect(inverted.volume).toBeCloseTo(-1000, 9);
    expect(assess('oneFlipped').integrity).toMatchObject({ watertight: true, windingConsistent: false, inverted: false });
    expect(assess('cubeWithSliver').integrity).toMatchObject({ degenerateFaces: 1, bodyCount: 2, boundaryEdges: 3 });
  });

  it('printability report and the explicit missing CAD capability', () => {
    withAnalyzer(fixtures.cube(), (a) => {
      a.buildSpatialIndex();
      expect(Number.isFinite(a.getPrintabilityReport(45, 0.8).score)).toBe(true);
    });
    const cad = new mod.GeomCoreCAD();
    expect(cad.initialize()).toBe(true);
    expect(cad.healthCheck().occtAvailable).toBe(false);
    const box = cad.makeBox({ width: 10, height: 10, depth: 10 });
    expect(box.success).toBe(false);
    expect(box.error?.code).toBe('OCCT_UNAVAILABLE');
    expect(cad.getShapeCount()).toBe(0);
    cad.shutdown();
    (cad as unknown as { delete(): void }).delete();
  });
});
