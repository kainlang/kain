# Blades Catalog

Every project under `X:/blades/` — what it is, what it's trying to achieve, and which Kain semantic layers it exercises.

______________________________________________________________________

## `c/` — C ABI / FFI Integration

Projects that stress-test Kain's ability to interface with native C code through `include ... as ...`, `@extern`/`@link_name`, C bridges, and platform SDKs.

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **asm** | Space Invaders proof-of-concept with inline `asm()` blocks + Win32 GDI desktop painting | Prove Kain can execute inline x86-64 assembly (`rep stosd`, `rdtsc`, `mov dword ptr`) without any C bridge, and paint directly to the Windows desktop HDC via Win32 | `asm()`, `Unsafe`, raw pointers (`alloc_zeroed`, `ptr_to_int`, `mem_load`, `mem_store`, `decay`), `include <windows.h>` |
| **component_fuzz** | Extreme component stress test — ~30 components across 10 sections: naked, stateful, computational, recursive, pointer-laden, actor-aware, deeply nested (10 levels) | Push every `component` boundary to breaking point and prove Kain's component/JSX system survives aggressive nesting, recursion, unsafe memory, actor integration, and world surface bindings | `component`, `world`, `entangle`, `patch`, `pulse`, `law`, `actor`, `shatter`, `collapse`/`observe`/`decay`, JSX rendering, `state`, raw memory ops |
| **component_minimal** | Minimal Win32 GDI window with interactive controls (click counters, debug toggles, animated sphere) + fast inverse square root | Show Kain's Win32 interop at its most direct: hand-rolled `@extern` declarations, manual message loop, GDI drawing, and the 0x5F3759DF fast inverse sqrt | `@extern`/`@link_name` FFI, `include <windows.h>`, GDI drawing, raw pointer math, `bitcast`, `Unsafe`, component JSX |
| **component_shader** | Interactive GPU Julia fractal shader editor — Win32 window + SPIR-V compute shaders for Mandelbrot/Julia pan/zoom | Demonstrate the full Kain GPU pipeline: authored GLSL compute shader → SPIR-V compilation → dispatch → file-based staging readback → GDI rendering. Also proves a handle-leak workaround for the GPU runtime | `shader compute` with `StorageBuffer`, `comptime`, `dispatch`, `@extern` Win32/GDI FFI, `bitcast`, `fs_open`/`fs_write`/`fs_read`/`fs_close`, epsilon LUT precomputation |
| **ephemaris** | Portable GPS baseband generator control deck — world map → Python ephemeris download → C bridge to gps-sdr-sim → PlutoSDR upload | Full end-to-end SDR pipeline: map coordinate selection, real ephemeris fetching, GPS IQ generation via vendored C library, and SDR upload. Proves Kain owns a multi-language, multi-stage RF toolchain | `std::ui` (full UI toolkit with fonts/textures/buttons/events), `std::process` (spawn/wait/capture), `std::json`, `std::fs`, `std::math`, `std::text`, `std::time`, C FFI bridge, Python interop, JSON config/state files |
| **ffmpeg** | FFmpeg C ABI gauntlet — video decode via real FFmpeg headers (`libavformat`, `libavcodec`, `libswscale`) with Win32 presenter | Stress-test Kain's ability to include and use real FFmpeg headers via natural C includes alongside a hand-written bridge. Decode, checksum, and frame-count validate end-to-end | Mixed natural `include <libavcodec/avcodec.h>` + explicit C FFI library config, `@extern`/`@link_name` FFI, `std::process`, multi-file modular Kain project, full media decode pipeline |
| **include-natural** | Minimal proof-of-concept for natural C includes (`include native/native_math.h as nm`) — auto-discovers sibling `.c` source, emits alias-aware externs | Prove the bare-minimum C ABI surface: no `KAIN.toml`, no `[c_ffi]` config, no tiers — just a header, a `.c` file, and a Kain file | Natural C include (`include ... as ...`), auto-discovery of sibling `.c` sources |
| **minimal** | The absolute minimal C ABI proof — even simpler than include-natural. One header, one `.c` file, one Kain file calling `m_add` and `m_mul` | Prove zero-configuration C interop: the simplest possible ABI bridge | Minimal natural C include, zero config needed |
| **nuklear** | Nuklear header-only GUI library natural include + Python/Pygame fusion renderer. Kain uses `include nuklear.h as nk` and drives a "Fusion Reactor" demo with worlds, actors, entangles, patches, and Pygame rendering | Prove Kain can import and use the Nuklear header API naturally while simultaneously driving a multi-runtime fusion: Kain semantic layers (worlds, actors, entangle, patch, pulse, laws, shatter) + Pygame rendering | `include nuklear.h as nk`, `world`/`entangle`, `actor` with `ask`, `patch`, `pulse`, `law`, `shatter`, `import pygame`, Python interop (`python_call_attr_raw`), Pygame surfaces/fonts/draw |
| **opengl** | Raw Win32/WGL OpenGL compatibility blade — creates an OpenGL window, renders triangles, writes a report | Prove Kain can drive OpenGL through a C bridge library linking against `opengl32.lib`, `user32`, and `gdi32` | C FFI bridge (`c::opengl_bridge`), `[c_ffi]` library config with shared lib and link libs, Kain facade module |
| **platform/windows** | Proof that libclang parses the real Windows SDK `<windows.h>` — 6,294 function declarations extracted with zero shim | Demonstrate that Kain can include the full `<windows.h>` from the real Windows SDK and call `MessageBoxA` directly — no hand-written shim, no bridge code, no macro workarounds | `include <windows.h>` system header include, zero-shim Win32 ABI access |
| **sqlite** | SQLite amalgamation natural include smoke test — proves Kain can import the real SQLite amalgamation (`sqlite3.h` + `sqlite3.c`) without a hand-written `[c_ffi]` entry | Call `sql_libversion_number()`, `sql_threadsafe()`, `sql_complete()` — all auto-generated from the amalgamation | Natural include of SQLite amalgamation (`include sqlite3.h as sql`), auto-discovery of sibling `.c` source, zero-manifest C integration |
| **vulkain** | Raw reusable Vulkan window package for Kain LLVM blades — provides `vulkain_probe`, `vulkain_run_window`, `vulkain_run_mesh_scene`, `vulkain_run_kloner_same_window` | Give other Kain projects a reusable Vulkan presentation surface. Includes two example blades: a mesh-scene demo and a std-math-bounce-game that uses worlds, actors, entangle, shatter, teleport, and stdlib math for a bouncing-cube simulation | C FFI (`c::vulkain_bridge`), `[c_ffi]` with platform Vulkan package, Kain facade module, `world`/`entangle`/`shatter`/`actor`/`pulse`/`patch`/`axiom`/`converge`/`orchestrate`/`teleport`, `std::math` (vec3, quat, AABB, ray, noise, FBM), `std::input`, GPU compute |
| **vulkan_v2** | GPU fluid simulator with two compute shader variants: semi-Lagrangian fluid advection + Gray-Scott reaction-diffusion. Rendered via Win32 GDI `StretchDIBits` | Pure Kain Win32 window + SPIR-V compute dispatch for fluid simulation. Multi-kernel GPU pipeline with file-based staging, GDI pixel packing, and handle-leak reclamation | `shader compute` with `StorageBuffer`, `dispatch`, `@extern` Win32/GDI FFI, `bitcast`, `alloc_zeroed`, `fs_open`/`fs_write`/`fs_read`/`fs_close`, `_putenv`, CUDA runtime queries |

