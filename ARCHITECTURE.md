# Kain Architecture

This file is the durable repo overview for `M:\Code\Kain`.
It is the fast way for future agents to understand what Kain is, where the important code lives, and which architectural rules matter enough to preserve.

## What Kain Is

Kain is a compiled multi-target language toolchain plus an embeddable runtime and host stack.

The repo currently spans five connected layers:

1. `crates/kain-core`
Language semantics, parsing, typing, effects, comptime, interpreter lanes, runtime-contract emission, shader metadata, and other compiler-owned truth.

2. Importers and orchestration
`crates/kain-driver`, `crates/kain-omni`, `crates/kain-selfhost`, `crates/kain-build`, and importer crates turn source and manifests into emitted bundles, artifacts, and multi-runtime workflows.

3. Runtime and host bridges
`runtime/native` is the canonical ABI floor and C runtime substrate. `crates/kain-host`, `crates/kain-sdk`, `crates/kain-reflect`, `crates/kain-c-ffi`, `crates/kain-crate-ffi`, `crates/kain-python`, `crates/kain-node`, and `crates/kain-interop` provide host/runtime integration.

4. UI, native desktop, and 3D
`crates/kain-ui`, `crates/kain-ui-native`, and `crates/kain-3D` are the semantic UI and accelerated native presentation stack.

5. Target adapters
`crates/web`, `crates/gpu`, `crates/kain-gpu-runtime`, and the `crates/ue5*` family consume compiler-owned contracts for specific runtime environments.

## Non-Negotiable Ownership

- `crates/kain-core` owns language meaning, typed metadata, capability requirements, and shader/compute-plan semantics.
- `crates/kain-driver` owns emitted bundle truth and app/runtime materialization.
- `runtime/native` owns the stable ABI floor, startup, service contracts, and low-level host/runtime substrate.
- Accelerated Rust lanes may optimize execution, but they must consume the same compiler-owned bundles rather than inventing a second semantic model.
- Web, UE5, selfhost, and future lanes are adapters, not alternate definitions of what Kain source means.

## Main Folders

- [README.md](/M:/Code/Kain/README.md): repo-level operating brief
- [repomap.md](/M:/Code/Kain/repomap.md): top-level folder map
- [MEMORY.md](/M:/Code/Kain/MEMORY.md): durable architectural task memory
- [crates](/M:/Code/Kain/crates): workspace crates
- [runtime](/M:/Code/Kain/runtime): native runtime substrate, conformance, fixtures, and companion lanes
- [smoketest](/M:/Code/Kain/smoketest): capability proof matrix for bridges, UI, 3D, and mixed runtimes
- [docs](/M:/Code/Kain/docs): doctrine, plans, pipeline notes, validation notes, and research
- [apps](/M:/Code/Kain/apps): first-class applications and prototypes
- [stdlib](/M:/Code/Kain/stdlib): runtime support and standard library data
- [testing](/M:/Code/Kain/testing): test infrastructure and fixtures

## Key Crates

- [kain-core](/M:/Code/Kain/crates/kain-core): parser, AST, typechecker, comptime, runtime contract emission, realtime bundle metadata
- [kain-driver](/M:/Code/Kain/crates/kain-driver): target orchestration, shader bundles, native app materialization, compute residency sidecars
- [cli](/M:/Code/Kain/crates/cli): `kain` command surface
- [kain-host](/M:/Code/Kain/crates/kain-host): Rust embedding and native function registration
- [kain-reflect](/M:/Code/Kain/crates/kain-reflect): reflection schemas and type identity
- [kain-sdk](/M:/Code/Kain/crates/kain-sdk): high-level embedding facade
- [kain-interop](/M:/Code/Kain/crates/kain-interop): shared buffer/image payload contracts
- [kain-gpu-runtime](/M:/Code/Kain/crates/kain-gpu-runtime): Vulkan compute executor consuming emitted shader bundles and residency metadata
- [kain-ui](/M:/Code/Kain/crates/kain-ui): semantic UI graph and patch-oriented UI meaning
- [kain-ui-native](/M:/Code/Kain/crates/kain-ui-native): native desktop host/runtime lane
- [kain-3D](/M:/Code/Kain/crates/kain-3D): native 3D renderer and viewport runtime

