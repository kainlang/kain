# Kain Working Memory

This file is the running memory for major architectural moves in `M:\Code\Kain`.
It is not meant to be a raw changelog.
It should preserve:

- what we were trying to make true
- what the system understands now that it did not before
- what design bets we made on purpose
- what remains incomplete or dangerous
- what future work should preserve instead of accidentally undoing

## 2026-03-26 - GPU Compute Pipeline Converged Into Fabric Executor

The Tensor/Compute Pipeline and the Fabric Polyglot Executor now share a unified execution path. `gpu_compute` is a first-class Fabric runtime kind and `compute_plan` is a valid contract kind.

### What changed

#### kain-omni (manifest schema layer)
- `FabricRuntimeKind::GpuCompute` added — requires `shader_source` and `compute_key` fields on `FabricStep`
- `FabricContractKind::ComputePlan` added with its own `FabricComputeDispatchSnapshot` for session reports
- Step shape validation enforces compute-specific field presence
- Capability registry includes `runtime.gpu-compute` and `contract.compute-plan`

#### kain-host (executor layer)
- `GpuComputeAdapter` registered in `FabricExecutor::new()` (6th adapter slot)
- Adapter flow: read shader → `compile_shader_artifact_bundle` → build residency from bundle entry_points/resource_layouts → write sidecars → `VulkanComputeExecutor::dispatch_from_sidecars` → map output bindings to `KainSharedBuffer` values
- Upstream shared-buffer inputs flow through `resolve_upstream_binding_bytes` into binding payloads
- `normalize_contract_value` and `snapshot_contract_value` extended for `ComputePlan`
- All Python/Node harness match arms extended for `ComputePlan`
- `kain-gpu-runtime` and `serde` added as dependencies

### Design decisions

- **No Fabric-specific compute dialect** — the adapter uses the same `kain.shared.buffer` contract family as all other adapters
- **Residency built from bundle metadata** — the adapter constructs residency entries from the `ShaderArtifactBundle.entry_points` and `resource_layouts`, not from a new `compute_plans` field. This avoids schema changes to `kain-core::ShaderArtifactBundle`.
- **`ComputePlan` contract kind** — added as a forward-looking concept for steps whose output is dispatch metadata rather than raw data. In practice, most GPU compute steps will output `SharedBuffer` values.

### What remains incomplete

- **Residency shape inference**: binding shapes default to `[1]` when inferred from `resource_layouts`. Future work should propagate upstream tensor shapes or extract them from shader reflection.
- **Workgroup/dispatch size**: currently hardcoded to `[8,1,1]`/`[1,1,1]`. Should be inferred from the compiled shader's `workgroup_size` metadata or declared in the Fabric manifest.
- **Dispatch reporting**: while `FabricComputeDispatchSnapshot` is defined and populated, it is not yet surfaced in the session report JSON flow.
- **End-to-end integration proof**: no smoketest yet exercises the full `Python → Kain → GPU → Node` manifest. The compile-time wiring is complete; runtime validation requires a machine with Vulkan support.

## 2026-03-26 - SM64 Fast3D Research Landed As An Isolated Smoketest Adapter Lane

The first serious SM64/Fast3D proof did not go into `crates/kain-3D`. It landed as a dedicated adapter smoke under `smoketest/3D/sm64_fast3d_smoke`.

What changed:

- a standalone local Rust crate now owns a manifest-driven Fast3D-style viewer executable with its own display-list model, segment registry, matrix stack, texture generation, software rasterizer, and small combiner compiler
- the smoke manifest (`scene_manifest.json`) is data-driven on purpose: textures, segments, display lists, and commands live in smoke data instead of being hardcoded into the viewer binary
- the crate supports a headless `--snapshot` mode in addition to the native viewer window, which makes it possible to validate the lane without requiring an interactive desktop loop
- helper scripts now build the release viewer, launch the native executable, and emit a snapshot into the smoke outputs folder

Why this matters:

- it gives the repo a real place to explore SM64-style native port architecture without prematurely widening the shared `kain-3D` scene/material contract
- it creates a concrete adapter target that future `sm64_all.kn` extraction work can emit into, instead of requiring the core renderer to understand raw N64 display-list semantics immediately
- it keeps the architecture honest about current shared-renderer limits: `kain-3D` does not yet expose UV-textured materials or console-style combiner contracts as first-class shared API, so the Fast3D lane should stay isolated until the generalized contract is clearer

What future work should preserve:

- keep SM64/N64-specific display-list, segment, and combiner semantics in the adapter lane until the repo is ready to define a genuinely reusable textured command-stream contract
- treat the manifest format as the source of truth for this smoke lane so extractors, converters, or importers can target one stable shape
- keep snapshot-capable validation in the lane even if a richer real-time viewer or native runtime integration arrives later

Next serious move:

- add an extractor stage that can lower selected imported `sm64_all.kn` geometry/display-list data into this manifest format
- widen the adapter with more Fast3D commands, texture formats, and matrix behaviors before attempting direct runtime/native hosting
- once the adapter proves the semantics, decide which parts deserve promotion into shared compiler/runtime contracts versus remaining console-specific tooling

Update:

- the stale SM64 import problem was partly a bad source root: the live decomp currently lives under `M:\Code\Other\Research\sm64-master\sm64-master`
- a repeatable refresh lane now lives beside the smoke as `refresh_sm64_import.ps1`, `refresh_sm64_import.bat`, and `sm64_import_profile.render_us.json`
- the refreshed staged import proved that normal C under `src/game`, `src/engine`, and related folders is increasingly workable through `import-c`, while most `actors/*/geo.inc.c`, `actors/*/model.inc.c`, `levels/**/model.inc.c`, and many behavior `.inc.c` files still fail because their macro-expanded form is not normal C after preprocessing
- the profile-driven refresh currently imports `573` files with the render-facing US/Fast3D-old subset and fails `1556`, with the biggest failure groups under `levels`, `actors`, and `src/game/behaviors`

Why this matters:

- it separates two different problems that were easy to conflate: stale pathing versus importer limits on inc-style display-list dialects
- it gives the repo a stable way to refresh the imported subset whenever the external decomp changes, instead of letting one dated generated folder become folklore
- it clarifies the clean next move for direct SM64 rendering: build an extractor or importer extension aimed at geo/model/collision inc assets instead of trying to stuff those N64 macro dialects straight into the shared Kain pipeline

What future work should preserve:

- keep the SM64 import recipe data-driven and colocated with the Fast3D smoke so it stays aligned with the adapter it feeds
- keep display-list extraction isolated from shared `kain-3D` contracts until the repo is ready to generalize that command-stream shape
- use Fabric only after extraction is stable, as an optional simulation lane that writes buffers or textures back into the adapter, not as a prerequisite for the base SM64 port path

Update:

- the smoke now has a real extractor entrypoint in `smoketest/3D/sm64_fast3d_smoke/local_crate/src/extractor.rs` that reads `actors/mario/model.inc.c`, parses light groups, vertex arrays, display lists, and a small SM64 combine-mode subset, and emits `scene_manifest_title_face.json`
- that adapter logic has now been promoted into the workspace backend crate `crates/kain-fast3d-runtime`, and the smoke folder's scripts build and run the workspace crate instead of a smoke-local engine binary
- the runtime now shades lit vertices after model transforms instead of before them, which matters for imported geometry because extracted display lists often need adapter-owned rotation and recentering before they resemble a camera-facing scene
- dedicated helper scripts now build, extract, launch, and snapshot the title-face lane without touching shared renderer crates: `extract_sm64_title_face.bat`, `launch_title_face_visual_exe.bat`, and `capture_title_face_snapshot.bat`
- the current title-face proof uses real extracted Mario face geometry and display-list structure, but still uses generated fallback title-card and facial textures because the external checkout does not ship the original extracted title-screen blobs or baserom assets
- the backend crate now owns a data-driven host config contract (`viewer`, `snapshot`, `extract_sm64_title_face`), and the smoke scripts call that contract through `host_configs/*.json` plus env expansion instead of baking more ad hoc arguments into launcher scripts
- `kain-driver` now has a generic host-sidecar packaging mechanism for native app materialization, plus a launcher entrypoint enum that can either call the default bundled UI runtime path or a crate-owned no-arg function
- the Fast3D smoke now uses that generic mechanism to materialize a real packaged native host under `smoketest/3D/sm64_fast3d_smoke/generated_native_host`, with copied `KAIN_FAST3D_CONFIG` and scene-manifest sidecars and a generated executable under `outputs/native_host`
- the end-to-end proof is a packaged launcher, not a direct smoke script shortcut: running `sm64-fast3d-native-host-snapshot.exe` writes `sm64_title_face_native_host_snapshot.png` from copied sidecars without any Fast3D-specific code in `kain-driver`

