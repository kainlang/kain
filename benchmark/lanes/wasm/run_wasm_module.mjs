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
const textEncoder = new TextEncoder();
const textDecoder = new TextDecoder();
let wasmMemory = null;
let hostAllocPtr = 1024 * 1024;

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

function ensureHostCapacity(size) {
  if (!wasmMemory) {
    return;
  }
  if (size <= wasmMemory.buffer.byteLength) {
    return;
  }
  const pageSize = 64 * 1024;
  const extraPages = Math.ceil((size - wasmMemory.buffer.byteLength) / pageSize);
  if (extraPages > 0) {
    wasmMemory.grow(extraPages);
  }
}

function allocHostBytes(size) {
  if (!wasmMemory) {
    return 0;
  }
  const alignedSize = (size + 7) & ~7;
  ensureHostCapacity(hostAllocPtr + alignedSize);
  const ptr = hostAllocPtr;
  hostAllocPtr += alignedSize;
  return ptr;
}

function readStringBytes(ptr) {
  if (!wasmMemory || ptr === 0) {
    return new Uint8Array();
  }
  const view = new DataView(wasmMemory.buffer);
  const len = view.getInt32(ptr - 4, true);
  return new Uint8Array(wasmMemory.buffer, ptr, len);
}

function readString(ptr) {
  return textDecoder.decode(readStringBytes(ptr));
}

function writeBytes(bytes) {
  if (!wasmMemory) {
    return 0;
  }
  const base = allocHostBytes(4 + bytes.length);
  const view = new DataView(wasmMemory.buffer);
  view.setInt32(base, bytes.length, true);
  new Uint8Array(wasmMemory.buffer).set(bytes, base + 4);
  return base + 4;
}

function writeString(text) {
  return writeBytes(textEncoder.encode(text));
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
    switch (imp.name) {
      case "print_str": {
        const [ptr, len] = args;
        if (!wasmMemory) {
          return 0;
        }
        const bytes = new Uint8Array(wasmMemory.buffer, Number(ptr), Number(len));
        stdout.push(textDecoder.decode(bytes));
        return 0;
      }
      case "print_i64":
      case "print_f64":
      case "print_bool": {
        stdout.push(args.map(normalizeResult).join(" "));
        return 0;
      }
      case "read_i64":
        return 0n;
      case "int_to_str":
        return writeString(String(args[0]));
      case "str_concat": {
        const left = readStringBytes(Number(args[0]));
        const right = readStringBytes(Number(args[1]));
        const merged = new Uint8Array(left.length + right.length);
        merged.set(left, 0);
        merged.set(right, left.length);
        return writeBytes(merged);
      }
      case "str_eq": {
        const left = readStringBytes(Number(args[0]));
        const right = readStringBytes(Number(args[1]));
        if (left.length !== right.length) {
          return 0;
        }
        for (let i = 0; i < left.length; i += 1) {
          if (left[i] !== right[i]) {
            return 0;
          }
        }
        return 1;
      }
      case "char_at": {
        const bytes = readStringBytes(Number(args[0]));
        const index = Number(args[1]);
        if (!Number.isFinite(index) || index < 0 || index >= bytes.length) {
          return writeString("");
        }
        return writeBytes(Uint8Array.of(bytes[index]));
      }
      case "time_now":
        return BigInt(Date.now());
      default:
        break;
    }
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
wasmMemory = instance.exports.memory instanceof WebAssembly.Memory ? instance.exports.memory : null;
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
