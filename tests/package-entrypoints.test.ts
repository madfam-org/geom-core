import { execFileSync } from 'node:child_process';
import { readFileSync } from 'node:fs';
import { expect, test } from 'vitest';

// package.json must agree with pyproject.toml (tests/test_version.py); the
// built entrypoints must report that same version.
const { version } = JSON.parse(readFileSync(new URL('../package.json', import.meta.url), 'utf8'));

// Run real Node consumers: Vitest's resolver would hide missing .js suffixes.
test('the advertised ESM entry imports without a bundler', () => {
  const output = execFileSync(process.execPath, ['--input-type=module', '-e',
    'import { VERSION, GeometryEngine } from "@madfam/geom-core"; console.log(VERSION, typeof GeometryEngine);',
  ], { encoding: 'utf8' });
  expect(output.trim()).toBe(`${version} function`);
});

test('the CommonJS entry remains usable by require', () => {
  const output = execFileSync(process.execPath, ['-e',
    'const { VERSION, GeometryEngine } = require("@madfam/geom-core"); console.log(VERSION, typeof GeometryEngine);',
  ], { encoding: 'utf8' });
  expect(output.trim()).toBe(`${version} function`);
});

test('kernel errors remain structured without the Node process global', () => {
  const output = execFileSync(process.execPath, ['--input-type=module', '-e', `
    import { GeometryEngine } from "@madfam/geom-core";
    const engine = new GeometryEngine({});
    delete globalThis.process;
    console.error = () => {};
    const result = engine.makeBox();
    console.log(result.success, !!result.error);
  `], { encoding: 'utf8' });
  expect(output.trim()).toBe('false true');
});