______________________________________________________________________

## `cuda/` — CUDA GPU Compute

Projects that exercise Kain's CUDA/PTX compilation pipeline, GPU compute kernels authored in Kain, and multi-stage GPU pipelines.

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **mcp** | GPU-accelerated semantic search MCP (Model Context Protocol) server for the Kain repo. Indexes source, builds embeddings, serves search via CUDA-scored kernels. Full MCP stdio tool | End-to-end semantic code search: file chunking → embedding → binary index → CUDA scoring kernel → CUDA top-k kernel → results merge. Includes a "God mode" fused single-kernel score+topk path. Exposes MCP tools for search, reindex, and health | `std::cuda` (cuda_runtime_state, cuda_dispatch, cuda_binding_payload, cuda_pack_u32_array_le), `std::python`, `std::fs`, `std::process`, GPU compute shader kernels authored in Kain, shader bundle + residency manifest, MCP stdio protocol |
| **ptx_1** | Author-first CUDA/PTX stress test — multi-stage GPU pipeline (field generation → blur → colorize) validated against a C++ reference comparator | Full multi-stage CUDA compute pipeline authored in Kain (3 kernels), staged payloads, dispatched to GPU, output verified against a C++ CPU implementation, BMP output with diff images | `shader compute` CUDA kernels, `comptime` blocks with dispatch/binding metadata, `std::cuda` (cuda_pack, cuda_dispatch_primary_compute, cuda_binding_payload_path, cuda_zero_output_payloads, cuda_copy_binding_payload), `std::fs`, `std::process` (spawn C++ verifier), multi-kernel pipeline |

______________________________________________________________________

## `network/` — Networking / Protocol Libraries

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **domains** | Sovereign built-in networking domain proof — exercises `std::net`, `std::http`, `std::tls`, `std::http2` end-to-end | Create a localhost HTTP server with actor request handler, validate request parsing (method, path, query, protocol, body), respond with status 207, verify via TCP, and create TLS/HTTP2 request objects | `std::net`, `std::http`, `std::tls`, `std::http2`, `std::io`, `std::uri`, `actor` with `on HttpRequest`, TCP client/server, TLS/HTTP2 request objects, buffered readers/writers |
| **http** | HTTP request/response helper library for reusable Kain blades — wraps `std::net` with JSON payload helpers | Provide `http_build_json_request`, `http_send_json_request`, `http_respond_json` — a small facade over std::net for JSON-based HTTP communication | `std::net`, JSON HTTP request/response helpers, library-style Kain blade with public API |
| **json** | JSON serialization and parsing library — `JsonValue` struct with kind/bool/int/string fields, serialization to/from text | Provide a lightweight JSON parsing/serialization dependency for other blades | Pure Kain library, custom struct types, string-to-value parsing, value-to-string serialization, `pub` module exports |

