
const getFunction = (instance: unknown, funcName: string) => {
  if (typeof instance === "object" && instance && funcName in instance) {
    return instance[funcName as keyof typeof instance];
  }
  throw Error(`function not found: ${funcName}`);
};

let promiseRlibMml: Promise<unknown>;
export const getInstanceRlibMml = async () => {
  if (!promiseRlibMml) {
    const hModule = require("../../rlib-MML/wasm/build/rlibmml.js");
    promiseRlibMml = hModule();
  }
  const instance = await promiseRlibMml;

  return {
    mmlToSmf: getFunction(instance,"mmlToSmf") as (mml: string) => {
      ok: boolean;
      result: Uint8Array;
      message: string;
    },
    smfToMml: getFunction(instance,"smfToMml") as (smf: Uint8Array) => {
      ok: boolean;
      result: string;
      message: string;
    },
  };
};


// type AppFuture = {
//   delete: () => void;
//   isProgress: () => boolean;
//   get: () => {
//     ok: number;
//     errorCode: number;
//     message?: string;
//     result?: Uint8Array;
//   };
// };

// export type Soundfont = {
//   delete: () => void;
//   info: () => {
//     FileInfo: {
//       ifil: string;
//       isng: string;
//       INAM: string;
//       iver: string;
//       ICRD: string;
//       IENG: string;
//       IPRD: string;
//       ICOP: string;
//       ICMT: string;
//       ISFT: string;
//     },
//     Presets: {
//       bank: number;
//       no: number;
//       name: string;
//     }[],
//   };
// };

// let promiseRlibSoundfont: Promise<unknown>;
// export const getInstanceRlibSoundfont = async () => {
//   if (!promiseRlibSoundfont) {
//     const url = "data:text/javascript;base64," + process.env.rlibsoundfontdecoderjs;
//     const ModuleSoundfont = await eval("import(url)"); // await import("rlibsoundfontdecoder.js")
//     promiseRlibSoundfont = ModuleSoundfont.default();
//   }
//   const instance = await promiseRlibSoundfont;

//   const getFuture = async (future: AppFuture) => {
//     return new Promise<ReturnType<AppFuture["get"]>>(async (resolve, reject) => {
//       try {
//         const retry = () => {
//           if (future.isProgress()) {
//             setTimeout(() => retry(), 100);
//           } else {
//             const result = future.get();
//             future.delete();
//             resolve(result);
//           }
//         };
//         retry();
//       } catch (e) {
//         reject(e);
//       }
//     });
//   };

//   return {
//     loadSoundfont: async (uint8Array: Uint8Array) => {
//       return new Promise<Soundfont>(async (resolve, reject) => {
//         try {
//           const loadSoundfont = getFunction(instance, "loadSoundfont") as (uint8Array: Uint8Array) => AppFuture;
//           const future = loadSoundfont(uint8Array);
//           const result = await getFuture(future);
//           if (result.errorCode) {
//             throw Error(result.message);
//           }
//           resolve(result.result as unknown as Soundfont);
//         } catch (e) {
//           console.error(e);
//           reject(e);
//         }
//       });
//     },

//     smfToWav: async (soundfont: Soundfont, uint8Array: Uint8Array) => {
//       return new Promise<Uint8Array>(async (resolve, reject) => {
//         try {
//           const smfToWav = getFunction(instance, "smfToWav") as (soundfont: Soundfont, uint8Array: Uint8Array) => AppFuture;
//           const future = smfToWav(soundfont, uint8Array);
//           const result = await getFuture(future);
//           if (result.errorCode) {
//             throw Error(result.message);
//           }
//           resolve(result.result as unknown as Uint8Array);
//         } catch (e) {
//           console.error(e);
//           reject(e);
//         }
//       });
//     },
//   };
// };


