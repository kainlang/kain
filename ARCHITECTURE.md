# Kain Architecture

This file is the durable repo overview for `M:\Code\Kain`.
It is the fast way for future agents to understand what Kain is, where the important code lives, and which architectural rules matter enough to preserve.

## What Kain Is

Kain is a compiled multi-target language toolchain, an executable semantic runtime, and an embeddable host stack.

It is not only a `KAIN.toml`/materialization language or an orchestration shell over Rust, C, Python, Node, and GPU targets. `crates/kain-core` already owns real language execution for substantial parts of Kain itself: parsing, `comptime`, executable-body typechecking, direct interpretation of functions and blocks, closures, control flow, `match`, async/await, actor semantics, JSX/UI expression evaluation, and runtime execution of compiler-owned declarations such as `patch`, `converge`, `world`, and `orchestrate`.

The build, packaging, and adapter crates matter because Kain is meant to ship into multiple targets, not because the language is limited to manifests and glue. The durable architecture rule is: keep authored logic in Kain when it belongs to Kain semantics, and use host bridges when the capability is genuinely platform-, ABI-, or ecosystem-owned.

The repo currently spans five connected layers:

1. `crates/kain-core`
Language semantics, parsing, typing, effects, comptime, interpreter lanes, runtime-contract emission, shader metadata, and other compiler-owned truth.

2. Materialization, import, and build orchestration
`crates/kain-driver`, `crates/kain-omni`, `crates/kain-selfhost`, `crates/kain-build`, and importer crates turn Kain semantic truth into emitted bundles, packaged artifacts, imported surfaces, and multi-runtime workflows.

3. Runtime and host bridges
`runtime/native` is the canonical ABI floor and C runtime substrate. `crates/kain-host`, `crates/kain-sdk`, `crates/kain-reflect`, `crates/kain-c-ffi`, `crates/kain-crate-ffi`, `crates/kain-python`, `crates/kain-node`, and `crates/kain-interop` provide host/runtime integration.

4. UI, native desktop, and 3D
`crates/kain-ui`, `crates/kain-ui-native`, and `crates/kain-3D` are the semantic UI and accelerated native presentation stack.

5. Target adapters
`crates/web`, `crates/gpu`, `crates/kain-gpu-runtime`, and the `crates/ue5*` family consume compiler-owned contracts for specific runtime environments.

## Non-Negotiable Ownership

- `crates/kain-core` owns language meaning, typed metadata, executable semantics, capability requirements, and shader/compute-plan semantics.
- `crates/kain-driver` owns emitted bundle truth and app/runtime materialization.
- `runtime/native` owns the stable ABI floor, startup, service contracts, and low-level host/runtime substrate.
- Host bridges and adapters extend Kain into external ecosystems; they do not downgrade Kain source into "configuration only."
- Accelerated Rust lanes may optimize execution, but they must consume the same compiler-owned bundles rather than inventing a second semantic model.
- Web, UE5, selfhost, and future lanes are adapters, not alternate definitions of what Kain source means.

## Main Folders

- [README.md](/M:/Code/Kain/README.md): repo-level operating brief
- [repomap.md](/M:/Code/Kain/repomap.md): top-level folder map
- [MEMORY.md](/M:/Code/Kain/MEMORY.md): durable architectural task memory
- [docs/kainplan/ui_slate_x100](/M:/Code/Kain/docs/kainplan/ui_slate_x100): active UI overhaul docs, acceptance criteria, regression notes, and Gamma operator guidance
- [docs/kainplan/08_COMPILER_OWNED_INTENT_QUARTET.md](/M:/Code/Kain/docs/kainplan/08_COMPILER_OWNED_INTENT_QUARTET.md): syntax, lowering, bundle contracts, and validation notes for the compiler-owned intent suite: `law`, `patch`, `converge`, `world`, and `orchestrate`
- [crates](/M:/Code/Kain/crates): workspace crates
- [runtime](/M:/Code/Kain/runtime): native runtime substrate, conformance, fixtures, and companion lanes
- [smoketest](/M:/Code/Kain/smoketest): capability proof matrix for bridges, UI, 3D, and mixed runtimes
- [smoketest/compiler_owned_intent](/M:/Code/Kain/smoketest/compiler_owned_intent): compiler-owned intent suite smoke covering `kain run` plus LLVM runtime-contract / realtime-bundle staging
- [smoketest/UI](/M:/Code/Kain/smoketest/UI): UI proof surface for authored shells, dense operator layouts, shader-canvas proofs, and packaged native launches
- [smoketest/allinone](/M:/Code/Kain/smoketest/allinone): broad regression harness that replays importers, standalone FFI bridges, GPU artifacts, Omni, Fabric, and UE5 codegen into per-lane output folders
- [docs](/M:/Code/Kain/docs): doctrine, plans, pipeline notes, validation notes, and research
- [apps](/M:/Code/Kain/apps): first-class applications and prototypes
- [apps/kain-fabric-modeler](/M:/Code/Kain/apps/kain-fabric-modeler): Fabric-first native 3D modeling app scaffold that converges Python, Kain, C ABI, Rust crate, GPU compute, Node, and native-ui packaging
- [apps/kain-fabric-dcc-suite](/M:/Code/Kain/apps/kain-fabric-dcc-suite): broader flagship Fabric-first DCC suite scaffold with scene, ingest, sculpt, material, rig, animation, sim, render, compositor, publish, automation, and tensor planning lanes
- [apps/kain-canvas-forge](/M:/Code/Kain/apps/kain-canvas-forge): Node-first desktop-ready painting and Three.js composition studio prototype that proves a browser and `.exe` app lane can live under `apps/`
- [stdlib](/M:/Code/Kain/stdlib): runtime support and standard library data
- [testing](/M:/Code/Kain/testing): test infrastructure and fixtures

