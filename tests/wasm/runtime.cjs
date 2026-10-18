const assert = require('node:assert/strict');
const path = require('node:path');
const createModule = require(path.resolve(__dirname, '../../dist/wasm/geom-core.js'));
const fixtures = require('./fixtures.cjs');

const near = (a, b, tol = 1e-6) => Math.abs(a - b) <= tol;

function assessFixture(mod, name) {
  const analyzer = new mod.Analyzer();
  assert.equal(analyzer.loadSTLFromBytes(fixtures[name]()), true, name);
  const result = {
    integrity: analyzer.assessIntegrity(0),
    area: analyzer.getSurfaceArea(),
    centroid: analyzer.getCentroid(),
    bodies: analyzer.getBodyCount(),
  };
  analyzer.delete();
  return result;
}

// Integrity verdict, surface area, centroid and body count through WASM.
function checkMeshAnalysis(mod) {
  const cube = assessFixture(mod, 'cube');
  assert.equal(cube.integrity.watertight, true);
  assert.equal(cube.integrity.eulerNumber, 2);
  assert.equal(cube.integrity.mergeTolerance, null);
  assert.equal(cube.integrity.inverted, false);
  assert.equal(cube.integrity.bodyCount, 1);
  assert.deepEqual(cube.integrity.notes, []);
  assert.equal(cube.integrity.summary, 'watertight (volume 1000.00)');
  assert.ok(near(cube.area, 600));
  assert.ok(near(cube.centroid.x, 0) && near(cube.centroid.y, 0) && near(cube.centroid.z, 0));
  assert.equal(cube.bodies, 1);

  const open = assessFixture(mod, 'openCube');
  assert.equal(open.integrity.watertight, false);
  assert.equal(open.integrity.boundaryEdges, 4);
  assert.equal(open.integrity.nonManifoldEdges, 0);
  assert.equal(open.integrity.eulerNumber, 1);
  assert.ok(open.integrity.summary.startsWith('NOT watertight — 4 boundary edge(s)'));

  const two = assessFixture(mod, 'twoCubes');
  assert.equal(two.integrity.watertight, true);
  assert.equal(two.integrity.bodyCount, 2);
  assert.equal(two.bodies, 2);
  assert.ok(near(two.area, 1200));
  assert.ok(near(two.centroid.x, 10) && near(two.centroid.y, 0) && near(two.centroid.z, 0));

  const inverted = assessFixture(mod, 'insideOutCube');
  assert.equal(inverted.integrity.inverted, true);
  assert.ok(near(inverted.integrity.volume, -1000));

  const flipped = assessFixture(mod, 'oneFlipped');
  assert.equal(flipped.integrity.watertight, true);
  assert.equal(flipped.integrity.windingConsistent, false);
  assert.equal(flipped.integrity.inverted, false);

  const sliver = assessFixture(mod, 'cubeWithSliver');
  assert.equal(sliver.integrity.degenerateFaces, 1);
  assert.equal(sliver.integrity.bodyCount, 2);
  assert.equal(sliver.integrity.boundaryEdges, 3);
}

async function main() {
  const mod = await createModule();
  const vertices = [[-5,-5,-5],[5,-5,-5],[5,5,-5],[-5,5,-5],[-5,-5,5],[5,-5,5],[5,5,5],[-5,5,5]];
  const faces = [[0,2,1],[0,3,2],[4,5,6],[4,6,7],[0,4,7],[0,7,3],[1,6,5],[1,2,6],[0,1,5],[0,5,4],[3,6,2],[3,7,6]];
  const bytes = Buffer.alloc(84 + faces.length * 50);
  bytes.writeUInt32LE(faces.length, 80);
  faces.forEach((face, i) => face.forEach((vertex, j) => vertices[vertex].forEach((n, k) => {
    bytes.writeFloatLE(n, 84 + i * 50 + 12 + j * 12 + k * 4);
  })));
  const analyzer = new mod.Analyzer();
  assert.equal(analyzer.loadSTLFromBytes(new Uint8Array(bytes)), true);
  assert.equal(analyzer.getVertexCount(), 8);
  assert.equal(analyzer.getTriangleCount(), 12);
  assert.equal(analyzer.isWatertight(), true);
  assert.ok(Math.abs(analyzer.getVolume() - 1000) < 1e-6);
  assert.deepEqual(analyzer.getBoundingBox(), {x: 10, y: 10, z: 10});
  analyzer.buildSpatialIndex();
  const report = analyzer.getPrintabilityReport(45, 0.8);
  assert.ok(Number.isFinite(report.score));
  const hostile = Buffer.alloc(84);
  hostile.writeUInt32LE(0xffffffff, 80);
  assert.equal(analyzer.loadSTLFromBytes(new Uint8Array(hostile)), false);
  analyzer.delete();

  checkMeshAnalysis(mod);

  const cad = new mod.GeomCoreCAD();
  assert.equal(cad.initialize(), true);
  assert.equal(cad.healthCheck().occtAvailable, false);
  const box = cad.makeBox({width: 10, height: 10, depth: 10});
  assert.equal(box.success, false);
  assert.equal(box.error.code, 'OCCT_UNAVAILABLE');
  assert.equal(cad.getShapeCount(), 0);
  cad.shutdown();
  cad.delete();
  console.log('WASM: real 10mm cube volume/topology/bounds, integrity verdicts, surface area, centroid and body count, malformed STL rejection, and explicit missing CAD capability passed');
}
main().then(() => process.exit(0), error => { console.error(error); process.exit(1); });