Why this matters:

- it moves the SM64 lane from abstract adapter architecture into a repeatable compiled proof with a concrete screenshot path
- it validated that the clean backend investment is not “teach shared `kain-3D` about N64,” but “teach the isolated adapter to parse a little more of the SM64 display-list dialect and texture/combine contract”
- it surfaced a reusable runtime lesson: imported retro geometry often needs lighting to happen after adapter-space transforms, not at raw vertex-load time
- it keeps the experiment honest about ownership: the crate can call into Kain-hosted power later, but the language and shared native runtime do not need SM64/Fast3D-specific semantics baked into them first
- it proves the next integration step cleanly: experimental backend crates can now be hosted by the existing native app materializer through generic sidecars and launcher selection, instead of forcing everything through `kain-ui-native` assumptions

What future work should preserve:

- keep widening the extractor and combiner mapping inside the smoke lane before promoting any semantics into shared crates
- prefer exact texture and segment extraction over inventing more fallback art once the required assets are available
- if Goddard/title-screen parity becomes the goal, add that as another extractor target beside the current face smoke instead of replacing the simpler model-inc proof
- keep the driver-level host-sidecar contract generic and data-driven; future adapters should bring their own config/data sidecars and entrypoints instead of widening shared runtime contracts prematurely

## 2026-03-25 - Fabric Polyglot Execution Became A Real Local-First Lane

Fabric stopped being mostly a manifest validator plus partial executor and became a runnable local-first polyglot pipeline across every declared runtime kind.

What changed:

- `kain-host` now executes `kain`, `python`, `rust_crate`, `c_abi`, and `node` Fabric steps as real local runtime adapters instead of stopping after the Kain/Python lanes
- Fabric step outputs now stay aligned with the canonical contract kinds through typed host payloads, so downstream steps consume named `value`, `shared_buffer`, and `shared_image` results instead of string-only placeholders
- Python and Node Fabric glue now support mixed named outputs by returning a dict/object whose fields match the manifest output names, while shared payloads still remain host-owned canonical handles under the hood
- `kain-omni`'s `kain fabric init --template polyglot` scaffold now emits the same smoke-grade local topology that the repo uses for end-to-end proof, including a local Rust crate manifest and native C fixture
- the `cli` import-c self-hosting regression no longer depends on `M:\Code\Other\kainselfhosting\...`; it now uses a repo-local fixture, and the over-eager C-FFI target gating bug was fixed so non-C-import sources stop failing unrelated staging tests

Why this matters:

- Fabric now feels like a real execution layer inside the repo's existing architecture rather than a future-facing placeholder
- the host/runtime split stayed honest: `kain-omni` still owns manifest schema and validation, while `kain-host` owns runtime execution and typed dependency plumbing
- the default polyglot template is now a practical onboarding path instead of a manifest that validates but cannot prove the full local pipeline

What future work should preserve:

- keep Fabric contract truth anchored to the canonical interop payload kinds instead of growing a second output typing system
- keep Python and Node on the canonical `fabric_inputs` lane now that their bridge crates can project shared buffer/image contracts into language-native objects without reopening host-internal handle APIs
- keep polyglot smoke fixtures and generated templates close enough that one can continue serving as the proving ground for the other

Update:

- Python and Node multi-output steps now fail with a structured `missing_output_field` Fabric error when a declared output field is absent, so downstream debugging no longer depends on raw bridge exception text
- `kain fabric init --template polyglot` now writes `FABRIC.README.md` alongside the runnable scaffold so the generated project itself explains the smoke-grade pipeline shape and quickstart commands

Update:

- the Python Fabric lane now receives canonical `fabric_inputs`, with `kain-python` projecting shared buffers and images into contract-shaped Python objects whose `bytes` fields are real `bytearray` values instead of serialized JSON blobs
- the Node Fabric lane now receives the same canonical `fabric_inputs`, with `kain-node` projecting shared buffers and images into contract-shaped JavaScript objects whose `bytes` fields are `Uint8Array`
- `kain-host` smoketests now keep deterministic work roots under `target/fab-init` and `target/fab-smoke`, preserving `.kain/cache` across separate `cargo test` invocations so repeated end-to-end Fabric runs no longer rebuild bridge artifacts inside the test body
- the first rerun after switching to deterministic roots still seeds those stable caches; after that seed, the heavy `polyglot_fixture` and `polyglot_init_template` test bodies drop back under a second and the remaining wall time is ordinary Cargo compilation

## 2026-03-25 - Viewport Camera And Presentation Defaults Became Bundle-Owned

Viewport startup behavior stopped being mostly host-local inference and became a wider part of the realtime bundle contract.

What changed:

- `RealtimeSceneBinding` in `kain-core` now carries optional `camera` and `presentation` metadata for viewport scene bindings
- authored viewport props like `camera.position.*`, `camera.target.*`, `camera.fov_y`, `viewport.profile`, `viewport.fog_density`, and `viewport.particle_budget` now serialize into the realtime bundle instead of disappearing after UI compilation
- `kain-ui-native` now seeds each viewport surface from bundle-owned camera metadata, and hot reload only recenters when the authored scene or authored camera binding actually changes
- the raw-native realtime sidecar parser now reads the same camera/presentation metadata, and the Win32 viewport host applies those values to initial camera pose, FOV, clip planes, profile defaults, fog density, and particle budget

Why this matters:

- it keeps viewport startup semantics in the compiler-emitted bundle family instead of leaving camera and presentation defaults split across two native hosts
- it gives authored SPIR-V/3D smokes a richer contract surface than just `scene` string matching
- it creates a better base for future work like camera rails, named presets, or host-agnostic cinematic startup behavior

What future work should preserve:

- keep bundle-owned viewport metadata optional and additive so older realtime bundles still load cleanly
- let explicit operator env vars remain last-mile overrides, but do not let host-local hardcoded defaults become the primary source of viewport intent again
- widen the shared bundle schema when new viewport semantics appear instead of teaching each host a separate camera or presentation dialect

## 2026-03-25 - Native Alpha Contract Spine Landed For Scene, Query, Inspection, And Ingestion

The native runtime now has a dedicated Alpha-owned contract layer for the first blocker wave instead of leaving those concepts implicit in the Win32 viewport host.

What changed:

- a new native scene ABI header and core implementation now define opaque scene handles, transactional mutation receipts, scene-query requests/results, and backend/device capability descriptors
- the runtime reflection ABI now includes runtime-inspection query/record surfaces so native tools can correlate runtime-owned state with compiler reflection metadata through one canonical contract family
- the native asset ABI now includes ingestion descriptors so staged host assets and emitted bundles have a shared descriptor shape instead of being only environment-variable conventions
- the native service catalog, runtime contract mask table, runtime manifest, and legacy umbrella source list were updated so these new Alpha surfaces are visible to the runtime's canonical metadata instead of existing as isolated helper code

Why this matters:

- it creates a real contract boundary between Alpha's substrate work and Delta's editor/workspace work
- it reduces the risk that viewport, inspector, and packaging work will invent a second scene or ingestion model inside host-specific code
- it gives future runtime bundle and tool work a more honest place to attach scene identity, query, inspection, and ingestion semantics

What is still incomplete:

- the Win32 viewport and UI lanes are not consuming these contracts end-to-end yet
- scene handles are canonical tokens now, but there is not yet a runtime-owned scene registry behind them
- runtime inspection is a usable ABI surface, not yet a fully wired live-host query engine
- ingestion descriptors exist, but most bundle loading still starts from existing path and env flows

What future work should preserve:

- keep scene identity, scene queries, mutation receipts, backend reflection, runtime inspection, and ingestion descriptors in shared native headers rather than re-declaring them inside viewport or UI code
- make Delta consume Alpha's contracts instead of smuggling editor semantics into platform-specific message handlers
- prefer descriptor-driven ingestion and registry-driven service discovery over more ad hoc environment-variable-only expansion