______________________________________________________________________

## `sims/` — Simulation / Visualization

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **chronosim** (kain-labs) | KQuantum-inspired GPU particle simulator — 262,144 particles, 8 physics modes (Zero-Point, Galactic Spiral, Quantum Pilot, Neural Lattice, Navier-Stokes, Hellfire, Plasma Arc, Super Vortex) via Vulkan C FFI bridge | Full native GPU particle lab: Kain-authored SPIR-V compute kernels (particle advection, velocity field, fluid pressure projection, feedback composite), Win32 UI with mode selection, metrics display, Vulkan window, world/entangle/patch/law semantic layers, config-driven mode catalog | `shader compute` (4 GPU kernels), `world`/`entangle`/`actor`/`patch`/`law`/`converge`/`orchestrate`, `pulse`, C FFI (`c::kquantum_vulkan_bridge`), full `std::ui` reconciliation tree, TOML-driven mode config, Z3 proof file for bridge bounds, `teleport`, `ui_state_set_i64`/`ui_state_set_string` |
| **fluid-studio** | Data-driven fluid simulator with Kaintana UI controls, authored GPU shaders, and Vulkain 3D presentation | Full semantic simulation pipeline: world/patch/entangle/law/converge/orchestrate layers, shatter structs for impulse data, actor relays, fluid pulse clock, teleport between worlds, collapse/observe/decay memory patterns, presented through Vulkain with multiple proof probes | `world`/`entangle`/`shatter`/`actor`/`patch`/`law`/`pulse`/`converge`/`orchestrate`/`teleport`, `collapse`/`observe`/`decay`, `std::hash`, `std::math`, `std::intent`, Kaintana widget framework, C FFI bridges (kaintana_desktop_bridge, vulkain_bridge), multi-source-root project with cross-blade dependencies, SPIR-V surface shader |
| **spirv-visualizer** | Data-driven SPIR-V capability visualizer — scans the Kain repo for SPIR-V artifacts, catalogs them with metadata, previews in a Vulkain Kloner window | Scan configurable directories for `*.spv`, `*.reflect.json`, shader bundles; extract byte-level metadata (binding counts, entry points, module sizes); select best renderable pair (vertex+fragment) or proxy scene; render via Vulkain Kloner. Writes detailed catalog and report files | `world`/`entangle`/`shatter`/`actor`/`patch`/`law`/`axiom`/`converge`/`orchestrate`/`teleport`, `std::math`, `std::fs` (fs_walk_paths_text, fs_write_bytes_hex), `std::json`, C FFI (`c::vulkain_bridge`), Vulkain Kloner multi-instance renderer, config-driven scan, catalog production with renderable/compute/capability scoring, `spawn`/`ask` actor relay |

______________________________________________________________________

## `python/` — Python Interop Projects

