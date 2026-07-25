# Blades Catalog

Every project under `X:/blades/` -- what it is, what it's trying to achieve, and which Kain semantic layers it exercises.

______________________________________________________________________

## `c/` -- C ABI / FFI Integration

Projects that stress-test Kain's ability to interface with native C code through `include ... as ...`, `@extern`/`@link_name`, C bridges, and platform SDKs.

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **asm** | Space Invaders proof-of-concept with inline `asm()` blocks + Win32 GDI desktop painting | Prove Kain can execute inline x86-64 assembly (`rep stosd`, `rdtsc`, `mov dword ptr`) without any C bridge, and paint directly to the Windows desktop HDC via Win32 | `asm()`, `Unsafe`, raw pointers (`alloc_zeroed`, `ptr_to_int`, `mem_load`, `mem_store`, `decay`), `include <windows.h>` |
| **component_fuzz** | Extreme component stress test -- ~30 components across 10 sections: naked, stateful, computational, recursive, pointer-laden, actor-aware, deeply nested (10 levels) | Push every `component` boundary to breaking point and prove Kain's component/JSX system survives aggressive nesting, recursion, unsafe memory, actor integration, and world surface bindings | `component`, `world`, `entangle`, `patch`, `pulse`, `law`, `actor`, `shatter`, `collapse`/`observe`/`decay`, JSX rendering, `state`, raw memory ops |
| **component_minimal** | Minimal Win32 GDI window with interactive controls (click counters, debug toggles, animated sphere) + fast inverse square root | Show Kain's Win32 interop at its most direct: hand-rolled `@extern` declarations, manual message loop, GDI drawing, and the 0x5F3759DF fast inverse sqrt | `@extern`/`@link_name` FFI, `include <windows.h>`, GDI drawing, raw pointer math, `bitcast`, `Unsafe`, component JSX |
| **component_shader** | Interactive GPU Julia fractal shader editor -- Win32 window + SPIR-V compute shaders for Mandelbrot/Julia pan/zoom | Demonstrate the full Kain GPU pipeline: authored GLSL compute shader → SPIR-V compilation → dispatch → file-based staging readback → GDI rendering. Also proves a handle-leak workaround for the GPU runtime | `shader compute` with `StorageBuffer`, `comptime`, `dispatch`, `@extern` Win32/GDI FFI, `bitcast`, `fs_open`/`fs_write`/`fs_read`/`fs_close`, epsilon LUT precomputation |
| **ephemaris** | Portable GPS baseband generator control deck -- world map → Python ephemeris download → C bridge to gps-sdr-sim → PlutoSDR upload | Full end-to-end SDR pipeline: map coordinate selection, real ephemeris fetching, GPS IQ generation via vendored C library, and SDR upload. Proves Kain owns a multi-language, multi-stage RF toolchain | `std::ui` (full UI toolkit with fonts/textures/buttons/events), `std::process` (spawn/wait/capture), `std::json`, `std::fs`, `std::math`, `std::text`, `std::time`, C FFI bridge, Python interop, JSON config/state files |
| **ffmpeg** | FFmpeg C ABI gauntlet -- video decode via real FFmpeg headers (`libavformat`, `libavcodec`, `libswscale`) with Win32 presenter | Stress-test Kain's ability to include and use real FFmpeg headers via natural C includes alongside a hand-written bridge. Decode, checksum, and frame-count validate end-to-end | Mixed natural `include <libavcodec/avcodec.h>` + explicit C FFI library config, `@extern`/`@link_name` FFI, `std::process`, multi-file modular Kain project, full media decode pipeline |
| **include-natural** | Minimal proof-of-concept for natural C includes (`include native/native_math.h as nm`) -- auto-discovers sibling `.c` source, emits alias-aware externs | Prove the bare-minimum C ABI surface: no `KAIN.toml`, no `[c_ffi]` config, no tiers -- just a header, a `.c` file, and a Kain file | Natural C include (`include ... as ...`), auto-discovery of sibling `.c` sources |
| **minimal** | The absolute minimal C ABI proof -- even simpler than include-natural. One header, one `.c` file, one Kain file calling `m_add` and `m_mul` | Prove zero-configuration C interop: the simplest possible ABI bridge | Minimal natural C include, zero config needed |
| **nuklear** | Nuklear header-only GUI library natural include + Python/Pygame fusion renderer. Kain uses `include nuklear.h as nk` and drives a "Fusion Reactor" demo with worlds, actors, entangles, patches, and Pygame rendering | Prove Kain can import and use the Nuklear header API naturally while simultaneously driving a multi-runtime fusion: Kain semantic layers (worlds, actors, entangle, patch, pulse, laws, shatter) + Pygame rendering | `include nuklear.h as nk`, `world`/`entangle`, `actor` with `ask`, `patch`, `pulse`, `law`, `shatter`, `import pygame`, Python interop (`python_call_attr_raw`), Pygame surfaces/fonts/draw |
| **opengl** | Raw Win32/WGL OpenGL compatibility blade -- creates an OpenGL window, renders triangles, writes a report | Prove Kain can drive OpenGL through a C bridge library linking against `opengl32.lib`, `user32`, and `gdi32` | C FFI bridge (`c::opengl_bridge`), `[c_ffi]` library config with shared lib and link libs, Kain facade module |
| **platform/windows** | Proof that libclang parses the real Windows SDK `<windows.h>` -- 6,294 function declarations extracted with zero shim | Demonstrate that Kain can include the full `<windows.h>` from the real Windows SDK and call `MessageBoxA` directly -- no hand-written shim, no bridge code, no macro workarounds | `include <windows.h>` system header include, zero-shim Win32 ABI access |
| **sqlite** | SQLite amalgamation natural include smoke test -- proves Kain can import the real SQLite amalgamation (`sqlite3.h` + `sqlite3.c`) without a hand-written `[c_ffi]` entry | Call `sql_libversion_number()`, `sql_threadsafe()`, `sql_complete()` -- all auto-generated from the amalgamation | Natural include of SQLite amalgamation (`include sqlite3.h as sql`), auto-discovery of sibling `.c` source, zero-manifest C integration |
| **vkcvg** (vkvg) | Vulkan Canvas Vector Graphics -- 36 directories of Vulkan canvas rendering with Kain bindings | Prove Kain can drive Vulkan vector graphics through a C bridge | C FFI Vulkan bridge, Vulkan canvas API bindings |
| **vcpkg** family (vcpkg, vcpkg_inline_smoke, vcpkg_lz4_demo, vcpkg_minizip_demo, vcpkg_nuklear_demo, vcpkg_zlib_compress) | vcpkg-powered C library integration demos: LZ4, minizip, Nuklear via vcpkg, zlib compress | Prove Kain can consume C libraries fetched and built via vcpkg without hand-written bridge code | `include <...> version as ...` vcpkg auto-fetch, `@extern` FFI, `KAIN_C_FFI_AUTO_FETCH` env var |

