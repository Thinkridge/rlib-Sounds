export class RlibSoundfont {
  private workerPromise;
  private seq = 0;
  private pending = new Map<number, { resolve: (value: unknown) => void; reject: (reason?: any) => void }>();

  constructor() {
    this.workerPromise = (async () => {
      const worker = await (async () => {
        const workerCode = process.env.WORKER_CODE as string;
        if (typeof window !== "undefined" && typeof (window as any).Worker !== "undefined") {
          // Browser
          const url = URL.createObjectURL(new Blob([workerCode], { type: "text/javascript" }));
          return new window.Worker(url, { type: "module" });
        } else {
          // Node.js
          const mod = await (new Function("return import('node:worker_threads')") as () => Promise<any>)(); // バンドラに解決させない
          return new mod.Worker(workerCode, { eval: true, type: "module" });
        }
      })();
      // onmessage
      const handler = (data: any) => {
        const { id, result, error } = data;
        const execArg = this.pending.get(id);
        if (!execArg) {
          console.error(`unknown id:${id}`);
          return;
        }
        this.pending.delete(id);
        if (error) {
          execArg.reject(error);
        } else {
          execArg.resolve(result);
        }
      };
      if ("onmessage" in worker) {
        worker.onmessage = (e: MessageEvent) => handler(e.data); // Browser
      } else {
        worker.on("message", handler); // Node
      }
      return worker;
    })();
  }

  dispose() {
    return (async () => (await this.workerPromise).terminate())();
  }

  private async call(type: string, payload: any, transfer: Transferable[] = []): Promise<any> {
    const worker = await this.workerPromise;
    const id = ++this.seq;
    return new Promise((resolve, reject) => {
      this.pending.set(id, { resolve, reject });
      worker.postMessage({ id, type, payload }, transfer);
    });
  }

  async initSoundfont(sf2: Uint8Array) {
    await this.call("init", { sf2 }, [sf2.buffer]);
  }

  async info(): Promise<{
    FileInfo: {
      ifil: string;
      isng: string;
      INAM: string;
      iver: string;
      ICRD: string;
      IENG: string;
      IPRD: string;
      ICOP: string;
      ICMT: string;
      ISFT: string;
    };
    Presets: {
      bank: number;
      no: number;
      name: string;
    }[];
  }> {
    return this.call("info", {}, []);
  }

  async smfToWav(smf: Uint8Array): Promise<Uint8Array> {
    return this.call("smfToWav", { smf }, [smf.buffer]);
  }
}
