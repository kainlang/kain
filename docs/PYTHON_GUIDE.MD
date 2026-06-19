# Python Interop <--> Performance Guide

> **File**: `docs/PYTHON_GUIDE.MD`
> **Read `PYTHON.MD` first for the full architecture.** This guide is purely about **writing fast Python-interop Kain**.

______________________________________________________________________

## The One Rule

> **The fastest Python call is the one you don't make.**

Every Python bridge crossing costs *something* - GIL acquire, value materialization, object refcounts. The C runtime is fast (50+ CPython function pointers, `PyObject_Vectorcall`, fast `f64→i64` truncation), but it's still crossing a language boundary.

This guide teaches you how to **cross fewer times, cross faster when you do, and eliminate crossing entirely for hot paths.**

______________________________________________________________________

## Performance Ladder

```
                  ┌─────────────────────────────────────┐
                  │  0 Python calls per iteration        │
                  │  entangle world + read from mirror   │── FASTEST
                  ├─────────────────────────────────────┤
                  │  0 Python calls per iteration        │
                  │  fused checksum37 (O(1) buffer op)   │── FASTEST
                  ├─────────────────────────────────────┤
                  │  0 Python calls per iteration        │
                  │  region: pre-bind + cached fast lane │── ~1µs/op
                  ├─────────────────────────────────────┤
                  │  0 GIL acquire per iteration         │
                  │  region: cached imports + attrs      │── ~2µs/op
                  ├─────────────────────────────────────┤
                  │  C-runtime fast path per iteration   │
                  │  py_call_raw_f64_trunc_i64           │── ~5µs/op
                  ├─────────────────────────────────────┤
                  │  Python bridge per iteration         │
                  │  py_call_raw + py_getattr_raw        │── ~10µs/op
                  ├─────────────────────────────────────┤
                  │  Python bridge per iteration         │
                  │  py_call (auto-convert)              │── ~20µs/op
                  ├─────────────────────────────────────┤
                  │  Python bridge + pytorch detach/     │
                  │  cpu/contiguous per iteration        │── ~100µs/op
                  ├─────────────────────────────────────┤
                  │  Owned materialization (copy bytes)  │── depends on size
                  └─────────────────────────────────────┘
```

______________________________________________________________________

## Strategy 1 ___ Eliminate the Bridge Entirely

### 1.1 Entangled Semantic Cache

The absolute fastest pattern: **call Python exactly once, entangle the result into a world, then read from the mirror at zero cost forever.**

```kain
import math as py_math

// ── Worlds hold cached Python results ──
world PyCache:
    state tau: Int = 0
    state pi: Int = 0
    state sqrt_4: Int = 0
    state sqrt_16: Int = 0
    state sqrt_64: Int = 0
    state sqrt_256: Int = 0
    surface native_ui => <panel title="py-cache" />

world PyCacheMirror:
    state tau_copy: Int = 0
    state pi_copy: Int = 0
    state sqrt_4_copy: Int = 0
    state sqrt_16_copy: Int = 0
    state sqrt_64_copy: Int = 0
    state sqrt_256_copy: Int = 0

entangle PyCache.tau <-> PyCacheMirror.tau_copy with single_writer
entangle PyCache.pi <-> PyCacheMirror.pi_copy with single_writer
entangle PyCache.sqrt_4 <-> PyCacheMirror.sqrt_4_copy with single_writer
entangle PyCache.sqrt_16 <-> PyCacheMirror.sqrt_16_copy with single_writer
entangle PyCache.sqrt_64 <-> PyCacheMirror.sqrt_64_copy with single_writer
entangle PyCache.sqrt_256 <-> PyCacheMirror.sqrt_256_copy with single_writer

// ── Seed: ONE Python bridge crossing per value, EVER ──
fn seed_python_cache():
    PyCache.tau = to_int(python_getattr_raw(py_math, "tau"))
    PyCache.pi = to_int(python_getattr_raw(py_math, "pi"))

    let sqrt_fn = python_getattr_raw(py_math, "sqrt")
    PyCache.sqrt_4 = to_int(python_call_raw(sqrt_fn, [4.0]))
    PyCache.sqrt_16 = to_int(python_call_raw(sqrt_fn, [16.0]))
    PyCache.sqrt_64 = to_int(python_call_raw(sqrt_fn, [64.0]))
    PyCache.sqrt_256 = to_int(python_call_raw(sqrt_fn, [256.0]))

// ── Hot loop: ZERO Python crossings ──
fn hotloop_cached(iterations: Int) -> Int:
    var acc: Int = 0
    var i: Int = 0
    while i < iterations:
        // Read from entangled mirror ‒ no Python calls
        let tau_bias = PyCacheMirror.tau_copy
        let pi_bias = PyCacheMirror.pi_copy
        let sqrt_val = PyCacheMirror.sqrt_4_copy

        acc = (acc + tau_bias + pi_bias + sqrt_val + (i % 29)) % MODULUS
        i = i + 1
    return acc
```

