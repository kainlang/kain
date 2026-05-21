#!/usr/bin/env node
import fs from "node:fs";
import { performance } from "node:perf_hooks";

function usage() {
  console.error("usage: node run_wasm_module.mjs <module.wasm> <export> <runs> <warmups>");
  process.exit(2);
}

const [, , wasmPath, exportName = "main", runsArg = "1", warmupsArg = "0"] = process.argv;
if (!wasmPath) {
  usage();
}

const runs = Number.parseInt(runsArg, 10);
const warmups = Number.parseInt(warmupsArg, 10);
if (!Number.isFinite(runs) || runs < 1 || !Number.isFinite(warmups) || warmups < 0) {
  usage();
}

const stdout = [];

function normalizeResult(value) {
  if (typeof value === "bigint") {
    return value.toString();
  }
  if (typeof value === "number") {
    return Number.isInteger(value) ? Math.trunc(value).toString() : value.toString();
  }
  if (typeof value === "undefined") {
    return "";
  }
  return String(value);
}

function makeImport(imp) {
  if (imp.kind === "memory") {
    return new WebAssembly.Memory({ initial: 2, maximum: 256 });
  }
  if (imp.kind === "table") {
    return new WebAssembly.Table({ element: "anyfunc", initial: 32, maximum: 1024 });
  }
  if (imp.kind === "global") {
    return new WebAssembly.Global({ value: "i32", mutable: true }, 0);
  }
  if (imp.kind !== "function") {
    return 0;
  }
  return (...args) => {
    if (imp.name.includes("print") || imp.name.includes("log")) {
      stdout.push(args.map(normalizeResult).join(" "));
    }
    return 0;
  };
}

const bytes = fs.readFileSync(wasmPath);
const module = new WebAssembly.Module(bytes);
const imports = {};
for (const imp of WebAssembly.Module.imports(module)) {
  imports[imp.module] ??= {};
  imports[imp.module][imp.name] = makeImport(imp);
}

const instance = new WebAssembly.Instance(module, imports);
const entry = instance.exports[exportName];
if (typeof entry !== "function") {
  throw new Error(`export '${exportName}' is not a function`);
}

for (let i = 0; i < warmups; i += 1) {
  entry();
}

const durationsMs = [];
let result = null;
let deterministic = true;
for (let i = 0; i < runs; i += 1) {
  const start = performance.now();
  const current = normalizeResult(entry());
  durationsMs.push(performance.now() - start);
  if (result === null) {
    result = current;
  } else if (current !== result) {
    deterministic = false;
  }
}

const stdoutText = stdout.join("\n");
const transcript = `result=${result ?? ""}\nstdout=${stdoutText}\n`;
process.stdout.write(
  JSON.stringify({
    wasm_path: wasmPath,
    export: exportName,
    imports: WebAssembly.Module.imports(module),
    exports: WebAssembly.Module.exports(module),
    result: result ?? "",
    stdout: stdoutText,
    transcript,
    deterministic,
    durations_ms: durationsMs
  })
);
