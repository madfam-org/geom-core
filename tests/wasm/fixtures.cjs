// Binary STL fixtures shared by the WASM runtime test. Same shapes as the C++
// (tests/native/test_mesh_analysis.cpp) and Python differential fixtures:
// closed cube, open cube, two bodies, inside-out cube, one flipped triangle,
// and a cube plus a zero-area sliver.
const CUBE_VERTICES = [[-5,-5,-5],[5,-5,-5],[5,5,-5],[-5,5,-5],[-5,-5,5],[5,-5,5],[5,5,5],[-5,5,5]];
const CUBE_FACES = [[0,2,1],[0,3,2],[4,5,6],[4,6,7],[0,4,7],[0,7,3],[1,6,5],[1,2,6],[0,1,5],[0,5,4],[3,6,2],[3,7,6]];

function stl(vertices, faces) {
  const bytes = Buffer.alloc(84 + faces.length * 50);
  bytes.writeUInt32LE(faces.length, 80);
  faces.forEach((face, i) => face.forEach((vertex, j) => vertices[vertex].forEach((n, k) => {
    bytes.writeFloatLE(n, 84 + i * 50 + 12 + j * 12 + k * 4);
  })));
  return new Uint8Array(bytes);
}

function shifted(dx) {
  return CUBE_VERTICES.map(([x, y, z]) => [x + dx, y, z]);
}

module.exports = {
  cube: () => stl(CUBE_VERTICES, CUBE_FACES),
  openCube: () => stl(CUBE_VERTICES, CUBE_FACES.slice(2)),
  twoCubes: () => stl([...CUBE_VERTICES, ...shifted(20)],
    [...CUBE_FACES, ...CUBE_FACES.map(f => f.map(i => i + 8))]),
  insideOutCube: () => stl(CUBE_VERTICES, CUBE_FACES.map(f => [f[0], f[2], f[1]])),
  oneFlipped: () => stl(CUBE_VERTICES, [[0,1,2], ...CUBE_FACES.slice(1)]),
  cubeWithSliver: () => stl([...CUBE_VERTICES, [20,0,0], [21,0,0], [22,0,0]],
    [...CUBE_FACES, [8,9,10]]),
};