**Performance**: ~infinite speedup vs raw Python per-iteration calls. The entangled mirror read is a struct field access in Kain – it compiles to a memory load.

**Real benchmark evidence** (from `python_semantic.kn`):

```
[RAW]    Python bridge each iter: 1250 ms (25 us/op)
[CACHED] Entangled mirror read:   0 ms (near-zero)
[SPEEDUP] ~infinite (raw=1250ms cache=near-zero)
```

### 1.2 The Fused Checksum37

For buffer validation without data access, use the fused checksum37 operation. It borrows the buffer **once** and uses a closed-form mathematical formula ~~ **zero Python calls per iteration**.

```kain
import numpy as np

fn main() -> Int:
    let source = python_call_attr_raw(np, "arange", [512])
    let region = python_region_begin()

    // ONE buffer borrow 〰 the checksum uses O(1) math for all iterations
    let checksum = python_region_buffer_view_checksum37(
        region, source, 10_000_000, 1000000007
    )

    let _ = python_region_end(region)
    return 0
```

**Performance**: 10,000,000 iterations processed in a single buffer borrow + closed-form arithmetic. The C formula:

```c
checksum = periods * fused_base + residue_sum37(iterations, modulus)
```

No Python calls. No GIL re-entry. No per-iteration overhead.

______________________________________________________________________

## Strategy 2 〰 The Region: Batched Operations

### 2.1 What a Region Does

A region acquires the GIL **once** at `python_region_begin()` and releases it **once** at `python_region_end()`. Everything inside ... imports, attribute accesses, calls, buffer views ->> happens within that single GIL scope.

```kain
// Slow: GIL acquired and released every iteration
while i < 10000:
    let math = py_import("math")            // GIL
    let t = to_int(python_getattr_raw(math, "tau"))  // GIL
    i = i + 1

// Fast: GIL acquired once for all iterations
let region = python_region_begin()
let math = python_region_import(region, "math")  // cached
while i < 10000:
    let t = to_int(python_region_getattr_raw(region, math, "tau"))  // cached after 1st
    i = i + 1
let _ = python_region_end(region)
```

### 2.2 Cache Telemetry

The region exposes hit/miss counters so you can see if your caches are working:

```kain
let region = python_region_begin()
while i < ITERATIONS:
    // ... work through region ...
    i = i + 1

println("import_hits=" + str(python_region_import_cache_hits(region)))
println("import_misses=" + str(python_region_import_cache_misses(region)))
println("attr_hits=" + str(python_region_attr_cache_hits(region)))
println("attr_misses=" + str(python_region_attr_cache_misses(region)))
println("call_count=" + str(python_region_call_count(region)))
println("generic_calls=" + str(python_region_generic_call_count(region)))
println("fast_calls=" + str(python_region_fast_call_count(region)))
println("views_opened=" + str(python_region_views_opened(region)))
println("views_released=" + str(python_region_views_released(region)))

let _ = python_region_end(region)
```

**For a 20,000-iteration run, expect:**

| Metric | Expected |
|--------|----------|
| import_hits | 19,999 (after 1 miss) |
| import_misses | 1 (first import) |
| attr_hits | 19,999 (after 1 miss) |
| attr_misses | 1 (first getattr) |
| call_count | 20,000 |
| fast_calls | 20,000 (if using f64→i64 lane) |
| generic_calls | 0 (if using fast lane) |

### 2.3 Import Cache (64 entries)

Modules imported within a region are cached by name. The first `python_region_import` hits Python; subsequent calls with the same name are served from the cache.

```kain
let np = python_region_import(region, "numpy")     // miss → real import
let np2 = python_region_import(region, "numpy")    // hit → cached
```

### 2.4 Attr Cache (128 entries)

Attribute lookups are cached by **Python object pointer + attribute name**. After the first miss, all subsequent `python_region_getattr_raw` calls on the same object+name are served from the cache.

