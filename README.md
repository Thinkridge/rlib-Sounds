# rlib-Sounds

Sound-related libraries for Web and Node.js, built with WebAssembly.

This repository contains SoundFont and MIDI-related utilities implemented in **C++**,
with optional **WebAssembly and JavaScript/TypeScript bindings** for modern runtimes.

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
[https://www.npmjs.com/package/@thinkridge/rlib-soundfont](https://www.npmjs.com/package/@thinkridge/rlib-soundfont)

📂 npm Package Source
`rlib-SoundFont/npmpackage`

---

### @thinkridge/rlib-mml (npm package)

Music Macro Language (MML) compiler and decompiler.

- WebAssembly-based implementation
- Works in Node.js and browser environments
- No native addons required

📦 npm
[https://www.npmjs.com/package/@thinkridge/rlib-mml](https://www.npmjs.com/package/@thinkridge/rlib-mml)

📂 npm Package Source
`rlib-MML/npmpackage`

---

## Development Setup

Clone the repository with submodules:

```bash
git clone --recurse-submodules https://github.com/thinkridge/rlib-Sounds
```

### Build Environment (Docker + Emscripten)
Building WebAssembly artifacts requires Docker and an Emscripten toolchain image.
We use the following Dockerfile to build the Emscripten environment:

https://github.com/tr-takatsuka/emsdk-docker

Build the Docker image:

```bash
docker build \
  --build-arg EMSCRIPTEN_VERSION=4.0.21 \
  --build-arg BOOST_VERSION=1.89.0 \
  -t emsdk .
```

---

## Examples

Example applications and demos using the libraries in this repository.

```bash
npm install
npm run build

npm run dev:reactdemo
npm run dev:nextdemo
npm run dev:smftowav
npm run dev:rlibmml
```

---

## License

See each package for license details.
The repository contains components under MIT and CC0 (for upstream rlib-MML code).
