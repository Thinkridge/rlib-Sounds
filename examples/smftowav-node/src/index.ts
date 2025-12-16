#!/usr/bin/env node

import * as fs from "node:fs/promises";
import * as path from "node:path";
import * as process from "node:process";
import { Command } from "commander";

import { RlibSoundfont } from "@thinkridge/rlib-soundfont";

try {
  const program = new Command();

  program
    .name("smftowav")
    .description("Convert Standard MIDI File to WAV using SoundFont")
    .requiredOption("-s, --soundfont <file>", "SoundFont (.sf2) file")
    .requiredOption("-i, --input <file>", "Input MIDI (.mid) file")
    .requiredOption("-o, --output <file>", "Output WAV (.wav) file")
    // .option("--rate <number>", "Sample rate (reserved)", "44100")
    .parse(process.argv);

  const options = program.opts<{
    soundfont: string;
    input: string;
    output: string;
    // rate: string;
  }>();

  const rlibSoundfont = new RlibSoundfont();
  try {
    const sf2Binary = new Uint8Array(await fs.readFile(options.soundfont));
    const smfBinary = new Uint8Array(await fs.readFile(options.input));
    await rlibSoundfont.initSoundfont(sf2Binary);
    const wavBinary = await rlibSoundfont.smfToWav(smfBinary); // smf -> wav
    await fs.writeFile(options.output, wavBinary);  // 書き込み
    console.log(`WAV generated: ${path.resolve(options.output)}`);
  } finally {
    rlibSoundfont.dispose();
  }
} catch (e) {
  console.error(e);
  process.exit(1);
}
