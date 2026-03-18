const readline = require('node:readline');
const { pathToFileURL } = require('node:url');
const path = require('node:path');

const registry = new Map();
let nextRefId = 1;

const safeConsole = {};
for (const level of ['log', 'info', 'warn', 'error']) {
  safeConsole[level] = (...args) => {
    const rendered = args.map((value) => {
      if (typeof value === 'string') return value;
      try { return JSON.stringify(value); } catch { return String(value); }
    }).join(' ');
    process.stderr.write(`[kain-node:${level}] ${rendered}\n`);
  };
}
globalThis.console = safeConsole;

function makeRef(value) {
  const id = nextRefId++;
  registry.set(id, value);
  const ctor = value && value.constructor && value.constructor.name ? value.constructor.name : typeof value;
  let kind = typeof value;
  if (kind === 'object' && value && value[Symbol.toStringTag] === 'Module') kind = 'module';
  if (kind === 'object' && value && typeof value.then === 'function') kind = 'promise';
  return { __kain_js_ref: id, kind, label: `javascript:${kind}:${ctor}` };
}

function isPlainObject(value) {
  if (!value || typeof value !== 'object') return false;
  const proto = Object.getPrototypeOf(value);
  return proto === Object.prototype || proto === null;
}

function toWire(value, raw = false, seen = new Set()) {
  if (value === undefined || value === null) return null;
  if (typeof value === 'boolean' || typeof value === 'string') return value;
  if (typeof value === 'number') return Number.isFinite(value) ? value : String(value);
  if (typeof value === 'bigint') return value.toString();
  if (seen.has(value)) return makeRef(value);
  if (raw) return makeRef(value);

  seen.add(value);
  try {
    if (Array.isArray(value)) return value.map((entry) => toWire(entry, false, seen));
    if (ArrayBuffer.isView(value)) return Array.from(value);
    if (value instanceof ArrayBuffer) return Array.from(new Uint8Array(value));
    if (isPlainObject(value)) {
      const out = {};
      for (const [key, entry] of Object.entries(value)) out[key] = toWire(entry, false, seen);
      return out;
    }
    return makeRef(value);
  } finally {
    seen.delete(value);
  }
}

function fromWire(value) {
  if (Array.isArray(value)) return value.map(fromWire);
  if (value && typeof value === 'object') {
    if (typeof value.__kain_js_ref === 'number') return registry.get(value.__kain_js_ref);
    const out = {};
    for (const [key, entry] of Object.entries(value)) out[key] = fromWire(entry);
    return out;
  }
  return value;
}

function globalEval(code) {
  return (0, eval)(code);
}

async function resolveSpecifier(specifier) {
  if (specifier.startsWith('.') || specifier.startsWith('/') || /^[A-Za-z]:[\\/]/.test(specifier)) {
    const absolute = path.isAbsolute(specifier) ? specifier : path.resolve(process.cwd(), specifier);
    return pathToFileURL(absolute).href;
  }
  return specifier;
}

function resolveTarget(target) {
  if (typeof target === 'string') return globalThis[target];
  return fromWire(target);
}

async function handleRequest(message) {
  switch (message.op) {
    case 'exec': {
      const result = globalEval(String(message.code ?? ''));
      if (result && typeof result.then === 'function') await result;
      return null;
    }
    case 'eval': {
      const result = globalEval(String(message.code ?? ''));
      const awaited = result && typeof result.then === 'function' ? await result : result;
      return toWire(awaited, Boolean(message.raw));
    }
    case 'import': {
      const specifier = await resolveSpecifier(String(message.specifier ?? ''));
      const imported = await import(specifier);
      return toWire(imported, Boolean(message.raw));
    }
    case 'getattr': {
      const target = resolveTarget(message.target);
      const value = target == null ? undefined : target[String(message.name ?? '')];
      const awaited = value && typeof value.then === 'function' ? await value : value;
      return toWire(awaited, Boolean(message.raw));
    }
    case 'setattr': {
      const target = resolveTarget(message.target);
      if (target == null) throw new Error(`cannot set property on ${target}`);
      target[String(message.name ?? '')] = fromWire(message.value);
      return null;
    }
    case 'hasattr': {
      const target = resolveTarget(message.target);
      return Boolean(target != null && String(message.name ?? '') in target);
    }
    case 'call': {
      const target = resolveTarget(message.target);
      if (typeof target !== 'function') throw new Error(`target is not callable: ${typeof target}`);
      const args = Array.isArray(message.args) ? message.args.map(fromWire) : [];
      const result = target(...args);
      const awaited = result && typeof result.then === 'function' ? await result : result;
      return toWire(awaited, Boolean(message.raw));
    }
    case 'call_method': {
      const target = resolveTarget(message.target);
      if (target == null) throw new Error('call_method target is null');
      const method = target[String(message.name ?? '')];
      if (typeof method !== 'function') throw new Error(`property ${String(message.name ?? '')} is not callable`);
      const args = Array.isArray(message.args) ? message.args.map(fromWire) : [];
      const result = method.apply(target, args);
      const awaited = result && typeof result.then === 'function' ? await result : result;
      return toWire(awaited, Boolean(message.raw));
    }
    case 'buffer_info': {
      const target = resolveTarget(message.target);
      if (target == null) throw new Error('buffer_info target is null');
      let view = null;
      let kind = 'unknown';
      let dtype = 'u8';
      if (ArrayBuffer.isView(target)) {
        view = new Uint8Array(target.buffer, target.byteOffset, target.byteLength);
        kind = target.constructor && target.constructor.name ? target.constructor.name : 'TypedArray';
        dtype = kind.toLowerCase();
      } else if (target instanceof ArrayBuffer) {
        view = new Uint8Array(target);
        kind = 'ArrayBuffer';
        dtype = 'u8';
      } else if (typeof Buffer !== 'undefined' && Buffer.isBuffer(target)) {
        view = new Uint8Array(target.buffer, target.byteOffset, target.byteLength);
        kind = 'Buffer';
        dtype = 'u8';
      } else {
        throw new Error(`target is not buffer-like: ${typeof target}`);
      }
      return {
        kind,
        dtype,
        byte_length: view.byteLength,
        length: 'length' in target ? Number(target.length) : view.byteLength,
      };
    }
    case 'buffer_bytes': {
      const target = resolveTarget(message.target);
      if (target == null) throw new Error('buffer_bytes target is null');
      if (ArrayBuffer.isView(target)) {
        return Array.from(new Uint8Array(target.buffer, target.byteOffset, target.byteLength));
      }
      if (target instanceof ArrayBuffer) {
        return Array.from(new Uint8Array(target));
      }
      if (typeof Buffer !== 'undefined' && Buffer.isBuffer(target)) {
        return Array.from(target.values());
      }
      throw new Error(`target is not buffer-like: ${typeof target}`);
    }
    default:
      throw new Error(`unknown op: ${message.op}`);
  }
}

const rl = readline.createInterface({ input: process.stdin, crlfDelay: Infinity });
rl.on('line', async (line) => {
  if (!line.trim()) return;
  let message = null;
  try {
    message = JSON.parse(line);
    const result = await handleRequest(message);
    process.stdout.write(JSON.stringify({ id: message.id, ok: true, result }) + '\n');
  } catch (error) {
    process.stdout.write(JSON.stringify({
      id: message && message.id ? message.id : 0,
      ok: false,
      error: error && error.stack ? String(error.stack) : String(error),
    }) + '\n');
  }
});
