# Kain Standard Library — Complete Reference & Wiring Audit

**Date:** 2026-06-21
**Status:** Full audit of 73+ modules by 6 independent agents. 465+ @extern verified against C runtime. 42 broken @extern found. 20 stub functions found.
**Based on:** 6 explorer audits in `X:/research/stdlib/AGENT1-6_*.md`, cross-referenced against `runtime/native/`, `stdlib/STDLIB_MAP.llm.md`, `docs/STDLIB.md`

---

## Taxonomy

| Classification | Definition | Count |
|---------------|-----------|:-----:|
| **C CONTRACT** | Has `@extern` functions backed by real C code in `runtime/native/src/` | ~16 |
| **KAIN ONLY** | Pure Kain — no `@extern` to C runtime. Uses compiler intrinsics (`alloc`, `mem_load`, `to_string`, etc.) | ~42 |
| **RUST BRIDGE** | Has `@extern` backed by Rust crates, not C runtime | ~2 |
| **PYTHON BRIDGE** | Routes through Python host embedding | ~2 |
| **MIXED** | Combination of C contracts + pure Kain | ~6 |
| **STUB** | `@extern` declared but no C/Rust implementation found | 0* |
| **BROKEN** | `@extern` declared but underlying native function not registered | 42 |

*\*Zero classic stubs (all declared @extern have SOME implementation). However, 42 python.kn @extern have no Rust registration, and markscript.kn has 20 pure-Kain no-op functions.*

---

## When to Use What

### "I need to read/write files"
→ **`use std::fs`** — 50+ @extern, all backed by real C in `stdlib_abi.c` (fopen/fread/fwrite/stat). Use `fs_read_text`, `fs_write_text`, `fs_exists`, `fs_read_dir_paths_text`, `fs_walk_paths_text`. For atomic writes: `fs_atomic_write_text` (write to temp + rename). For raw bytes: `fs_read_bytes_hex`, `fs_write_bytes_hex`. For streaming: `fs_open`/`fs_read`/`fs_write`/`fs_close`.

### "I need networking (TCP/HTTP)"
→ **`use std::net`** — ~60 @extern, all backed by real C in `net_system.c` (Winsock/BSD sockets). Use `tcp_connect`, `tcp_listen`, `tcp_accept`, `tcp_read_text`, `tcp_write_text`. For HTTP: `http_request_create`, `http_client_send`, `http_server_create`, `http_server_route_actor`, `http_server_pump`. Z3-proven overflow guards.

### "I need an HTTP client convenience layer"
→ **`use std::http`** — pure Kain wrappers over `std::net`. No new @extern. Use `http_get_text`, `http_post_text`, `http_server_create_localhost`. For HTTP/2: `use std::http2` — capability-gated API shape; runtime currently returns "not supported" for http2.

### "I need a UI (windows, nodes, styles, events)"
→ **`use std::ui`** — 83 @extern, all backed by real C in `ui_system.c` + `ui_host_adapter.c`. This is the largest C contract in the stdlib. Use `ui_host_session_create(name, title, w, h, "winit")` for a real Win32 window. Use `ui_node_create`, `ui_node_set_rect`, `ui_node_set_text`, `ui_node_set_style_string` for retained-mode UI. **⚠️ The renderer ignores the node tree — elements are stored but not rendered to pixels. Only the GDI proof-of-concept renders a hardcoded gradient.**

### "I need keyboard/mouse/gamepad input"
→ **`use std::input`** — 26 @extern, all backed by real C in `input_system.c`. CBMC-verified mailbox semantics. Use `input_session_create`, `input_bind_action`, `input_action_pressed`, `input_action_down`, `input_action_released`, `input_axis_value`. **⚠️ Win32 only — no Linux/macOS input service.**

### "I need audio playback/MIDI"
→ **`use std::audio::device`** — 9 @extern, all backed by real C in `audio_system.c` (WASAPI/WinMM/CoreAudio/ALSA). Use `audio_default_output_device`, `audio_stream_open`, `audio_stream_start`. For MIDI: `audio_midi_open_input`, `audio_midi_device_name`. For WAV/AIFF file I/O: `audio/file.kn` (pure Kain parser). **⚠️ FLAC/MP3/OGG @extern are NOT implemented — falls back to WAV silently.**

