use std::collections::{BTreeMap, HashMap};
use std::fs;
use std::io::{BufRead, BufReader, Write};
use std::path::{Path, PathBuf};
use std::process::{Child, ChildStdin, ChildStdout, Command, Stdio};
use std::sync::{Arc, Mutex, Once, RwLock};

use kain_core::error::{KainError, KainResult};
use kain_core::runtime::{register_env_extension, Env, Value};
use kain_core::stdlib::{register_stdlib_extension, BuiltinFn, StdLib};
use kain_core::CompileTarget;
use kain_interop::{
    shared_buffer_value, shared_image_value, KainSharedBuffer, KainSharedImage,
    SharedBufferMetadata, SharedImageMetadata,
};
use once_cell::sync::Lazy;
use regex::Regex;
use serde::Deserialize;
use serde_json::{json, Map, Value as JsonValue};

const NODE_EXTENSION_KEY: &str = "kain.node.runtime";
const KAIN_MANIFEST_NAMES: &[&str] = &["KAIN.toml", "kain.toml"];

static REGISTER: Once = Once::new();

const NODE_BRIDGE_SOURCE: &str = r#"const readline = require('node:readline');
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
    if (
      (out.contract === 'kain.shared.buffer' || out.contract === 'kain.shared.image') &&
      Array.isArray(out.bytes)
    ) {
      out.bytes = Uint8Array.from(out.bytes);
    }
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

const textEncoder = new TextEncoder();

function utf8View(text) {
  return textEncoder.encode(String(text ?? ''));
}

function bufferView(target) {
  if (target == null) return null;
  if (Array.isArray(target)) {
    return Uint8Array.from(target);
  }
  if (ArrayBuffer.isView(target)) {
    return new Uint8Array(target.buffer, target.byteOffset, target.byteLength);
  }
  if (target instanceof ArrayBuffer) {
    return new Uint8Array(target);
  }
  if (typeof Buffer !== 'undefined' && Buffer.isBuffer(target)) {
    return new Uint8Array(target.buffer, target.byteOffset, target.byteLength);
  }
  return null;
}

function normalizeBufferPayload(target) {
  const direct = bufferView(target);
  if (direct) {
    let kind = 'buffer';
    if (ArrayBuffer.isView(target)) {
      kind = target.constructor && target.constructor.name ? target.constructor.name : 'TypedArray';
    } else if (target instanceof ArrayBuffer) {
      kind = 'ArrayBuffer';
    } else if (typeof Buffer !== 'undefined' && Buffer.isBuffer(target)) {
      kind = 'Buffer';
    }
    return {
      kind,
      dtype: 'u8',
      byte_length: direct.byteLength,
      length: 'length' in Object(target) ? Number(target.length) : direct.byteLength,
      bytes: direct,
    };
  }
  if (!target || typeof target !== 'object') {
    throw new Error(`unsupported buffer payload: ${typeof target}`);
  }
  const bytes = bufferView(target.bytes ?? target.buffer ?? target.data ?? null);
  if (!bytes) {
    throw new Error(`target is not buffer-like: ${typeof target}`);
  }
  const length = Number(
    target.length ?? (Array.isArray(target.shape) && target.shape.length > 0 ? target.shape[0] : bytes.byteLength)
  );
  return {
    kind: typeof target.kind === 'string' ? target.kind : 'buffer',
    dtype:
      typeof target.element_type === 'string' ? target.element_type :
      typeof target.dtype === 'string' ? target.dtype :
      'u8',
    byte_length: bytes.byteLength,
    length: Number.isFinite(length) ? length : bytes.byteLength,
    bytes,
  };
}

function inferExtension(mimeType, fallback) {
  switch (mimeType) {
    case 'text/html':
      return 'html';
    case 'image/svg+xml':
      return 'svg';
    case 'application/json':
      return 'json';
    case 'image/png':
      return 'png';
    default:
      return fallback;
  }
}

function normalizeDocumentPayload(target) {
  if (typeof target === 'string') {
    const text = target;
    return {
      kind: 'document',
      title: '',
      mime_type: 'text/html',
      extension: 'html',
      byte_length: utf8View(text).byteLength,
      text,
    };
  }
  if (!target || typeof target !== 'object') {
    throw new Error(`unsupported document payload: ${typeof target}`);
  }
  const text =
    typeof target.text === 'string' ? target.text :
    typeof target.html === 'string' ? target.html :
    typeof target.source === 'string' ? target.source :
    typeof target.document === 'string' ? target.document :
    null;
  if (text == null) {
    throw new Error('document payload is missing a text/html/source field');
  }
  const bytes = bufferView(target.bytes);
  const mimeType = typeof target.mime_type === 'string' ? target.mime_type : 'text/html';
  return {
    kind: typeof target.kind === 'string' ? target.kind : 'document',
    title: typeof target.title === 'string' ? target.title : '',
    mime_type: mimeType,
    extension: typeof target.extension === 'string' ? target.extension : inferExtension(mimeType, 'html'),
    byte_length: bytes ? bytes.byteLength : utf8View(text).byteLength,
    text,
  };
}

