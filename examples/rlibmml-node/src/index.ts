#!/usr/bin/env node

import * as fs from "node:fs/promises";
import * as process from "node:process";
import { Command } from "commander";
import { getRlibMml } from "@thinkridge/rlib-mml";

try {
  const rlibMml = await getRlibMml();

  function readInputText(file?: string): Promise<string> {
    if (file) return fs.readFile(file, "utf8");
    return new Promise((resolve) => {
      let data = "";
      process.stdin.setEncoding("utf8");
      process.stdin.on("data", (c) => (data += c));
      process.stdin.on("end", () => resolve(data));
    });
  }

  function readInputBinary(file?: string): Promise<Buffer> {
    if (file) return fs.readFile(file);
    return new Promise((resolve) => {
      const chunks: Buffer[] = [];
      process.stdin.on("data", (c) => chunks.push(c));
      process.stdin.on("end", () => resolve(Buffer.concat(chunks)));
    });
  }

  function writeOutput(data: string | Uint8Array, file?: string) {
    if (file) return fs.writeFile(file, data);
    process.stdout.write(data);
  }

  const program = new Command();

  program.name("rlibmml").description("MML <-> SMF converter").version("0.0.1");

  program.addCommand(
    new Command("mmltosmf")
      .option("-i, --input <file>", "input MML file (default: stdin)")
      .option("-o, --output <file>", "output SMF file (default: stdout)")
      .action(async (opts) => {
        const mml = await readInputText(opts.input);
        const r = rlibMml.mmlToSmf(mml);
        if (!r.smf) throw new Error(JSON.stringify(r.errors));
        await writeOutput(r.smf, opts.output);
      }),
  );

  program.addCommand(
    new Command("smftomml")
      .option("-i, --input <file>", "input SMF file (default: stdin)")
      .option("-o, --output <file>", "output MML file (default: stdout)")
      .action(async (opts) => {
        const smf = await readInputBinary(opts.input);
        const mml = rlibMml.smfToMml(smf);
        await writeOutput(mml, opts.output);
      }),
  );

  await program.parseAsync(process.argv);
} catch (e) {
  console.error(e);
  process.exit(1);
}