### "I need audio DSP (oscillators, filters, FFT, reverb)"
→ **`use std::audio::dsp`** — 1,224 lines of pure Kain. No C dependency. Oscillators (sine/saw/square/triangle), biquad/SVF filters, FFT, convolution, dynamics (compressor/limiter/gate), reverb, delay, modulation (chorus/flanger/phaser/tremolo).

### "I need GPU compute (CUDA, SPIR-V, pipelines)"
→ **`use std::graphics`** — 47 @extern for graphics sessions, buffers, shaders, meshes, pipelines, draw commands. All backed by real C in `graphics_system.c`. **⚠️ Vulkan/D3D12 backends are catalog-only (listed as "degraded"). Only the software path exists.** For GPU types and pipeline library: `use std::gpu` (pure Kain policy layer). For CUDA: `use std::cuda` — 19 runtime @extern (real C in `cuda_runtime.c`), 18 device-side PTX intrinsics (no C contract — lowered by GPU backend).

### "I need math (vectors, matrices, quaternions, colors)"
→ **`use std::math`** — 3,100 lines of pure Kain. Largest self-contained module. Zero C dependency. Vec2/3/4, Mat3/4, Quat, Affine2/3, Ray3, Aabb, Obb, Frustum, ColorRgb/Rgba, Hsv/Hsl, Complex, DualQuat, noise (Perlin/Simplex/Worley), easing (30+ functions), physics (spring/PID), tonemapping (ACES/Reinhard/Uncharted2), spherical harmonics, half-float packing.

### "I need data structures (arrays, maps, queues, sets)"
→ **`use std::collections`** — 797 lines, mostly pure Kain. Array, List, Map, Set, Stack, Queue, Deque, PriorityQueue, ArrayList, HashMap, SlotMap, IntrusiveHashMap. Uses `alloc`/`decay` intrinsics (backed by C memory.c) and 1 @extern (`abi_map_release` → real C in stdlib_abi.c).

### "I need string/bytes/encoding operations"
→ **`use std::text`** (search, split, join, trim, replace, case conversion), **`use std::fmt`** (formatting, padding, hex/binary display), **`use std::bytes`** (byte buffers, hex encode/decode, endianness), **`use std::base64`** (encode/decode), **`use std::unicode`** (UTF-8 encode/decode, ⚠️ normalize is stub). All pure Kain over compiler intrinsics.

### "I need JSON parsing/generation"
→ **`use std::json`** — 17 C-backed builtins (in `json.c`: parse, stringify, object/array CRUD) + 113 pure-Kain query/validation wrappers. Use `json_parse_text`, `json_string_field`, `json_int_field`, `json_object_set`, `json_array_push`. Service key: `data.json` (available).

### "I need crypto (SHA-256, HMAC, BLAKE3, random bytes)"
→ **`use std::crypto`** — 4 @extern, all backed by real hand-rolled C in `stdlib_abi.c`. No external library dependency. `crypto_sha256`, `crypto_hmac_sha256`, `crypto_blake3`, `crypto_random_bytes_hex` (OS entropy).

### "I need hashing (FNV-1a, CRC32, xxHash)"
→ **`use std::hash`** — pure Kain. All deterministic, target-neutral. Use `hash_fnv1a32`, `hash_crc32`, `hash_mix64`, `hash_bucket_mod64`. No C dependency — safe for capsules, caches, and proof blades.

### "I need OS services (env vars, processes, threads, time)"
→ **`use std::os`** — 38 @extern for env vars, mmap, syscalls, chdir, terminal size, fork/exec, io_uring. **`use std::process`** — ~40 @extern for process spawn/wait/kill/pipe/pty. **`use std::thread`** — 4 @extern for thread create/join/yield/affinity. **`use std::time`** — 2 @extern for now_millis/sleep_millis + pure Kain Duration/Instant/Deadline types.

### "I need synchronization (mutex, rwlock, semaphore, channels)"
→ **`use std::sync`** — pure Kain over `std::atomic` intrinsics. McsMutex, RwLock, Once, WaitGroup, Semaphore, CondVar, TeleportChannel (lock-free SPSC ring buffer). **`use std::atomic`** — 3 @extern for wait/notify (backed by real C in core.c: futex/SRW lock) + compiler-builtin atomics.

