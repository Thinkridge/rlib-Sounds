# @thinkridge/rlib-soundfont

SoundFont, FM, and PSG audio rendering for Web / Node.js, powered by WebAssembly.

Converts Standard MIDI Files (SMF) to WAV audio. Each track can be rendered
through a SoundFont (.sf2), or through the built-in FM (YM2203/OPN) or PSG
(SSG) chiptune synth engines — freely mixed within a single song.

## Features

- Works in both Node.js and modern browsers
- Decode SoundFont (.sf2) files
- Convert SMF (MIDI) → WAV audio
- Built-in FM (YM2203/OPN) and PSG (SSG) synth engines, powered by the [ymfm](https://github.com/aaronsgiles/ymfm) sound chip emulator
- SoundFont, FM, and PSG tracks can be freely mixed within a single SMF
- Powered by WebAssembly (fast and portable)
- No external runtime dependencies

## Demo

- https://rlib-mml.thinkridge.jp/

  An interactive demo that lets you create, compile, and play MML entirely in the browser.  
  Supports SoundFont, FM, and PSG based playback. No installation required.

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

## Rendering SoundFont / FM / PSG tracks

No separate API is needed to use the FM/PSG engines — `smfToWav()` picks the
renderer per track automatically, using the instrument named in each track's
Track Name meta event (the same convention used by
[rlib-MML](https://github.com/tr-takatsuka/rlib-MML)'s
`CreatePort(instrument:...)`):

- `"fm"` — rendered with the built-in FM (YM2203/OPN) synth engine
- `"psg"` — rendered with the built-in PSG (SSG) synth engine
- any other name (including the default, unnamed track) — rendered with the
  SoundFont passed to `initSoundfont()`

A single SMF can freely mix SoundFont, FM, and PSG tracks. Custom voice
presets can also be embedded in the SMF via rlib-MML's `DefinePresetFM()` /
`DefinePresetPSG()` meta events.

## License

MIT © Thinkridge Co., Ltd.

### Notes

This package's WebAssembly build is compiled from the C++ core at
`rlib-SoundFont/src`, which includes a vendored copy of the **ymfm** FM/PSG sound chip emulation library (https://github.com/aaronsgiles/ymfm) under `rlib-SoundFont/src/ymfm`.
ymfm is released under the **BSD 3-Clause License**, Copyright (c) 2021,
Aaron Giles. See `rlib-SoundFont/src/ymfm/LICENSE` for the full license text.

## Build

This package is published with prebuilt artifacts.
End users do not need emsdk or Docker.
