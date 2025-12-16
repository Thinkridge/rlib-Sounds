# @thinkridge/rlib-soundfont

SoundFont (.sf2) decoder for Web / Node.js using WebAssembly.

## Repository

GitHub: https://github.com/thinkridge/rlib-Sounds

## Installation

```bash
npm install @thinkridge/rlib-soundfont
```

## Usage (Node.js)

```javascript
import * as fs from "node:fs/promises";
import { RlibSoundfont } from "@thinkridge/rlib-soundfont";

const rlibSoundfont = new RlibSoundfont();
try {
  const sf2Binary = new Uint8Array(await fs.readFile("example.sf2"));
  const smfBinary = new Uint8Array(await fs.readFile("test.mid"));
  await rlibSoundfont.initSoundfont(sf2Binary);
  const wavBinary = await rlibSoundfont.smfToWav(smfBinary); // smf -> wav
} catch (e) {
  console.error(e);
} finally {
  rlibSoundfont.dispose();
}
```

## License

MIT

## Build

This package is published with prebuilt artifacts.
End users do not need emsdk or Docker.