Projects that exercise Kain's first-class Python interop (`import ...`, `from ... import ...`, `python_shared_buffer`, `python_actor_callback`, etc.) across game engines, audio production, tensor computation, and UI frameworks.

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **audio/kainbleton** | Full Kain-owned DAW (Digital Audio Workstation) that orchestrates Python audio ecosystems (DawDreamer, sounddevice, SoundFile, PyQtGraph, MIDI) | Prove Kain can own a professional-grade audio production tool: real microphone recording/playback, transport-driven timeline, real-time waveform visualization, semantic score tracking, proof reports, UI screenshots. Kain owns orchestration, Python owns pixel rendering and DSP | Python FFI (`import`/`from ... import`), `world`/`entangle`, `actor` with `spawn`/`send`, `law`/`patch`, `converge`, C FFI (`include`/`use c::`), JSON manipulation, `Unsafe` |
| **py_2** | Kain-first Pygame game loop — Kain owns game logic (physics, AI, scoring), Pygame owns pixel rendering. Breakout-style with ball, paddle, AI, ghosts | Prove zero-copy shared image transfer between Kain and Python via `python_shared_image`. Kain controls all state (ball physics, paddle AI, scoring, ghost spawning, collision detection) while Pygame renders each frame | `world`/`entangle`/`mirror`, `actor` with `spawn`/`ask`/`Pulse`, `teleport shard from ... to ... via ...`, `shatter struct`, `law`/`patch`, Python FFI (`import pygame`, `python_call_attr_raw`), `python_shared_image`/`interop_shared_image_info`, JSON config |
| **py_c** | Canonical Kain Python import lab — combines Python FFI (numpy, torch, pygame, Z3, FastMCP) with a native C bridge for maximum interop stress testing | Prove the entire Kain-Python interop surface works simultaneously: module import/digest/name checking, numpy+torch tensor sharing (shared/owned), zero-copy buffer and image contracts, pygame surface rendering, Z3 solver invocation, FastMCP server construction, actor relay with native C functions, shatter/teleport between worlds, byte-level buffer/image mutation | Python FFI (numpy, torch, pygame, z3, fastmcp), C FFI (`include python_lab_bridge`), `world`/`entangle`, `actor` with `spawn`/`ask`/`Fold`, `shatter struct`, `teleport`, `law`/`patch`, `collapse`/`decay`, `alloc_zeroed`/`ptr_offset`/`mem_store`, `Unsafe`, `python_shared_buffer`/`python_shared_image`, `kain_tensor_info`/`kain_tensor_set` |
| **library/** | Collection of isolated micro-probes testing specific Python interop features | Stress-test each Python interop surface individually: buffer views (20K iterations), region-based buffer checksum (10M iterations), shared buffer adoption (20K iterations), Python call hotloop (150K sqrt calls), pygame shader/world/actor pipeline at ~60fps, pyglet OpenGL window, Flet desktop dashboard with full Kain semantic stack | `python_region_begin`/`end`, `python_buffer_view`, `python_shared_buffer`/`python_shared_image`, `py_call_raw_f64_trunc_i64`, `shader fragment`, `world`/`entangle`/`mirror`, `actor`/`spawn`/`ask`, `teleport shard`, `collapse`/`observe`/`decay`, `converge`, pyglet OpenGL, Flet dashboard |
| **24_tet** | Empty directory — name likely refers to 24-tone equal temperament (microtonal music) | Placeholder / not yet implemented | — |
| **actor_relay** | Empty directory | Placeholder for actor-relay experiments | — |

______________________________________________________________________

## `three-d/` — 3D Graphics Applications

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **graphics/kloner** | Faithful Kain-native workstation recreation of the legacy KCloner operator — a 3D clone/scatter tool with grid, radial, honeycomb, and helix layouts | Full desktop application with Vulkan hardware rendering, Kaintana UI overlay (sidebar controls, inspector, charts, transport controls), clone layout math (vec3/quat/mat4), Catmull-Clark subdivision, presenter reporting. Designed as a build+certify capsule | `world`/`entangle`, `law`/`patch`, `component`, C FFI (`use c::kaintana_desktop_bridge`, `use c::vulkain_bridge`), Kaintana UI framework, Vulkain Vulkan engine, `certify_gate`, `native_executable`, vec3/quat/mat4 math, `hsv_to_rgb`, `fbm2` noise |
| **zender** | GPU-accelerated data-driven sculpting system — a Kain-native ZBrush clone with Vulkan rendering, Catmull-Clark subdivision, and a sculpt benchmark suite with 7 brush types | Load GLB 3D assets → Catmull-Clark subdivision → GPU particle scene → Vulkan window with orbiting particles → GPU compute brush kernels (Clay Build-Up, Smooth, Pinch, Inflate, DamStandard, Move, Flatten) → detailed telemetry/reports. Full sculpting benchmark with 7 brush types and 6 GPU compute shaders | `world`/`entangle`/`mirror`, `shatter struct`, `teleport`, `law`/`patch`, `converge`, `component`, `shader compute` (6 GPU brush kernels with StorageBuffer uniforms), `spirv`/`cuda` compile targets, C FFI (`include native/zender_vulkan.h`), vec3/quat/mat4 math, `pulse`, `certify_gate`, extensive JSON/report writing |
| **sculpt** | Empty directory | Sculpting work lives in `zender/src/sculpt/` instead | — |

______________________________________________________________________

## `ui/` — UI Framework Ecosystem

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **kaintana** | **THE FLAGSHIP UI FRAMEWORK** — retained+immediate mode GUI system built entirely in Kain with the semantic stack (world/entangle/resonate/axiom) as its backbone. 21+ widget types, 4 color themes, 3 platform backends (Desktop GDI, Vulkan, Winit) | Kain's answer to React + egui: compile-time reactive UI with hot-reload, keyboard action binding, agent intent injection (AI agents push UI events), IME, clipboard, menus, dialogs, popovers, focus management, scroll containers, React-style keyed reconciliation. Ships as reusable `kain_library` with capsule/amalgamation | `world`/`entangle`/`resonate`/`axiom`, `component`, `patch`/`law`, `converge`/`orchestrate`, C FFI, `std::ui` host session, `std::reload`, hot-reload integration, agent intent injection (`kaintana_action_push_agent_intent`), capsule/amalgamation build, `certify_gate` |
| **kaintana-test** | Consumer proof blade for Kaintana's hot-reload surface — exercises all widget APIs, validates shape correctness | Prove every Kaintana feature works end-to-end: worlds, entangles, patches, laws, converges, orchestrates, menus, dialogs, popovers, IME, clipboard, keyboard action binding, agent intent injection, focus management | All Kaintana widget APIs, keyboard action binding, agent intent injection, IME, clipboard, dialogs, menus, popovers, focus management, frame reports, shape verification |
| **kaintana-vulkan** | Optional Vulkan embed adapter lane — separates the Vulkan presenter into its own blade so the default Kaintana desktop executable never morphs into the Vulkan proof lane | Provide `kaintana_vulkan_embed_available()` probe, `kaintana_vulkan_host_run_window()`, frame/geometry reporting for the Vulkan backend | Depends on `kaintana` + `vulkain`, C FFI bridges, modular architecture |
| **kaintana-vulkan-test** | Foreign presenter acceptance blade for Kaintana's Vulkan embed lane | Proves the Vulkan adapter works without contaminating the default desktop executable — Kaintana session with Vulkan backend, worlds/entangles/converges/orchestrates, info dashboard | `world`/`entangle`/`patch`/`law`/`converge`/`orchestrate`, C FFI bridges, `native_entangle_registered_count`/`native_entangle_propagation_count`, frame/host reports |
| **kain-tui** | Small yazi-like Kain terminal file explorer (j/k navigation, h/l parent/enter, r refresh, q quit) with a pulse clock mode | Terminal file browser + animated pulse clock face with orbit animation — proves the runtime pulse system fires in real-time with configurable cadence (250ms jitter 25ms) | `pulse` (250ms jitter 25ms), `runtime_machine_pulse_total_fire_count()`, `std::fs`, `std::process`, `std::text`, `std::time`, `datetime_from_epoch_millis` |

______________________________________________________________________

## `kain/` — Starter Template

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **kain/** | Minimal "hello world" Kain project starter template | Blank-slate starting point for new Kain projects. Defines a build graph with project config, LLVM target, debug profile, check task, and native executable task | Basic `use std::build` build graph DSL, native executable compilation pipeline |

______________________________________________________________________

## `edge_cases/` — Compiler / Runtime Edge Case Tests

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **case_phi_pyattr** | Scaffold — placeholder for phi-node / Python attribute access edge case | Named after a specific compiler edge case about phi-node ordering when Python attribute access is involved in codegen. Skeleton only | Scaffold, no real test content yet |
| **codegen_edge_gaps** | Precision regression test suite for 6 LLVM codegen edge-case gaps discovered during markscript development | Capture and fix 6 distinct LLVM codegen failure modes: `::` leaking into LLVM type names, `py_getattr_raw` fallback firing incorrectly for Kain-to-Kain struct access, named-field enum variant destructure failures, function pointers missing from resolver, `return` in match arm producing dead PHI predecessor, PHI node predecessor mismatches from `break`/`continue` | 4-layer architecture (cause → effect → spookymagic → diagnostics), VM isolation wrapper, test table pattern with discoverable test registration, CLI flag parsing |
| **runtime** | Debug template for rapid Kain edge-case testing — self-replicating cloner script (`spawn.kn`) copies the entire template | Provide a surgical instrument for rapid single-bug reproduction. Contains the same 4-layer architecture as codegen_edge_gaps, precompiled `debug-template.exe` for immediate use, and a self-replicating cloner that duplicates the template with one command | `std::fs`, `std::path`, `std::process`, `std::runtime`, `std::text`, CLI flag parsing, `runtime_init`/`runtime_shutdown` lifecycle, self-replicating project cloner |

______________________________________________________________________

## `example/` — Canonical Kain Example / Interactive Workbench

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **example/** | The definitive "first file future agents should inspect" — most comprehensive example in blades (154 KB Kain source, 12 files). Full Kain surface: UI, graphics, input, networking, actors, worlds, themes, layout, runtime workbench | Serve as the canonical reference for what fully authored Kain looks like. Demonstrate every major Kain subsystem: enums, match, for loops, vec!/format!/println macros, observe/collapse/decay ownership, worlds, actors, native stdlib services, raw memory, shaders, UI, graphics, process, net, fs, input, effects, async values. Verify LLVM IR contains correct ABI calls | `match`, `for`/`range`, `vec!`/`format!`/`println!`, `observe`/`collapse`/`decay`, `world`, `actor`, `@extern fn`, `entangle`, UI host attachment, graphics ABI, input system, `NativeMetric` trait, 10+ native stdlib subsystem probes |

______________________________________________________________________

## `experiments/` — Experimental / Research Projects

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **convergence** (Schrödinger's Rats) | Maze-solving simulation where three rats (BFS, A\*, Random Walk) compete every frame — `converge` picks the winning strategy, `orchestrate` runs all three algorithms, Python/pygame shows colored trails | Prove that Kain's semantic constructs (`converge`, `orchestrate`, `world`, `patch`, `law`, `actor`, `shatter`, `pulse`, `teleport`) are general-purpose relationship descriptors, not domain-locked to CPU dispatch. The same `converge` that picks an AVX2 lane can pick a maze-solving strategy | `converge` with `spec reference`, `fast` lanes, `verify random(8)`, capability-based lane selection; `orchestrate` for typed multi-algorithm composition; `world`/`patch`/`law`/`actor`/`shatter struct`/`pulse`/`teleport`/`collapse`/`decay`, `ptr<Int>` raw buffers, `alloc_zeroed`, Python interop |
| **neural_lattice** | Semantic entanglement visualization — OpenGL window showing Kain's compiler-owned semantics in real time. Kain computes a "neural lattice" (128 synapses) through worlds, entangles, collapse/observe/decay, converge, actor, pulse, teleport. C side renders a dual-waveform visualization with 5 interactive modes | Demonstrate a computation-then-visualization split where Kain owns semantics and C owns pixels. The bridge is 22 integers — no pointers, no structs, no callbacks. Every Kain semantic construct's effect is directly visible in the OpenGL window | `world` (3 worlds: CorticalAuthority, DeepMirror, RogueProjection), `entangle` (3 couplings), `law`, `patch`, `converge`, `actor` (NeuralIgniter), `pulse` (4ms jitter 1ms), `teleport`, `collapse`/`observe`/`decay`, `shatter struct` (ShatteredSynapse), raw `ptr<Int>` buffers, C bridge (`use c::neural_lattice_bridge`), proof guards with specific error exit codes |
| **pong** | Pong game implementation with world/entangle/actors/ownership model — 1460x900 Win32 OpenGL window, vector arcade oscilloscope aesthetic, two panels (authority + mirror), 100,000 swarm particles, chaos mode, drift detection | A "native UI Pong state-lattice demo for Kain worlds, entangle, actors, and ownership transitions." The authority world owns the game state, the mirror world is entangled for comparison/drift detection. 18 entangled fields including paddle, ball, score, swarm energy. Includes Z3 proof scaffolding | `world` (PongAuthority, PongMirror with 18 state fields each), `entangle`, `use c::pong_window_bridge` C FFI, `component App()` with JSX-like render, JSON-driven configuration via `load_pong_config`, `@extern` function declarations, theme system with structured layout constants |
| **quantum_entangled_automata** | Cellular automata simulation using Kain's quantum/entanglement semantics — imports `std::proof`, `std::bench`, `std::attrition`, `std::certify` | A formally verifiable, benchmarked, attrition-tested cellular automata experiment with certification evidence. Scaffold stage with build graph but minimal implementation | `std::proof`, `std::bench`, `std::attrition`, `std::certify` evidence toolkit, build graph with check task and native executable |
| **ulta** | Empty scaffold — three empty subdirs (`cloner/`, `fluid-sim/`, `ui/`), zero-byte `build.kn` | Warehouse for future experiments | — |

______________________________________________________________________

## `boundary/` — FFI Boundary Demos

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **ts** | Dual-direction TypeScript/Kain FFI boundary demo — TS calls Kain via native DLL (koffi), Kain calls TS via process bridge (Node.js worker). Cross-validates prime computations | Demonstrate Kain can be called from TypeScript (compiled DLL loaded via FFI) and can call into TypeScript (spawning a Node.js worker process). Uses prime number computation as cross-validation test case | Compilation to native DLL, `use std::process` with `process_output_text()`, `use std::json` (json_parse, json_get_bool, json_get_int), `runtime_init`/`runtime_shutdown`, koffi FFI library for TS native binding |

______________________________________________________________________

## `lsp/` — Language Server Protocol + MCP Server

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **lsp/** | Dual-protocol server implementing LSP v3.0 (editor integration) + MCP (AI coding assistants). 82.9 KB Kain source across 11 files. Bundled as VS Code extension (25.2 MB .vsix) | Provide IDE-grade developer tools for Kain: diagnostics, completions, hover info, go-to-definition, references, formatting, semantic tokens, code actions, code lenses. MCP side exposes same services as tools for AI coding agents. Uses `std::kain` compiler services as backend | `std::json` for JSON-RPC, `std::kain` compiler service API (Document, Diagnostic, Symbol, Location), `std::mcp` for MCP protocol, `std::fs`, `std::process`, LSP Content-Length framed stdin/stdout transport, semantic token encoding (LSP 3.17 5-integer delta), code action FixIt→TextEdit conversion, code lens for reference counts, document tracking with URI↔path conversion, `node_require()` for VSIX packaging, `IO` effect annotation, unit tests with `test "name":` and `assert()` |

______________________________________________________________________

## `markscript/` — Prose-Native Scripting Runtime

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **markscript/** | Complete markdown-native bytecode VM — Kain's companion language for configuration, orchestration, and executable documentation. 15,000 lines across 9 files. Lexer (22 token types), parser/compiler (20 opcodes), stack VM, IVT bridge, @import resolution, structured error system. Produces `mks.exe` | Allow writing executable programs in pure Markdown. Every `#`, `>`, `|`, and \`\`\`\`\` is valid syntax. Headings are domains, sections are routines, blockquotes are intents, tables are matrices, code blocks are extracted. The only errors are runtime errors. The README itself is a valid MarkScript program (625 ops from 567 lines) | Full VM implementation in pure Kain: lexer, parser, stack VM (20 opcodes), IVT (Intent Vector Table) bridge with 6 built-in handlers, recursive @import resolution (MAX_IMPORT_DEPTH=16), typed runtime values (MarkValue with Int/Float/String/Table/Code), table type inference, disassembler with typed bytecode dump, REPL mode, structured error system with kind/message/line/domain/routine/did-you-mean, `std::text`, `std::fs`, `std::process`, `std::os` |

______________________________________________________________________

## `templates/` — Project Starters

| Template | What It Is | Status |
|----------|-----------|--------|
| **starter/** | Most basic Kain project — hello world with standard build plumbing | ✅ Complete |
| **debug/** | Self-contained, copy-pasteable debug template for rapid edge-case testing — 4-layer architecture (cause → effect → spookymagic → diagnostics), self-replicating cloner (`spawn.kn`), precompiled `debug-template.exe` | ✅ Complete |
| **python/pygame/** | Kain + Pygame 2D game template with full semantic stack: actors, world state, component UI, Python interop, converge/orchestrate pipelines. Precompiled to `pygame_template.exe` | ✅ Complete |
| **python/ursina/** | Kain + Ursina (Panda3D-based) 3D game template — most feature-complete Python interop template. Ursina bridge with 7 orbiting cubes, ground plane, camera orbit, FPS counter, bidirectional Kain callbacks | ✅ Complete |
| **python/flet/** | Kain + Flet (Python UI framework) template | 🔲 Scaffold only (hello world) |
| **python/panda3D/** | Kain + Panda3D template | 🔲 Scaffold only (hello world) |
| **python/pyglet/** | Kain + Pyglet template | 🔲 Scaffold only (hello world) |
| **three-d/** | Minimal 3D project template | 🔲 Scaffold only (same as starter) |

______________________________________________________________________

## `test/` — Proof / Test / Certification Blades

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **actor-ask-roundtrip** | Proof blade for native LLVM actor ask/reply roundtrip semantics | Verify `ask()` and `ask_timeout()` work correctly with typed actor reply ports (`reply_to` pattern). Tests bias addition, timeout, and boolean response filtering | `actor` with `on Call(reply_to: P, request: Int)`, `send reply_to.Reply(...)`, `ask()`/`ask_timeout()` |
| **amalgamate-capsule-probe** | Portable Kain capsule dogfood blade for pack/inspect/unpack/run routing | Prove the amalgamate/capsule pipeline can bundle Kain source into a self-contained capsule (including C headers) and that the capsule can be unpacked and executed correctly | Amalgamate/capsule packaging format, C header inclusion in Kain projects |
| **build-kn-system-smoke** | Comprehensive stress test for the `build.kn` evidence DAG system — the most sophisticated test project | Prove a root `build.kn` can: discover nested blades via workspace, own explicit test/proof/bench/attrition/certify tasks, drive hidden adapter lanes (Cargo, C shared library, GPU, Fabric, Node, Bun), skip capability-gated work without poisoning evidence, reject planner failures (duplicate task IDs, output collisions). **The core integration test for the build system itself** | `std::build`, `std::test`, `std::proof`, `std::bench`, `std::attrition`, `std::certify`, `workspace_defaults()` with blade patterns, 9+ task kinds, `requires_capability()`, workspace discovery of nested blades, GPU shader compute, Z3 formal proof obligation with SMT-LIB2, C interop bridge, evidence DAG composition checksums |
| **converge-autotune-probe** | Dogfood blade for native CPU capability-backed converge lane selection and autotuning | Test the runtime's ability to probe CPU capabilities (AVX2), select the optimal converge lane, record telemetry, and commit the winner | `converge` with `fast` lanes + `verify random()`, `orchestrate` with `kain`/`rust` dispatch, `runtime_cpu_capability_mask()`, `runtime_converge_select_lane()`/`record_telemetry()`/`commit_winner()` |
| **format** | Placeholder for future code formatting tests | 🔲 Empty directory | — |
| **hash-domains** | Proof blade for `std::hash` module — 22 test assertions covering the full hash API | Exercise every function in the hash module: hash masks, rotations (rotl32, rotr32), hash_u32/u64, hash_mix64, hash_pair32, hash_unordered_pair32, hash buckets, FNV-1a, CRC32, fingerprint32 | Full `std::hash` API surface |
| **machine-stones** | Proof blade for machine primitives: axiom, pulse, shatter, teleport | Test the hardware-near semantic layer: `axiom` (arch-specific guarantees), `pulse` (timed heartbeat fire), `shatter struct` (SoA layout), `teleport` (cross-world state transfer) | `axiom` with arch/capability guards, `pulse every ... jitter ...`, `shatter struct`, `teleport`, `world` with `surface` bindings, `component`, `for lane in range()` |
| **math-domains** | Comprehensive proof blade for `std::math` module — 17 test assertions | Exercise the full math API: vec3/vec4 ops, quaternions, mat4, affine3, AABB ray intersection, triangle ray intersection, Bezier cubic curves, HSV→RGB, fbm2, std140 layout, SIMD vec3x4, Worley noise | Full `std::math` API |
| **platform/linux** | Comprehensive Linux/Unix runtime proof blade — 9 test functions, 32 KB source | Validate Linux-specific behavior: OS identity, procfs, libc dynamic loading, temp-dir/path/hidden files/atomic writes/kernel rename semantics, process gap (explicitly tests unsupported-platform for process spawning), TCP loopback HTTP server/client, HTTP malformed request rejection, software graphics backend, GPU shared resource contracts, final heap validation. Writes human-readable report | `std::runtime`, `std::fs`, `std::os`, `std::os_path`, `std::process`, `std::net`, `std::http`, `std::platform`, `std::graphics`, `std::gpu`, `std::graphics::shared`, `std::json`, platform detection, library loading (libc.so.6), TCP/HTTP server/client, graphics session with SPIR-V/pipeline/draw/present, GPU shared buffers/images, heap validation |
| **platform-package-smoke** | Proof blade for the platform-package lock/import system | Test that platform packages (external native SDKs) can be declared, locked, and imported before Vulkan consumes generated dispatch packages | `platform_package()` in build DSL, `std::platform` library open/resolve/close, cross-platform conditional logic |
| **stdlib-domains** | Proof blade that imports and exercises the entire root stdlib domain surface — 24 stdlib modules, ~350 lines | Verify that all major stdlib domain imports resolve and their core functions work over the native stdlib profile | 24 stdlib imports (`std::runtime` through `std::reload`), actor spawn/send/shutdown, input system with key events, HTTP/2 request creation, GPU compute + graphics shared resources, UI session with nodes/text/state, hot-reload with migration plan |
| **stdlib-foundations** | Proof blade for `std::text`, `std::collections`, `std::crypto`, `std::alloc` — ~400 lines with 6 probe functions | Exercise the foundational stdlib in depth: text views/strings, ASCII utils, semver parse/compare/format, JSON construction/parsing, fmt writer, typed maps, queues, deques, priority queues, slot maps, SHA-256, HMAC-SHA256, BLAKE3, random bytes, bump/arena/pool allocators | `std::text`, `std::ascii`, `std::fmt`, `std::json`, `std::semver`, `std::collections` (typed_map, queue, deque, priority_queue, slot_map), `std::crypto` (sha256, hmac_sha256, blake3, random_bytes), `std::alloc` (bump/arena/pool), `Unsafe`, `decay` |
| **windows** | Win32 native window test blade — two approaches: pure `@extern` to `user32!MessageBoxA` and `include native/win32_window.h` for full Win32 window with WNDPROC | Prove Kain can call native Windows APIs via two methods: (1) zero C sidecar with `@extern` annotation directly to DLL exports, and (2) C header include with sibling `.c` for full window creation. Key demonstration of Kain's native ABI capabilities on Windows | `@extern`/`@link_name` for direct DLL function imports, `include native/win32_window.h as win`, C sibling source discovery (`.c` sidecar), native Win32 window creation with WNDPROC |

______________________________________________________________________

## `tools/` — Utility Tools

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **kg** (killgrep) | Actor-sharded Kain grep CLI tool — full-featured file search utility with pattern matching, recursive directory traversal, case-insensitive search, line numbers, files-only mode, count mode, hidden file support, stats, worker count control | Provide a fast, concurrent grep-like tool using Kain's actor system for parallel file scanning (up to 8 workers). Outputs to repo-root `kg.exe` | `actor` system for parallel worker sharding, `std::fs` for file enumeration and reading, `std::process` for user args, `std::text`/`std::time`, `std::runtime`, CLI flag parsing from first principles, recursive directory traversal with ignore logic (.git, .kain, node_modules, target, bazel-\*), batch distribution (16 files per push) |

______________________________________________________________________

## `_old/` — Archived Projects

| Project | What It Is | Status |
|---------|-----------|--------|
| **fast3d-runtime** | Rust crate (`kain-fast3d-runtime`) for rendering Super Mario 64 Fast3D scenes — extracts SM64 title-face and level chunk scenes, GPU-accelerated via eframe/wgpu, PNG snapshots. Pure Rust, predates Kain's own GPU pipeline | Archived (pre-Kain GPU) |
| **kain-fsx** | Older Kain library for filesystem/path utilities — path resolution, joining, directory creation, text read/write with fallbacks, JSON file ops. Predates `std::fs` | Archived (superseded by stdlib) |
| **kain-process-kit** | Older Kain library for process/command execution helpers — process run, result-to-JSON, command formatting, ready logging, piped process execution. Predates `std::process` | Archived (superseded by stdlib) |

______________________________________________________________________

## Quick Stats

| Category | Active Projects | Scaffolds | Archived |
|----------|---------------|-----------|----------|
| `c/` (C ABI/FFI) | 14 | 0 | 0 |
| `cuda/` (GPU Compute) | 2 | 0 | 0 |
| `network/` | 3 | 0 | 0 |
| `sims/` | 3 | 0 | 0 |
| `python/` (Python Interop) | 4 | 2 | 0 |
| `three-d/` (3D Graphics) | 2 | 1 | 0 |
| `ui/` (UI Framework) | 5 | 0 | 0 |
| `kain/` (Starter) | 1 | 0 | 0 |
| `edge_cases/` | 2 | 1 | 0 |
| `example/` | 1 | 0 | 0 |
| `experiments/` | 4 | 1 | 0 |
| `boundary/` | 1 | 0 | 0 |
| `lsp/` | 1 | 0 | 0 |
| `markscript/` | 1 | 0 | 0 |
| `templates/` | 4 | 4 | 0 |
| `test/` | 12 | 1 | 0 |
| `tools/` | 1 | 0 | 0 |
| `_old/` | 0 | 0 | 3 |
| **Total** | **61** | **10** | **3** |
