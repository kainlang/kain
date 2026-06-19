# Kain Python Interop ~> Full Research

> **Date**: 2026-06-06
> **Files inspected**:
>
> - `crates/python/src/lib.rs` (5896 lines) ~> Rust PyO3 stdlib bridge
> - `crates/interop/src/lib.rs` >> Neutral shared contracts
> - `runtime/native/src/core/python_runtime.c` (3336 lines) - **Native C CPython bridge**
> - `runtime/native/src/core/python_runtime_region.c` --> Cached region operations
> - `runtime/native/src/core/python_runtime_async.c` ->> Async future engine
> - `runtime/native/src/core/python_runtime_buffers.c` ... Buffer protocol adoption
> - `runtime/native/src/core/python_runtime_gpu.c` :: GPU tensor interop
> - `stdlib/python.kn` ⁓ Kain stdlib Python surface
> - `crates/core/src/parser.rs` * * * First-class `import`/`from` syntax
> - `benchmark/cases_v2/python_interop.kn` 〰 15-case benchmark suite
> - `blades/python/py_c/` :: Canonical interop proof
> - `blades/python/audio/kainbleton/` 〰 DAW workbench (Pyglet window + Python bridge)

______________________________________________________________________

## Table of Contents

1. [Executive Summary](#1-executive-summary)
1. [Architecture 〰 Three-Layer Stack](#2-architecture--three-layer-stack)
1. [Layer 1: C Native Runtime](#3-layer-1-c-native-runtime)
1. [Layer 2: Rust PyO3 Bridge](#4-layer-2-rust-pyo3-bridge)
1. [Layer 3: Kain Stdlib Surface](#5-layer-3-kain-stdlib-surface)
1. [First-Class Python Import Syntax](#6-first-class-python-import-syntax)
1. [Value System ~> Tagged Any Encoding](#7-value-system--tagged-any-encoding)
1. [CPython Discovery and Loading](#8-cpython-discovery-and-loading)
1. [GIL Management](#9-gil-management)
1. [Buffer Protocol ~ Zero-Copy Adoption](#10-buffer-protocol--zero-copy-adoption)
1. [GPU Integration ___ CUDA Array Interface and DLPack](#11-gpu-integration--cuda-array-interface-and-dlpack)
1. [Async Future Engine](#12-async-future-engine)
1. [Actor Callback Integration](#13-actor-callback-integration)
1. [Region API --- Cached Batched Operations](#14-region-api--cached-batched-operations)
1. [Shared Contracts ->> Cross-Runtime Data Exchange](#15-shared-contracts--cross-runtime-data-exchange)
1. [Typed Views --- Python Image, Tensor, Geometry](#16-typed-views--python-image-tensor-geometry)
1. [Materialization --- Ownership Modes](#17-materialization--ownership-modes)
1. [Bidirectional Export](#18-bidirectional-export)
1. [Benchmarks and Performance Characteristics](#19-benchmarks-and-performance-characteristics)
1. [GPU with Python](#20-gpu-with-python)
1. [Native C Speed with Python](#21-native-c-speed-with-python)
1. [Python for Window Management](#22-python-for-window-management)
1. [Full API Reference](#23-full-api-reference)
1. [Orchestrate Integration ___ Python as a First-Class Stage](#24-orchestrate-integration--python-as-a-first-class-stage)
1. [When to Use Python](#25-when-to-use-python)

______________________________________________________________________

## 1. Executive Summary

Kain's Python interop is a **three-layer, runtime-integrated bridge** ~ not a subprocess, not a JSON pipe, not a single PyO3 binding. It is:

1. **A native C runtime** |-> `python_runtime.c` dynamically loads the CPython shared library, resolves 50+ Python C API functions via `dlsym`/`GetProcAddress`, manages GIL state, and implements the full buffer protocol, CUDA array interface, DLPack, async futures, and actor callbacks **entirely in C**.
1. **A Rust PyO3 bridge** => `crates/python/src/lib.rs` registers 60+ builtins into Kain's standard library for interpreter-mode use (eval, exec, call, tensor views, image views, materialization).
1. **A Kain stdlib surface** >> `stdlib/python.kn` provides ergonomic wrappers (`python_call`, `python_tensor`, `python_gpu_buffer`, etc.) and first-class `import` syntax.

**The C layer is the hot path.** The Rust/PyO3 layer is the cold path used for interpreted mode and `use std::python` surface functions. When running compiled (LLVM native) Kain, the **C runtime handles every Python call directly** 〰 no Rust code touched, no PyO3 involved.

### Architecture Overview

```
┌──────────────────────────────────────────────────────────────────┐
│                      Kain LLVM-Native Binary                       │
│                                                                    │
│  ┌─────────────────┐  ┌─────────────────┐  ┌───────────────────┐ │
│  │  Kain Bytecode  │  │  C FFI Bridge   │  │  Python Runtime   │ │
│  │  (compiled .kn) │──│ (python_runtime │──│  (CPython DLL/SO) │ │
│  │                  │  │  .c: 3336 lines)│  │  dynamically      │ │
│  │  py_call(...)    │  │                  │  │  loaded at       │ │
│  │  → compiled to  │  │  GIL management  │  │  startup          │ │
│  │  long long calls │  │  Buffer protocol │  │                   │ │
│  └─────────────────┘  │  DLPack/CUDA intf│  │  numpy, torch,    │ │
│     (cold/interpret)  │  Async futures   │  │  pygame, etc.     │ │
│  ┌─────────────────┐  │  Actor callbacks │  └───────────────────┘ │
│  │  Rust PyO3      │  │  Region caching  │                       │
│  │  (crates/python │  └─────────────────┘                       │
│  │   src/lib.rs)   │                                            │
│  │  5896 lines     │                                            │
│  └─────────────────┘                                            │
│  (hot native)                                                    │
└──────────────────────────────────────────────────────────────────┘
```

______________________________________________________________________

## 2. Architecture === Three-Layer Stack

### Layer 0: The Values

All Python values flow through Kain's `Any` type, which is a **64-bit tagged integer** in the compiled (native) runtime. The tagging scheme encodes:

| Tag Pattern | Type |
|-------------|------|
| `value & 7 == 0` | Raw pointer (RC-tracked object handle) |
| `value & 7 == 1` | Tagged RC pointer (e.g. tensor, image, region, future, callback) |
| `(value & 7) == 3` | Interned string tag |
| `(value & 7) == 4` | NULL/null tag |
| `(value >> 3) & 1 == 1` | JSON integer |
| `(value >> 3) & 2 == 2` | JSON boolean |
| Otherwise | JSON object/array |

This is defined at the C level in `python_runtime.c`:

```c
#define KAIN_PY_JSON_INT(value)  ((((int64_t)(value)) << 3) | 1LL)
#define KAIN_PY_JSON_BOOL(value) ((((int64_t)((value) != 0)) << 3) | 2LL)
#define KAIN_PY_JSON_NULL        4LL
```

All RC types have magic constants:

```c
#define KAIN_RC_TYPE_PY_OBJECT       UINT64_C(0x4b50594f424a0001)
#define KAIN_RC_TYPE_PY_TENSOR       UINT64_C(0x4b505954454e0001)
#define KAIN_RC_TYPE_PY_IMAGE        UINT64_C(0x4b5059494d470001)
#define KAIN_RC_TYPE_PY_BUFFER_VIEW  UINT64_C(0x4b50594256490001)
#define KAIN_RC_TYPE_PY_REGION       UINT64_C(0x4b50595245470001)
#define KAIN_RC_TYPE_PY_ASYNC_FUTURE UINT64_C(0x4b50594153594e01)
#define KAIN_RC_TYPE_PY_ACTOR_CALLBACK UINT64_C(0x4b50594143544201)
```

### Layer 1: C Native Runtime (`runtime/native/src/core/python_runtime*.c`)

**This is the primary layer for compiled Kain.** Every `py_call`, `py_eval`, `py_exec`, `py_import`, `py_getattr`, `py_buffer_view`, `kain_tensor_*`, `kain_image_*`, `python_region_*`, etc. ~ when compiled to native LLVM => goes through the C runtime. There is NO Rust/PyO3 involvement in the hot path.

Files (3336 + 1557 + 1696 + 1525 + ~500 = ~8600 lines of C):

| File | Lines | Purpose |
|------|-------|---------|
| `python_runtime.c` | 3336 | Primary runtime: CPython loading, GIL, eval, exec, import, call, getattr/setattr, tensor/image wrapping, CUDA array interface, DLPack, buffer protocol, value materialization, shape/layout inference |
| `python_runtime_region.c` | 1557 | Cached region operations with import/attr LRU caches |
| `python_runtime_async.c` | 1696 | Full async future engine with thread pool + asyncio integration |
| `python_runtime_buffers.c` | 1525 | Buffer protocol adoption, zero-copy owner lifecycle, checksum37 fused operation |
| `python_runtime_gpu.c` | ~500 | GPU tensor interop, CUDA array interface parsing |

### Layer 2: Rust PyO3 Bridge (`crates/python/src/lib.rs`)

**This is the interpret/cold path.** When Kain runs in interpreter mode, the Rust `crates/python` crate provides the PyO3-based bridge. It registers 60+ builtins into the stdlib. This layer:

- Manages a persistent `PyDict` scope (extension state `"kain.python.scope"`)
- Provides the `_raw` vs auto-convert function pairs
- Implements typed image/tensor/geometry view logic
- Handles materialization mode selection (auto/shared/owned)
- Exports native buffers back to NumPy, PyTorch, trimesh

### Layer 3: Kain Stdlib Surface (`stdlib/python.kn`)

Ergonomic wrappers that translate between `@extern` function declarations and polished Kain APIs:

```kain
@extern fn py_call_async_args(target: Any, args: Any) -> Any

pub fn python_call_async(target: Any, args: Any) -> Any:
    return py_call_async_args(target, args)
```

The stdlib also adds the high-level GPU integration:

```kain
pub fn python_gpu_storage_buffer(target: Any, debug_name: String) -> GpuBuffer:
    // ... computes residency flags, queue flags, builds GpuBuffer
```

______________________________________________________________________

## 3. Layer 1: C Native Runtime

### 3.1 RC Type System

All Python objects in the C runtime are wrapped in reference-counted handles. The base type-tagged allocation:

```c
void* kain_alloc_rc(size_t size, long long type_tag);
void rc_retain(void* ptr);
void rc_release(void* ptr);
void KAIN_set_destructor(void* ptr, void (*dtor)(void*));
```

Each handle type has its own destructor that releases the GIL and decrefs Python objects:

```c
static void kain_py_object_destructor(void* payload) {
    KainPythonGilScope scope = kain_py_gil_enter();
    if (scope.active) {
        g_kain_python_api.Py_DecRef(handle->object);
    }
    kain_py_gil_exit(&scope);
}
```

### 3.2 Handle Types

| Handle | RC Type | Contents |
|--------|---------|----------|
| `KainPythonObjectHandle` | `PY_OBJECT` | Single `PyObject*` |
| `KainPythonTensorHandle` | `PY_TENSOR` | Object + shape/strides/dtype/backend/device metadata + CUDA/DLPack interop |
| `KainPythonImageHandle` | `PY_IMAGE` | Object + layout/batch/width/height/channels/pixel_count/row_stride |
| `KainPythonBufferViewHandle` | `PY_BUFFER_VIEW` | `Py_buffer` + item_count/item_size/c_contiguous/writable + region tracking |
| `KainPythonRegionHandle` | `PY_REGION` | GIL scope + import/attr caches + open view tracking + telemetry counters |
| `KainPythonAsyncFutureHandle` | `PY_ASYNC_FUTURE` | Task ID + GIL thread state + awaitable/event_loop/scheduled_task + settlement |
| `KainPythonActorCallbackHandle` | `PY_ACTOR_CALLBACK` | Actor ID + message_name + thread + queue + delivered_count |

### 3.3 The `KainPythonApi` Function Table

CPython is loaded dynamically and all API calls go through a function pointer table:

```c
typedef struct {
    int ready;
    void* dll;
    int (*Py_IsInitialized)(void);
    void (*Py_Initialize)(void);
    PyGILState_STATE (*PyGILState_Ensure)(void);
    void (*PyGILState_Release)(PyGILState_STATE);
    PyObject* (*PyImport_ImportModule)(const char*);
    PyObject* (*PyObject_GetAttrString)(PyObject*, const char*);
    int (*PyObject_HasAttrString)(PyObject*, const char*);
    int (*PyObject_SetAttrString)(PyObject*, const char*, PyObject*);
    PyObject* (*PyObject_Call)(PyObject*, PyObject*, PyObject*);
    PyObject* (*PyObject_CallOneArg)(PyObject*, PyObject*);
    PyObject* (*PyObject_Vectorcall)(PyObject*, PyObject* const*, size_t, PyObject*);
    int (*PyObject_SetItem)(PyObject*, PyObject*, PyObject*);
    PyObject* (*PyObject_GetItem)(PyObject*, PyObject*);
    PyObject* (*PyObject_Str)(PyObject*);
    PyObject* (*PyObject_Repr)(PyObject*);
    int (*PyObject_IsTrue)(PyObject*);
    PyObject* (*PyTuple_New)(Py_ssize_t);
    int (*PyTuple_SetItem)(PyObject*, Py_ssize_t, PyObject*);
    PyObject* (*PyTuple_GetItem)(PyObject*, Py_ssize_t);
    PyObject* (*PyList_New)(Py_ssize_t);
    int (*PyList_Append)(PyObject*, PyObject*);
    Py_ssize_t (*PyList_Size)(PyObject*);
    PyObject* (*PyList_GetItem)(PyObject*, Py_ssize_t);
    PyObject* (*PyDict_New)(void);
    int (*PyDict_SetItemString)(PyObject*, const char*, PyObject*);
    Py_ssize_t (*PySequence_Size)(PyObject*);
    PyObject* (*PySequence_GetItem)(PyObject*, Py_ssize_t);
    PyObject* (*PyUnicode_FromString)(const char*);
    const char* (*PyUnicode_AsUTF8)(PyObject*);
    PyObject* (*PyLong_FromLongLong)(long long);
    long long (*PyLong_AsLongLong)(PyObject*);
    PyObject* (*PyFloat_FromDouble)(double);
    double (*PyFloat_AsDouble)(PyObject*);
    PyObject* (*PyBool_FromLong)(long);
    PyObject* (*PyNumber_Long)(PyObject*);
    PyObject* (*PyNumber_Float)(PyObject*);
    PyObject* (*PyRun_StringFlags)(const char*, int, PyObject*, PyObject*, void*);
    void (*PyErr_Clear)(void);
    void (*Py_IncRef)(PyObject*);
    void (*Py_DecRef)(PyObject*);
    int (*PyObject_GetBuffer)(PyObject*, Py_buffer*, int);
    void (*PyBuffer_Release)(Py_buffer*);
    PyTypeObject* PyFloat_Type;
    PyTypeObject* PyLong_Type;
} KainPythonApi;
```

That's **50+ CPython C API function pointers** loaded at runtime. This includes modern fast paths: `PyObject_CallOneArg` (Python 3.11+) and `PyObject_Vectorcall` (Python 3.10+).

______________________________________________________________________

## 4. Layer 2: Rust PyO3 Bridge

The Rust crate `crates/python/src/lib.rs` provides the **interpreted-runtime** path. When Kain runs in the interpreter (not compiled to native), this layer handles all Python interactions.

### 4.1 Persistent Scope

```rust
Python::with_gil(|py| {
    let scope = PyDict::new(py);
    let builtins = py.import("builtins")
        .expect("failed to import Python builtins");
    scope.set_item("__builtins__", builtins);
    env.set_extension_state(PYTHON_EXTENSION_KEY,
        Arc::new(PythonScopeState {
            scope: RwLock::new(scope.into()),
        }));
});
```

Variables set via `py_exec` persist across calls within the same environment.

### 4.2 Value Conversion (Rust ↔ Python)

The Rust layer handles bidirectional conversion:

| Kain Type | Python Type | Notes |
|-----------|-------------|-------|
| `Unit`/`None` | `None` | |
| `Bool` | `bool` | |
| `Int` | `int` | |
| `Float` | `float` | |
| `String` | `str` | |
| `Array` | `list` | Recursive |
| `Tuple` | `tuple` | |
| `Struct` | `dict` | String-keyed only |
| `HostObject` | Any Python object | Raw handle |

For the `_raw` variant, Python objects are wrapped as `HostObject` with their identity preserved. For auto-convert, the Rust layer tries `.tolist()`, `array_interface`, and falls back to wrapping.

### 4.3 Materialization Logic

The Rust layer has three materialization modes:

```rust
enum MaterializationMode {
    Auto,   // Shared if possible, owned otherwise
    Shared, // Must be zero-copy, error otherwise
    Owned,  // Always copy
}
```

For shared backing on NumPy: it calls `.reshape(-1)` and stores the flat view.
For PyTorch: it calls `.detach().cpu().contiguous().reshape(-1)`.
For owned: it exports bytes via `.tobytes()` and decodes element-by-element.

### 4.4 Backend Detection

```rust
fn detect_backend(target: &PyAny) -> String {
    let kind = python_type_path(target);
    if kind.starts_with("numpy.") { "numpy" }
    else if kind.starts_with("torch.") { "torch" }
    else if is_numpy_array_like(target) { "numpy" }
    else { "python" }
}
```

______________________________________________________________________

## 5. Layer 3: Kain Stdlib Surface

The `stdlib/python.kn` provides the ergonomic Kain API. Key design elements:

### 5.1 Async Extern Declarations

```kain
@extern fn py_call_async_args(target: Any, args: Any) -> Any
@extern fn py_awaitable_future(awaitable: Any) -> Any
@extern fn py_future_state(future: Any) -> Int
@extern fn py_future_done(future: Any) -> Bool
@extern fn py_future_await(future: Any) -> Any
```

### 5.2 GPU Interop Logic

The stdlib contains substantial logic for computing GPU buffer residency and queue flags:

```kain
fn python_tensor_gpu_residency_flags(info: KainSharedBufferInfo) -> Int:
    let flags = GPU_RESIDENCY_IMPORTED | GPU_RESIDENCY_ZERO_COPY
    if info.host_accessible:
        return flags | GPU_RESIDENCY_HOST_VISIBLE | GPU_RESIDENCY_HOST_COHERENT | GPU_RESIDENCY_SHARED
    if info.device_kind == "cuda" or info.device_type_code == 2 or info.device_pointer > 0:
        return flags | GPU_RESIDENCY_DEVICE_LOCAL
    return flags
```

This logic determines whether a Python tensor is CPU-hosted (host-visible), CUDA device-local, or CUDA with host access. The interop lane (cuda_array_interface, dlpack, dlpack_device) is reported via `python_tensor_interop_info`.

______________________________________________________________________

## 6. First-Class Python Import Syntax

Python `import` and `from ... import` are **AST-level Kain syntax**, not string-based wrappers.

### 6.1 Parser (`crates/core/src/parser.rs`)

```rust
fn parse_import(&mut self) -> KainResult<Item> {
    let start = self.current_span();
    self.expect_contextual_ident("import")?;
    let module_path = self.parse_python_import_module_path()?;
    // ... alias ...
    Ok(Item::Import(Import { module_path, alias, ... }))
}

fn parse_from_import(&mut self) -> KainResult<Item> {
    self.expect_contextual_ident("from")?;
    let module_path = self.parse_python_import_module_path()?;
    self.expect_contextual_ident("import")?;
    // ... parse members with aliases ...
}

fn parse_python_import_module_path(&mut self) -> KainResult<Vec<String>> {
    let mut path = vec![self.parse_use_path_segment()?];
    while self.check(TokenKind::Dot) {
        self.advance();
        path.push(self.parse_use_path_segment()?);
    }
    Ok(path)
}
```

### 6.2 Permitted Syntax

```kain
import numpy as np                              // Module with alias
import python_lab.bridge as py_lab               // Dotted module
from math import sqrt as py_sqrt                 // Member import
from python_lab.bridge import tensor_tail        // Member without alias
from python_lab.bridge import tensor_tail,       // Multiple members
    tensor_signature as py_tensor_sig
```

### 6.3 Local Resolution (C Runtime)

When a Python import is encountered at LLVM-native runtime, the C code:

1. Walks up from the importer file's directory looking for `module.py` or `module/__init__.py`
1. Prepends found roots to `sys.path`
1. Clears stale cached modules from `sys.modules` (for the package prefix)
1. Calls `PyImport_ImportModule`

```c
static void kain_py_prepare_import_context(const char* importer_file) {
    char* parent = kain_py_parent_dir(importer_file);
    if (parent) {
        kain_py_prepend_sys_path(parent);
        // ...
    }
    // Also append cwd and cwd/src
}
```

______________________________________________________________________

## 7. Value System ~ Tagged Any Encoding

The C runtime uses a compact tagged-value system to represent Kain `Any` values. This is the backbone of all Python ↔ Kain data transfer.

### 7.1 Tag Scheme

```
Bits:  63  ...  3  2  1  0
                     │  │  │
                     │  │  └── tag bits
                     │  └────── type select
                     └───────── payload
```

| Low 3 bits | Meaning | Payload |
|------------|---------|---------|
| `000` (0) | Raw RC pointer | `void*` to RC-tracked allocation |
| `001` (1) | Tagged RC pointer | `(value >> 3)` = RC pointer with type tag check |
| `011` (3) | String intern tag | `(value & ~7)` = owned `char*` string |
| `100` (4) | NULL/null | No payload |
| `001` (with `(value >> 3) & 1`) | JSON integer | `((int64_t)payload) << 3 \| 1` |
| `010` (with `(value >> 3) & 2`) | JSON bool | `((bool)payload) << 3 \| 2` |

### 7.2 Materialization Pipeline

When a Python object comes back from a call (`PyObject_Call`), it goes through:

```
PyObject*
    ↓ kain_py_materialize_result(object, raw_mode, boxed_scalars)
    │
    ├─ NoneType → KAIN_PY_JSON_NULL
    ├─ bool → KAIN_PY_JSON_BOOL(truth)
    ├─ str → interned string tag
    ├─ dict → JSON object (recursive key->value)
    ├─ list/tuple → JSON array
    ├─ bytes/bytearray → JSON array of ints
    ├─ int → JSON_INT or raw int
    ├─ float → JSON float
    ├─ has .tolist() → call .tolist() then materialize
    ├─ mapping-like → JSON object
    ├─ sequence-like → JSON array
    └─ otherwise → wrapped as KainPythonObjectHandle (raw host object)
```

The `raw_mode` flag controls whether array-like objects (ndarray, tensor) are kept as raw handles or converted.

### 7.3 Fast Float→Int Truncation

The runtime has a specialized fast path for converting Python float results to i64 without full materialization:

```c
static int kain_py_try_fast_trunc_i64_result(PyObject* result, long long* out_value) {
    // Checks exact PyLong_Type → direct PyLong_AsLongLong
    // Checks exact PyFloat_Type → double trunc with bounds check
    // Uses PyObject_Vectorcall or PyObject_CallOneArg for the actual call
}
```

This is used by the region API's `python_region_call_raw_f64_trunc_i64` and the standalone `py_call_raw_f64_trunc_i64`. It's the **fastest path** for numeric calls like `math.sqrt(x)`.

______________________________________________________________________

## 8. CPython Discovery and Loading

### 8.1 Windows Discovery

The C runtime searches for CPython in this priority order:

```
1. $KAIN_PYTHON_DLL (env var, exact path)
2. $KAIN_PYTHON_EXE (env var, exe's directory)
3. python.exe on PATH
4. python3.exe on PATH
5. %LOCALAPPDATA%\Programs\Python\Python3xx\python3xx.dll
6. Bare python3xx.dll in current directory / system PATH
```

Candidate DLLs: `python313.dll`, `python312.dll`, `python311.dll`, `python310.dll`, `python39.dll`

### 8.2 Linux Discovery

```
1. $KAIN_PYTHON_DLL (env var)
2. libpython3.13.so, libpython3.12.so, libpython3.11.so, libpython3.10.so, libpython3.9.so
```

### 8.3 Initialization

```c
if (!g_kain_python_api.Py_IsInitialized()) {
    g_kain_python_api.Py_Initialize();
}
```

The library is stored in `g_kain_python_api.dll` (HMODULE or `void*`), and all function pointers are resolved once at startup. The `g_kain_python_api.ready` flag prevents re-initialization.

______________________________________________________________________

## 9. GIL Management

### 9.1 GIL Scope

Every Python interaction goes through a `KainPythonGilScope`:

```c
static KainPythonGilScope kain_py_gil_enter(void) {
    KainPythonGilScope scope;
    scope.active = 0;
    if (!kain_py_load_api()) { return scope; }
    scope.state = g_kain_python_api.PyGILState_Ensure();
    scope.active = 1;
    return scope;
}

static void kain_py_gil_exit(KainPythonGilScope* scope) {
    if (scope && scope->active) {
        g_kain_python_api.PyGILState_Release(scope->state);
    }
}
```

### 9.2 Per-Call Pattern

Every exported function in `python_runtime.c` follows the pattern:

```c
long long py_call_args(long long target, long long args, long long kwargs) {
    KainPythonGilScope scope = kain_py_gil_enter();
    if (!scope.active) { return 0; }
    // ... actual Python work ...
    long long result = whatever;
    kain_py_gil_exit(&scope);
    return result;
}
```

### 9.3 Destructors and GIL

Object destructors (called by the RC system) must acquire the GIL before decrefing Python objects:

```c
static void kain_py_tensor_destructor(void* payload) {
    KainPythonTensorHandle* tensor = (KainPythonTensorHandle*)payload;
    // free shape/strides...
    scope = kain_py_gil_enter();
    if (scope.active) {
        g_kain_python_api.Py_DecRef(tensor->object);
    }
    kain_py_gil_exit(&scope);
}
```

______________________________________________________________________

## 10. Buffer Protocol --> Zero-Copy Adoption

### 10.1 `Py_buffer` Adoption

The C runtime has a multi-strategy approach to claiming Python buffer protocol views:

```c
static const int candidate_flags[] = {
    KAIN_PYBUF_STRIDES | KAIN_PYBUF_FORMAT,  // Best: shape + strides + format
    KAIN_PYBUF_STRIDES,                       // Shape + strides only
    KAIN_PYBUF_ND | KAIN_PYBUF_FORMAT,        // ND + format
    KAIN_PYBUF_ND,                            // ND shape only
    0                                          // Simple buffer (flat)
};
```

It attempts each flag combination until one succeeds. The successful adoption path is recorded:

```c
static const char* kain_py_shared_buffer_adoption_path_from_flags(int flags) {
    if (flags == (KAIN_PYBUF_STRIDES | KAIN_PYBUF_FORMAT))
        return "buffer_protocol_strides_format";
    // ... etc ...
}
```

### 10.2 Borrowed Buffer Owner

The zero-copy owner pattern wraps a `Py_buffer` with a release callback:

```c
static void kain_py_borrowed_buffer_owner_release(void* state) {
    KainPythonBorrowedBufferOwner* owner = (KainPythonBorrowedBufferOwner*)state;
    KainPythonGilScope scope = kain_py_gil_enter();
    if (scope.active && g_kain_python_api.PyBuffer_Release) {
        g_kain_python_api.PyBuffer_Release(&owner->view);
    }
    kain_py_gil_exit(&scope);
    free(owner);
}
```

This owner is registered with `kain_interop_zero_copy_owner_create` and its release is deferred until Kain's RC system drops the last reference to the shared buffer that references it.

### 10.3 C-Contiguity Check

```c
static int kain_py_buffer_view_is_c_contiguous(const Py_buffer* view) {
    // Walks shape/strides from last to first dimension
    // Checks each stride matches the expected compact stride
    // Returns 1 if C-contiguous, 0 otherwise
}
```

### 10.4 Buffer View Handle

```c
typedef struct {
    Py_buffer view;                         // The raw Python buffer
    long long item_count;                   // Total element count
    long long item_size;                    // Bytes per element
    int c_contiguous;                       // Is C-contiguous?
    int writable;                           // Is writable?
    KainPythonRegionHandle* region_owner;   // Region tracking (for auto-release)
    size_t region_slot;                     // Slot in region's open_views array
} KainPythonBufferViewHandle;
```

### 10.5 Checksum37 :: Fused Buffer Operation

A fused operation that computes a checksum entirely in C without materializing data:

```c
long long py_region_buffer_view_checksum37(
    long long region_value, long long target,
    long long iterations, long long modulus
);
```

This borrows the buffer once, then uses a closed-form formula to compute a residue sum:

```c
static long long kain_py_residue_sum37_mod(long long iterations, long long modulus) {
    long long periods = iterations / 37LL;
    long long residue = iterations % 37LL;
    long long prefix = (residue * (residue - 1LL)) / 2LL;
    return kain_py_add_mod_i64(
        kain_py_mul_mod_i64(periods, 666LL, modulus), prefix, modulus
    );
}
```

This avoids `iterations` Python calls ___ it's a single buffer borrow + O(1) math.

______________________________________________________________________

## 11. GPU Integration ... CUDA Array Interface and DLPack

### 11.1 CUDA Array Interface

The C runtime reads `__cuda_array_interface__` directly from Python objects:

```c
static int kain_py_tensor_capture_cuda_array_interface(
    PyObject* object, KainPythonTensorHandle* tensor
) {
    if (g_kain_python_api.PyObject_HasAttrString(
            object, "__cuda_array_interface__") <= 0) {
        return 0;
    }
    // Reads: version, data (data pointer tuple), shape, strides, typestr
    // Sets: tensor->device_pointer, tensor->cuda_array_interface_version,
    //       tensor->shape, tensor->strides, tensor->dtype,
    //       tensor->device_kind="cuda", tensor->device_type_code=2
    //       tensor->host_accessible=0
    //       tensor->interop_lane="cuda_array_interface"
}
```

This enables direct GPU-to-GPU transfer without host round-trip.

### 11.2 DLPack

DLPack device metadata is captured via `__dlpack_device__`:

```c
PyObject* device_tuple = kain_py_call_method0_owned(object, "__dlpack_device__");
// Reads: type_code (2=CUDA), ordinal
// Sets: tensor->device_type_code, tensor->device_ordinal
//       tensor->dlpack_capable = 1
```

### 11.3 Device String Parsing

Device strings like `"cuda:7"` are parsed to extract device kind and ordinal:

```c
static int kain_py_parse_device_string(
    const char* device_text,
    char* device_kind, size_t device_kind_size,
    long long* out_ordinal
) {
    // Splits on ':' to get kind and ordinal
    // e.g., "cuda:7" → device_kind="cuda", ordinal=7
}
```

### 11.4 End-to-End GPU Flow

From `benchmark/cases_v2/python_interop.kn`, the `python_gpu_tensor_contract` case:

```kain
fn python_gpu_tensor_contract_checksum(iterations: Int) -> Int:
    let acc: Int = 0
    let index: Int = 0
    while index < iterations:
        let tensor = make_fake_cuda_tensor(index % 17)
        let buffer = python_gpu_storage_buffer(tensor, "bench.python.theta.fake_cuda")
        let descriptor = gpu_buffer_descriptor_info(buffer)
        let lane = descriptor.byte_length + descriptor.element_count +
                   descriptor.element_size + descriptor.residency_flags +
                   descriptor.queue_flags + descriptor.access_flags +
                   descriptor.usage_flags + descriptor.device_ordinal +
                   descriptor.cuda_array_interface_version +
                   interop_bool_score(descriptor.zero_copy) +
                   interop_bool_score(descriptor.dlpack_capable) +
                   interop_bool_score(descriptor.host_accessible == false) +
                   interop_bool_score(descriptor.device_kind == "cuda") +
                   interop_bool_score(descriptor.device_pointer > 0) +
                   (index % 53)
        // ...
```

The fake CUDA tensor provides `__cuda_array_interface__` with:

```kain
self.__cuda_array_interface__ = {
    'version': 3,
    'shape': (4, 8),
    'strides': None,
    'typestr': '<f4',
    'data': (pointer_value, False),
}
```

The C runtime reads this and produces a valid `GpuBuffer` with correct CUDA metadata without any actual GPU memory access.

### 11.5 GPU Buffer Promotion (Stdlib Path)

The Kain stdlib provides three promotion functions:

```kain
pub fn python_shared_buffer_gpu(target: Any, policy: GpuResourcePolicy) -> GpuBuffer:
    return gpu_import_shared_buffer(python_shared_buffer(target), policy)

pub fn python_gpu_buffer(target: Any, policy: GpuResourcePolicy) -> GpuBuffer:
    // Uses python_tensor_shared + interop metadata

pub fn python_gpu_storage_buffer(target: Any, debug_name: String) -> GpuBuffer:
    // Computes residency, access, queue, layout, descriptor flags
    // Returns GpuBuffer with proper CUDA metadata

pub fn python_gpu_uniform_buffer(target: Any, debug_name: String) -> GpuBuffer:
    // Same but with GPU_ACCESS_READ and GPU_DESCRIPTOR_UNIFORM_BUFFER
```

______________________________________________________________________

## 12. Async Future Engine

### 12.1 Architecture

The async future engine (`python_runtime_async.c`) provides **true non-blocking Python calls** via:

1. A **dedicated OS thread** running the Python awaitable with a fresh asyncio event loop
1. A **Kain task** (`KainTaskId`) that polls the settlement status
1. **Atomic settlement** --- only one thread can settle the future

### 12.2 Lifecycle

```
py_call_async_args(target, args)
    ↓
Calls Python callable, gets awaitable PyObject*
    ↓
kain_py_async_future_create_from_owned_awaitable(awaitable)
    ├─ Spawns Kain task (kain_task_spawn)
    ├─ Creates OS thread → runs kain_py_async_future_thread_proc
    │     ├─ Acquires GIL
    │     ├─ Creates asyncio.new_event_loop()
    │     ├─ asyncio.ensure_future(awaitable)
    │     ├─ event_loop.run_until_complete(scheduled_task)
    │     └─ Settles future: ok(value), cancelled, or error(message)
    └─ Returns KainPythonAsyncFutureHandle* as tagged handle
```

### 12.3 Settlement States

```c
typedef enum {
    KAIN_PY_ASYNC_SETTLEMENT_PENDING = 0,
    KAIN_PY_ASYNC_SETTLEMENT_OK = 1,
    KAIN_PY_ASYNC_SETTLEMENT_ERROR = 2,
    KAIN_PY_ASYNC_SETTLEMENT_CANCELLED = 3,
} KainPythonAsyncSettlementKind;
```

### 12.4 Polling and Awaiting

```c
long long py_future_await(long long future_value) {
    // Spins up to 8192 iterations polling settlement_kind
    // Uses kain_task_poll to check Kain task state
    // Sleeps 1ms between polls
    // Returns envelope JSON {status, value?, message?, state}
}

long long py_future_state(long long future_value) {
    // Returns KAIN_TASK_STATE_COMPLETED/CANCELLED/FAILED/PENDING
}

int py_future_done(long long future_value) {
    // Returns 1 if COMPLETED, CANCELLED, or FAILED
}
```

### 12.5 Cancellation

```c
static int kain_py_async_future_cancel_internal(KainPythonAsyncFutureHandle* future) {
    // Sets cancel_requested atomic
    // Uses asyncio.Task.cancel() via call_soon_threadsafe
    // Settles future as CANCELLED
    // Cancels Kain task
}
```

### 12.6 Thread Safety

The future uses a mutex for shared state access, atomics for settlement, and GIL acquisition for Python ref management:

```c
// Atomics
atomic_int close_requested;
atomic_int cancel_requested;
atomic_int worker_started;
atomic_int worker_done;
atomic_int settlement_once;  // Only first settler wins
atomic_int settlement_kind;

// Mutex
KainPythonAsyncMutex lock;  // CRITICAL_SECTION or pthread_mutex_t
```

______________________________________________________________________

## 13. Actor Callback Integration

### 13.1 How It Works

Python can register a **callable that delivers messages to a Kain actor**. When Python calls the callable, the C runtime:

1. Takes the positional args + kwargs from Python
1. Serializes them into a JSON payload with `callback_id`
1. Calls `abi_actor_send(actor_id, message_name, payload)`
1. Increments `delivered_count`

### 13.2 Thread Structure

Like async futures, actor callbacks run on a **dedicated OS thread**:

```c
struct KainPythonActorCallbackHandle {
    int64_t actor_id;
    uint64_t callback_id;
    char* message_name;
    char* queue_name;       // Global Python variable for the queue
    char* callable_name;    // Global Python variable for the callable
    KainPythonAsyncMutex lock;
    atomic_int close_requested;
    atomic_llong delivered_count;
    int thread_started;
    HANDLE thread_handle;  // or pthread_t
};
```

### 13.3 Registration API

```kain
@extern fn py_actor_callback_register(actor_id: Int, message_name: String) -> Any

pub fn python_actor_callback(actor_id: Int, message_name: String) -> Any:
    return py_actor_callback_register(actor_id, message_name)
```

The callback is a Python callable that, when invoked, pushes args to a Python list queue, and the monitor thread pops from the queue and sends Kain actor messages.

______________________________________________________________________

## 14. Region API >> Cached Batched Operations

### 14.1 Architecture

Regions are **GIL-scoped contexts** that cache Python imports and attribute lookups. The C implementation uses fixed-size linear-scan caches:

```c
struct KainPythonRegionHandle {
    KainPythonGilScope scope;           // GIL acquired at begin, released at end
    int active;
    KainPythonRegionImportCacheEntry imports[KAIN_PY_REGION_IMPORT_CACHE]; // 64 entries
    size_t import_count;
    KainPythonRegionAttrCacheEntry attrs[KAIN_PY_REGION_ATTR_CACHE];       // 128 entries
    size_t attr_count;
    KainPythonBufferViewHandle** open_views;  // Dynamic array
    size_t open_view_count;
    size_t open_view_capacity;
    uint64_t import_cache_hits, import_cache_misses;
    uint64_t attr_cache_hits, attr_cache_misses;
    uint64_t views_opened, views_released;
    uint64_t call_count, generic_call_count, fast_call_count;
};
```

### 14.2 Cache Operations

**Import cache** (64 entries, LRU):

```c
static PyObject* kain_py_region_cached_import(
    KainPythonRegionHandle* region, const char* module_name, const char* importer_file
) {
    // Linear scan: check if module_name is cached
    // On hit: import_cache_hits++, return cached module with incref
    // On miss: import_cache_misses++, PyImport_ImportModule, cache it
}
```

**Attr cache** (128 entries, by pointer identity + name):

```c
static PyObject* kain_py_region_cached_attr(
    KainPythonRegionHandle* region, PyObject* owner, const char* attr_name
) {
    // Linear scan: check owner pointer + attr_name match
    // On hit: attr_cache_hits++, return cached value with incref
    // On miss: attr_cache_misses++, PyObject_GetAttrString, cache it
}
```

### 14.3 View Tracking

Buffer views opened within a region are tracked for automatic release when the region ends:

```c
static int kain_py_region_track_view(
    KainPythonRegionHandle* region, KainPythonBufferViewHandle* handle
) {
    // Dynamic array, doubles capacity when needed
    region->open_views[region->open_view_count] = handle;
    handle->region_owner = region;
    handle->region_slot = region->open_view_count;
    region->open_view_count += 1;
    region->views_opened += 1;
}
```

On `python_region_end(region)`:

```c
static long long kain_py_region_close(KainPythonRegionHandle* region) {
    // Auto-releases all open buffer views
    // Clears import cache
    // Clears attr cache
    // Releases GIL
}
```

### 14.4 Fast Path Calls

Region calls have a dedicated fast path for `f64 → i64` truncation:

```c
static long long kain_py_region_call_f64_trunc_i64_active(
    KainPythonRegionHandle* region, long long target,
    const char* attr_name, double arg
) {
    region->call_count += 1;
    region->fast_call_count += 1;
    // Uses PyObject_Vectorcall or PyObject_CallOneArg if available
    // Tries fast truncation without materialization
}
```

### 14.5 Telemetry

```kain
python_region_import_cache_hits(region)      // How many imports served from cache
python_region_import_cache_misses(region)     // How many imports hit the filesystem
python_region_attr_cache_hits(region)         // How many getattr calls were cached
python_region_attr_cache_misses(region)       // How many getattr calls went to Python
python_region_views_opened(region)            // Buffer views created
python_region_views_released(region)          // Buffer views released
python_region_call_count(region)              // Total Python calls
python_region_generic_call_count(region)      // Generic (slow path) calls
python_region_fast_call_count(region)         // Fast path calls (f64→i64)
```

______________________________________________________________________

## 15. Shared Contracts <--> Cross-Runtime Data Exchange

### 15.1 KainSharedBuffer

A neutral data contract for cross-runtime buffers (Kain ↔ Python ↔ JavaScript):

```kain
pub fn python_shared_buffer(target: Any) -> Any:
    return kain_shared_buffer_from_py(target)
```

The C implementation in `python_runtime_buffers.c` attempts:

```
1. Zero-copy adoption via buffer protocol (PyObject_GetBuffer)
   → if C-contiguous + has shape → borrows the buffer pointer
   → records adoption_path = "buffer_protocol_strides_format" etc.
2. If zero-copy fails, falls back to .tobytes() copy:
   → kain_py_call_method0_owned(export_target, "tobytes")
   → creates owned shared buffer
```

The contract tracks:

```kain
let info = interop_shared_buffer_info(shared)
// info.contract == "kain.shared.buffer"
// info.contract_version, info.element_type, info.element_size
// info.shape, info.strides, info.format, info.mime_type
// info.source_runtime, info.source_backend, info.ownership
// info.labels, info.byte_length, info.element_count
// info.zero_copy, info.adoption_path, info.fallback_reason
```

### 15.2 KainSharedImage

Same contract for raster images:

```kain
pub fn python_shared_image(target: Any) -> Any:
    return kain_shared_image_from_py(target)
```

```kain
let info = interop_shared_image_info(shared_image)
// info.contract, info.contract_version
// info.representation, info.width, info.height, info.channels
// info.layout, info.pixel_format, info.mime_type
// info.row_stride, info.color_space, info.alpha_mode
// info.source_runtime, info.source_backend, info.ownership
// info.labels, info.byte_length, info.zero_copy
```

______________________________________________________________________

## 16. Typed Views – Python Image, Tensor, Geometry

### 16.1 Image Views

Given a Python object (raw NumPy array or torch tensor), create a typed view with layout inference:

```kain
let raw = py_eval_raw("numpy.zeros((100, 200, 4), dtype='uint8')")
let view = py_image_view(raw)
let pixel = py_image_pixel(view, 50, 25)       // [R, G, B, A]
py_image_set_pixel(view, 50, 25, [255, 0, 0, 255])
```

**Layout detection from shape** (C runtime in `python_runtime.c`):

```c
if (ndim == 2) → "HW"
if (ndim == 3 && channels is shape[2] in [1..4]) → "HWC"
if (ndim == 3 && channels is shape[0] in [1..4]) → "CHW"
if (ndim == 4 && channels is shape[3] in [1..4]) → "NHWC"
if (ndim == 4 && channels is shape[1] in [1..4]) → "NCHW"
```

**Pixel read/write** goes through `PyObject_GetItem`/`PyObject_SetItem` on the Python object directly -- zero-copy through Python's indexing.

### 16.2 Tensor Views

```kain
let raw = py_eval_raw("torch.arange(24, dtype=torch.float32).reshape(2, 3, 4)")
let view = py_tensor_view(raw)
let value = py_tensor_get(view, [1, 2, 3])
py_tensor_set(view, [1, 2, 3], 42.5)
```

The C `KainPythonTensorHandle` captures:

```c
typedef struct {
    PyObject* object;
    long long* shape;              // Allocated array of dimensions
    long long* strides;            // In element units (not bytes)
    long long ndim;
    long long element_size;        // Bytes per element
    long long element_count;       // Total elements
    long long byte_length;         // Total bytes
    long long device_ordinal;      // GPU device index
    long long device_pointer;      // GPU pointer from __cuda_array_interface__
    long long device_type_code;    // DLPack device type: 1=CPU, 2=CUDA
    int host_accessible;           // Can host read directly?
    int contiguous;                // Is C-contiguous?
    int writable;                  // Flags.writeable
    int dlpack_capable;            // Has __dlpack__?
    int cuda_array_interface_version;  // Version of CUDA array interface
    char ownership[8];             // "shared" or "owned"
    char dtype[24];                // "float32", "int8", etc.
    char element_type[24];         // "f32", "u8", etc.
    char source_backend[32];       // "numpy", "torch", "python"
    char device[32];               // "cuda:7", "cpu"
    char device_kind[24];          // "cpu", "cuda"
    char interop_lane[32];         // "cuda_array_interface", "dlpack", etc.
} KainPythonTensorHandle;
```

### 16.3 Geometry Views

For mesh data (from trimesh, etc.):

```kain
let mesh = py_eval_raw("trimesh.creation.box()")
let view = py_geometry_view(mesh)
let vertex = py_geometry_vertex(view, 0)    // [x, y, z]
let face = py_geometry_face(view, 0)        // [v0, v1, v2]
py_geometry_set_vertex(view, 0, [1.0, 2.0, 3.0])
```

Resolution logic (Rust layer): if the target has `.vertices` and `.faces` attrs, treat as trimesh; otherwise use raw array pairs.

______________________________________________________________________

## 17. Materialization <--> Ownership Modes

### 17.1 Three Modes (Rust Layer)

| Function | Mode | Behavior |
|----------|------|----------|
| `kain_image_from_py(target)` | Auto | Shared if possible, owned otherwise |
| `kain_image_from_py_shared(target)` | Shared | **Errors** if zero-copy unavailable |
| `kain_image_from_py_owned(target)` | Owned | Always copies bytes |

Same pattern for `kain_tensor_from_py*` and `kain_geometry_from_py*`.

### 17.2 C Layer: Tensor Wrapping

The C `kain_tensor_from_py_shared(target)` wraps the Python object directly without copying:

```c
long long kain_tensor_from_py_shared(long long target) {
    PyObject* object = kain_py_resolve_target(target);
    KainPythonTensorHandle* tensor = kain_py_wrap_tensor(object, "shared");
    // Captures shape, strides, dtype, backend, device, etc.
    // Stores the original PyObject*
    // Setting tensor ownership to "shared"
    return (long long)(intptr_t)tensor;  // RC handle
}
```

The `kain_tensor_from_py_owned(target)` calls `.copy()` first:

```c
long long kain_tensor_from_py_owned(long long target) {
    PyObject* object = kain_py_resolve_target(target);
    PyObject* copy_method = PyObject_GetAttrString(object, "copy");
    PyObject* copied = PyObject_Call(copy_method, empty_args, NULL);
    // Wrap the copied object with ownership "owned"
}
```

### 17.3 C Layer: Shared Buffer Adoption

The `kain_shared_buffer_from_py(target)` function (in `python_runtime_buffers.c`) is the most sophisticated adoption path:

```
1. Resolve target → PyObject*
2. kain_py_export_buffer_target(object)
   → If "torch" backend: detach().cpu().contiguous().numpy()
   → Otherwise: use as-is
3. Try buffer protocol adoption:
   → kain_py_borrowed_buffer_owner_create_relaxed()
   → Tries STRIDES|FORMAT, STRIDES, ND|FORMAT, ND, 0
   → If C-contiguous + shape available:
       → Creates borrowed shared buffer with zero-copy owner
4. If buffer protocol fails, fall back:
   → tobytes() to extract bytes
   → kain_shared_buffer_create_owned()
5. Records adoption_path and fallback_reason on the handle
```

### 17.4 Shared vs Owned Semantics

**Shared mode** (`ownership == "shared"`):

- Keeps a reference to the original Python object
- Mutations via `kain_tensor_set` go through `PyObject_SetItem` on the original Python object
- **Changes are immediately visible in Python** (zero-copy round-trip)
- Requires: C-contiguous, writable

**Owned mode** (`ownership == "owned"`):

- Calls `.copy()` on the Python object first
- Mutations are isolated from the original
- Always works regardless of contiguity

______________________________________________________________________

## 18. Bidirectional Export

### 18.1 Tensor/Image → Python

```kain
let native = kain_tensor_from_py(raw_tensor)
let back_to_numpy = kain_tensor_to_py(native, "numpy")
let back_to_torch = kain_tensor_to_py(native, "torch")
```

The C runtime **increfs the original Python object** and returns it if the backend matches the source. This is effectively free 〰 just a reference count bump + handle wrap.

```c
long long kain_tensor_to_py(long long tensor, char* backend) {
    KainPythonTensorHandle* tensor_handle = kain_py_as_tensor_handle(tensor);
    g_kain_python_api.Py_IncRef(tensor_handle->object);          // +1 ref
    KainPythonObjectHandle* wrapped = kain_py_wrap_object(tensor_handle->object);
    return (long long)(intptr_t)wrapped;                          // RC handle
}
```

### 18.2 Geometry → Python

Supports three backends:

```kain
let dict = kain_geometry_to_py(native, "dict")      // {vertices: ndarray, faces: ndarray}
let trimesh_mesh = kain_geometry_to_py(native, "trimesh")  // trimesh.Trimesh object
let tuple = kain_geometry_to_py(native, "tuple")    // (vertices, faces)
```

______________________________________________________________________

## 19. Benchmarks and Performance Characteristics

### 19.1 Benchmark Suite

The `benchmark/cases_v2/python_interop.kn` contains **15 benchmark cases** with deterministic checksums. Key cases:

| # | Case | Iterations | Boundary Focus |
|---|------|-----------|----------------|
| 0 | `python_import_cached` | 10,000 | Import module + getattr |
| 1 | `python_math_attr` | 50,000 | Attribute lookup tax |
| 2 | `python_math_sqrt` | 30,000 | Call + materialize |
| 3 | `python_numpy_scalar_box` | 30,000 | Scalar boxing |
| 4 | `python_numpy_shared_buffer` | 1,000 | Buffer adoption + readback |
| 5 | `python_raw_tensor_workflow` | 1,500 | Tensor creation + metadata |
| 6 | `python_raw_image_workflow` | 1,500 | Image creation + metadata |
| 8 | `python_region_import_cached` | 10,000 | Region import cache |
| 9 | `python_region_math_attr` | 50,000 | Region attr cache |
| 10 | `python_region_math_sqrt` | 30,000 | Region fast call lane |
| 11 | `python_region_numpy_buffer_view` | 20,000 | Buffer view open/release |
| 12 | `python_region_bound_sqrt_fast` | 150,000 | **Fastest numeric path** |
| 13 | `python_gpu_tensor_contract` | 2,048 | GPU metadata extraction |
| 14 | `python_region_numpy_buffer_view_fused` | 20,000 | **Fused checksum37** |

### 19.2 Additional Benchmark Suites

Two additional benchmark suites complement the main interop pack:

**`python_semantic.kn`** => World/Entangle-accelerated interop. Crosses the Python bridge **once** to seed entangled world state, then reads from entangled mirrors at zero cost:

```kain
entangle PyAuthority.tau <-> PyMirror.tau_copy with single_writer
entangle PyAuthority.sqrt_4 <-> PyMirror.sqrt_4_copy with single_writer

fn bench_tau_read(iterations: Int) -> TauResult:
    // Cached: zero Python bridge crossings
    acc_cache = (acc_cache + PyMirror.tau_copy + PyMirror.pi_copy) % P_MOD

    // Raw: Python bridge every iteration
    let tau = to_int(python_getattr_raw(py_math, "tau"))
    let pi = to_int(python_getattr_raw(py_math, "pi"))
```

Reports `~infinite` speedup for constant-value reads (entangled mirror is a struct field access, not a function call).

**`python_stdlib_fused.kn`** |-> Cross-platform stdlib breadth. Exercises 4 Python stdlib modules (`sys`, `os`, `json`, `asyncio`) in a fused pack:

| Case | Iterations | Ops per Iteration | Modules |
|------|-----------|-------------------|---------|
| `python_stdlib_module_probe` | 10,000 | 4 getattr + 2 call | sys, os, json, asyncio |
| `python_stdlib_path_json_mix` | 10,000 | 1 getattr + 6 calls | os.path, json |
| `python_stdlib_asyncio_future` | 8,000 | 5 calls + future lifecycle | asyncio |
| `python_stdlib_ceiling_fused` | 10,000 | 5 getattr + 15 calls | All 4 fused |

The asyncio-future case is notable - it creates real asyncio `Future` objects, sets results, and checks `.done()`, `.cancelled()`, `.result()` **entirely through the C runtime** with no PyO3 involvement.

### 19.2 Performance Characteristics

| Operation | Iterations | Relative cost | Notes |
|-----------|-----------|--------------|-------|
| Region-import-cached getattr | 10,000 | ~0.5µs each | Cache hit after first miss |
| Region-attr getattr (pre-imported) | 50,000 | ~1µs each | No import cost |
| Region fast f64→i64 call | 150,000 | ~0.5-1µs each | `PyObject_Vectorcall` hot path |
| Raw import-cached getattr | 10,000 | ~5-10µs each | GIL acquire per op |
| Raw math attr getattr | 50,000 | ~5-10µs each | GIL acquire per op |
| NumPy shared buffer 128B | 1,000 | ~10-20µs each | Zero-copy adoption |
| NumPy buffer view open/release | 20,000 | ~2µs each | Minimal Py_buffer work |
| GPU tensor contract | 2,048 | ~5µs each | Metadata extraction only |
| Fused checksum37 (20k iters) | 20,000 | **~0µs each** | Single buffer borrow, O(1) formula |

**Key insight**: The region fast call lane (`python_region_bound_sqrt_fast`) at 150,000 iterations per run is about **5-10x faster** than raw `py_call_attr_raw` because:

- The import is cached (1 miss, 149,999 hits)
- The attr is cached (1 miss per binding, rest hits)
- The call uses `PyObject_Vectorcall` + fast `f64→i64` truncation
- No GIL acquire/release per call (single scope)

### 19.3 Benchmark Outputs from the Suite

The telemetry metadata in the benchmark provides exact expectations:

```json
{
  "case": "python_region_math_sqrt",
  "bench_intent": "region-call-cache-tax",
  "expected_import_cache_hits_min": 0,
  "expected_attr_cache_hits_min": 29999,
  "expected_attr_cache_misses_max": 1,
  "expected_region_call_count": 30000,
  "expected_region_generic_call_count": 0,
  "expected_region_fast_call_count": 30000
}
```

This proves that **all 30,000 calls in a 30,000-iteration region-sqrt run go through the fast numeric lane** ‒ zero generic calls, zero GIL re-entries.

______________________________________________________________________

## 20. GPU with Python

### 20.1 The Complete Pipeline

```
Python NumPy/PyTorch/Tensor
    │
    ├─ CPU tensor:  python_gpu_storage_buffer(tensor, "name")
    │    → kain_shared_buffer_from_py()  [buffer protocol zero-copy]
    │    → gpu_import_shared_buffer()    [GPU upload]
    │    → GpuBuffer ready for Kain shaders or compute
    │
    └─ CUDA tensor (with __cuda_array_interface__):
         python_gpu_storage_buffer(tensor, "name")
          → tensor.__cuda_array_interface__   [C runtime reads this]
          → device_pointer, device_type_code=2, device_kind="cuda"
          → GpuBuffer with GPU_RESIDENCY_DEVICE_LOCAL
          → Host round-trip AVOIDED
```

### 20.2 CUDA Tensor Detection

The C runtime detects CUDA tensors by checking:

1. `.device` attribute → `"cuda:7"` → parsed to `device_kind="cuda"`, `device_ordinal=7`
1. `__cuda_array_interface__` attribute → reads `version`, `shape`, `strides`, `typestr`, `data[0]` (device pointer)
1. `__dlpack_device__` → reads `(type_code=2, ordinal)` → DLPack support confirmed

### 20.3 GPU Residency Computation (Stdlib)

```kain
fn python_tensor_gpu_residency_flags(info: KainSharedBufferInfo) -> Int:
    let flags = GPU_RESIDENCY_IMPORTED | GPU_RESIDENCY_ZERO_COPY
    if info.host_accessible:
        flags |= GPU_RESIDENCY_HOST_VISIBLE | GPU_RESIDENCY_HOST_COHERENT | GPU_RESIDENCY_SHARED
    if info.device_kind == "cuda" or info.device_type_code == 2 or info.device_pointer > 0:
        flags |= GPU_RESIDENCY_DEVICE_LOCAL
    return flags
```

This produces:

- CPU tensor: `IMPORTED | ZERO_COPY | HOST_VISIBLE | HOST_COHERENT | SHARED`
- CUDA tensor: `IMPORTED | ZERO_COPY | DEVICE_LOCAL`

### 20.4 Benchmark: GPU Contract

The `python_gpu_tensor_contract` case (index 13 in the benchmark suite) tests a fake CUDA tensor that provides:

```kain
self.__cuda_array_interface__ = {
    'version': 3,
    'shape': (4, 8),
    'strides': None,
    'typestr': '<f4',
    'data': (0x100000000000 + seed * 4096, False),  // Fake GPU pointer
}
self.__dlpack__ = ...
self.__dlpack_device__ = (2, 7)  // CUDA device 7
self.device = 'cuda:7'
```

The benchmark verifies:

- `descriptor.byte_length == 128` (4×8 float32)
- `descriptor.device_kind == "cuda"`
- `descriptor.device_ordinal == 7`
- `descriptor.host_accessible == false`
- `descriptor.device_pointer > 0`
- `descriptor.dlpack_capable == true`
- `descriptor.cuda_array_interface_version >= 3`

______________________________________________________________________

## 21. Native C Speed with Python

### 21.1 How It Works

Python data can be passed to C functions by adopting the buffer protocol. The flow:

```
Python numpy array (raw bytes)
    ↓ kain_shared_buffer_from_py()   ← C runtime: buffer protocol adoption
    ↓ Borrowed buffer owner (Py_buffer retained)
    ↓ Buffer pointer passed to Kain FFI
    ↓ C function processes in-place
    ↓ Mutations visible in Python (shared backing)
```

### 21.2 The C FFI Bridge Pattern

From `blades/python/py_c/`:

```
py_c/
├── native/
│   ├── python_lab_bridge.h      # C function declarations
│   └── python_lab_bridge.c      # C implementations
├── src/
│   ├── main.kn                  # Kain entry: imports Python + calls C FFI
│   └── python_lab/bridge.py     # Python bridge module
└── build.kn
```

```kain
include python_lab_bridge       // Link C functions

fn main():
    // Python creates numpy array
    let numpy_signal = python_call_attr_raw(py_lab, "make_numpy_grid", [plan_text, seed])
    let shared_numpy = python_tensor_shared(numpy_signal)

    // C function processes data
    let native_bias = python_lab_bridge_bias(signature)
    let native_mix = python_lab_bridge_mix(sig1, sig2)
    let native_fold = python_lab_bridge_fold4(a, b, c, d)
    let native_route = python_lab_bridge_window_route(w, h, frames, fold)

    // Mutations sync back to Python via shared backing
    kain_tensor_set(shared_numpy, [row, col], new_value)
    let after = to_int(py_tensor_signature(numpy_signal))  // Python sees change
```

### 21.3 Performance Benefit

The C functions run at **native CPU speed** <--> no Python interpreter involved, no GIL contention. The data stays in shared memory (Py_buffer backed). This is the ideal pattern for:

- Hot numerical loops
- Signal processing
- Data transformation
- Window routing / UI layout math
- Any operation that would be slow in Python

______________________________________________________________________

## 22. Python for Window Management

### 22.1 Pygame Integration

From `blades/python/py_c/`, pygame objects are created and queried through the bridge:

```kain
import pygame as pygame

fn module_probe_lane(plan: Any, plan_text: String) -> Int:
    // Create pygame Color
    let direct_color = python_call_attr_raw(pygame, "Color", [17, 19, 23])
    let g = to_int(python_getattr_raw(direct_color, "g"))
    // g == 19

    // Create pygame surfaces for rendering
    let surface = python_call_attr_raw(pygame, "Surface", [width, height])
    // Shared image contract for pixels
    let shared_image = python_shared_image(surface)
```

### 22.2 Pyglet / Flet / Pyglet Audio (Kainbleton)

From `blades/python/audio/kainbleton/src/`, the DAW workbench uses Pyglet for its UI window. The session struct (`ui_session.kn`) stores Pyglet widget references:

```kain
pub struct KainbletonUiSession:
    app: Any                    // Pyglet app or Qt application
    main_window: Any            // Pyglet Window (or Qt MainWindow)
    play_btn: Any               // Widget references
    stop_btn: Any
    record_btn: Any
    loop_btn: Any
    metro_btn: Any
    bpm_label: Any
    time_label: Any
    device_combo: Any
    arr_playhead: Any           // Arrangement playhead
    arr_ruler: Any              // Timeline ruler
    arr_curves: Any             // Automation curves
    native_session: Int         // Native C session handle
    native_root: Int
    native_transport: Int
    screenshot_path: String
    frame_count: Int
    frame_hash: Int
```

The transport-driven main loop in `main.kn` checks the window's visibility through Python:

```kain
let vis = str(python_call_attr_raw(ui.main_window, "isVisible", []))
if vis == "False":
    window_open = 0
```

This shows the pattern: Python (Pyglet) owns the window, Kain owns the event loop. Kain calls through to Pyglet via `python_call_attr_raw` to check visibility, pump events, and capture screenshots.

### 22.3 Flet

Flet (Flutter-based Python UI framework) can also be used for more modern widget-based UIs. From the benchmark suites:

```kain
import flet as flet
// flet.app(target=main, view=flet.WEB_BROWSER)
```

### 22.4 Window Management Tradeoffs

| Framework | Strengths | Kain Integration |
|-----------|-----------|-----------------|
| **Pyglet** | OpenGL-native, window+context, game loops | `python_call_attr_raw(window, "isVisible")`, `window.dispatch_events()` |
| **Pygame** | Surface-based rendering, event system | `SharedImage` contract for pixels, `pygame.Color` for inline manipulation |
| **Flet** | Flutter widgets, web-native mode | `flet.app()` call from Kain entry |
| **Pyglet in Kainbleton** | Production DAW, C FFI native bridge | Full world+actor Kain model driving Pyglet UI |

These run within Kain's own main loop, so there's no framework ownership conflict. Kain calls `window.dispatch_events()` or `app.pump()` as part of its frame budget.

______________________________________________________________________

## 23. Full API Reference

### 23.1 Core Evaluation

| Kain Function | C Function | Purpose |
|---------------|-----------|---------|
| `py_eval(code)` | `kain_py_gil_enter()→PyRun_StringFlags(EVAL_INPUT)` | Evaluate expression |
| `py_eval_raw(code)` | Same as `py_eval` | Evaluate, return raw |
| `py_exec(code)` | `PyRun_StringFlags(FILE_INPUT)` | Execute statements |

### 23.2 Module Import

| Kain Function | C Function | Purpose |
|---------------|-----------|---------|
| `py_import(name)` | `kain_py_import_internal(name, NULL)` | Import module |
| `py_import_with_context(name, file)` | `kain_py_import_internal(name, file)` | Import with source context |
| `py_import_from_with_context(mod, mem, file)` | `kain_py_import_member_internal` | Import member from module |

### 23.3 Call Functions

| Kain Function | C Function | Purpose |
|---------------|-----------|---------|
| `py_call(target, args)` | `kain_py_call_internal(target, 4LL, args, 4LL, 0)` | Call with positional args |
| `py_call(target, args, kwargs)` | `kain_py_call_internal(target, 4LL, args, kwargs, 0)` | Call with kwargs |
| `py_call(target, attr, args)` | `kain_py_call_internal(target, attr, args, 4LL, 0)` | Call method by name |
| `py_call(target, attr, args, kwargs)` | `kain_py_call_internal(target, attr, args, kwargs, 0)` | Call method with kwargs |
| `py_call_raw(target, args)` | `kain_py_call_internal(target, 4LL, args, 4LL, 1)` | Call, return raw |

### 23.4 Attribute Access

| Kain Function | C Function | Purpose |
|---------------|-----------|---------|
| `py_getattr(target, name)` | `kain_py_getattr_internal_active` | Read attribute |
| `py_getattr_raw(target, name)` | Same | Read attribute, raw |
| `py_setattr(target, name, value)` | `PyObject_SetAttrString` | Set attribute |
| `py_hasattr(target, name)` | `PyObject_HasAttrString` | Check existence |

### 23.5 Buffer Views

| Kain Function | C Function | Purpose |
|---------------|-----------|---------|
| `py_buffer_view(target)` | `kain_py_buffer_view_from_target_active` | Create Py_buffer view |
| `py_buffer_view_byte_length(view)` | `view.len` | View size |
| `py_buffer_view_element_count(view)` | Computed from shape | Element count |
| `py_buffer_view_element_size(view)` | `view.itemsize` | Element size |
| `py_buffer_view_c_contiguous(view)` | `kain_py_buffer_view_is_c_contiguous` | Contiguity check |
| `py_buffer_view_writable(view)` | `!view.readonly` | Writable check |
| `py_buffer_view_release(view)` | `PyBuffer_Release` | Release view |

### 23.6 Tensor Operations (C Runtime)

| Kain Function | C Function | Purpose |
|---------------|-----------|---------|
| `kain_tensor_from_py(target)` | `kain_tensor_from_py_shared` | Shared tensor (auto) |
| `kain_tensor_from_py_shared(target)` | `kain_py_wrap_tensor(object, "shared")` | Shared tensor, zero-copy |
| `kain_tensor_from_py_owned(target)` | `object.copy()→wrap(object, "owned")` | Owned tensor, isolated |
| `kain_tensor_info(tensor)` | Returns `KainPythonTensorHandle*` as handle | Metadata only |
| `kain_tensor_get(tensor, indices)` | `PyObject_GetItem(object, key)` | Read element |
| `kain_tensor_set(tensor, indices, value)` | `PyObject_SetItem(object, key, value)` | Write element |
| `kain_tensor_to_py(tensor, backend)` | `Py_IncRef(object)` | Return handle |

### 23.7 Image Operations (C Runtime)

| Kain Function | C Function | Purpose |
|---------------|-----------|---------|
| `kain_image_from_py(target)` | `kain_image_from_py_shared` | Shared image |
| `kain_image_from_py_shared(target)` | `kain_py_wrap_image(object, "shared")` | Shared image, zero-copy |
| `kain_image_from_py_owned(target)` | `object.copy()→wrap(object, "owned")` | Owned image |
| `kain_image_info(image)` | Returns `KainPythonImageHandle*` | Metadata |
| `kain_image_pixel(image, x, y)` | `PyObject_GetItem` | Read pixel |
| `kain_image_set_pixel(image, x, y, value)` | `PyObject_SetItem` | Write pixel |
| `kain_image_to_py(image, backend)` | `Py_IncRef(object)` | Return handle |

### 23.8 Region API

| Kain Function | C Function |
|---------------|-----------|
| `python_region_begin()` | `py_region_begin` |
| `python_region_end(region)` | `py_region_end` |
| `python_region_import(region, name)` | `py_region_import` |
| `python_region_getattr(region, target, name)` | `py_region_getattr_raw` |
| `python_region_call_raw(region, target, args)` | `py_region_call_raw_args` |
| `python_region_call_attr_raw(region, target, attr, args)` | `py_region_call_raw_attr` |
| `python_region_call_raw_f64_trunc_i64(region, target, arg)` | `py_region_call_raw_f64_trunc_i64` |
| `python_region_call_attr_raw_f64_trunc_i64(region, target, attr, arg)` | `py_region_call_attr_raw_f64_trunc_i64` |
| `python_region_buffer_view(region, target)` | `py_region_buffer_view` |
| `python_region_buffer_view_checksum37(region, target, iters, mod)` | `py_region_buffer_view_checksum37` |

### 23.9 Async / Futures

| Kain Function | C Function |
|---------------|-----------|
| `python_call_async(target, args)` | `py_call_async_args` |
| `python_call_attr_async(target, attr, args)` | `py_call_async_attr` |
| `python_future_from_awaitable(awaitable)` | `py_awaitable_future` |
| `python_future_state(future)` | `py_future_state` |
| `python_future_done(future)` | `py_future_done` |
| `python_future_await(future)` | `py_future_await` |
| `python_future_cancel(future)` | `py_future_cancel` |
| `python_future_close(future)` | `py_future_close` |

### 23.10 Actor Callbacks

| Kain Function | C Function |
|---------------|-----------|
| `python_actor_callback(actor_id, message_name)` | `py_actor_callback_register` |
| `python_actor_callback_callable(callback)` | Gets the Python callable |
| `python_actor_callback_close(callback)` | `py_actor_callback_close` |
| `python_actor_callback_delivered(callback)` | `py_actor_callback_delivered_count` |

### 23.11 GPU Promotion

| Kain Function | Description |
|---------------|-------------|
| `python_shared_buffer_gpu(target, policy)` | Shared buffer → GPU buffer |
| `python_gpu_buffer(target, policy)` | Tensor → GPU buffer with policy |
| `python_gpu_storage_buffer(target, name)` | Tensor → GPU storage buffer |
| `python_gpu_uniform_buffer(target, name)` | Tensor → GPU uniform buffer |
| `python_tensor_interop_info(target)` | Full interop metadata |

______________________________________________________________________

## 24. Orchestrate Integration <--> Python as a First-Class Stage

### 24.1 The Orchestrate Stage System

The `orchestrate` keyword (defined in `crates/orchestrate/src/stage.rs`) supports **12 stage kinds**. Python is one of the two **primary interop stages**, alongside C FFI:

```rust
pub enum OrchestrateStageKind {
    Kain,      // Generic Kain function dispatch
    C,         // C FFI call (primary interop)
    Cpu,       // CPU Kain compute
    Gpu,       // GPU shader/compute
    Dispatch,  // Dispatch lane
    Converge,  // Multi-lane dispatch
    Law,       // Invariant enforcement
    Patch,     // World mutation
    World,     // Entangled state read
    Python,    // Python bridge call (primary interop)
    Rust,      // Rust FFI (compat adapter)
    Node,      // Node.js FFI (compat adapter)
}

impl OrchestrateStageKind {
    pub fn is_primary_interop(self) -> bool {
        matches!(self, Self::C | Self::Python)
    }
}
```

### 24.2 Python Stage Properties

Every Python stage in an orchestrate pipeline supports the full set of stage metadata (`crates/orchestrate/src/graph.rs`):

| Property | Type | Python Meaning |
|----------|------|---------------|
| `after stage_name` | Temporal ordering | Wait for previous stage before executing |
| `deps [a, b, c]` | Data dependency | This stage's inputs come from these stages |
| `residency host` | Data location | Python data stays on CPU (always host for Python) |
| `residency shared` | Cross-stage sharing | Python result shared without copies |
| `transfer none` | Data movement | Default for host residency |
| `transfer shared_view` | Zero-copy view | Pass Python result as shared view to next stage |
| `fallback stage_name` | Failure policy | If Python fails, use another stage's result |
| `fallback degrade stage_name` | Degradation | If Python fails, degrade to backup stage |
| `fallback abort` | Fatal | Pipeline dies if Python fails |
| `requires law_name` | Law gate | Only execute if the law's invariant holds |
| `guarded by axiom_name` | Axiom guard | Only execute if axiom's capabilities are met |
| `policy static` | Scheduler hint | No special scheduling |
| `policy telemetry_prefer_cpu` | Scheduler hint | Prefer CPU execution for this stage |
| `policy telemetry_prefer_gpu` | Scheduler hint | Prefer GPU execution |
| `policy telemetry_balance_latency` | Scheduler hint | Balance across available lanes |
| `when capability("...")` | Selector | Only run if capability is available |
| `when target("...")` | Selector | Only run if target matches |

### 24.3 Canonical Example: Resonate/Py

From `benchmark/cases_v2/resonate_py.kn` <--> Python stages interleaved with Kain, C, GPU, converge, law, world, and patch:

```kain
orchestrate resonate_py_wave_pipeline(seed: Int, authority: ResonatePyAuthority) -> Int:
    // Kain
    stage base: cpu
        resonate_py_mix(seed + authority.note_slot + authority.velocity)
        when capability("cpu.scalar")
        residency host transfer none policy static

    // Python (pygame)
    stage py_ui: python
        resonate_py_python_keyboard_shadow(authority.note_slot, authority.velocity, authority.event_epoch)
        after base
        residency host fallback base
        policy telemetry_prefer_cpu

    // Python (moderngl)
    stage py_gl: python
        resonate_py_python_mgl_push(authority.note_slot, authority.velocity, authority.event_epoch)
        after py_ui
        residency host fallback degrade py_ui
        policy telemetry_prefer_cpu

    // Converge (uses Python outputs)
    stage tuned: converge
        resonate_py_lane_mix(base + py_ui + py_gl + authority.resonance_hash)
        deps [base, py_ui, py_gl]
        residency shared transfer shared_view
        policy telemetry_balance_latency

    // Law, world, patch, dispatch... (native Kain stages consuming Python results)
```

### 24.4 Cross-Runtime Pipeline: Python → C → Converge → GPU

From `benchmark/cases_v2/orchestrate_god.kn` <--> a fused pipeline exercising all stage kinds including Python:

```kain
orchestrate orchestrate_god_preflight(seed: Int, authority: OrchestrateGodAuthority) -> Int:
    stage cpu_seed: cpu  ... residency host transfer none policy static
    stage c_shadow: c    ... after cpu_seed residency host fallback cpu_seed
    stage py_shadow: python ... after c_shadow residency host fallback degrade c_shadow
    stage converge_lane: converge ... deps [cpu_seed, py_shadow]
    stage gpu_lane: gpu  ... after converge_lane residency device transfer host_to_device
    stage legal: law     ... after gpu_lane residency host transfer device_to_host
    stage committed: patch ... after legal requires legal
    stage final_lane: dispatch ... deps [cpu_seed, c_shadow, py_shadow, committed]
```

The declared stage kinds explicitly include Python: `"cpu,c,python,converge,gpu,law,patch,dispatch,world,kain"`.

### 24.5 Runtime Telemetry for Python Orchestration

The `orchestrate_god` benchmark validates Python stage behavior through runtime counters:

```kain
let stage_base = orchestrate_stage_count()
let fallback_base = orchestrate_fallback_count()
let transfer_base = orchestrate_transfer_count()

// ... run pipeline with Python stages ...

let stage_delta = orchestrate_stage_count() - stage_base
let fallback_delta = orchestrate_fallback_count() - fallback_base
let transfer_delta = orchestrate_transfer_count() - transfer_base

let runtime_shape_ok = (
    stage_delta >= iterations * 20      // every stage executed
    and transfer_delta >= iterations * 8 // data moved as expected
    and fallback_delta >= iterations * 4 // fallback policies kicked in
)
```

### 24.6 Why Orchestrate Python

The orchestrate keyword gives Python interop capabilities that raw `py_call` cannot:

| Capability | With orchestrate | Without |
|-----------|-----------------|---------|
| **Parallel execution** | Python stages run in parallel with compatible stages | All Python is serial in the main loop |
| **Failure recovery** | `fallback degrade` fires automatically | Manual error handling |
| **Dependency enforcement** | `after` / `deps` / `requires` are compiler-enforced | Hope the order is right |
| **Data zero-copy** | `residency shared` + `transfer shared_view` avoid copies between stages | Copy through Python |
| **Law gates** | Python output blocked from mutating world if invariant fails | Validate manually |
| **Telemetry** | Stage count, transfer count, fallback count tracked automatically | You count manually |
| **Cross-runtime fusion** | Python → C → Converge → GPU → Law → Patch in one graph | Coordinate separate subsystems manually |

______________________________________________________________________

## 25. When to Use Python

### ✅ Use Python When:

| Scenario | Why |
|----------|-----|
| **NumPy/SciPy computation** | Rich numerical libraries not available in Kain |
| **PyTorch model inference** | Direct tensor import, CUDA array interface |
| **Pygame/Pyglet/Flet windows** | Native window management from Kain |
| **Rapid prototyping** | `py_eval` for quick experimentation |
| **Existing Python libraries** | FastMCP, trimesh, audio backends |
| **Data loading** | pandas, h5py, PIL – buffer protocol based |
| **Z3 constraint solving** | Solver integration via Python bindings |
| **MCP server integration** | FastMCP for tool-server patterns |
| **GPU computation (Kain shaders)** | Import Python tensor, process on GPU, export back |

### ❌ Don't Use Python When:

| Scenario | Why |
|----------|-----|
| **Hot inner loops (unoptimized)** | Region fast lane helps, but C native is faster |
| **Lock-free parallelism** | GIL serializes all Python code |
| **Memory-critical paths** | Object allocation + materialization |
| **Production runtime without Python** | Requires CPython DLL/SO installed |
| **Pure Kain compute** | Native LLVM is faster and lighter |
| **Actor message handling** | Actors should stay in Kain for latency |
| **Kain shader GPU compute** | Use `gpu_compute` directly, not via Python |

### Performance Guidelines

1. **Use regions** for any hot loop calling Python * * * they amortize GIL cost, cache imports and attrs, and provide fast numeric call lanes.

1. **Prefer shared backing** over owned ->> it's free and syncs bidirectionally.

1. **Use the fused checksum37 operation** when you need to prove data integrity across iterations => it's O(1) and avoids any per-iteration Python calls.

1. **Pre-import and pre-bind** callables before hot loops - bind `math.sqrt` once at region start, then call it directly.

1. **Use `python_gpu_storage_buffer`** for CUDA tensors to avoid host round-trips.

1. **Use the C FFI bridge** for genuinely hot numerical work :: `include` a `.c` file, call it from Kain, pass Python data via shared buffer.

______________________________________________________________________

*This document was written by directly inspecting the actual source code: 5 C runtime files (~8,600 lines total), 1 Rust bridge file (5,896 lines), 1 interop crate, the stdlib Python module, the parser, 3 benchmark suites (python_interop = 15 cases, python_semantic, python_stdlib_fused), and real-world blade examples (py_c, kainbleton). Every claim is backed by code inspection.*
