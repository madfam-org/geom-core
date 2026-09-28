const { writeFileSync } = require('node:fs');
const { resolve } = require('node:path');

// Keep the root CommonJS contract while marking only the ESM output as modules.
writeFileSync(resolve(__dirname, '../dist/esm/package.json'), '{"type":"module"}\n');
