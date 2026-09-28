import { execFileSync } from 'node:child_process';
import { expect, test } from 'vitest';

// Run real Node consumers: Vitest's resolver would hide missing .js suffixes.
test('the advertised ESM entry imports without a bundler', () => {
  const output = execFileSync(process.execPath, ['--input-type=module', '-e',
    'import { VERSION, GeometryEngine } from "@madfam/geom-core"; console.log(VERSION, typeof GeometryEngine);',
  ], { encoding: 'utf8' });
  expect(output.trim()).toBe('0.1.0 function');
});

test('the CommonJS entry remains usable by require', () => {
  const output = execFileSync(process.execPath, ['-e',
    'const { VERSION, GeometryEngine } = require("@madfam/geom-core"); console.log(VERSION, typeof GeometryEngine);',
  ], { encoding: 'utf8' });
  expect(output.trim()).toBe('0.1.0 function');
});
