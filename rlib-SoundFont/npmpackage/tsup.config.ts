import { defineConfig } from "tsup";
import fs from "fs";

export default defineConfig({
  entry: {
    index: "src/index.ts",
    worker: "src/worker.ts",
  },
  format: ["esm"],
  dts: true,
  clean: true,
  define: {
    "process.env.RLIB_JS_BASE64": JSON.stringify(fs.readFileSync("../src/wasm/build/rlibsoundfontdecoder.js", "base64")),
  },
});