## Key Crates

- [kain-core](/M:/Code/Kain/crates/kain-core): parser, AST, executable-body semantic typechecker, `comptime`, interpreter/runtime execution for real Kain logic, runtime contract emission, realtime bundle metadata, and the compiler-owned intent suite (`law`, `patch`, `converge`, `world`, `orchestrate`)
- [kain-driver](/M:/Code/Kain/crates/kain-driver): target orchestration, shader bundles, native app materialization, packaged launcher snapshots, compute residency sidecars
- [cli](/M:/Code/Kain/crates/cli): `kain` command surface
- [kain-repair](/M:/Code/Kain/crates/kain-repair): profile-driven deterministic source repair engine consumed by the doctor/CLI repair lane; now split into a declarative rule registry plus a per-rule execution engine so repair policy stays visible and mode-aware; includes header normalization for parser-hostile `enum_` / `struct_` / `trait_` / `impl_` declaration forms
- [kain-host](/M:/Code/Kain/crates/kain-host): Rust embedding and native function registration
- [kain-reflect](/M:/Code/Kain/crates/kain-reflect): reflection schemas and type identity
- [kain-sdk](/M:/Code/Kain/crates/kain-sdk): high-level embedding facade
- [kain-interop](/M:/Code/Kain/crates/kain-interop): shared buffer/image payload contracts
- [kain-gpu-runtime](/M:/Code/Kain/crates/kain-gpu-runtime): Vulkan compute executor consuming emitted shader bundles and residency metadata
- [kain-ui](/M:/Code/Kain/crates/kain-ui): semantic UI graph and patch-oriented UI meaning
- [kain-ui-native](/M:/Code/Kain/crates/kain-ui-native): native desktop host/runtime lane
- [kain-3D](/M:/Code/Kain/crates/kain-3D): native 3D renderer and viewport runtime

## Primary Data Flows

### Semantic execution flow

`Kain source -> lexer/parser -> comptime -> executable-body typecheck -> kain-core runtime/interpreter executes authored Kain logic`

This repo does not only compile source outward into foreign runtimes. `kain-core` is already an execution engine for meaningful language behavior:

- direct evaluation of functions, blocks, loops, assignment, field/index mutation, closures, and pattern matching
- async/await and future/poll semantics in the in-language runtime lane
- actor state initialization, message handling, and runtime-side actor behavior
- JSX/UI expression evaluation and signal-driven UI contract execution
- runtime execution of `law`, `patch`, `converge`, and `orchestrate`, including law calls, converge verification, and patch transaction recording

When `kain run` succeeds, Kain is not merely validating authored source before handing work to another backend. In many cases it is executing the language's own semantic model directly. Treat that lane as a first-class truth source for what Kain code means.

### Compile and runtime bundle flow

`Kain source -> kain-core semantic analysis -> runtime contract / realtime app bundle / shader bundle metadata -> kain-driver materialization -> runtime/native and accelerated lanes consume the same bundle family`