In addition to the sub-projects above, `c/` also contains two archived RAR files:
- `vulkain.rar` -- Raw reusable Vulkan window package for Kain LLVM blades (superseded by `gpu/` projects)
- `vulkan_v2.rar` -- GPU fluid simulator with compute shader variants (superseded by `gpu/fluid-studio`)

______________________________________________________________________

## `cuda/` -- CUDA/PTX GPU Compute

Projects that stress-test Kain's CUDA/PTX GPU compute pipeline -- authored kernels, multi-stage compute, and MCP tooling.

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **mcp** | GPU-accelerated semantic search MCP tool for the Kain repository -- indexes crates/runtime and authored Kain files, then serves code search through Kain-authored CUDA scoring and top-k kernels | Full CUDA compute pipeline: authored shader compute kernels compiled to PTX, dispatched from Kain host code, with MCP JSON-RPC server frontend. Indexes codebase for semantic code search | `shader compute`, `dispatch`, CUDA/PTX codegen, `StorageBuffer`, MCP protocol server, MCP tools (search, reindex, health), `semantic-search.exe` built artifact |
| **ptx_1** | Author-first CUDA/PTX blade -- Kain drives multi-stage compute and a native C++ reference comparator | Prove Kain can author and dispatch CUDA/PTX compute kernels with a native C++ bridge providing visual output verification | `shader compute` with CUDA target, C FFI bridge (`cuda_visual_bridge`), native C++ comparison harness, `run.ps1` for end-to-end verification |

______________________________________________________________________

## `gpu/` -- GPU Projects & 3D Applications

GPU-accelerated applications that fuse Kain's shader pipeline, Kaintana UI framework, Vulkain Vulkan engine, and the semantic stack into full desktop applications.

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **chronosim** | Chrono-simulation project -- scaffold with config, native C bridge, and Kain source | Foundation for GPU-accelerated time/physics simulation | `shader` pipeline, C FFI, `component`, config-driven architecture |
| **fluid-studio** | Data-driven Kain fluid simulator with Kaintana controls, authored GPU shaders (compute + fragment surface), and a Vulkain 3D presentation lane | Full GPU fluid simulation with interactive Kaintana UI panel for parameter adjustment. Builds to SPIR-V + LLVM dual-target. Depends on Kaintana + Vulkain + kain-json | `shader compute` + `shader fragment`, `world`/`entangle`, `component`, `patch`/`law`, `converge`, `orchestrate`, C FFI (`kaintana_desktop_bridge`, `vulkain_bridge`), SPIR-V codegen, `certify_gate`, `native_executable` |
| **kloner** | Faithful Kain-native workstation recreation of the legacy KCloner operator -- a 3D clone/scatter tool with grid, radial, honeycomb, and helix layouts | Full desktop application with Vulkan hardware rendering, Kaintana UI overlay (sidebar controls, inspector, charts, transport controls), clone layout math (vec3/quat/mat4), Catmull-Clark subdivision, presenter reporting. Designed as a build+certify capsule | `world`/`entangle`, `law`/`patch`, `component`, C FFI (`use c::kaintana_desktop_bridge`, `use c::vulkain_bridge`), Kaintana UI framework, Vulkain Vulkan engine, `certify_gate`, `native_executable`, vec3/quat/mat4 math, `hsv_to_rgb`, `fbm2` noise |
| **spirv-visualizer** | Data-driven SPIR-V capability visualizer for Kain-authored shader artifacts -- examines SPIR-V binaries and visualizes capabilities, extensions, and resource bindings | Prove Kain can parse and inspect SPIR-V artifacts it generates, visualizing shader capabilities in a Vulkain window. Depends on kain-config, kain-fsx, kain-json, kain-fmt, vulkain | `shader`, SPIR-V reflection, `component`, C FFI (`vulkain_bridge`), `certify_gate`, `native_executable`, config-driven architecture, JSON/Runtime contract loading |
| **zender** | GPU-accelerated data-driven sculpting system -- a Kain-native ZBrush clone with Vulkan rendering, Catmull-Clark subdivision, and 7 brush types | Load GLB 3D assets → Catmull-Clark subdivision → GPU particle scene → Vulkan window with orbiting particles → GPU compute brush kernels (Clay Build-Up, Smooth, Pinch, Inflate, DamStandard, Move, Flatten) → detailed telemetry/reports. Full sculpting benchmark with 7 brush types and 6 GPU compute shaders | `world`/`entangle`/`mirror`, `shatter struct`, `teleport`, `law`/`patch`, `converge`, `component`, `shader compute` (6 GPU brush kernels with StorageBuffer uniforms), `spirv`/`cuda` compile targets, C FFI (`include native/zender_vulkan.h`), vec3/quat/mat4 math, `pulse`, `certify_gate`, extensive JSON/report writing |

______________________________________________________________________