### "I need semantic telemetry (patch journal, entangle propagation, converge mismatch, pulse fire count)"
→ **`use std::intent`** — 34 @extern, the only C contract module in the testing/meta category. All verified in `stdlib_abi.h`. Use `entangle_propagation_count()`, `patch_journal_count()`, `resonate_fire_count()`, `converge_mismatch_count()`, `orchestrate_stage_count()`, `law_status()`.

### "I need testing/benchmarking/proof/certification"
→ **`use std::test`** (test runner, assert/assert_eq, outcome structs), **`use std::bench`** (benchmark task factory), **`use std::proof`** (Z3-backed proof harness → requires Python+z3-solver), **`use std::certify`** (certification gate task factory), **`use std::attrition`** (attrition runtime certification task factory). All pure Kain. Proof routes through `std::z3` → `std::python` → Python host bridge.

### "I need Python interop"
→ **`use std::python`** — ⚠️ **39 @extern are BROKEN** (no Rust registration). The basic `py_import`, `py_call`, `py_getattr`, `py_buffer`, `py_buffer_info`, `py_buffer_bytes` work (registered in `crates/python/src/lib.rs`). But async futures, actor callbacks, region caches, and buffer views are declared without implementation. Use `python_import`, `python_call`, `python_getattr` for basic interop. Use `std::python::venv` for virtual environment management (pure Kain over std::fs).

### "I need JavaScript/TypeScript interop"
→ **`use std::js`** — compiler builtins backed by `crates/node` Rust bridge (Node.js target only). No C contract. Use `js_eval`, `js_call`, `js_import`, `js_getattr`.

### "I need WASM binary parsing"
→ **`use std::wasm`** — pure Kain. WASM magic/version validation, section header parsing. No C dependency.

### "I need compiler services (LSP, hover, completions, formatting)"
→ **`use std::kain`** — 14 @extern backed by Rust bridge (`crates/service-bridge`). Not C runtime. Use `kain_service_open_workspace`, `kain_service_hover_at`, `kain_service_completions_at`, `kain_service_format_document`.

### "I need CPU feature detection, VM operations, cache info"
→ **`use std::machine`** — 24 @extern, all backed by real C in `cpu.c` + `virtual_alloc.c`. Z3-proven. Use `cpu_has_capability`, `cpu_cache_line_bytes`, `cpu_core_count`, `vm_page_size`, `vm_reserve`/`vm_commit`. Also: `lfence`/`sfence`/`mfence` (compiler inline asm), `clflush`, `asm("pause")`.

### "I need compression (gzip, deflate, ZIP, TAR, RLE)"
→ **`use std::compress`** (RLE — pure Kain), **`use std::zip`** (ZIP header serializer — pure Kain, ⚠️ no file I/O or compression), **`use std::tar`** (TAR read/write — pure Kain over std::io). All pure Kain. No C dependency.

### "I need URI parsing"
→ **`use std::uri`** — pure Kain RFC 3986 parser. `uri_parse`, `uri_encode`, `uri_decode`, query parameter iteration. Zero C dependency.

### "I need ELF binary parsing"
→ **`use std::elf`** — pure Kain. ELF64 header parser (simplified word-packed layout). No C dependency.

### "I need memory-mapped I/O utilities"
→ **`use std::mmio`** — mixed: pure Kain bitfield manipulation + C-backed volatile_load/store (via std::memory) + compiler inline asm fences. Use `mmio_read_int`, `mmio_write_int`, `mmio_write_one_to_clear`.

### "I need Markscript (docs-that-execute)"
→ **`use std::markscript`** — ⚠️ **ALL 20 FUNCTIONS ARE STUBS.** Every public function returns zero/empty. Documented as serving the kainc self-host compiler. **`use std::mks`** — 1,815 lines of pure Kain MarkScript VM (lexer, parser, bytecode compiler, interpreter). Functional. Uses std::fs for file I/O.

### "I need MCP (Model Context Protocol) server"
→ **`use std::mcp`** — pure Kain. Full protocol implementation (initialize, ping, tools/list, tools/call). Uses stdin/stdout. No C dependency.