Next serious move:

- wire the new scene/query contracts into the Win32 viewport and native UI shell
- build the first runtime-owned scene registry behind the opaque handles
- then start Alpha Phase 2 with render-graph, residency, and compute-scheduling descriptors that reuse the same contract discipline

## 2026-03-25 - Native Alpha Graphics Contracts Landed As Synthesized Render Graph, Residency, And Schedule Surfaces

Alpha Phase 2 is now real in the native graphics bundle path instead of staying a wishlist item in planning docs.

What changed:

- `kain_runtime_graphics.h` now defines canonical render-graph, residency, and compute-schedule contract structs that live directly on `KainRuntimeGraphicsBundle`
- `kain_runtime_realtime.c` now synthesizes those contracts from the current emitted graphics-bundle truth during bundle load, instead of leaving pass sequencing, resource residency, and cross-lane ordering implicit in host-local code
- graphics validation now treats render graph, residency, and compute schedule as first-class readiness checks, and compute execution state now reports schedule counts and the primary schedule key
- Alpha published a contract freeze for downstream work in `docs/kainplan/native-runtime-three-agent/alpha-phase2-contract-freeze.md`

Why this matters:

- it gives Delta a stable execution-facing contract family to consume instead of reverse-engineering the Win32 host path
- it moves the native lane closer to the repo rule that host code should consume canonical contracts rather than inventing local orchestration models
- it creates a bridge between current compiler outputs and future compiler-authored explicit render/residency/schedule tables without changing the public ABI again

What is still incomplete:

- the current render graph, residency bytes, and schedule barriers are synthesized heuristics, not compiler-authored explicit execution tables
- runtime reflection is not yet widened enough to enumerate every residency resource and schedule node as a live inspection service
- Delta has not consumed these contracts through the viewport, workspace, or editor shell yet

What future work should preserve:

- keep render graph, residency, and schedule semantics on the shared graphics bundle instead of growing a second execution model in platform-specific code
- replace synthesized values with authored compiler truth by widening emitters, not by deleting the ABI surface
- keep the Phase 2 freeze doc updated when any field becomes authoritative enough for Delta or Charlie to rely on

Next serious move:

- make the Win32/native UI lanes consume the new schedule and render graph contracts in their validation and startup flow
- widen runtime inspection to expose residency resources and schedule steps through the Phase 1 reflection family
- then move Alpha toward compiler-authored contract emission so synthesis becomes compatibility fallback instead of the primary source

## 2026-03-25 - Shader Canvas Became An Explicit Native UI Lane

The native shader-canvas path stopped being only a best-effort surface heuristic and became an explicit bundle-driven lane.

What changed:

- `RealtimeAppBundle` now carries `shader_canvases` metadata so native hosts can resolve shader-canvas surfaces from compiler-emitted bundle truth instead of only rediscovering shader refs from surface-local props
- the native host now keeps a `shader_canvases_by_surface` catalog and resolves shader canvases through that lane first, with surface-local fallback only when bundle metadata is missing
- presented shader surfaces now upload a richer built-in runtime payload: uniform data, storage payload data, and a small fallback texture sample instead of leaving storage/texture bindings as inert placeholders
- shader-surface diagnostics in the native host now expose payload format and resolved shader-bundle ref identity so shader-canvas failures are easier to explain from the UI itself

Why this matters:

- it makes shader canvas a real native UI subsystem instead of a fragile convenience path
- it keeps the repo aligned with its architecture rule that hosts should consume compiler-owned bundle metadata rather than inventing renderer-local contracts
- it creates a practical base for the harder next steps like text atlases, glyph buffers, and richer per-surface instance payloads without throwing away the retained semantic UI model

What is still incomplete:

- the current WGPU native execution path still consumes WGSL or SPIR-V-to-WGSL transpilation rather than driving raw SPIR-V directly through a Vulkan-first host
- automatic shader-canvas resource provisioning is still intentionally small and built-in; it is not yet a full data-driven asset/font/texture registry for serious text-heavy UI
- text, accessibility, and high-level retained UI state still belong to the host/runtime side; shader canvas is an accelerated lane, not the whole UI architecture

What future work should preserve:

- keep shader-canvas binding truth in `kain-core`/`RealtimeAppBundle`, not re-inferred ad hoc inside `kain-ui-native`
- keep the retained semantic UI/runtime model as the source of layout, focus, and tool-state truth even when more pixels move into shader-canvas execution
- extend shader-canvas resources through typed bundle contracts and reflection-driven binding metadata instead of baking more name-based guesses into the host

Next serious move:

- add typed font atlas and glyph-run contracts so shader canvas can own high-performance text without abandoning compiler/runtime truth
- promote shader-canvas resource catalogs beyond the current built-in payloads into reusable bundle-driven image/buffer/font bindings
- if direct SPIR-V execution becomes necessary for the next performance step, add it as another host adapter that still consumes the same shader-canvas bundle contract family

## 2026-03-25 - Shader Canvas Text Contract Started Provisioning Real GPU Text Resources

Shader canvas stopped being "shapes plus a generic payload" and gained the first real text/resource contract that native hosts can provision automatically.

What changed:

- `kain-core` now derives per-surface shader-canvas font atlas descriptors, text runs, and runtime resource bindings directly from UI node props instead of leaving text as an implicit future idea
- `kain-ui-native` now carries those metadata structures through shader-surface resolution, folds them into the shader-surface signature, and exposes the counts in the on-surface diagnostics
- presented shader surfaces now serialize a richer storage payload that includes shader-canvas header data plus atlas records, text-run records, atlas glyph bytes, and run text bytes
- the native host now synthesizes a packed bitmap atlas texture from bundle metadata, records per-atlas origins in the shader storage payload, and reuses cached packed textures when surfaces share the same atlas content
- focused validation confirmed the new bundle-emission contract in `kain-core`, and once the Fabric/Python boundary was fixed the previously blocked `kain-ui-native` validation path turned green as well

Why this matters:

- shader authors now have a real first-class lane for atlas-backed text and per-surface resource catalogs instead of a placeholder story
- the host/runtime still consumes compiler-owned metadata rather than inventing a second renderer-local text schema
- the design stays data-driven enough to grow toward multiple atlases, typed image resources, and richer shader-authored widgets without throwing away the retained UI model

What is still incomplete:

- the current atlas generator is intentionally simple and bitmap-based; it is a real GPU text resource path, not yet a high-quality font rasterization pipeline
- the current host path now reuses cached packed textures across repeated atlas signatures, but it is still a per-app cache rather than a global atlas service or a bindless multi-texture system
- direct SPIR-V execution for the UI lane is still future work; the current native host remains WGSL/WGPU-backed even though the bundle contract is SPIR-V-canonical

What future work should preserve:

- keep text/resource declarations in `RealtimeAppBundle.shader_canvases` so host provisioning stays compiler-owned and host-agnostic
- widen the storage/texture contract in additive ways instead of replacing it with a renderer-local special case once richer text or image resources land
- keep the Fabric/Python boundary narrow by exposing execution helpers from `kain-python` instead of letting `kain-host` depend on `pyo3` imports or private Python scope internals

Update:

- `kain-ui-native` now prefers `ab_glyph` rasterization for shader-canvas font atlases and resolves that through a small data-driven registry of system-font aliases and candidate paths, with `kain.default-ui-sans` as the default emitted atlas family from `kain-core`
- shader-canvas font atlases can now point at the shared realtime asset catalog through `asset_key`, and `kain-driver` materializes those font files into the native app artifact set while rewriting the emitted realtime bundle to the packaged filenames
- native app bundle compilation now carries an explicit `source_root` through `NativeAppBundleConfig` and `NativeAppMetadata`, so relative realtime asset sources are resolved against the authored input location during packaging instead of whichever working directory happens to run materialization
- `kain-ui-native` now resolves packaged realtime font assets relative to the loaded realtime bundle before falling back to system aliases, so font quality is no longer tied to host-local font installation for packaged apps
- the packed multi-atlas texture contract, atlas-origin storage records, and per-app texture cache were preserved exactly, so the quality upgrade stayed under the existing shader-surface resource contract instead of inventing a second text path
- the bitmap 5x7 rasterizer remains as compatibility fallback when neither a packaged asset nor a requested font alias can be resolved on the current machine

