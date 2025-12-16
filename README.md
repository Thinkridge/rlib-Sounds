# rlib-Sounds

Sound-related libraries for Web and Node.js, built with WebAssembly.

This repository contains SoundFont and MIDI related utilities implemented in **C++**,
with optional **WebAssembly and JavaScript / TypeScript bindings** for modern runtimes.

The core libraries can be used directly as C++ libraries, or consumed via npm packages.

---

## Packages and Libraries

### rlib-soundfont (C++ core)

SoundFont (.sf2) decoder and MIDI-to-WAV converter implemented in C++.

- Can be used as a standalone C++ library
- No JavaScript or WebAssembly dependency required at the core level
- Used internally by the WebAssembly / npm package

📂 C++ Source  
`rlib-SoundFont/src`

---

### @thinkridge/rlib-soundfont (npm package)

WebAssembly + JavaScript/TypeScript bindings for **rlib-soundfont**.

- WebAssembly-based implementation
- Works in Node.js and browser environments
- No native addons required

📦 npm  
https://www.npmjs.com/package/@thinkridge/rlib-soundfont

📂 npm Package Source  
`rlib-SoundFont/npmpackage`

---

### @thinkridge/rlib-mml

Music Macro Language Compiler.

⚠ Experimental package.

---

## Examples

Example applications and demos using the libraries in this repository.

```bash
npm install
npm run dev:reactdemo
npm run dev:smftowav
```