### "I need build system DSL (build.kn authoring)"
→ **`use std::build`** — 823 lines of pure Kain. All task types (check, native_executable, test, proof, bench, attrition, certify, amalgamate, exec, gpu_suite). Builder-pattern struct DSL. No C dependency.

### "I need semantic versioning"
→ **`use std::semver`** — 799 lines of pure Kain. Full SemVer 2.0: parse, compare, range matching (^, ~, >=, <=), pre-release, wildcards. Zero C dependency.

### "I need platform detection"
→ **`use std::platform`** — 12 @extern, all backed by real C in `platform_library.c`. `platform_current_kind()`, `platform_current_name()`, `platform_library_open/resolve`. **`use std::target`** — pure Kain enum mapping over platform. Zero own @extern.

### "I need allocators (arena, bump, pool)"
→ **`use std::alloc`** — pure Kain over `alloc_zeroed`/`decay` intrinsics. BumpAllocator, ArenaAllocator, PoolAllocator, AllocatorVTable. No C dependency beyond memory intrinsics.

### "I need raw memory operations (alloc, volatile, atomics, fences)"
→ **`use std::memory`** — compiler intrinsics lowered to LLVM IR (→ memory.c/ownership.c). `alloc`, `alloc_zeroed`, `mem_load`, `mem_store`, `ptr_offset`, `bitcast`, `ptr_to_int`, `int_to_ptr`, `volatile_load`, `volatile_store`, `atomic_load`, `atomic_store`, `atomic_fence`, `lfence`, `sfence`, `mfence`. These are NOT @extern — they are compiler builtins.

---

## Module Inventory

### C CONTRACT — Wired to the Native C Runtime

| Module | @extern | C Runtime File(s) | Verdict |
|--------|:-------:|-------------------|---------|
| **std::runtime** | 27 | `stdlib_abi.c`, `simd.c`, `converge.c`, `machine_stones.c`, `cpu.c` | ✅ All verified |
| **std::time** | 2 | `stdlib_abi.c` | ✅ All verified |
| **std::os** | 38 | `os_system.c` | ✅ All verified (cross-platform; syscall stubs on Windows) |
| **std::process** | ~40 | `process_system.c` | ✅ All verified |
| **std::thread** | 4 | `core.c` | ✅ All verified |
| **std::atomic** | 3 | `core.c` (futex/SRW lock) | ✅ All verified + compiler builtins |
| **std::platform** | 12 | `platform_library.c` | ✅ All verified |
| **std::actor** | 33 | `stdlib_abi.c` | ✅ All verified |
| **std::fs** | 50+ | `stdlib_abi.c`, `core.c` | ✅ All verified; Z3 proofs on text builder |
| **std::net** | ~60 | `net_system.c` | ✅ All verified; Z3-proven overflow guards |
| **std::crypto** | 4 | `stdlib_abi.c` | ✅ Hand-rolled SHA-256, HMAC-SHA256, BLAKE3 |
| **std::graphics** | 47 | `graphics_system.c` | ✅ All verified; Vulkan/D3D12 backends catalog-only |
| **std::machine** | 24 | `cpu.c`, `virtual_alloc.c` | ✅ All verified; Z3-proven |
| **std::ui** | 83 | `ui_system.c`, `ui_host_adapter.c` | ✅ All verified; Z3-proven; **renderer ignores node tree** |
| **std::input** | 26 | `input_system.c` | ✅ All verified; CBMC-verified |
| **std::audio::device** | 9 | `audio_system.c` | ✅ All verified; smoke-tested |
| **std::intent** | 34 | `stdlib_abi.c` | ✅ All verified |

### KAIN ONLY — Pure Kain, No C Dependencies