The semantic-analysis part of that pipeline now includes real executable-body checks in `kain-core`, not only declaration registration. The compiler validates return values, call arguments, `match` arm type agreement, duplicate boolean arms, and `await` / `async` future typing before downstream codegen and bundle emission consume the typed program. That typechecked program also feeds the runtime/interpreter lane; bundle/codegen flows are downstream consumers of the same semantic truth, not a replacement for it.

That same frontend lane now owns five compiler-owned intent declarations:

- `law` lowers to callable invariant metadata through explicit `laws[]` contract sections.
- `patch` lowers to transactional mutation metadata with inferred undo mode plus explicit `patches[]` contract sections.
- `converge` lowers to dispatcher-plus-lane metadata with deterministic selection and executable `verify random(n)` verification through `converges[]`.
- `world` lowers to shared state/surface projection metadata through sparse `worlds[]` entries and compiler-owned active-world selection.
- `orchestrate` lowers to strict typed stage metadata through `orchestrations[]`.

The runtime-contract and realtime-bundle families now both carry these explicit sections, and downstream adapters should consume them directly instead of reverse-engineering equivalent intent from local conventions.

### Host bridge flow

`.kn source -> compiler/runtime contracts -> host bridge crates (Python, Node, C ABI, Rust crate FFI) -> shared payload contracts via kain-interop`

Bridges exist to expose external capabilities cleanly. They should not become the default place to hide logic that Kain can already express and execute itself. Prefer Kain-owned logic for domain behavior, control flow, state transitions, and semantic contracts; use bridge crates for foreign APIs, packaged dependencies, and target-native runtime services.

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
- `kain-core` also emits first-class shader-canvas text resources per surface: font atlas descriptors, text runs, declared runtime resource bindings, and optional asset-backed font references through the shared realtime asset catalog
- `kain-driver` materializes shader bundles and native app sidecars that keep shader-canvas metadata, shader refs, native UI bundles, and packaged realtime font assets aligned, resolving relative realtime asset sources against the authored source root instead of the materialization working directory
- `kain-ui-native` resolves shader canvases from realtime bundle metadata first and only falls back to surface-local shader refs when metadata is missing
- `kain-ui-native` now turns the shader-canvas text contract into real GPU inputs by serializing atlas/text metadata into the surface storage buffer and synthesizing a host-provisioned packed atlas texture, preferring packaged realtime font assets first, then `ab_glyph` rasterization from data-driven system-font aliases, with bitmap fallback and cache reuse across repeated surfaces that share atlas content
- `smoketest/UI/spv_ui_surface_probe` is the canonical native proof for this lane: it authors a real `<canvas>` node, packages a relative font asset, emits SPIR-V, and shows a fragment shader sampling the runtime-provided packed atlas texture
- canonical native shader payload remains SPIR-V, while the current WGPU native host may consume derived WGSL or runtime-transpiled WGSL from the same bundle family

The architecture rule here is the same as the viewport and compute lanes: shader-canvas execution can optimize presentation, but it must stay subordinate to compiler-owned bundle truth rather than inventing a renderer-local UI shader dialect.

### Semantic Tab Workspace Lane

Semantic authored tabs now follow the same ownership rule:

- `kain-ui` and `kain-core` own tab intent through authored node metadata such as `tab_group_id`, `tab_label`, `tab_order`, `tab_default_active`, and `persistent_layout_id`
- `kain-ui-native` may render that intent as native clickable tab chrome, but it should resolve and persist the active selection through `output.systems.workspace_layout.active_tabs`
- host-side tab rendering is allowed to optimize presentation, but it must not invent a second tab schema or bypass the emitted UI/runtime bundle truth

`smoketest/UI/kinetic_ui_atlas` is now the durable repo-local proof for this lane: a fresh four-page native executable that uses semantic top tabs, docked shells, shader canvases, and a real viewport workspace without reusing the older smoketest compositions.

### Reload-Safe UI Contract Lane

UI reload and derived-value semantics now follow an explicit compiler-to-runtime contract:

- `kain-core` emits stable signal ids, computed lowering (`writes_signal`, runtime `expr`, invalidation targets, scheduler phase), and event-route metadata including route ids, command routes, and transaction labels
- `kain-ui` owns runtime execution of those contracts, including derived recompute, exact invalidation, hot-reload state transfer, reload patch reporting, and bounded reconciliation
- `realtime_app_bundle.ui_contracts` now exposes computed, event-route, reload, focus, selection, overlay, motion, and workspace payloads so downstream native hosts can inspect semantic truth without rediscovering it from local widget state