## 2026-03-25 - Fabric Python Execution Stopped Leaking Through Kain Host Internals

The `kain-host` Fabric runtime no longer reaches directly into Python implementation details just to execute a Python step.

What changed:

- `kain-python` now exposes `execute_python_source(env, source)` as the narrow public seam for running Python source inside the registered Kain Python scope and surfacing an optional `result` value or `run()` return value
- `python_scope_state` and `scope_dict_from_guard` went back to private helper status inside `kain-python`, so the crate no longer publishes a private type through a public API
- `kain-host`'s Fabric `PythonAdapter` now delegates Python execution through `kain_python::execute_python_source` instead of importing `pyo3` directly or reading `PythonScopeState.scope`
- with that boundary cleaned up, `cargo check -p kain-host --lib`, `cargo check -p kain-ui-native --lib`, and the previously blocked `cargo test -p kain-ui-native resolve_shader_surface_uses_runtime_catalog_and_wgsl_output -- --nocapture` all succeeded

Why this matters:

- it keeps Python runtime ownership in `kain-python` instead of letting `kain-host` grow a second, leaky Python integration surface
- it removed the exact blocker that was masking real `kain-ui-native` shader-canvas validation
- it gives future Fabric Python work a cleaner extension seam than direct `pyo3` coupling inside the host crate

What future work should preserve:

- if Fabric needs richer Python behavior, add explicit helpers to `kain-python` instead of reopening access to internal scope state
- keep `kain-host` focused on host/runtime orchestration, not Python object model ownership
- prefer narrow execution APIs over public state handles when one crate owns a language bridge

## 2026-03-25 - Native Runtime Service Discovery Stopped Pretending The Runtime Was Smaller Than It Is

The raw-native runtime now has one shared canonical service catalog in `runtime/native/src/core/kain_runtime_services.c` instead of splitting service truth across tiny contract-era registration helpers and scattered tests.

What changed:

- the native runtime service registry now registers the broader implemented surface, including scene/runtime inspection and asset-ingestion lanes, not just app host, input, viewport, glTF, UI bundle, and compute
- contract/service lookups now canonicalize legacy aliases like `native.app-host` and `native.compute` through the shared service layer
- repeated registry population now refreshes in place instead of silently trying to duplicate service entries
- service conformance tests now expect the fuller runtime catalog, including contract, reflection, actor, async, realtime, compatibility, and host bridge services

Why this matters:

- startup validation, host integration, and diagnostics can now reason about the same runtime surface instead of each seeing a different partial picture
- this narrows a long-standing honesty gap where the runtime had real subsystems but the registry still advertised a much smaller platform
- future runtime features can extend one table-driven catalog rather than reintroducing scattered key checks and one-off registration paths

What future work should preserve:

- keep the service catalog in one shared runtime-owned place and make new services land there first
- keep legacy alias support at the service layer, not duplicated in contract code and tests
- distinguish clearly between the richer registry catalog and the still-smaller legacy contract service mask until the bundle contract itself is widened

Next serious move:

- widen compiler/driver-emitted runtime contract service masks so bundle-level startup metadata can express more of the same catalog the registry already exposes
- align `runtime/native_runtime_metadata.json` and any downstream docs/fixtures with the richer service truth so machine-readable runtime metadata stops lagging behind the registry
- consider introducing service-family level conformance checks for actor, async, reflection, and host bridge discovery instead of only presence checks

## 2026-03-25 - 3D Scene IDs, Shader Bundles, And Raw-Native Profiles Were Re-Aligned

The 3D/runtime lane had drifted into an awkward split:

- authored UI/realtime bundles could name scenes like `tensor_stream_probe` and related SPIR-V smokes
- `kain-ui-native` could load shader bundles from disk, but the presented WGPU viewport path still rebuilt pipelines from the baked default viewport shader
- the raw-native Win32 runtime only understood a smaller hardcoded profile set, so newer authored scene ids degraded back to the default profile instead of resolving intentionally

What changed:

- `crates/kain-3D` now registers a dedicated `tensor_stream_probe` scene plus a small alias map for authored smoke names like `gpu_compute_surface_probe` and `spv_ui_surface_probe`
- the Rust-native viewport host now threads the active `ShaderArtifactBundle` into both the readback renderer and the presented WGPU viewport path, and shader-bundle hot reload now refreshes existing viewport surface state instead of leaving old pipelines alive
- the raw-native Win32 profile registry now has explicit alias-aware resolution and a `tensor_stream_probe` profile so compiled/realtime scene ids from the compiler lane map to a meaningful native profile instead of silently collapsing to the first entry
- the raw-native OpenGL fallback lane now has first-class procedural scene branches for `tensor_stream_probe`, `retirement_demo`, and `kerr_black_hole`, with the tensor probe reacting to live compute execution phase/throughput instead of only showing the old generic city fallback

Why this matters:

- it closes a real cross-lane contract gap between compiler-authored 3D/SPIR-V intent and what the Rust-native and C-native runtime lanes actually render
- shader-bundle hot reload is now honest for viewport rendering instead of only affecting auxiliary shader-surface paths
- authored scene ids are becoming a shared contract surface instead of a lane-local convention
- the native runtime is now a more serious proof lane for authored 3D/runtime identities instead of a single procedural world with renamed presets

What future work should preserve:

- keep scene-name reconciliation data-driven through registries and aliases instead of scattering ad hoc fallback `if` checks across runtimes
- keep `ShaderArtifactBundle` as the single shader payload contract for viewport/runtime consumption, even when a runtime still derives WGSL as a compatibility bridge
- keep raw-native profiles as substrate-level runtime presentation presets, not the place where semantic scene truth is invented

What is still incomplete:

- the raw-native OpenGL viewport still renders profile-driven procedural geometry rather than the richer Rust `SceneCatalog` scene graph
- no heavy validation was run in this pass yet; test execution remains gated on user approval per repo policy
- viewport camera and presentation metadata are now shared, but full raw-native geometry still comes from profile-driven procedural rendering instead of one shared scene runtime

## 2026-03-25 - SPV UI Smoke Landed As An Honest First Probe Instead Of A Fake UI Engine Claim

The repo now has a dedicated UI smoke for the "SPIR-V-based UI" direction under `smoketest/UI/spv_ui_surface_probe`.

What changed:

- a new smoke combines semantic UI authoring with a compute shader that behaves like a procedural UI surface concept
- the smoke includes the usual interpret/test/native-app runners plus a direct `gpu-artifacts` helper so the emitted `.spv` can be inspected without pretending the host renderer is already complete
- the smoke README states the current boundary explicitly: Kain can emit the shader-side surface idea today, but the full host-side fullscreen-quad/input/text/runtime loop is still future work

Why this matters:

- it gives future work a grounded proof point for "SPV UI" that matches current repo reality instead of skipping straight to Makepad/GPUI-class claims
- it preserves the architecture rule that shader truth should be compiler-authored while the host/runtime side owns windowing, retained state, input routing, and text plumbing
- it creates a small operator-friendly lane for inspecting emitted shader artifacts during UI/runtime experimentation

What future work should preserve:

- keep this lane honest about the current split between emitted SPIR-V artifacts and the still-missing dedicated host renderer
- prefer small, inspectable smoke steps over prematurely claiming a full GPU-native widget engine
- route future pointer/text/state work through shared runtime contracts and buffers instead of inventing one-off smoke-only wiring

Next serious move:

- add a minimal host surface that can present a shader-authored full-screen quad or equivalent canvas in the native UI lane
- thread pointer/window inputs into the shader contract through a stable buffer or uniform path
- then add font atlas and glyph-buffer plumbing so the SPV UI experiment stops being "shapes only"


## 2026-03-25 - Compute Plan Contract Started Moving From Heuristic To Authored

The tensor-stream pipeline now has a stronger compiler-owned contract in `kain-core`.

What changed:

- shader `comptime` compute metadata now supports an extended five-entry form that can author explicit `workgroup_size` and stream plans in addition to dispatch, tensor plans, and neural-node plans
- the older three-entry form stays valid, so existing compute-plan shaders do not need to be rewritten just to keep parsing
- realtime bundle emission now respects authored workgroup and stream metadata when present instead of always inferring those fields from storage buffers and placeholder `LOCAL_SIZE_*` handling