| Module | Lines | What It Does |
|--------|:-----:|-------------|
| **std::math** | 3,100 | Vectors, matrices, quaternions, colors, noise, easing, physics, tonemapping, packing |
| **std::collections** | 797 | Array, List, Map, Set, Stack, Queue, Deque, PriorityQueue, ArrayList, HashMap, SlotMap |
| **std::text** | 422 | Search, split, join, trim, replace, case conversion, tokenization |
| **std::fmt** | 510 | Formatting, padding, hex/binary display, JSON escape, FmtWriter |
| **std::json** | 1,043 | 17 C-backed builtins + 113 pure-Kain query/validation wrappers |
| **std::hash** | 197 | FNV-1a, CRC32, xxHash, mix64, bucket selection — all deterministic |
| **std::bytes** | 196 | Byte buffers, hex encode/decode, endianness |
| **std::bits** | 108 | Bit manipulation, popcount, clz, ctz, bswap, rotl |
| **std::base64** | 156 | Base64 encode/decode, base16/hex encode/decode |
| **std::semver** | 799 | Full SemVer 2.0: parse, compare, range matching, pre-release |
| **std::unicode** | 177 | UTF-8 encode/decode; normalize is stub |
| **std::uri** | 313 | RFC 3986 URI parser: scheme, authority, path, query, fragment |
| **std::path** | 215 | Cross-platform path manipulation (join, parent, extension, normalize) |
| **std::io** | 456 | RingBuffer, StringBuilder, BufferedReader, BufferedWriter — pure Kain |
| **std::http** | 235 | HTTP convenience wrappers over std::net |
| **std::http2** | 50 | HTTP/2 API shape — capability-gated, runtime returns "not supported" |
| **std::compress** | 235 | RLE compression writer/reader |
| **std::zip** | 109 | ZIP header serializer — header-only, no file I/O or compression |
| **std::tar** | 175 | TAR archive read/write — pure Kain over std::io |
| **std::elf** | 88 | ELF64 header parser — simplified word-packed layout |
| **std::gpu** | 652 | PipelineLibrary, PipelineHandle, resource policies — policy layer |
| **std::graphics_shared** | 267 | Shared buffer/image/tensor view constructors |
| **std::simd** | 84 | I64x4 struct-based SIMD simulation |
| **std::sync** | 452 | McsMutex, RwLock, Once, WaitGroup, Semaphore, CondVar, TeleportChannel |
| **std::alloc** | 188 | BumpAllocator, ArenaAllocator, PoolAllocator, AllocatorVTable |
| **std::memory** | 72 | Compiler intrinsics: alloc, mem_load/store, volatile, atomics, fences |
| **std::result** | 63 | Integer status code convention (Ok=0, Cancelled=1, InvalidArgument=-1...) |
| **std::diagnostics** | 109 | LogLevel, LogEntry, status codes, progress, debug_dump_memory |
| **std::target** | 60 | Arch/OS/Env enums, Target struct, target_current() |
| **std::reload** | 187 | Hot-reload wrappers → delegates to std::ui hot_reload @extern |
| **std::test** | 113 | Test runner, assert/assert_eq, outcome structs |
| **std::bench** | 14 | Benchmark task factory |
| **std::proof** | 343 | Z3-backed proof harness → requires Python+z3-solver |
| **std::certify** | 11 | Certification gate task factory |
| **std::attrition** | 11 | Attrition runtime certification task factory |
| **std::build** | 823 | Build system DSL: all task types, builder-pattern structs |
| **std::z3** | 137 | Z3 solver bindings → routes through Python host bridge |
| **std::reflect** | 110 | Type reflection via to_string() |
| **std::gen_server** | 53 | Generic server actor pattern (init/handle_call/handle_cast) |
| **std::mks** | 1,815 | MarkScript VM: lexer, parser, bytecode compiler, interpreter |
| **std::mcp** | 690 | MCP protocol server (stdio JSON-RPC) |
| **std::wasm** | 120 | WASM binary format parsing (magic, version, sections) |
| **std::interop** | 195 | Shared buffer/image/tensor bridge types |
| **std::js** | 250 | JavaScript/TypeScript interop → Rust Node bridge (not C) |
| **std::no_std** | 107 | Module inclusion surface — no functions, import-only |
| **std::os_path** | 524 | Python os.path-style operations — delegates to fs + path + process |
| **std::mmio** | 100 | MMIO bitfield ops + volatile read/write — mixed (some C via memory) |

### BROKEN — @extern Declared But No Implementation

| Module | Broken Count | Details |
|--------|:-----------:|---------|
| **std::python** | **39** | Async futures, actor callbacks, region caches, buffer views — declared in python.kn but NOT registered in `crates/python/src/lib.rs`. Basic `py_import`/`py_call`/`py_getattr`/`py_buffer` WORK. |
| **std::audio::file** | **3** | FLAC/MP3/OGG @extern — declared but no C implementation. Falls back to WAV silently. |