The architectural rule is the same as every other lane: reload behavior may be optimized by hosts, but the host must not become the source of truth for identity transfer, derived state, focus/selection state, or transaction semantics.

`UiRuntimeBundle.native_projection` is now a compatibility-only sidecar rather than a normal bundle contract surface. Canonical serialization keeps `output.tree` and `output.systems` authoritative and omits the projection when it is empty; legacy raw-native consumers should opt into the explicit projection helper when they still need the flat view.

### Native Packaging And Operator Loop

The native packaging lane is the operator-facing loop for UI iteration:

- `kain-driver` materializes native apps as a package set, writing the runtime bundle, runtime contract, realtime bundle, `app_manifest.json`, `runtime_snapshot.json`, and any required sidecars into the app artifact tree.
- Generated launchers resolve those packaged sidecars beside the executable so the app boots from authored bundle truth instead of a debug-host template.
- The runtime snapshot is the reload/control surface, not hidden launcher state. It carries explicit provider, session, workspace, command, and capability records, including the `runtime.reload` command already emitted by the packaging path.
- Devtools and inspectors must stay opt-in and remain represented in packaged truth, not injected as default product chrome.
- When a packaged launch stops reflecting a change, check the materialized manifest and snapshot sidecars first. That is the stable operator boundary before assuming the host itself is wrong.
- target-aware world selection now resolves native desktop targets against `native_ui`, web targets against `web`, and UE5 targets against `ue5`; ambiguous multi-world cases must use an explicit `--root` selection.

### Viewport Contract Lane

Viewport startup intent now follows the same compiler-owned pattern:

- `kain-core` emits `render.scenes` bindings with authored scene ids plus optional camera and presentation metadata
- `kain-ui-native` and `runtime/native` consume those bundle defaults first and only fall back to local scene/profile defaults when the bundle leaves a field unspecified
- scene ids, shader refs, camera presets, and presentation presets should travel together through the realtime bundle instead of being re-guessed independently by each host
- `kain-3D` now owns the reusable manipulator drag contract as well: screen drag, axis/plane constraints, snap application, and local-vs-world transform math live in `crates/kain-3D/src/interaction.rs`, while `kain-ui-native` should stay a host/input forwarder instead of carrying a second copy of viewport-edit math
- `kain-3D` now also owns the authored primitive mesh pipeline in `crates/kain-3D/src/primitive.rs`: stable primitive ids and `mesh://primitives/authored/*` resource URIs, high-fidelity box / plane / uv-sphere / quad-sphere / cylinder / cone / capsule / torus builders, and a `PrimitiveLibrary` that can register those shapes into authoring scenes without inventing a second primitive catalog in the host
- authored `.kn` code reaches that same primitive seam through the `zen3d` prelude and runtime-native `__zen3d_*` bindings, so primitive authoring stays consistent across Rust scene setup, Kain host sessions, and viewport/runtime consumption

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

- `python3 install_kain.py`
- `py install_kain.py`
- `kain doctor`
- `kain doctor --repair <file>`
- `kain doctor --repair-tree <dir>`
- `kain build`
- `kain build native-ui <file.kn>`
- `kain run <file.kn>`
- `kain gpu-artifacts <file.kn> --output <dir>`
- `kain selfhost phase1`
- `kain selfhost phase2` for the bounded self-host repair lane
- `kain omni build`
- `kain fabric init --template polyglot`
- `kain fabric validate`
- `kain fabric run`
- `kain import-c`, `kain import-rust`, `kain import-ts`, `kain import-asm`, `kain import-crate`
- `./runtime/fixtures/validate_all.sh`
- `./runtime/conformance/run_all.sh`
- `./runtime/validate_native_runtime.sh`
- `powershell -ExecutionPolicy Bypass -File smoketest/allinone/run_all.ps1`

If the debug CLI is missing:

- `cargo build -p cli`
- `target/debug/kain --help`

## Architectural Guardrails