Why this matters:

- this narrows a real architecture gap between "compiler-owned compute truth" and "runtime guessed enough metadata to keep moving"
- stream cadence and direction can now live in emitted bundle data instead of existing only as runtime heuristics
- the bundle path is closer to the repo doctrine that authored Kain meaning should live in `kain-core` and `kain-driver`, not in host-local conventions

What is still incomplete:

- tensor shapes still fall back to inferred defaults when authors do not provide explicit tensor plans
- compute residency is still file-first and writes zero-filled payload sidecars, which is useful for packaging but too weak to become the center of a serious tensor pipeline
- the raw-native runtime still has a metadata/overlay execution path beside the real Vulkan dispatch lane

What future work should preserve:

- keep explicit workgroup, dispatch, tensor, stream, and neural metadata in one compiler-owned compute-plan contract
- do not let future runtime lanes invent their own stream/workgroup dialects once the authored bundle shape exists
- if Fabric, Python, Cargo FFI, or Node steps start orchestrating tensor pipelines, they should consume the same compute-plan and shared-buffer contract family rather than bypassing it

## 2026-03-25 - C ABI FFI Stopped Being Locked To Host-Backed Interpret/Test In The Native-UI Packaging Lane

The repo now has a real native packaging story for `use c::...` imports instead of only a host-backed smoke path.

What changed:

- `kain-c-ffi` now accepts the Rust/native packaging lane during source preparation instead of hard-failing outside `Interpret` and `Test`
- generated C bridge metadata is now explicit: each imported library emits packaged host-bridge descriptors, bridge/shared-library artifact names, and symbol/service registration data
- packaged bridge loading is now a first-class runtime helper in `kain-c-ffi`, so generated native apps can load prebuilt bridge DLLs from a manifest instead of depending on cache-only absolute paths
- generated bridge crates stopped depending on the larger `kain-host` surface and now use local value-conversion helpers on top of `kain-core`, which makes packaged bridge DLLs build cleanly without dragging unrelated host subsystems into the bridge build
- native app materialization now copies bridge DLLs, shared C libraries, and binding/report sidecars into the packaged artifact set, writes an aggregate `kain_c_host_bridges.json`, adds `host.bridge` / `c.ffi` runtime metadata, and generates `config/app_manifest.json` plus `state/runtime_snapshot.json`
- generated native app entrypoints now load the packaged bridge manifest before booting `kain-ui-native`, and the generated Cargo manifest adds `kain-c-ffi` automatically when imported C libraries are present

Why this matters:

- it removes a real honesty gap where the importer could understand C ABI libraries but the native-ui/native-app lane still behaved like those imports did not exist
- it gives the runtime a package-owned bridge manifest instead of relying on transient cache paths and ad hoc operator setup
- it moves Kain closer to one lane-converged foreign-runtime story, even though the underlying native UI host is still bundle-driven rather than a full general-purpose Kain interpreter

What is still incomplete:

- the native-ui app still boots from emitted UI/runtime bundles; packaging C bridges does not by itself turn the current native host into a full arbitrary Kain runtime executor
- static-library packaging is still not modeled separately; today the packaging path is oriented around shared libraries / DLL sidecars
- the emitted host-bridge descriptors are aligned with the native runtime host-bridge ABI shape, but the Rust native UI lane still consumes them through manifest-driven startup rather than a direct Rust binding to the C host-bridge registry

What future work should preserve:

- keep bridge/library packaging data manifest-owned instead of rebuilding it from scattered path guesses
- keep `host.bridge` and `c.ffi` runtime requirements explicit on the emitted native app metadata so downstream tools do not have to infer foreign dependencies from source text
- if the native UI lane grows a richer live Kain execution environment, make it consume the same packaged bridge manifest and descriptors rather than inventing a second foreign-library registry

Next serious move:

- thread explicit stream/workgroup metadata through compute residency and runtime conformance
- replace file-first compute payload handoff with stable runtime/shared-buffer handles or equivalent contract references
- then make cross-lane execution prove that the same authored `primary_compute` plan behaves coherently in raw-native, Vulkan, and host-bridge lanes

## 2026-03-21 - Testing Lane Guide Was Made Explicit

The top-level `testing/` directory finally has a root README that explains how phases progress and which outputs stay disposable.

Key takeaways:

- treat `Intermediate/`, `_Builds/`, `Binaries/`, and compiled artifacts as disposable test outputs
- keep durable test results in `docs/validation/` or `docs/recent/`
- move probes from `Unsorted/` into the smallest stable phase once they are vetted

## 2026-03-21 - Pipeline Output Hygiene Was Re-centered

The pipeline lanes were accumulating compiled outputs in `generated/`, `labs/`, and `smoketest/`.

The working rule now:

- compiled artifacts (`.exe`, `.dll`, `.lib`, `.obj`, `.o`, `.pdb`, `.ilk`) stay disposable
- caches like `target/`, `.kain`, and `.kain-runtime` should be cleared after validation
- any log or validation proof worth keeping should live under `docs/validation/` or `docs/recent/`

## 2026-03-21 - Parent Ignore Globs Were Normalized

Repo-wide searches were getting noisy because the parent `M:\.gitignore` had malformed Windows-style backslash globs.

The fix was simple, but the lesson matters:

- `gitignore` syntax needs to stay portable and valid, even in parent workspace files
- a broken parent ignore file can make repo hygiene work look more broken than the tree actually is
- when search tooling starts warning on ignore parsing, fix the ignore file instead of normalizing the warning away

## 2026-03-21 - Docs Landing Pages Were Restored

The repo map and README had drifted ahead of the filesystem again: `docs/README.md` was missing even though the root docs navigation still expected it.

This pass restored the docs landing pages and tightened the doc anchors so future cleanup work has a real navigation layer to follow:

- `docs/README.md`
- `docs/crates/README.md`
- `docs/pipeline/README.md`

The important lesson is the same one that keeps repeating in this repo:

- if a folder is important enough to show up in the repo map, it needs a living README
- stale memory references should point at current doc anchors, not retired one-off audits
- pipeline docs should stay pinned to the canonical runtime contract and not float as invisible knowledge

## 2026-03-21 - Remaining Stale Root Docs Were Confirmed Safe To Remove

This pass checked the still-pending root markdown deletions against the active docs map and found no current references outside the repo memory itself.

That means these files can stay gone without breaking the current documentation surface:

- `CODEGEN_OPERATOR_AUDIT.md`
- `WILD_FEATURE_RECOMMENDATIONS.md`
- `docs/archive/cleanup.md`
- `docs/archive/EDITOR_PIPELINE_IMPROVEMENTS.md`
- `docs/crates/README.md`

The useful lesson here is that cleanup work should always be confirmed against the live repo maps before being treated as final. The repo-level docs can get ahead of the tree, but the tree must stay internally consistent.

Current run recorded at 2026-03-21T17:17:46.8323301Z.

## 2026-03-22 - C Runtime Pipeline Notes Were Promoted

The C runtime lane now has a dedicated pipeline doc under `docs/pipeline/` to keep
runtime bundle validation, outputs, and cleanup rules anchored in the docs index.

Key takeaways:

- runtime bundle validation should write temporary JSON into `generated/` instead of the repo root
- `graphics_runtime_smoke_*` bundles are disposable and should be removed after each run
- `target/` remains disposable and should be cleared after pipeline runs (some files may be locked)

Supporting updates:

- `crates/README.md` now points at the crates maintenance pipeline doc
- `ouroborosV2/README.md` is now the folder guide for the nested repo
- the stale root `graphics_runtime_smoke_env_bundle.realtime_app.json` artifact was removed

Current run recorded at 2026-03-22T00:19:52.9857184-04:00.

## 2026-03-21 - Stale Root Docs And Empty Placeholders Were Removed

This pass cleaned a small set of dead markdown artifacts that were no longer referenced by the active repo docs:

- `CODEGEN_OPERATOR_AUDIT.md`
- `WILD_FEATURE_RECOMMENDATIONS.md`
- `docs/archive/cleanup.md`
- `docs/archive/EDITOR_PIPELINE_IMPROVEMENTS.md`
- `docs/crates/README.md`

