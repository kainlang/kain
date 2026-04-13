# KAIN

> **KAIN is a compiled multi-target language toolchain plus an embeddable runtime/host stack.**
>
> One `.kn` source can compile to web, native, GPU, UE5, scripting, and native UI outputs, and the same authored source can also orchestrate host-backed runtime bridges for C, Rust crates, Python, Node, and mixed-runtime pipelines.

This README is the top-level operational brief for the current Kain repo state in `M:\Code\Kain`.
It is written for engineers, agents, and future-you who need the truth of the current system, not a marketing summary.

The canonical long-form guide tree lives in `guides/`. The older `docs/`
tree is legacy support material and may lag behind the code.

Validated against:

- workspace manifest: `M:\Code\Kain\Cargo.toml`
- root workspace map: `M:\Code\Kain\repomap.md`
- canonical guide tree: `M:\Code\Kain\guides\README.md`
- research lane guide: `M:\Code\Kain\docs\research\README.md`
- docs pipeline index: `M:\Code\Kain\docs\pipeline\README.md`
- C runtime pipeline notes: `M:\Code\Kain\docs\pipeline\C_RUNTIME_PIPELINE.md`
- crate folder guide: `M:\Code\Kain\crates\README.md`
- crate workspace map: `M:\Code\Kain\crates\repomap.md`
- apps folder guide: `M:\Code\Kain\apps\README.md`
- toolchain folder guide: `M:\Code\Kain\toolchain\README.md`
- testing folder guide: `M:\Code\Kain\testing\README.md`
- Kain library guide: `M:\Code\Kain\kn_library\README.md`
- live CLI (if built): `M:\Code\Kain\target\debug\kain.exe`
- proof suites: `M:\Code\Kain\smoketest\*` and `M:\Code\Kain\labs\*`

---

## Read This First