- Do not split semantic truth across runtime lanes.
- Do not describe Kain as "only orchestration" when `kain-core` already executes real authored logic.
- Do not let hosts re-parse source as the normal execution path.
- Do not push Kain-expressible business/domain logic into host bridges by default. Bridge when a capability is genuinely external, not because the language/runtime lane was ignored.
- Do not invent lane-specific shader, UI, or compute metadata when compiler-owned bundles already exist.
- Prefer data-driven capabilities, manifests, registries, and bundle metadata over scattered string checks and host-local assumptions.
- Keep the interpreter/runtime lane and emitted bundle/codegen lanes semantically aligned. A packaged target may optimize or lower behavior, but it should not silently define different language meaning than `kain-core`.
- Preserve the distinction between authored language semantics, importer behavior, and backend/runtime support.
- Platform- or console-specific render-command experiments should start as isolated adapter lanes under `smoketest/` or another dedicated adapter crate before any shared `kain-3D` contract is widened. The new `smoketest/3D/sm64_fast3d_smoke` is the pattern: it owns its own manifest, segment registry, display-list interpreter, and combiner logic instead of baking N64-specific assumptions into the common scene/material API too early.
- The SM64 import refresh workflow for that lane is now profile-driven and lives beside the smoke under `smoketest/3D/sm64_fast3d_smoke`. Use `refresh_sm64_import.bat` and `sm64_import_profile.render_us.json` instead of reconstructing long one-off `import-c` commands from memory.
- The same smoke now has a title-face extraction lane. `extract_sm64_title_face.bat`, `launch_title_face_visual_exe.bat`, and `capture_title_face_snapshot.bat` are the quickest path to a compiled proof that uses real extracted Mario face geometry while keeping N64-specific semantics inside the adapter.
- The adapter is no longer only a smoke-local runtime. The reusable host surface now lives in `crates/kain-fast3d-runtime`, while the smoke folder acts as a consumer that provides manifests, scripts, and validation assets.
- Keep the Fast3D lane data-driven. Its host startup now flows through crate-owned config files and env hooks (`KAIN_FAST3D_CONFIG`, `KAIN_FAST3D_MANIFEST`, `KAIN_FAST3D_SM64_ROOT`) rather than widening Kain language semantics or shared runtime contracts for one experimental console adapter.
- Native app launcher materialization now also supports generic host-sidecar packaging in `kain-driver`: generated launchers can copy arbitrary sidecars into the artifact/executable set, optionally export them as env vars, and switch between the default `run_bundled_app_json(...)` launcher path and a crate-owned no-arg entrypoint like `kain-fast3d-runtime::run_fast3d_cli()`. Preserve that mechanism as a generic host adapter capability, not a Fast3D-specific special case.
- The Bob-omb Battlefield proof now uses the same host-sidecar path with three data files: the extracted scene manifest, a gameplay animation document, and a display-list shader-override document. Keep live actor binding and material experiments in these sidecars instead of adding SM64-specific semantics to `kain-core` or the shared runtime.

## Common Errors

