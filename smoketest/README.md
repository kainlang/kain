# Smoketest => The Kain Album-Edition Proving Ground

> **Last updated:** 2026-06-08
> **What this is:** The **single most comprehensive Kain feature surface in the entire repo**. Every semantic layer, every effect, every compile target, and every interop lane is exercised here in one unified proof surface. This is the definitive teaching ground for LLMs, agents, and new contributors learning how Kain works end-to-end.
> **Entry point:** `src/main.kn` (40.7 KB, 741 lines)
> **Build authority:** `build.kn` (103 lines) <--> defines 9 module roots, 3 compile targets, GPU artifact generation, WASM check, telemetry runner, benchmark, attrition, and capsule certification.

______________________________________________________________________

## Table of Contents

1. [What Smoketest Proves](#1-what-smoketest-proves)
1. [Directory Layout](#2-directory-layout)
1. [How Smoketest Is Structured (The Source Lanes)](#3-how-smoketest-is-structured)
1. [How Smoketest Is Executed](#4-how-smoketest-is-executed)
1. [The Import System in Smoketest](#5-the-import-system-in-smoketest)
1. [build.kn === The Build Authority](#6-buildkn--the-build-authority)
1. [C Interop in Smoketest](#7-c-interop-in-smoketest)
1. [GPU Shader Artifacts](#8-gpu-shader-artifacts)
1. [WASM Target](#9-wasm-target)
1. [Telemetry & Evidence DAG](#10-telemetry--evidence-dag)
1. [Python Bridge (Run Orchestration)](#11-python-bridge-run-orchestration)
1. [Visualizer Bridge (OpenGL UI)](#12-visualizer-bridge-opengl-ui)
1. [Smoke Modes (full, benchmark, attrition, visual)](#13-smoke-modes)
1. [Capsule Output](#14-capsule-output)
1. [Reference Doc Map](#15-reference-doc-map)

______________________________________________________________________

## 1. What Smoketest Proves

The smoketest is an **album-edition workspace**: it collects every Kain feature into a single compiled executable that exercises them all sequentially and produces telemetry evidence. It is not a unit test suite |-> it is an **integrated semantic proof surface** that proves:

| Category | What It Proves | Files |
|----------|---------------|-------|
| **Kain's 8-layer decision ladder** | Every layer from L0 (plain `fn`) through L7 (`actor`, `collapse`/`observe`/`decay`) is exercised | `src/semantics/` (19 files) |
| **Low-level memory & ownership** | `alloc_zeroed`, `realloc_mem`, `collapse`/`observe`/`decay`, `share`/`fanout`, atomics, raw pointer math | `src/systems/` (7 files) |
| **ABI control** | `@thread_local`, `@section`, `@link_name`, `@callconv`, inline `asm()`, SIMD vectors | `src/systems/abi_control.kn` |
| **GPU compute & graphics** | Vertex/fragment shaders, compute kernels, SPIR-V emission, `comptime` metadata, `dispatch` statements | `src/gpu/` (2 files) |
| **All 65+ stdlib modules** | `std::math`, `std::fs`, `std::crypto`, `std::json`, `std::collections`, `std::time`, `std::os`, `std::platform`, `std::process`, `std::cuda`, `std::python`, `std::z3`, `std::mcp`, and more | `src/stdlib/` (32 files) |
| **C interop** | Natural `include` with companion `.c` discovery, system header registry, SQLite amalgamation binding (9.1 MB of C), visualizer bridge | `src/interop/` (3 files) + `native/` (14 C files) |
| **Python interop** | Python subprocess orchestration, Python→Kain bridge, `python_exec()`, `python_call_raw()` | `telemetry/python_bridge.kn` |
| **WASM target** | Cross-compilation to WebAssembly with `wasm_add`, `wasm_factorial`, `wasm_fibonacci` | `src/wasm/wasm_main.kn` |
| **UI components & graphics** | Component composition, JSX rendering, graphics sessions, OpenGL window presentation | `src/ui/` (2 files) |
| **Actor system** | `spawn`, `send`, `ask`, typed message handlers with reply ports | `src/semantics/actor.kn` |
| **Cross-file composition** | Every lane imports from other lanes ~ types from `types.kn`, mix functions from `converge.kn`, law validators from `law.kn`, shard scoring from `shatter.kn` | All files |
| **Telemetry evidence** | Track-level JSON reports, composition checksums, patch journal counts, converge mismatch detection | `src/telemetry/` (3 files) |

**Reference docs for understanding each feature:**

| Feature | Deep-Dive Doc | Source Dir |
|---------|--------------|------------|
| world, entangle | `docs/WORLD.MD`, `docs/ENTANGLE.MD` | `src/semantics/world.kn`, `src/semantics/entangle.kn` |
| patch, law | `docs/PATCH.MD`, `docs/LAW.MD` | `src/semantics/patch.kn`, `src/semantics/law.kn` |
| converge | `docs/CONVERGE.MD` | `src/semantics/converge.kn` |
| orchestrate | `docs/ORCHESTRATE.MD` | `src/semantics/orchestrate.kn` |
| pulse, resonate | `docs/PULSE.MD`, `docs/RESONATE.MD` | `src/semantics/pulse.kn`, `src/semantics/resonate.kn` |
| axiom, shatter, teleport | `docs/AXIOM.MD`, `docs/SHATTER.MD`, `docs/TELEPORT.MD` | `src/semantics/axiom.kn`, `src/semantics/shatter.kn`, `src/semantics/teleport.kn` |
| actor | `docs/ACTOR.MD` | `src/semantics/actor.kn` |
| ownership (collapse/observe/decay) | `docs/OWNERSHIP.MD` | `src/systems/ownership.kn`, `src/systems/memory.kn` |
| effects | `docs/EFFECTS.MD` | `src/semantics/effects.kn` |
| GPU shaders | `docs/SHADER_GPU.MD` | `src/gpu/fragment.kn`, `src/gpu/compute.kn` |
| C interop | `docs/C.MD`, `docs/C_GUIDE.MD` | `src/interop/sqlite_rally.kn`, `src/interop/c_bridge.kn`, `src/interop/c_abi_album.kn` |
| Python interop | `docs/PYTHON.MD`, `docs/PYTHON_GUIDE.MD` | `telemetry/python_bridge.kn` |
| Components & UI | `docs/COMPONENT.MD` | `src/ui/dashboard.kn` |
| Build system | `docs/BUILD_PROJECTS.MD` | `build.kn`, `build_alt.kn` |
| Systems programming | `docs/SYSTEMS_PROGRAMMING.MD` | `src/systems/` |

______________________________________________________________________

## 2. Directory Layout

```
smoketest/
├── README.md                            ← You are here
├── build.kn                             ← Build authority (103 lines)
├── build_alt.kn                         ← Alternative build graph (full .input() tree, 85 tasks)
├── KAIN.toml                            ← Compatibility metadata (C FFI config)
│
├── src/                                 ← Source root (7 categories)
│   ├── main.kn                          ← Entry point (741 lines) ~ imports all lanes, runs them
│   ├── os_basics.kn                     ← std::os probe (platform, PID, CWD, env, paths)
│   ├── rc_underflow_probe.kn            ← Reference count stress probe
│   ├── tmp_extern_probe.kn              ← Minimal @extern function probe
│   │
│   ├── semantics/                       ← L0–L7 decision ladder (19 files)
│   │   ├── types.kn                     ← Shared types: SmokePacket, SmokeLane enum
│   │   ├── control.kn                   ← Control flow: if/else/match/for/while/loop/break/continue
│   │   ├── effects.kn                   ← Effect system: Pure, IO, Async, GPU, Unsafe, Reactive
│   │   ├── option_result.kn             ← Option<T> and Result<T,E> with ? operator
│   │   ├── async_future.kn              ← async fn + await future resolution
│   │   ├── world.kn                     ← world + entangle + surface (dual-world authority/mirror)
│   │   ├── entangle.kn                  ← Field-level entangle with single_writer policy
│   │   ├── law.kn                       ← law predicates (bounds, ranges, domain invariants)
│   │   ├── patch.kn                     ← patch journaled mutation with epoch tracking
│   │   ├── resonate.kn                  ← resonate state-change tripwires with dampening
│   │   ├── actor.kn                     ← actor spawn/send/ask with typed message handlers
│   │   ├── converge.kn                  ← converge spec + fast lanes with verify random(N)
│   │   ├── orchestrate.kn              ← orchestrate multi-stage pipeline (CPU→converge→law→patch→GPU)
│   │   ├── axiom.kn                     ← axiom capability assumptions with when/guarantee/fallback
│   │   ├── shatter.kn                   ← shatter struct for SoA layout intent
│   │   ├── pulse.kn                     ← pulse timed recurrence with jitter tolerance
│   │   ├── teleport.kn                  ← teleport cross-world zero-copy value transfer
│   │   ├── comptime.kn                  ← comptime blocks for shader metadata
│   │   └── keyword_mesh.kn              ← Dense keyword-crossing stress test
│   │
│   ├── systems/                         ← Low-level memory, ownership, ABI (7 files)
│   │   ├── memory.kn                    ← alloc_zeroed, realloc_mem, mem_store, mem_load
│   │   ├── ownership.kn                ← collapse / observe / decay lifecycle
│   │   ├── share_fanout.kn             ← share + fanout parallel write lanes
│   │   ├── abi_control.kn              ← @thread_local, @section, @link_name, @callconv, asm(), SIMD
│   │   ├── vm_topology.kn              ← VM/intrinsic topology probe
│   │   ├── mmio_interrupt.kn           ← MMIO and interrupt semantics
│   │   └── native_cli.kn               ← Native CLI helper imports
│   │
│   ├── gpu/                             ← GPU shaders (2 files)
│   │   ├── fragment.kn                  ← Vertex + fragment shaders (SmokeVertex, SmokeGradient, SmokeVignette)
│   │   └── compute.kn                   ← Compute kernels (SmokeParticleStep, SmokeReductionKernel, SmokeOrchestrateKernel)
│   │
│   ├── interop/                         ← C interop lanes (3 files)
│   │   ├── sqlite_rally.kn              ← Physical `include` sites for SQLite amalgamation
│   │   ├── c_bridge.kn                  ← Low-level C bridge pressure lane
│   │   └── c_abi_album.kn               ← High-level ABI album composition wrapping C calls
│   │
│   ├── stdlib/                          ← Stdlib module test lanes (32 files)
│   │   ├── ascii_lane.kn, base64_lane.kn, bytes_lane.kn, collections_lane.kn
│   │   ├── crypto_lane.kn, alloc_lane.kn, diagnostics_lane.kn, fs_lane.kn
│   │   ├── z3_lane.kn, json_lane.kn, math_lane.kn, cuda_lane.kn, cuda_artifact_probe.kn
│   │   ├── interop_lane.kn, python_async_lane.kn, python_bridge_arrays_lane.kn
│   │   ├── os_lane.kn, platform_lane.kn, process_lane.kn, input_lane.kn
│   │   ├── reload_lane.kn, text_lane.kn, time_lane.kn, unicode_lane.kn
│   │   ├── random_lane.kn, uri_lane.kn, semver_lane.kn, sync_lane.kn
│   │   ├── io_lane.kn, meta_lane.kn, thread_lane.kn, mcp_lane.kn
│   │
│   ├── ui/                              ← UI components & graphics (2 files)
│   │   ├── dashboard.kn                 ← UI album snapshot: graphics session, draw commands, frame hashing
│   │   └── presenter.kn                 ← OpenGL visualizer bridge: viz_probe(), viz_run_window(), viz_write_report()
│   │
│   ├── wasm/                            ← WASM target (1 file)
│   │   └── wasm_main.kn                 ← wasm_add, wasm_factorial, wasm_fibonacci compiled to .wasm
│   │
│   └── telemetry/                       ← Evidence & flow system (3 files)
│       ├── report.kn                    ← Telemetry report writing: JSON track files, mode detection, output root
│       ├── flow.kn                      ← Album flow runner: full/benchmark/attrition mode dispatch
│       └── headless_host.kn            ← Headless UI host probe (ui_reset → session → reconcile → frame → submit)
│
├── native/                              ← C companion sources (14 files)
│   ├── sqlite3.h / sqlite3.c            ← SQLite 3 amalgamation (9.1 MB .c, 675 KB .h)
│   ├── sqlite3ext.h                     ← SQLite extension API header
│   ├── smoketest_sqlite_pingpong.h/.c   ← SQLite ping-pong test wrapper (10.4 KB .c)
│   ├── smoketest_visualizer_bridge.h/.c ← OpenGL visualizer bridge (32.9 KB .c)
│   ├── nuklear.h                        ← Nuklear immediate-mode UI library (247 KB)
│   └── uthash.h, utarray.h, utlist.h, utringbuffer.h, utstack.h, utstring.h ← UT hash/array/list utilities
│
├── telemetry/                           ← Run orchestration & output (3 source files + 4 mode dirs)
│   ├── run_smoketest_mode.kn            ← CLI arg parser for mode/executable/output_dir
│   ├── python_bridge.kn                 ← Python subprocess orchestration: installs Python runner, launches smoketest.exe
│   ├── invoke_kain.ps1                  ← Kain binary resolution (Bazel → .kain/bin → Cargo → PATH)
│   ├── full/                            ← Full mode output directory
│   ├── benchmark/                       ← Benchmark mode output
│   ├── attrition/                       ← Attrition mode output
│   └── visual/                          ← Visual mode output (UI dashboard note, OpenGL album note)
│
├── build-smoketest-visualizer-bridge.ps1 ← Compiles smoketest_visualizer_bridge.c → .obj via clang
├── run-visual-smoketest.ps1             ← Visual mode wrapper: compiles bridge, compiles main.kn, runs with KAIN_SMOKETEST_MODE=visual
│
├── kain_shader_bundle.json              ← SPIR-V shader bundle manifest (25.5 KB)
├── kain_compute_residency.json          ← GPU compute residency metadata (5.2 KB)
├── kain_gpu_runtime.dll                 ← GPU runtime DLL (1.7 MB)
│
├── smoketest.exe                        ← Built native executable (4.6 MB)
├── smoketest.kn                         ← Editable source capsule (11.4 MB, emitted after certify)
├── smoketest.artifacts.kn               ← Artifact capsule (54.5 MB)
├── smoketest.evidence.kn                ← Evidence capsule (113.5 KB)
│
├── manual-smoketest.exp/.lib            ← Export/lib lists
├── manual-smoketest.runtime_contract.json ← Full runtime contract (522.6 KB) => required capabilities, resource bindings
├── manual-smoketest.realtime_app.json   ← Realtime application descriptor (10.7 KB)
│
├── generated/
└── vendor/                              ← Vendor source dependency roots (sqlite-src manifest) (SQLite manifest)
```

______________________________________________________________________

## 3. How Smoketest Is Structured (The Source Lanes)

### 3.1 Semantic Lanes (L0–L7 of the Decision Ladder)

Every semantic layer from Kain's 8-layer decision ladder (`docs/RULEBOOK.md`) is tested in `src/semantics/`. The lanes are ordered by layer:

| File | Layer | Keyword(s) Tested | What Happens |
|------|-------|-------------------|-------------|
| `types.kn` | L0 | `struct`, `enum` | Defines `SmokePacket` and `SmokeLane` enum shared across ALL other lanes. |
| `control.kn` | L0 | `if`, `else`, `elif`, `match`, `for`, `while`, `loop`, `break`, `continue` | Control flow expression semantics |
| `effects.kn` | L0 | `Pure`, `IO`, `Async`, `GPU`, `Reactive`, `Unsafe` | Function effect annotations |
| `option_result.kn` | L0 | `Option<T>`, `Result<T,E>`, `?` | Sum type unwrapping |
| `async_future.kn` | L0 | `async fn`, `await` | Async function + future resolution |
| `world.kn` | **L1** | `world`, `surface`, `native_ui`, `web` | Creates `SmokeAuthority` world + `SmokeMirror` world with UI surfaces |
| `entangle.kn` | **L1** | `entangle`, `single_writer` | Field-level state coupling between worlds |
| `law.kn` | **L2** | `law` | Bounds/domain invariant predicates (`smoke_validate_range`) |
| `patch.kn` | **L2** | `patch` | Journaled world state mutation with epoch increment |
| `resonate.kn` | **L5** | `resonate`, `dampen` | Reactive state-change tripwires |
| `actor.kn` | **L7** | `actor`, `spawn`, `send`, `on`, `ask` | Typed message-passing concurrency (`SmokeRelay` actor) |
| `converge.kn` | **L3** | `converge`, `spec`, `fast`, `verify`, `random`, `when`, `target` | Multi-lane dispatch with spec + fast lanes |
| `orchestrate.kn` | **L4** | `orchestrate`, `stage`, `after`, `deps`, `residency`, `transfer`, `guarded`, `requires`, `policy`, `fallback` | Multi-stage pipeline with CPU/converge/law/patch/GPU stages |
| `axiom.kn` | **L6** | `axiom`, `when`, `guarantee`, `fallback`, `target`, `capability` | Machine capability assumptions |
| `shatter.kn` | **L6** | `shatter struct` | SoA layout intent (`SmokeShard`) |
| `pulse.kn` | **L5** | `pulse`, `every`, `jitter` | Timed recurrence beats |
| `teleport.kn` | **L6** | `teleport`, `from`, `to`, `via` | Cross-world zero-copy value transfer |
| `comptime.kn` | L0 | `comptime` | Compile-time shader metadata blocks |
| `keyword_mesh.kn` | Mixed | ~80+ keywords | Dense keyword-crossing stress test |

### 3.2 Systems Lanes (Low-Level & Ownership)

| File | What It Tests |
|------|-------------|
| `memory.kn` | `alloc_zeroed`, `realloc_mem`, `mem_store`, `mem_load` * * * raw pointer allocation/read/write |
| `ownership.kn` | Full `collapse`/`observe`/`decay` lifecycle on heap-allocated AND stack-imported pointers. Cross-calls to `memory.kn` (alloc) and `converge.kn` (mix) |
| `share_fanout.kn` | `share` + `fanout` with 4 parallel workers, `atomic_store`, cross-calls to `keyword_mesh.kn`, `law.kn`, `types.kn` |
| `abi_control.kn` | `@thread_local` + `@section` TLS constants, `@link_name` + `@callconv` custom ABI symbols, `asm()` with constraints/clobbers, SIMD `i64x4` vectors |
| `vm_topology.kn` | VM/intrinsic topology probe |
| `mmio_interrupt.kn` | MMIO and interrupt semantics |
| `native_cli.kn` | Native CLI helper imports |

### 3.3 GPU Lanes

| File | What It Tests |
|------|-------------|
| `fragment.kn` | `shader vertex` (SmokeVertex: position+offset+UV → clip space), `shader fragment` (SmokeGradient, SmokeVignette: UV→color with `uniform` bindings), `vec3`, `vec4` math |
| `compute.kn` | `shader compute` (SmokeParticleStep, SmokeReductionKernel, SmokeOrchestrateKernel), `StorageBuffer<T>` bindings, `comptime` metadata blocks with dispatch-size + resource descriptors, `UniformType`/`ResourceDescriptor` shapes |

**GPU artifact output:**

- SPIR-V module: `SmokeParticleStep__SmokeReductionKernel__SmokeOrchestrateKernel__SmokeVertex__SmokeGradient__SmokeVignette` (5796 bytes, see `kain_shader_bundle.json:3`)
- 6 entry points: `SmokeParticleStep`, `SmokeReductionKernel`, `SmokeOrchestrateKernel`, `SmokeVertex`, `SmokeGradient`, `SmokeVignette`
- Residency sidecars at `kain_compute_residency_shader_*.bin` (6 binary files)

For GPU semantics, see `docs/SHADER_GPU.MD` and the compute residency JSON at `kain_compute_residency.json`.

### 3.4 Interop Lanes (C + Python)

**Three-tier C interop architecture:**

| File | Role | How It Works |
|------|------|-------------|
| `sqlite_rally.kn` | **Physical include site** ~> the ONLY file that contains `include` directives | `include "../../native/sqlite3.h" as sql` + `include "../../native/smoketest_sqlite_pingpong.h" as ping` ~~ produces `sql_*` and `ping_*` extern surfaces |
| `c_bridge.kn` | **Low-level pressure lane** >> `use`-imports from `sqlite_rally` | Calls `sql_*`/`ping_*` functions directly, builds `SmokePacket` from results |
| `c_abi_album.kn` | **High-level album composition** :: wraps C calls in Kain semantics | Combines C results with `converge::smoke_mix_pair`, `law` validation, `SmokePacket` construction |

**C companion files in `native/`:**

| File | Size | Discovered By |
|------|------|-------------|
| `sqlite3.c` | 9.1 MB | Auto-discovered as sibling of `sqlite3.h` (see `docs/C_GUIDE.MD` § Strategy 4) |
| `smoketest_sqlite_pingpong.c` | 10.4 KB | Auto-discovered via the `[c_ffi.libraries]` entry in `KAIN.toml:29-32` |
| `smoketest_visualizer_bridge.c` | 32.9 KB | Auto-discovered via `KAIN.toml:34-38` -- compiled separately via `build-smoketest-visualizer-bridge.ps1` |

For the full C interop architecture, see `docs/C.MD` (1912 lines --> 4-layer stack: parser → libclang extraction → runtime bridge → codegen backends). For the usage guide, see `docs/C_GUIDE.MD` (13 strategies from eliminate-the-bridge to full Win32 GDI apps).

**Python interop** is handled in `telemetry/python_bridge.kn`:

- Uses `use std::python` and `std::collections`
- Installs a Python runner via `python_exec("import os\nimport pathlib\n...")` ~ see line 18 of `python_bridge.kn`
- The Python runner spawns `smoketest.exe` as a subprocess with env vars (`KAIN_SMOKETEST_MODE`, `KAIN_SMOKETEST_OUTPUT_DIR`)
- Calls `python_call_raw()` and `python_call_attr_raw()` for Python object manipulation
- See `docs/PYTHON.MD` and `docs/PYTHON_GUIDE.MD` for the full Python interop reference

### 3.5 Stdlib Lanes

27 test lanes in `src/stdlib/` exercise the full stdlib surface. Each file exports a `smoke_<module>_lane()` function that is imported by `src/main.kn`. For the complete stdlib reference, see `docs/STDLIB.md` (67 modules, ~3,250 public symbols).

### 3.6 UI Lanes

| File | What It Tests |
|------|-------------|
| `dashboard.kn` | `std::graphics` session create/destroy, draw commands, frame hashing, `SmokeUiAlbumSnapshot` struct that captures graphics state |
| `presenter.kn` | OpenGL visualizer bridge: `include "../../native/smoketest_visualizer_bridge.h" as viz` → `viz_probe()`, `viz_run_window()`, `viz_frames_presented()`, `viz_write_report()` |

### 3.7 WASM Target

`src/wasm/wasm_main.kn` is a standalone entry point compiled with `--target wasm`. It proves:

- `wasm_add(17, 25) == 42`
- `wasm_factorial(5) == 120`
- `wasm_fibonacci(10) == 55`
- No imports from the rest of the album * * * pure computation

### 3.8 Telemetry Lanes

| File | What It Does |
|------|-------------|
| `report.kn` | Telemetry report infrastructure: `smoke_telemetry_mode()` reads `KAIN_SMOKETEST_MODE` env var, `smoke_telemetry_prepare()` creates output directories, `smoke_write_track_report()` writes per-track JSON files with checksums, `smoke_write_note_report()` writes notes |
| `flow.kn` | Album flow engine: defines `smoke_telemetry_flow_lane()` which runs all lanes, records track reports, computes composition checksums. Also defines `smoke_run_benchmark_mode()` and `smoke_run_attrition_mode()` for batch execution |
| `headless_host.kn` | Headless UI host: `ui_reset()` → `ui_host_session_create()` → `ui_reconcile_node()` → `ui_frame_begin()` → `ui_render_box()` → `ui_frame_submit()` → `ui_host_present()` |

______________________________________________________________________

## 4. How Smoketest Is Executed

### 4.1 Build Pipeline

The build graph in `build.kn` defines this DAG of tasks (topological order):

```
build.kn fn build()
    │
    ├── check-llvm          ← Typecheck all LLVM sources
    │     ├── requires: (nothing)
    │     └── produces: check pass/fail
    │
    ├── gpu-artifacts       ← Compile SPIR-V + CUDA + HLSL from shader sources
    │     ├── requires: check-llvm
    │     └── produces: kain_shader_bundle.json, .bin residency files
    │
    ├── check-wasm           ← Typecheck WASM entry point
    │     ├── requires: (nothing)
    │     └── produces: check pass/fail
    │
    ├── source-tests         ← Run inline source tests via kain test
    │     ├── requires: check-llvm
    │     └── produces: test pass/fail
    │
    ├── check-telemetry-runner ← Typecheck the Python bridge runner
    │     ├── requires: (nothing)
    │     └── produces: check pass/fail
    │
    ├── root-executable      ← Compile main.kn → smoketest.exe
    │     ├── requires: check-llvm, gpu-artifacts, check-wasm, source-tests
    │     └── produces: smoketest.exe (4.6 MB)
    │
    ├── album-full           ← Run full smoke mode
    │     ├── requires: root-executable, check-telemetry-runner
    │     ├── runs: telemetry/invoke_kain.ps1 → python_bridge → smoketest.exe --mode full
    │     └── produces: telemetry/full/ (track JSONs + summary)
    │
    ├── album-benchmark      ← Run benchmark mode (256 rounds, 6 passes)
    │     ├── requires: root-executable, check-telemetry-runner
    │     └── produces: telemetry/benchmark/
    │
    ├── album-attrition      ← Run attrition mode (20 ops, 96 rounds)
    │     ├── requires: root-executable, check-telemetry-runner
    │     └── produces: telemetry/attrition/
    │
    ├── smoketest.local      ← Certify: all tasks passed
    │     ├── requires: check, runner, gpu, wasm, tests, exe, full, bench, abuse
    │     └── produces: certification evidence
    │
    └── smoketest capsule set ← Pack sources + artifacts + evidence
          ├── after: certify
          └── produces: smoketest.kn (11.4 MB), smoketest.artifacts.kn (54.5 MB), smoketest.evidence.kn (113.5 KB)
```

### 4.2 Run Flow

The `main()` function in `src/main.kn` (line 121+) determines the run mode:

```
smoketest.exe
    │
    ├── mode = env("KAIN_SMOKETEST_MODE") or "visual" (default)
    │
    ├── mode == "full":
    │     runtime_init() → smoke_telemetry_prepare("full")
    │     → Run ALL lanes (semantics + systems + gpu + stdlib + interop + UI + telemetry)
    │     → smoke_write_summary_report(...)
    │     → smoke_write_note_report(...)
    │
    ├── mode == "benchmark":
    │     → Read KAIN_SMOKETEST_BENCH_ROUNDS / KAIN_SMOKETEST_BENCH_PASSES
    │     → run smoke_run_benchmark_mode() from flow.kn
    │     → Re-run lanes multiple times, record per-round track reports
    │
    ├── mode == "attrition":
    │     → Read KAIN_SMOKETEST_ATTRITION_OPS / KAIN_SMOKETEST_ATTRITION_ROUNDS
    │     → run smoke_run_attrition_mode() from flow.kn
    │     → Stress lanes with repeated create/destroy cycles
    │
    └── mode == "visual":
          → runtime_init() → smoke_ui_album_lane() → smoke_opengl_album_lane()
          → Writes ui_dashboard.json and opengl_album.json notes
```

### 4.3 Execution From Command Line

```powershell
# Build only
kain build                        # Uses build.kn
kain build --target llvm          # Explicit target

# Typecheck only
kain check src/main.kn --json

# Run full smoke
$env:KAIN_SMOKETEST_MODE = "full"
kain run src/main.kn --target llvm

# Run visual mode (requires OpenGL bridge compiled first)
.\build-smoketest-visualizer-bridge.ps1
.\run-visual-smoketest.ps1

# Run benchmark
$env:KAIN_SMOKETEST_MODE = "benchmark"
$env:KAIN_SMOKETEST_BENCH_ROUNDS = "256"
$env:KAIN_SMOKETEST_BENCH_PASSES = "6"
kain run src/main.kn --target llvm

# Build + certify + capsule
kain build                        # Runs the full DAG including certify and capsule tasks
```

______________________________________________________________________

## 5. The Import System in Smoketest

### 5.1 Module Resolution Architecture

Kain uses **filesystem-native module resolution**. Every `use X::Y` maps to a filesystem path. The smoketest project has **9 module roots** (see `build.kn:5-13`):

```kn
const ALBUM_ROOTS = [
    "src/semantics",   // L0-L7 semantic lanes
    "src/systems",     // Low-level memory/ownership/ABI
    "src/gpu",         // GPU shader kernels
    "src/stdlib",      // Stdlib module test lanes
    "src/interop",     // C interop lanes
    "src/wasm",        // WASM entry point
    "src/telemetry",   // Evidence & flow system
    "src/ui",          // UI components & graphics
]
```

These are passed as `.module_roots(ALBUM_ROOTS)` to both the `project()` spec and the `BladeSpec`.

### 5.2 Import Categories

**A. Stdlib imports** >> `use std::<module>`:

```kn
use std::runtime      // → resolved from $KAIN_STDLIB_PATH/runtime.kn
use std::intent       // → resolved from $KAIN_STDLIB_PATH/intent.kn
use std::actor        // → resolved from $KAIN_STDLIB_PATH/actor.kn
use std::memory       // → resolved from $KAIN_STDLIB_PATH/memory.kn
use std::math         // → resolved from $KAIN_STDLIB_PATH/math.kn
use std::fs           // → resolved from $KAIN_STDLIB_PATH/fs.kn
use std::cuda         // → resolved from $KAIN_STDLIB_PATH/cuda.kn
use std::python       // → resolved from $KAIN_STDLIB_PATH/python.kn
```

**B. Cross-lane imports** ___ `use <lane_dir>::<symbol>`:

```kn
use types::SmokePacket           // resolves to src/semantics/types.kn (module root: src/semantics)
use converge::smoke_mix          // resolves to src/semantics/converge.kn
use law::smoke_validate_range    // resolves to src/semantics/law.kn
use memory::smoke_alloc_cells    // resolves to src/systems/memory.kn
use sqlite_rally::smoke_sqlite_version  // resolves to src/interop/sqlite_rally.kn
use report::smoke_telemetry_prepare     // resolves to src/telemetry/report.kn
use dashboard::SmokeUiAlbumSnapshot     // resolves to src/ui/dashboard.kn
```

Resolution order (from `docs/BUILD_PROJECTS.MD` § 8):

1. Local filesystem relative to importing file and its parents
1. Blade module roots (from `.module_roots(ALBUM_ROOTS)` in `build.kn`)
1. Stdlib roots (from `$KAIN_STDLIB_PATH`)
1. Installed packages

So `use types::SmokePacket` from `src/main.kn` resolves:

1. `<project>/src/semantics/types.kn` ← **found here** (via module root "src/semantics")

**C. C interop imports** ___ `include ... as ...`:

```kn
include "../../native/sqlite3.h" as sql           // → sql_libversion_number(), sql_threadsafe(), etc.
include "../../native/smoketest_sqlite_pingpong.h" as ping  // → ping_score(), ping_signature(), etc.
include "../../native/smoketest_visualizer_bridge.h" as viz // → viz_probe(), viz_run_window(), etc.
```

These are declared ONLY in `sqlite_rally.kn:8-9` and `presenter.kn:1` === all other files import the Kain wrappers via `use`:

```kn
// c_bridge.kn imports from sqlite_rally.kn (the Kain facade)
use sqlite_rally::smoke_sqlite_version
use sqlite_rally::smoke_sqlite_ping_score
// Never raw "use c::sqlite3::sqlite3_libversion_number"
```

### 5.3 The Two-Layer C Import Pattern

This is the canonical pattern for C interop throughout the repo:

```
Layer 1: sqlite_rally.kn          ← Physical include sites ONLY
  include "../../native/sqlite3.h" as sql
  → Exposes: sql_libversion_number(), sql_threadsafe(), etc.

Layer 2: c_bridge.kn / c_abi_album.kn  ← Kain semantic wrappers
  use sqlite_rally::smoke_sqlite_version   (NOT a C include)
  → Calls into sql_* functions through the Kain facade
```

The `smoketest_sqlite_pingpong.h` companion C (10.4 KB) provides:

- `ping_score()`, `ping_signature()`, `ping_row_count()`, `ping_text_bytes()`, `ping_bounce()`, `ping_hot()` <--> SQLite-backed ping-pong operations

For the full C interop pattern reference, see `docs/C_GUIDE.MD` § Strategy 5 (The Kain Facade Pattern) and `docs/C.MD` § 3.2-3.4 (include syntax forms).

### 5.4 Cross-File Dependency Graph (Example)

Here is a real import trace through the smoketest:

```
src/main.kn
  └─ use actor::smoke_actor_lane                    → src/semantics/actor.kn
       ├─ use std::runtime                          → <stdlib>/runtime.kn
       ├─ use std::actor                            → <stdlib>/actor.kn
       ├─ use types::SmokePacket                    → src/semantics/types.kn
       └─ use types::smoke_weighted_checksum        → src/semantics/types.kn

  └─ use interop_lane::smoke_c_bridge_lane          → src/interop/c_bridge.kn
       ├─ use sqlite_rally::smoke_sqlite_version     → src/interop/sqlite_rally.kn
       │    └─ include "../../native/sqlite3.h" as sql   → native/sqlite3.h (9.1 MB C)
       │         └─ [compiler discovers native/sqlite3.c as companion]
       │         └─ [libclang extracts SQLite function declarations]
       │         └─ [generates sql_* extern wrappers]
       └─ use types::SmokePacket                    → src/semantics/types.kn

  └─ use flow::smoke_telemetry_flow_lane            → src/telemetry/flow.kn
       ├─ use shatter::SmokeShard                   → src/semantics/shatter.kn
       ├─ use converge::smoke_mix_pair              → src/semantics/converge.kn
       ├─ use c_abi_album::smoke_c_abi_album_score  → src/interop/c_abi_album.kn
       │    └─ use sqlite_rally::smoke_sqlite_ping_score  → src/interop/sqlite_rally.kn
       ├─ use report::smoke_write_track_report      → src/telemetry/report.kn
       └─ use law::smoke_validate_range             → src/semantics/law.kn
```

______________________________________________________________________

## 6. build.kn -- The Build Authority

### 6.1 Overview

`build.kn` (103 lines) is the **sole build authority** for the smoketest. It exports a `fn build(ctx: BuildContext) -> BuildGraph` that defines every task in the build DAG. The `KAIN.toml` file is vestigial - it carries only C FFI compatibility metadata (`docs/BUILD_PROJECTS.MD` § 1).

### 6.2 Project Configuration

```kn
let album = project("smoketest")
    .kind("kain_executable")
    .version("0.1.0")
    .description("Album-edition workspace covering every Kain feature in one proof surface.")
    .entry("src/main.kn")
    .source_roots(ALBUM_ROOTS)       // 9 source roots
    .module_roots(ALBUM_ROOTS)       // 9 module roots for import resolution
    .generated_root("telemetry")
    .targets("llvm", "spirv", "wasm") // Multi-target: native + GPU + web
    .artifact_root(".kain/out")
    .cache_root(".kain/cache/build")
```

### 6.3 Source Set

```kn
let sources = source_set("album-sources")
    .root("src")
    .glob("src/**/*.kn")
    .glob("telemetry/**/*.kn")
    .glob("native/**/*.{h,c}")       // C companion files tracked for invalidation
    .file("KAIN.toml")
    .file("vendor/sqlite-src/manifest.uuid")
    .file("build-smoketest-visualizer-bridge.ps1")
    .file("run-visual-smoketest.ps1")
```

Note: `.glob("native/**/*.{h,c}")` puts C sources in the build edge for change detection, but C files are NOT compiled by Kain ~~ they are discovered by the C-FFI pipeline and linked into the final binary.

### 6.4 Task: LLVM Check

```kn
let check = check_task("check-llvm")
    .project(album)
    .target("llvm")
    .inputs(sources)
    .telemetry("llm.evidence", "smoketest.album")
```

Typechecks all sources against the LLVM target with structured telemetry output.

### 6.5 Task: GPU Artifacts

```kn
let gpu = gpu_suite("gpu-artifacts")
    .fragment("src/gpu/fragment.kn")      // Vertex + fragment shaders
    .compute("src/gpu/compute.kn")        // Compute kernels
    .targets("spirv", "cuda", "hlsl")     // All three GPU targets
    .artifact_root(".kain/out/gpu")
    .requires("check-llvm")
```

Generates SPIR-V binary modules, CUDA PTX, and HLSL shader text from Kain shader sources. Output goes to `.kain/out/gpu/`.

### 6.6 Task: WASM Check

```kn
let wasm = check_task("check-wasm")
    .entry("src/wasm/wasm_main.kn")
    .target("wasm")
    .telemetry("llm.wasm")
```

### 6.7 Task: Source Tests

```kn
let tests = source_tests("source-tests")
    .project(album)
    .inputs(sources)
    .requires("check-llvm")
```

### 6.8 Task: Telemetry Runner

```kn
let runner = kain_runner("check-telemetry-runner")
    .entry("telemetry/run_smoketest_mode.kn")
    .target("interpret")
    .inputs(VISUAL_RUNNER_INPUTS)
    .telemetry("smoketest.python-ffi")
```

Runs the Python bridge in interpret mode to verify it typechecks.

### 6.9 Task: Native Executable

```kn
let exe = native_executable("root-executable")
    .project(album)
    .output("$blade/smoketest.exe")
    .requires(check, gpu, wasm, tests)
```

`$blade` resolves to the blade output root. The executable depends on check + gpu + wasm + tests all passing first.

### 6.10 Smoke Mode Task Factory

```kn
fn smoke_mode(name: String, args: Array<String>) -> BuildTask:
    return album_mode("album-" + name)
        .mode(name)
        .runner("telemetry/run_smoketest_mode.kn")
        .executable("$root/smoketest.exe")
        .output_dir("$root/telemetry/" + name)
        .inputs(VISUAL_RUNNER_INPUTS)
        .args(args)
        .requires("root-executable")
        .requires("check-telemetry-runner")
        .telemetry("smoketest." + name)

let full = smoke_mode("full", [])
let bench = smoke_mode("benchmark", ["--bench-rounds", "256", "--bench-passes", "6"])
let abuse = smoke_mode("attrition", ["--attrition-ops", "20", "--attrition-rounds", "96"])
```

This is a **custom task factory function** ~> a Kain function that returns a `BuildTask`. This keeps the build graph DRY: all three smoke modes share the same runner, executable, and inputs, differing only in mode name and CLI args.

### 6.11 Certification

```kn
let cert = certify("smoketest.local")
    .requires(check, runner, gpu, wasm, tests, exe, full, bench, abuse)
```

A meta-task: it runs after everything else and attests the ENTIRE pipeline passed.

### 6.12 Capsule Set

```kn
let capsules = capsule_set("smoketest")
    .after(cert)
    .source("$root/smoketest.kn")
    .artifacts("$root/smoketest.artifacts.kn")
    .evidence("$root/smoketest.evidence.kn")
    .tag("portable")
    .tag("smoketest")
    .telemetry("smoketest.capsule")
```

After certification passes, the entire project is packed into three portable capsule files.

### 6.13 Path Interpolation

| Prefix | Resolves To (in smoketest context) |
|--------|-----------------------------------|
| `$root` | `X:/smoketest/` (workspace root where build.kn lives) |
| `$blade` | `X:/smoketest/.kain/out/` (blade output root) |
| `$task` | `X:/smoketest/.kain/cache/build/<task-id>/` (task cache) |

### 6.14 build_alt.kn (Alternative Build Graph)

`build_alt.kn` (85 tasks, 500+ lines) is the **pre-DSL-era build graph** ~> it spells out every `.input()` explicitly instead of using `source_set()` + `.inputs()`. Every source file is listed individually. It exists as a reference for the explicit build manifest pattern but is **not the active build authority**. The active build authority is `build.kn`.

For the full build system reference, see `docs/BUILD_PROJECTS.MD` (1694 lines --> complete build.kn DSL, all task types, all CLI commands, module resolution rules, path prefixes).

______________________________________________________________________

## 7. C Interop in Smoketest

### 7.1 Architecture

The smoketest exercises all three forms of C interop:

| Form | Example | Resolution |
|------|---------|-----------|
| **Quoted local include** | `include "../../native/sqlite3.h" as sql` | Filesystem: finds `native/sqlite3.h` relative to the importing `.kn` file |
| **Quoted local include** | `include "../../native/smoketest_visualizer_bridge.h" as viz` | Filesystem: finds `native/smoketest_visualizer_bridge.h` |
| **System angle-bracket** | `include <windows.h> as win` | System header registry: `crates/c-ffi/system_headers.toml` |

### 7.2 Companion C Discovery

When `sqlite_rally.kn` declares `include "../../native/sqlite3.h" as sql`, the compiler:

1. **Resolves the header** at `native/sqlite3.h` (675 KB)
1. **Discovers the companion** `native/sqlite3.c` (9.1 MB) ~> by checking if a `.c` file with the same stem exists at the same path
1. **Extracts function bindings** via libclang (Tier 1): `sqlite3_libversion_number()`, `sqlite3_threadsafe()`, `sqlite3_complete()`, `sqlite3_keyword_count()`, and all ~200+ SQLite API functions
1. **Generates alias externs** with `sql_` prefix: `sql_libversion_number()`, `sql_threadsafe()`, etc.
1. **Compiles `sqlite3.c`** into the native link
1. **Binds real C symbols** via `@link_name` on the generated thunks

The same process applies to `smoketest_sqlite_pingpong.h` → `smoketest_sqlite_pingpong.c` (10.4 KB), producing `ping_score()`, `ping_signature()`, etc.

For the visualizer bridge, `smoketest_visualizer_bridge.c` (32.9 KB) is compiled **separately** via `build-smoketest-visualizer-bridge.ps1` using clang, producing `smoketest_visualizer_bridge.obj` => then linked into the final binary.

### 7.3 CFFI Config in KAIN.toml

The `KAIN.toml:24-38` records explicit bridge metadata:

```toml
[c_ffi]
include_paths = ["native"]

[[c_ffi.libraries]]
name = "sqlite3"
header = "native/sqlite3.h"
sources = ["native/sqlite3.c"]

[[c_ffi.libraries]]
name = "smoketest_sqlite_pingpong"
header = "native/smoketest_sqlite_pingpong.h"
sources = ["native/smoketest_sqlite_pingpong.c"]

[[c_ffi.libraries]]
name = "smoketest_visualizer_bridge"
header = "native/smoketest_visualizer_bridge.h"
sources = ["native/smoketest_visualizer_bridge.c"]
link_libs = ["user32", "gdi32", "opengl32"]
```

### 7.4 The Kain Facade Pattern (Critical Rule)

The smoketest demonstrates the **correct C interop pattern** from `docs/C_GUIDE.MD` § Strategy 5:

```
❌ WRONG: every file does "include native/sqlite3.h as sql"
   → Multiple include sites, scattered raw C vocabulary

✅ RIGHT: sqlite_rally.kn OWNS the include
   → c_bridge.kn uses "use sqlite_rally::smoke_sqlite_version"
   → c_abi_album.kn uses "use sqlite_rally::smoke_sqlite_ping_score"
   → Never leaks raw C vocabulary to the rest of the codebase
```

### 7.5 C Type System Mapping

The smoketest exercises these C→Kain type mappings (from `docs/C.MD` § 10):

| C Type | Kain Type | Example |
|--------|-----------|---------|
| `int` | `Int` | `sql_libversion_number() -> Int` |
| `const char*` | `String` | `sql_libversion() -> String` (via `@c_string_return`) |
| `sqlite3*` | `Int` (opaque handle) | `sql_open(path, 0) -> Int` |
| `int (*)(void*,int,char**,char**)` | `Int` (callback ptr) | `sql_exec(db, sql, callback_ptr, 0, err_ptr) -> Int` |

### 7.6 Full C Interop Reading

- **Architecture reference:** `docs/C.MD` :: 4-layer stack: parser → libclang extraction → runtime bridge → codegen backends. 1912 lines covering all 605 Win32 functions, 755 Vulkan functions, C type mapping, runtime contract emission, inline assembly, and the `@extern`/`@link_name` path.
- **Usage guide:** `docs/C_GUIDE.MD` ~> 13 strategies from "eliminate the bridge entirely" to the full Win32 GDI app pattern. See § 4 for the SQLite zero-manifest amalgamation pattern used here.

______________________________________________________________________

## 8. GPU Shader Artifacts

### 8.1 Shader Sources

**`src/gpu/fragment.kn`** defines:

- `SmokeVertex` – `shader vertex` with `position: Vec3`, `uv: Vec2`, `uniform offset: Vec3 @0`
- `SmokeGradient` ~~ `shader fragment` with `uv: Vec2`, `uniform accent: Vec3 @0`
- `SmokeVignette` ‒ `shader fragment` with `uv: Vec2`, `uniform tint: Vec3 @0`

**`src/gpu/compute.kn`** defines:

- `SmokeParticleStep` ‒ `shader compute` with `StorageBuffer<Vec4>` particles + field
- `SmokeReductionKernel` :: `shader compute` with `StorageBuffer<Float>` src + dst
- `SmokeOrchestrateKernel` ~ `shader compute` with `StorageBuffer<UInt>` src + dst, `workgroup(8, 1, 1)`

### 8.2 Generated Artifacts

The build produces these GPU artifacts in `smoketest/`:

| File | Content | Size |
|------|---------|------|
| `kain_shader_bundle.json` | SPIR-V module hex + entry points + stage hints | 25.5 KB |
| `kain_compute_residency.json` | Per-kernel residency metadata (dispatch sizes, bindings, stream policies) | 5.2 KB |
| `kain_compute_residency_shader_smokeparticlestep_compute_particles.bin` | Particle step compute residency | 1.0 KB |
| `kain_compute_residency_shader_smokeparticlestep_compute_field.bin` | Particle step field residency | 1.0 KB |
| `kain_compute_residency_shader_smokeorchestratekernel_compute_src.bin` | Orchestrate kernel src residency | 96 B |
| `kain_compute_residency_shader_smokeorchestratekernel_compute_dst.bin` | Orchestrate kernel dst residency | 96 B |
| `kain_compute_residency_shader_smokereductionkernel_compute_src.bin` | Reduction kernel src residency | 32 B |
| `kain_compute_residency_shader_smokereductionkernel_compute_dst.bin` | Reduction kernel dst residency | 32 B |
| `kain_gpu_runtime.dll` | GPU runtime DLL | 1.7 MB |

### 8.3 Orchestrate + GPU Pipeline

The `src/semantics/orchestrate.kn` file demonstrates the full GPU compute pipeline combined with upper semantic layers:

- `stage gpu_tune: gpu` dispatches `"shader::SmokeOrchestrateKernel::compute"` with `[12, 2, 1]` workgroups
- `guarded by smoke_silicon_truth` (axiom from `axiom.kn`) gates the GPU stage on capability predicates
- `fallback degrade smoke_orch_degrade` provides a CPU fallback when GPU is unavailable
- Compute residency metadata at `kain_compute_residency.json` maps the kernel to dispatch sizes and stream policies

For GPU semantics, see `docs/SHADER_GPU.MD`.

______________________________________________________________________

## 9. WASM Target

`src/wasm/wasm_main.kn` is the standalone WASM entry point => it does NOT import from the rest of the album. It proves:

- Simple arithmetic (`wasm_add`)
- Recursive functions (`wasm_factorial`)
- Iterative computation with `var` and `while` (`wasm_fibonacci`)

The build graph defines a separate `check-wasm` task for this file with `target("wasm")`. It is a check-only task >> no `.wasm` binary is produced in the default build graph (the task verifies the WASM target typechecks correctly).

______________________________________________________________________

## 10. Telemetry & Evidence DAG

### 10.1 Report Files

Every lane emits a per-track JSON file to `<output_root>/tracks/<track_name>.json`:

```json
{
  "mode": "full",
  "category": "semantics",
  "track": "actor",
  "lane": "smoke_actor_lane",
  "offset": 10,
  "status": 0,
  "ok": 1,
  "started_ms": 1700000000001,
  "ended_ms": 1700000000003,
  "elapsed_ms": 2,
  "track_checksum": 1704200007,
  "composition_checksum": 3124500321,
  "cpu_feature_fingerprint": 15,
  "patch_journal_count": 47,
  "entangle_propagation_count": 12,
  "converge_mismatch_count": 0
}
```

Each track includes runtime telemetry: `patch_journal_count()`, `entangle_propagation_count()`, and `converge_mismatch_count()` ___ compiler-owned metrics that verify the semantic stack is operating correctly.

### 10.2 Summary Report

After all lanes complete, `smoke_write_summary_report()` in `report.kn` writes a summary JSON with:

- Total tracks, succeeded tracks, failed tracks
- Composition checksum (cumulative hash of all track results)
- Total elapsed time
- Per-category breakdown

### 10.3 Notes

Arbitrary structured notes can be written via `smoke_write_note_report()` --- used by:

- `dashboard.kn` → `ui_dashboard.json` (UI snapshot)
- `presenter.kn` → `opengl_album.json` + `opengl_window_report.txt`
- `headless_host.kn` → `headless_host.json`

______________________________________________________________________

## 11. Python Bridge (Run Orchestration)

The Python bridge (`telemetry/python_bridge.kn` + `telemetry/run_smoketest_mode.kn`) handles mode dispatch for `album-full`, `album-benchmark`, and `album-attrition`:

```
build.kn exec_task("album-full")
    → powershell → invoke_kain.ps1 → kain run run_smoketest_mode.kn --target interpret
        → run_smoketest_mode.kn main():
            parses --mode, --executable, --output-dir, --bench-rounds, etc.
            → smoke_python_bridge_run_mode(...)
                → python_exec("import os\nimport subprocess\n...")  [python_bridge.kn:18]
                    Python runner spawns smoketest.exe as subprocess with:
                    env['KAIN_SMOKETEST_MODE'] = mode
                    env['KAIN_SMOKETEST_OUTPUT_DIR'] = output_dir
                    → subprocess.run([exe_path], capture_output=True)
                    → Returns structured result dict
                → Parses Python result, writes summary.json to output_dir
```

The `invoke_kain.ps1` script (93 lines) handles Kain binary resolution with this priority:

1. `$env:KAIN_BIN` (explicit override)
1. Bazel-built binary at `Z:/_b/.../bin/crates/cli/kain.exe`
1. Repo launcher at `.kain/bin/kain.exe`
1. On-demand `bazel build //:kain --config=dev`
1. Cargo target at `target/debug/kain.exe`
1. PATH-located `kain.exe`

For Python interop documentation, see `docs/PYTHON.MD` and `docs/PYTHON_GUIDE.MD`.

______________________________________________________________________

## 12. Visualizer Bridge (OpenGL UI)

The OpenGL visualizer bridge is compiled separately from the main smoketest:

```
build-smoketest-visualizer-bridge.ps1:
    clang -c -O2 -D_CRT_SECURE_NO_WARNINGS -I native
        native/smoketest_visualizer_bridge.c
        -o .kain/native/smoketest_visualizer_bridge.obj

run-visual-smoketest.ps1:
    1. Runs build-smoketest-visualizer-bridge.ps1
    2. Compiles src/main.kn → smoketest.exe via kain build
    3. Sets KAIN_SMOKETEST_MODE=visual
    4. Runs smoketest.exe
    5. Validates ui_dashboard.json, opengl_album.json, opengl_window_report.txt exist
```

The visualizer bridge provides:

- `viz_probe()` → returns 1 if the bridge is live
- `viz_run_window(title, width, height, frame_budget, input_path)` → runs the OpenGL window
- `viz_frames_presented()` → frame count
- `viz_cells_drawn()` → cell draw count
- `viz_write_report(path)` → writes an OpenGL window report to disk

Linked against: `user32`, `gdi32`, `opengl32` (see `KAIN.toml:38`).

______________________________________________________________________

## 13. Smoke Modes

| Mode | Env Var | What Happens | Output |
|------|---------|-------------|--------|
| `full` | `KAIN_SMOKETEST_MODE=full` | Runs every lane once, records all track reports, writes summary | `telemetry/full/` |
| `benchmark` | `KAIN_SMOKETEST_MODE=benchmark` + `KAIN_SMOKETEST_BENCH_ROUNDS=256` + `KAIN_SMOKETEST_BENCH_PASSES=6` | Re-runs lanes multiple times, records per-round track reports | `telemetry/benchmark/` |
| `attrition` | `KAIN_SMOKETEST_MODE=attrition` + `KAIN_SMOKETEST_ATTRITION_OPS=20` + `KAIN_SMOKETEST_ATTRITION_ROUNDS=96` | Stress-tests lanes with repeated create/destroy cycles | `telemetry/attrition/` |
| `visual` | `KAIN_SMOKETEST_MODE=visual` (default when run as `smoketest.exe`) | Runs UI album lane + OpenGL album lane, writes notes | `telemetry/visual/notes/` |

Mode detection in `report.kn:22-24`:

```kn
fn smoke_default_mode() -> String:
    let executable_name = to_lower(process_current_executable_name())
    if executable_name == "smoketest.exe" or executable_name == "smoketest":
        return "visual"       // Standalone exe = interactive mode
    return "full"             // Automatically invoked = batch mode
```

______________________________________________________________________

## 14. Capsule Output

After certification passes, three capsule files are emitted to the smoketest root:

| File | Size | Contents |
|------|------|----------|
| `smoketest.kn` | 11.4 MB | Editable source capsule containing ALL `.kn` files inline, with rich header, 96 preview symbols |
| `smoketest.artifacts.kn` | 54.5 MB | Artifact companion capsule (binary outputs, GPU modules, residency data) |
| `smoketest.evidence.kn` | 113.5 KB | Evidence companion capsule (telemetry reports, track JSONs, summary data) |

These form a portable "smoketest" capsule set that can be unpacked with `kain amalgamate unpack smoketest.kn -o my-copy/`.

______________________________________________________________________

## 15. Reference Doc Map

Every system exercised by the smoketest has a corresponding deep-dive document in `docs/`. This table maps each smoketest source directory to its reference documentation:

| Smoketest Directory | Reference Docs | Line Count (approx.) |
|--------------------|----------------|---------------------|
| `src/semantics/` | `docs/KEYWORDS.MD` (all 110 keywords), `docs/RULEBOOK.md`, `docs/WORLD.MD`, `docs/ENTANGLE.MD`, `docs/PATCH.MD`, `docs/LAW.MD`, `docs/CONVERGE.MD`, `docs/ORCHESTRATE.MD`, `docs/PULSE.MD`, `docs/RESONATE.MD`, `docs/AXIOM.MD`, `docs/SHATTER.MD`, `docs/TELEPORT.MD`, `docs/OWNERSHIP.MD`, `docs/ACTOR.MD`, `docs/EFFECTS.MD` | ~800 pages |
| `src/systems/` | `docs/OWNERSHIP.MD`, `docs/SYSTEMS_PROGRAMMING.MD` | ~160 pages |
| `src/gpu/` | `docs/SHADER_GPU.MD` | ~51 pages |
| `src/interop/` | `docs/C.MD`, `docs/C_GUIDE.MD` | ~127 pages |
| `src/stdlib/` | `docs/STDLIB.md` | ~103 pages |
| `src/ui/` | `docs/COMPONENT.MD` | ~48 pages |
| `src/wasm/` | `docs/BUILD_PROJECTS.MD` (target table) | ~53 pages |
| `src/telemetry/` | `docs/BUILD_PROJECTS.MD` (evidence DAG) | ~53 pages |
| `telemetry/` (bridge) | `docs/PYTHON.MD`, `docs/PYTHON_GUIDE.MD` | ~107 pages |
| `build.kn` | `docs/BUILD_PROJECTS.MD` (complete DSL reference) | ~53 pages |

**Additional reading:**

- `GLOSSARY.MD` ~ Maps every Kain term to its physical repo location (crates, runtime, stdlib)
- `AGENTS.md` - Agent doctrine, tool map, Bazel guide, Kain CLI quick reference
- `benchmark/cases_v2/fusion_chain.kn` --> The definitive 550-line causal-chain proof exercising all 7 layers
- `benchmark/cases_v2/keyword_crucible.kn` --- 108/110 keywords exercised in context

______________________________________________________________________

*This README documents the smoketest system as of 2026-06-08. Every path, filename, import, and build task is anchored to actual files on disk. For structural changes, update this file to match.*