function normalizeImagePayload(target) {
  if (typeof target === 'string') {
    const text = target;
    const mimeType = text.trimStart().startsWith('<svg') ? 'image/svg+xml' : 'text/plain';
    return {
      kind: 'image',
      mime_type: mimeType,
      extension: inferExtension(mimeType, 'txt'),
      width: 0,
      height: 0,
      channels: 0,
      row_stride: 0,
      layout: '',
      pixel_format: '',
      representation: 'encoded',
      color_space: 'srgb',
      alpha_mode: 'opaque',
      byte_length: utf8View(text).byteLength,
      text,
      bytes: utf8View(text),
    };
  }
  if (!target || typeof target !== 'object') {
    throw new Error(`unsupported image payload: ${typeof target}`);
  }
  const text =
    typeof target.text === 'string' ? target.text :
    typeof target.svg === 'string' ? target.svg :
    typeof target.source === 'string' ? target.source :
    null;
  const bytes = bufferView(target.bytes ?? target.buffer ?? target.data ?? null);
  const inferredSvg = text != null && text.trimStart().startsWith('<svg');
  const mimeType =
    typeof target.mime_type === 'string' ? target.mime_type :
    inferredSvg ? 'image/svg+xml' :
    bytes ? 'application/octet-stream' :
    'text/plain';
  const width = Number(target.width ?? 0);
  const height = Number(target.height ?? 0);
  const channels = Number(target.channels ?? 0);
  const rowStride = Number(target.row_stride ?? (Number.isFinite(width) && Number.isFinite(channels) ? width * channels : 0));
  return {
    kind: typeof target.kind === 'string' ? target.kind : 'image',
    mime_type: mimeType,
    extension: typeof target.extension === 'string' ? target.extension : inferExtension(mimeType, text != null ? 'txt' : 'bin'),
    width: Number.isFinite(width) ? width : 0,
    height: Number.isFinite(height) ? height : 0,
    channels: Number.isFinite(channels) ? channels : 0,
    row_stride: Number.isFinite(rowStride) ? rowStride : 0,
    layout: typeof target.layout === 'string' ? target.layout : '',
    pixel_format: typeof target.pixel_format === 'string' ? target.pixel_format : '',
    representation: typeof target.representation === 'string' ? target.representation : (bytes && width > 0 && height > 0 ? 'raster' : 'encoded'),
    color_space: typeof target.color_space === 'string' ? target.color_space : 'srgb',
    alpha_mode: typeof target.alpha_mode === 'string' ? target.alpha_mode : (channels === 4 ? 'straight' : 'opaque'),
    byte_length: bytes ? bytes.byteLength : utf8View(text ?? '').byteLength,
    text,
    bytes: bytes ?? (text != null ? utf8View(text) : null),
  };
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
      const payload = normalizeBufferPayload(resolveTarget(message.target));
      return {
        kind: payload.kind,
        dtype: payload.dtype,
        byte_length: payload.byte_length,
        length: payload.length,
      };
    }
    case 'buffer_bytes': {
      const payload = normalizeBufferPayload(resolveTarget(message.target));
      return Array.from(payload.bytes);
    }
    case 'document_info': {
      const payload = normalizeDocumentPayload(resolveTarget(message.target));
      return {
        kind: payload.kind,
        title: payload.title,
        mime_type: payload.mime_type,
        extension: payload.extension,
        byte_length: payload.byte_length,
      };
    }
    case 'document_text': {
      const payload = normalizeDocumentPayload(resolveTarget(message.target));
      return payload.text;
    }
    case 'image_info': {
      const payload = normalizeImagePayload(resolveTarget(message.target));
      return {
        kind: payload.kind,
        mime_type: payload.mime_type,
        extension: payload.extension,
        width: payload.width,
        height: payload.height,
        channels: payload.channels,
        row_stride: payload.row_stride,
        layout: payload.layout,
        pixel_format: payload.pixel_format,
        representation: payload.representation,
        color_space: payload.color_space,
        alpha_mode: payload.alpha_mode,
        byte_length: payload.byte_length,
      };
    }
    case 'image_text': {
      const payload = normalizeImagePayload(resolveTarget(message.target));
      if (payload.text == null) {
        throw new Error('image payload does not expose text content');
      }
      return payload.text;
    }
    case 'image_bytes': {
      const payload = normalizeImagePayload(resolveTarget(message.target));
      if (!payload.bytes) {
        throw new Error('image payload does not expose byte content');
      }
      return Array.from(payload.bytes);
    }
    case 'image_buffer': {
      const payload = normalizeImagePayload(resolveTarget(message.target));
      if (!payload.bytes) {
        throw new Error('image payload does not expose byte content');
      }
      return toWire(payload.bytes, true);
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
"#;

#[derive(Debug, Clone, Default, Deserialize)]
pub struct NodeFfiConfig {
    #[serde(default)]
    pub command: Option<String>,
    #[serde(default)]
    pub args: Vec<String>,
    #[serde(default)]
    pub cwd: Option<PathBuf>,
    #[serde(default)]
    pub env: BTreeMap<String, String>,
}

#[derive(Debug, Clone, Default, Deserialize)]
struct KainNodeManifest {
    #[serde(default)]
    node_ffi: Option<NodeFfiConfig>,
}

#[derive(Debug, Clone)]
struct NodeResolvedConfig {
    command: String,
    args: Vec<String>,
    cwd: PathBuf,
    env: BTreeMap<String, String>,
    bridge_script_path: PathBuf,
}

struct NodeRuntimeState {
    config: Result<NodeResolvedConfig, String>,
    process: Mutex<Option<NodeProcess>>,
}

struct NodeProcess {
    child: Child,
    stdin: ChildStdin,
    stdout: BufReader<ChildStdout>,
    next_id: u64,
}

#[derive(Clone)]
struct NodeObjectRef {
    id: u64,
    label: String,
}

pub fn register() {
    REGISTER.call_once(|| {
        kain_interop::register();
        register_stdlib_extension("javascript", register_node_stdlib);
        register_env_extension("javascript", register_node_env);
    });
}

pub fn prepare_source_for_runtime(
    source: &str,
    target: CompileTarget,
) -> Result<String, KainError> {
    if !source_uses_node_ffi(source) {
        return Ok(source.to_string());
    }
    if !matches!(target, CompileTarget::Interpret | CompileTarget::Test) {
        return Err(KainError::runtime(
            "JavaScript/Node FFI is only available in host-backed Kain execution lanes for now",
        ));
    }
    Ok(source.to_string())
}

fn register_node_stdlib(stdlib: &mut StdLib) {
    for builtin in [
        BuiltinFn {
            name: "js_eval",
            params: vec![("code", "String")],
            return_type: "Any",
            doc: "Evaluate JavaScript in the persistent Node runtime",
        },
        BuiltinFn {
            name: "js_eval_raw",
            params: vec![("code", "String")],
            return_type: "Any",
            doc: "Evaluate JavaScript and keep the raw host object handle",
        },
        BuiltinFn {
            name: "js_exec",
            params: vec![("code", "String")],
            return_type: "Unit",
            doc: "Execute JavaScript statements in the persistent Node runtime",
        },
        BuiltinFn {
            name: "js_import",
            params: vec![("specifier", "String")],
            return_type: "Any",
            doc: "Import a JavaScript or Node module",
        },
        BuiltinFn {
            name: "js_import_raw",
            params: vec![("specifier", "String")],
            return_type: "Any",
            doc: "Import a JavaScript or Node module and keep the raw module handle",
        },
        BuiltinFn {
            name: "js_call",
            params: vec![("target", "Any"), ("args", "Any")],
            return_type: "Any",
            doc: "Call a JavaScript function or callable host handle",
        },
        BuiltinFn {
            name: "js_call_raw",
            params: vec![("target", "Any"), ("args", "Any")],
            return_type: "Any",
            doc: "Call JavaScript and keep the raw result handle when needed",
        },
        BuiltinFn {
            name: "js_call_method",
            params: vec![("target", "Any"), ("name", "String"), ("args", "Any")],
            return_type: "Any",
            doc: "Call a JavaScript method with its original this binding",
        },
        BuiltinFn {
            name: "js_call_method_raw",
            params: vec![("target", "Any"), ("name", "String"), ("args", "Any")],
            return_type: "Any",
            doc: "Call a JavaScript method and keep complex results as raw handles",
        },
        BuiltinFn {
            name: "js_getattr",
            params: vec![("target", "Any"), ("name", "String")],
            return_type: "Any",
            doc: "Read a property from a JavaScript object or module",
        },
        BuiltinFn {
            name: "js_getattr_raw",
            params: vec![("target", "Any"), ("name", "String")],
            return_type: "Any",
            doc: "Read a JavaScript property and keep complex results as raw handles",
        },
        BuiltinFn {
            name: "js_setattr",
            params: vec![("target", "Any"), ("name", "String"), ("value", "Any")],
            return_type: "Unit",
            doc: "Set a property on a JavaScript object",
        },
        BuiltinFn {
            name: "js_hasattr",
            params: vec![("target", "Any"), ("name", "String")],
            return_type: "Bool",
            doc: "Check whether a JavaScript object exposes a property",
        },
        BuiltinFn {
            name: "js_buffer_info",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Inspect buffer-like JavaScript payloads such as Uint8Array, Buffer, and ArrayBuffer",
        },
        BuiltinFn {
            name: "js_buffer_bytes",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Snapshot a JavaScript buffer-like payload into byte values",
        },
        BuiltinFn {
            name: "js_document_info",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Inspect a JavaScript document payload with metadata like mime type and byte length",
        },
        BuiltinFn {
            name: "js_document_text",
            params: vec![("target", "Any")],
            return_type: "String",
            doc: "Extract document text from a JavaScript document payload",
        },
        BuiltinFn {
            name: "js_image_info",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Inspect a JavaScript image or canvas payload with dimensions and byte length",
        },
        BuiltinFn {
            name: "js_image_text",
            params: vec![("target", "Any")],
            return_type: "String",
            doc: "Extract text-backed image content such as SVG from a JavaScript payload",
        },
        BuiltinFn {
            name: "js_image_bytes",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Snapshot byte-backed image payloads into byte values",
        },
        BuiltinFn {
            name: "js_image_buffer",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Extract the raw byte buffer handle from a JavaScript image payload",
        },
        BuiltinFn {
            name: "kain_shared_buffer_from_js",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Materialize a JavaScript typed-array or buffer payload into the neutral Kain shared buffer contract",
        },
        BuiltinFn {
            name: "kain_shared_image_from_js",
            params: vec![("target", "Any")],
            return_type: "Any",
            doc: "Materialize a JavaScript image payload into the neutral Kain shared image contract",
        },
    ] {
        stdlib.functions.insert(builtin.name.to_string(), builtin);
    }
}

fn register_node_env(env: &mut Env) {
    if env
        .get_extension_state::<NodeRuntimeState>(NODE_EXTENSION_KEY)
        .is_none()
    {
        env.set_extension_state(
            NODE_EXTENSION_KEY,
            Arc::new(NodeRuntimeState {
                config: resolve_node_config().map_err(|err| err.to_string()),
                process: Mutex::new(None),
            }),
        );
    }

    env.register_native_fn("js_eval", builtin_js_eval);
    env.register_native_fn("js_eval_raw", builtin_js_eval_raw);
    env.register_native_fn("js_exec", builtin_js_exec);
    env.register_native_fn("js_import", builtin_js_import);
    env.register_native_fn("js_import_raw", builtin_js_import_raw);
    env.register_native_fn("js_call", builtin_js_call);
    env.register_native_fn("js_call_raw", builtin_js_call_raw);
    env.register_native_fn("js_call_method", builtin_js_call_method);
    env.register_native_fn("js_call_method_raw", builtin_js_call_method_raw);
    env.register_native_fn("js_getattr", builtin_js_getattr);
    env.register_native_fn("js_getattr_raw", builtin_js_getattr_raw);
    env.register_native_fn("js_setattr", builtin_js_setattr);
    env.register_native_fn("js_hasattr", builtin_js_hasattr);
    env.register_native_fn("js_buffer_info", builtin_js_buffer_info);
    env.register_native_fn("js_buffer_bytes", builtin_js_buffer_bytes);
    env.register_native_fn("js_document_info", builtin_js_document_info);
    env.register_native_fn("js_document_text", builtin_js_document_text);
    env.register_native_fn("js_image_info", builtin_js_image_info);
    env.register_native_fn("js_image_text", builtin_js_image_text);
    env.register_native_fn("js_image_bytes", builtin_js_image_bytes);
    env.register_native_fn("js_image_buffer", builtin_js_image_buffer);
    env.register_native_fn(
        "kain_shared_buffer_from_js",
        builtin_kain_shared_buffer_from_js,
    );
    env.register_native_fn(
        "kain_shared_image_from_js",
        builtin_kain_shared_image_from_js,
    );
}

fn builtin_js_eval(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let code = expect_string_arg(&args, 0, "js_eval")?;
    wire_to_value(&request_node(
        env,
        json!({ "op": "eval", "code": code, "raw": false }),
    )?)
}

fn builtin_js_eval_raw(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let code = expect_string_arg(&args, 0, "js_eval_raw")?;
    wire_to_value(&request_node(
        env,
        json!({ "op": "eval", "code": code, "raw": true }),
    )?)
}

fn builtin_js_exec(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let code = expect_string_arg(&args, 0, "js_exec")?;
    let _ = request_node(env, json!({ "op": "exec", "code": code }))?;
    Ok(Value::Unit)
}

fn builtin_js_import(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let specifier = expect_string_arg(&args, 0, "js_import")?;
    wire_to_value(&request_node(
        env,
        json!({ "op": "import", "specifier": specifier, "raw": false }),
    )?)
}

fn builtin_js_import_raw(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let specifier = expect_string_arg(&args, 0, "js_import_raw")?;
    wire_to_value(&request_node(
        env,
        json!({ "op": "import", "specifier": specifier, "raw": true }),
    )?)
}

fn builtin_js_call(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    call_like(env, &args, "js_call", "call", false)
}

fn builtin_js_call_raw(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    call_like(env, &args, "js_call_raw", "call", true)
}

fn builtin_js_call_method(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    call_like(env, &args, "js_call_method", "call_method", false)
}

fn builtin_js_call_method_raw(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    call_like(env, &args, "js_call_method_raw", "call_method", true)
}

fn builtin_js_getattr(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    getattr_like(env, &args, "js_getattr", false)
}

fn builtin_js_getattr_raw(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    getattr_like(env, &args, "js_getattr_raw", true)
}

fn builtin_js_setattr(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let target = args
        .get(0)
        .ok_or_else(|| KainError::runtime("js_setattr expects target"))?;
    let name = expect_string_arg(&args, 1, "js_setattr")?;
    let value = args
        .get(2)
        .ok_or_else(|| KainError::runtime("js_setattr expects value"))?;
    let _ = request_node(
        env,
        json!({
            "op": "setattr",
            "target": value_to_wire(target)?,
            "name": name,
            "value": value_to_wire(value)?,
        }),
    )?;
    Ok(Value::Unit)
}

fn builtin_js_hasattr(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let target = args
        .get(0)
        .ok_or_else(|| KainError::runtime("js_hasattr expects target"))?;
    let name = expect_string_arg(&args, 1, "js_hasattr")?;
    match request_node(
        env,
        json!({
            "op": "hasattr",
            "target": value_to_wire(target)?,
            "name": name,
        }),
    )? {
        JsonValue::Bool(value) => Ok(Value::Bool(value)),
        other => Err(KainError::runtime(format!(
            "Node bridge returned non-bool for js_hasattr: {other}"
        ))),
    }
}

fn builtin_js_buffer_info(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let target = args
        .first()
        .ok_or_else(|| KainError::runtime("js_buffer_info expects target"))?;
    wire_to_value(&request_node(
        env,
        json!({
            "op": "buffer_info",
            "target": value_to_wire(target)?,
        }),
    )?)
}

fn builtin_js_buffer_bytes(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let target = args
        .first()
        .ok_or_else(|| KainError::runtime("js_buffer_bytes expects target"))?;
    wire_to_value(&request_node(
        env,
        json!({
            "op": "buffer_bytes",
            "target": value_to_wire(target)?,
        }),
    )?)
}

fn builtin_js_document_info(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let target = args
        .first()
        .ok_or_else(|| KainError::runtime("js_document_info expects target"))?;
    wire_to_value(&request_node(
        env,
        json!({
            "op": "document_info",
            "target": value_to_wire(target)?,
        }),
    )?)
}

fn builtin_js_document_text(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let target = args
        .first()
        .ok_or_else(|| KainError::runtime("js_document_text expects target"))?;
    wire_to_value(&request_node(
        env,
        json!({
            "op": "document_text",
            "target": value_to_wire(target)?,
        }),
    )?)
}

fn builtin_js_image_info(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let target = args
        .first()
        .ok_or_else(|| KainError::runtime("js_image_info expects target"))?;
    wire_to_value(&request_node(
        env,
        json!({
            "op": "image_info",
            "target": value_to_wire(target)?,
        }),
    )?)
}

fn builtin_js_image_text(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let target = args
        .first()
        .ok_or_else(|| KainError::runtime("js_image_text expects target"))?;
    wire_to_value(&request_node(
        env,
        json!({
            "op": "image_text",
            "target": value_to_wire(target)?,
        }),
    )?)
}

fn builtin_js_image_bytes(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let target = args
        .first()
        .ok_or_else(|| KainError::runtime("js_image_bytes expects target"))?;
    wire_to_value(&request_node(
        env,
        json!({
            "op": "image_bytes",
            "target": value_to_wire(target)?,
        }),
    )?)
}

fn builtin_js_image_buffer(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let target = args
        .first()
        .ok_or_else(|| KainError::runtime("js_image_buffer expects target"))?;
    wire_to_value(&request_node(
        env,
        json!({
            "op": "image_buffer",
            "target": value_to_wire(target)?,
        }),
    )?)
}

fn builtin_kain_shared_buffer_from_js(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let target = args
        .first()
        .ok_or_else(|| KainError::runtime("kain_shared_buffer_from_js expects target"))?;
    let info = request_node(
        env,
        json!({
            "op": "buffer_info",
            "target": value_to_wire(target)?,
        }),
    )?;
    let bytes = request_node(
        env,
        json!({
            "op": "buffer_bytes",
            "target": value_to_wire(target)?,
        }),
    )?;
    let buffer = KainSharedBuffer::owned(
        SharedBufferMetadata {
            element_type: json_string_field(&info, "dtype").unwrap_or_else(|| "u8".to_string()),
            element_size: 1,
            shape: vec![json_i64_field(&info, "length")
                .unwrap_or_else(|| json_i64_field(&info, "byte_length").unwrap_or(0))],
            strides: vec![1],
            format: Some(json_string_field(&info, "kind").unwrap_or_else(|| "buffer".to_string())),
            mime_type: Some("application/octet-stream".to_string()),
            source_runtime: "javascript".to_string(),
            source_backend: Some("node".to_string()),
            ownership: "owned".to_string(),
            labels: vec![json_string_field(&info, "kind").unwrap_or_else(|| "buffer".to_string())],
        },
        json_u8_vec("kain_shared_buffer_from_js", &bytes)?,
    );
    Ok(shared_buffer_value(buffer))
}

fn builtin_kain_shared_image_from_js(env: &mut Env, args: Vec<Value>) -> KainResult<Value> {
    let target = args
        .first()
        .ok_or_else(|| KainError::runtime("kain_shared_image_from_js expects target"))?;
    let info = request_node(
        env,
        json!({
            "op": "image_info",
            "target": value_to_wire(target)?,
        }),
    )?;
    let bytes = request_node(
        env,
        json!({
            "op": "image_bytes",
            "target": value_to_wire(target)?,
        }),
    )?;
    let channels = json_i64_field(&info, "channels").unwrap_or(0);
    let width = json_i64_field(&info, "width").unwrap_or(0);
    let image = KainSharedImage::owned(
        SharedImageMetadata {
            representation: json_string_field(&info, "representation")
                .unwrap_or_else(|| "encoded".to_string()),
            width,
            height: json_i64_field(&info, "height").unwrap_or(0),
            channels,
            layout: json_string_field(&info, "layout").unwrap_or_default(),
            pixel_format: json_string_field(&info, "pixel_format").unwrap_or_else(|| {
                if channels == 4 {
                    "rgba8".to_string()
                } else if channels == 3 {
                    "rgb8".to_string()
                } else {
                    "encoded".to_string()
                }
            }),
            mime_type: json_string_field(&info, "mime_type")
                .unwrap_or_else(|| "application/octet-stream".to_string()),
            row_stride: json_i64_field(&info, "row_stride")
                .unwrap_or(width.saturating_mul(channels)),
            color_space: json_string_field(&info, "color_space")
                .unwrap_or_else(|| "srgb".to_string()),
            alpha_mode: json_string_field(&info, "alpha_mode").unwrap_or_else(|| {
                if channels == 4 {
                    "straight".to_string()
                } else {
                    "opaque".to_string()
                }
            }),
            source_runtime: "javascript".to_string(),
            source_backend: Some("node".to_string()),
            ownership: "owned".to_string(),
            labels: vec![json_string_field(&info, "kind").unwrap_or_else(|| "image".to_string())],
        },
        json_u8_vec("kain_shared_image_from_js", &bytes)?,
    )?;
    Ok(shared_image_value(image))
}

fn call_like(
    env: &mut Env,
    args: &[Value],
    builtin: &str,
    op: &str,
    raw: bool,
) -> KainResult<Value> {
    let target = args
        .get(0)
        .ok_or_else(|| KainError::runtime(format!("{builtin} expects target")))?;
    let payload = if op == "call_method" {
        let name = expect_string_arg(args, 1, builtin)?;
        let call_args = args
            .get(2)
            .ok_or_else(|| KainError::runtime(format!("{builtin} expects args")))?;
        json!({
            "op": op,
            "target": value_to_wire(target)?,
            "name": name,
            "args": args_value_to_wire(call_args)?,
            "raw": raw,
        })
    } else {
        let call_args = args
            .get(1)
            .ok_or_else(|| KainError::runtime(format!("{builtin} expects args")))?;
        json!({
            "op": op,
            "target": value_to_wire(target)?,
            "args": args_value_to_wire(call_args)?,
            "raw": raw,
        })
    };
    wire_to_value(&request_node(env, payload)?)
}

fn getattr_like(env: &mut Env, args: &[Value], builtin: &str, raw: bool) -> KainResult<Value> {
    let target = args
        .get(0)
        .ok_or_else(|| KainError::runtime(format!("{builtin} expects target")))?;
    let name = expect_string_arg(args, 1, builtin)?;
    wire_to_value(&request_node(
        env,
        json!({
            "op": "getattr",
            "target": value_to_wire(target)?,
            "name": name,
            "raw": raw,
        }),
    )?)
}

fn expect_string_arg(args: &[Value], index: usize, name: &str) -> KainResult<String> {
    match args.get(index) {
        Some(Value::String(value)) => Ok(value.clone()),
        Some(other) => Err(KainError::runtime(format!(
            "{name} expected argument {} to be String, got {other}",
            index + 1
        ))),
        None => Err(KainError::runtime(format!(
            "{name} expected argument {}",
            index + 1
        ))),
    }
}

fn args_value_to_wire(value: &Value) -> KainResult<JsonValue> {
    match value {
        Value::Array(items) => {
            let items = items.read().unwrap();
            let mut result = Vec::with_capacity(items.len());
            for item in items.iter() {
                result.push(value_to_wire(item)?);
            }
            Ok(JsonValue::Array(result))
        }
        _ => Ok(JsonValue::Array(vec![value_to_wire(value)?])),
    }
}

fn value_to_wire(value: &Value) -> KainResult<JsonValue> {
    match value {
        Value::Unit | Value::None => Ok(JsonValue::Null),
        Value::Bool(value) => Ok(JsonValue::Bool(*value)),
        Value::Int(value) => Ok(json!(value)),
        Value::Float(value) => Ok(json!(value)),
        Value::String(value) => Ok(JsonValue::String(value.clone())),
        Value::Array(items) => {
            let items = items.read().unwrap();
            let mut result = Vec::with_capacity(items.len());
            for item in items.iter() {
                result.push(value_to_wire(item)?);
            }
            Ok(JsonValue::Array(result))
        }
        Value::Tuple(items) => {
            let mut result = Vec::with_capacity(items.len());
            for item in items {
                result.push(value_to_wire(item)?);
            }
            Ok(JsonValue::Array(result))
        }
        Value::Struct(_, fields) => {
            let fields = fields.read().unwrap();
            let mut result = Map::new();
            for (key, value) in fields.iter() {
                result.insert(key.clone(), value_to_wire(value)?);
            }
            Ok(JsonValue::Object(result))
        }
        Value::HostObject(_, object) => {
            if let Ok(reference) = object.clone().downcast::<NodeObjectRef>() {
                Ok(json!({
                    "__kain_js_ref": reference.id,
                    "label": reference.label,
                }))
            } else if let Ok(buffer) = object.clone().downcast::<KainSharedBuffer>() {
                Ok(shared_buffer_to_wire(buffer.as_ref()))
            } else if let Ok(image) = object.clone().downcast::<KainSharedImage>() {
                Ok(shared_image_to_wire(image.as_ref()))
            } else {
                Err(KainError::runtime(
                    "JavaScript bridge cannot serialize foreign host objects".to_string(),
                ))
            }
        }
        other => Ok(JsonValue::String(other.to_string())),
    }
}

fn shared_buffer_to_wire(buffer: &KainSharedBuffer) -> JsonValue {
    json!({
        "contract": "kain.shared.buffer",
        "byte_length": buffer.byte_length(),
        "element_type": buffer.metadata.element_type,
        "element_size": buffer.metadata.element_size,
        "shape": buffer.metadata.shape,
        "strides": buffer.metadata.strides,
        "format": buffer.metadata.format,
        "mime_type": buffer.metadata.mime_type,
        "source_runtime": buffer.metadata.source_runtime,
        "source_backend": buffer.metadata.source_backend,
        "ownership": buffer.metadata.ownership,
        "labels": buffer.metadata.labels,
        "bytes": buffer.bytes(),
    })
}

fn shared_image_to_wire(image: &KainSharedImage) -> JsonValue {
    json!({
        "contract": "kain.shared.image",
        "byte_length": image.buffer.byte_length(),
        "representation": image.metadata.representation,
        "width": image.metadata.width,
        "height": image.metadata.height,
        "channels": image.metadata.channels,
        "layout": image.metadata.layout,
        "pixel_format": image.metadata.pixel_format,
        "mime_type": image.metadata.mime_type,
        "row_stride": image.metadata.row_stride,
        "color_space": image.metadata.color_space,
        "alpha_mode": image.metadata.alpha_mode,
        "source_runtime": image.metadata.source_runtime,
        "source_backend": image.metadata.source_backend,
        "ownership": image.metadata.ownership,
        "labels": image.metadata.labels,
        "bytes": image.bytes(),
    })
}

fn wire_to_value(value: &JsonValue) -> KainResult<Value> {
    match value {
        JsonValue::Null => Ok(Value::None),
        JsonValue::Bool(value) => Ok(Value::Bool(*value)),
        JsonValue::Number(number) => {
            if let Some(value) = number.as_i64() {
                Ok(Value::Int(value))
            } else if let Some(value) = number.as_f64() {
                Ok(Value::Float(value))
            } else {
                Err(KainError::runtime(format!(
                    "Unsupported numeric value from Node bridge: {number}"
                )))
            }
        }
        JsonValue::String(value) => Ok(Value::String(value.clone())),
        JsonValue::Array(items) => {
            let mut converted = Vec::with_capacity(items.len());
            for item in items {
                converted.push(wire_to_value(item)?);
            }
            Ok(Value::Array(Arc::new(RwLock::new(converted))))
        }
        JsonValue::Object(object) => {
            if let Some(id_value) = object.get("__kain_js_ref") {
                let Some(id) = id_value.as_u64() else {
                    return Err(KainError::runtime(
                        "Malformed JavaScript host object reference".to_string(),
                    ));
                };
                let label = object
                    .get("label")
                    .and_then(|value| value.as_str())
                    .unwrap_or("javascript:object")
                    .to_string();
                return Ok(Value::host_object(
                    label.clone(),
                    Arc::new(NodeObjectRef { id, label }),
                ));
            }

            let mut fields = HashMap::new();
            for (key, value) in object {
                fields.insert(key.clone(), wire_to_value(value)?);
            }
            Ok(Value::Struct(
                "JsObject".to_string(),
                Arc::new(RwLock::new(fields)),
            ))
        }
    }
}

fn json_string_field(value: &JsonValue, field: &str) -> Option<String> {
    value
        .get(field)
        .and_then(|value| value.as_str())
        .map(ToString::to_string)
}

fn json_i64_field(value: &JsonValue, field: &str) -> Option<i64> {
    value.get(field).and_then(|value| value.as_i64())
}

fn json_u8_vec(fn_name: &str, value: &JsonValue) -> KainResult<Vec<u8>> {
    let Some(items) = value.as_array() else {
        return Err(KainError::runtime(format!(
            "{fn_name}: expected byte array from Node bridge"
        )));
    };
    let mut bytes = Vec::with_capacity(items.len());
    for (index, item) in items.iter().enumerate() {
        let Some(value) = item.as_u64() else {
            return Err(KainError::runtime(format!(
                "{fn_name}: byte {index} was not an unsigned integer"
            )));
        };
        let byte = u8::try_from(value).map_err(|_| {
            KainError::runtime(format!(
                "{fn_name}: byte {index} value {value} is outside u8 range"
            ))
        })?;
        bytes.push(byte);
    }
    Ok(bytes)
}

fn request_node(env: &Env, request: JsonValue) -> KainResult<JsonValue> {
    let state = node_runtime_state(env)?;
    let config = state.config.clone().map_err(KainError::runtime)?;
    let mut slot = state.process.lock().unwrap();
    let mut last_error = None;

    for _ in 0..2 {
        if slot.is_none() {
            *slot = Some(NodeProcess::spawn(&config)?);
        }
        let result = if let Some(process) = slot.as_mut() {
            process.request(&request)
        } else {
            Err(KainError::runtime(
                "Node bridge process slot was unexpectedly empty".to_string(),
            ))
        };

        match result {
            Ok(value) => return Ok(value),
            Err(err) => {
                last_error = Some(err);
                if let Some(mut process) = slot.take() {
                    let _ = process.kill();
                }
            }
        }
    }

    Err(last_error.unwrap_or_else(|| {
        KainError::runtime("Node bridge failed without a concrete error".to_string())
    }))
}

impl NodeProcess {
    fn spawn(config: &NodeResolvedConfig) -> KainResult<Self> {
        let mut last_error = None;
        for candidate in runtime_command_candidates(&config.command) {
            let mut command = Command::new(&candidate);
            command.args(&config.args);
            command.arg(&config.bridge_script_path);
            command.current_dir(&config.cwd);
            command.stdin(Stdio::piped());
            command.stdout(Stdio::piped());
            command.stderr(Stdio::inherit());
            for (key, value) in &config.env {
                command.env(key, value);
            }

            match command.spawn() {
                Ok(mut child) => {
                    let stdin = child.stdin.take().ok_or_else(|| {
                        KainError::runtime("Node bridge stdin was not piped".to_string())
                    })?;
                    let stdout = child.stdout.take().ok_or_else(|| {
                        KainError::runtime("Node bridge stdout was not piped".to_string())
                    })?;

                    return Ok(Self {
                        child,
                        stdin,
                        stdout: BufReader::new(stdout),
                        next_id: 1,
                    });
                }
                Err(err) => last_error = Some((candidate, err)),
            }
        }

        let detail = if let Some((candidate, err)) = last_error {
            format!("last attempt '{}' failed: {err}", candidate)
        } else {
            "no runtime candidates were generated".to_string()
        };
        Err(KainError::runtime(format!(
            "Failed to spawn JavaScript runtime '{}' ({detail})",
            config.command
        )))
    }

    fn request(&mut self, request: &JsonValue) -> KainResult<JsonValue> {
        let id = self.next_id;
        self.next_id += 1;
        let mut payload = request.clone();
        let Some(object) = payload.as_object_mut() else {
            return Err(KainError::runtime(
                "Node bridge requests must be JSON objects".to_string(),
            ));
        };
        object.insert("id".to_string(), json!(id));

        let encoded = serde_json::to_string(&payload).map_err(|err| {
            KainError::runtime(format!("Failed to encode Node bridge request: {err}"))
        })?;
        writeln!(self.stdin, "{encoded}").map_err(|err| {
            KainError::runtime(format!("Failed to write Node bridge request: {err}"))
        })?;
        self.stdin.flush().map_err(|err| {
            KainError::runtime(format!("Failed to flush Node bridge request: {err}"))
        })?;

        let mut line = String::new();
        let read = self.stdout.read_line(&mut line).map_err(|err| {
            KainError::runtime(format!("Failed to read Node bridge response: {err}"))
        })?;
        if read == 0 {
            return Err(KainError::runtime(
                "Node bridge exited before sending a response".to_string(),
            ));
        }

        let response: JsonValue = serde_json::from_str(line.trim()).map_err(|err| {
            KainError::runtime(format!("Failed to decode Node bridge response: {err}"))
        })?;
        let response_id = response
            .get("id")
            .and_then(|value| value.as_u64())
            .unwrap_or(0);
        if response_id != id {
            return Err(KainError::runtime(format!(
                "Node bridge response ID mismatch: expected {id}, got {response_id}"
            )));
        }
        if response.get("ok").and_then(|value| value.as_bool()) == Some(true) {
            Ok(response.get("result").cloned().unwrap_or(JsonValue::Null))
        } else {
            Err(KainError::runtime(
                response
                    .get("error")
                    .and_then(|value| value.as_str())
                    .unwrap_or("Node bridge request failed")
                    .to_string(),
            ))
        }
    }

    fn kill(&mut self) -> KainResult<()> {
        self.child
            .kill()
            .map_err(|err| KainError::runtime(format!("Failed to stop Node bridge: {err}")))
    }
}

impl Drop for NodeProcess {
    fn drop(&mut self) {
        let _ = self.child.kill();
        let _ = self.child.wait();
    }
}

fn node_runtime_state(env: &Env) -> KainResult<Arc<NodeRuntimeState>> {
    env.get_extension_state::<NodeRuntimeState>(NODE_EXTENSION_KEY)
        .ok_or_else(|| {
            KainError::runtime("JavaScript runtime is not registered for this environment")
        })
}

fn resolve_node_config() -> KainResult<NodeResolvedConfig> {
    let current_dir = std::env::current_dir().map_err(KainError::Io)?;
    let (manifest_root, config) = load_node_manifest_config(&current_dir)?;
    let cache_root = manifest_root.join(".kain").join("cache").join("node_ffi");
    fs::create_dir_all(&cache_root).map_err(KainError::Io)?;
    let bridge_script_path = cache_root.join("kain_node_bridge.cjs");
    let existing_source = fs::read_to_string(&bridge_script_path).ok();
    if existing_source.as_deref() != Some(NODE_BRIDGE_SOURCE) {
        fs::write(&bridge_script_path, NODE_BRIDGE_SOURCE).map_err(KainError::Io)?;
    }

    let command = config
        .command
        .clone()
        .or_else(|| std::env::var("KAIN_NODE_COMMAND").ok())
        .unwrap_or_else(|| "node".to_string());
    let cwd = config
        .cwd
        .as_ref()
        .map(|path| resolve_relative_path(&manifest_root, path))
        .unwrap_or_else(|| manifest_root.clone());

    Ok(NodeResolvedConfig {
        command,
        args: config.args.clone(),
        cwd,
        env: config.env.clone(),
        bridge_script_path,
    })
}

fn runtime_command_candidates(command: &str) -> Vec<String> {
    let mut candidates = vec![command.to_string()];
    if cfg!(windows) {
        let lowered = command.to_ascii_lowercase();
        if !lowered.ends_with(".cmd") {
            candidates.push(format!("{command}.cmd"));
        }
        if !lowered.ends_with(".exe") {
            candidates.push(format!("{command}.exe"));
        }
        if !lowered.ends_with(".bat") {
            candidates.push(format!("{command}.bat"));
        }
    }
    candidates
}

fn load_node_manifest_config(start_dir: &Path) -> KainResult<(PathBuf, NodeFfiConfig)> {
    if let Some(root) = find_kain_manifest_root(start_dir) {
        for name in KAIN_MANIFEST_NAMES {
            let manifest_path = root.join(name);
            if !manifest_path.exists() {
                continue;
            }
            let source = fs::read_to_string(&manifest_path).map_err(KainError::Io)?;
            let manifest: KainNodeManifest = toml::from_str(&source).map_err(|err| {
                KainError::runtime(format!(
                    "Failed to parse Node FFI config from '{}': {err}",
                    manifest_path.display()
                ))
            })?;
            return Ok((root, manifest.node_ffi.unwrap_or_default()));
        }
        return Ok((root, NodeFfiConfig::default()));
    }
    Ok((start_dir.to_path_buf(), NodeFfiConfig::default()))
}

fn find_kain_manifest_root(start_dir: &Path) -> Option<PathBuf> {
    let mut current = Some(start_dir);
    while let Some(dir) = current {
        if KAIN_MANIFEST_NAMES
            .iter()
            .any(|name| dir.join(name).exists())
        {
            return Some(dir.to_path_buf());
        }
        current = dir.parent();
    }
    None
}

fn resolve_relative_path(root: &Path, path: &Path) -> PathBuf {
    if path.is_absolute() {
        path.to_path_buf()
    } else {
        root.join(path)
    }
}

fn source_uses_node_ffi(source: &str) -> bool {
    static USE_IMPORT_REGEX: Lazy<Regex> = Lazy::new(|| {
        Regex::new(r"(?m)^\s*use\s+std(?:::|/)javascript(?:::|/)bridge").expect("regex")
    });
    static BUILTIN_REGEX: Lazy<Regex> =
        Lazy::new(|| Regex::new(r"\bjs_[A-Za-z0-9_]+\s*\(").expect("regex"));
    USE_IMPORT_REGEX.is_match(source) || BUILTIN_REGEX.is_match(source)
}

#[cfg(test)]
mod tests {
    use super::{prepare_source_for_runtime, register};
    use kain_core::diagnostics::SpanMapper;
    use kain_core::lexer::Lexer;
    use kain_core::parser::Parser;
    use kain_core::runtime::{interpret, Value};
    use kain_core::stdlib::StdLib;
    use kain_core::types;
    use kain_core::CompileTarget;
    use std::fs;
    use std::time::{SystemTime, UNIX_EPOCH};

    #[test]
    fn javascript_builtins_extend_stdlib_metadata() {
        register();
        let stdlib = StdLib::new();
        assert!(stdlib.functions.contains_key("js_eval"));
        assert!(stdlib.functions.contains_key("js_exec"));
        assert!(stdlib.functions.contains_key("js_import"));
        assert!(stdlib.functions.contains_key("js_import_raw"));
        assert!(stdlib.functions.contains_key("js_call"));
        assert!(stdlib.functions.contains_key("js_call_method"));
        assert!(stdlib.functions.contains_key("js_getattr"));
        assert!(stdlib.functions.contains_key("js_setattr"));
        assert!(stdlib.functions.contains_key("js_hasattr"));
        assert!(stdlib.functions.contains_key("js_buffer_info"));
        assert!(stdlib.functions.contains_key("js_buffer_bytes"));
        assert!(stdlib.functions.contains_key("js_document_info"));
        assert!(stdlib.functions.contains_key("js_document_text"));
        assert!(stdlib.functions.contains_key("js_image_info"));
        assert!(stdlib.functions.contains_key("js_image_text"));
        assert!(stdlib.functions.contains_key("js_image_bytes"));
        assert!(stdlib.functions.contains_key("js_image_buffer"));
        assert!(stdlib.functions.contains_key("kain_shared_buffer_from_js"));
        assert!(stdlib.functions.contains_key("kain_shared_image_from_js"));
    }

    #[test]
    fn prepare_blocks_non_host_targets_when_js_bridge_is_used() {
        let error = prepare_source_for_runtime(
            "use std::javascript::bridge\nfn main(): js_eval(\"1 + 1\")\n",
            CompileTarget::Js,
        )
        .expect_err("js bridge should reject JS codegen target");
        assert!(error.to_string().contains("host-backed"));
    }

    #[test]
    fn javascript_exec_and_eval_persist_scope() {
        let result = interpret_source(
            r#"
use std::javascript::bridge

fn main() -> Int:
    js_exec("globalThis.kainNodeValue = 39")
    return js_eval("globalThis.kainNodeValue + 3")
"#,
        );

        match result {
            Value::Int(value) => assert_eq!(value, 42),
            other => panic!("expected Int(42), got {other:?}"),
        }
    }

    #[test]
    fn javascript_import_and_call_support_node_modules() {
        let result = interpret_source(
            r#"
use std::javascript::bridge

fn main() -> String:
    let path = js_import("node:path")
    return js_call_method(path, "basename", ["M:/Code/Kain/demo/orbit.html"])
"#,
        );

        match result {
            Value::String(value) => assert_eq!(value, "orbit.html"),
            other => panic!("expected String(\"orbit.html\"), got {other:?}"),
        }
    }

    #[test]
    fn javascript_raw_import_and_call_support_node_modules() {
        let result = interpret_source(
            r#"
use std::javascript::bridge

fn main() -> String:
    let path = js_import_raw("node:path")
    return js_call_method(path, "basename", ["M:/Code/Kain/demo/orbit.html"])
"#,
        );

        match result {
            Value::String(value) => assert_eq!(value, "orbit.html"),
            other => panic!("expected String(\"orbit.html\"), got {other:?}"),
        }
    }

    #[test]
    fn javascript_raw_import_and_call_support_local_modules() {
        let unique = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap()
            .as_nanos();
        let module_dir = std::env::temp_dir().join(format!("kain-node-test-{unique}"));
        fs::create_dir_all(&module_dir).unwrap();
        let module_path = module_dir.join("local_step.mjs");
        fs::write(
            &module_path,
            "export function run() { return 'local-raw-ok'; }\n",
        )
        .unwrap();
        let module_literal = serde_json::to_string(&module_path.display().to_string()).unwrap();
        let source = format!(
            r#"
use std::javascript::bridge

fn main() -> String:
    let module_ref = js_import_raw({module_literal})
    return js_call_method(module_ref, "run", [])
"#
        );

        let result = interpret_source(&source);
        match result {
            Value::String(value) => assert_eq!(value, "local-raw-ok"),
            other => panic!("expected String(\"local-raw-ok\"), got {other:?}"),
        }

        let _ = fs::remove_file(&module_path);
        let _ = fs::remove_dir(&module_dir);
    }

    #[test]
    fn javascript_buffer_info_and_bytes_support_typed_arrays() {
        let result = interpret_source(
            r#"
use std::javascript::bridge

fn main():
    let bytes = js_eval_raw("new Uint8Array([4, 8, 15, 16, 23, 42])")
    let info = js_buffer_info(bytes)
    let snapshot = js_buffer_bytes(bytes)
    return [info.byte_length, len(snapshot), snapshot[5]]
"#,
        );

        match result {
            Value::Array(values) => {
                let values = values.read().unwrap();
                assert_eq!(values.len(), 3);
                assert!(matches!(values[0], Value::Int(6)));
                assert!(matches!(values[1], Value::Int(6)));
                assert!(matches!(values[2], Value::Int(42)));
            }
            other => panic!("expected Array([6, 6, 42]), got {other:?}"),
        }
    }

    #[test]
    fn javascript_bridge_projects_shared_contracts_as_typed_payloads() {
        let result = interpret_source(
            r#"
use std::javascript::bridge
use std::interop::bridge

fn main():
    js_exec("globalThis.inspectSharedContracts = (image, snapshot) => [image.contract, image.bytes instanceof Uint8Array, image.bytes[2], snapshot.contract, snapshot.bytes instanceof Uint8Array, snapshot.bytes[1]]")
    let image = interop_shared_image_from_bytes([1, 2, 3, 4], 1, 1, 4, "HWC", "rgba8", "image/x-kain-raster")
    let snapshot = interop_shared_buffer_from_bytes([9, 8, 7, 6], "u8", [4], "bytes", "application/octet-stream")
    return js_call("inspectSharedContracts", [image, snapshot])
"#,
        );

        match result {
            Value::Array(values) => {
                let values = values.read().unwrap();
                assert_eq!(values.len(), 6);
                assert!(matches!(&values[0], Value::String(value) if value == "kain.shared.image"));
                assert!(matches!(values[1], Value::Bool(true)));
                assert!(matches!(values[2], Value::Int(3)));
                assert!(
                    matches!(&values[3], Value::String(value) if value == "kain.shared.buffer")
                );
                assert!(matches!(values[4], Value::Bool(true)));
                assert!(matches!(values[5], Value::Int(8)));
            }
            other => panic!("expected shared contract inspection array, got {other:?}"),
        }
    }

    #[test]
    fn javascript_document_and_image_payload_adapters_work() {
        let result = interpret_source(
            r#"
use std::javascript::bridge

fn main():
    let document = js_eval_raw("({ kind: 'document', title: 'Signal Notes', text: '<!doctype html><html><body>signal</body></html>', mime_type: 'text/html' })")
    let image = js_eval_raw("({ kind: 'canvas', width: 64, height: 32, mime_type: 'image/svg+xml', text: '<svg viewBox=\"0 0 64 32\"></svg>', bytes: new TextEncoder().encode('<svg viewBox=\"0 0 64 32\"></svg>') })")
    let doc_info = js_document_info(document)
    let image_info = js_image_info(image)
    let image_bytes = js_image_bytes(image)
    let doc_text = js_document_text(document)
    let image_text = js_image_text(image)
    return [doc_info.byte_length, image_info.width, image_info.height, len(image_bytes), len(doc_text), len(image_text), image_info.kind]
"#,
        );

        match result {
            Value::Array(values) => {
                let values = values.read().unwrap();
                assert_eq!(values.len(), 7);
                assert!(matches!(values[0], Value::Int(value) if value > 10));
                assert!(matches!(values[1], Value::Int(64)));
                assert!(matches!(values[2], Value::Int(32)));
                assert!(matches!(values[3], Value::Int(value) if value > 10));
                assert!(matches!(values[4], Value::Int(value) if value > 10));
                assert!(matches!(values[5], Value::Int(value) if value > 10));
                assert!(matches!(values[6], Value::String(ref value) if value == "canvas"));
            }
            other => panic!("expected Array payload metadata, got {other:?}"),
        }
    }

    #[test]
    fn javascript_can_materialize_shared_image_contracts() {
        let result = interpret_source(
            r#"
use std::javascript::bridge

fn main():
    let image = js_eval_raw("({ kind: 'image', width: 8, height: 4, channels: 3, layout: 'HWC', pixel_format: 'rgb8', representation: 'raster', mime_type: 'image/x-kain-raster', bytes: new Uint8Array(8 * 4 * 3).fill(17) })")
    let shared_image = kain_shared_image_from_js(image)
    let info = kain_shared_image_info(shared_image)
    return [info.source_runtime, info.width, info.height, info.channels, info.representation, info.byte_length]
"#,
        );

        match result {
            Value::Array(values) => {
                let values = values.read().unwrap();
                assert!(matches!(values[0], Value::String(ref value) if value == "javascript"));
                assert!(matches!(values[1], Value::Int(8)));
                assert!(matches!(values[2], Value::Int(4)));
                assert!(matches!(values[3], Value::Int(3)));
                assert!(matches!(values[4], Value::String(ref value) if value == "raster"));
                assert!(matches!(values[5], Value::Int(96)));
            }
            other => panic!("expected shared image metadata array, got {other:?}"),
        }
    }

    fn interpret_source(source: &str) -> Value {
        register();
        let stdlib = kain_core::stdlib::load_stdlib_for_target(CompileTarget::Interpret);
        let full_source = format!("{stdlib}\n{source}");
        let tokens = Lexer::new(&full_source).tokenize().unwrap();
        let span_mapper = SpanMapper::new(&full_source);
        let mut ast = Parser::new(&tokens, &span_mapper, "<test>")
            .parse()
            .unwrap();
        kain_core::comptime::eval_program(&mut ast).unwrap();
        let typed = types::check(&ast, &span_mapper, "<test>").unwrap();
        interpret(&typed).unwrap()
    }
}