- The root `README.md` is useful, but live source and the built CLI are the real source of truth.
- Fresh Linux and macOS clones should start with the root `install_kain.py` bootstrapper. It is now the cross-platform entrypoint that resolves or installs LLVM, repopulates `toolchain/llvm/bin`, builds `kain`, installs `kain` and `kn`, and emits shell activation scripts under `generated/`.
- Fresh clones may not include a populated `toolchain/llvm/bin/clang.exe` even though older docs and helper scripts reference it. When that happens, install LLVM separately and point `KAIN_CLANG_PATH` at the external `clang.exe`; `scripts/sync-kain-source-of-truth.ps1` now falls back to PATH and `C:\Program Files\LLVM\bin\clang.exe` before assuming the vendored drop exists.
- The `cli` suite no longer depends on the external self-hosting fixture under `M:\Code\Other\kainselfhosting\...`; the repo-local import-c fixture under `crates/cli/tests/fixtures/import_c` is the durable regression source now.
- The repair lane is profile-driven. If a file is being "fixed" in a way that changes meaning, that is a bug in the caller or profile selection, not a feature.
- Large Windows test binaries can hit linker OOM pressure.
- The workspace still pins `pyo3 0.20.x`, so a machine-default Python 3.13+ or 3.14 can break builds. Prefer Python 3.12, set `PYO3_PYTHON` explicitly when needed, and keep the Python 3.12 install directory on PATH so the built `kain.exe` can resolve `python312.dll` at runtime.
- `generated/`, `target/`, `.kain`, runtime sidecars, and compiled smoke outputs are disposable unless explicitly archived under `docs/validation/` or `docs/recent/`.
- The live SM64 decomp root currently sits at `M:\Code\Other\Research\sm64-master\sm64-master`, not the outer `sm64-master` folder. The older stale import reports pointed at the outer folder, which hid a real pathing mistake.
- Linux now validates the core raw-native lane end-to-end: `cargo build -p cli`, `kain build -t llvm`, `./runtime/fixtures/validate_all.sh`, `./runtime/conformance/run_all.sh`, and `./runtime/validate_native_runtime.sh` all pass on a Linux host. The Win32 app-host, input, and viewport host services are still Windows-specific until a non-Win32 native host lands.
- The compute pipeline is mid-transition from heuristic metadata to compiler-owned truth. When touching it, prefer extending bundle contracts over adding new runtime-only inference.
- Multiple authored `world` roots are now treated as an explicit-selection problem, not a guessing problem. If build/run flows see more than one world, require a caller-provided selection instead of silently picking one.
- Frontend bridge registration must be target-scoped. Host/runtime extensions that are valid for `Interpret` or `Test` must not leak into shader artifact compilation or other non-host targets, or Fabric and direct driver paths will diverge.
- The native shader-canvas lane is SPIR-V-canonical at the bundle level, but the current WGPU host still resolves WGSL for execution. Do not mistake that compatibility bridge for permission to move shader-canvas truth out of the emitted bundles.
- The native packaging loop is file-backed. If hot reload or packaged state looks stale, verify the generated `app_manifest.json`, `runtime_snapshot.json`, and launcher env vars before blaming the runtime.
- Fabric Python execution should stay behind `kain-python` helpers. Do not make `kain-host` reach directly into `pyo3` imports or `PythonScopeState` internals when the Python lane can expose a narrower execution API.
- Fabric runtime ownership is now split cleanly: `kain-omni` owns `KAIN.fabric.toml` schema/validation/report types, while `kain-host` owns local execution, dependency plumbing, and runtime adapter behavior.
- Fabric step inputs now flow through raw `fabric_inputs` for every runtime adapter. Kain/C/Rust glue consumes canonical host objects directly, while the Python and Node bridge crates project shared buffer/image payloads into language-native contract objects with `bytearray` and `Uint8Array` bytes views.
- Fabric Python and Node steps now support mixed named outputs when they return a dict/object whose fields match the manifest's declared output names. Shared outputs round-trip through the canonical host-owned interop contract family instead of falling back to string-only placeholders.
- Missing declared Python/Node output fields now fail with structured Fabric errors keyed as `missing_output_field`, with `output_name` recorded in failure details. Preserve that contract surface when touching adapter execution or bridge helpers.
- The durable end-to-end Fabric proof lives under `smoketest/fabric/polyglot_local`. It is the quickest repo-local example of Python -> Kain -> C ABI -> Rust crate -> Node execution with typed shared image/shared buffer flow.
- The durable GPU Fabric proof lives under `smoketest/fabric/gpu_compute_convergence`. If a Fabric GPU step succeeds through `kain gpu-artifacts` but fails through `kain fabric run`, compare `kain-driver` target registration/augmentation behavior first, then inspect Fabric residency metadata and shared-buffer shape/access inference before blaming Vulkan.
- `kain fabric init --template polyglot` now emits a runnable local smoke-grade scaffold, including its local Rust crate manifest and native C fixture, instead of a validation-only placeholder.
- Fabric host smoketests now use deterministic roots under `target/fab-init` and `target/fab-smoke`, preserving `.kain/cache` across separate `cargo test` invocations. If a rerun is still slow, the remaining wall time is usually Cargo recompilation rather than bridge cache misses inside the test body.
- The generated polyglot scaffold also writes `FABRIC.README.md`; treat that file as the first-stop quickstart for the smoke-grade local pipeline shape.
- `smoketest/allinone` is the broad regression umbrella for major CLI and bridge surfaces. Its runner is manifest-driven and clears each lane's generated outputs before rerun, so stale artifacts should not be treated as proof that a current codegen path still works.
- For SM64/Fast3D research, Fabric should stay an optional post-extraction simulation lane that feeds buffers or textures into the adapter. Do not make display-list extraction or the base render loop depend on Fabric before the geometry and segment path is stable.

## Template Packs

`templates/Web` now hosts a manifest-driven universal web starter aimed at users who should not need Rust or Cargo installed.

Key rules for this lane:

- Kain owns orchestration and semantic UI preview entrypoints.
- Node FFI owns browser packaging, local serving, and actor-server runtime glue.
- themes, content, scenes, and experiences are registry-driven data, not scattered starter literals.
- web template boilerplate should prefer reusable stdlib wrappers (`std::javascript::site_runtime`, `std::javascript::site_actor`) plus shared helper runtimes over copy-pasted starter code.
