# @thinkridge/rlib-mml

Music Macro Language (MML) compiler and decompiler for Web and Node.js, powered by WebAssembly.

## Features
- Works in both Node.js and modern browsers
- Powered by WebAssembly (fast and portable)
- Convert MML ⇄ SMF (Standard MIDI File)
- No external runtime dependencies

## Demo

- https://rlib-mml.thinkridge.jp/

  An interactive demo that lets you create, compile, and play MML entirely in the browser.  
  Supports SoundFont-based playback. No installation required.

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
  const r = rlibMml.mmlToSmf(mml); // MML -> SMF (Standard MIDI File)
  if (!r.smf) throw new Error(JSON.stringify(r.errors));  // succeeded?
  await fs.writeFile("test.mid", r.smf);
  const decMml = rlibMml.smfToMml(r.smf); // SMF -> MML
  await fs.writeFile("smftomml.mml", decMml);
} catch (e) {
  console.error(e);
}
```

## Usage: measures / tick / time conversion (Timeline)

```javascript
const rlibMml = await getRlibMml();
const { smf } = rlibMml.mmlToSmf(mml);
const timeline = rlibMml.getTimeline(smf);
try {
  const loc = timeline.getLocationFromTick(1920);   // { measure, beat, tick } (0-based)
  const loc2 = timeline.getLocationFromTime(10);    // time (seconds from the start) -> Location
  const sec = timeline.getTimeFromLocation(loc2);   // Location -> time (seconds)
  const tick = timeline.getTickFromTime(sec);       // time (seconds) -> tick (4th note = 480 ticks)
  const bars = timeline.getMeasureTicks(9600);   // Uint32Array: start tick of each measure, up to the measure containing tick 9600 (plus the next bar line)
  const sections = timeline.getSections();        // time signature sections
} finally {
  timeline.dispose(); // release (also released automatically on GC)
}
```

- The timeline has no end: values at any position are converted as if the last time signature and tempo continue forever.
- Negative values are treated as 0. Time to tick is rounded down.
- A time signature change in the middle of a measure starts a new measure from that point.

## Usage: tracks of a SMF (getTracks)

```javascript
const data = rlibMml.getTracks(smf); // { ticksPerQuarter: 480, endTick, tracks: [{ name, instrument, channel, port?, events: [...] }, ...] }
for (const track of data.tracks) {
  console.log(track.name, track.channel, track.port);
  for (const e of track.events) {
    switch (e.type) {
      case "note": console.log(e.tick, e.length, e.note, e.velocity); break;
      case "tempo": console.log(e.tick, e.bpm); break;
      case "timeSignature": console.log(e.tick, `${e.numerator}/${e.denominator}`); break;
      // "programChange" | "controlChange" | "pitchBend" | "meta" | "sysEx"
    }
  }
}
```

- A track is one channel (and port). A SMF track that uses several channels becomes several tracks. Events of each track are sorted by tick. The resolution is converted to 480 ticks per quarter note.
- NoteOn and NoteOff are merged into one `note` event with `length`. A NoteOn with velocity 0 is treated as NoteOff. A note that is not ended lasts until the next NoteOn of the same note, up to an eighth note.
- Polyphonic key pressure and channel pressure are not included.
- `meta` has `metaType` and `data` (bytes). For text, decode it with `new TextDecoder().decode(Uint8Array.from(e.data))`.
- `endTick` is the position of the last event of all tracks (the end position for notes).

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