## Primary Data Flows

### Compile and runtime bundle flow

`Kain source -> kain-core semantic analysis -> runtime contract / realtime app bundle / shader bundle metadata -> kain-driver materialization -> runtime/native and accelerated lanes consume the same bundle family`

### Host bridge flow

`.kn source -> compiler/runtime contracts -> host bridge crates (Python, Node, C ABI, Rust crate FFI) -> shared payload contracts via kain-interop`

Current native-ui packaging rule for C ABI imports:

- `kain-c-ffi` is no longer only an `Interpret`/`Test` lane concern. The Rust/native-ui packaging lane now emits packaged bridge manifests, copies bridge/shared-library sidecars into the app artifact set, and has the generated native app launcher load those packaged bridges before boot.
- This does not mean the current native UI host is a full general-purpose Kain interpreter. The lane is still bundle-driven; the packaging change makes foreign bridge dependencies explicit and shippable rather than hidden behind cache-local host-backed behavior.

### Compute pipeline flow

The current compute direction is:

- authored compute truth starts in shader `comptime` metadata in `kain-core`
- `kain-core` emits workgroup, dispatch, tensor, stream, and neural metadata into realtime/shader-facing bundle structures
- `kain-driver` materializes compute residency sidecars and native app packaging data
- `runtime/native` validates and surfaces `primary_compute`
- `kain-gpu-runtime` is the real Vulkan dispatch bridge that consumes emitted shader/residency artifacts

The architecture rule here is important: runtimes may execute the compute plan, but they must not become the source of truth for what that plan is.

### Shader Canvas Lane

The native shader-canvas UI lane now follows this contract:

- `kain-ui` owns authored semantic surfaces and shader-canvas intent on canvas-like nodes
- `kain-core` emits explicit `shader_canvases` entries in `RealtimeAppBundle` so hosts do not have to rediscover shader-canvas bindings by guessing from local UI props
- `kain-core` also emits first-class shader-canvas text resources per surface: font atlas descriptors, text runs, and declared runtime resource bindings
- `kain-driver` materializes shader bundles and native app sidecars that keep shader-canvas metadata, shader refs, and native UI bundles aligned
- `kain-ui-native` resolves shader canvases from realtime bundle metadata first and only falls back to surface-local shader refs when metadata is missing
- `kain-ui-native` now turns the shader-canvas text contract into real GPU inputs by serializing atlas/text metadata into the surface storage buffer and synthesizing a host-provisioned packed atlas texture, preferring `ab_glyph` rasterization from data-driven system-font aliases with bitmap fallback and cache reuse across repeated surfaces that share atlas content
- canonical native shader payload remains SPIR-V, while the current WGPU native host may consume derived WGSL or runtime-transpiled WGSL from the same bundle family

The architecture rule here is the same as the viewport and compute lanes: shader-canvas execution can optimize presentation, but it must stay subordinate to compiler-owned bundle truth rather than inventing a renderer-local UI shader dialect.

### Viewport Contract Lane

Viewport startup intent now follows the same compiler-owned pattern:

- `kain-core` emits `render.scenes` bindings with authored scene ids plus optional camera and presentation metadata
- `kain-ui-native` and `runtime/native` consume those bundle defaults first and only fall back to local scene/profile defaults when the bundle leaves a field unspecified
- scene ids, shader refs, camera presets, and presentation presets should travel together through the realtime bundle instead of being re-guessed independently by each host

## Important Folders By Intent

- [runtime/native](/M:/Code/Kain/runtime/native): canonical C runtime and ABI/service floor
- [runtime/conformance](/M:/Code/Kain/runtime/conformance): lane-level conformance harnesses
- [runtime/parallel](/M:/Code/Kain/runtime/parallel): Rust/Zig companion runtime work that must stay aligned with the native runtime doctrine
- [docs/kainplan](/M:/Code/Kain/docs/kainplan): active design and execution docs
- [docs/pipeline](/M:/Code/Kain/docs/pipeline): pipeline notes and operational docs
- [labs](/M:/Code/Kain/labs): focused validation labs
- [generated](/M:/Code/Kain/generated): disposable generated outputs

## Common Commands

