import { defineConfig } from "rollup";
import typescript from "@rollup/plugin-typescript";
import path from 'path';
import { fileURLToPath } from 'url';
import resolve from '@rollup/plugin-node-resolve';
import commonjs from '@rollup/plugin-commonjs';
import json from "@rollup/plugin-json";
import { string } from "rollup-plugin-string";
import babel from '@rollup/plugin-babel';

const __filename = fileURLToPath(import.meta.url);
const __dirname = path.dirname(__filename);

const scriptName = __dirname.split("/").at(-1) ?? "script.js";

export default defineConfig({
  input: "src/index.ts",
  output: {
    file: `out/${scriptName}.js`,
    format: "cjs",
    generatedCode: 'es5'
  },
  plugins: [
      typescript({
          compilerOptions: {
              target: 'ES5'
          }
      }),
      resolve(),
      json({
          exclude: "**/*.default.jsonc"
      }),
      string({
          include: "**/*.default.jsonc"
      }),
      commonjs(),
      babel({
          babelHelpers: 'bundled',
          presets: [
              [
                  '@babel/preset-env',
                  {
                      targets: {
                          ie: '11'
                      },
                      modules: false
                  }
              ]
          ]
      })
  ],
});