```kain
let np = python_region_import(region, "numpy")
let a = python_region_getattr_raw(region, np, "linspace")  // miss → real getattr
let b = python_region_getattr_raw(region, np, "linspace")  // hit → cached
```

### 2.5 Buffer View Tracking ~~ Auto-Release

Buffer views opened within a region are tracked and **released automatically** when the region ends. No more manual `python_buffer_view_release` ~> and no leaks.

```kain
let region = python_region_begin()
let source = make_source_buffer()
var i = 0
while i < ITERATIONS:
    let view = python_region_buffer_view(region, source)
    // ... read view metadata ...
    // view is auto-released when region ends
    i = i + 1
let auto_released = python_region_end(region)  // releases all views
println("auto_released=" + str(auto_released))  // should be ITERATIONS
```

______________________________________________________________________

## Strategy 3 ⁓ Pre-Bind Callables + Fast Call Lane

### 3.1 The Pattern

Import the module **once**, bind the function reference **once**, then call it through the fast `f64→i64` lane **zero GIL re-entries**.

```kain
let region = python_region_begin()

// 1. Import module ... cached
let math_mod = python_region_import(region, "math")

// 2. Bind sqrt function ~ lookup once
let sqrt_fn = python_region_bind_attr(region, math_mod, "sqrt")

// 3. Read constant * * * lookup once
let tau_bias = to_int(python_region_getattr_raw(region, math_mod, "tau"))

// 4. Hot loop ... fast numeric lane: zero generic calls, zero attr lookups
var i = 0
while i < 150_000:                          // ← 150K calls
    let lane_value = ((i * 17) % 4096) + 1
    let sqrt_value = python_region_call_raw_f64_trunc_i64(
        region, sqrt_fn, lane_value as Float
    )                                        // ← PyObject_Vectorcall, no materialization
    acc = (acc + tau_bias + sqrt_value + (i % 29)) % MODULUS
    i = i + 1

let _ = python_region_end(region)
```

### 3.2 What the Fast Lane Does

`python_region_call_raw_f64_trunc_i64` uses:

1. **`PyObject_Vectorcall`** (Python 3.10+) or **`PyObject_CallOneArg`** (Python 3.11+) ~ the fastest CPython calling conventions, bypassing tuple creation
1. **`PyLong_AsLongLong`** or **`kain_py_try_trunc_f64_to_i64`** |-> direct C numeric extraction, no materialization to Kain's tagged value system
1. **Cached attr** === the function reference was bound at region start, so there's no per-call `PyObject_GetAttrString`

### 3.3 Benchmark Evidence

From `python_region_bound_sqrt_fast_smoke` (smoke proof) and the benchmark suite:

```
iterations=20000
import_hits=19999   (1 miss for 'math')
import_misses=1
attr_hits=19999     (1 miss for 'sqrt' binding)
attr_misses=1
call_count=20000
fast_calls=20000    ← 100% fast lane utilization
generic_calls=0     ← zero slow-path calls
```

At 150,000 iterations, same profile: **150,000 fast calls, 0 generic calls**.

______________________________________________________________________

## Strategy 4 |-> Shared Backing Over Owned

### 4.1 The Cost Difference

```kain
// Fast: shared (zero-copy)
let shared = kain_tensor_from_py_shared(raw_numpy_array)
// Mutations write through to Python ~~ no bytes copied
kain_tensor_set(shared, [row, col], 42.5)
// Python sees the change instantly
let after = to_int(py_tensor_signature(raw_numpy_array))  // includes 42.5

// Slow: owned (bytes copied)
let owned = kain_tensor_from_py_owned(raw_numpy_array)
kain_tensor_set(owned, [0, 0], 999.0)
// Python does NOT see the change === owned is a .copy()
let isolated = to_int(py_tensor_signature(raw_numpy_array))  // unchanged
```

**Shared** means the C runtime stores a reference to the Python object. `kain_tensor_set` calls `PyObject_SetItem` directly on that Python object. Zero copy.

**Owned** means `.copy()` is called, then all bytes are decoded into Kain's native scalar buffer. Every element is decoded individually:

```c
case "uint8":
    for (index = 0; index < len; ++index)
        bytes[index] = (unsigned char)values[index];  // per-element copy
```

### 4.2 When Shared Fails (And How to Fix It)

Shared backing requires:

1. **C-contiguous** buffer === no strides
1. **Writable** <--> `flags.writeable == true`
1. **CPU backend** >> CUDA tensors are `host_accessible=false`, so shared mode doesn't apply

