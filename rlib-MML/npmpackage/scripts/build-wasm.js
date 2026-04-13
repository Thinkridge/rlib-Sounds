const { execSync } = require("child_process");
const fs = require("fs");

function canUseDocker() {
  try {
    execSync("docker info", { stdio: "ignore" });
    return true;
  } catch {
    return false;
  }
}

function cleanBuildDir() {
  try {
    fs.rmSync("./../rlib-MML/wasm/build", { recursive: true, force: true });
    console.log("Cleaned wasm/build");
  } catch {}
}

try {
  cleanBuildDir();
  if (canUseDocker()) {
    console.log("Using Docker (emsdk)");
    execSync(
      'docker run --rm --name emsdk -v ./../rlib-MML/:/opt/vol -w /opt/vol emsdk "emcmake cmake -S ./wasm -B ./wasm/build && cmake --build ./wasm/build"',
      { stdio: "inherit" }
    );
  } else {
    console.log("Docker not available, using local emsdk");
    execSync(
      "emcmake cmake -S ./../rlib-MML/wasm -B ./../rlib-MML/wasm/build && cmake --build ./../rlib-MML/wasm/build",
      { stdio: "inherit" }
    );
  }
} catch (e) {
  console.error("Build failed");
  process.exit(1);
}