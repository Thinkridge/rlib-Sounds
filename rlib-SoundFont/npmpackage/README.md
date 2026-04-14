# @thinkridge/rlib-soundfont

SoundFont (.sf2) decoder for Web / Node.js using WebAssembly.

## Features

- Works in both Node.js and modern browsers
- Decode SoundFont (.sf2) files
- Convert SMF (MIDI) → WAV audio
- Powered by WebAssembly (fast and portable)
- No external runtime dependencies

## Demo

- https://rlib-mml.thinkridge.jp/

  An interactive demo that lets you create, compile, and play MML entirely in the browser.  
  Supports SoundFont-based playback. No installation required.

## Repository

- **Monorepo root**  
  https://github.com/thinkridge/rlib-Sounds

- **Package location (@thinkridge/rlib-soundfont)**  
  https://github.com/thinkridge/rlib-Sounds/tree/main/rlib-SoundFont/npmpackage

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

MIT © Thinkridge Co., Ltd.

## Build

This package is published with prebuilt artifacts.
End users do not need emsdk or Docker.
