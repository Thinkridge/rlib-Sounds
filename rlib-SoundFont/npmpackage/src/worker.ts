(async () => {

  const inst = {
    instance: null as any | null,
    soundfont: null as any,
  };
  const onMessage = async (data: any, port: { postMessage: (msg: any) => void }) => {
    const { id, type, payload } = data;

    try {
      if (!inst.instance) {
        const url = "data:text/javascript;base64," + process.env.RLIB_JS_BASE64;
        const hModule = await import(/* @vite-ignore */url);
        inst.instance = await hModule.default();
      }

      if (type === "init") {
        inst.soundfont = inst.instance.loadSoundfont(payload.sf2);
        port.postMessage({ id, result: true });
        return;
      }
      if (!inst.soundfont) throw new Error("soundfont not initialized");

      if (type === "info") {
        const info = inst.soundfont.info();
        // console.log(`${JSON.stringify(info)}`);
        port.postMessage({ id, result: info });
        return;
      }

      if (type === "smfToWav") {
        const result = inst.instance.smfToWav(inst.soundfont, payload.smf);
        if (result.errorCode) throw Error(result.message);
        port.postMessage({ id, result: result.result });
        return;
      }
      throw new Error("unknown command");
    } catch (err) {
      console.error(err);
      port.postMessage({ id, error: String(err) });
    }

  };

  const isNode = typeof process !== "undefined" && process.versions?.node;

  if (isNode) {
    const { parentPort } = await import("node:worker_threads");

    if (!parentPort) {
      throw new Error("worker_threads parentPort is not available");
    }

    parentPort.on("message", async (data) => {
      onMessage(data, parentPort!);
    });
  } else {
    self.onmessage = async (e) => {
      onMessage(e.data, self);
    };
  }

})();