Prefer the live CLI and source over stale docs when they disagree.

Typical commands:

- `kain doctor`
- `kain build`
- `kain build native-ui <file.kn>`
- `kain run <file.kn>`
- `kain gpu-artifacts <file.kn> --output <dir>`
- `kain selfhost phase1`
- `kain omni build`
- `kain fabric init --template polyglot`
- `kain fabric validate`
- `kain fabric run`
- `kain import-c`, `kain import-rust`, `kain import-ts`, `kain import-asm`, `kain import-crate`

If the debug CLI is missing:

- `cargo build -p cli`
- `target/debug/kain.exe --help`

## Architectural Guardrails

- Do not split semantic truth across runtime lanes.
- Do not let hosts re-parse source as the normal execution path.
- Do not invent lane-specific shader, UI, or compute metadata when compiler-owned bundles already exist.
- Prefer data-driven capabilities, manifests, registries, and bundle metadata over scattered string checks and host-local assumptions.
- Preserve the distinction between authored language semantics, importer behavior, and backend/runtime support.

## Common Errors

- The root `README.md` is useful, but live source and the built CLI are the real source of truth.
- The `cli` suite no longer depends on the external self-hosting fixture under `M:\Code\Other\kainselfhosting\...`; the repo-local import-c fixture under `crates/cli/tests/fixtures/import_c` is the durable regression source now.
- Large Windows test binaries can hit linker OOM pressure.
- `generated/`, `target/`, `.kain`, runtime sidecars, and compiled smoke outputs are disposable unless explicitly archived under `docs/validation/` or `docs/recent/`.
- The native runtime is Windows-first today. Linux and macOS surfaces exist, but much of that lane is still stubbed or partial.
- The compute pipeline is mid-transition from heuristic metadata to compiler-owned truth. When touching it, prefer extending bundle contracts over adding new runtime-only inference.
- The native shader-canvas lane is SPIR-V-canonical at the bundle level, but the current WGPU host still resolves WGSL for execution. Do not mistake that compatibility bridge for permission to move shader-canvas truth out of the emitted bundles.
- Fabric Python execution should stay behind `kain-python` helpers. Do not make `kain-host` reach directly into `pyo3` imports or `PythonScopeState` internals when the Python lane can expose a narrower execution API.
- Fabric runtime ownership is now split cleanly: `kain-omni` owns `KAIN.fabric.toml` schema/validation/report types, while `kain-host` owns local execution, dependency plumbing, and runtime adapter behavior.
- Fabric step inputs now have two host-facing forms: raw `fabric_inputs` for Kain/C/Rust glue that needs canonical shared contract handles, and normalized `fabric_serialized_inputs` for Python/Node glue that cannot accept foreign host objects directly.
- Fabric Python and Node steps now support mixed named outputs when they return a dict/object whose fields match the manifest's declared output names. Value outputs are normalized, while shared outputs still flow through canonical host-owned interop handles.
- Missing declared Python/Node output fields now fail with structured Fabric errors keyed as `missing_output_field`, with `output_name` recorded in failure details. Preserve that contract surface when touching adapter execution or bridge helpers.
- The durable end-to-end Fabric proof lives under `smoketest/fabric/polyglot_local`. It is the quickest repo-local example of Python -> Kain -> C ABI -> Rust crate -> Node execution with typed shared image/shared buffer flow.
- `kain fabric init --template polyglot` now emits a runnable local smoke-grade scaffold, including its local Rust crate manifest and native C fixture, instead of a validation-only placeholder.
- The generated polyglot scaffold also writes `FABRIC.README.md`; treat that file as the first-stop quickstart for the smoke-grade local pipeline shape.

## Template Packs

`templates/Web` now hosts a manifest-driven universal web starter aimed at users who should not need Rust or Cargo installed.

Key rules for this lane:

- Kain owns orchestration and semantic UI preview entrypoints.
- Node FFI owns browser packaging, local serving, and actor-server runtime glue.
- themes, content, scenes, and experiences are registry-driven data, not scattered starter literals.
- web template boilerplate should prefer reusable stdlib wrappers (`std::javascript::site_runtime`, `std::javascript::site_actor`) plus shared helper runtimes over copy-pasted starter code.