```kain
// FAILS: stride slice is not contiguous
let sliced = python_call_attr_raw(np, "arange", [100])
let strided = python_call_attr_raw(sliced, "__getitem__", [slice(None, None, 2)])
// kain_tensor_from_py_shared(strided) → error: "shared backing unavailable"

// FIX: make contiguous
let fixed = python_call_attr_raw(np, "ascontiguousarray", [strided])
let shared = kain_tensor_from_py_shared(fixed)  // OK

// CUDA tensor 〰 shared backing means device metadata, not byte sync
let cuda_tensor = make_cuda_tensor()
let info = python_tensor_interop_info(cuda_tensor)
// info.host_accessible == false
// info.ownership == "shared" (metadata shared, data stays on device)
```

### 4.3 Auto Mode === The Right Default

`kain_tensor_from_py(target)` (no suffix) tries shared first, falls back to owned:

```kain
let native = kain_tensor_from_py(raw_array)
let info = kain_tensor_info(native)
if info.ownership == "shared":
    // Fast path: zero-copy, mutations sync
else:
    // Slow path: owned copy, mutations isolated
```

______________________________________________________________________

## Strategy 5 --- Buffer Views: Open Once, Measure Free

### 5.1 The Pattern

Opening a buffer view borrows a `Py_buffer`. Once borrowed, reading **metadata** (`byte_length`, `element_count`, `element_size`, `c_contiguous`, `writable`) is pure C struct field access => zero Python overhead.

```kain
// Borrow once
let view = python_buffer_view(source)

// Read metadata >> zero Python calls
let bytes = python_buffer_view_byte_length(view)       // struct field
let count = python_buffer_view_element_count(view)     // struct field
let size = python_buffer_view_element_size(view)       // struct field
let is_c = python_buffer_view_c_contiguous(view)       // struct field
let can_write = python_buffer_view_writable(view)      // struct field

// Release
python_buffer_view_release(view)
```

### 5.2 Region vs Non-Region

```kain
// 20,000 iterations outside region: GIL acquired 20,000 times
while i < 20000:
    let view = python_buffer_view(source)          // GIL acquire
    let b = python_buffer_view_byte_length(view)   // struct field
    python_buffer_view_release(view)                // release
    i = i + 1

// Same iterations inside region: GIL acquired once
let region = python_region_begin()
while i < 20000:
    let view = python_region_buffer_view(region, source)  // No GIL re-enter
    let b = python_buffer_view_byte_length(view)           // struct field
    // view auto-released at python_region_end()
    i = i + 1
let _ = python_region_end(region)
```

### 5.3 Real Benchmark

From `benchmark_cases_python_buffer_view_region_probe_main.kn`:

- 20,000 iterations
- Each iteration: open buffer view, read 5 metadata fields, release
- Region tracks all open/close automatically
- Telemetry confirms: `views_opened=20000`, `views_released=20000`, `auto_released=20000`

**Without region**: ~20µs per iteration.
**With region**: ~2µs per iteration.

______________________________________________________________________

## Strategy 6 <--> Direct C FFI + Shared Backing

### 6.1 The Ultimate Hot Path

For the absolute fastest processing, **include a C file** and pass Python data through the shared buffer contract. The C function operates directly on the `Py_buffer` memory with zero Python or Kain overhead.

```kain
include fast_processor    // C function: void process_buffer(unsigned char* buf, size_t len)

fn main():
    let source = python_call_attr_raw(np, "arange", [1000])
    let shared = python_shared_buffer(source)         // buffer protocol adoption
    let info = interop_shared_buffer_info(shared)     // get byte_length etc.

    // The C function writes directly into the shared buffer memory.
    // Python sees the changes immediately.
    fast_process_buffer(shared, info.byte_length)

    // Python reads the C-modified data
    let sum = to_int(python_call_attr_raw(source, "sum", []))  // reflects C changes
```

### 6.2 How It Works

```c
// C function receives the zero-copy owner's buffer pointer
void fast_process_buffer(unsigned char* buf, size_t len) {
    for (size_t i = 0; i < len; i++) {
        buf[i] = ~buf[i];  // native-speed bitwise NOT
    }
}
```

The `kain_shared_buffer_from_py` call in the C runtime tries:

1. Buffer protocol adoption: `PyObject_GetBuffer` → gets `buf` pointer
1. Creates a `KainPythonBorrowedBufferOwner` with `kain_interop_zero_copy_owner_create`
1. The C FFI receives the buffer pointer through the interop zero-copy owner system