- Prefer the modern subcommand CLI.
- Treat the live binary and source as truth when docs disagree.
- Keep runtime pipeline outputs out of the repo root; use `generated\` and clean `target\` after runs.
- Distinguish three layers before making claims:
  - Kain language/frontend features
  - importer or bridge behavior
  - backend/codegen/runtime support
- Not every feature is portable to every target.
- Host-backed runtime bridges are a major current pillar of Kain and are just as important as classic codegen.

Canonical commands:

```powershell
kain doctor
kain build
kain build native-ui
kain run
kain selfhost
kain omni
kain gpu-artifacts
kain inject
kain import-c
kain import-rust
kain import-crate
kain import-ts
kain import-asm
kain lsp
```

Bootstrap the repo-local toolchain and CLI with the root installer:

```bash
python3 install_kain.py
source generated/kain-env.sh
kain doctor
```

On Windows:

```powershell
py install_kain.py
. .\generated\kain-env.ps1
kain doctor
```

The installer resolves or installs platform LLVM, bundles `clang` into `toolchain/llvm/bin`, builds `kain`, installs `kain` and `kn` into the cargo bin directory, and emits activation scripts under `generated/`.

---

## Current State

The repo is no longer "just a compiler with some backends."

Kain today is a layered system with all of the following active in the workspace:

- core language frontend in `crates/kain-core`
- embeddable compiler orchestration in `crates/kain-driver`
- native Rust host runtime in `crates/kain-host`
- derive/reflection support in `crates/kain-host-derive` and `crates/kain-reflect`
- shared neutral interop contracts in `crates/kain-interop`
- Rust crate FFI generation and live bridge loading in `crates/kain-crate-ffi`
- C host-backed FFI lane in `crates/kain-c-ffi`
- runtime-facing Vulkan compute executor in `crates/kain-gpu-runtime`
- Python embedded runtime bridge in `crates/kain-python`
- JavaScript/Node bridge in `crates/kain-node`
- semantic UI compiler/runtime in `crates/kain-ui`
- native desktop UI runtime in `crates/kain-ui-native`
- 3D scene/renderer/interaction/runtime layer in `crates/kain-3D`
- mixed-language omni orchestration in `crates/kain-omni`
- embeddable SDK facade in `crates/kain-sdk`
- classic codegen targets for web/system/gpu/UE5

That means the current Kain story is:

1. Compile `.kn` to many targets.
2. Import foreign source into Kain.
3. Run `.kn` in host-backed execution lanes with live interop.
4. Embed Kain inside Rust hosts and native tools.
5. Materialize UI/native/3D applications, not just emit source text.

---

## Fast Capability Snapshot

| Area | Status | Notes |
|------|--------|-------|
| Core language frontend | Active | parse, typecheck, comptime, interpreter/test lanes |
| Multi-target compilation | Active | validated by `kain doctor` target list |
| Web backends | Active | `wasm`, `js`, `ts`, `ks`, `hybrid` |
| System backends | Active | `llvm`, `rust`, `cpp` |
| GPU backends | Active | `spirv`, `hlsl`, `usf`, plus artifact bundling |
| GPU runtime executor | Active | Vulkan compute executor for authored payloads |
| UE5 backend | Active and broad | runtime, editor, shaders, materials, graphs, blueprints, GAS/config adjacent crates |
| Native UI build lane | Active | `kain build native-ui` materializes desktop apps |
| 3D viewport/runtime lane | Active | `kain-3D` + `kain-ui-native` + WGPU viewport smoke |
| C source import | Active | `kain import-c` |
| Rust source import | Active | `kain import-rust`, selfhost workflows |
| TypeScript source import | Active | `kain import-ts` |
| Assembly import | Active | `kain import-asm` |
| Rust crate FFI | Active | `kain import-crate` and `use rust::<crate>` |
| C ABI host FFI | Active | `use c::...` host-backed lane |
| Python bridge | Active | embedded Python plus DCC-oriented wrappers |
| Node / JS bridge | Active | local JS/TS helper import, Node built-ins, web packaging |
| Shared interop contracts | Active | neutral buffer/image contract across runtimes |
| Mixed-runtime orchestration | Active | Python + Cargo + Node + C smokes exist now |

---

## Live CLI Surface

Current top-level commands from the debug binary:

```text
init
lsp
doctor
selfhost
omni
build
run
gpu-artifacts
inject
import-asm
import-c
import-rust
import-crate
import-ts
```

Current build subcommands:

```text
build
build native-ui
```

Important implication:

- the README must document `import-crate`, `omni`, `selfhost`, and `build native-ui` as first-class features
- `import-crate` is not a side experiment anymore
- native UI is a first-class build lane, not just an internal lab

---

## Supported Compile Targets

`kain doctor` currently reports these supported targets:

- `wasm`
- `llvm`
- `spirv`
- `hlsl`
- `usf`
- `js`
- `ts`
- `rust`
- `hybrid`
- `cpp`
- `ue5`
- `ue5editor`
- `run`
- `test`
- `ks`

### Target Groups

| Group | Targets | Primary Use |
|------|---------|-------------|
| Web | `wasm`, `js`, `ts`, `ks`, `hybrid` | browser, Node/web output, script generation |
| System | `llvm`, `rust`, `cpp` | native/system codegen, inspection, bootstrap |
| GPU | `spirv`, `hlsl`, `usf` | cross-platform shaders, DirectX, UE5 shaders |
| UE5 | `ue5`, `ue5editor` | runtime/editor plugin codegen |
| Host-backed execution | `run`, `test` | interpreter and runtime validation lanes |

### Important Constraint

Host-backed bridge features are not universal compile targets.

Examples:

- Rust crate FFI currently targets `run` and `test` lanes, not arbitrary offline codegen targets.
- Node/JS bridge behavior is runtime-hosted behavior, even if Kain can also emit JS/TS/KS.
- Python bridge is a host/runtime feature, not a Python source importer.
- C ABI host FFI and shared payload interop are runtime lanes, distinct from `import-c`.

---

## The Modern Kain Mental Model

Think of Kain as five connected layers:

1. **Frontend**
   - lexer, parser, comptime, typechecking, runtime/test execution
2. **Importers**
   - convert C, Rust, TypeScript, assembly into Kain program form
3. **Codegen backends**
   - web, system, GPU, UE5
4. **Host-backed bridges**
   - C, Rust crates, Python, Node, shared-neutral interop contracts
5. **Embeddable application/runtime stack**
   - driver, host, SDK, semantic UI, native desktop runtime, 3D viewport/runtime

That architecture matters because "Kain supports X" can mean very different things:

- imported as source
- called live at runtime
- compiled into a target artifact
- embedded inside a Rust host
- materialized into a native desktop/UI/3D application

---

## Command Reference

### `kain doctor`

Use this first.

It reports:

- compiler version/build
- build timestamp
- git SHA and dirty state
- profile
- host/target triple
- binary path
- current directory
- supported targets
- enabled features
- resolved LLVM/Clang location

```powershell
kain doctor
```

### `kain build`

Build a project from `KAIN.toml` or build a single file.

```powershell
kain build
kain build src/main.kn --target wasm
kain build src/main.kn --target rust
kain build src/main.kn --target cpp
kain build src/shader.kn --target spirv
kain build src/shader.kn --target hlsl
kain build src/shader.kn --target usf
kain build --ue5
```

### `kain build native-ui`

Materialize a standalone native UI app and optionally compile the desktop executable.

```powershell
kain build native-ui src/main.kn
kain build native-ui src/main.kn --app-name MyTool --window-title "My Tool"
kain build native-ui src/main.kn --bundle-only
kain build native-ui src/main.kn --release
```

This is the command surface for the semantic UI + native runtime lane.

### `kain run`

Run a `.kn` file explicitly.

```powershell
kain run smoketest/python/pygame_poster/smoke.kn
```

### `kain selfhost`

Run self-host bootstrap workflows.

Current command families:

- `phase1`
- `phase2`

`phase2` is the repair-oriented lane: it builds the self-host slice, applies bounded repairs to copied outputs, and validates the repaired workspace.

```powershell
kain selfhost phase1
kain selfhost phase2
```

Repair-specific notes live in [`guides/cli/doctor-and-repair.md`](guides/cli/doctor-and-repair.md).

### `kain omni`

Build mixed-language manifests through a dedicated orchestration layer.

Current command families:

- `init`
- `build`

```powershell
kain omni init
kain omni build
```

This is the data-driven mixed-language build lane for staged imports and multi-output builds.

### `kain gpu-artifacts`

Generate paired GPU artifacts:

- SPIR-V
- Rust host wrapper
- reflection JSON

```powershell
kain gpu-artifacts src/shader.kn --output dist
```

### `kain inject`

Inject `.kn` files into an existing UE plugin without destructive overwrite.

```powershell
kain inject src/new_actor.kn --ue5
```

### Source Import Commands

```powershell
kain import-c
kain import-rust
kain import-ts
kain import-asm
```

### Rust Crate FFI Command

```powershell
kain import-crate <crate_name>
```

Example:

```powershell
kain import-crate cargo_smoke_lab --crate-path .\local_crate --mode both -o .\outputs\generated
```

This command generates Kain binding artifacts and can also build/load a live bridge for runtime use.

---

## Source Import Pipelines

These commands transform foreign source into Kain.

### C Import

```powershell
kain import-c .\src\main.c --output .\main.kn
kain import-c .\src --output .\combined.kn
```

Use when you want source-level ingestion of C into Kain.

Do not confuse this with host-backed C FFI.

### Rust Import

```powershell
kain import-rust .\src\lib.rs --output .\lib.kn
kain import-rust .\src --flat --output .\combined.kn
```

This is part of the selfhost/Ouroboros direction.

### TypeScript Import

```powershell
kain import-ts .\src\app.ts --output .\app.kn
kain import-ts .\src --output .\combined.kn
```

### Assembly Import

```powershell
kain import-asm .\firmware.asm --format gameboy --out game.kn
kain import-asm .\firmware.asm --format 6502 --out furby.kn
kain import-asm .\firmware.asm --format z80 --out arcade.kn
```

Supported assembly families in the repo today:

- Game Boy LR35902
- 6502 / Furby-oriented lane
- Z80

---

## Runtime Bridge Pipelines

This is where the README previously lagged the most.

Kain now has several host-backed runtime lanes that let `.kn` orchestrate external systems directly.

### 1. C ABI Host FFI

Current proof surface:

- `smoketest/c_ffi/beacon_math`
- `smoketest/c_ffi/cgltf_scene_probe`
- `smoketest/c_ffi/miniaudio_tone_lab`
- `smoketest/c_ffi/shared_image_contract`

What this proves:

- local header-driven/shared-library workflows
- scalar and string calls
- live native loading
- image/shared-buffer mutation
- opaque handle round-trips
- practical media/scene/audio examples

This is a runtime bridge lane, not the same thing as `import-c`.

### 2. Rust Crate FFI

Current proof surface:

- `smoketest/cargo/local_crate_synth`
- `smoketest/cargo_node/signal_workbench`
- `smoketest/py_cargo/triple_stack_canvas`
- multiple higher-order mixed-runtime smokes

What this proves:

- `use rust::<crate_name>` authoring
- path-crate resolution through `KAIN.toml`
- generated `.kn` bindings and preludes
- binding reports
- bridge dylib build/load
- runtime reuse via cache

Important current constraint:

- Rust crate FFI is currently for host-backed `run` / `test` lanes

### 3. Python Bridge

Current proof surface:

- `smoketest/python/pygame_poster`
- `smoketest/python/trimesh_glb_forge`
- `smoketest/python/numpy_supernova`

What this proves:

- embedded Python execution
- wrapper surfaces such as:
  - `std::python::bridge`
  - `std::python::pygame`
  - `std::python::trimesh`
  - `std::python::numpy`
- Kain-native DCC payload handling via:
  - `std::dcc::image`
  - `std::dcc::tensor`
  - `std::dcc::mesh`

This is not a Python source importer.
It is a live runtime bridge and payload/materialization stack.

### 4. JavaScript / Node Bridge

Current proof surface:

- `smoketest/node/orbit_portal`
- `smoketest/node/typescript_signal_forge`

What this proves:

- local `.mjs` helper import
- local `.ts` helper import via TS-aware runtime
- Node built-in access
- typed buffer inspection
- HTML/SVG/web artifact packaging from `.kn`

Primary wrapper surface:

- `std::javascript::bridge`

### 5. Shared Neutral Interop Contracts

`crates/kain-interop` provides neutral shared buffer/image contracts that let runtimes exchange payloads without ad hoc glue per example.

This is a major architectural step.

The interop layer now models:

- shared buffers
- shared images
- ownership metadata
- layout/shape/stride metadata
- source runtime/backend provenance
- neutral payload transfer across Python, C, Node, and Rust-hosted lanes

This is the foundation behind the newer mixed-runtime smokes.

---

## Mixed-Runtime Smoke Matrix

The smoke suite now acts as a capability map for real-world orchestration.

### Single-runtime proof suites

| Folder | Focus |
|------|-------|
| `smoketest/c_ffi` | host-backed C ABI interop |
| `smoketest/cargo` | Rust crate FFI |
| `smoketest/python` | embedded Python + DCC wrappers |
| `smoketest/node` | JS/Node bridge |
| `smoketest/UI` | semantic UI compiler lane |

### Mixed-runtime proof suites

| Folder | Focus |
|------|-------|
| `smoketest/py_node` | Python + Node |
| `smoketest/cargo_node` | Cargo FFI + Node |
| `smoketest/py_cargo` | Python + Cargo |
| `smoketest/py_cargo_node` | Python + Cargo + Node |
| `smoketest/py_cargo_node_c` | Python + Cargo + Node + C |

### Notable examples

- `shared_image_contract`
  - Python generates image bytes
  - C mutates the same contract in place
  - Kain inspects/materializes the contract
- `trinity_web_lattice`
  - Python payload generation
  - Rust crate FFI structural markers
  - Kain composition
  - Node packaging
- `quad_prism_halo`
  - Python + Rust crate FFI + Node + C in one `.kn`

This is one of the strongest current differentiators of the codebase.

---

## Embeddable Rust Stack

The repo now has a real embedding story, not just a CLI binary.

### `crates/kain-driver`

Embeddable compiler driver.

Purpose:

- thin orchestration layer between frontend, backends, and host/runtime lanes
- compile Kain without routing through the CLI
- expose compile helpers for targets and artifact bundles
- prepare bridge-aware source before frontend compilation
- materialize runtime contract bundles and realtime app bundles

This crate owns the current target registry and much of the "glue code" of the system.

### `crates/kain-host`

Native Rust host runtime.

Purpose:

- load Kain source into a Rust host
- register native Rust functions for Kain to call
- call Kain functions from Rust
- exchange common values across the boundary
- emit engine/prelude/module shims for host-driven workflows

### `crates/kain-host-derive`

Derive macros for host boundary ergonomics.

Purpose:

- derive value conversion traits
- derive reflection metadata

### `crates/kain-reflect`

Reflection/type schema layer.

Purpose:

- describe structs/enums/transparent types
- render Kain-side type information
- power host/schema/prelude generation

### `crates/kain-sdk`

High-level embedder facade.

Purpose:

- re-export host/build/reflection pieces
- expose `KainEngine`
- provide a small surface for embedding Kain inside Rust applications

This is the cleanest entry point if you want to consume Kain as a library instead of living at the raw crate level.

---

## Semantic UI, Native Desktop, and 3D

This is another area where the old README undersold the repo.

### Semantic UI Lane

Relevant crates:

- `crates/kain-ui`
- `crates/kain-ui-native`

Relevant smokes:

- `smoketest/UI/theme_authoring_shell`
- `smoketest/UI/dock_layout_workbench`
- `smoketest/UI/surface_modes_gallery`
- `smoketest/UI/website_clone_signalcraft`

What this lane supports today:

- semantic widget/compiler pipeline
- theme bundles and variant maps
- dock layouts and split rails
- scrollable/structured native shells
- native desktop materialization through `build native-ui`

### Native Desktop Runtime

`crates/kain-ui-native` is not a stub.

It currently contains:

- desktop app runtime on top of `eframe` / `egui`
- runtime bundle loading
- native renderer preferences
- viewport support
- runtime tracing hooks
- desktop app materialization/build flow

### 3D Runtime

Relevant crate:

- `crates/kain-3D`

It currently exposes:

- authoring/scene primitives
- geometry and modifiers
- lights/cameras/materials/particles
- interaction and picking
- manipulators/gizmos
- renderer abstractions
- software renderer
- WGPU renderer path
- shader bundle helpers

### Native 3D Viewport Proof

Relevant lab:

- `labs/native_ui_viewport_smoke`

This lab validates the current `kain-3D` + `kain-ui-native` lane:

- native app shell
- viewport-first layout
- WGPU renderer label/fallback behavior
- stable roaming
- selection
- gizmo overlays
- interaction mode switching

This means Kain is now documenting not only a language but also a real native tool/runtime lane.

---

## GPU and Shader Pipeline

Relevant crates:

- `crates/gpu`
- `crates/kain-gpu-runtime`
- `crates/ue5-shaders`
- `crates/kain-driver`

Current active GPU outputs:

- `spirv`
- `hlsl`
- `usf`

Current artifact bundle story:

- SPIR-V binaries
- Rust shader host helpers
- reflection JSON
- derived HLSL where applicable

### Explicit Compute Plans

Compute shaders can now carry authored `comptime` metadata that describes:

- workgroup size
- dispatch size
- tensor bindings, roles, and contract names
- stream bindings
- neural node plans

That metadata is compiler-owned truth, not a host-local guess. It now flows through `kain-core` into runtime contract bundles, marks `gpu.compute-plan` as a required capability when present, and gives the raw-native viewport lane enough structure to validate and step a per-frame compute execution state.

The compiler currently accepts both:

- a legacy 3-entry plan: `(dispatch, tensors, nodes)`
- an extended 5-entry plan: `(workgroup, dispatch, tensors, streams, nodes)`

That keeps older authored shaders valid while allowing new compute lanes to move workgroup and stream semantics out of runtime heuristics and into compiler-owned bundle data.

The native runtime should still treat that as an execution bridge, not as full GPU dispatch. The useful distinction is:

- authored compute intent lives in the compiler
- runtime contracts carry it forward
- the raw-native viewport surfaces and advances it
- future real dispatch backends should consume the same plan instead of inventing a parallel dialect

Core command:

```powershell
kain gpu-artifacts src/shader.kn --output dist
```

This is more than simple shader text emission now.
The repo has a bundle/reflection/host-wrapper story around GPU outputs.

### GPU Runtime Executor

`crates/kain-gpu-runtime` is the runtime-facing Vulkan lane that executes authored compute payloads against the current interop and shader bundle model.

It currently owns:

- Vulkan compute executor setup and teardown
- buffer and binding preparation for shared payloads
- dispatch request/result FFI structs for host-facing calls
- compute residency and shader bundle loading
- error reporting close to the runtime contract boundary

This crate is intentionally narrower than the compiler or shader authoring pipeline:

- it consumes prepared GPU payloads
- it does not replace compiler ownership of the compute plan
- it is the execution bridge, not the source of truth for authored shader intent

---

## UE5 Stack

Relevant crates:

- `crates/ue5`
- `crates/ue5-editor`
- `crates/ue5-shaders`
- `crates/ue5-materials`
- `crates/ue5-graphs`
- `crates/ue5-blueprints`
- `crates/ue5-gas`
- `crates/ue5-config`
- `crates/ue5-asset-utils`
- `crates/unreal/*`

The current UE5 story includes:

- runtime plugin generation
- editor generation
- shader generation
- material graph generation
- graph editor/runtime support
- blueprint-oriented support
- asset utility support
- config/build helpers
- Unreal asset read/write sub-crates

The CLI packager code also shows active investment in:

- data-driven module dependency resolution
- split runtime/editor modules
- plugin layout detection
- injection into existing plugins
- asset registry writing
- build cs generation
- material factory generation
- post-processing and validation hooks

This is still one of the deepest codegen stacks in the repo.

---

## Omni Manifests

`crates/kain-omni` adds a data-driven mixed-language orchestration layer.

Key concepts:

- `KAIN.omni.toml`
- staged imports
- resolved entry generation
- per-target output declarations
- mixed import sets across:
  - Kain
  - Rust
  - TypeScript
  - C
  - assembly

The omni model is important because it gives Kain a proper declarative build surface for mixed-language projects instead of forcing everything through imperative one-off commands.

Example workflow:

```powershell
kain omni init
kain omni build
```

---

## Selfhost Direction

Selfhosting remains active and visible.

Relevant pieces:

- `kain import-rust`
- `kain selfhost`
- `crates/kain-selfhost`
- `crates/kain-driver`
- `crates/kain-core`
- `scripts/kain_linux_pipeline.sh`

The current repo direction still includes Project Ouroboros style flows, but it now sits alongside a much larger runtime/interop stack than earlier README versions suggested.

---

## Repository Structure

High-level current map:

```text
M:\Code\Kain
├── crates/
│   ├── cli/                  # CLI binary; thin over kain-driver
│   ├── kain-core/            # frontend, runtime, stdlib loading, typecheck, comptime
│   ├── kain-driver/          # embeddable compiler driver/orchestration layer
│   ├── kain-host/            # Rust host runtime for calling/embedding Kain
│   ├── kain-host-derive/     # host derive macros
│   ├── kain-reflect/         # schema/reflection/type metadata
│   ├── kain-interop/         # neutral shared buffer/image contracts
│   ├── kain-c-ffi/           # C ABI runtime bridge lane
│   ├── kain-gpu-runtime/     # runtime-facing Vulkan compute executor
│   ├── kain-crate-ffi/       # Rust crate FFI extraction/generation/live bridge loading
│   ├── kain-python/          # embedded Python bridge + DCC payload wrappers
│   ├── kain-node/            # Node/JS runtime bridge
│   ├── kain-ui/              # semantic UI compiler/runtime model
│   ├── kain-ui-native/       # native desktop UI runtime
│   ├── kain-3D/              # 3D authoring/scene/renderer/interaction/runtime
│   ├── kain-sdk/             # high-level embedding facade
│   ├── kain-omni/            # mixed-language manifest orchestration
│   ├── kain-build/           # engine/module build helpers
│   ├── kain-import/          # C/Rust/TS importers
│   ├── kain-asm/             # assembly importers
│   ├── web/                  # web targets
│   ├── gpu/                  # GPU targets
│   ├── kain-sys-codegen/     # LLVM/Rust/C++ system backends
│   ├── ue5*/                 # UE5 family crates
│   └── unreal/*              # vendored Unreal asset/tooling crates
├── smoketest/                # proof matrix for bridges, mixed runtimes, UI, 3D
├── labs/                     # focused validation labs, including native viewport
├── runtime/                  # native runtime contracts, C runtime, raw-native lane, and parallel companion lane
│   ├── native/               # raw-native execution lane and viewport host
│   └── parallel/             # Rust/Zig companion lane for runtime completion work
├── kn_library/               # curated Kain corpus, data tables, and library-oriented samples
├── stdlib/                   # stdlib data and target/runtime support
├── toolchain/                # LLVM and related toolchain support
├── generated/                # generated artifacts and larger smoke outputs
└── README.md                 # this file
```

For navigation detail, check:

- `M:\Code\Kain\docs\README.md`
- `M:\Code\Kain\docs\pipeline\README.md`
- `M:\Code\Kain\crates\README.md`
- `M:\Code\Kain\crates\repomap.md`
- `M:\Code\Kain\repomap.md`

---

## Recommended Workflows

### Inspect the toolchain

```powershell
kain doctor
.\target\debug\kain.exe --help
.\target\debug\kain.exe build --help
.\target\debug\kain.exe import-crate --help
.\target\debug\kain.exe build native-ui --help
```

### Build a normal target

```powershell
kain build src/main.kn --target ts
kain build src/main.kn --target rust
kain build src/main.kn --target cpp
```

### Build a native UI app

```powershell
kain build native-ui smoketest/UI/theme_authoring_shell/smoke.kn --bundle-only
```

### Run a bridge-heavy smoke

```powershell
kain run smoketest/python/pygame_poster/smoke.kn
kain run smoketest/node/orbit_portal/smoke.kn
```

### Generate Rust crate FFI bindings

```powershell
kain import-crate cargo_smoke_lab --crate-path .\smoketest\cargo\local_crate_synth\local_crate -o .\out
```

### Drive an omni manifest

```powershell
kain omni init
kain omni build
```

### Build UE5 output from project config

```powershell
kain build --ue5
```

### Generate GPU artifacts

```powershell
kain gpu-artifacts src/shader.kn --output dist
```

---

## Practical Guardrails

- Do not claim that Python is currently a source importer. In this repo it is a runtime bridge.
- Do not collapse `import-c` and C host FFI into the same feature. They are different.
- Do not describe Rust crate FFI as merely planned. It is active and has multiple smoke suites.
- Do not describe native UI/3D as hypothetical. There is a real build command and a real viewport smoke lab.
- Do not describe `kain-driver`, `kain-host`, `kain-reflect`, `kain-sdk`, or `kain-omni` as incidental crates. They define the current architecture.
- Do not flatten authored compute metadata back into heuristic-only dispatch planning when the compiler already emitted a concrete plan.
- When documenting portability, call out host-backed-only behavior explicitly.

---

## What Most Changed Since Older README Revisions

If you last looked at Kain when it was mostly "compiler + importers + UE5/web/system backends", the biggest updates are:

1. `kain-driver` is now the central embeddable orchestration layer.
2. `kain-host`, `kain-host-derive`, `kain-reflect`, and `kain-sdk` create a real embedding story.
3. Rust crate FFI is active, visible, and documented through `import-crate` and `use rust::<crate>`.
4. Python and Node bridges now have broad proof surfaces and mixed-runtime examples.
5. `kain-interop` gives the repo a neutral shared payload contract across runtimes.
6. `kain-3D` and `kain-ui-native` make Kain a native-tool/runtime stack, not just a text compiler.
7. `kain build native-ui` and `kain omni` are now part of the real command surface.
8. The smoke folder is now a major source of truth for current capability, especially FFI and hybrid lanes.
9. Explicit compute metadata now threads from shader `comptime` into runtime contracts and raw-native execution state.

---

## Bottom Line

Kain in this repo is currently:

- a language frontend
- a multi-target compiler
- a source-import system
- a host-backed interop runtime
- an embeddable Rust engine/sdk
- a semantic UI compiler/runtime
- a native 3D desktop tool stack
- a UE5 codegen platform
- a mixed-language orchestration system

That is the current state this README should reflect, and this version is intended to do exactly that.