The useful lesson from this run is that repository searches can still be tripped up by parent ignore files outside the workspace. If `rg` starts failing on glob parsing, use `--no-ignore-parent` instead of assuming the repo itself is broken.

Current run recorded at 2026-03-21T12:53:13.2630386-04:00.

## 2026-03-21 - Kain Fabric Phase 1 Landed As A Real Manifest And Validation Surface

Today `Kain Fabric` stopped being only a product idea and became a real repo-visible entry point.

The important truth is narrow on purpose:
Fabric is not a distributed runtime yet.
It is not a cloud scheduler.
It is not a replacement for compiler-owned execution semantics.

What became real is the first honest layer:

- a canonical `KAIN.fabric.toml` manifest
- local-first Fabric templates
- typed runtime-step declarations for `kain`, `python`, `rust_crate`, `c_abi`, and `node`
- capability validation
- dependency-cycle and duplicate-id validation
- first-class CLI commands for `kain fabric init`, `kain fabric validate`, and `kain fabric run`

### What Changed In Practice

On the orchestration side, `crates/kain-omni` now owns a real Fabric manifest/validation path instead of leaving the concept as a doc-only plan.

That path includes:

- manifest schema/version truth
- local and polyglot starter templates
- runtime kind declarations
- contract-kind declarations
- local capability validation
- dependency graph validation

On the CLI side, `crates/cli` now exposes Fabric as a first-class command family instead of hiding it behind future-work docs.

The commands are intentionally split by honesty:

- `kain fabric init` scaffolds a workspace and starter manifest
- `kain fabric validate` parses and validates a Fabric manifest
- `kain fabric run` validates successfully and then explicitly reports that execution is not wired yet

That last point matters.
The run command is a truthful stub, not a fake implementation dressed up as a platform.

### Files That Became The First Fabric Spine

- `crates/kain-omni/src/fabric.rs`
- `crates/kain-omni/src/lib.rs`
- `crates/cli/src/fabric.rs`
- `crates/cli/src/lib.rs`
- `crates/cli/src/main.rs`

### Architectural Meaning

Three design bets became real today:

1. Fabric will grow out of existing manifest infrastructure, not beside it.
   `kain-omni` is now the home for Fabric manifest truth.

2. Fabric will be local-first before it is ambitious.
   The validator knows about local capabilities and local runtime kinds first.

3. Fabric will stay subordinate to compiler/runtime truth.
   It validates orchestration shape.
   It does not define what compute, UI, shader, or runtime semantics mean.

### Why This Matters

This is the first point where Kain can start moving from:

- "we have many bridges and many targets"

toward:

- "we have one typed entry point for heterogeneous software composition"

That is strategically important because it gives Kain a practical adoption wedge that does not require users to rewrite everything into Kain first.

### Validation That Passed

The focused validation loop for the first Fabric slice passed:

- `cargo fmt --package kain-omni --package cli`
- `cargo test -p kain-omni fabric -- --nocapture`
- `cargo test -p cli fabric -- --nocapture`
- `target/debug/kain.exe fabric --help`
- `target/debug/kain.exe fabric init --help`
- `target/debug/kain.exe fabric validate --help`

This does not mean the full workspace is globally clean.
It means the Fabric phase-1 slice compiles and validates inside the existing repo reality.

### What Is Still Incomplete

- No Fabric executor exists yet.
- `kain fabric run` does not execute steps.
- No session lock file exists yet.
- No event stream exists yet.
- No `kain-host` Fabric runtime exists yet.
- Python, Rust crate FFI, C ABI, and Node are declared runtime kinds, not executed Fabric adapters yet.
- No end-to-end `smoketest/fabric/*` proof exists yet.

### What Future Work Should Preserve

- Do not turn Fabric into a second semantics layer.
  It should orchestrate runtimes and contracts, not redefine them.

- Do not invent a Fabric-specific compute dialect.
  If compute plans, tensor metadata, or dispatch semantics already belong to compiler-owned bundles, Fabric should consume those outputs rather than replacing them.

- Do not move Fabric ownership into a new god crate if `kain-omni`, `kain-driver`, `kain-interop`, and `kain-host` can keep the boundaries clean.

- Do not claim remote/distributed execution until local session execution is undeniably real.

- Do not let `kain fabric run` become a fake success command.
  It should remain explicit about scaffolded versus implemented behavior.

### Next Serious Move

The next real step is Phase 2:

- add a local Fabric session model in `kain-host`
- make `kain fabric run` execute a Kain-only manifest first
- emit session events and a lock/report artifact
- then wire Python, Rust crate FFI, C ABI, and Node adapters one by one

If that path holds, Fabric stops being "manifest paperwork" and starts becoming a genuine local-first polyglot execution lane for Kain.

## 2026-03-25 - Fabric Phase 3 Became A Real Local Polyglot Executor

This pass closed the gap between "Fabric validates" and "Fabric runs."
`kain fabric run` is no longer a stringly typed Kain/Python stub. The host executor now runs all declared local runtime kinds, preserves contract-aware outputs, and leaves behind session artifacts that are actually useful for debugging.

### What Changed

- `crates/kain-host/src/fabric.rs` now owns a real `FabricSession` executor with adapters for `kain`, `python`, `rust_crate`, `c_abi`, and `node`.
- Fabric step results in `crates/kain-omni/src/fabric.rs` now store typed output snapshots and structured failure reasons instead of one optional output string.
- Fabric reports/locks/events now include richer per-step provenance:
  runtime, adapter label, resolved paths, timestamps, resolved dependency inputs, typed outputs, and structured failures.
- `smoketest/fabric/polyglot_local` is the first durable local-first proof that a declared Python -> Kain -> C ABI -> Rust crate -> Node pipeline can succeed end-to-end.

### Design Decisions That Matter

- `kain-omni` still owns manifest schema, validation, report types, and capability truth. It does not execute runtime steps.
- `kain-host` owns execution and dependency-to-output plumbing.
- Shared payload truth stays aligned to the canonical `contract.value`, `contract.shared-buffer`, and `contract.shared-image` model. No second Fabric-specific contract dialect was added.
- Script lanes receive normalized `fabric_serialized_inputs` because Python and Node cannot accept foreign Kain host objects directly; Kain/C/Rust glue still receives raw `fabric_inputs` so shared handles stay real where the bridge supports them.
- Rust crate and C ABI Fabric steps now expect an `entry` Kain glue file. That keeps the runtime adapters small and local-first instead of forcing `kain-host` to guess call signatures for arbitrary imported functions.

### Current Risks And Follow-Up

- The default `kain fabric init --template polyglot` scaffold is better aligned to the runtime now, but the fully runnable vertical slice still lives in `smoketest/fabric/polyglot_local`.
- Python/Node multi-output steps that mix several shared contract outputs are still intentionally narrow; today the clean path is one shared output or plain value objects.
- The next hardening pass should validate this executor with the intended crate-level test commands and update any docs or CLI UX that still describe Fabric as validation-only.

## 2026-03-21 - LLVM Native Packaging Stopped Being A Side Quest

This pass closed an important emotional gap in the pipeline.
We already had a real compute executor, a residency contract, and a raw-native viewport that wanted to consume them, but the normal LLVM/native build lane was still too casual about staging the runtime truth beside the executable.
That kind of gap is how strong systems quietly turn back into demos.

The main correction was architectural discipline:
we pulled the LLVM/native artifact staging logic out of the CLI monolith and turned it into a dedicated library module.
That sounds small, but it matters because `kn` still includes `main.rs` directly, so every extra ounce of packaging logic left in that file gets duplicated in the noisiest possible compilation path.
Moving the staging code into a real module gave the packaging lane a stable home and stopped the raw-native build contract from living as a brittle side effect.

### What The System Understands Now

The LLVM/native lane now treats these artifacts as a single runtime story, not a bag of unrelated files:

- runtime contract
- realtime app bundle
- compute residency manifest
- compute residency payload binaries
- shader bundle
- `kain_gpu_runtime.dll`

That means a raw-native build no longer has to rely on wishful thinking that the viewport will somehow discover the right compute-side assets later.
The executable lane now stages the files that the runtime actually needs in order to execute `primary_compute` as runtime truth.

### Why The Module Split Matters

There was a deeper lesson hiding here:
the raw-native packaging path is not just another helper.
It is the place where compiler intent, runtime contracts, SPIR-V assets, residency sidecars, and native executable layout all become one physical deployment shape.
That deserves a named seam.