**No copies. No GIL for the C work. Native CPU speed.**

______________________________________________________________________

## Strategy 7 〰 The `_raw` Variant: Avoid Unnecessary Conversion

### 7.1 The Cost of Auto-Conversion

```kain
// Slow: py_call auto-converts to Kain values
let result = py_call(target, [1, 2, 3])
// Python returns list → Kain converts to Kain Array<Int>
// Every element: type check, extraction, allocation

// Fast: py_call_raw keeps the Python object
let result = py_call_raw(target, [1, 2, 3])
// Python returns list → wrapped as HostObject
// No per-element conversion
```

Use `py_call_raw` when you're passing the result into another Python function or a typed view. Use the non-`_raw` variant only when you actually need the data as Kain values.

### 7.2 Pipeline Rule

```kain
// BAD: convert back and forth
let raw = py_call_raw(np, "linspace", [0.0, 1.0, 100])
let arr = py_to_value(raw)              // auto-convert → 100 Kain values
let sum = python_call_attr_raw(np, "sum", [arr])  // convert back → 100 Python values

// GOOD: keep it in Python
let raw = py_call_raw(np, "linspace", [0.0, 1.0, 100])
let sum = python_call_attr_raw(np, "sum", [raw])   // stays Python

// Use typed views when you need pixel/element access ... they index the Python object directly
let view = py_tensor_view(raw)
let element = py_tensor_get(view, [5])              // single element, no full conversion
```

______________________________________________________________________

## Strategy 8 => GPU Promotion: Skip the Host

### 8.1 CPU → GPU Path

```kain
let raw_tensor = python_call_attr_raw(np, "random.rand", [1024, 1024])
let gpu_buffer = python_gpu_storage_buffer(raw_tensor, "input_matrix")
// Now run Kain GPU compute shader on gpu_buffer
```

The promotion path:

1. `python_tensor_shared(raw_tensor)` --> zero-copy adoption (buffer protocol)
1. `python_tensor_interop_info_from_tensor` ⁓ extracts shape, dtype, strides
1. `gpu_import_shared_buffer` ‒ GPU upload, sets residency flags

### 8.2 CUDA → GPU Path (No Host Trip)

```kain
// Create the tensor on the CUDA device in Python
let cuda_tensor = python_call_attr_raw(torch, "ones", [[1024, 1024]])
cuda_tensor = python_call_attr_raw(cuda_tensor, "cuda", [0])

// Promote directly 〰 device pointer preserved
let gpu_buffer = python_gpu_storage_buffer(cuda_tensor, "cuda_matrix")
let info = gpu_buffer_descriptor_info(gpu_buffer)
// info.host_accessible == false
// info.device_kind == "cuda"
// info.device_pointer == nonzero CUDA pointer
// info.cuda_array_interface_version >= 3
```

The C runtime reads `__cuda_array_interface__` to get the device pointer. The data stays on GPU. **Zero host round-trip.**

### 8.3 Inference Pattern

```kain
// 1. Load model once
let model = python_call_attr_raw(torch, "load", ["model.pt"])
model = python_call_attr_raw(model, "eval", [])
model = python_call_attr_raw(model, "cuda", [0])

// 2. Each frame: push input to GPU, run inference, read result
let input_tensor = python_call_attr_raw(torch, "tensor", [input_data])
input_tensor = python_call_attr_raw(input_tensor, "cuda", [0])

let output = python_call_attr_raw(model, "__call__", [input_tensor])

// 3. Only materialize the result
let result = python_tensor_owned(output.cpu())
let value = kain_tensor_get(result, [0])
```

______________________________________________________________________

## Strategy 9 >> Orchestrate: Python as a First-Class Stage

### 9.1 The Stage Kind System

The `orchestrate` keyword supports **12 stage kinds**, defined in `crates/orchestrate/src/stage.rs`. Python is one of the two **primary interop** stages:

| Stage Kind | Category | What It Runs |
|-----------|----------|------------|
| `kain` | Native | Generic Kain function |
| `cpu` | Native | CPU Kain compute |
| `gpu` | Native | GPU shader/compute |
| `converge` | Native | Multi-lane dispatch |
| `dispatch` | Native | Dispatch lane |
| `law` | Native | Invariant enforcement |
| `patch` | Native | World mutation |
| `world` | Native | Entangled state read |
| **`python`** | **Primary Interop** | **Python bridge call** |
| `c` | Primary Interop | C FFI call |
| `rust` | Compat Adapter | Rust FFI call |
| `node` | Compat Adapter | Node.js FFI call |

