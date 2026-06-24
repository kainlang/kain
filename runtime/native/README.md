# Kain Native Runtime

The Kain native C runtime is the execution substrate for compiled Kain programs. It provides memory management, concurrency primitives, platform abstraction, graphics/compute services, UI hosting, crash forensics, and formal verification infrastructure ~> all in portable C11 with minimal dependencies.

**Version:** 0.1.0 · **ABI:** 0.1.0 · **Targets:** Windows, Linux, macOS

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                    UI Layer (src/ui/)                     │
│  ui_system · ui_runtime · ui_host_adapter · ui_hot_reload │
│  ui_compiled_bundle                                      │
├─────────────────────────────────────────────────────────┤
│                Platform Layer (src/platform/)              │
│  platform.c · os_system.c · platform_library.c           │
│  ┌──────────┬───────────┬────────────┐                   │
│  │  win32/   │  linux/   │  macos/    │                   │
│  │ app_host  │ shared    │ shared     │                   │
│  │ input     │ crash_hdl │ crash_hdl  │                   │
│  │ shared    │           │            │                   │
│  │ crash_hdl │           │            │                   │
│  └──────────┴───────────┴────────────┘                   │
├─────────────────────────────────────────────────────────┤
│                 Core Layer (src/core/)                     │
│                                                           │
│  ┌─────────────────────────────────────────────────┐     │
│  │         MEMORY & OWNERSHIP SUBSYSTEMS            │     │
│  │  memory.c  · arena.c  · buddy.c  · deferred_free │     │
│  │  virtual_alloc  · ownership.c  · bitfield.c      │     │
│  │  union.c  · fixup.c  · handle.c                  │     │
│  └─────────────────────────────────────────────────┘     │
│                                                           │
│  ┌─────────────────────────────────────────────────┐     │
│  │         CONCURRENCY SUBSYSTEMS                   │     │
│  │  actor.c  · async.c  · fanout.c  · batch_queue.c │     │
│  └─────────────────────────────────────────────────┘     │
│                                                           │
│  ┌─────────────────────────────────────────────────┐     │
│  │         COMPILER SEMANTIC RUNTIME                │     │
│  │  entangle.c  · converge.c  · machine_stones.c   │     │
│  │  wire.c  · core.c                                │     │
│  └─────────────────────────────────────────────────┘     │
│                                                           │
│  ┌─────────────────────────────────────────────────┐     │
│  │         SYSTEM SERVICES                          │     │
│  │  net_system.c  · process_system.c               │     │
│  │  input_system.c  · graphics_system.c            │     │
│  │  renderer_backend.c  · renderer_session.c        │     │
│  │  scene.c  · cuda_runtime.c  · simd.c            │     │
│  │  json.c  · json_benchmark.c                      │     │
│  │  ray_sphere_benchmark.c                          │     │
│  └─────────────────────────────────────────────────┘     │
│                                                           │
│  ┌─────────────────────────────────────────────────┐     │
│  │         INFRASTRUCTURE                           │     │
│  │  core.c (init/shutdown)  · crash_handler.c      │     │
│  │  version.c  · diagnostics.c  · profile.c        │     │
│  │  services.c  · contract.c  · reflection.c       │     │
│  │  compatibility.c  · host_bridge.c               │     │
│  │  cpu.c  · attrition.c  · stdlib_abi.c           │     │
│  │  interop_contracts.c  · interop_zero_copy.c     │     │
│  │  python_runtime.c  + async/buffers/gpu/region   │     │
│  └─────────────────────────────────────────────────┘     │
│                                                           │
│  ┌─────────────────────────────────────────────────┐     │
│  │         Z3 PROOF PACKS (140 proofs)             │     │
│  │  z3/proofs/*.yaml · z3/scripts/ · z3/data/      │     │
│  │  z3/reports/ · z3/generated/ · z3/cache/        │     │
│  └─────────────────────────────────────────────────┘     │
└─────────────────────────────────────────────────────────┘
```

---


## GPU Backend Architecture (Multi-Backend Layered)

Kain's GPU presentation is split into two layers per backend:

**Layer 1 — Runtime Shims (~650 lines total, in `src/core/`)**
Each backend has a thin shim that owns Kain-level protocol: capability flag,
env-var resolution, error/telemetry globals, `KainComponentSurface` vtable
shape declaration, and `dlopen` of the separately-linked ABI library.

**Layer 2 — ABI Libraries (~2,900 lines total, in `extras/<name>-abi/`)**
Each backend implements the `KainComponentSurface` vtable in a separately-linked
shared library. These libraries own the concrete GPU API calls and are dlopen'd
at runtime by the shims.

### Backends

| Backend | Shim | ABI Library | Platforms |
|---------|------|-------------|-----------|
| Vulkan | `vulkan_surface_shim.c` | `libkain-vulkan-abi.so` | Win32, Linux X11, Linux Wayland, macOS (MoltenVK) |
| D3D12 | `d3d12_surface_shim.c` | `libkain-d3d12-abi.dll` | Windows |
| WebGPU | `webgpu_surface_shim.c` | `libkain-webgpu-abi.so` | Native (wgpu-native) + WASM (browser) |

### ABI Paths

All three Kain-to-runtime paths converge through `KainComponentSurface`:
- `std::graphics` → `graphics_system.c` → delegates to component surface
- `std::ui` → `ui_host_adapter.c` → resolves component surface from registry
- `surface vulkan => Component` → compiler emits vtable calls directly

### Build Gates

| Gate | Default On | Default Off |
|------|-----------|-------------|
| `KAIN_RUNTIME_HAS_VULKAN_LOADER` | Desktop | Wasm, embedded |
| `KAIN_RUNTIME_HAS_D3D12` | Windows desktop | All others |
| `KAIN_RUNTIME_HAS_WEBGPU` | All platforms | Embedded (no GPU) |

### Raw PFN Access

The Vulkan ABI library also exposes a `KainVulkanPfnTable` (57 PFNs) via
`kain_vulkan_abi_get_vtable()->pfns` for blade-level raw Vulkan consumers
(e.g., chronosim's particle renderer) that need direct API access without
the component surface abstraction.

### Precedent

This architecture mirrors `cuda_runtime.c` (contract in runtime,
implementation in `kain-gpu-runtime.dll`). The shim owns the catalog entry and
`dlopen` plumbing; the library owns the concrete GPU calls.

## GPU Backend ABI Libraries

The `extras/` directory contains the separately-linked GPU ABI libraries:

### extras/vulkan-abi/ (~2,050 lines)

`libkain-vulkan-abi.so` / `.dll` — 43+ Vulkan PFNs dynamically resolved,
per-platform WSI surfaces (Win32/Xlib/Wayland/MoltenVK), swapchain lifecycle,
fence/semaphore-based frame submission, and all 18 `KainComponentSurface`
vtable slots. Also exposes a `KainVulkanPfnTable` with 57 resolved PFNs for
blade-level raw Vulkan consumers. Never includes `<vulkan/vulkan.h>`.

### extras/d3d12-abi/ 

`libkain-d3d12-abi.dll` — Direct3D 12 backend. Native mesh shader pipeline
support (`ShaderStage::Mesh` + `ShaderStage::Task`). Windows-only.

### extras/webgpu-abi/ (~870 lines)

`libkain-webgpu-abi.so` — WebGPU via `wgpu-native`. WASM browser fallback.
Supports WGSL subgroup intrinsics via Kain's `subgroup` keyword.

---

## Directory Layout

```
runtime/native/
├── src/
│   ├── core/              # 50+ .c files === all core runtime subsystems
│   │   ├── z3/            # Formal verification artifacts
│   │   │   ├── proofs/    #   140 YAML proof packs
│   │   │   ├── scripts/   #   8 Python automation scripts
│   │   │   ├── data/      #   runtime function catalog + findings
│   │   │   ├── reports/   #   coverage & automation reports
│   │   │   ├── generated/ #   symbol summaries & coverage
│   │   │   ├── cache/     #   tree-sitter & libclang caches
│   │   │   └── z3.toml    #   Z3 config
│   │   └── map.json       #   file-to-description mapping
│   ├── platform/           # Platform abstraction layer
│   │   ├── win32/          #   Windows: app host, input, crash handler, shared
│   │   ├── linux/          #   Linux: crash handler, shared
│   │   └── macos/          #   macOS: crash handler, shared
│   └── ui/                 # UI hosting subsystem
│       ├── ui_system.c     #   Component state, invalidation, focus, events
│       ├── ui_runtime.c    #   UI runtime lifecycle
│       ├── ui_host_adapter.c/h  # Host adapter interface
│       ├── ui_hot_reload.c #   Hot-reload of UI components
│       ├── ui_compiled_bundle.c # Compiled UI bundle loading
│       ├── ui_system_internal.h # Internal header
│       └── z3/             #   Z3 verification for UI runtime
├── include/                # 50+ headers |-> public C ABI
├── test/                   # Verification pipeline
│   ├── smoke/              #   Sanity-compile-and-run tests
│   ├── property/           #   Invariant property tests
│   ├── fuzz/               #   libFuzzer coverage-guided harnesses
│   ├── stress/             #   TSan multi-threaded stress tests
│   ├── cbmc/               #   ★ CBMC formal verification harnesses
│   │   ├── check_arena.c   #      833 assertions, all pass
│   │   ├── check_actor.c   #      5,676 assertions, all pass
│   │   ├── check_crash_handler.c
│   │   ├── check_crash_handler_linux.c
│   │   └── combined_*.c    #      Combined source+harnass files
│   └── scripts/            #   Python pipeline orchestrator
│       ├── run_pipeline.py #   Main entry point
│       ├── _common.py      #   Shared paths/helpers
│       └── data/           #   Function catalog, CBMC harnesses
├── Makefile                # Fast local dev build (clang, ASan, UBSan, TSan)
├── .gitignore
├── SERVICE_TABLE_MAPPING.md
├── kain_prefix_rename_manifest.json
├── kain_prefixed_symbol_inventory.json
└── kain_prefixed_symbol_inventory.md
```

---

## Core Layer (`src/core/`)

### Initialization & Lifecycle

| File | Purpose |
|------|---------|
| **`core.c`** | Runtime bootstrap and shutdown. Contains the global `main()` entry wrappers, RC allocator, `kain_alloc`, `rc_retain`/`rc_release`, string operations (`string_new`, `str_concat*`), array management (`array_new`, `array_push`, `array_get`), file I/O (`file_read`, `file_write`), `env()`, `cwd()`, CLI argument collection, `kain_spawn`, `kain_sleep`, stdout/stderr helpers, `to_string`, `read_line`, `stdin_read_exact`, `read_file`, `write_file`, `file_exists`/`fs_exists`/`fs_is_file`/`fs_is_dir`, map operations, `deep_eq`, `kain_chr`/`kain_ord`, `kain_parse_i64_string`, `kain_parse_f64_string`, `kain_clampd`/`kain_floor_i64`/`kain_ceil_i64`/`kain_round_i64`, and more. This is the largest single file (~3,900 lines) :: the catch-all for compiler-emitted ABI helpers and standard library glue. |
| **`version.c`** | Runtime and ABI version constants. Reports `version_get_info()`, `version_print_info()`, and `version_check_abi_compatibility()` for startup validation. |
| **`diagnostics.c`** | Structured diagnostic subsystem. Defines `KainDiagnostic` records, severity levels (Info/Warning/Error/Fatal), subsystem-specific error code ranges (1000–10999), channel filtering per tier, and the `KainDiagnosticCollector` for batched startup reporting. |
| **`profile.c`** | Scoped push/pop profiling zones with compile-out tiers (`KAIN_RUNTIME_TIER_NOOP/GATED/FULL`) and fixed-cost native timing telemetry. |
| **`services.c`** | Central service registry (the "service table"). Populated with ~35 canonical services like `base.memory`, `actor.runtime`, `gfx.raw-native`, `io.net`, `ui.component`. Supports registration, lookup, status/requirement queries, validation. Backed by alias canonicalization for legacy key compatibility. |
| **`contract.c`** | Runtime contract bundle loading and validation. |
| **`compatibility.c`** | Version validation, migration, hot reload, and snapshot flow for runtime upgrades. |

### Crash Forensics

| File | Purpose |
|------|---------|
| **`crash_handler.c`** | Cross-platform crash forensics core. Binary-searches a compiler-emitted `__kain_crash_table` (emitted when `-g` is on) to map faulting instruction pointers back to source locations (`fn_name`, `file`, `line:col`). Renders human-readable crash reports to stderr with callstack resolution through the same table. Then `_Exit(1)`. **No external dependencies** ‒ no libunwind, no libdwarf, no addr2line. |

### Memory & Ownership

| File | Purpose |
|------|---------|
| **`memory.c`** | Low-level memory helpers for compiler-emitted code. Pointer operations (`__kain_bind_local`, `__kain_addr_of`, `__kain_ptr_offset`, `__kain_field_ptr`, `__kain_index_ptr`), load/store (`__kain_mem_load`, `__kain_mem_store`), volatile byte-exact I/O (`__kain_volatile_load/store`), ordered atomic operations (load/store/add/sub/and/or/xor/exchange/CAS/fence at Relaxed through SeqCst), allocation (`__kain_alloc`, `__kain_realloc`, `__kain_free`). The allocation header (`KainAllocHeader`) embeds magic, slot token, arena ID, memtype, and flags in 8 bytes. |
| **`arena.c`** | Arena allocator with frame markers. Supports four named arenas: `MAIN`, `SHARED`, `GPU`, `SCRATCH`. Frame-based allocation with `kain_arena_alloc_lo` (bottom-up), `kain_arena_alloc_hi` (top-down), frame markers via `kain_frame_set_marker`/`kain_frame_release_to_last_marker`. Memory types model CPU/GPU visibility. **833 CBMC properties proven.** |
| **`buddy.c`** | Buddy allocator for power-of-two block sizes. Manages free lists per order, supports split/merge of blocks. |
| **`deferred_free.c`** | Deferred deallocation queue. Batches `free()` calls for cache-friendly release. |
| **`virtual_alloc.c`** | OS-level virtual memory page management. Wraps `VirtualAlloc` (Win32) and `mmap` (POSIX) for page-size allocation, deallocation, and protection changes. |
| **`ownership.c`** | Runtime backing for the Kain `collapse`/`observe`/`decay` ownership semantics. Tracks region kinds (local-alloca, heap-allocation, RC-object, world-state, entangled-authority, entangled-mirror, imported-pointer) and state machines (idle → observed → collapsed → shared → decayed). Supports helper allocation slot registration for realloc correctness, relocate tracking, and deferred decay flush. |
| **`fixup.c`** | Relocation fixup registry. Tracks allocations by handle (`KainRuntimeHandle`), registers known pointer references for self-updating relocation, and supports handle-aware reallocation tracking. |
| **`handle.c`** | Generation-tagged runtime handles (`KainRuntimeHandle`). A handle table with free-list slot allocation, magic validation, and kind-tagged resolve/rebind/release operations. Used by fixup, profile, and other subsystems requiring stale-reference rejection. |
| **`bitfield.c`** | Bit-level field access helpers for struct bitfield operations. |
| **`union.c`** | Union type-aware load/store operations for preserving bit patterns across type-punned access. |

### Concurrency

| File | Purpose |
|------|---------|
| **`actor.c`** | Full actor runtime. Spawn, mailbox (bounded/unbounded with backpressure), send/receive/try-receive, generation-tagged actor refs (`KainActorRef`), reply ports for compiler-lowered `ask`/`ask_timeout`, inline ask fast-path with borrowed payload lanes, exit reason propagation, supervision trees (OneForOne/OneForAll/RestForOne with restart policies), monitors, links, named registry, execution classes (Microcell/Worldcell/Netcell/Hostcell/Accelerator/SyntheticReplyPort), locality classes, scheduler integration with per-thread ready queue and overflow thread spawning. **5,676 CBMC properties proven.** |
| **`async.c`** | Async task/future runtime. Task lifecycle (Pending → Ready → Running → Completed/Cancelled/Failed), poll-based execution with wake handles, timers (registration, cancellation, sleep), task graphs with child-wait (ALL/ANY), continuation scheduling, dependency wait (ALL/ANY), batch lock/unlock for atomic graph mutations, and `kain_task_yield` for cooperative scheduling. |
| **`fanout.c`** | Shared-memory fanout over OS threads. Supports compiler-owned `share`/`fanout` lowering with seq-cst atomic cells for synchronization. |
| **`batch_queue.c`** | Batched message queue for efficient bulk enqueue/dequeue patterns across actor and async subsystems. |

### Compiler-Semantic Runtime

| File | Purpose |
|------|---------|
| **`entangle.c`** | World entangle registry. Tracks authority↔mirror bindings with policy and type-name metadata. Supports up to 128 bindings with `entangle_registry_register`, `entangle_registry_get`, `entangle_registry_reset`. |
| **`converge.c`** | Multi-lane dispatch telemetry and lane selection. `abi_converge_select_lane_for_key` chooses lanes by key+shape, `abi_converge_commit_winner` records the winning lane for cache affinity, `abi_converge_record_telemetry` gathers timing samples into a fixed-size ring buffer for future autotuning. Max 8 lanes, 64 telemetry samples, 64-entry tune cache. |
| **`machine_stones.c`** | The machine-stones substrate => runtime backing for the Kain `axiom`, `pulse`, `shatter`, and `teleport` constructs. Provides `kain_machine_now_ns` (high-resolution timer), `kain_machine_axiom_accept` (capability predicate), `kain_machine_pulse_start`/`pulse_snapshot`/`pulse_stop_all` (timed recurring callbacks), `kain_machine_shatter_alloc`/`lane_ptr`/`lane_base`/`free` (SoA lane buffers for SIMD-friendly layout), `kain_machine_teleport_ptr`/`teleport_note` (zero-copy cross-world handoff with telemetry). Capability bitmask includes atomics, time, shatter, teleport, and x86 SIMD ISA levels. |
| **`wire.c`** | Data wire encoding and transport layer for serialized cross-world data transfer. |

### System Services

| File | Purpose |
|------|---------|
| **`net_system.c`** | Native networking. TCP sockets, protocol-aware HTTP client/server, capability-query, Windows-first HTTPS/HTTP2 client primitives. |
| **`process_system.c`** | Native child-process management. Pipe and PTY session creation, process lifecycle, I/O redirection. |
| **`input_system.c`** | Canonical input session management. Semantic action dispatch, replay trace capture, platform event translation. |
| **`graphics_system.c`** | Catalog-free graphics kernel. Buffer management, SPIR-V shader module registration, pipeline state, draw command recording. |
| **`renderer_backend.c`** | Renderer backend identity and capability descriptors for Vulkan and DirectX 12 targets. |
| **`renderer_session.c`** | Renderer session lifecycle: frame begin/end, resource binding, command submission. |
| **`scene.c`** | Scene graph runtime. Stable scene handles, picking/raycast/bounds/visibility queries, transactional mutation requests and receipts, realtime bundle loading. |
| **`cuda_runtime.c`** | CUDA PTX dispatch bridge. Compute bundle validation, dispatch planning, and GPU runtime handoff. Status: **degraded** |-> backed by external `kain-gpu-runtime` driver lane. |
| **`simd.c`** | Runtime-published SIMD capability detection and dispatch. |

### Data & Interop

| File | Purpose |
|------|---------|
| **`json.c`** | Native JSON parse/render. Supports null, boolean, integer, float (f64), string, array, and object values. Linked-list/node-based object/array representation. |
| **`json_benchmark.c`** | Performance benchmark for JSON parse/render operations. |
| **`ray_sphere_benchmark.c`** | Ray-sphere intersection benchmark exercising the `converge` selector and `machine_stones` timer. |
| **`stdlib_abi.c`** | ABI bridge to Kain standard library constructs. Implements `abi_option_*`, `abi_result_*`, `abi_tagged_*`, `abi_future_*`, and `abi_runtime_init/shutdown/heap_validate`. Also `abi_attrition_*` checkpoint/progress/result reporting for the certification harness. |
| **`interop_contracts.c`** | Neutral shared buffer/image contracts for Python, JS, GPU, and foreign host bridge handoff. |
| **`interop_zero_copy.c`** | Zero-copy buffer materialization for interop with Python/Rust/Node host environments. |
| **`python_runtime.c`** | Python bridge base --> marshaling, object lifetime, host context management. |
| **`python_runtime_async.c`** | Python async integration ___ future conversion, event loop interop. |
| **`python_runtime_buffers.c`** | Python buffer protocol --> NumPy array views, C-contiguity checks. |
| **`python_runtime_gpu.c`** | Python GPU bridge ~~ CUDA tensor/materialization contracts. |
| **`python_runtime_region.c`** | Python bridge region caches for buffer and image materialization. |

### Attrition & Reflection

| File | Purpose |
|------|---------|
| **`attrition.c`** | The runtime certification (attrition) harness. Tracks RC allocations/frees, heap operations, clock ticks, sleep/millis, and provides `kain_attrition_*` checkpoint and result-reporting ABI for the `attrition/` pipeline. |
| **`reflection.c`** | Reflection payload loading and runtime type lookup. |
| **`host_bridge.c`** | Plugin and foreign service module integration. Supports registration of modules from foreign runtimes (Rust, Python, Node, C, Zig) against the native service registry. |
| **`cpu.c`** | Runtime-published CPU feature detection (x86 vendor strings, AVX/AVX2/AVX-512 gates, logical CPU count, thread identity, affinity controls). |

---

## Platform Layer (`src/platform/`)

### Common Files

| File | Purpose |
|------|---------|
| **`platform.c`** | Platform capability descriptor system. Defines `KainPlatformKind` (Unknown/Win32/Linux/macOS) and a bitmask of platform services (app-host, input, viewport, graphics, filesystem, process, timers, network, clipboard, hot-reload, native-library). Win32 supports all 11 services; Linux/macOS currently provide core filesystem/process/timer/network/library. Query functions: `kain_platform_current_kind()`, `kain_platform_describe_current()`, `kain_platform_require_current()`. |
| **`os_system.c`** | OS-level helper functions used across platforms: environment variable access, executable path resolution, system directory queries. |
| **`platform_library.c`** | Raw platform dynamic library loader (`dlopen`/`dlsym`/`dlclose` on POSIX, `LoadLibrary`/`GetProcAddress`/`FreeLibrary` on Windows). Used by the generated typed platform packages >> no generic public ABI call surface is exposed. |

### Windows (`src/platform/win32/`)

| File | Purpose |
|------|---------|
| **`kain_win32_app_host.c`** | Win32 application/window host substrate. Registers window class, creates and manages the message pump, handles window lifecycle. |
| **`kain_win32_input_host.c`** | Win32 input event translation. Converts `WM_KEYDOWN`/`WM_KEYUP`/`WM_MOUSEMOVE`/etc. into Kain input sessions, semantic actions, and replay traces. |
| **`win32_shared.c`** | Win32 shared helpers: environment variable get/set (string, int, double, flag), executable path discovery (`GetModuleFileNameA`), sidecar path construction, UTF-16↔UTF-8 string conversion. |
| **`crash_handler_win32.c`** | Windows Vectored Exception Handler (VEH). Registered via `AddVectoredExceptionHandler`. Handles `ACCESS_VIOLATION`, `ILLEGAL_INSTRUCTION`, `DIVIDE_BY_ZERO`, `STACK_OVERFLOW`. Performs x64 frame-pointer-based stack unwinding through the crash table. |

### Linux (`src/platform/linux/`)

| File | Purpose |
|------|---------|
| **`crash_handler_linux.c`** | Linux signal handler. Registers `SIGSEGV`, `SIGILL`, `SIGFPE`, `SIGABRT` via `sigaction` with `SA_SIGINFO`. Uses `siglongjmp`-based unwinding and matches fault addresses against the crash table. |
| **`linux_shared.c`** | Linux shared helpers: `/proc/self/exe` path resolution, `/proc/self/cmdline` arg discovery, environment helpers. |

### macOS (`src/platform/macos/`)

| File | Purpose |
|------|---------|
| **`crash_handler_macos.c`** | macOS crash handler. Registers `SIGSEGV`/`SIGILL`/`SIGFPE`/`SIGABRT` via `sigaction`. |
| **`macos_shared.c`** | macOS shared helpers: `_NSGetExecutablePath` for binary path, environment helpers. |

---

## UI Layer (`src/ui/`)

| File | Purpose |
|------|---------|
| **`ui_system.c`** | UI component system: component state management, invalidation tracking, focus routing, and event dispatch. |
| **`ui_system_internal.h`** | Internal UI system data structures not exposed in the public ABI. |
| **`ui_runtime.c`** | UI runtime lifecycle: initialization, termination, top-level component tree management. |
| **`ui_host_adapter.c` / `ui_host_adapter.h`** | Interface layer between the platform window system and the Kain UI component tree. Translates platform events into component updates. |
| **`ui_hot_reload.c`** | UI hot-reload support. Monitors compiled UI bundles for changes and triggers seamless component swaps. |
| **`ui_compiled_bundle.c`** | Loads and validates compiled UI bundle payloads (pre-compiled component trees from the Kain compiler). |

---

## Formal Verification

### CBMC Harnesses (`test/cbmc/`)

CBMC (C Bounded Model Checker) converts C code into SAT/SMT formulas and proves that no assertion violation, pointer dereference, arithmetic overflow, or undefined behavior is possible within bounded loop unwinding.

| Harness | Assertions | Status |
|---------|-----------|--------|
| **`check_arena.c`** | 833 | ✅ All pass * * * proves arena init preserves bounds, frame marker/release restores state, alloc_lo/alloc_hi regions never overlap, allocation fits in buffer. |
| **`check_actor.c`** | 5,676 | ✅ All pass -- proves queue enqueue/dequeue preserves linked-list integrity, FIFO order, capacity enforcement, NULL safety, OOM handling, bounded/unbounded mailbox invariants. |
| **`check_crash_handler.c`** | ->> | Crash table lookup edge cases. |
| **`check_crash_handler_linux.c`** | :: | Linux signal registration verification. |

Combined source+harnass files (`combined_check_*.c`) are preprocessed single-translation-unit versions for CBMC consumption.

### Z3 Proof Packs (`src/core/z3/`)

140 YAML proof packs verified with the Z3 SMT solver, stored in `z3/proofs/`. These prove mathematical invariants that CBMC's bounded approach cannot reach:

| Domain | Example Proofs |
|--------|---------------|
| **Actor** | Mailbox bounded send count stays within capacity, receive count never underflows, node cache stays bounded, reply-port rearm invalidates stale generation, scheduler queue depth no underflow, microcell ready-state is exclusive, generation-zero never stored after skip-guard |
| **Memory** | RC registry half-load preserves empty slot, array capacity doubling overflow, self-updating ptr rebind keeps pointer inside relocated range |
| **Services** | Copy text fits destination before null write |
| **Allocation** | Arena alloc lo/hi region non-overlap, alloc header magic validation |

The `z3/scripts/` pipeline automates proof lifecycle:

| Script | Purpose |
|--------|---------|
| `01_catalog_range_functions.py` | Scan core C files, catalog range/arithmetic functions |
| `02_find_sync_gaps.py` | Find synchronization gaps across runtime |
| `03_arithmetic_scanner.py` | Scan for overflow-prone arithmetic sites |
| `04_auto_z3_prover.py` | Automated Z3 proof generation |
| `05_benchmark_sync_pathways.py` | Benchmark synchronization pathways |
| `05_branch_condition_extractor.py` | Extract branch conditions for SMT modeling |
| `06_memory_order_auditor.py` | Audit memory ordering correctness |
| `07_ownership_state_machine_auditor.py` | Verify ownership state machine transitions |
| `08_abstract_concept_prover.py` | Prove abstract concept-level invariants |
| `run_pipeline.py` | Orchestrate the full Z3 pipeline |

The `z3/data/` directory contains the runtime function catalog, abstract concept findings, arithmetic sites, ownership state findings, sync findings, and sync pathway benchmarks. The `z3/cache/` directory stores tree-sitter and libclang parse caches for fast re-analysis.

---

## Verification Pipeline (`test/`)

The native runtime has a three-layer verification pipeline:

```
┌───────────────────────────────────────────────────────────┐
│  Layer 1: Sanitizer Tests  (make test / make stress)       │
│  ASan+UBSan for memory errors  ·  TSan for data races     │
│  Seconds to run  ·  Great for regression                   │
├───────────────────────────────────────────────────────────┤
│  Layer 2: Fuzz Tests  (make fuzz)                          │
│  libFuzzer + coverage-guided input generation             │
│  Minutes to run  ·  Finds edge cases                       │
├───────────────────────────────────────────────────────────┤
│  Layer 3: CBMC Formal Verification  (run_pipeline.py)      │
│  Exhaustive path exploration within unwind bound          │
│  Proves absence of UB for bounded paths                    │
└───────────────────────────────────────────────────────────┘
```

The Python pipeline at `test/scripts/run_pipeline.py` orchestrates extraction of function catalogs from C sources, CBMC verification (WSL-first on Windows with native fallback), ESBMC integration, and cross-referencing against Z3 proofs.

---

## Build System

### Makefile (Fast Local Dev)

```
make          → compile all .o files (clang, C11, -Wall -Wextra)
make lib      → static library (_build/lib/libkain_runtime.a/.lib)
make shared   → shared library (_build/lib/libkain_runtime.so/.dylib/.dll)
make test     → build + run smoke + property tests (ASan+UBSan)
make fuzz     → build libFuzzer harnesses
make stress   → build + run stress tests (TSan)
make clean    → remove build artifacts
```

### Bazel (Production Build)

The `runtime/runtime_manifest_data.bzl` manifest (auto-generated by `tools/bazel/sync_native_runtime_builds.py`) drives production Bazel builds from `runtime/native_core_runtime.toml`. `native_runtime_rules.bzl` provides helper macros including `platform_select()` for platform-conditioned source, define, and link-flag resolution.

### TOML Manifests

| File | Purpose |
|------|---------|
| **`runtime/native_core_runtime.toml`** | Canonical runtime manifest used by production Bazel builds. Lists all sources, platform-conditioned files, defines, link libraries, versions, and ~35 registered services with status/requirement/platform metadata. |
| **`runtime/native_runtime.toml`** | Compatibility mirror of the canonical manifest (older discovery paths). |
| **`runtime/native_attrition_runtime.toml`** | Runtime variant for the attrition (certification) pipeline. |
| **`runtime/native_async_benchmark_runtime.toml`** | Runtime variant for async subsystem benchmarks. |

---

## Service Table

The service registry (`services.h`/`services.c`) defines the canonical runtime contract. All subsystems declare their required and optional services here.

| Service Key | Provider | Status | Description |
|------------|----------|--------|-------------|
| `base.memory` | native-core | ✅ available | Core allocation, retain/release, memory management |
| `memory.ownership` | native-core | ✅ available | Collapse/observe/decay ownership guards |
| `memory.shared-fanout` | native-core | ✅ available | Shared-memory fanout over OS threads |
| `memory.atomic-seqcst` | native-core | ✅ available | SeqCst atomic cell ABI |
| `memory.atomic-v2` | native-core | ✅ available | Ordered atomic load/store/RMW/fence |
| `memory.volatile` | native-core | ✅ available | MMIO volatile load/store |
| `base.diagnostics` | native-core | ✅ available | Structured diagnostics and error reporting |
| `contract` | native-core | ✅ available | Runtime contract loading and validation |
| `cpu.capabilities` | native-core | ✅ available | CPU feature bits for converge selectors |
| `machine.stones` | native-core | ✅ available | Axiom, pulse, shatter, teleport runtime |
| `machine.topology` | native-core | ✅ available | CPU count, thread identity, affinity |
| `machine.virtual-memory` | native-core | ✅ available | Page map/unmap/protect |
| `control.converge.autotune` | native-core | ✅ available | Converge lane selection + telemetry |
| `control.runtime-tiers` | native-core | ✅ available | Noop/gated/full control tiers |
| `runtime.profile` | native-core | ✅ available | Scoped profiling zones |
| `memory.handles` | native-core | ✅ available | Generation-tagged runtime handles |
| `memory.fixup` | native-core | ✅ available | Relocation fixup registry |
| `data.json` | native-core | ✅ available | JSON parse/render helpers |
| `interop.shared-contracts` | native-core | ✅ available | Shared buffer/image contracts |
| `actor.runtime` | native-core | ✅ available | Actor spawn, mailbox, lifecycle, scheduling |
| `actor.registry` | native-core | ✅ available | Named actor registration |
| `async.runtime` | native-core | ✅ available | Task/future execution, wake/poll |
| `async.timers` | native-core | ✅ available | Timer registration and wake |
| `io.net` | native-core | ✅ available | TCP, HTTP, HTTPS client/server |
| `io.process` | native-core | ✅ available | Child process, pipe, PTY |
| `platform.app-host` | platform-win32 | ✅ available | Win32 app/window host (win32 only) |
| `platform.input` | platform-win32 | ✅ available | Input sessions, actions, replay (win32 only) |
| `platform.library` | native-core | ✅ available | Dynamic library open/resolve/close |
| `gfx.raw-native` | native-core | ✅ available | Buffer, SPIR-V, pipeline, draw commands |
| `gfx.shader.spirv` | native-core | ✅ available | Shader payload registration |
| `gfx.compute` | native-core | ✅ available | Compute dispatch validation and handoff |
| `gfx.compute.cuda` | native-core | ⚠️ degraded | CUDA PTX dispatch bridge |
| `gfx.backend.vulkan` | native-core | ✅ available | Vulkan backend via `vulkan_surface_shim.c` + `libkain-vulkan-abi.so` — 57 PFNs, per-platform WSI surfaces, swapchain lifecycle |
| `gfx.backend.d3d12` | native-core | ✅ available | DirectX 12 backend via `d3d12_surface_shim.c` + `libkain-d3d12-abi.dll` — native mesh shader pipeline support |
| `gfx.backend.webgpu` | native-core | ✅ available | WebGPU backend via `webgpu_surface_shim.c` + `libkain-webgpu-abi.so` — wgpu-native, WASM browser fallback, WGSL subgroup intrinsics |
| `gfx.viewport` | platform-win32 | ⚠️ degraded | Window handles and presenter attachment |
| `scene.runtime` | native-core | ✅ available | Scene handles and state access |
| `scene.query` | native-core | ✅ available | Picking, raycast, bounds, visibility |
| `scene.mutation` | native-core | ✅ available | Transactional scene mutations |
| `asset.ingestion` | native-core | ✅ available | Descriptor-driven asset entry |
| `asset.realtime` | native-core | ✅ available | Realtime bundle loading |
| `asset.gltf` | native-core | ⚠️ degraded | glTF loader removed; blade-owned loaders |
| `ui.bundle` | native-core | ✅ available | Compiled UI bundle loading |
| `ui.component` | native-core | ✅ available | Component state, invalidation, focus |
| `reflection` | native-core | ✅ available | Reflection payload + type lookup |
| `runtime.inspection` | native-core | ✅ available | Scene, resource, binding inspection |
| `device.reflection` | native-core | ✅ available | Backend, GPU, display descriptors |
| `compatibility` | native-core | ✅ available | Version check, migration, hot reload |
| `host.bridge` | native-core | ✅ available | Plugin and foreign service integration |

---

## Runtime Tiers

The `runtime_tiers.h` header defines compile-time control tiers that gate diagnostic, profiling, and fixup operations at three levels:

| Tier | Behavior | Default (Debug) | Default (Release) |
|------|----------|-----------------|-------------------|
| `KAIN_RUNTIME_TIER_NOOP` | Removed at compile time | |-> | - |
| `KAIN_RUNTIME_TIER_GATED` | Active but low-cost | ... | Diagnostics, profiling, fixup |
| `KAIN_RUNTIME_TIER_FULL` | Full instrumentation | Diagnostics, profiling, fixup | ~~ |

Subsystem tiers (`KAIN_RUNTIME_DIAG_TIER`, `KAIN_RUNTIME_PROFILE_TIER`, `KAIN_RUNTIME_FIXUP_TIER`) can be set independently via preprocessor defines or header defaults.

---

## Public Headers (`include/`)

The `include/` directory exposes the full public C ABI. Each header corresponds to one `src/core/*.c` source.

| Header | Defines |
|--------|---------|
| `base.h` | Core types: `RcHeader`, `KainArray`, `KainMap`, `MapEntry`, `MessageQueue`, `MessageNode`, `ThreadArgs`, plus portable C11 shims (`_s` functions on POSIX), `KAIN_LIKELY`/`KAIN_UNLIKELY` macros, and RC magic constants. |
| `actor.h` | Full actor ABI: `KainActorId`, lifecycle, mailbox, supervision, monitors, links, registry, reply ports, scheduler snapshots. |
| `arena.h` | Arena allocator: `KainArenaId`, `KainMemType`, frame marker API. |
| `async.h` | Async runtime: `KainTaskId`, `KainTimerId`, futures, poll, wake, task graph. |
| `attrition.h` | Attrition certification: RC tracking, heap validation, checkpoint ABI. |
| `batch_queue.h` | Batched message queue. |
| `bitfield.h` | Bit-level field access. |
| `buddy.h` | Buddy allocator. |
| `compatibility.h` | Hot reload, version migration. |
| `contract.h` | Runtime contract loading. |
| `converge.h` | Multi-lane dispatch: `abi_converge_select_lane_for_key`, telemetry. |
| `cpu.h` | CPU capability detection. |
| `crash_handler.h` | Crash forensics: `KainCrashEntry`, `__kain_crash_handler_init`, lookup, render. |
| `cuda_runtime.h` | CUDA PTX dispatch. |
| `deferred_free.h` | Deferred deallocation. |
| `diagnostics.h` | Structured diagnostics: `KainDiagnostic`, collector, startup validation. |
| `entangle.h` | World entangle registry. |
| `fanout.h` | Shared-memory fanout. |
| `fixup.h` | Relocation fixup + known-ref tracking. |
| `graphics_bundle.h` | Graphics bundle metadata. |
| `graphics_system.h` | Raw graphics kernel: buffers, shaders, pipelines. |
| `handle.h` | `KainRuntimeHandle` generation-tagged handle table. |
| `host_bridge.h` | Plugin and foreign runtime module integration. |
| `input_system.h` | Input session management. |
| `interop_contracts.h` | Shared buffer/image contracts. |
| `interop_zero_copy.h` | Zero-copy interop. |
| `json.h` | JSON parse/render. |
| `lru.h` | LRU cache. |
| `machine_stones.h` | Axiom, pulse, shatter, teleport runtime. |
| `memory.h` | Low-level memory helpers: bind, load/store, atomic, alloc. |
| `net_system.h` | Networking: TCP, HTTP. |
| `os_system.h` | OS-level helpers. |
| `ownership.h` | Collapse/observe/decay state machine. |
| `platform.h` | Platform capability descriptors. |
| `platform_library.h` | Dynamic library loader. |
| `process_system.h` | Child process management. |
| `profile.h` | Profiling zones. |
| `realtime.h` | Realtime bundle loading. |
| `reflection.h` | Reflection payload loading. |
| `renderer_backend.h` | Vulkan/D3D12 backend descriptors. |
| `renderer_session.h` | Render session lifecycle. |
| `runtime_tiers.h` | Compile-time control tiers. |
| `scene.h` | Scene graph, queries, mutations. |
| `services.h` | Service registry. |
| `simd.h` | SIMD capability detection. |
| `stdlib_abi.h` | ABI bridge for `Option`, `Result`, `Future`, `attrition`. |
| `ui_bundle.h` | UI bundle loading. |
| `ui_hot_reload.h` | UI hot-reload. |
| `ui_runtime.h` | UI runtime lifecycle. |
| `ui_system.h` | UI component system. |
| `union.h` | Union type access. |
| `version.h` | ABI/runtime version constants. |
| `virtual_alloc.h` | OS page management. |
| `vulkan_loader_subset.h` | Minimal Vulkan loader bindings. |
| `ffmpeg_version_subset.h` | Minimal FFmpeg version constants. |
| `c_runtime_math_subset.h` | Minimal C math subset for build environments without full libm. |
| `win32.h` | Win32 shared helpers. |
| `wire.h` | Data wire encoding. |
| `self_updating_ptr.h` | Self-updating pointer relocation. |
| `ray_sphere_benchmark.h` | Ray-sphere benchmark declarations. |

---

## Symbol Prefixing

The files `kain_prefix_rename_manifest.json` and `kain_prefixed_symbol_inventory.json/.md` document the systematic renaming of all public symbols with the `kain_` prefix to avoid symbol collisions when linking the runtime as a static or shared library alongside other C code.

---

## Portability

The runtime targets three platforms with a single C11 codebase:

| Platform | Compiler | Service Coverage | Key Abstractions |
|----------|----------|-----------------|-------------------|
| **Windows** | MSVC/clang-cl | Full (11/11 services) | `CRITICAL_SECTION`, `HANDLE`, `AddVectoredExceptionHandler`, `VirtualAlloc`, `WS2_32`, `WinHTTP` |
| **Linux** | GCC/clang | Core (5/11) | `pthread_mutex_t`, `pthread_cond_t`, `sigaction`, `mmap`, POSIX sockets |
| **macOS** | Apple clang | Core (5/11) | Same POSIX base as Linux |

The `base.h` header provides comprehensive POSIX→Win32 compatibility shims including `kain_fopen_s`, `kain_dupenv_s`, `kain_sleep_millis`, and `strncpy_s`/`strncat_s` replacements.
