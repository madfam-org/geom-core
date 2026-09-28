const assert = require('node:assert/strict');
const path = require('node:path');
const createModule = require(path.resolve(__dirname, '../../dist/wasm/geom-core.js'));

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

  const cad = new mod.GeomCoreCAD();
  assert.equal(cad.initialize(), true);
  assert.equal(cad.healthCheck().occtAvailable, false);
  const box = cad.makeBox({width: 10, height: 10, depth: 10});
  assert.equal(box.success, false);
  assert.equal(box.error.code, 'OCCT_UNAVAILABLE');
  assert.equal(cad.getShapeCount(), 0);
  cad.shutdown();
  cad.delete();
  console.log('WASM: real 10mm cube volume/topology/bounds, malformed STL rejection, and explicit missing CAD capability passed');
}
main().then(() => process.exit(0), error => { console.error(error); process.exit(1); });