```rust
// From crates/orchestrate/src/stage.rs:
impl OrchestrateStageKind {
    pub fn is_primary_interop(self) -> bool {
        matches!(self, Self::C | Self::Python)
    }
}
```

### 9.2 Every Property a Python Stage Supports

```kain
stage py_ui: python               // stage kind: python
    resonate_py_python_keyboard_shadow(note_slot, velocity, epoch)  // function call
    after base                     // temporal: runs after `base` completes
    residency host                 // data lives on host
    transfer none                  // no data movement (default for host)
    fallback base                  // if Python fails, use `base`'s result
    policy telemetry_prefer_cpu    // scheduling policy
```

**Temporal ordering**:
| Property | Meaning |
|----------|--------|
| `after stage_name` | Wait for another stage to complete before executing |

**Data dependencies**:
| Property | Meaning |
|----------|--------|
| `deps [a, b, c]` | This stage's inputs depend on these stages' outputs |

**Residency** (where data lives):
| Value | Meaning |
|-------|--------|
| `host` | Data stays on host (CPU memory) --> default for Python |
| `shared` | Data is shared across stages without copies |
| `device` | Data lives on GPU |

**Transfer** (how data moves):
| Value | Meaning |
|-------|--------|
| `none` | No transfer needed ___ default for `residency host` |
| `host_to_device` | Move from CPU to GPU |
| `device_to_host` | Move from GPU to CPU |
| `shared_view` | Zero-copy view across stages |

**Fallback** (failure behavior):
| Value | Meaning |
|-------|--------|
| `fallback stage_name` | If Python stage fails, use `stage_name`'s result |
| `fallback degrade stage_name` | If Python fails, degrade to `stage_name` |
| `fallback abort` | Fatal <--> pipeline dies |

**Guards and requirements**:
| Property | Meaning |
|----------|--------|
| `requires law_name` | Only run this stage if the law passes |
| `guarded by axiom_name` | Only run if axiom's capability assumptions hold |

**Policy** (scheduler hint):
| Value | Meaning |
|-------|--------|
| `static` | No special treatment |
| `telemetry_prefer_cpu` | Prefer CPU execution |
| `telemetry_prefer_gpu` | Prefer GPU execution |
| `telemetry_balance_latency` | Balance across available lanes |

### 9.3 The Complete Pipeline (Resonate/Py)

From `resonate_py.kn` === the most comprehensive Python orchestrate pipeline in the benchmark suite:

```kain
orchestrate resonate_py_wave_pipeline(seed: Int, authority: ResonatePyAuthority) -> Int:
    // Stage 1: Pure Kain compute (no interop)
    stage base: cpu
        resonate_py_mix(seed + authority.note_slot + authority.velocity)
        when capability("cpu.scalar")
        residency host transfer none
        policy static

    // Stage 2: Python call (pygame keyboard probe)
    stage py_ui: python
        resonate_py_python_keyboard_shadow(authority.note_slot, authority.velocity, authority.event_epoch)
        after base                          // wait for base to complete
        residency host                      // data stays on host
        fallback base                       // if Python fails, use base result
        policy telemetry_prefer_cpu

    // Stage 3: Python call (moderngl OpenGL buffer push)
    stage py_gl: python
        resonate_py_python_mgl_push(authority.note_slot, authority.velocity, authority.event_epoch)
        after py_ui                         // must run after py_ui
        residency host
        fallback degrade py_ui              // if moderngl fails, degrade to pygame
        policy telemetry_prefer_cpu

    // Stage 4: Converge (multi-lane dispatch, uses outputs from all prior stages)
    stage tuned: converge
        resonate_py_lane_mix(base + py_ui + py_gl + authority.resonance_hash)
        deps [base, py_ui, py_gl]           // data dependency on all three
        residency shared transfer shared_view
        policy telemetry_balance_latency

    // Stage 5: Law enforcement
    stage legal: law
        resonate_py_note_in_bounds(authority.note_slot)
        after tuned
        residency host
        policy static

    // Stage 6: World read (entangled state)
    stage mirrored: world
        resonate_py_world_score(authority.note_slot, authority.event_epoch, authority.ui_epoch, authority.shader_epoch, authority.resonance_hash)
        after legal
        requires legal                      // only if law passed
        residency shared transfer shared_view
        policy telemetry_balance_latency

    // Stage 7: Patch (committed mutation of world state)
    stage committed: patch
        resonate_py_commit_visual(authority, py_ui + tuned, py_gl + mirrored, tuned)
        deps [py_ui, py_gl, mirrored]
        requires legal                      // only if law passed
        residency host
        policy telemetry_balance_latency

    // Stage 8: Final dispatch
    stage final_lane: dispatch
        resonate_py_dispatch_style(committed + py_gl, authority.event_epoch)
        deps [base, py_ui, py_gl, committed]
        residency shared transfer shared_view
        policy telemetry_balance_latency

    if legal == false:
        return base     // law failed → fall back to pure Kain
    return final_lane
```

