# @thinkridge/rlib-mml

Music Macro Language (MML) compiler and decompiler for Web and Node.js, powered by WebAssembly.

## Repository

- **Monorepo root**  
  https://github.com/thinkridge/rlib-Sounds

- **Package location (@thinkridge/rlib-mml)**  
  https://github.com/thinkridge/rlib-Sounds/tree/main/rlib-MML/npmpackage

## Installation

```bash
npm install @thinkridge/rlib-mml
```

## Usage (Node.js)

```javascript
import * as fs from "node:fs/promises";
import { getRlibMml } from "@thinkridge/rlib-mml";

try {
  const rlibMml = await getRlibMml();
  const mml = await fs.readFile("test.mml", "utf8");
  const smf = rlibMml.mmlToSmf(mml); // MML -> SMF (Standard MIDI File)
  await fs.writeFile("test.mid", smf);
  const decMml = rlibMml.smfToMml(smf); // SMF -> MML
  await fs.writeFile("smftomml.mml", decMml);
} catch (e) {
  console.error(e);
}
```

## MML Reference

See the full MML specification:  
https://github.com/tr-takatsuka/rlib-MML/blob/master/README.md#mml-music-macro-language

## License

MIT © Thinkridge Co., Ltd.

### Notes

This package includes code derived from the **rlib-MML** project  
(https://github.com/tr-takatsuka/rlib-MML), which is released under the  
**CC0 1.0 Universal** license.

## Build

This package is published with prebuilt artifacts.
End users do not need emsdk or Docker.
