import { defineConfig } from "tsup";
import fs from "fs";
import { build } from "esbuild";

export default defineConfig(async (options) => {

  // worker.ts
  const workerBuild = await build({
    entryPoints: ["src/worker.ts"],
    bundle: true,
    write: false,
    format: "esm",
    minify: true,
    external: ["node:worker_threads"], // 組み込みモジュールを除外
    platform: "node", // ブラウザ、node.js 両対応を考慮
    define: {
      "process.env.RLIB_JS_BASE64": JSON.stringify(fs.readFileSync("../src/wasm/build/rlibsoundfontdecoder.js", "base64")),
    },
  });

  // index.ts
  return {
    entry: {
      index: "src/index.ts",
    },
    format: ["esm"],
    dts: true,
    clean: true,
    bundle: true,
    define: {
      "process.env.WORKER_CODE": JSON.stringify(workerBuild.outputFiles[0].text), // worker を埋め込む
    },
    external: ["node:worker_threads", "node:url"], // 組み込みモジュールを除外
  };
});