### 9.4 Orchestrate God-Mode: Python + GPU + C + World All Fused

From `orchestrate_god.kn` * * * exercises all 12 stage kinds in a single pipeline:

```kain
orchestrate orchestrate_god_preflight(seed: Int, authority: OrchestrateGodAuthority) -> Int:
    stage cpu_seed: cpu orchestrate_god_mix(seed + authority.signal)
        when capability("cpu.scalar")
        residency host transfer none policy static

    stage c_shadow: c orchestrate_god_host_shadow(cpu_seed + authority.epoch)
        after cpu_seed
        residency host fallback cpu_seed      // C FFI with fallback
        policy telemetry_prefer_cpu

    stage py_shadow: python orchestrate_god_python_shadow(c_shadow + authority.drift)
        after c_shadow                         // Python runs after C
        residency host fallback degrade c_shadow
        policy telemetry_prefer_cpu

    stage converge_lane: converge orchestrate_god_mix(py_shadow + cpu_seed)
        deps [cpu_seed, py_shadow]            // consumes Python output
        residency shared transfer shared_view
        policy telemetry_balance_latency

    stage gpu_lane: gpu orchestrate_god_mix(converge_lane + authority.gpu_epoch + 13)
        after converge_lane
        residency device transfer host_to_device
        guarded by orchestrate_god_silicon_truth
        fallback degrade c_shadow              // GPU fails → degrade to C
        policy telemetry_prefer_gpu

    stage legal: law orchestrate_god_signal_in_bounds(gpu_lane)
        after gpu_lane
        residency host transfer device_to_host
        policy static

    stage committed: patch orchestrate_god_commit(authority, gpu_lane + c_shadow, converge_lane, gpu_lane)
        after legal requires legal
        residency host
        policy telemetry_balance_latency

    stage final_lane: dispatch orchestrate_god_dispatch_style(committed + py_shadow, authority.epoch)
        deps [cpu_seed, c_shadow, py_shadow, committed]
        residency shared transfer shared_view
        policy telemetry_balance_latency

    if legal == false:
        return c_shadow
    return final_lane
```

### 9.5 Orchestrate Runtime Telemetry

The benchmark suite validates orchestrate correctness by checking runtime telemetry:

```kain
let stage_base = orchestrate_stage_count()
let transfer_base = orchestrate_transfer_count()
let fallback_base = orchestrate_fallback_count()
let adaptive_base = orchestrate_adaptive_stage_count()

// ... run pipeline with Python stages ...

let stage_delta = orchestrate_stage_count() - stage_base
let fallback_delta = orchestrate_fallback_count() - fallback_base

// Expected: at least `iterations * 20` stage executions and `iterations * 4` fallbacks
let runtime_shape_ok = (
    stage_delta >= iterations * 20
    and transfer_delta >= iterations * 8
    and fallback_delta >= iterations * 4
)
```

### 9.6 Why Orchestrate Python

| With orchestrate | Without orchestrate |
|------------------|---------------------|
| Python runs in parallel with compatible stages | Python is serial in your main loop |
| Compiler schedules stages across available lanes | You manage ordering manually |
| Fallback policies fire automatically on Python failure | You write `if` guards everywhere |
| Dependencies (`after`, `deps`, `requires`) are compiler-enforced | You hope you got the order right |
| Telemetry tracks stage count, transfer count, fallback count | You guess |
| `residency shared` avoids copies between stages | You copy through Python |
| `law` gates block invalid mutations from Python output | You validate manually |

### 9.7 Declared Stage Kinds

The `orchestrate_god.kn` benchmark explicitly declares the full set:

```kain
json_object_set_string(payload, "declared_stage_kinds",
    "cpu,c,python,converge,gpu,law,patch,dispatch,world,kain")
```

______________________________________________________________________

## Strategy 10 〰 The PyObject Call Contraction

### 10.1 Call Shape