### STUB — Functions That Are No-Ops

| Module | Stub Count | Details |
|--------|:----------:|---------|
| **std::markscript** | **20** | Every public function returns zero/empty. Documented as serving kainc self-host compiler. |
| **std::unicode** | **1** | `unicode_normalize()` returns string unmodified. |
| **std::gpu** | **5** | `gpu_pipeline_library_find` always returns id=-1. `gpu_pipeline_library_destroy` returns 0. `gpu_indirect_buffer_read` always returns zeros. |
| **std::cuda** | **18** | Device-side PTX intrinsics (`cuda_lane_id`, `cuda_ballot`, `cuda_shfl_xor`, `cuda_wmma_matmul`...) — no C contract. Must be lowered by GPU codegen backend. |

---

## What's Healthy

| Pattern | Examples | Verdict |
|---------|----------|---------|
| C contract with Z3 proofs | `std::ui`, `std::input`, `std::fs`, `std::net`, `std::machine` | Production quality |
| C contract with CBMC | `std::input` (mailbox), `std::fs` (text builder) | Exhaustively verified |
| Pure Kain, no FFI | `std::math`, `std::mcp`, `std::mks`, `std::dsp`, `std::semver` | Self-contained, portable |
| Compiler builtins, not @extern | `std::js`, `std::interop`, `std::memory`, `std::atomic` | Clean ABI boundary |

## What Needs Work

| Problem | Severity | Fix |
|---------|----------|-----|
| 39 python.kn @extern unregistered | **HIGH** | Register in `crates/python/src/lib.rs` or remove @extern declarations |
| 3 audio/file.kn FLAC/MP3/OGG @extern missing | **MEDIUM** | Implement in `audio_system.c` or remove @extern |
| markscript.kn: 20 no-op functions | **LOW** | Implement or move to `research/` |
| `unicode_normalize()` stub | **LOW** | Implement NFC/NFD/NFKC/NFKD or document as deferred |
| gpu.kn pipeline library stubs | **MEDIUM** | Implement real pipeline cache in C runtime |
| std::ui renderer ignores node tree | **HIGH** | Implement tree-walking renderer in `ui_host_adapter.c` or Vulkan ABI library |
| Component methods from JSX `{expr}` broken | **MEDIUM** | Fix `_self` auto-pass in `component.rs` codegen |
| `pulse` runtime crash | **MEDIUM** | Debug `pulse.c` |

---

## Service Key Registry

Service keys link stdlib modules to C runtime services (`runtime/native/include/services.h`):

| Service Key | Used By | Platform | Status |
|------------|---------|----------|--------|
| `base.memory` | ALL (via alloc intrinsics) | All | ✅ available |
| `memory.ownership` | alloc, collections (via decay) | All | ✅ available |
| `memory.atomic-v2` | memory (atomic ops) | All | ✅ available |
| `actor.runtime` | actor, gen_server | All | ✅ available |
| `async.runtime` | async/await | All | ✅ available |
| `io.net` | net, http, http2 | All | ✅ available |
| `io.process` | process | All | ✅ available |
| `audio.device` | audio::device | All | ✅ available |
| `audio.midi` | audio::device, audio::midi | All | ✅ available |
| `platform.app-host` | ui (window creation) | **Win32 only** | ✅ available |
| `platform.input` | input | **Win32 only** | ✅ available |
| `platform.clipboard` | ui | **Win32 only** | ⚠️ stubbed |
| `gfx.raw-native` | graphics | All | ✅ available |
| `gfx.shader.spirv` | graphics, gpu | All | ✅ available |
| `gfx.compute` | cuda | All | ✅ available |
| `gfx.compute.cuda` | cuda | All | |✅ available
| `gfx.backend.vulkan` | graphics | All | ✅ available |
| `gfx.backend.d3d12` | graphics | Win32 only | ✅ available|
| `ui.component` | ui | All | ✅ available |
| `ui.bundle` | ui | All | ✅ available |
| `cpu.capabilities` | machine | All | ✅ available |
| `machine.topology` | machine | All | ✅ available |
| `machine.virtual-memory` | machine, os | All | ✅ available |
| `machine.stones` | machine, intent | All | ✅ available |
| `data.json` | json | All | ✅ available |

---