## `experiments/` -- Experimental / Research Projects

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **challenger** | Challenger experiment -- 9 files, build.kn + src | Experimental project scaffold | Standard Kain project structure |
| **convergence** (Schrödinger's Rats) | Maze-solving simulation where three rats (BFS, A*, Random Walk) compete every frame -- `converge` picks the winning strategy, `orchestrate` runs all three algorithms, Python/pygame shows colored trails | Prove that Kain's semantic constructs (`converge`, `orchestrate`, `world`, `patch`, `law`, `actor`, `shatter`, `pulse`, `teleport`) are general-purpose relationship descriptors, not domain-locked to CPU dispatch. The same `converge` that picks an AVX2 lane can pick a maze-solving strategy | `converge` with `spec reference`, `fast` lanes, `verify random(8)`, capability-based lane selection; `orchestrate` for typed multi-algorithm composition; `world`/`patch`/`law`/`actor`/`shatter struct`/`pulse`/`teleport`/`collapse`/`decay`, `ptr<Int>` raw buffers, `alloc_zeroed`, Python interop |
| **neural_lattice** | Semantic entanglement visualization -- OpenGL window showing Kain's compiler-owned semantics in real time. Kain computes a "neural lattice" (128 synapses) through worlds, entangles, collapse/observe/decay, converge, actor, pulse, teleport. C side renders a dual-waveform visualization with 5 interactive modes | Demonstrate a computation-then-visualization split where Kain owns semantics and C owns pixels. The bridge is 22 integers -- no pointers, no structs, no callbacks. Every Kain semantic construct's effect is directly visible in the OpenGL window | `world` (3 worlds: CorticalAuthority, DeepMirror, RogueProjection), `entangle` (3 couplings), `law`, `patch`, `converge`, `actor` (NeuralIgniter), `pulse` (4ms jitter 1ms), `teleport`, `collapse`/`observe`/`decay`, `shatter struct` (ShatteredSynapse), raw `ptr<Int>` buffers, C bridge (`use c::neural_lattice_bridge`), proof guards with specific error exit codes |
| **pong** | Pong game implementation with world/entangle/actors/ownership model -- 1460x900 Win32 OpenGL window, vector arcade oscilloscope aesthetic, two panels (authority + mirror), 100,000 swarm particles, chaos mode, drift detection | A "native UI Pong state-lattice demo for Kain worlds, entangle, actors, and ownership transitions." The authority world owns the game state, the mirror world is entangled for comparison/drift detection. 18 entangled fields including paddle, ball, score, swarm energy. Includes Z3 proof scaffolding | `world` (PongAuthority, PongMirror with 18 state fields each), `entangle`, `use c::pong_window_bridge` C FFI, `component App()` with JSX-like render, JSON-driven configuration via `load_pong_config`, `@extern` function declarations, theme system with structured layout constants |
| **quantum_entangled_automata** | Cellular automata simulation using Kain's quantum/entanglement semantics -- imports `std::proof`, `std::bench`, `std::attrition`, `std::certify` | A formally verifiable, benchmarked, attrition-tested cellular automata experiment with certification evidence. Scaffold stage with build graph but minimal implementation | `std::proof`, `std::bench`, `std::attrition`, `std::certify` evidence toolkit, build graph with check task and native executable |

______________________________________________________________________

## `three-kn/` -- Kain-Native 3D Rendering Engine

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **three-kn** | A portable, pure-Kain reimagining of Three.js -- 19 flat source files compressing ~220,000 lines of JS+GLSL into ~2,500 lines of Kain | Prove that Kain's full semantic stack (world, entangle, law, patch, converge, orchestrate, shatter, actor, pulse, component, shader, axiom, resonate, teleport, collapse/observe/decay) can replace thousands of lines of manual state management, dispatch, timing, coupling, and pipeline code in a production-grade 3D engine. Every construct maps to a specific rendering concern | `world` (8 worlds: SceneGraph, Material, Light, Texture, GPU, Camera, Engine, Audio), `entangle`, `law`/`patch`, `converge` (9+ material lanes, camera projection dispatch, backend selection), `orchestrate` (render_frame DAG, PMREM pipeline), `shatter struct` (GeometryBuffer, LightData), `actor` (AnimationMixer, AudioPlayer), `pulse` (animation_tick, render_loop), `component` (OrbitControls, TrackballControls, 11 helpers), `shader` (7 vertex + 10 fragment variants + compute kernels), `axiom` (backend selection), `resonate`, `teleport`, `collapse`/`observe`/`decay`, `trait Curve<T>`, `enum`, `impl` |

______________________________________________________________________

## `greeble/` -- Erlang-Style Actor Server Framework

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **greeble** | A portable, pure-Kain Erlang/OTP-style actor server framework -- 11 flat source files implementing an HTTP server with supervision trees, worker pools, rate limiting, and live terminal dashboard | Provide a reference architecture and reusable framework for actor-based servers in Kain. Demonstrates the full actor lifecycle: spawn, send, ask, supervision trees (OneForOne, OneForAll), mailbox configuration, backpressure, and runtime telemetry | `actor` (RouterActor, WorkerActor, WorkerPoolSupervisor, RootSupervisor, RateLimiter, AuthGate, SessionActor), `world`/`entangle` (ServerAuthority/ServerMirror dual-world lock-free state), `law`/`patch`, `pulse` (live dashboard ticker), `std::http`, `std::net`, `std::actor`, `std::os`, supervision tree, CLI flag parsing, terminal dashboard with `\r` in-place refresh |

______________________________________________________________________

## `reson8/` -- The Kain Digital Audio Workstation (DAW)

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **reson8** | The world's first Kain-native DAW -- compiler-owned state, journaled undo, zero-copy metering, Python+ML interop, GPU-accelerated UI. 4 worlds, 21 source files, C bridges for audio/VST3/CLAP | Prove Kain can be a full professional-grade audio workstation: MixerWorld (transport, meters, tempo, loop, session, peak/rms), PluginWorld (registry, slots, scan -- 3 lanes: Kain-native, VST3/CLAP, Python), ThemeWorld (80+ color/text/spacing/animation properties), ProjectWorld (file path, history, undo). All UI reads go through entangle mirrors -- zero lock contention. Patches journal every mutation -- full undo/redo history. Laws verify invariants at compile time | `world` (4 compiler-owned state containers), `entangle` (lock-free mirror reads), `law`/`patch` (journaled undo, invariant enforcement), `pulse` (metronome, transport), `resonate`, `orchestrate`, `component`, C FFI (3 bridge pairs: audio_device, vst3_host, clap_host), `import` Python (Demucs, Matchering, RNNoise), vendored SDKs (miniaudio, vst3_sdk, clap), `std::process`, `std::json`, `std::fs` |

______________________________________________________________________

## `kaintana/` -- The Flagship UI Framework

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **kaintana** | **THE FLAGSHIP UI FRAMEWORK** -- retained+immediate mode GUI system built entirely in Kain with the semantic stack (world/entangle/resonate/axiom) as its backbone. 21+ widget types, 4 color themes, 3 platform backends (Desktop GDI, Vulkan, Winit). 30+ source files, 10 example apps. Builds as a `kain_library` with capsule/amalgamation support | Kain's answer to React + egui: compile-time reactive UI with hot-reload, keyboard action binding, agent intent injection (AI agents push UI events), IME, clipboard, menus, dialogs, popovers, focus management, scroll containers, React-style keyed reconciliation. Ships as reusable library that other blades depend on | `world`/`entangle`/`resonate`/`axiom`, `component`, `patch`/`law`, `converge`/`orchestrate`, C FFI, `std::ui` host session, `std::reload`, hot-reload integration, agent intent injection (`kaintana_action_push_agent_intent`), capsule/amalgamation build, `certify_gate`, 10 example apps (data grid, file explorer, keypad, mega button test, modal popup, resizable panel, tabbed pane, todo list, tour suite, auto layout, comprehensive, scroll) |

______________________________________________________________________

## `kain/` -- Self-Host Kain Compiler (kainc)

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **kainc** | The Kain-written Kain compiler -- lex, parse, typecheck, monomorphize, codegen→LLVM IR, JIT, orchestrate. 23 source files, build graph with check → test → native executable → certify pipeline | Bootstrap proof: Kain can compile itself. Full compiler pipeline implemented in Kain: tokenization, span tracking, error diagnostics, AST construction, parser, type system, effects checker, monomorphization, LLVM IR codegen (via LLVM FFI), JIT execution (metal, x86, ORC, cache), orchestration, CLI. The ultimate stress test of the language | `fn`, `struct`, `enum`, `trait`, `impl`, `match`, `use`, `pub mod`, `LLVM FFI` (llvm_ffi.kn), `JIT` (jit_metal.kn, jit_x86.kn, jit_orc.kn, jit_cache.kn), `orchestrate` (orchestrator.kn), `std::build` (build.kn), `certify_gate`, `source_tests`, 23-module flat source architecture |

______________________________________________________________________

## `markscript/` -- Markdown-Native Bytecode VM

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **markscript** | A **markdown-native bytecode VM** that serves as Kain's companion language for configuration, orchestration, UI scripting, build systems, and executable documentation. Your README IS the executable. Compiles to native `.exe` via Kain's LLVM backend. 23 VM opcodes, 78 IVT handlers, 13 CLI subcommands, 114 test cases, 6 Z3 proofs, 17 benchmarks | Prove that markdown can be a first-class executable format. Headings are domains (`#`), sections are routines (`##`), blockquotes are intents (`>`), tables are data matrices (`|`), code blocks are extracted. Markdown has no syntax errors -- only runtime errors. Provides `mks run/build/check/disasm/repl/eval/init/handlers/doc/pipe/watch/test/clean` CLI | `fn`, 23 custom VM opcodes, 78 IVT handlers (stdlib, process, UI events), 13 CLI subcommands, `std::markscript` embedding module, UI event bridge, schema validation, code generation, layered config merge, Z3 proof packs, benchmark suite, self-hosting (README is a valid markscript program), `kain build` → standalone `mks.exe` |

______________________________________________________________________

## `lsp/` -- Language Server Protocol + MCP Server

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **kain-lsp** | Kain LSP + MCP dual-protocol server -- Language Server Protocol for editors and Model Context Protocol for AI coding agents. Includes VS Code extension (`.vsix`), generated LSP type bindings, and full source tree | Provide editor intelligence (completion, diagnostics, hover, goto-def) and AI agent tooling (MCP JSON-RPC server) for Kain development. Dual-protocol architecture: stdin/stdout LSP for VS Code, MCP for AI agents | `std::build`, `std::test`, `test_suite`, `native_executable`, LSP protocol (JSON-RPC), MCP protocol, VS Code extension packaging (`pack_vsix.kn` + `pack_vsix_impl.js`), generated type bindings, 183+ source files |

______________________________________________________________________

## `os/` -- KAINOS Kernel

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **kainos** | **KAINOS** -- a single-address-space actor kernel written in Kain. 18 subsystem directories (arch, kernel, mm, fs, net, drivers, ipc, security, compat, ui, init, lib, runtime, test, research, spec), targeting `x86_64-unknown-none` bare metal with multiboot2 boot | Build a complete operating system kernel in Kain: boot code (GDT, IDT, paging, APIC, HPET), kernel core (scheduler, actor lifecycle, converge, panic), memory management (physical/virtual, heap, world isolation), VFS, FAT32, ext4, TCP/IP stack, PCI/NVMe/USB/GPU/HID drivers, IPC (messages, mailboxes, supervision), security (ownership gates, capability system), compatibility (PosixActor, ELF translator, WINE bridge), UI (compositor, Wayland, desktop shell). Multi-stream parallel build with 8 spec streams | `freestanding` target, `no_std`, `bare_metal`, `linker_script`, `asm_object` (assembly bootstrap), `subproject` (multi-module kernel), `static_library`, `x86_64-unknown-none` triple, 14 subproject dependencies, QEMU boot target |

______________________________________________________________________

## `web/` -- Kauri HTTP Server + TypeScript Frontend

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **kauri** | Kauri: Kain HTTP server + TypeScript frontend, zero bloat. Pure-Kain HTTP server on localhost:9090, routes API calls to Kain actors, serves static TypeScript frontend, launches Edge/Chrome app-mode webview. Includes Greeble supervision tree | Prove Kain can be a full-stack web application server: HTTP routing → actor handlers → world state → JSON responses → TypeScript frontend. Zero C code, zero build deps, zero 1000-crate dependency trees | `actor` (KauriApi, StaticFileServer, http_route_actor, RateLimiter, AuthGate), `world`/`entangle` (ServerAuthority/ServerMirror), `law`/`patch`, `std::net`, `std::http`, `std::os` (webview launch), `std::fs` (static file serving), Greeble supervision tree, TypeScript frontend (`kauri-client.ts`, `app.ts`, `index.html`), CLI flag parsing |

______________________________________________________________________

## `python/` -- Python Interop Blades

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **24_tet** | 24-TET microtonal music system with Python interop -- `world ResonatePyAuthority` + `ResonatePyMirror`, keyboard UI, pitch computation, Pygame rendering | Full Python interop demo: Kain owns the state (note grid, pitch tables, keyboard), Python/Pygame renders the UI. 24-tone equal temperament music theory in Kain with Python visualization | `world`/`entangle`, `import pygame`, `component`, Python interop, `patch`, `law`, 24-TET pitch tables, note names, keyboard slot mapping |
| **actor_relay** | Actor relay pattern with Python bridge -- Kain actors communicating through Python interop | Prove Kain actors can interoperate with Python host objects | `actor`, `send`, `ask`, `import` Python, `python_call_attr_raw` |
| **data_gang** | Data pipeline gang scheduling via Python interop | Prove Kain can orchestrate multi-stage data pipelines with Python backends | `orchestrate`, `actor`, `import` Python |
| **dear_dashboard** | Dear ImGui-style dashboard via Python interop | Prove Kain can drive Python GUI frameworks for real-time dashboards | `import` Python, `world`/`entangle`, `pulse` |
| **flet_monitor** | Flet (Flutter) monitoring dashboard via Python interop | Prove Kain can drive Flet (Flutter-in-Python) UI from Kain state | `import flet`, `world`, `pulse` |
| **kainbleton** | Kainbleton -- Ableton Live-inspired DAW mixer deck. World KainbletonAuthority, actor KainbletonRenderConductor, 16 modules | Full Python interop audio workstation concept: mixer deck UI, transport controls, waveform preview, Python audio backend | `world`/`entangle`, `actor`, `component`, `import` Python, `pulse` |
| **moderngl_shader** | ModernGL GPU shader demo via Python interop | Prove Kain can drive ModernGL for GPU-accelerated rendering through Python | `import moderngl`, `shader`, GPU interop |
| **nicegui_api** | NiceGUI web UI API demo via Python interop | Prove Kain can serve web UIs through NiceGUI (Python web framework) | `import nicegui`, `world`, `actor` |
| **pong_god** | Pong "God Mode" -- the ultimate Pong implementation with Python/Pygame rendering | Full arcade game: Kain owns game state, physics, scoring; Python/Pygame renders | `world`/`entangle`, `patch`/`law`, `import pygame`, `component` |
| **py_c_test** | Python-C interop test -- combined Python and C bridge testing | Stress-test Kain's ability to use both Python and C bridges simultaneously | `import` Python, `include` C, `@extern`, mixed interop |
| **pyglet_3d** | 3D rendering via Pyglet (Python OpenGL framework) | Prove Kain can drive 3D rendering through Python's Pyglet library | `import pyglet`, 3D math |
| **python_interop_god** | The Python interop "God file" -- 242 KB, 5,233 lines amalgamated, 19 modules, exercises every Python interop path | Comprehensive Python interop stress test: imports NumPy, PyTorch, PIL, Pygame, Flet, Dear ImGui, ModernGL, NiceGUI, Pyglet, Tkinter, wgpu, and more. Exercises buffers, tensors, images, GPU interop, async futures, actor callbacks, region caches | `import` (every major Python package), `from ... import`, `py_eval`, `py_call`, `py_getattr`, `py_buffer_view`, `kain_tensor_from_py`, `kain_image_from_py`, `python_gpu_storage_buffer`, `python_shared_buffer`, `python_region_begin/end`, `py_call_async`, `python_actor_callback`, `std::python` |
| **tkinter_editor** | Tkinter-based text editor via Python interop | Prove Kain can drive Tkinter for native desktop UI | `import tkinter`, `component` |
| **wgpu_voxel** | wgpu (WebGPU) voxel renderer via Python interop | Prove Kain can drive WebGPU through Python's wgpu-py for GPU-accelerated voxel rendering | `import wgpu`, GPU interop, `shader` |

The `python/` directory also contains `py_kn/` -- a deep sub-project with 12 subdirs and 226 files for advanced Python interop testing.

______________________________________________________________________

## `network/` -- Network Test Proofs

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **domains** | Network domain proofs -- exercises `std::net` across multiple domains | Prove Kain's networking stack works across different protocols and address families | `std::net`, `std::http`, `std::os` |
| **http** | HTTP server/client test proofs | Prove Kain's HTTP implementation works for both server and client roles | `std::http`, `std::net`, `KAIN.toml` project |
| **json** | JSON over network test proofs | Prove Kain can serialize/deserialize JSON over network connections | `std::json`, `std::net`, `std::http` |

______________________________________________________________________

## `edge_cases/` -- Compiler / Runtime Edge Case Tests

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **GPU** | Debug template clone -- rapid Kain edge-case testing with 4-layer architecture (cause → effect → spookymagic → diagnostics) + VM isolation + self-replicating cloner | Provide a surgical instrument for rapid single-bug reproduction. Contains self-replicating cloner (`spawn.kn`) that duplicates the template with one command | `std::fs`, `std::path`, `std::process`, `std::runtime`, `std::text`, CLI flag parsing, `runtime_init`/`runtime_shutdown` lifecycle, self-replicating project cloner |
| **QUARANTINEZONE** | Quarantine zone for dangerous/breaking edge case tests | Isolate tests that may crash the compiler or runtime from the main edge_cases suite | Various, isolation wrapper |
| **actor** | Comprehensive Kain actor system edge case testing suite -- 42 tests across 15 categories: lifecycle, send/cast, ask/call, mailbox, registry, monitor, link, supervision, scheduler, worker pool, GenServer, game loop (UE5-style Input→Physics→Render), fusion chain, stress (64 spawns, 256 asks), telemetry delta guards | Prove every layer of the actor runtime works: basic lifecycle through Erlang-style supervision trees, UE5-style game loop pipelines, stress tests, and telemetry delta guards that mathematically prove the scheduler actually processed messages | `actor`, `spawn`, `send`, `ask`, `on`, `state`, `reply_to`, `pack`/`unpack`, supervision tree (OneForOne, OneForAll), `GenServer` (init/call/cast/info), `actor_monitor`, `actor_link`, `actor_registry`, mailbox, scheduler telemetry deltas, 42 discoverable tests with error code taxonomy |
| **codegen_edge_gaps** | Precision regression test suite for 6 LLVM codegen edge-case gaps discovered during markscript development | Capture and fix 6 distinct LLVM codegen failure modes: `::` leaking into LLVM type names, `py_getattr_raw` fallback firing incorrectly for Kain-to-Kain struct access, named-field enum variant destructure failures, function pointers missing from resolver, `return` in match arm producing dead PHI predecessor, PHI node predecessor mismatches from `break`/`continue` | 4-layer architecture (cause → effect → spookymagic → diagnostics), VM isolation wrapper, test table pattern with discoverable test registration, CLI flag parsing |
| **component** | Component system edge case tests -- 29 subdirs, 544 files -- massive test suite for component/JSX compiler pipeline | Push the component system to its limits: nested components, recursive JSX, state management, event handling, world surface binding, every edge case in the vtable pipeline | `component`, `world`, `surface native_ui`, JSX, `state`, vtable pipeline |
| **error_check** | Error checking edge cases -- 21 subdirs, 182 files -- test that the compiler produces correct error messages for every error category | Verify error diagnostics: parse errors, type errors, borrow errors, effect errors -- each subdir targets a specific error code family | All Kain constructs in error states, error code taxonomy |
| **py** | Python interop edge cases -- 3 subdirs, 23 files | Stress-test Python interop boundary conditions | `import` Python, `py_eval`, `py_call`, buffer views, async futures |
| **regression_harness** | Regression test harness -- 16 files | Automated regression testing infrastructure for catching regressions across compiler versions | `std::process`, `std::fs`, `std::test`, automated test runner |
| **runtime** | Runtime edge case tests -- 10 files, 628 KB | Stress-test the Kain native runtime: init/shutdown, memory, actors, telemetry | `std::runtime`, `runtime_init`, `runtime_shutdown`, `runtime_heap_validate`, all runtime telemetry counters |
| **window_spawn** | Window spawn edge case tests -- 8 subdirs, 15 files | Test window creation and lifecycle edge cases | `world` + `surface native_ui`, window lifecycle, GDI backend |

______________________________________________________________________

## `test/` -- Proof Tests

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **actor-ask-roundtrip** | Minimal actor ask roundtrip proof -- Echo actor + Gate actor, typed `ask()` + `ask_timeout()` | Prove the fundamental actor ask/response pattern works end-to-end | `actor`, `spawn`, `ask`, `ask_timeout`, `reply_to`, `send reply_to.Reply()` |
| **amalgamate-capsule-probe** | Amalgamate capsule resolution probe -- tests that capsule imports resolve correctly | Prove the amalgamate capsule import pipeline works for dependency resolution | `use` from capsule, amalgamate pipeline |
| **build-kn-system-smoke** | Build system smoke test -- 17 subdirs, 20 files. Exercises every build.kn DSL construct | Verify the build graph DSL works end-to-end: project, blade, package, check_task, native_executable, source_tests, certify, capsule_set, source_set, build_defaults, run_defaults, workspace_defaults, platform_package, subproject, asm_object, platform_requirement, build_check, test_suite, certify_gate | `std::build` DSL (all constructs), build graph DAG, evidence pipeline |
| **converge-autotune-probe** | Converge autotune probe -- tests that converge lane selection auto-tunes correctly | Verify the converge autotune mechanism selects the right lane at runtime | `converge`, `spec reference`, `fast`, `verify random(N)`, lane selection |
| **format** | Format tests -- empty directory scaffold | Reserved for format/fmt edge case tests | -- |
| **hash-domains** | Hash function domain proofs -- exercises `std::hash` across multiple hash families | Prove all hash functions in stdlib produce consistent outputs | `std::hash`, hash domains (Wang, FNV-1a, CRC32, Fingerprint32) |
| **machine-stones** | Machine stones test -- exercises `axiom`, `pulse`, `shatter`, `teleport` runtime contracts | Verify the machine stones subsystem (Layer 6) compiles and links correctly | `axiom`, `pulse`, `shatter struct`, `teleport`, runtime contracts |
| **math-domains** | Math function domain proofs -- exercises `std::math` across multiple math domains | Prove all math functions in stdlib produce correct results | `std::math`, vec3, quat, mat4, noise, FBM |
| **platform** | Platform compatibility test suite -- 21 subdirs, 181 files | Verify Kain compiles and runs correctly across different platform configurations | `platform_package`, `std::platform`, cross-platform conditional logic |
| **platform-package-smoke** | Proof blade for the platform-package lock/import system | Test that platform packages (external native SDKs) can be declared, locked, and imported | `platform_package()` in build DSL, `std::platform` library open/resolve/close |
| **stdlib-domains** | Proof blade that imports and exercises the entire root stdlib domain surface -- 24 stdlib modules, ~350 lines | Verify that all major stdlib domain imports resolve and their core functions work over the native stdlib profile | 24 stdlib imports (`std::runtime` through `std::reload`), actor spawn/send/shutdown, input system with key events, HTTP/2 request creation, GPU compute + graphics shared resources, UI session with nodes/text/state, hot-reload with migration plan |
| **stdlib-foundations** | Proof blade for `std::text`, `std::collections`, `std::crypto`, `std::alloc` -- ~400 lines with 6 probe functions | Exercise the foundational stdlib in depth: text views/strings, ASCII utils, semver parse/compare/format, JSON construction/parsing, fmt writer, typed maps, queues, deques, priority queues, slot maps, SHA-256, HMAC-SHA256, BLAKE3, random bytes, bump/arena/pool allocators | `std::text`, `std::ascii`, `std::fmt`, `std::json`, `std::semver`, `std::collections` (typed_map, queue, deque, priority_queue, slot_map), `std::crypto` (sha256, hmac_sha256, blake3, random_bytes), `std::alloc` (bump/arena/pool), `Unsafe`, `decay` |
| **windows** | Win32 native window test blade -- two approaches: pure `@extern` to `user32!MessageBoxA` and `include native/win32_window.h` for full Win32 window with WNDPROC | Prove Kain can call native Windows APIs via two methods: (1) zero C sidecar with `@extern` annotation directly to DLL exports, and (2) C header include with sibling `.c` for full window creation. Key demonstration of Kain's native ABI capabilities on Windows | `@extern`/`@link_name` for direct DLL function imports, `include native/win32_window.h as win`, C sibling source discovery (`.c` sidecar), native Win32 window creation with WNDPROC |

______________________________________________________________________

## `example/` -- Canonical Kain Example / Interactive Workbench

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **example** | The definitive "first file future agents should inspect" -- most comprehensive example in blades (154 KB Kain source, 12 files). Full Kain surface: UI, graphics, input, networking, actors, worlds, themes, layout, runtime workbench | Serve as the canonical reference for what fully authored Kain looks like. Demonstrate every major Kain subsystem: enums, match, for loops, vec!/format!/println macros, observe/collapse/decay ownership, worlds, actors, native stdlib services, raw memory, shaders, UI, graphics, process, net, fs, input, effects, async values. Verify LLVM IR contains correct ABI calls | `match`, `for`/`range`, `vec!`/`format!`/`println!`, `observe`/`collapse`/`decay`, `world`, `actor`, `@extern fn`, `entangle`, UI host attachment, graphics ABI, input system, `NativeMetric` trait, 10+ native stdlib subsystem probes. 12 files: `main.kn`, `generic.kn`, `ui.kn`, `workbench_labs.kn`, `episode_graphics.kn`, `episode_input.kn`, `episode_layout.kn`, `episode_network.kn`, `episode_pages.kn`, `episode_strings.kn`, `episode_theme.kn`, `episode_ui_helpers.kn` |

______________________________________________________________________

## `benchmark/` -- Microbenchmark Runner

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **bench** | Kain microbenchmark runner -- fast, auto-scaling, solver-ready. Drop `.kn` files in a folder, each file self-times using a template. Auto-scales iterations to hit ~1000ms target. Reports min/median/mean/max, checksum verification, JSON output | Provide zero-friction performance measurement for any Kain code. Designed for solver-driven optimization loops (Z3/autoresearch). Supports build-and-run, list mode, verbose output, JSON telemetry | `std::time`, `std::fs`, `std::path`, `std::os`, `std::runtime`, `std::text`, `std::ascii`, auto-scaling iteration loop, statistical aggregation (min/max/median/mean), checksum verification, JSON output, template-based bench authoring (`template.kn`) |

______________________________________________________________________

## `semantic-search/` -- Cross-Subsystem Codebase Search

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **semantic-search** | A pure-Kain, CPU-only, BM25+FST inverted index that traces connections between Kain keywords, stdlib modules, Rust bootstrap crates, and C runtime implementations. Indexes `crates/` + `runtime/native/` + `stdlib/` -- answers "where does this concept live across all three domains?" | Build a search engine that understands Kain's multi-substrate architecture. Every indexed symbol carries a profile showing its cross-subsystem connections (e.g., `entangle` traces from Kain keyword → stdlib module → Rust crate → C runtime ABI). Uses `shatter struct` for SoA index layout and `converge` for search algorithm dispatch | `shatter struct` (SoA inverted index), `converge` (BM25 vs TF-IDF dispatch), `world` (index state), `law`/`patch`, BM25+FST implementation, cross-subsystem connection profiling, `std::fs`, `std::path`, `std::text`, `std::json` |

______________________________________________________________________

## `tools/` -- Utility Tools

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **kg** (killgrep) | Actor-sharded Kain grep CLI tool -- full-featured file search utility with pattern matching, recursive directory traversal, case-insensitive search, line numbers, files-only mode, count mode, hidden file support, stats, worker count control | Provide a fast, concurrent grep-like tool using Kain's actor system for parallel file scanning (up to 8 workers) | `actor` system for parallel worker sharding, `std::fs` for file enumeration and reading, `std::process` for user args, `std::text`/`std::time`, `std::runtime`, CLI flag parsing from first principles, recursive directory traversal with ignore logic (.git, .kain, node_modules, target, bazel-*), batch distribution (16 files per push) |

______________________________________________________________________

## `templates/` -- Project Templates

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **cli** | CLI project template -- 5 files, 390 KB (includes pre-built .exe) | Quick-start template for CLI tools with flag parsing | `std::os`, `std::process`, CLI arg parsing, `build.kn` |
| **debug** | Debug template for rapid edge-case testing -- self-replicating cloner (`spawn.kn`), 4-layer architecture (cause/effect/spookymagic/diagnostics), VM isolation | Provide a surgical instrument for rapid single-bug reproduction. Clone with one command, write code in `cause.kn`, run diagnostics. Self-replicating | `std::fs`, `std::path`, `std::process`, `std::runtime`, `std::text`, CLI flag parsing, self-replicating project cloner, 4-layer test architecture |
| **markscript** | Markscript project template -- 1 file | Quick-start template for markscript projects | MarkScript project scaffold |
| **python** | Python interop template -- 31 subdirs, 542 files, 13.3 MB | Full Python interop project template with all major packages pre-configured | `import` Python, `std::python`, comprehensive Python package integration |
| **starter** | Minimal starter template -- 2 files (`build.kn` + `src/main.kn`) | Absolute minimal "hello world" starting point for new Kain projects | `build.kn`, `native_executable`, basic project structure |
| **test** | Test project template -- 4 subdirs, 8 files | Template for test-driven Kain projects with std::test, std::proof, etc. | `std::test`, `std::proof`, `std::bench`, test suite structure |
| **ui** | UI project template -- Interactive Hex Color Mixer with real text input, live color preview, preset swatches. Pure Kain `std::ui` | Template for Kain UI applications using the std::ui framework with interactive controls and live preview | `std::ui`, `component`, `world`, `build.kn`, color math |

______________________________________________________________________

## `boundary/` -- FFI Boundary Demos

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **rs** (Rust) | Rust FFI boundary demo -- 7 files, 1.2 MB | Prove Kain can call into Rust crates via FFI boundary | `use rust::`, Rust crate FFI, Cargo.toml bridge |
| **ts** (TypeScript) | TypeScript FFI boundary demo -- 16 files | Prove Kain can interoperate with TypeScript/JavaScript via FFI boundary | `use js::`, TypeScript interop, Node.js bridge |

______________________________________________________________________

## `shaderz/` + `shaderlib/` -- Shader Playground & Library

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **shaderz** | Interactive shader + component playground -- 4 demos: interactive_playground (4 shader modes: plasma/waves/spiral/kaleidoscope + mouse/keyboard + HUD), shader_dashboard (system monitor with shader background), particle_world (GPU particles + 4 themes + telemetry), multi_shader_world (one world, 3 shader surfaces sharing parameters) | Prove Kain can fuse GPU shader pipeline, component surface system, world state authority, and input handling into interactive animated experiences. Every demo follows: World → surface shader + surface native_ui → input → world fields → uniforms + component | `world` + `surface shader => Fragment` + `surface native_ui => Component`, `shader fragment` with uniform bindings, multi-surface worlds, `std::input` (keyboard, mouse, action bindings, axis bindings, agent intent), `component` JSX, `pulse`, GDI/Vulkan dual backend, branchless shader mode switching |
| **shaderlib** | Shader library reference files -- blackhole.kn (Schwarzschild black hole raymarcher), gpu_showcase.kn (all 12 shader stages in one file), ocean.kn (ocean wave shader), supermotion_v2.kn (motion blur shader), ocean_shader_world.kn (canonical surface shader pattern) | Provide reference implementations for every shader technique: raymarching, wave simulation, motion blur, fullscreen quad patterns, multi-stage shader surfaces | `shader fragment`, `shader vertex`, `shader compute`, `surface shader`, uniform bindings, raymarching, procedural generation |
| **shaderz2** | Empty directory -- scaffold for next-gen shader playground | Reserved for future shader experiments | -- |

______________________________________________________________________

## `training/` -- Training Files

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **THE_BEAST.kn** | 18.2 KB Kain training file -- comprehensive syntax and semantics reference | Serve as a compact training reference for agents learning Kain syntax and semantics | Full Kain surface: all keywords, effects, constructs, stdlib imports, component/world/actor patterns |

______________________________________________________________________

## `ui_demos/` -- UI Ghost Harness & Testing

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **ui_demos** | Boringly reliable, fully automated UI testing for Kain. Python harness (`harness.py`) builds `.kn` → `.exe`, launches invisible ghost window, captures via `PrintWindow(PW_RENDERFULLCONTENT)`, analyzes with GEMMA 4 vision model. Includes organized test taxonomy (9 folders: component_ui, std_ui, std_ui_widgets, std_ui_input, vtable, shader_ui, vulkan, DX12, demos) | Gaslight-proof UI validation: prove a Kain app actually renders content (not blank, not crashed) without human visual inspection. The app runs completely invisible (alpha=1/255, click-through, hidden from Alt-Tab) while the GPU renders at full speed. GEMMA 4 vision model evaluates every screenshot for visual breakdown, aesthetics, and hardcoding assessment | `component`, `world` + `surface native_ui`, `std::ui`, `shader`, `surface shader`, Python harness (`harness.py`), Oracle integration, GEMMA 4 vision analysis, JSON + Markdown logging, batch testing, ghost capture |

______________________________________________________________________

## `window_proof/` -- Component Surface Pipeline Proof

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **window_proof** | Kain Window Proof -- Component Surface Pipeline Demos. 3 working demos (Minimal Window, World State, Dashboard), 6 known codegen/runtime gaps documented. Pre-built `.exe` artifacts for verification | Prove the full component surface pipeline works end-to-end: `world` + `surface native_ui => Component` creates a real OS window, 17-slot KainComponentSurface vtable is emitted and called correctly, GDI backend paints to screen. Document every vtable slot, every codegen gap, and the exact C/Rust files that enable each step | `world` + `surface native_ui => Component`, 17-slot `KainComponentSurface` vtable (session_create/destroy, element_begin/end, element_set_text/attr, state_get/set, begin/end_frame, present, poll_event, should_close, window_open, host_pump), GDI backend, Oracle verification, Vulkan plan, documented codegen gaps |

______________________________________________________________________

## `amalgamate/` -- The Katamari Protocol (Capsule Pipeline)

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **amalgamate** | Kain's amalgamate capsule pipeline -- pack any number of modules (2 to 2,594) into a single portable `.kn` capsule file. Import directly from capsule without unpacking. Proven at scale: 2,594 modules, 316K lines, 15 MB, 3 seconds to amalgamate, 3,211+ public symbols | Solve the permanent problem of code distribution. Capsules are first-class import targets -- the module resolution system reads them natively. Drop a capsule into any project, `use` whatever you need, the compiler resolves everything directly. No unpack. No install. No network. No lockfile. No version solver. One file. All the code. Forever | `amalgamate` CLI, `capsule_set()` in build.kn, `kain amalgamate inspect/unpack`, editable/archive capsule formats, content policies (source/snapshot/assets/artifacts/evidence), companion capsules, content-addressed cache, `use` from capsule imports, mass-scale proof (2,594→1 file) |

______________________________________________________________________

## `openkain/` -- Open Source Reference

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **openclaw-main** | OpenCLaw -- 167 directories of open-source game code reference | Reference implementation for Kain game development patterns | C/C++ game code, reference architecture |
| **reference** | Massive reference archive -- 1,059 subdirs, 19,644 files, 181.1 MB | Comprehensive reference material for Kain development: code samples, documentation, third-party library integration patterns | Reference archive, not directly executable |

______________________________________________________________________

## `private/` -- Private Testing & Development

| Project | What It Is | What It's Trying To Achieve | Key Kain Features |
|---------|-----------|----------------------------|-------------------|
| **private** | Kain's private testing folder -- build and test here before moving out into public repo. Contains 6 agent workstreams (A through E, plus NO_C), RAPID_FIRE parallel test suite (44 files, 2.9 MB), C_UI_GAUNTLET (UI stress testing), and Python interop god file | Pre-release testing ground: get EVERY Python package in `PIP.txt` imported through Kain's native `import` syntax and doing something visible in a pygame window. Zero .py files. Four agents (A/B/C/D), one winner. DO NOT GIVE UP | `import` Python (every pip package), `world`/`entangle`, `component`, `actor`, `pulse`, `import pygame`, black magic voodoo Python interop, multi-agent parallel development |

______________________________________________________________________

## Quick Stats

| Section | Active Projects | Scaffolds/Empty | Description |
|---------|---------------|-----------------|-------------|
| `c/` (C ABI/FFI) | 19 | 0 | C interop stress tests + 2 archived RARs |
| `cuda/` (GPU Compute) | 2 | 0 | CUDA/PTX compute pipeline |
| `gpu/` (GPU Projects) | 5 | 0 | GPU applications with Kaintana+Vulkain |
| `experiments/` | 5 | 0 | Research/experimental projects |
| `three-kn/` | 1 | 0 | 3D rendering engine |
| `greeble/` | 1 | 0 | Actor server framework |
| `reson8/` | 1 | 0 | Digital Audio Workstation |
| `kaintana/` | 1 | 0 | Flagship UI framework |
| `kain/` | 1 | 0 | Self-host compiler |
| `markscript/` | 1 | 0 | Markdown bytecode VM |
| `lsp/` | 1 | 0 | LSP + MCP server |
| `os/` | 1 | 0 | KAINOS kernel |
| `web/` | 1 | 0 | Kauri HTTP server |
| `python/` | 14 | 0 | Python interop blades |
| `network/` | 3 | 0 | Network test proofs |
| `edge_cases/` | 10 | 0 | Compiler/runtime edge cases |
| `test/` | 13 | 1 | Proof tests |
| `example/` | 1 | 0 | Canonical example |
| `benchmark/` | 1 | 0 | Microbenchmark runner |
| `semantic-search/` | 1 | 0 | Codebase search engine |
| `tools/` | 1 | 0 | Utility tools |
| `templates/` | 7 | 0 | Project templates |
| `boundary/` | 2 | 0 | FFI boundary demos |
| `shaderlib/` | 1 | 0 | Shader reference library |
| `training/` | 1 | 0 | Training reference |
| `ui_demos/` | 1 | 0 | UI ghost harness |
| `window_proof/` | 1 | 0 | Window pipeline proof |
| `amalgamate/` | 1 | 0 | Capsule pipeline |
| `private/` | 1 | 0 | Private testing |
| **Total** | **101** | **2** | |
