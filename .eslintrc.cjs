/* ESLint 8 config for the TypeScript SDK (src/typescript). `pnpm lint` is a CI gate. */
module.exports = {
  root: true,
  parser: '@typescript-eslint/parser',
  parserOptions: { ecmaVersion: 2022, sourceType: 'module' },
  plugins: ['@typescript-eslint'],
  extends: ['eslint:recommended', 'plugin:@typescript-eslint/recommended'],
  env: { es2022: true, browser: true, node: true },
  ignorePatterns: ['dist/', 'build/', 'node_modules/'],
  rules: {
    // Same convention tsconfig's noUnusedParameters honours: a leading
    // underscore marks a deliberately unused, reserved parameter.
    '@typescript-eslint/no-unused-vars': ['error', { argsIgnorePattern: '^_' }],
  },
};