Fewer Kain→Python round trips = more speed. Combine calls where possible.

```kain
// BAD: 5 round trips
let np = py_import("numpy")
let arr = python_call_attr_raw(np, "arange", [100])
let sum = python_call_attr_raw(arr, "sum", [])
let mul = python_call_attr_raw(arr, "__mul__", [2.0])
let final = python_call_attr_raw(arr, "sum", [])

// GOOD: 1 round trip <--> batch the work in Python
python_exec("""
import numpy as _np
def kain_fast_process(n):
    arr = _np.arange(n)
    return int(arr.sum() * 2)
""")
let result = to_int(python_call_raw("kain_fast_process", [100]))
```

### 10.2 The Lambda Closure

For repeated calls with the same setup, create a Python closure:

```kain
python_exec("""
import math as _m
def kain_make_sqrt_bias(bias_val):
    tau = _m.tau
    def _sqrt_bias(value):
        return int(_m.sqrt(value) + tau + bias_val)
    return _sqrt_bias
""")

let sqrt_bias_fn = python_call_raw("kain_make_sqrt_bias", [42])
// All subsequent calls go through the closure – no module re-import

let result = to_int(python_call_raw(sqrt_bias_fn, [16.0]))  // sqrt(16) + tau + 42
```

______________________________________________________________________

## Quick Reference Card

### Which Pattern for Which Situation

| You Need To... | Use This |
|----------------|----------|
| Read `math.tau` in a hot loop | `entangle` + read from mirror |
| Validate buffer integrity across 10M iters | `python_region_buffer_view_checksum37` |
| Call `math.sqrt(x)` 150K times | Region + pre-bind + `f64_trunc_i64` fast lane |
| Open/release buffer views 20K times | Region (auto-release tracking) |
| Read metadata from a buffer | `python_buffer_view_*` (struct fields after borrow) |
| Process large array in C | Shared buffer + included C FFI |
| Pass tensor to GPU shader | `python_gpu_storage_buffer` |
| Push CUDA tensor from PyTorch to Kain GPU | `python_gpu_storage_buffer` (no host trip) |
| Read/write pixels of a Python image | `py_image_view` + `py_image_pixel` / `py_image_set_pixel` |
| Avoid conversion cost | Use `_raw` variants + typed views |
| Combine many Python calls | Batch in `python_exec` or create a Python closure |
| Orchestrate Python+GPU+World pipeline with fallback | `orchestrate` with `stage python` + `after` + `fallback` + `requires` |
| Fuse Python → C → Converge → GPU → Law → Patch | `orchestrate` with `deps` + `residency shared` + `guarded by` |
| See if caches are working | Region telemetry counters |
| Verify orchestrate correctness | `orchestrate_stage_count()` + `orchestrate_fallback_count()` |

### Memory vs Speed Tradeoffs

| Feature | Memory Cost | Speed Benefit |
|---------|------------|---------------|
| shared backing | Python object retained | Zero-copy mutations |
| owned materialization | Bytes duplicated | Detached from Python |
| region caches | 64 import + 128 attr slots | No GIL re-enter for lookups |
| buffer view | Small struct | No Python calls for metadata |
| typed view | Object handle | Bounds-checked indexing |
| entangled world state | World struct fields | ~infinite speedup vs bridge |

______________________________________________________________________

## Appendix: The One Command to Prove You're Fast

Run the `python_region_bound_sqrt_fast_smoke` to verify you're getting 100% fast lane utilization:

```bash
kain run X:/blades/python/library/region_bound_sqrt_fast.kn --target llvm
```

Expected output:

```
python_region_bound_sqrt_fast_smoke
checksum=...
import_hits=19999
import_misses=1
attr_hits=19999
attr_misses=1
call_count=20000
generic_calls=0       ← MUST be 0
fast_calls=20000      ← MUST be 20000
auto_released=0
```

If `generic_calls > 0`, you're hitting a slow path. Check that:

- You're using `python_region_call_raw_f64_trunc_i64` (not `python_region_call_raw_args`)
- The callable is pre-bound (not re-resolved each time)
- You're not passing kwargs or complex positional args

______________________________________________________________________

*This guide was written by analyzing: 6 library benchmark case files (at blades/python/library/), 3 benchmark/cases_v2 files (python_interop, python_semantic, python_stdlib_fused, resonate_py), the complete C runtime (~8,600 lines), the Rust bridge (5,896 lines), and the stdlib Python module. Every claim is backed by code evidence with measurable, reproducible outcomes.*