We created `cli/src/llvm_native_stage.rs` specifically so this deployment logic can grow without dragging more complexity into `main.rs`.
This should be preserved.
If future work adds release-vs-debug DLL policy, richer sidecar manifests, or platform-specific staging rules, that logic belongs in the staging module first, not scattered back into the CLI entrypoint.

### Validation Outcome

The good news is that the new packaging seam validated cleanly:

- new CLI tests now prove LLVM/native staging for compute-bearing and UI-only sources
- the native UI packaging regression still passes with compute residency sidecars present
- the native runtime C smoke compile still passes
- full `cargo test` still fails only on the pre-existing external self-hosting fixture under `M:\Code\Other\kainselfhosting\...`

That is exactly the result we wanted.
This move changed the runtime deployment shape, but the workspace-wide failure signature did not get worse or shift in a suspicious way.

### Guardrails

- Do not move the raw-native artifact staging policy back into `main.rs`.
- Do not let the LLVM/native lane emit only the `.ll` and executable while quietly omitting the runtime-side compute assets.
- Do not treat the residency manifest as optional when `primary_compute` is part of the emitted truth.
- If future packaging lanes appear, they should reuse the same staging semantics instead of inventing a second compute deployment dialect.

## 2026-03-21 - Crates Guide Restored And Strategy Notes Indexed

The repo map had drifted ahead of the filesystem again: `crates/README.md` was missing even though the root map still treated it like a first-class navigation point.
I restored that guide, synced the root and crate-level maps, and added a small README for `docs/kainvsgiants/` so the strategy note folder is a deliberate doc surface instead of a loose one-off.

Lesson:

- If a folder is important enough to show up in the repo map, it is important enough to have a real README and stay in sync with the map.
- `kain-gpu-runtime` now needs to stay visible as a runtime executor crate, not buried as a side artifact.
- Stale audit dump docs should be retired in favor of a small, living folder guide.

## 2026-03-20 - Tensor-Stream Compute Lane Becomes Real Compiler/Runtime Memory

Today the work stopped being "Kain has compute shaders somewhere" and started becoming "Kain is learning how to describe a compute-native execution lane as compiler-owned truth."

The most important shift is semantic, not cosmetic:
we pushed the system from vague GPU capability toward a structured model where a compute shader can now carry authored intent about dispatch, tensor payloads, stream roles, and neural-node planning.
That matters because the long-term goal is not just to emit SPIR-V blobs.
The goal is for Kain to understand continuous dataflow at compile time and then hand native runtimes enough structure to execute that intent coherently.

### What Changed In Practice

On the compiler side, `kain-core` now supports explicit compute-plan metadata authored from shader `comptime` blocks.
The current convention is intentionally constrained and conservative:

```kn
let compute = (
    [dispatch_x, dispatch_y, dispatch_z],
    [
        ("binding", "element_type", ["shape", "dims"], "role", "contract")
    ],
    [
        ("node_key", "op", ["inputs"], ["outputs"], stateful)
    ]
)
```

That data is now parsed, validated, and threaded into emitted realtime bundles and runtime contracts.
When present, it overrides the older heuristic-only path for dispatch/tensor/node planning.
When absent, the legacy fallback still exists so the broader tree does not collapse.

On the native-runtime side, the raw-native lane now does more than validate `primary_compute`.
It has a real execution handoff/fallback state path:

- the graphics bundle is loaded explicitly in the raw-native viewport lane
- `primary_compute` is executed into a per-frame runtime state record
- that execution state is surfaced in overlay/debug information
- viewport rendering now reflects that compute state instead of pretending it does not exist

This is not yet true GPU compute dispatch.
It is a runtime-owned bridge between "metadata exists" and "execution semantics are visible and alive."
That bridge is important because it gives us a place to evolve dispatch, residency, scheduling, and future SPIR-V execution without falling back into one-off demo code.

### Architectural Meaning

Three ideas became more concrete today:

1. Compute is no longer just a shader stage.
It is being treated as a first-class execution domain with tensor, stream, and neural semantics.

2. The compiler is beginning to own dispatch intent.
Even though some fallback behavior remains, the direction is now explicit:
dispatch sizing and operator metadata should be authored and emitted, not guessed by hosts forever.

3. The raw-native runtime is no longer purely passive.
It now has a legitimate role in executing and surfacing compute plans rather than only rejecting malformed metadata.

### Design Bets We Made On Purpose

- We preferred `comptime`-block authored metadata over adding a wider public AST break for shader constructors.
  That let the feature land without detonating other crates that instantiate `Shader` directly.

- We treated tensor and stream semantics as data attached to resource bindings, not as hardcoded runtime assumptions.
  This keeps the door open for future backends, NPUs, CUDA, or other ML/runtime targets.

- We added a native execution fallback/handoff instead of pretending full GPU dispatch was already solved.
  This gives us a truthful intermediate substrate that can still drive viewport/runtime behavior.

### What Is Still Incomplete

- True native GPU compute execution is not wired yet.
  The runtime fallback executes a compute-state model, not actual SPIR-V dispatch.

- `workgroup_size` still has fallback behavior when authored metadata is absent.

- Tensor shapes are explicit only when authored.
  Otherwise they still fall back to inferred/simple defaults.

- Neural nodes are still a compiler-emitted operator plan, not a true runtime scheduler with residency, fusion, or dependency orchestration.

- Full workspace tests are still not globally green due to repo-level issues outside this slice.
  The notable blockers during validation were:
  an external missing fixture under `M:\Code\Other\kainselfhosting\...`
  and linker OOM pressure in large CLI test binaries on Windows.

### What Future Work Should Preserve

- Do not collapse tensor/stream/neural metadata back into anonymous compute bindings.
  The whole point is that Kain should progressively understand dataflow structure, not just pass through lower-level payloads.

- Do not move dispatch ownership back into host heuristics if authored metadata exists.

- Do not let raw-native, Rust-native, and future backends invent separate compute-plan dialects.
  The emitted bundle must stay the center of truth.

- If we add real SPIR-V/native compute dispatch next, it should consume the same `primary_compute` plan and enrich it, not replace it with a host-local shortcut.

### Next Serious Move

The next step is to connect this authored compute-plan lane to actual backend execution:

- compiler-owned dispatch/workgroup truth should become standard, not optional
- tensor shape metadata should map to real residency/buffer layouts
- `primary_compute` should dispatch through a real execution backend
- neural-node plans should graduate from descriptive metadata into runtime scheduling primitives

If this direction holds, Kain stops looking like "a language that can target GPU shaders" and starts looking more like "a language/runtime that understands heterogeneous dataflow as part of compilation itself."

## 2026-03-20 - Explicit Compute Plans Landed, Runtime Execution Stopped Being Purely Decorative

The follow-up move today was to stop pretending the compiler and runtime were "close enough" on compute intent.
We added an authored compute-plan path and then made the raw-native viewport consume executable compute state instead of treating `primary_compute` as a validation artifact.

### What Changed

On the compiler side, compute shaders can now carry an explicit authored plan through `comptime` data.
That plan gives the compiler an intentional source of truth for:

- dispatch size
- tensor binding metadata
- neural node planning

This is materially different from the earlier heuristic pass.
The heuristic path still exists for compatibility, but there is now a real authored lane that tells the compiler what the compute workload is supposed to mean.

On the native side, the raw-native viewport now loads the graphics bundle and drives a real per-frame compute execution state.
This is still a fallback execution substrate, not full SPIR-V/Vulkan dispatch in the C runtime, but it means:

- `primary_compute` is stepped every frame
- dispatch counts, tensor counts, stream counts, and neural-node counts now live as runtime state
- that execution state feeds overlay/debug output
- viewport rendering can respond to compute phase instead of acting like compute metadata is inert

### Why This Matters

Before this pass, the runtime could say "the compute plan is valid."
After this pass, the runtime can at least say "the compute lane is alive right now, here is the state it is producing, and the host is reacting to it."

That is still not the final end state, but it is a meaningful transition:
validation-only systems die in place.
Execution-visible systems become pressure points that force the backend story to mature.

### What Is Still Missing

The actual last leap is still ahead:

- full SPIR-V dispatch promoted from test-only Vulkan code into a reusable runtime service
- shared-buffer residency and binding moved from descriptive contracts into true backend resource ownership
- native runtime compute results feeding real scene buffers, materials, particles, terrain, or viewport surfaces instead of only debug/live-state channels

The repo already contains a strong clue for the next move:
`crates/gpu/tests/spirv_execute.rs` is not hypothetical.
It is a real Vulkan SPIR-V execution harness.
The correct direction is to promote that into a reusable runtime/backend service rather than rebuilding execution semantics from scratch in every host.

## 2026-03-20 - The Vulkan Executor Graduated Out Of Test-Only Space

This was the first pass where the SPIR-V execution story stopped living only inside a test and started becoming runtime infrastructure.

The important move was not just "we made another crate."
The important move was that the old `spirv_execute.rs` Vulkan path was promoted into a dedicated runtime-facing module with a C ABI surface, and the raw-native viewport was pointed at that direction instead of only carrying synthetic compute state.

### What Changed

We now have a dedicated `kain-gpu-runtime` crate.
That crate owns the Vulkan setup and SPIR-V dispatch logic that used to be trapped in the GPU test harness.
The old test still matters, but it is now proving a library instead of being the only place where compute execution really exists.

We also moved the residency sidecar from a loose compute metadata snapshot toward an actual bootstrap artifact:

- deterministic compute residency manifest
- per-binding payload files
- resolved descriptor/binding metadata that the runtime can consume

On top of that, `kain-interop` now has a concrete shared-buffer-to-GPU-binding adapter.
That means the `kain.shared.buffer` contract is no longer just conceptual in this lane.
It is beginning to function as the runtime-facing binding truth for compute execution.

Finally, the raw-native viewport now has a real ABI loading path toward the GPU runtime.
It is still early and not yet the final generalized host-bridge form, but the direction is correct:
the C lane is no longer forced to fake compute forever.

### Why This Matters

Before this pass, the best compute execution path in the repo was:

- real Vulkan dispatch in test code
- runtime metadata in production code
- synthetic execution state in the raw-native host

That split was not sustainable.

After this pass, the architecture is more coherent:

- Vulkan dispatch is becoming reusable runtime code
- residency is beginning to exist as a runtime bootstrap contract
- shared buffers have a descriptor-facing adapter
- raw-native is beginning to talk to a real compute executor

That is the first shape that can realistically grow into a serious heterogeneous runtime story.

### What Is Still Incomplete

- The C ABI is intentionally minimal and still path-oriented in places.
  It is enough to establish execution, but it is not yet the final "all buffer metadata passed explicitly as plain structs" design.

- The residency sidecar is now real enough to bootstrap compute bindings, but uniform/scalar policy is still thinner than the storage-buffer lane.

- The raw-native viewport can now prepare for real compute execution, but the packaging and native startup path still need a more complete production handoff for the runtime DLL in all lanes.

- Full workspace validation is still constrained by unrelated repo blockers and Windows linker pressure, so broad green status remains noisy.

### Guardrail

Do not let `kain-gpu-runtime` turn into a random dumping ground for GPU experiments.
It should stay the execution-side counterpart to compiler-owned SPIR-V bundles and residency contracts.
Its job is not to become "another graphics engine."
Its job is to make Kain-owned compute payloads executable as runtime truth.

### Guardrail

Do not let the fallback execution path become the final architecture.
It exists to keep the compute lane alive while we promote real backend dispatch into the runtime story.

## 2026-03-20 - Root Repo Map And Compute Docs Were Brought Back Into Sync

This run tightened the repo's documentation around the compute lane and the top-level layout.

### What Changed

- Added a top-level `repomap.md` so the root workspace has the same folder-guide treatment as `crates/`.
- Updated the README to describe authored compute metadata as compiler-owned truth, not a runtime heuristic.
- Documented the raw-native viewport bridge as a compute execution/state surface, not a full GPU dispatcher.

### Lesson

When a feature starts crossing compiler, runtime contract, and native viewport boundaries, the docs should call out the ownership split explicitly.
That keeps future changes from collapsing authored intent back into host-local inference.

## 2026-03-21 - Crates + App/Toolchain Guides Hardened

The docs layer now explicitly tracks the crates maintenance pipeline, and the repo has folder guides for `apps/` and `toolchain/`.

The intent is to keep the crate surface and tooling lanes data-driven and discoverable:

- `docs/pipeline/CRATES_PIPELINE.md` defines the update order for crates metadata.
- `apps/README.md` and `toolchain/README.md` keep app outputs and toolchain drops understandable.


## 2026-03-22 - Research + Report Lanes Re-homed

The top-level `Research/` and `reports/` folders were moved into the docs layer to keep the repo root focused on source and runtime lanes.

### What Changed

- Consolidated `Research/` into `docs/research/` with a new folder guide.
- Moved the latest report into `docs/recent/reports/` and kept the reports README as a sub-guide.
- Updated the docs index and repo map to reflect the new `docs/research/` lane.

### Cleanup Notes

- Removed cached `.kain` directories where possible; the cache inside `generated/_ue5_smoke_pokered/.kain` could not be deleted due to access locks.
- `target/` still appears locked by another process and needs a clean sweep when the build pipeline releases it.

## 2026-03-22 - Stale Native App Outputs Were Purged

A cleanup pass removed generated native-app outputs that had leaked into source-controlled lanes, including app and smoketest native UI build products.

Key takeaways:

- `apps/kade-desktop/native-app` and `native-app-preview` are disposable build outputs, not canonical sources.
- UI smoke `native-app` folders are build artifacts and should be cleared after validation runs.
- `target/` and `.kain` caches are still the primary cleanup targets; some directories may be locked during active builds and must be cleared once the processes exit.

Current run recorded at 2026-03-22T06:20:00-04:00.

## 2026-03-22 - Conformance Bin Cleanup Pass

Conformance harness binaries under `runtime/conformance/**/bin` are disposable artifacts and were cleared this run.
Testing lane intermediate build outputs were removed to keep the test tree clean.

Locked outputs remain under:
- `generated/_ue5_smoke_pokered/.kain`
- `smoketest/UI/website_clone_signalcraft/native-app/target`
- `target/` (repo root)

Current run recorded at 2026-03-22T04:19:26-04:00.

## 2026-03-25 - Universal Web Template Pack Landed

A first serious web template lane now exists under `templates/Web/universal`.

What changed:

- `templates/Web` is no longer an empty placeholder; it now has a pack registry, durable docs, and a limitations file that records the real language/runtime gaps instead of hiding them inside one-off workarounds
- `templates/Web/universal` now ships a no-Rust-required starter with:
  - Kain entrypoints for build orchestration, actor-server reporting, and semantic UI preview
  - a shared Node helper runtime for manifest loading, HTML rendering, local serving, and actor-route output
  - manifest registries for themes, content, scenes, and experiences
  - archetypes for business, portfolio, immersive 3D, chat, and actor-server site modes
- `stdlib/javascript` gained `site_runtime.kn` and `site_actor.kn` so future web starters can call the shared helper surface without rewriting the same Node bridge boilerplate

Why this matters:

- it creates the first honest path toward web starters that keep Kain in control without forcing end users to install Cargo or Rust toolchains
- it pushes the web lane toward a data-driven system instead of accumulating one-off HTML generators in smokes or app folders
- it gives future work a stable place to extend client islands, actor routes, browser adapters, and eventual semantic UI web lowering

What is still incomplete:

- this template still uses Node-hosted browser and actor-server runtime glue because Kain UI does not yet have a first-class web backend
- the client interaction layer is still JS-authored and should eventually become Kain-authored or semantic-UI-backed
- there is not yet a first-class `kain init web` command that can select and hydrate these archetypes directly

What future work should preserve:

- keep themes, content, scenes, and experiences registry-driven
- keep browser packaging and actor runtime logic centralized in shared helpers instead of cloning it into new starters
- treat `templates/Web/limitations.md` as the contract for what should move upstream into the language/runtime rather than silently re-implementing workarounds

Next serious move:

- add first-class schema validation and CLI hydration for the web template registries
- evolve the Node helper runtime into a reusable browser adapter surface that future KainScript or semantic UI web lanes can consume
- then replace the current HTML-plus-islands workaround with a real semantic `kain-ui-web` lowering path

