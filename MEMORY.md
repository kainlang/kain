<<<<<<< 3D
# Kain Memory

- The Kain 3D pipeline is a live fleet initiative now, and its steering should stay spec-first.
- The intended build path is native, GPU-aware 3D capability that can grow toward DCC-class tools like ZBrush, Substance Painter, and UE5-style workflows.
- Use Codex CLI through the coding-agent skill for pipeline tasks unless the user asks for another harness.
- If Codex reports a usage-limit error, verify the actual CLI output before assuming any seat-switch workaround.
- The user wants frequent updates while the pipeline is active, especially when branches, specs, or heartbeat behavior change.
- Kaino should keep the heartbeat/operator guidance current in this workspace so future passes stay aligned.
- New Kain 3D pass (2026-04-14): the auto-framing camera in `crates/kain-3D/src/scene.rs` now uses a shape-specific bounds-aware framing direction helper, so wide, tall, and deep scenes bias the orbit offset more intentionally instead of sharing a single diagonal heuristic. Added a regression covering wide/deep framing alongside the existing tall-scene check. Validation still hit the local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` / `-lgcc`), and `cargo fmt --all --check` is still blocked by unrelated trailing whitespace in `crates/ue5-shaders/src/validation.rs`.
- Added a 3D platform uplift in `crates/kain-3D`: primitive libraries now export richer scene metadata (`definition_count`, `definition_ids`, and startup primitive display name) when registered into an authoring scene, which makes the library more self-describing for tooling and runtime composition.
- Added `SceneDescription::composition_summary(...)` plus a shared bounds helper in `crates/kain-3D`, so tooling can ask a scene for counts and framing data in one pass instead of re-deriving it ad hoc.
- Validation was blocked by the local Windows GNU toolchain, not by the change itself. `cargo test -p kain-3d scene::tests::scene_bounds_and_framed_camera_follow_scene_composition -- --nocapture` failed while linking build scripts because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): tightened default scene framing in `crates/kain-3D` so the auto-camera distance now scales with field of view instead of using a fixed radius multiplier. Added a regression test for the new framing helper to prove tighter FOVs push the camera farther back. Validation hit a repo-env Windows GNU linker gap, not a code failure: `cargo test -p kain-3d framed_camera_distance_scales_with_field_of_view` failed while building dependencies because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): scene bounds now include particle emitters, not just meshes/terrain/black holes, so auto-framing keeps volumetric FX inside the camera composition. Added a regression test proving an emitter-only scene still produces bounds and a framed camera pose. Validation was blocked by the same local Windows GNU linker gap, not by the scene logic: `cargo test -p kain-3d scene::tests` failed while building dependencies because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): `SceneCompositionSummary` now has a human-readable `brief_label()`/`Display` form, so 3D tooling and logs can describe a scene's composition without reformatting counts ad hoc. Added a regression assertion that `to_string()` matches the brief label. Validation was again blocked by the local Windows GNU linker gap, not the code change: `cargo test -p kain-3d scene::tests::scene_bounds_and_framed_camera_follow_scene_composition -- --nocapture` failed while building dependencies because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): auto-framed camera placement now scales its framing direction with the scene's horizontal and vertical extents instead of always biasing toward a fixed diagonal offset, and a new regression test covers tall-scene framing so vertical compositions stay above the scene center. This should behave better on wide or asymmetrical 3D compositions while keeping the same bounds-driven camera target. Validation hit the same repo-local Windows GNU linker gap before the test binary could build: `cargo test -p kain-3d scene::tests -- --nocapture` failed because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): `SceneBounds` now exposes a `span()` helper and `SceneCompositionSummary::brief_label()` includes the full XYZ span alongside radius. This makes scene logs and tooling more spatially descriptive without re-deriving extents at each call site. Added a regression assertion that the label includes span text and that `span()` equals `half_extents * 2.0`.
- New Kain 3D pass (2026-04-14): auto-framing now respects per-view instance transform overrides through `SceneDescription::bounds_with_overrides(...)` and `framed_camera_pose_with_overrides(...)`, and the software renderer uses that override-aware camera when no explicit view camera is supplied. Added a regression test proving the frame target follows an overridden material_atrium node. Validation is still blocked locally by the Windows GNU linker gap (`-lgcc_eh` / `-lgcc` missing from `x86_64-w64-mingw32-gcc`).
- New Kain 3D pass (2026-04-14): hardened zero-length vector handling in the 3D math/render path by adding `Vec3::normalized_or(...)` and using it for particle emitter axes, orbit rotation, and basis construction in the CPU and WGPU renderers. This prevents zero-axis scene data from producing brittle normalization behavior and keeps particle/orbit math stable. Added regression tests for zero-axis particle emitters and zero-axis rotation. Validation is still blocked by the repo-local Windows GNU linker gap, and `cargo fmt --all` is currently blocked by unrelated trailing whitespace in `crates/ue5-shaders/src/validation.rs`.
- New Kain 3D pass (2026-04-14): added explicit scene resolution metadata to `SceneCatalog` via `resolve_scene(...)`, so tools can distinguish exact hits, aliases, and default fallbacks instead of treating every lookup as a plain `scene(...)` fetch. The `material_atrium_smoke` report now records requested vs resolved scene names plus the resolution kind, which makes smoke output much more useful for alias/debug triage. Validation is still blocked by the local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`) before the test binary can link.
- New Kain 3D pass (2026-04-14): auto-framed camera poses now compute near/far clip planes from scene bounds, which should reduce clipping in large or shallow compositions while preserving the bounds-driven framing target. Also cleaned up a stray syntax brace in `crates/kain-3D/src/scene.rs` that `rustfmt` surfaced during validation. Validation remains blocked by the same local Windows GNU linker gap, so `cargo test -p kain-3d scene::tests::framed_camera_clip_planes_expand_with_bounds -- --nocapture` could not link because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): `SceneCompositionSummary` now includes an explicit `framed_camera_distance` derived from the scene bounds and camera FOV, and the brief label reports that fit distance alongside bounds. This gives 3D tooling a direct framing cue instead of forcing it to recompute camera fit from the raw summary. Validation on the focused `scene_bounds_and_framed_camera_follow_scene_composition` test is still blocked by the local Windows GNU linker gap (`-lgcc_eh` / `-lgcc`).
- New Kain 3D pass (2026-04-14): the software renderer now forwards scene/tooling metadata through `FrameDiagnostics` (`scene_name`, `viewport_summary`, and a brief `composition_summary`), so hosts can label 3D frames without re-deriving context from pixels. Added a regression assertion that the framed-camera smoke scene reports those fields. Validation was blocked by the same local Windows GNU linker gap, because `cargo test -p kain-3d` could not link build scripts while `x86_64-w64-mingw32-gcc` lacked `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): auto-framing now takes viewport aspect ratio into account in `crates/kain-3D`, and both the software and WGPU renderers pass their actual aspect ratio into the scene camera fit. This should reduce clipping on wide or tall viewports without changing authored scene meaning. Added a regression test that wide viewports demand a farther camera fit than square ones. Validation is pending, but the repo-local Windows GNU linker gap has been the recurring blocker for `cargo test -p kain-3d` on this machine (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` / `-lgcc`).
- New Kain 3D pass (2026-04-14): the `material_atrium_smoke` report now serializes each tile's frame diagnostics (`camera_source`, scene name, viewport summary, composition summary, and visible/culled instance lists), so tooling can inspect the actual framing decision instead of inferring it from screenshots alone. This is a tooling uplift that makes the 3D smoke output more self-describing for future debugging and scene-composition work.
- New Kain 3D pass (2026-04-14): scene composition summaries are now aspect-ratio aware in `crates/kain-3D`, so renderer diagnostics report a framing distance that matches the actual viewport instead of assuming a square view. The software renderer now feeds its real aspect ratio into the summary path, which makes frame metadata and logs more trustworthy for wide native viewports. Added a regression test for the new aspect-aware summary helper. Validation was blocked by the same local Windows GNU linker gap before the test binary could finish linking (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` / `-lgcc`).
- New Kain 3D pass (2026-04-14): `templates/3D/src-kain/stdlib/three_d_runtime/viewport_runtime.kn` now carries explicit `composition_policy` and `framing_policy` fields on `ViewportDescriptor`, with the default profile bound to `scene_summary_driven_and_launch_preset_bound` and `bounds_fov_and_aspect_ratio_fit`. This makes viewport launch contracts line up with the scene-summary/framing work already landing in `crates/kain-3D`, and the template README now calls out the policy explicitly for future authors.
- New Kain 3D pass (2026-04-14): `SceneBounds` now exposes a dominant-axis label, and `SceneCompositionSummary::brief_label()` appends a simple wide/tall/deep cue next to the span, so tooling can read scene proportions faster from logs and frame metadata. This is a small but practical authoring/tooling improvement for 3D composition debugging. Validation hit the same environment blocker as other local runs: `cargo test -p kain-3d scene::tests::scene_bounds_and_framed_camera_follow_scene_composition -- --nocapture` failed during dependency linking because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
# New Kain 3D pass (2026-04-14): `SceneCompositionSummary` now carries the viewport aspect ratio and includes it in `brief_label()`, so frame diagnostics can report the actual render shape alongside bounds and camera fit instead of leaving aspect implicit. Added a regression assertion that the summary label includes `aspect 1.00:1` for the default path. Validation pending.
- New Kain 3D pass (2026-04-14): unified 3D frame diagnostics across software and WGPU renderers by adding a shared `frame_diagnostics_for_scene(...)` helper. Both backends now emit scene name, viewport summary, composition summary, and camera-source metadata in `RenderFrame`, which makes backend comparisons and headless tooling easier. Validation is still blocked by the local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

# MEMORY

# 2026-04-14 - Node FFI Three.js space lab landed under labs

The repo now has a minimal browser-side proof under
`labs/threejs_node_ffi_space_lab/` that shows Kain can orchestrate a Node-owned
Three.js app and serve it on localhost without going through the native-ui lane.

What changed:

- Added `labs/threejs_node_ffi_space_lab/` with a manifest-driven app config,
  scene registry, Node runtime helper, browser client, and Kain entrypoint.
- The lab uses `std::javascript::bridge` from `src/main.kn` to call
  `helpers/space_lab_runtime.mjs`, which bundles the browser client with
  `esbuild`, emits `outputs/index.html`, and serves the generated files over a
  local Node HTTP server.
- The browser client is intentionally small and purpose-built: a giant star
  field, a beacon ring, a floating emissive sphere, and pointer-lock free-fly
  movement so the lane proves real Three.js interactivity instead of a static
  canvas.
- Added lab-local docs plus root-level `labs/README.md` and `ARCHITECTURE.md`
  updates so future agents can find the proof surface quickly.

Validation:

- `npm install` in `labs/threejs_node_ffi_space_lab`
- `npm run build` in `labs/threejs_node_ffi_space_lab`
- `npm run serve` in `labs/threejs_node_ffi_space_lab`
- `cargo run -q -p cli --bin kain -- fabric validate --manifest labs/threejs_node_ffi_space_lab/KAIN.fabric.toml`

Important behavior notes:

- The live localhost proof is validated through the Node/browser lane, not the
  native-ui or `kain-3D` renderer lane. That distinction matters when debugging
  runtime regressions.
- Scene scale, lighting, server port, and movement tuning live in JSON
  manifests. Future tweaks should stay data-driven rather than drifting into
  hardcoded client constants.
- The Kain-facing entrypoints (`src/main.kn` and `KAIN.fabric.toml`) are wired
  in place, but this checkout currently fails Kain execution with unknown
  `js_import` / `js_bridge_import` identifiers before the Node helper runtime
  is reached.

Current risk:

- The proof still depends on local Node package installation in the lab root,
  so a clean checkout needs `npm install` before browser bundling or serving can
  succeed.
- The host-backed Kain JavaScript bridge registration appears to be drifting
  from the checkout's authored examples, which means the lab currently proves
  the Node + Three.js runtime path more strongly than the Kain execution path.

Recommended next step:

- Repair the host-backed Kain JavaScript bridge registration so `src/main.kn`
  and `kain fabric run --manifest labs/threejs_node_ffi_space_lab/KAIN.fabric.toml`
  can execute successfully, then keep the reusable Node-side browser bundling
  and localhost helper path as a template for future web/Three.js labs.

# 2026-04-13 - native-ui dev loop tightened, Chronos native proof added, and TS effect hooks lower into native semantics

The repo now has a real native desktop iteration lane centered on
`kain native-ui dev`, plus a first Chronos-scale proof app that exercises the
same packaged runtime/realtime/shader sidecar path instead of relying on an
imported TS shell.

What changed:

- Added and validated the native desktop dev loop around
  `crates/cli/src/native_ui_dev.rs`. The loop materializes once, launches the
  packaged child, watches the authored app root recursively, ignores generated
  project/artifact trees plus common editor temp files, debounces save bursts,
  and classifies each rebuild as `Noop`, `HotReloadInProcess`, or
  `RestartProcess`.
- Repaired the native-ui reload-coordinator tests so they reflect the live
  executable-path compatibility rule instead of stale assumptions.
- Added the first native Chronos proof under `labs/chronos_native/`, authored
  directly in Kain with compiler-owned `world` state, docked native UI, tabbed
  control panels, `viewport3d`, shader sidecars, and packaged runtime snapshot
  output from one `main.kn`.
- Tightened the TypeScript importer so recognized React effect hooks
  (`useEffect`, `useLayoutEffect`, `useInsertionEffect`) lower into reactive
  component methods instead of surviving as raw hook calls in emitted Kain.
- The importer's degradation/report path is now the truth source for whether a
  generated `.kn` output is honest: parse/compile validation failures are part
  of degradation, and strict mode can fail the import while still writing the
  JSON report.

Validation:

- `cargo test -q -p kain-import test_component_hooks_lower_to_reactive_methods -- --nocapture`
- `cargo test -q -p cli native_ui_dev -- --nocapture`
- `cargo run -q -p cli --bin kain -- build native-ui labs/chronos_native/main.kn --app-name chronos-native-lab --window-title "Chronos Native Lab"`
- `timeout 20 cargo run -q -p cli --bin kain -- native-ui dev labs/chronos_native/main.kn --app-name chronos-native-lab --window-title "Chronos Native Lab"`

Important behavior notes:

- The Chronos native lab proves the packaging/dev loop shape even in this
  environment where the launched child exits through `/usr/local/bin/qmlscene`
  with status `134`. The dev loop itself still materializes, launches, prints
  the executable path, and keeps watching the app root.
- The native-ui packaging/typecheck lane is still stricter than the direct GPU
  artifact lane for at least some compute expressions. The current Chronos
  proof therefore keeps a simplified compute kernel instead of a full
  dispatch-indexed particle step.
- Dependency arrays from imported React effects are still preserved only as
  importer diagnostics, not as a complete reactive scheduler model.

Current risk:

- Native Chronos is now a real proof surface, but the current Qt host/runtime
  environment can still fail after packaging succeeds, which means desktop-loop
  validation remains split between CLI/materialization proof and live GUI-host
  proof.
- The compute authoring seam still needs reconciliation between direct
  `gpu-artifacts` acceptance and `build native-ui` acceptance before this lane
  can claim full descriptor parity for dispatch-indexed simulation code.

Recommended next step:

- Reconcile the native-ui packaging/typecheck lane with the direct GPU artifact
  lane for compute dispatch indexing, then upgrade `labs/chronos_native` from
  the simplified kernel to a real particle-step implementation and revalidate it
  in a GUI-capable environment.

# 2026-04-14 - full parity spec package for KSculpt and KPainter

The repo now has a full spec package under `.specs/ksculpt-kpainter-parity/`
plus steering docs under `.specs/steering/` that define the execution program
for taking Kain to native KSculpt and KPainter parity.

What changed:

- Added a full spec package with `requirements.md`, `design.md`, `tasks.md`,
  `validation.md`, and `decisions.md` for the parity program.
- Added steering for repo-wide standards, git workflow, and DCC native-authoring
  rules so future implementation agents have durable guardrails.
- Locked the parity destination to `apps/kain-fabric-dcc-suite` as the flagship
  native DCC app instead of spreading parity work across multiple equal app
  surfaces.
- Locked the sculpt baseline to `.reference/sculpting/*` and the painter
  baseline to `.reference/graphos/*` plus the current Kain painter scaffolds,
  because the repo does not contain a single dedicated `paint/` reference tree.
- Structured the program around:
  1. native authoring and hot-reload foundation,
  2. shared DCC session, workbench, and asset contracts,
  3. KSculpt parity vertical slices,
  4. KPainter parity vertical slices,
  5. parity harness and importer honesty.

Important behavior notes:

- The spec explicitly rejects TypeScript transliteration as the parity strategy.
  Importers remain migration aids, not the end-state authoring model.
- Risky GPU kernels, topology services, and host/runtime experiments should land
  in `labs/*` first and integrate into the flagship app only after contracts and
  benchmarks are proven.
- Painter parity now has a composite baseline by design, so any future parity
  claim should carry an explicit feature id, source reference, owning subsystem,
  and validation hook.

Current risk:

- The native host launcher decision remains open because the current
  `qmlscene`-backed path is still a real stability constraint.
- Pressure-sensitive input and the initial OS release matrix are not yet locked,
  which means full sculpt and painter parity cannot be claimed until that choice
  is made and validated.

Recommended next step:

- Execute Task 1.1 from the spec first: build the explicit parity capability
  matrix with feature ids, source references, owners, and validation hooks
  before claiming additional sculpt or painter parity features.

# 2026-04-14 - DCC parity matrix and validator landed

The first implementation slice from the KSculpt/KPainter parity spec is now
real: the repo has a machine-readable parity inventory, a validator, and
canonical docs pointing at the same source of truth.

What changed:

- Added `apps/kain-fabric-dcc-suite/config/dcc_parity_matrix.json` as the
  flagship parity registry for shared, sculpt, and painter capability claims.
- Wired the matrix into
  `apps/kain-fabric-dcc-suite/config/app_manifest.json` so the flagship app
  exposes it like the other registries.
- Added `scripts/python/validate_dcc_parity_matrix.py` plus
  `scripts/python/test_validate_dcc_parity_matrix.py` so the matrix has a real
  validation path instead of being a passive document.
- Added `docs/reference/dcc-parity-matrix.md` as the operator-facing guide and
  updated durable architecture/docs pointers so future agents can discover the
  matrix without rereading the full spec package.

Important behavior notes:

- The matrix uses five status levels: `reference_only`, `scaffolded`,
  `in_progress`, `implemented`, and `validated`.
- The painter baseline is explicitly composite:
  `.reference/graphos/*` defines the legacy feature surface, while
  `apps/kain-canvas-forge/*` and `apps/kain-fabric-dcc-suite/*` provide the
  current Kain implementation seams.
- Validation is intentionally structural for now: it checks ids, paths, runtime
  lanes, hooks, and app-manifest wiring. It does not yet execute the parity
  scenarios themselves.

Current risk:

- The matrix is only the first gate. Many validation hooks still point to
  planned scenarios or manual walkthroughs because the scenario harness has not
  been built yet.

Recommended next step:

- Implement Task 1.2 and Task 6.2 together: turn the highest-priority
  `scenario:*` hooks in the matrix into executable parity harness checks,
  starting with the shared native dev loop and the first sculpt and painter
  workflow slices.

# 2026-04-14 - parity harness landed and the session materializers now consume the live matrix schema

The first executable parity-harness layer is now in the repo, and the DCC
session materializers no longer drift from the live parity-matrix schema.

What changed:

- Fixed `apps/kain-fabric-dcc-suite/scripts/materialize_session_state.py` and
  `apps/kain-fabric-dcc-suite/scripts/materialize-session-state.ps1` so parity
  summary data is derived from the current `features[]` and `status` fields
  instead of the stale `capabilities` / `parity_status` schema.
- Added `scripts/python/run_dcc_parity_harness.py` as the executable scenario
  runner for the highest-priority shared, sculpt, and painter hooks in
  `config/dcc_parity_matrix.json`.
- Added `scripts/python/test_run_dcc_parity_harness.py` so two critical drift
  risks are covered:
  1. non-`reference_only` scenario hooks must have registered handlers,
  2. parity summary generation must match the live matrix schema.
- Updated the parity doc and architecture docs so the new harness is discoverable
  alongside the validator.

Important behavior notes:

- The harness defaults to the active implementation set:
  `scaffolded`, `in_progress`, `implemented`, and `validated` features.
- Reference-only hooks are still part of the matrix, but they are excluded from
  the default harness run unless `--include-reference-only` is used.
- In-progress seams such as shared undo/redo and full painter brush symmetry now
  report `pending` explicitly instead of pretending they are either complete or
  absent.

Current risk:

- The harness currently proves the strongest structural seams and the shared
  materializer path, not full artist-interaction playback. Cursor projection,
  symmetry-rich sculpt strokes, and time-based painter behavior still need
  deeper scenario coverage.

Recommended next step:

- Use the harness results to drive Task 1.2 and Task 2.3 next: lock the
  flagship-app ownership rules in docs/config, then harden the native loop and
  DCC state-restore scenarios until the shared layer is no longer the bottleneck.

# 2026-04-14 - machine-readable DCC parity matrix landed for KSculpt and KPainter work

The first implementation slice from the parity spec is now in the repo as a
real app-owned registry instead of only prose.

What changed:

- Added `apps/kain-fabric-dcc-suite/config/dcc_parity_matrix.json` as the
  machine-readable flagship parity inventory for shared workbench, sculpt, and
  painter capabilities.
- Added `scripts/python/validate_dcc_parity_matrix.py` to enforce schema shape,
  unique ids, valid enums, scenario references, and path existence for both
  reference sources and current Kain surfaces.
- Added `docs/reference/dcc-parity-matrix.md` as the operator-facing guide and
  linked it from the docs index and feature matrix.
- Threaded parity summary data into
  `apps/kain-fabric-dcc-suite/scripts/materialize_session_state.py` and the
  PowerShell materializer so runtime snapshots now carry capability-count and
  status-summary metadata derived from the parity matrix.
- Updated `apps/kain-fabric-dcc-suite/scripts/materialize_shell.py` so the
  generated native shell can surface parity summary telemetry from the snapshot.

Important behavior notes:

- The parity matrix is intentionally honest about current state. Most sculpt and
  advanced painter features are still `reference_only` or `scaffolded`, while a
  smaller set of shared and material-lane features are `partial`.
- `apps/kain-fabric-dcc-suite` is now the explicit machine-readable parity
  destination as well as the architectural one; parity claims should update this
  config before they update marketing-style prose.
- Validation scenarios are tracked as ids even when the full scenario harness
  has not been built yet. That keeps future automation and CI work anchored to
  stable identifiers.

Current risk:

- The parity inventory is strong enough to drive implementation, but it is still
  hand-curated. Drift remains possible until a broader scenario harness consumes
  the validation-scenario ids directly.
- The Linux materializer path now surfaces parity telemetry in the generated
  shell, but the PowerShell shell materializer does not yet render dedicated
  parity UI even though the snapshot carries the data.

Recommended next step:

- Execute the next spec task against the new matrix instead of prose:
  promote Task 1.2 and then start Task 2.1 or 2.3 with capability ids pulled
  directly from `dcc_parity_matrix.json`.

# 2026-04-13 - Kain Flight Control MCP server added as a portable repo-native control plane

The repo now has a local MCP sidecar under `tools/kain-flight-control/` so
agents can stop rediscovering Kain lane ownership, validation commands, and
paired metadata rules by hand.

What changed:

- Added a new Go module at `tools/kain-flight-control/` with a stdio MCP server
  entrypoint plus a small Python launcher.
- Added root `mcp.json` and `codex.config.toml` templates that launch the
  server through `KAIN_REPO_ROOT` instead of a checkout-specific absolute path.
- Added `tools/kain-flight-control/config/server.toml` as the machine-readable
  registry for workspace/env settings, canonical source files, allowlisted
  validation commands, artifact parsers, lane routing rules, and paired-surface
  consistency checks.
- Implemented the deterministic MCP tool surface: `resolve_lane`,
  `context_pack`, `plan_validation`, `run_validation`, `inspect_artifact`,
  `triage_failure`, and `check_pairing`.
- Seeded the first runtime and selfhost lanes from existing repo truth such as
  `runtime/native_runtime_metadata.json`,
  `runtime/changelogs/NATIVE_RUNTIME_VALIDATION.md`,
  `ouroboros/docs/selfhost/metadata/selfhost_source_profile.json`,
  `ouroboros/docs/selfhost/inventories/module_map.json`, and
  `crates/flowgraph.json`.
- Added tests covering config loading, repo-root resolution, lane planning,
  allowlist enforcement, artifact parsing, pairing drift detection, triage
  classification, and MCP `initialize` / `tools/list` / `tools/call`.

Validation:

- `cd tools/kain-flight-control && go test ./...`
- `cd tools/kain-flight-control && python3 -m py_compile launcher.py`

Important behavior notes:

- The server is intentionally a controlled-exec MCP. It does not expose a
  generic shell tool; every runnable command must be declared in
  `config/server.toml`.
- The launcher prefers a built binary under `tools/kain-flight-control/bin/`
  and falls back to `go run` for first-run convenience.
- Pairing drift for the native runtime is now machine-checkable through
  `runtime/native_runtime.toml` and `runtime/native_runtime_metadata.json`.
- The current validation cache is command-keyed and repo-state-aware, but still
  intentionally conservative; the main speedup in v1 comes from lane routing
  and smaller allowlisted check selection, not from a fully global artifact
  cache.

Recommended next step:

- Extend `server.toml` with more Kain-native lanes such as Fabric, UI/native,
  importer repair, and website/public-site validation before adding more code
  paths to the server itself.

# 2026-04-13 - docs/examples replaced with a runnable manifest-driven Kain source suite

The old prose-only `docs/examples` pages were replaced with a real example
ladder made of brand-new `.kn` files, a machine-readable manifest, and a single
validator script so future agents have one honest place to learn current Kain
authoring.

What changed:

- Replaced the deleted markdown-only example pages under `docs/examples/` with
  twelve real Kain source files:
  - `00_hello_and_cli.kn`
  - `01_types_structs_enums_patterns.kn`
  - `02_modules_traits_impls_and_comptime.kn`
  - `03_collections_strings_filesystem.kn`
  - `04_async_actors_and_gen_server.kn`
  - `05_components_ui_and_theme.kn`
  - `06_shader_compute_and_gpu_artifacts.kn`
  - `07_low_level_memory_and_layout.kn`
  - `08_world_patch_law_converge_and_local_orchestrate.kn`
  - `09_ue5_authoring_gallery.kn`
  - `10_polyglot_bridge_pipeline.kn`
  - `11_ultimate_kain_pipeline.kn`
- Added `docs/examples/examples_manifest.json` as the data-driven registry for
  coverage tags, next-example links, local validation classes, and canonical
  commands.
- Added `docs/examples/validate_examples.py` as the single validation entrypoint
  for the suite.
- Added `docs/examples/README.md` as the human-facing index and updated the
  surrounding docs pointers in `docs/README.md`, `docs/quickstart.md`,
  `docs/reference/legacy-crosswalk.md`, and `docs/kn_library/README.md`.
- Updated `ARCHITECTURE.md` so future agents know `docs/examples/` is now the
  one intentional exception inside the otherwise legacy `docs/` tree.

Validation:

- `python3 docs/examples/validate_examples.py --kain ./target/debug/kain`
- The full suite passed locally, including:
  - `run` lanes for interpreter-backed examples
  - `build -t rust/js/ts` on the text-backend examples
  - `build -t hlsl` plus `gpu-artifacts` on the shader example

Important behavior notes:

- The validator should prefer `./target/debug/kain` over a potentially stale
  PATH launcher; this repo has already shown launcher drift in practice.
- `09_ue5_authoring_gallery.kn` is intentionally validated through the Rust
  backend only. The current checkout still fails on direct `-t ue5` loads
  because `stdlib/ue5` does not resolve `max` correctly.
- `@target_actor` and `@ability_task` remain known work-in-progress surfaces for
  the general typechecker, so they are documented as local limits rather than
  being smuggled into the “validated” suite.
- `10_polyglot_bridge_pipeline.kn` is build-first by design: foreign
  `orchestrate` stages compile into Rust/JS/TS targets, but they are not run by
  the local interpreter.

Recommended next step:

- If the UE5 stdlib load bug is repaired, extend the validator and manifest so
  `09_ue5_authoring_gallery.kn` can grow a real `-t ue5` proof lane instead of
  stopping at the Rust backend.

# 2026-04-12 - tiny GenServer-style stdlib layer added on top of actors

The repo now has a small KAIN-authored actor-service helper under
`stdlib/gen_server.kn` plus a native `ask` primitive in the interpreter runtime,
so raw `spawn` / `send` actors can be wrapped in a cleaner request/reply shape.

What changed:

- Added `stdlib/gen_server.kn` with `gen_server_start`, `gen_server_start_link`,
  `gen_server_call`, `gen_server_cast`, `gen_server_info`, and
  `gen_server_call_result`.
- Added native runtime support for `ask(actor, message, request)` in
  `crates/kain-core/src/runtime.rs`; it sends a one-shot reply actor ref and
  waits up to 30 seconds for the first reply.
- Registered `ask` in the typechecker and stdlib metadata via
  `crates/kain-core/src/types.rs` and `crates/kain-core/src/stdlib.rs`.
- Added runtime regression coverage in
  `crates/kain-core/src/runtime_tests.rs` for a full call/cast/info round trip.
- Updated `stdlib/README.md` and `ARCHITECTURE.md` so the helper and its current
  limits are discoverable without reading the test.

Behavior notes:

- `gen_server_start_link` is a naming alias only right now. It does not create
  a runtime link or supervision relationship yet.
- The current `ask` primitive returns the first reply message payload:
  zero args becomes `Unit`, one arg returns the value directly, and multiple
  args return a tuple.
- KAIN still has two important syntax/runtime quirks around this lane:
  struct construction should use `TypeName { field: value }` instead of
  `TypeName(field = value)`, and closure-valued actor state must be loaded into
  a local before calling it.

Validation:

- `cargo test -p kain-core runtime_tests::gen_server_stdlib_round_trip -- --nocapture`
- `cargo test -p kain-core runtime_tests::stdlib_registry_exposes_ord_and_chr -- --nocapture`
- `cargo test -p kain-core test_stdlib_builtin_functions_exist -- --nocapture`

Current risk:

- The helper is intentionally tiny and interpreter-focused. It does not yet
  expose true links, monitors, selective receive, or typed mailbox protocols.

Recommended next step:

- Decide whether the next actor-ergonomics step is real link/monitor semantics
  in the runtime, or a slightly richer stdlib layer with reply tuples, timeout
  control, and optional name registration.

# 2026-04-12 - KAIN filesystem utility lane made real under scripts/kain

The repo now has a working KAIN-authored filesystem automation lane under
`scripts/kain/` that uses actual runtime builtins instead of placeholder helper
names.

What changed:

- Added `scripts/kain/append_text_to_file.kn` for appending text to a file and
  creating parent directories when needed.
- Added `scripts/kain/organize_by_extension.kn` for dry-run or apply-mode file
  organization by extension.
- Added `scripts/kain/README.md` as the usage guide and environment-variable
  contract for the new lane.
- Extended `crates/kain-core` runtime/type metadata with filesystem helpers for
  `read_dir`, `create_dir_all`, `copy_file`, `remove_file`, `path_join`,
  `path_parent`, `path_file_name`, `path_extension`, `path_stem`,
  `path_is_file`, and `path_is_dir`.
- Exposed `write_file`, `file_exists`, and `env` through the typechecker and
  stdlib metadata so KAIN scripts can actually call them.

Behavior notes:

- `env(name)` now returns an empty string when the variable is missing, which
  keeps the scripts simple and string-based.
- `read_file` and `write_file` now surface I/O failures directly as runtime
  errors instead of wrapping them in result values.
- The KAIN control-flow used by these scripts is statement-oriented, so the
  scripts use explicit branching and returns rather than Rust-style expression
  blocks.

Validation:

- `scripts/kain/append_text_to_file.kn` passes a temp-dir append smoke test and
  writes the expected newline-separated output.
- `scripts/kain/organize_by_extension.kn` passes a temp-dir apply-mode smoke
  test and moves files into `txt/`, `md/`, and `no_extension/` buckets.
- `cargo test -p kain-core --lib` still has unrelated pre-existing failures in
  other areas of the workspace, but the filesystem builtin additions compile.

Current risk:

- The organizer is intentionally non-recursive and collision-avoidant; it skips
  a file if the destination name already exists in the target bucket.

Recommended next step:

- Add more focused KAIN utilities only when they solve a real repetitive task
  and keep the env-var contract documented in the folder README.

# 2026-04-12 - actor demos added under scripts/kain/actor

The KAIN script lane now includes two actor-oriented demos that exercise the
real `spawn` / `send` surface instead of just filesystem helpers.

What changed:

- Added `scripts/kain/actor/folder_job_runner.kn`, a coordinator/worker demo
  that scans direct child text files, spawns one worker actor per job, and
  retries once when `KAIN_JOB_RETRY_TOKEN` matches the file name.
- Added `scripts/kain/actor/file_indexer.kn`, a coordinator/bucket demo that
  spawns one actor per extension bucket and routes each direct child file to
  the matching bucket actor.
- Added `scripts/kain/actor/README.md` and extended `scripts/kain/README.md`
  so the actor lane is discoverable from the main scripts index.
- Updated `ARCHITECTURE.md` so the durable repo overview now calls out the
  actor demo subtree explicitly.

Design notes:

- The demos keep the message syntax aligned with the parser's actual
  `send actor.Message(field: value)` form.
- The bucket demo uses arrays of actor refs rather than a custom map registry
  because that stays simple, explicit, and easy for future agents to follow.
- The job runner only touches text-like direct child files so the retry demo is
  safe to run against the repo root without immediately tripping over binary
  inputs.
- The current executable surface does not expose `sleep` to these scripts, so
  both demos use a short busy-loop flush helper to keep the process alive long
  enough for background actor threads to print.
- Actor handler state access needs `self.` in this repo's checker, so the demos
  were rewritten to reference state fields explicitly instead of relying on bare
  names.
- String predicates in this lane are easiest to keep stable by using
  `path_extension(...).trim().to_ascii_lowercase()` plus equality checks
  instead of depending on missing string helper forms.

Current risk:

- The actor runtime is real, but the broader language still has enough surface
  area mismatch that actor-heavy scripts need smoke testing after edits.

Validation:

- `scripts/kain/actor/file_indexer.kn` ran successfully on a temp folder with
  `README.md`, `notes.txt`, and `project.toml`, spawning one bucket actor per
  extension and printing a clean summary.
- `scripts/kain/actor/folder_job_runner.kn` ran successfully on the same temp
  folder with `KAIN_JOB_RETRY_TOKEN=README.md`, retrying that file once and
  reporting the final completion summary.

Recommended next step:

- Smoke-test both actor demos on the local interpreter, then keep future actor
  utilities in `scripts/kain/actor/` so the lane stays organized.

# 2026-04-12 - scripts tree reorganized into directory-only root

The `scripts/` directory was flattened into purpose-based subtrees so the root
no longer contains files.

What changed:

- Moved human-facing docs into `scripts/docs/`.
- Moved Bash entrypoints into `scripts/linux/`.
- Moved Windows wrappers into `scripts/windows/`.
- Kept Python utilities in `scripts/python/` and moved the remaining root
  Python helpers there.
- Moved the Rust build helper into `scripts/rust/` and the UE5 sample input
  into `scripts/tests/`.

Why:

- The root `scripts/` directory now stays directory-only, which makes the tree
  easier for agents to scan and prevents loose root-level helpers from
  accumulating.

Validation:

- Verified `scripts/` root contains only subdirectories.
- Updated repo-wide references in docs, hooks, architecture notes, and CLI
  messages to the new paths.

Current risk:

- A few historical notes outside the active tree still mention the old flat
  `scripts/...` layout. Those are archival and do not affect the current
  checkout, but they may be confusing if read out of context.
- The round-trip demo script still uses a legacy plugin target name. The path
  move is complete, but that demo input may need a future refresh if it is meant
  to be runnable end-to-end.

Recommended next step:

- Keep future script additions inside the new subtrees and update the docs index
  if a new category appears.

# 2026-04-12 - extern crate lowering now shares the external module diagnostic lane

The Rust importer previously surfaced `syn::Item::ExternCrate` under a one-off `extern_crate_decl` class, which the selfhost allowlist did not recognize. That class now folds into `external_mod_decl`, matching the existing `allow_external_mod_decls` path and letting `cli` import cleanly during phase2.

What changed:

- `crates/kain-import/src/rust/transformer.rs` now emits `class:external_mod_decl` for `extern crate ...`.
- The mirrored Kain source under `src/rust-import/kain-import/rust/transformer.kn` was updated to stay in sync.
- Added a regression test covering `extern crate ue5;` classification.

Validation:

- `cargo test -p kain-import external_mod_decl -- --nocapture`
- `cargo test -p kain-import selfhost::tests::keeps_external_mod_decls_compatibility_flag -- --nocapture`
- `cargo run -q -p cli --bin kain -- selfhost phase2 --inventory-dir ouroboros/docs/selfhost/inventories --output-dir /tmp/kain_selfhost_importfix --emit-roundtrip-rust false --assemble-stage2 false --build-stage2 false --force`

Current risk:

- The broader selfhost lane is still bridge-first, but this specific import blocker is cleared. The next failure, if any, will be a different crate or a later stage2 assembly/build issue.

Recommended next step:

- Continue phase2 against the same inventory profile and chase the new front blocker after `cli`.

# 2026-04-12 - native runtime cache moved to a repo-local host cache and now prebuilds vendor archives

The manifest-driven native runtime no longer ties object reuse to the current
LLVM/native executable output directory. The CLI now stages runtime build
artifacts under `generated/native_runtime/cache/<host>/<runtime-name>/`, keeps
repo-owned runtime sources as loose cached objects, and prebuilds a cached
static archive for the heavy vendored `3rdparty/` source family.

What changed:

- Added `[[archive_groups]]` support to `runtime/native_runtime.toml` and
  mirrored that build metadata into `runtime/native_runtime_metadata.json`.
- Changed `crates/cli/src/main.rs` so the LLVM/native build path writes runtime
  objects into a stable repo-local cache root instead of `<output>/.kain-runtime/`.
- Added portable archiver detection with `KAIN_AR_PATH` override support and
  built the current `3rdparty/` vendor surface into a cached `vendor-runtime`
  static archive.
- Updated `runtime/compile_native_runtime.sh` to reuse the same repo-local
  cache root and to emit/reuse the vendor archive during compile-only runtime
  validation runs.
- Updated runtime/architecture docs to explain the new cache root, archive
  group contract, and `KAIN_RUNTIME_CACHE_DIR` / `KAIN_AR_PATH` overrides.

Validation:

- `cargo test -p cli runtime_`
- `bash -n runtime/compile_native_runtime.sh`

Current risk:

- The CLI path tracks header dependencies with depfiles, but
  `runtime/compile_native_runtime.sh` still uses source-plus-fingerprint reuse
  rather than the full depfile-aware freshness check. That helper script should
  eventually share the stricter cache logic too.
- The current runtime manifest uses one big `vendor-runtime` archive. That is
  the safest cross-platform first cut, but future work may still want smaller
  archive partitions if link-time measurements justify them.

Recommended next step:

- Measure cold build, warm build, and relink-only times for `kain build -t llvm`
  after the new cache lands, then decide whether the next win is a slimmer
  runtime profile or finer-grained archive partitioning.

# 2026-04-12 - selfhost now treats the kain executable as the first bootstrap correctness gate

The active selfhost profile stays on the Rust bridge path, but phase2 now front-loads `cli` ahead of `kain-sys-codegen` so the `kain` executable is the first thing we prove correct on this pass. That keeps the import bridge intact while making executable parity the gating milestone instead of a later byproduct.

# 2026-04-12 - src ownership root is now split into owned core plus reference lanes

The `src/` tree no longer mixes hand-owned Kain work with donor corpus and live phase2 mirrors at the top level. `src/core` is now the only active owned source lane, `src/.rustimport/reference` holds the moved donor corpus from the older Rust import lane, and `src/.rustimport/phase2` is the canonical live selfhost mirror root.

What changed:

- Moved the old donor corpus from `src/rust-import` into `src/.rustimport/reference`.
- Moved the live phase2 mirror trees from top-level `src/cli`, `src/kain-core`, `src/kain-import`, and `src/kain-sys-codegen` into `src/.rustimport/phase2/...`.
- Changed the default selfhost profile root in `ouroboros/docs/selfhost/metadata/selfhost_source_profile.json` and `crates/cli/src/selfhost_profile.rs` so future phase2 runs emit canonical mirrors into `src/.rustimport/phase2`.
- Updated the selfhost path-based tests in `crates/cli/src/selfhost.rs` to assert the new canonical mirror layout.
- Updated `src/README.md`, `src/TASK.md`, `src/core/README.md`, `src/core/language_features.kn`, `ARCHITECTURE.md`, and the live docs under `docs/` so operators see the owned/reference split instead of the old flat `src/` mirror story.
- Added `src/.rustimport/README.md` as a no-edit ownership note for the reference lanes.

Validation:

- `cargo test -p cli default_profile_resolves_relative_roots`
- `cargo test -p cli build_file_mirror_plans_preserves_file_level_paths`
- `cargo test -p cli write_roundtrip_rust_tree_splits_inline_modules_into_real_files`
- `cargo run -q -p cli --bin kain -- selfhost phase2 --output-dir ouroboros/out/selfhost/src_root_normalization_probe_2026-04-12 --emit-roundtrip-rust false --assemble-stage2 false --build-stage2 false --force`

Current risk:

- Git now sees the moved reference trees as delete-plus-add until they are staged. That is expected for this restructure, but it means later reviews should treat `src/.rustimport/*` as path moves rather than semantic rewrites.
- Historical notes in `MEMORY.md` and other archival prose may still mention `src/rust-import/...`. Those references are historical context, not the live path contract.

Recommended next step:

- Stage the `src/` move as a pure ownership/layout change first, then keep future selfhost work pointed at `src/core` for owned code and `src/.rustimport/phase2` for mirror output.

What changed:

- Updated `crates/cli/src/selfhost_profile.rs` and `ouroboros/docs/selfhost/metadata/selfhost_source_profile.json` to describe the profile as executable-first and to order phase2 as `kain-core`, `kain-import`, `cli`, `kain-sys-codegen`.
- Synced `ouroboros/docs/selfhost/inventories/module_map.json` and `guides/cli/selfhost-omni-fabric-lsp.md` so the live inventory and command docs match the profile.
- Updated `ARCHITECTURE.md` so the durable repo overview now states that the `kain` executable is the current bootstrap priority for the selfhost lane.

Validation:

- `cargo test -p cli --lib default_profile_maps_phase_slices -- --nocapture`
- `cargo test -p cli --lib default_profile_resolves_relative_roots -- --nocapture`

Current risk:

- This is still a bridge-first bootstrap path; only the sequencing changed. The remaining work is to keep driving the `kain` binary until the phase2 frontend and stage2 build are actually green.

Recommended next step:

- Continue with the executable-parity lane, then use the same profile to chase the next lowering or build blocker.

# 2026-04-12 - official Kain website rebuilt as a data-driven public site

The official KAIN website at `/home/ephemara/Dev/Kain/website` was rebuilt as a
data-driven public-site shell instead of the old monolithic inline page. The
site now uses structured content plus a thin browser adapter, and it mounts a
real Kain-compiled preview component in the hero.

What changed:

- Replaced the old inline page with a minimal shell in `index.html` and a
  shared renderer in `site.js`.
- Moved all page copy, capability cards, resources, and playground examples
  into `site-data.js` so the surface is content-driven instead of hardcoded.
- Reworked `style.css` into the full visual system for the site.
- Added a Kain-authored preview component in `src/main.kn` and regenerated
  `dist/main.js` from it.
- Limited the live browser playground to targets that actually compile in the
  current browser/compiler path.
- Fixed `crates/kain-core/src/types.rs` so component props and component-local
  state are visible in render/typecheck scope.

Validation:

- Node import/syntax check for `website/site.js` with a stubbed DOM
- local static serving check for `website/`
- compile probes for the playground examples across the verified browser
  targets

Current risk:

- `site.js` depends on the generated browser compiler package and the generated
  `dist/main.js` preview artifact staying in sync.
- The playground should keep its target list aligned with the examples that are
  actually proven in-browser, not with aspirational backend support.

Recommended next step:

- Keep the website data model stable and expand content from repo truth instead
  of reintroducing a giant inline HTML/JS monolith.

# 2026-04-12 - deep docs pass added pipeline, UE5, and low-level memory chapters

The canonical guide tree now includes dedicated conceptual pages for the
remaining gaps in the language and tooling story:

- `guides/syntax-and-semantics/functions-traits-and-impls.md` for function
  signatures, traits, and impl blocks
- `guides/syntax-and-semantics/low-level-memory.md` for pointer provenance,
  raw/imported pointers, layout-aware lowering, and the helper ABI seam
- `guides/runtime/compiler-owned-intents.md` for `patch`, `law`, `converge`,
  `world`, and `orchestrate`
- `guides/pipelines/omni.md` and `guides/pipelines/fabric.md` for the
  orchestration models behind the corresponding CLI commands
- `guides/ue5/overview.md` for UE5 project layout, module inference, Oracle
  validation, and generated plugin outputs

What changed:

- Added dedicated conceptual chapters instead of leaving these topics buried in
  the broader CLI or runtime pages.
- Cross-linked the new pages from the reader map, glossary, and legacy
  crosswalk.
- Updated the durable architecture notes so future agents know the guides now
  have explicit pipeline and UE5 conceptual homes.

Current risk:

- The new pages are source-driven, but the repo still contains older docs that
  use legacy terminology. Future docs work should keep using the live code and
  the new guide tree as truth.

Recommended next step:

- Keep the command pages, conceptual pages, and crosswalk aligned when the CLI
  or UE5 pipeline changes again.

# 2026-04-12 - top-level docs received a deeper reader-path pass

The guide tree now has a clearer entrypoint story: `guides/README.md` names the reading order, `guides/reference/legacy-crosswalk.md` bridges old prose to current canonical pages, `guides/quickstart.md` reads like a first-run path, and the example pages are framed as workflow or proof surfaces instead of directory summaries.

What changed:

- Added a legacy crosswalk page under `guides/reference/` to map older topics and terms into the current guide tree.
- Strengthened the top-level guide map so readers can move from quickstart to the crosswalk to the deeper guide families without guessing.
- Reframed `guides/examples/smoketest.md`, `guides/examples/apps.md`, `guides/examples/unreal-plugins.md`, and `guides/examples/kn-library.md` around their actual role in the repo.
- Added docs-system guidance to `ARCHITECTURE.md` so future agents know `guides/` is canonical and `docs/` is audit-only.

Current risk:

- The deeper language/runtime/CLI pages are still being expanded in parallel, so top-level docs should keep pointing at them instead of trying to duplicate their content.

Recommended next step:

- Keep the reader order and crosswalk aligned with whatever final shape the deeper guide pages take, and update the crosswalk whenever legacy language or old README topics reappear in the repo.

# 2026-04-12 - new LLVM dogfood lab added under labs/

The repo now has a dedicated LLVM dogfood application at
`labs/llvm_world_dogfood_lab/`. It is meant to be the go-to repo-local proof
for the current LLVM pipeline shape, especially after the canonical native
actor ABI alignment work.

What changed:

- Added a new lab README, bash build/run wrappers, and a single-source
  `src/main.kn` entrypoint.
- The lab combines `world`, `patch`, `converge`, `orchestrate`, canonical
  actor spawn/send, and native UI + viewport rendering in one authored app.
- The source keeps to LLVM-safe shapes that are already proven elsewhere in the
  repo: named actor payloads, compiler-owned world patches, array helpers,
  loops, and JSX expressions.

Current risk:

- The lab is intentionally dense. If it fails, the most likely issues are
  source-shape mismatches in actor payload names or a backend regression in one
  of the compiler-owned intent lanes rather than the lab packaging itself.

Recommended next step:

- Build `labs/llvm_world_dogfood_lab` with the local CLI, keep the source in
  sync with any backend fixes, and use it as the default LLVM dogfood app when
  validating future runtime or codegen changes.

## 2026-04-12 - canonical long-form guides moved to guides/

The repo now has a canonical long-form documentation tree under `guides/`.
The goal is to keep the code-driven language, runtime, CLI, and example
explanations in one source-driven place while treating the older `docs/` tree
as legacy support material.

What changed:

- Added `guides/README.md` as the entry point for the new guide set.
- Added layered language, runtime, native ABI, CLI, crate, example, and
  reference pages under `guides/`.
- Cross-linked the root README, crate index, example READMEs, runtime README,
  and project architecture notes to the new guide tree.

Current risk:

- Some older `docs/` material still exists and may disagree with the live code.
  Future docs updates should continue to treat the code and the `guides/` tree
  as the source of truth.

Recommended next step:

- Keep expanding the guide tree from the code, not from the stale `docs/`
  folder, and add missing examples or command notes only when they map back to
  the current source.

## 2026-04-12 - runtime vendor tree renamed to runtime/3rdparty

The canonical vendored runtime checkout moved from `runtime/thirdparty` to
`runtime/3rdparty`. Repo-owned manifests, metadata, docs, and the piano lab
build script now reference the new path.

What changed:

- Renamed the root vendor tree to `runtime/3rdparty`.
- Updated `runtime/native_runtime.toml` and
  `runtime/native_runtime_metadata.json` to use `3rdparty/` source prefixes.
- Rewrote runtime architecture/historical docs and
  `labs/playground/piano/build.sh` to point at `runtime/3rdparty`.
- Verified that repo-owned references to `runtime/thirdparty` are gone outside
  the moved tree.

Current risk:

- Upstream vendor subtrees still contain their own internal `thirdparty` and
  `3rdparty` directory names and comments, which are unrelated to the repo-root
  rename.

Recommended next step:

- Keep new runtime path references on `runtime/3rdparty` so the old root path
  does not creep back into manifests or docs.

## 2026-04-12 - LLVM actor lowering now targets the canonical native actor ABI

The LLVM backend in `crates/kain-sys-codegen` now lowers actor programs against `runtime/native/include/kain_runtime_actor.h` instead of the old `KAIN_spawn` / `mq_*` path. The emitted IR now carries explicit `%KainActorMessage` and `%KainActorSpawnConfig` types, and actor entrypoints use the canonical `(actor_id, mailbox, user_data)` signature.

What changed:

- Replaced legacy actor spawn/message lowering with `kain_actor_spawn_config_init`, `kain_actor_spawn`, `kain_actor_send`, and `kain_actor_receive`.
- Mapped the native runtime actor ABI types into LLVM so the backend stays aligned with the C runtime headers.
- Treated compiler-owned actor state as borrowed runtime state during actor cleanup, and freed received message payload buffers explicitly after dispatch.

Validation:

- Rebuilt the CLI with `cargo build -p cli` so the `target/debug/kain` binary reflects the new backend before running fixture generation.
- Regenerated the LLVM actor fixture output from `runtime/fixtures/llvm_actor_message/main.kn` and confirmed the new IR shape.

Current risk:

- `kain build ... -t llvm` still has a heavyweight native link phase after LLVM emission. The `.ll` artifact is now aligned, but end-to-end fixture validation still depends on the full runtime link finishing successfully.

Recommended next step:

- Keep `runtime/native/include/kain_runtime_actor.h`, `crates/kain-sys-codegen`, and the LLVM fixture expectations in lockstep whenever actor ownership or ABI layout changes again.

## 2026-04-12 - native runtime vendor lanes now promote from probe results

The native runtime now treats several vendor-backed lanes as bridge-first runtime capabilities instead of permanent staged placeholders. The service registry refreshes availability from each vendor function table's `probe()` result, so a lane can move from manifest truth to active runtime truth when the external runtime or binary is actually present.

What changed:

- Updated the native vendor catalog to use bridge-branded runtime identities for the activated lanes so renderer and service diagnostics show the active bridge name instead of the old staged wording.
- Extended the WAMR probe to accept either an explicit runtime path or a PATH-resolved `iwasm` / `wamr` binary.
- Synced `runtime/native_runtime.toml` and `runtime/native_runtime_metadata.json` so `gfx.compute` is available in the manifest mirror and the bridge-backed runtime set is described consistently.
- Reworded `ARCHITECTURE.md` so the active graphics/UI vendor lanes are documented as probe-backed capabilities, not future stubs.

Current risks:

- `gfx.shader` and `gfx.material` are still future-facing contract entries. They should remain planned until the native service wiring actually exists.
- Bridge-backed lanes still depend on the corresponding runtime or binary being present on the host, so the probe layer is the activation gate.
- The baseline renderer should stay `bgfx` unless the default-selection logic is intentionally rewritten; the bridge-backed renderers are not meant to become the accidental default.

Recommended next step:

- Run the native compile/validation path and then trim any stale `staged` wording that still survives in non-authoritative docs.

## 2026-04-12 - LLVM/native builds compile the full runtime bundle and now reuse runtime objects incrementally

The current `kain build ... -t llvm` lane does more than emit LLVM IR. After
writing the `.ll` file and native sidecars, the CLI resolves
`runtime/native_runtime.toml`, compiles every listed runtime source into object
files under `<output>/.kain-runtime/<runtime-name>/`, and then links those
objects with the generated program.

What this means in practice:

- The Rust bootstrap compiler binary itself is still just a Rust/Cargo artifact.
- LLVM/native outputs are the lane that pulls in the native runtime bundle.
- The current runtime bundle is very large and includes core runtime C plus
  third-party stacks like bgfx, bimg, yoga, libuv, QuickJS, miniaudio, wasm3,
  mimalloc, and rpmalloc.
- The CLI now reuses per-source runtime objects under
  `<output>/.kain-runtime/<runtime-name>/` when the object file, depfile, and
  compile fingerprint are present and all source/header dependencies are older
  than the cached object.

What changed in this pass:

- Added the missing `rc_retain`, `rc_weak_retain`, `rc_release`, and
  `rc_weak_release` declarations to
  `runtime/native/include/kain_runtime_base.h` so actor runtime sources can
  compile under modern C rules.
- Reworked `src/core/kainc.kn` again to avoid `impl self` lowering in the LLVM
  seed shell after the backend produced duplicate local names like `self.addr`.
- Added depfile-driven native runtime object caching in
  `crates/cli/src/main.rs`. The runtime bundle compiler now emits one depfile
  and one compile-fingerprint file per object and skips recompilation for fresh
  objects on repeat LLVM/native builds.
- Removed the no-op `String` reporting helpers from `src/core/kainc.kn` after
  the first native executable segfaulted during startup cleanup through
  `rc_release`. The current seed shell now avoids that runtime seam entirely.

Validation:

- `target/debug/kain build src/core/kainc.kn -t llvm -o /tmp/kainc_native_full/kainc`
  - emitted `/tmp/kainc_native_full/kainc.ll`
  - emitted `/tmp/kainc_native_full/kainc.runtime_contract.json`
  - emitted `/tmp/kainc_native_full/kainc.realtime_app.json`
  - then entered the full native runtime compile path from
    `runtime/native_runtime.toml`
- Earlier failure on undeclared `rc_*` calls in `kain_runtime_actor.c` was
  removed by the header fix.
- The next LLVM/codegen seam hit before the shell rewrite was a duplicate local
  name from method lowering (`self.addr`); the current `kainc.kn` no longer
  uses that shape.
- `cargo test -p cli --bin kain native_runtime_ -- --nocapture`
- `cargo test -p cli --bin kain runtime_source_cpp_detection_matches_known_extensions -- --nocapture`
- `cargo build -p cli --bin kain`
- `target/debug/kain build src/core/kainc.kn -t llvm -o /tmp/kainc_native_cache_probe_v2/kainc`
  - cold pass: `Native runtime object cache: 0 reused, 189 compiled`
  - warm pass: `Native runtime object cache: 189 reused, 0 compiled`
  - emitted runnable native executable:
    `/tmp/kainc_native_cache_probe_v2/kainc`
- `bash -lc '/tmp/kainc_native_cache_probe_v2/kainc; status=$?; echo EXIT:$status'`
  - final executable exits with `EXIT:0`
- `gdb -batch -ex run -ex bt --args /tmp/kainc_native_cache_probe_v2/kainc`
  - earlier crash root was `kain_print_error()` releasing invalid memory via
    `rc_release` / `rpmalloc`; removing the string-reporting helpers fixed the
    startup path

Current risk:

- The native runtime build is still extremely heavy for tiny shell programs
  because the CLI still links the full runtime manifest even though it now
  reuses compiled objects on repeat builds.

Recommended next step:

- Introduce a much smaller `kainc`/compiler-shell runtime profile so tiny shell
  builds stop linking the whole engine/runtime stack after the object cache.

## 2026-04-12 - `src/core/kainc.kn` now clears LLVM emission as a backend-safe seed shell

The owned `src/core/kainc.kn` shell now emits LLVM IR successfully. The shell is
still intentionally minimal, but it no longer trips the backend on frontend-safe
seed constructs like `println`, field access through the current LLVM lowering,
or helper functions that return `String`.

What changed:

- Reworked `src/core/kainc.kn` into a stricter LLVM-safe seed shell.
- `kain_print_phase` and `kain_print_error` are now no-op reporting stubs instead
  of `println` wrappers.
- Removed source-file IO and config field reads from the live `run()` path.
- Dropped helper functions that returned `String`, because the current LLVM
  backend still rejects those return forms in this seed shell.
- Changed `main` to `fn main() -> Int` and return explicit process codes.

Validation:

- `target/debug/kain build src/core/kainc.kn -t llvm -o /tmp/kainc_llvm_probe/kainc`
  - emitted `/tmp/kainc_llvm_probe/kainc.ll`
  - emitted `/tmp/kainc_llvm_probe/kainc.runtime_contract.json`
  - emitted `/tmp/kainc_llvm_probe/kainc.realtime_app.json`
  - native runtime linking then began compiling the bundled C/C++ runtime tree
- `target/debug/kain build src/core/kainc.kn -t rust -o /tmp/kainc_rust_probe/kainc.rs`

Current risk:

- LLVM emission is green for the seed shell, but a full native executable still
  depends on the heavyweight native runtime compile/link finishing successfully.
- The current `kainc.kn` shell is backend-safe because it avoids host print/IO
  and field-access patterns that the LLVM lane still lowers poorly. Those are
  backend limitations to fix later, not the final desired shell behavior.

## 2026-04-12 - `src/core` now has a compile-safe seed floor across every owned module

The first owned `src/core` wave now passes an individual-file frontend/codegen
sanity sweep for every current `.kn` file in the folder. This is not a claim
that the modules are semantically complete; it is the new bootstrap floor for
parallel hand-ownership work.

What changed:

- Reworked the failing `src/core` seed modules so each file now compiles
  individually through `target/debug/kain build <file> -t rust`.
- Simplified over-ambitious seed implementations in `lexer.kn`, `parser.kn`,
  `runtime.kn`, `types.kn`, and `kainc.kn` into bounded, explicit bootstrap
  versions.
- Added the local `KainResult` alias in `error.kn`, but used explicit
  `Result<..., KainError>` signatures in the owned seed files where the current
  frontend still fails to unify imported aliases cleanly.
- Replaced cross-module static helper/method usage with direct field checks or
  local helpers where needed. The current frontend still struggles with some
  imported static methods and method calls across modules.

Validation:

- Full sweep over `src/core/*.kn` with:
  `target/debug/kain build <file> -t rust -o /tmp/core_sanity/<file>.rs`
- Result after the pass:
  `ast`, `comptime`, `diagnostic`, `effects`, `error`, `kainc`,
  `language_features`, `lexer`, `low_level_abi`, `low_level_memory`,
  `low_level_memory_metadata`, `parser`, `runtime`, `span`, `stdlib`, and
  `types` all emitted output successfully.

Current risks:

- Several modules are intentionally thinner now than their Rust/bootstrap donor
  equivalents. They are compile-safe seeds, not feature-complete translations.
- The current frontend still has rough edges around imported aliases, some enum
  pattern bindings, and cross-module static/method resolution. Those are
  language/frontend limitations, not just seed-file mistakes.

Recommended next step:

- Keep `src/core` compile-safe while agents deepen functionality module by
  module. The next serious ownership pass should expand `lexer`, `parser`,
  `types`, and `runtime` together so they grow as a coherent semantic slice
  rather than independently reintroducing frontend-hostile patterns.

## 2026-04-12 - `src/core` is the owned folder path, and active identifiers are Kain

The owned language tree now lives under `src/core`. The folder rename removes
the old spelling from the active owned tree, while the code still uses `Kain`
for the current language/core identity.

What changed:

- Renamed the self-host driver shell from `korec.kn` to `kainc.kn`.
- Renamed the driver shell types, helpers, and log prefixes to `Kain*` and
  `KAIN*`.
- Renamed the feature contract types and constants in
  `src/core/language_features.kn` to `Kain*` / `KAIN_*`.
- Renamed the owned-core folder from the previous nod path to `src/core`.
- Updated the owned-core README to describe Kain and to point at `src/core`.

Current note:

- The only remaining legacy donor naming reference in the owned tree is the historic donor
  filename `src/.legacy/src/korec.kn` in the README matrix.
- The driver shell is still a seed shell; it is named correctly now, but the
  implementation remains intentionally synthetic.

## 2026-04-12 - selfhost can now mirror every workspace crate with `--all-crates`

The selfhost mirror tree no longer has to stop at the profile's bounded phase slices when the operator wants a whole-workspace source dump. `kain selfhost phase1` and `phase2` now accept `--all-crates`, which discovers every `crates/*/Cargo.toml` directory at runtime, drives the same file-preserving mirror pipeline over that live crate set, and records the selection mode in the emitted report.

What changed:

- Reworked `crates/cli/src/selfhost.rs`
  - Added `--all-crates` to both selfhost phases.
  - Phase crate selection is now overridable by live workspace discovery from `repo_root/crates/*/Cargo.toml` instead of only the profile/module-map slices.
  - Added focused tests covering sorted crate discovery and the override behavior.
- Updated `crates/cli/src/selfhost_report.rs`
  - Reports and markdown summaries now record `all_crates_mode` so forced whole-workspace sweeps are explicit evidence artifacts instead of inferred behavior.
- Updated `ARCHITECTURE.md`
  - Added the canonical whole-workspace mirror command for future operators and agents.

Validation:

- `cargo test -p cli --lib discover_all_workspace_crates_lists_sorted_cargo_directories -- --nocapture`
- `cargo test -p cli --lib resolve_crates_for_phase_prefers_all_crates_override -- --nocapture`
- `cargo test -p cli --lib default_profile_maps_phase_slices -- --nocapture`
- `cargo test -p cli --lib build_file_mirror_plans_preserves_file_level_paths -- --nocapture`
- `cargo test -p cli --lib write_roundtrip_rust_tree_splits_inline_modules_into_real_files -- --nocapture`
- `cargo run -q -p cli --bin kain -- selfhost phase2 --inventory-dir ouroboros/docs/selfhost/inventories --output-dir ouroboros/out/selfhost/phase2_all_crates_repo_src --emit-roundtrip-rust false --assemble-stage2 false --build-stage2 false --force --all-crates`
  - Mirrored 36 workspace crates into repo-root `src/` as 269 `.kn` files.
  - 23 crates imported cleanly enough to mirror under strict mode; 13 still hard-failed.
  - Dominant strict blocker families in the wide sweep were `trait_surface_lowering` (24 diagnostics), `trait_object_lowering` (13), `extern_crate_decl` (4), and `array_repeat_lowering` (4).
  - Representative failing crates in the wide sweep were `cli`, `kain-3D`, `kain-host`, `kain-ui-native`, and `ue5-editor`.

Design decisions:

- Kept `--all-crates` as a CLI override instead of mutating the default profile slices so the bounded repair lane stays stable and the workspace-wide source dump stays opt-in.
- Discovery keys off real `Cargo.toml` directories under `crates/` because the selfhost pipeline is path-preserving around the repo's crate folder names, not Cargo package metadata aliases.

Current risks:

- The whole-workspace sweep is structurally correct now, but it is still a mirror-first bootstrap artifact set rather than a green selfhost compile lane.
- Trait-surface and trait-object support remain the main semantic blockers across the newly imported crates.

Recommended next step:

- Triage the wide-sweep failures by diagnostic family rather than crate count. `trait_surface_lowering` and `trait_object_lowering` now dominate enough of the remaining frontier that they should be attacked as importer policy families, not one crate at a time.

## 2026-04-12 - selfhost now emits a profile-driven file mirror tree and can keep partial artifacts under `--force`

The selfhost lane is no longer structurally centered on one generated `.kn` per crate. It now emits a file-preserving Kain mirror tree, a source-correspondence manifest, split roundtrip Rust trees, and a stage2 workspace assembled from those per-file artifacts. The CLI also now has a `--force` mode so phase runs keep later artifacts instead of aborting on the first crate that fails.

What changed:

- Extended the Rust selfhost importer in `crates/kain-import/src/rust/selfhost.rs`
  - The detailed selfhost result now exposes one typed `Program` per imported Rust module/file instead of only the crate aggregate.
- Added `crates/cli/src/selfhost_profile.rs`
  - Introduced the data-driven `SelfHostSourceProfile` model for canonical source roots, output mirror roots, roundtrip roots, stage2 workspace roots, and phase slices.
- Added `ouroboros/docs/selfhost/metadata/selfhost_source_profile.json`
  - The default selfhost source profile now defines the canonical `src/<crate>/...` mirror contract and the phase1/phase2 crate waves.
- Reworked `crates/cli/src/selfhost.rs`
  - Phase1 and phase2 now build one mirror plan per Rust source file.
  - The pipeline writes canonical Kain mirrors, output-local mirror copies, aggregate compatibility bundles, and a `source_correspondence_manifest.json`.
  - Phase2 roundtrip Rust is split back into `roundtrip_rust/<crate>/src/...` instead of staying as a single flat file.
  - Stage2 workspace assembly now copies those split source trees into `stage2_workspace/crates/<crate>/src/...`.
  - Added true boolean control for `--emit-roundtrip-rust`, `--assemble-stage2`, and `--build-stage2`.
  - Added `--force` so selfhost continues emitting later crate artifacts and report evidence after earlier crate failures.
- Updated `crates/cli/src/selfhost_report.rs`
  - Reports now record profile roots, source-correspondence manifest paths, mirror counts, roundtrip tree roots, force mode, and stage2 error state.
- Updated `crates/cli/Cargo.toml`
  - Wired `kain-c-ffi` into the CLI crate so the current `main.rs` import path builds during full-bin validation.

Validation:

- `cargo test -p cli --lib default_profile_maps_phase_slices -- --nocapture`
- `cargo test -p cli --lib build_file_mirror_plans_preserves_file_level_paths -- --nocapture`
- `cargo test -p cli --lib write_roundtrip_rust_tree_splits_inline_modules_into_real_files -- --nocapture`
- `cargo run -q -p cli --bin kain -- selfhost phase1 --inventory-dir ouroboros/docs/selfhost/inventories --profile-path /tmp/kain_selfhost_source_profile_validation.json --output-dir /tmp/kain_selfhost_phase1_validation`
- `cargo run -q -p cli --bin kain -- selfhost phase2 --inventory-dir ouroboros/docs/selfhost/inventories --profile-path /tmp/kain_selfhost_source_profile_validation.json --output-dir /tmp/kain_selfhost_phase2_mirror_validation --emit-roundtrip-rust false --assemble-stage2 false --build-stage2 false`
  - Produced a `hard_fail` report, but still emitted 71 mirrored files plus `source_correspondence_manifest.json` across `kain-core`, `kain-import`, `kain-sys-codegen`, and `cli`.
- `cargo run -q -p cli --bin kain -- selfhost phase2 --inventory-dir ouroboros/docs/selfhost/inventories --profile-path /tmp/kain_selfhost_source_profile_validation.json --output-dir /tmp/kain_selfhost_phase2_force_roundtrip_validation --assemble-stage2 false --build-stage2 false --force`
  - Previously this lane died at the first roundtrip/codegen failure. Under `--force` it now emitted aggregate bundles and a full correspondence/report artifact set for all four phase2 crates before returning `hard_fail`.
- `cargo run -q -p cli --bin kain -- selfhost phase2 --inventory-dir ouroboros/docs/selfhost/inventories --output-dir ouroboros/out/selfhost/phase2_repo_src --emit-roundtrip-rust false --assemble-stage2 false --build-stage2 false --force`
  - Materialized the first live repo-root canonical mirror tree under `src/` with 71 generated `.kn` files while still returning `hard_fail` because the `cli` slice keeps its `extern crate` strict-import rejections.

Design decisions:

- Treated the file-preserving mirror tree as the primary structural artifact and kept aggregate `.kn` / `.roundtrip.rs` outputs only as a compatibility bridge for the still-single-source frontend/codegen lane.
- Kept `--force` honest: it preserves artifact generation and reporting, but it does not hide failure. Final status stays `hard_fail` when import, roundtrip, or stage2 errors remain.
- Allowed force-mode stage2 assembly to assemble only crates that actually produced roundtrip Rust, instead of aborting the whole lane on missing earlier outputs.

Current risks:

- The active frontend still compiles one source string at a time, so true multi-file Kain compilation has not landed yet.
- Phase2 is still not green. The current force-mode validation shows roundtrip/type errors in `kain-core`, `kain-import`, and `kain-sys-codegen`, plus strict `extern crate` rejections in `cli`.
- `ARCHITECTURE.md` still contains some legacy `M:/Code/Kain` link targets even though the durable selfhost story is now checkout-relative and Linux-safe.

Recommended next step:

- Remove the current phase2 blockers one crate family at a time, but keep using the file mirror tree plus `--force` so every rerun leaves behind a full artifact graph and not just the first failure site.

# 2026-04-12 - material_atrium now compiles through the LLVM/native executable lane

The `material_atrium_showcase` smoke now compiles through Kain's LLVM/native executable lane into a standalone native binary under `smoketest/3D/material_atrium_showcase/llvm-native/`, but Linux still uses the Qt shell as the visible compatibility presenter. The authored smoke stays source-first and Kain-owned; the remaining gap is a real non-Rust native host that can present the LLVM executable directly on Linux.

What changed:

- Repaired `smoketest/3D/material_atrium_showcase/smoke.kn` so the backend switch buttons lower as string-backed command routes instead of unresolved function identifiers.
- Added a tiny `fn main() -> Int` entrypoint so the LLVM/native lane can link the smoke as a standalone executable.
- Compiled the smoke through `./target/debug/kain ... --target llvm ...`, which now emits `material-atrium-showcase.ll`, `material-atrium-showcase.runtime_contract.json`, `material-atrium-showcase.realtime_app.json`, and a linked `material-atrium-showcase` binary in the `llvm-native/` output tree.
- Refreshed the Qt compatibility shell with `build native-ui` so the updated smoke remains visible on screen while the native host gap is still being closed.

Current risk:

- The standalone LLVM binary currently exits immediately on Linux because the visible presentation path still depends on the Qt host. The compile lane is native now, but the cross-platform native presenter still needs a real non-Rust host.

Recommended next step:

- Replace the remaining Qt/Rust presentation hop with a real native host surface for Linux so the LLVM executable itself can be the thing the user launches and sees.

## 2026-04-12 - material_atrium smoke moved to a source-first, Kain-owned launcher path

The `smoketest/3D/material_atrium_showcase` smoke now treats `smoke.kn` as the authored entrypoint. The native launcher includes the Kain source directly, the top bar state and backend mood switching live in Kain language code, and the smoke names `material_atrium` as a first-class runtime profile instead of a generated bundle preview.

What changed:

- Rewrote `smoketest/3D/material_atrium_showcase/smoke.kn` so the hero shell, backend switchboard, and runtime-owner messaging are authored in Kain source.
- Updated `smoketest/3D/material_atrium_showcase/native-app/src/main.rs` so the launcher includes `smoke.kn` directly and can accept a renderer backend override at startup.
- Added a dedicated `material_atrium` geometry branch and viewport-profile hinting in `runtime/native/src/platform/win32/kain_runtime_viewport_win32.c`.
- Refreshed the smoke README and durable architecture notes so they describe the source-first path instead of the older bundle-centric story.

Current risk:

- The Qt shell is still the compatibility host, so the smoke is source-first and Kain-owned but not yet a live native renderer surface on Linux.

Recommended next step:

- Collapse the remaining compatibility preview layer and drive one live native 3D surface end-to-end, starting with the `bgfx` lane.

## 2026-04-12 - Linux piano lab now boots as a real native UI/audio loop demo

The `labs/playground/piano` lab now builds a Linux-native 2D piano app through Kain semantics, the native UI host, and a small C audio runtime. It opens a keyboard surface, plays generated notes, records loop events, and replays them from the C bridge instead of treating audio as a mock.

What changed:

- Added `labs/playground/piano` with Kain `world`/`patch`-driven UI state, transport controls, octave key rows, and an action tape.
- Added a C audio bridge under `labs/playground/piano/native` that boots `miniaudio`, caches note WAVs, records loop events, and replays them on a background thread.
- Fixed the C-FFI generator in `crates/kain-c-ffi` so `Void` parameters consume `Value::Unit` correctly instead of emitting broken `_arg1`/`arg1` wrappers for zero-arg C APIs.
- Taught `labs/playground/piano/run.sh` to auto-detect the current Wayland/X11 desktop session and export the minimum env needed to attach on Linux even when the shell did not inherit GUI variables.

Validation:

- `./build.sh`
- `nohup ./run.sh >/tmp/kain_piano_launch.log 2>&1 &`
- Verified the launched process stayed resident as `native-app/kain-piano`; the startup log only showed repeated Qt font bearing warnings and no fatal display error.

Risks:

- The native Qt host still prints repeated `Apple Color Emoji` font bearing warnings on startup. They were non-fatal in this run but are noisy enough to revisit later.
- The current loop recorder is intentionally simple: it captures note events and timing, not a richer performance timeline or edit stack.

Recommended next step:

- Add keyboard shortcuts or MIDI-style input so the piano can be played without relying only on mouse clicks.

## 2026-04-12 - Qt smoke styling is demo-only; Kain UI remains theme/data driven

The Plasma-ish Qt smoke shell is a presentation skin for the host proof, not the authored UI contract. The actual Kain UI model still needs to stay theme-driven and backend-neutral through `UiStyleSpec`, `UiThemeRegistry`, surface roles, and bundle metadata.

What changed:

- Clarified the UI host docs and native Qt smoke contract so future work does not confuse the generated showcase shell with the runtime's authored look model.
- Kept the Qt shell stylized for smoke readability, but the durable design rule is that Kain-authored apps should continue to control appearance through theme data and surface metadata rather than a single baked visual preset.

Why it matters:

- The smoke can keep one strong visual identity for proof screenshots.
- The runtime itself must still be able to express many different UI styles and component libraries without reworking the host around one fixed aesthetic.

## 2026-04-12 - material_atrium smoke is now Qt-native and primitive-backed

The `material_atrium_showcase` smoke now presents the 3D runtime through the `kain-ui-native` Qt shell instead of the old egui host path, and the atrium scene itself is now authored from Kain primitives instead of a mostly hand-wired mesh pile.

What changed:

- Rewrote `smoketest/3D/material_atrium_showcase/smoke.kn`
  - The shell now reads as a native Qt product surface with a larger hero, primitive stack cards, and a tighter runtime matrix.
  - The smoke source stays inside the native runtime path through `kain-ui-native`.
- Updated the native-ui generator pipeline in `crates/cli/src/native_ui_build.rs`
  - `material_atrium_visual_example.png` is now packaged as a real host sidecar when present.
  - The generated launcher now forwards that preview image through `KAIN_UI_NATIVE_QT_VIEWPORT_IMAGE_PATH`, which is why the atrium preview now shows up in the Qt shell without a manual one-off edit.
- Refreshed `crates/kain-ui-native/src/no_egui_qt_host.rs`
  - The atrium branch is a real renderer switchboard with top-level backend cards and a preview rail instead of a single static view.
- Reworked `crates/kain-3D/src/scene.rs`
  - `material_atrium` now builds through the authoring scene path and registers `PrimitiveLibrary::authored_defaults()`.
  - The scene massing now uses the authored primitive library for the floor, columns, halo ring, monoliths, and spire shapes.
  - The scene keeps motion by restoring a post-flatten animation list for the central orb, halo ring, monolith, and spire.
- Refreshed `crates/kain-3D/src/bin/material_atrium_smoke.rs`
  - The deterministic preview PNG and matrix report were regenerated from the new primitive-backed scene.
  - The header copy was shortened so the image no longer clips the top explanatory line.
- Updated `smoketest/3D/material_atrium_showcase/README.md`
  - The doc now says the showcase is Qt-native and primitive-backed instead of reading like a generic harness.
- Regenerated the checked-in native app bundle
  - `smoketest/3D/material_atrium_showcase/native-app/generated/native_app_bundle.json`
  - This keeps the Qt shell bundle in sync with the authored smoke source.

Validation:

- `cargo run -p cli --bin kain -- build native-ui smoketest/3D/material_atrium_showcase/smoke.kn --app-name material-atrium-showcase --window-title "Kain Material Atrium Showcase" -o smoketest/3D/material_atrium_showcase/native-app`
- `cargo test -q -p cli native_ui_build_packages_material_atrium_preview_sidecar_when_present -- --nocapture`
- `cargo run -p kain-3d --bin material_atrium_smoke -- --output-image smoketest/3D/material_atrium_showcase/material_atrium_visual_example.png --output-json smoketest/3D/material_atrium_showcase/generated/material_atrium_runtime_matrix.json`

Risks:

- The native smoke is still a Qt-hosted shell around the runtime scene contract; it is not yet a live in-process bgfx / filament / diligent / the-forge viewport bridge.
- The image generator still uses the Kain compatibility renderer, so it proves scene composition and shell presentation rather than real vendor backend execution.

Recommended next step:

- Hook the Qt viewport surface to a real backend session when the renderer bridge is ready, then regenerate the smoke image from the live path instead of the compatibility renderer.

## 2026-04-12 - Qt host gained deterministic screenshot capture and a Plasma-style smoke lane

The default Qt-backed `kain-ui-native` host is no longer just a launcher plus temp artifacts. It now has a deterministic smoke path that can render the real generated shell, save its generated host files, and capture a proof PNG without a manual desktop session.

What changed:

- Extended `crates/kain-ui-native/src/no_egui_qt_host.rs`
  - Added `KAIN_UI_NATIVE_QT_ARTIFACT_DIR` so callers can force the generated `Main.qml` and `session.json` into a durable output folder instead of an anonymous temp dir.
  - Added `KAIN_UI_NATIVE_QT_SCREENSHOT_PATH` so the generated Qt Quick host auto-captures itself with `grabToImage(...)` and exits once the PNG is written.
  - Restyled the generated host shell into a stronger Plasma-inspired control deck instead of the earlier flat diagnostic scaffold.
- Added a dedicated Qt host smoke at `smoketest/UI/qt_plasma_runtime_lounge`
  - `native-app/src/main.rs` builds a curated runtime bundle directly in Rust using `kain-ui` types.
  - The bundle exercises all current routing lanes the shell can represent honestly: Qt-backed documents, a viewport slot, ImGui devtools slots, and a staged CEF fallback surface.
  - `run_smoke.sh` forces a deterministic offscreen Qt run with `qml`, software rendering, a fixed artifact directory, and a screenshot output path.
  - The smoke produces:
    - `outputs/qt_plasma_runtime_lounge.png`
    - `outputs/generated/Main.qml`
    - `outputs/generated/session.json`
- Updated `smoketest/UI/README.md` so the smoke suite advertises the new Qt proof lane explicitly.

Validation completed:

- `cargo check -p kain-ui-native`
- `cargo check --manifest-path smoketest/UI/qt_plasma_runtime_lounge/native-app/Cargo.toml`
- `./smoketest/UI/qt_plasma_runtime_lounge/run_smoke.sh`

Design decisions:

- Chose host-driven screenshot capture instead of desktop screenshot tooling so the proof works in headless/offscreen sessions and uses the actual generated Qt shell.
- Kept the smoke metadata-first because that matches the current product truth: the Qt shell is live, while viewport and ImGui embeddings are still deliberate placeholders.
- Put the proof under `smoketest/UI/` rather than inside `crates/kain-ui-native/examples` so it stays aligned with the repo’s durable smoke matrix and operator flow.

Current risks:

- The screenshot path proves the shell and routing contract, not fully live in-process rendering for bgfx, ImGui, RmlUi, or CEF.
- The generated `Main.qml` embeds the session JSON inline for simplicity; if the session payload grows much larger, the shell should probably switch to reading `session.json` at runtime instead of inlining it.
- Offscreen capture currently depends on the external `qml` runtime supporting `grabToImage(...)`; if host packaging moves to a compiled-in Qt bridge later, this contract should be preserved but revalidated.

Recommended next step:

- Replace one placeholder with a real adapter inside this same smoke lane, starting with the viewport slot so the proof PNG shows an actual live Kain-rendered surface instead of only metadata cards.

## 2026-04-11 - default kain-ui-native host now launches a real Qt Quick session

The no-`egui` cut is no longer just a facade. `kain-ui-native` now has a live default host path that materializes a Qt-backed session when the machine has an external Qt Quick runtime available.

What changed:

- Added a real default-host implementation in `crates/kain-ui-native`
  - Split the non-legacy path into:
    - `no_egui.rs` for the public API and routing
    - `no_egui_session.rs` for bundle-to-session classification
    - `no_egui_qt_host.rs` for Qt runtime discovery, artifact emission, and process launch
  - `run_bundled_app(...)` now launches a generated Qt Quick session instead of always erroring.
- Added a data-driven Qt session manifest
  - The host now classifies compiled UI surfaces into document, viewport, devtools, and fallback lanes.
  - It writes a temp `session.json` and generated `Main.qml` before launching `qmlscene` or `qml`.
  - Missing live adapters degrade to explicit placeholder panes instead of aborting startup.
- Promoted default bundle semantics to Qt-first
  - `crates/kain-ui/src/lib.rs` now defaults `UiRuntimeMetadata.compatibility_host_backend` to `Qt`.
  - The no-`egui` backend plan already defaulted shell compatibility to Qt; now the shared bundle metadata agrees.
- Promoted the native runtime Qt lane from staged fiction to honest external-runtime probing
  - `runtime/native/include/kain_runtime_vendor_lane.h` now marks `imgui`, `yoga`, and `qt` as present runtime lanes in the vendor catalog.
  - `runtime/native/src/vendor/kain_runtime_vendor_ui_bridge.cpp` now probes for external Qt Quick runtimes via `KAIN_UI_NATIVE_QT_RUNTIME`, `KAIN_QT_QML_RUNTIME`, `qmlscene`, or `qml`.
  - `ui.backend.qt` now reports `qt-external-runtime` instead of `qt-staged` when that lane is available.

Validation completed:

- `cargo check -p kain-ui-native`
- `cargo test -q -p kain-ui-native`
- `cargo test -q -p kain-ui`
- `cargo check -p kain-ui-native --features legacy-egui`
- `./runtime/compile_native_runtime.sh`
- `./runtime/conformance/run_all.sh --category host_bridge --verbose`

Design decisions:

- Chose an external Qt Quick runtime first instead of a compiled-in Qt Widgets/QML bridge because the current environment does not ship a linkable Qt development toolchain.
- Kept the session metadata-first and role-routed so a later in-process Qt bridge can replace the launcher without changing Kain bundle semantics again.
- Treated viewport and devtools as honest placeholders in the Qt shell for now rather than lying about bgfx or ImGui being embedded already.

Current risks:

- The default host now depends on an external `qmlscene` or `qml` runtime being installed or pointed to by env var; this repo does not bundle a Qt runtime yet.
- The Qt shell is real, but it still renders metadata-driven panes rather than fully executing retained document surfaces or embedding the bgfx viewport in-process.
- `ui.backend.qt` is now truthier than before, but it still represents external-runtime availability, not a fully bundled Qt toolchain.

Recommended next step:

- Replace the metadata-only document/viewport/devtools placeholders with the first real in-process adapters, starting with a viewport container handoff and a dedicated Qt document-surface presenter.

## 2026-04-11 - renderer-session host wiring and deterministic 3D smoke artifact landed

The 3D runtime push moved from “backend catalog and showcase shell” into a real host-consumed renderer lane. The native viewport now understands renderer-session truth, the smoke folder now has a reproducible image generator, and the visual example asset is no longer just a concept render.

What changed:

- Wired the renderer session into the Win32 raw-native viewport host
  - Added `runtime/native/include/kain_runtime_renderer_session.h` and `runtime/native/src/core/kain_runtime_renderer_session.c` to the manifest-driven native runtime bundle.
  - Updated `runtime/native/src/platform/win32/kain_runtime_viewport_win32.c` so the host boots a `KainRuntimeRendererSession` from `KAIN_RUNTIME_RENDERER_BACKEND`, the runtime graphics bundle, and the backend catalog.
  - The viewport overlay now reports requested/active backend identity, service key, vendor runtime/version, probe/start truth, and compatibility-executor diagnostics instead of only showing the older GL/procedural-world summary.
  - Host shutdown now tears down the renderer session cleanly through the Kain-owned vendor service surface.
- Added a deterministic visual-proof path under `crates/kain-3D`
  - Added `crates/kain-3D/src/bin/material_atrium_smoke.rs`.
  - Added `png`, `font8x8`, and `serde_json` dependencies to `crates/kain-3D/Cargo.toml`.
  - The new binary renders the real `material_atrium` scene through Kain’s software compatibility renderer, composes a four-card backend matrix for `bgfx`, `filament`, `diligent`, and `the-forge`, and writes:
    - `smoketest/3D/material_atrium_showcase/material_atrium_visual_example.png`
    - `smoketest/3D/material_atrium_showcase/generated/material_atrium_runtime_matrix.json`
- Expanded smoke tooling and docs
  - Added `generate_runtime_matrix.sh` and `generate_runtime_matrix.bat`.
  - Updated `launch_native_app.sh` and `launch_visual_exe.bat` so they accept an optional backend argument and export `KAIN_RUNTIME_RENDERER_BACKEND`.
  - Updated `smoketest/3D/material_atrium_showcase/README.md` so it documents the real generation flow and stops claiming the visual example is only a concept mock.

Validation completed:

- `cargo run -p kain-3d --bin material_atrium_smoke -- --output-image smoketest/3D/material_atrium_showcase/material_atrium_visual_example.png --output-json smoketest/3D/material_atrium_showcase/generated/material_atrium_runtime_matrix.json`
- `cargo build -p cli`
- `./runtime/compile_native_runtime.sh`
- `./runtime/validate_native_runtime.sh`

Design decisions:

- Kept Kain as the owner of renderer-session identity, fallback, and diagnostics instead of exposing raw vendor APIs in the viewport host.
- Chose to keep the smoke artifact deterministic and software-rendered from `kain-3D` so Linux and Windows can both regenerate the same proof without needing every vendor backend fully live in the headless environment.
- Overwrote `material_atrium_visual_example.png` with a real generated artifact rather than leaving a concept-only image in a repo that now has enough code to produce the scene honestly.

Current risks:

- Win32 host execution still routes the actual scene through the compatibility OpenGL executor. The session truth is real; direct `bgfx`/Filament/Diligent/The Forge viewport execution is still future work.
- Linux still has the software smoke and runtime catalog truth, but not a full native app-host/input/viewport execution lane equivalent to the Win32 host.
- The generated JSON matrix lives under `generated/`, which is gitignored; the tracked PNG is durable, but the detailed report is ephemeral unless regenerated.

Recommended next step:

- Replace the compatibility executor with the first direct vendor viewport path, starting with `bgfx`, then teach the Linux host lane to consume the same renderer-session contract instead of remaining mostly adapter-only.

## 2026-04-11 - egui was evicted from the default kain-ui-native crate path

`kain-ui-native` no longer drags the old `egui` host through the default build. The crate now has a clean default facade for bundle/build metadata and an explicit legacy feature for the old desktop host.

What changed:

- Split `crates/kain-ui-native/src/lib.rs` into a feature-gated dispatcher
  - Default builds now export a small no-`egui` facade.
  - The old host was preserved as `crates/kain-ui-native/src/legacy_egui.rs`.
- Added `crates/kain-ui-native/src/no_egui.rs`
  - Preserves the public bundle/runtime API surface used by apps and smoketests:
    - `KainUiNativeBackendPlan`
    - `KainUiNativeAppConfig`
    - runtime bundle serialization helpers
    - `run_*` entrypoints
  - Keeps build/bundle generation working from `kain-core` + `kain-ui`.
  - Makes runtime launch fail fast with an explicit error instead of silently booting the legacy host.
- Feature-gated the old host dependency blast in `crates/kain-ui-native/Cargo.toml`
  - `eframe`, `egui-wgpu`, `wgpu`, `kain-3D`, `ab_glyph`, `fdsm`, `fdsm-ttf-parser`, `image`, `nalgebra`, `bytemuck` are now optional and only enabled by `legacy-egui`.
- Stopped default runtime metadata from advertising `LegacyEgui` as the compatibility backend
  - The no-`egui` facade now defaults the compatibility lane to `Qt` so bundle metadata does not claim a host that the default crate build no longer provides.

Validation completed:

- `cargo check -p kain-ui-native`
- `cargo check -p kain-ui-native --features legacy-egui`

Design decisions:

- Chose extraction over deletion. The old host still exists for compatibility and archaeology, but it no longer defines the default crate identity.
- Preserved the bundle/build API first because many apps and smoketests already call `run_bundled_app_json(...)` and related helpers.
- Made the default runtime launch path fail loudly instead of pretending Qt/Imgui/RmlUi are live hosts before they exist.

Current risks:

- Default `kain-ui-native` launches now fail intentionally until a real non-`egui` shell lands.
- The bundle metadata now points at the future UI lane (`Qt` shell, `RmlUi` document host, `Imgui` devtools, `Yoga` layout), but those are planning defaults rather than live host implementations in this crate.
- The legacy feature still compiles the old monolith; it is compatibility debt, not a sustainable endpoint.

Recommended next step:

- Land the first real non-`egui` host adapter in `kain-ui-native`, starting with a shell/session coordinator that can launch one concrete backend instead of erroring out after bundle validation.

## 2026-04-11 - first UI vendor runtime slice landed with honest service bits and backend-role metadata

The UI overhaul is no longer only a plan. The native runtime now has a real UI vendor lane, the startup contract surface has room to describe it honestly, and the semantic UI crates can declare mixed-backend sessions without treating the native host as one opaque `egui` blob.

What changed:

- Expanded `runtime/native` for UI vendor families
  - Added `runtime/native/include/kain_runtime_vendor_ui_bridge.h` and `runtime/native/src/vendor/kain_runtime_vendor_ui_bridge.cpp`.
  - Added service families for:
    - `ui.layout.yoga`
    - `ui.render.skia`
    - `ui.backend.imgui`
    - `ui.backend.rmlui`
    - `ui.backend.slint`
    - `ui.backend.qt`
    - `ui.surface.browser.cef`
    - `ui.devtools`
  - Wired `imgui` and `yoga` as the first compile-backed UI vendors through the manifest-driven native runtime bundle.
  - Kept `skia`, `rmlui`, `slint`, `qt`, and `cef` staged behind the same Kain-owned vendor bridge instead of pretending they are production-ready.
- Widened the runtime contract mask
  - `runtime/native/include/kain_runtime_contract.h` now uses a `uint64_t`-backed `KainRuntimeServiceMask`.
  - This unblocked honest contract bits for the new UI service families instead of aliasing them into an already full 32-bit mask.
  - Updated startup validation, win32 host callers, and the diagnostics conformance helper to use the wider mask.
- Expanded semantic UI truth in `crates/kain-ui`
  - Added explicit backend-role enums:
    - `UiHostBackendKind`
    - `UiLayoutEngineKind`
    - `UiRenderEngineKind`
  - Extended `UiRuntimeMetadata` and `UiSurface` so bundles can describe shell/document/devtools hosts plus layout/render preferences without host-local side channels.
  - Added host-backend capability tables alongside the older coarse renderer table.
- Added the first native host coordinator seam in `crates/kain-ui-native`
  - Introduced `KainUiNativeBackendPlan`.
  - The native host now carries shell/devtools/layout/render/compatibility choices through bundle creation and runtime reload.
  - `egui` remains the live compatibility host, but the crate no longer has to pretend it is the only backend model worth describing.
- Re-synced `runtime/native_runtime_metadata.json` with the manifest after the UI lane additions so tooling sees the same service families, source groups, and include roots as the build.

Validation completed:

- `cargo check -p kain-ui`
- `cargo check -p kain-ui-native`
- `cargo test -q -p kain-ui`
- `./runtime/compile_native_runtime.sh`
- `./runtime/conformance/run_all.sh --category diagnostics --verbose`
- `./runtime/conformance/run_all.sh --category host_bridge --verbose`

Design decisions:

- Chose `imgui` and `yoga` as the first compile-backed UI vendors because they are small enough to prove the runtime seam now and useful enough to shape the architecture.
- Chose a 64-bit service mask instead of another round of bit aliasing because the startup contract needs to stay trustworthy as the runtime broadens.
- Kept the heavier UI stacks staged; the point of this pass was to create a Kain-owned UI vendor lane and coordinator seam, not to jam Qt, CEF, Slint, RmlUi, and Skia into one unstable host.
- Preserved legacy `egui` compatibility in `kain-ui-native` while making backend selection explicit runtime metadata instead of a hardcoded host assumption.

Current risks:

- `kain-ui-native` is still operationally `egui`-hosted. The new backend plan is a coordinator seam, not a full backend cutover yet.
- `runtime/native_runtime_metadata.json` is synchronized again, but the repo still lacks a strict automated parity check that fails fast when the TOML and JSON drift.
- `imgui` and `yoga` are compile-backed and registered, but they are not yet deeply integrated into patch execution, the runtime value ABI, or the native host rendering path.
- The heavier UI vendors are intentionally staged only; they still need deliberate source selection and adapter design before compile-backed incorporation.

Recommended next step:

- Consume the new backend-role metadata for one real host cutover seam:
  - route layout execution through `ui.layout.yoga`
  - route devtools and inspector surfaces through `ui.devtools` / `ui.backend.imgui`
  - keep the shell on the compatibility host until one Qt- or Skia-backed presentation seam is deliberately proven

## 2026-04-11 - mixed-language graphics vendor slice landed with bgfx/bimg live and The Forge staged

The runtime now has a real renderer-backend seam in native code instead of only a graphics-vendor plan doc. The first implementation pass added a mixed C/C++ native build path, a Kain-owned renderer backend catalog, real `bgfx` + `bx` + `bimg` vendor incorporation, a staged `the-forge` backend identity, and a source-first 3D smoke shell under `smoketest/3D/material_atrium_showcase`.

What changed:

- Extended the manifest-driven native build path so `runtime/native_runtime.toml` can compile mixed C and C++ source graphs.
- Added a Kain-owned renderer backend catalog and graphics vendor bridge under `runtime/native/include/` and `runtime/native/src/`.
- Promoted these service families into the runtime registry and metadata:
  - `gfx.backend.bgfx`
  - `gfx.backend.filament`
  - `gfx.backend.diligent`
  - `gfx.backend.forge`
  - `asset.image.bimg`
  - `asset.texture.bimg`
- Wired `bgfx`, `bx`, and `bimg` into the native runtime bundle as the first live graphics vendor lane.
- Staged `the-forge` in the same backend seam as a non-compiled future lane so the service catalog, metadata, and smoke shell already understand it.
- Added the `material_atrium` scene and the new `smoketest/3D/material_atrium_showcase` native-ui smoke shell to prove the renderer-backend direction in a product-style presentation instead of a bare viewport.

Design decisions:

- Kept Kain as the owner of renderer identity, service keys, and backend selection while letting vendor code sit behind a graphics bridge.
- Chose to stage `the-forge` now rather than compile it directly because the current bridge seam is ready for identity/catalog work, but not for a large Common_3 compile blast.
- Treated the smoke shell as a durable proof artifact for the graphics-runtime lane, not a one-off demo.

Current risks:

- `bgfx` and `bimg` are compiled and link-proven, but the runtime still does not have a full host-facing viewport/backend split yet.
- `filament`, `diligent`, and `the-forge` are still staged identities behind the seam rather than fully compiled backends.
- `bimg` has embedded third-party implementation files like `lodepng.cpp`; if the manifest adds those same implementation units separately, fixture linking will fail with duplicate symbols.

Recommended next step:

- Build the real viewport/backend handoff on top of the new backend catalog, then wire the first host-facing `bgfx` presentation path before attempting a full bridge for Filament, Diligent, or The Forge.

## 2026-04-11 - graphics vendor incorporation plan added for bgfx, bx, bimg, filament-core, and diligentcore

The runtime now has a concrete graphics-vendor doctrine instead of a generic “maybe wire all the renderers in” impulse. The new vendors are useful, but they need different roles.

What changed:

- Added `runtime/KAIN_GRAPHICS_VENDOR_INTEGRATION_PLAN_2026-04-11.md`
  - Positioned `bgfx` as the first practical renderer/backend lane for Kain.
  - Positioned `bx` and `bimg` as support infrastructure that should travel with the `bgfx` lane instead of pretending to be standalone runtimes.
  - Positioned `filament-core` as the higher-level premium scene/material/lighting experiment.
  - Positioned `diligentcore` as the future explicit Kain-native render-graph / compute / device-control lane.
  - Defined the wiring order:
    - build-path preparation for graphics vendors
    - `bgfx` + `bx` + `bimg`
    - viewport/backend split
    - `filament-core`
    - `diligentcore`
- Updated `ARCHITECTURE.md`
  - Recorded the renderer-vendor doctrine as a guardrail so future agents do not try to wire `bgfx`, Filament, and Diligent into one undifferentiated viewport path.

Design decisions:

- Chose `bgfx` first because it solves the fastest practical problem: a real cross-platform renderer/backend lane.
- Chose not to start with Filament even though it offers better immediate visuals, because it solves “look expensive” before it solves “make the runtime host/render sanely across platforms”.
- Chose not to start with Diligent even though it is strategically valuable, because it is the deeper architecture lane rather than the fastest usable runtime rendering substrate.
- Treated `gfx.viewport` as a host-facing presentation service, not as the permanent renderer contract.

Current risks:

- The current native runtime compile path is still effectively C-oriented, while `bgfx`, `filament-core`, and `diligentcore` are C++-heavy implementation trees.
- `bgfx` is easier to approach first because it has a C99 surface, but Filament and Diligent likely need deliberate bridge-library treatment before they should enter the runtime manifest path.
- The runtime still lacks a Kain-owned renderer backend interface, so the first implementation slice should create that seam before trying to make the viewport host cross-renderer-aware.

Recommended next step:

- Build the first renderer-backend seam with `bgfx` + `bx` + `bimg`, then split `gfx.viewport` into host-facing and backend-facing halves before attempting Filament or Diligent integration.

## 2026-04-11 - native runtime metadata and platform truth were resynchronized after the vendor slice

The first vendor-backed runtime slice landed cleanly on Linux, but the manifest and tooling metadata drifted immediately afterward. This follow-up pass brought the native runtime companion metadata back into sync and widened the new vendor lane so Windows development is represented explicitly instead of being treated like a Linux-only accident.

What changed:

- Re-synced the native runtime manifest, CLI manifest loader, compile script, and metadata JSON
  - Updated `crates/cli/src/main.rs` so the manifest parser understands platform-specific define lists through `windows_defines`, `linux_defines`, and `macos_defines`.
  - Updated `runtime/compile_native_runtime.sh` to honor those platform-specific define lists instead of treating POSIX-oriented defines as globally applicable.
  - Rebuilt `runtime/native_runtime_metadata.json` so it reflects the current vendor-backed service families, source groupings, link dependencies, and per-service platform scope from `runtime/native_runtime.toml`.
- Tightened vendor/platform truth for the new service families
  - Updated `runtime/native_runtime.toml` so the vendor-backed `io.*`, `script.quickjs`, `audio.*`, `wasm.*`, and `allocator.*` families declare `platforms = ["windows", "linux"]` instead of looking globally available.
  - Moved libuv-specific compile flags out of shared `defines` and into `windows_defines` / `linux_defines`.
  - Added the Windows libuv source graph plus the required extra Windows link libraries so the manifest can describe a real Windows vendor lane rather than only the Linux build.
  - Updated `runtime/native/include/kain_runtime_vendor_lane.h` and `runtime/native/src/vendor/kain_runtime_vendor_lane.c` so libuv-backed services are allowed on Windows as well as Linux.
- Strengthened docs around metadata durability
  - Updated `runtime/README.md`, `runtime/changelogs/NATIVE_RUNTIME_METADATA.md`, and `ARCHITECTURE.md` to make the TOML/JSON companion rule explicit and to document the platform-specific define model.

Validation completed:

- `cargo test -q -p cli runtime_manifest_resolves_relative_paths -- --nocapture`
- `cargo run -q -p kain-runtime-parallel -- json`
- `./runtime/compile_native_runtime.sh`
- `./runtime/validate_native_runtime.sh`
  - full suite passed again after the manifest/metadata sync: CLI build, native runtime compilation, LLVM/raw-native fixtures, and native runtime conformance

Design decisions:

- Treated `native_runtime.toml` as the primary build/runtime truth and `native_runtime_metadata.json` as a required tooling mirror, not as an eventually-consistent doc artifact.
- Chose explicit per-service platform declaration instead of overloading `status = "available"` to mean "maybe, depending on hidden macros".
- Extended the libuv-backed lane to Windows in the manifest/build model now, even though Linux remains the only deeply validated host at the moment.

Current risks:

- Windows support is now modeled directly in the manifest and vendor lane, but it has not yet been proven by the same end-to-end runtime validation depth as Linux on this host.
- The tooling metadata is synchronized again, but there is still no automated guard that diff-checks TOML service declarations against the JSON companion file.
- `wasm.runtime.full` and `wasm.wasi` remain staged/planned surfaces; the metadata now says so more honestly, but the runtime still needs the real WAMR source selection and compile proof.

Recommended next step:

- Add an automated metadata parity check between `runtime/native_runtime.toml` and `runtime/native_runtime_metadata.json`, then run a Windows-native compile/validation pass so the newly declared Windows vendor lane is proven instead of only modeled.

## 2026-04-11 - first native vendor incorporation slice is now live in the manifest-driven C runtime

The native runtime is no longer only planning around third-party vendors; the first real vendor-backed service slice is wired into `runtime/native`, compiled through the manifest-driven bundle, and validated end to end on Linux.

What changed:

- Expanded `runtime/native_runtime.toml`
  - Added `native/src/vendor/kain_runtime_vendor_lane.c`.
  - Added real vendor source sets for `QuickJS`, `miniaudio`, `wasm3`, `mimalloc`, `rpmalloc`, and the Linux `libuv` source graph.
  - Added vendor include roots and manifest defines for the Linux/libuv-oriented build.
  - Added new service declarations for:
    - `io.loop`, `io.fs`, `io.net`, `io.process`, `io.timers`
    - `script.quickjs`
    - `audio.backend`, `audio.graph`, `audio.device`, `audio.assets`
    - `wasm.runtime.light`, `wasm.runtime.full`, `wasm.module`, `wasm.wasi`
    - `allocator.mimalloc`, `allocator.rpmalloc`
- Added `runtime/native/include/kain_runtime_vendor_lane.h` and `runtime/native/src/vendor/kain_runtime_vendor_lane.c`
  - Introduced Kain-owned vendor function tables and a small vendor descriptor catalog.
  - Wired real probe/start/allocate/eval hooks for `libuv`, `QuickJS`, `miniaudio`, `wasm3`, `mimalloc`, and `rpmalloc`.
  - Left `WAMR` intentionally staged/degraded instead of pretending the curated tree is production-ready.
  - Added `KAIN_RUNTIME_VENDOR_STUBS_ONLY` so conformance harnesses can compile the service catalog without dragging the full vendor source graph into every isolated test binary.
- Expanded runtime service and contract surfaces
  - Updated `runtime/native/include/kain_runtime_services.h` and `runtime/native/src/core/kain_runtime_services.c` with the new vendor-backed service families and function-table pointers.
  - Updated `runtime/native/include/kain_runtime_contract.h` and `runtime/native/src/core/kain_runtime_contract.c` with new service-mask bits and `has_*` fields for the vendor families.
  - Preserved the raw-native core assumptions: `KAIN_RUNTIME_SERVICE_CORE_MASK`, `missing_core_service_count`, and `valid_for_raw_native` still only care about the existing three Win32 host services.
- Hardened build and vendor trees
  - Updated `runtime/compile_native_runtime.sh` to consume manifest `defines`, not just sources and include dirs.
  - Patched `runtime/3rdparty/quickjs/quickjs.c` with a `CONFIG_VERSION` fallback so Kain can compile the engine cleanly from the vendor tree.
  - Added `runtime/3rdparty/wamr/core/version.h` as a Kain shim because the curated WAMR tree omits the generated upstream header that `wasm_runtime_common.c` expects.
- Updated conformance harnesses
  - Patched `runtime/conformance/diagnostics/compile_tests.sh` and `runtime/conformance/host_bridge/run_tests.sh` so they compile the vendor lane in stub-only mode when they only need the registry/catalog surface.

Validation completed:

- `./runtime/compile_native_runtime.sh`
  - now compiles a 92-object native runtime bundle with the vendor sources included
- `./runtime/conformance/run_all.sh --verbose`
- `./runtime/validate_native_runtime.sh`
  - full suite passed: CLI build, native runtime compilation, LLVM/raw-native fixtures, and native runtime conformance

Design decisions:

- Treated vendor incorporation as Kain-owned service expansion, not as raw API exposure.
- Chose a real first slice now: `libuv`, `QuickJS`, `miniaudio`, `wasm3`, `mimalloc`, and `rpmalloc` are compile-backed and surfaced through the runtime.
- Kept `WAMR` staged because the curated tree still needs more than a thin manifest add; the missing generated `version.h` was only the first blocker, not proof of readiness.
- Added a stub-only vendor mode for narrow conformance binaries instead of forcing every conformance harness to link the full vendor graph.

Current risks:

- The vendor-heavy manifest materially increases compile time and warning volume; warning cleanup is now a real runtime maintenance lane.
- The new vendor service families are surfaced and probeable, but they are not yet deeply integrated into scheduler policy, host bridge semantics, or the runtime value ABI.
- `WAMR` is still only staged. The shimmed `version.h` unblocks future work, but the full/runtime+WASI lane still needs a deliberately reduced source selection and another compile pass.
- Windows support for the new vendor lane is not proven by the same depth as Linux yet, even though the wrapper source is guarded to avoid forcing Linux-only `libuv` symbols into non-Linux builds.

Recommended next step:

- Start replacing ad hoc runtime subsystems with the new vendor-backed families one seam at a time:
  - move async timer/wake infrastructure toward `io.loop` / `io.timers`
  - add first real `script.quickjs` module-loading and host-bridge hooks
  - add a Kain-owned `audio.*` API above the miniaudio lane
  - decide whether allocator selection becomes runtime-configurable before wiring mimalloc/rpmalloc into shared allocation helpers

## 2026-04-11 - vendor-edit harvest plan added for third-party runtimes under runtime/3rdparty

The repo now has an explicit plan for how imported third-party runtimes should strengthen Kain without becoming the runtime's source of semantic truth.

What changed:

- Added `runtime/KAIN_RUNTIME_HARVEST_PLAN_2026-04-11.md`
  - Defined the operating model for `runtime/3rdparty/` as `vendor, complete, patch, wrap, validate`.
  - Made the intended workflow explicit: edit vendored runtime files in place for Kain rather than embedding them unchanged or rewriting them from scratch.
  - Classified the current third-party imports by practical usefulness:
    - `quickjs`: strongest real integration candidate and the first recommended active service family
    - `wren`: promising embed API, but current import is incomplete
    - `mruby`: useful architecture source, but current import is incomplete
    - `lua`: valuable VM/data-structure reference, but current import is incomplete
    - `cpython/dictobject.c`: idea source for maps/dicts, not a runtime integration path by itself
  - Defined Kain-owned target families around those imports such as `script.quickjs`, `script.wren`, `script.mruby`, `script.lua`, and `data.dict.experimental`.
  - Recorded the first execution order: provenance/completeness audit, QuickJS-first integration, data-structure harvest from Lua and CPython dict behavior, then explicit completion-or-prune decisions for the half-imported runtimes.

Design decisions:

- Chose a vendor-edit strategy instead of a clean-room rewrite strategy because the imported runtimes already contain useful mature machinery that Kain can adapt faster than recreating from zero.
- Preserved the rule that Kain still owns semantics, the kernel, diagnostics, tracing, permissions, and public extension contracts.
- Positioned QuickJS as the first active dynamic scripting/service-runtime candidate because it is the only current third-party tree that looks close to a usable embeddable drop.

Current risks:

- `runtime/3rdparty/` is still in an ambiguous repo state: several trees look partial, no shared inventory or patch ledger exists yet, and provenance/build completeness is not normalized.
- Editing vendor code in place will pay off only if patch provenance and conformance are kept disciplined; otherwise the trees will decay into untraceable forks.
- The new plan still depends on a future unified Kain runtime value ABI and Kain-owned script-runtime contract so foreign runtimes do not become ad hoc boundary APIs.

Recommended next step:

- Add `runtime/3rdparty/INVENTORY.md` and a first QuickJS integration spec so the repo distinguishes clearly between buildable vendor lanes, reference-only imports, and the first real harvested runtime service.

## 2026-04-11 - phase2 selfhost advanced through parser, borrow, variant, and builtin-method blockers but is still not green

Phase2 selfhost is materially further forward than the earlier parser-wall state, but it is not fully green yet. The work in this pass pushed the generated `kain-core.kn` through several real language and selfhost-repair seams and left the current front blocker at a smaller builtin-surface gap instead of the earlier structural failures.

What changed:

- Updated `crates/kain-import/src/rust/transformer.rs`
  - Resolved impl `Self` more aggressively across receivers, return types, value/type paths, and pattern variant heads.
  - Added enum-constructor-aware lowering so explicit enum variant values and constructor references lower correctly, including lambda synthesis for higher-order constructor values.
  - Preserved borrowed `for` iterables and added explicit borrowed-field-base deref insertion plus borrow-adapter lowering (`as_ref`, `as_mut`, `as_deref`, `as_deref_mut`).
  - Normalized const/static borrowed slices into owned array-ish shapes for selfhost import.
- Updated `crates/kain-core/src/types.rs`
  - Added bidirectional lambda inference for higher-order call/method sites.
  - Fixed borrowed pattern matching so borrowed enum payloads stay borrowed.
  - Allowed field access through borrowed struct/pointer wrappers.
  - Preserved named-field identity for struct-style enum variants in both pattern binding and enum construction checks instead of positionally zipping fields.
  - Added builtin method typing for `Option.map`, `Option.unwrap_or`, `Result.map`, `Result.map_err`, `Result.unwrap_or`, and `Array.contains`.
  - Normalized empty tuple literals `()` to `Unit`.
  - Added focused regressions for borrowed field access, borrowed struct-variant binding by field name, higher-order `Result.map`, empty tuple unit inference, and array `contains`.
- Updated `crates/cli/src/selfhost.rs`
  - Added a named-function selfhost repair for `extract_compute_metadata_from_comptime_block(...)` so the generated Kain stays statement-shaped instead of leaking `none`-typed expression arms into a unit-only control-flow position.

Validation completed:

- `cargo test -q -p kain-import preserves_borrowed_for_loop_iterables -- --nocapture`
- `cargo test -q -p kain-import lowers_builtin_variant_constructor_values_to_lambdas -- --nocapture`
- `cargo test -q -p kain-import lowers_explicit_tuple_variant_constructor_values_to_lambdas -- --nocapture`
- `cargo test -q -p kain-import lowers_explicit_enum_variant_values_in_expression_position -- --nocapture`
- `cargo test -q -p kain-core typecheck_for_loops_over_borrowed_arrays_as_borrowed_items -- --nocapture`
- `cargo test -q -p kain-core typecheck_infers_lambda_parameters_from_expected_function_type -- --nocapture`
- `cargo test -q -p kain-core typecheck_infers_lambda_arguments_for_higher_order_methods -- --nocapture`
- `cargo test -q -p kain-core typecheck_borrowed_variant_patterns_bind_borrowed_payloads -- --nocapture`
- `cargo test -q -p kain-core typecheck_allows_field_access_through_borrowed_structs -- --nocapture`
- `cargo test -q -p kain-core typecheck_binds_struct_variant_fields_by_name_under_borrow -- --nocapture`
- `cargo test -q -p kain-core typecheck_infers_result_map_callback_types -- --nocapture`
- `cargo test -q -p kain-core typecheck_treats_empty_tuple_literal_as_unit -- --nocapture`
- `cargo test -q -p kain-core typecheck_allows_array_contains -- --nocapture`
- repeated `cargo run -q -p cli --bin kain -- selfhost phase2 --inventory-dir ouroboros/docs/selfhost/inventories --output-dir /tmp/kain_phase2_probe_after_patch*`

Design decisions:

- Fixed the real type/import semantics where the language surface was clearly missing something broadly useful, instead of papering over every phase2 blocker in the repair lane.
- Used the selfhost named-function repair only where the generated shape itself was clearly the wrong control-flow form for Kain (`extract_compute_metadata_from_comptime_block`).
- Kept generic selfhost strictness intact; this pass widened well-scoped language support rather than weakening type safety globally.

Current risks:

- Phase2 still is not green. After the latest pass, the current front blocker is in generated `kain-core.kn` around `COMPUTE_PLAN_BINDING_NAMES.contains(&name)`: the checker now accepts `Array.contains`, but the call site is still producing a deeper borrowed argument shape (`&&String`) and likely needs either another small builtin-method compatibility expansion or importer-side borrow cleanup.
- The selfhost lane is still surfacing method-surface gaps one family at a time, so more builtin collection/option/result/string methods may still need to be modeled before the stage2 workspace build becomes the active blocker.
- The worktree also contained unrelated user-side changes in `runtime/conformance/platform_parity/*`; those were left untouched.

Recommended next step:

- Continue phase2 from the current `Array.contains(&name)` seam, then keep rerunning `kain selfhost phase2` until the next front blocker moves from semantic typing to stage2 workspace assembly/build failures.

## 2026-04-11 - dated runtime expansion roadmap added for self-hosting, MCP, web, graphics, audio, and foreign runtime growth

A new dated runtime planning document now captures the desired long-range direction for the native/runtime stack under the repo's current private, AI-developed assumptions.

What changed:

- Added `runtime/KAIN_RUNTIME_EXPANSION_ROADMAP_2026-04-11.md`
  - Reframed the runtime target away from a narrow native substrate and toward a capability-driven runtime kernel intended to support self-hosting, web/runtime services, MCP, large 3D/graphics systems, audio/DAW execution, comptime growth, and a foreign-runtime mesh.
  - Defined a phased roadmap covering runtime constitution, kernel/scheduler work, unified runtime values and memory, platform and web services, MCP runtime families, graphics/scene/compute, audio/media, UI/workspace hosting, foreign language integration, self-hosting/comptime, and hardening.
  - Added a foreign-language viability study anchored to official Go, Rust, and Zig documentation.
  - Recorded the strategic language split:
    - Rust and Zig are the strongest Tier 1 in-process runtime-extension candidates.
    - Go is useful but should primarily be treated as a sidecar/service/WASI language rather than the default deep in-process kernel language.
    - WASM should become a first-class sandboxed extension lane once the runtime value ABI is mature.

Design decisions:

- Treated the repo's private/unreleased status as explicit permission for aggressive runtime and ABI refactors before any deliberate v1 freeze.
- Positioned the next big architectural jump as a kernel/value/module-system problem, not just a feature backlog problem.
- Kept the foreign-runtime recommendations contract-first: generated schemas and canonical service families should own the language boundaries rather than bespoke bridge logic.

Current risks:

- The roadmap is intentionally aggressive and assumes the project will tolerate large refactors across compiler, runtime, driver, and bridge layers.
- The current runtime still has real architectural gaps relative to the new target, especially around a unified value model, a stronger scheduler/kernel, cross-platform host services, deep graphics execution, and an audio/DAW subsystem.
- Zig references currently point at the official `master` language reference; production adoption should pin to stable Zig releases when concrete implementation work starts.

Recommended next step:

- Turn the roadmap's first two phases into concrete implementation specs for the runtime constitution, the unified runtime value ABI, and the scheduler/kernel refactor, because those three decisions will constrain every later web/MCP/graphics/audio/foreign-runtime lane.

## 2026-04-11 - LLVM/native validation is now stricter, helper-owned realloc is truthful, and the fixture lane proves real generated executables

The LLVM/native lane was hardened from a mostly compile-and-link story into a stricter, test-backed path with real executable proof.

What changed:

- Updated `crates/kain-core/src/parser.rs`
  - Fixed `send ...` parsing to accept the real `MethodCall` AST shape produced by postfix parsing instead of only the older `Call(Field(...))` shape.
  - This matters because executable LLVM actor fixtures now rely on authored `send actor.Message()` syntax instead of synthetic AST-only coverage.
- Updated `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Kept alloc/realloc lowering strict: raw `alloc` / `realloc_mem` must already be lowered into canonical helper calls, and helper call arity is exact instead of permissive.
  - Kept `print` / `println` fail-fast in LLVM until runtime semantics are exact.
  - Fixed LLVM actor spawn lowering so actor field 0 (`__mailbox`) is initialized with `mq_new()` before `KAIN_spawn`. Without that, generated actor binaries crash when `send` lowers to `mq_push`.
- Updated `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - The alloc/realloc regression is green.
  - Added stronger IR assertions for canonical helper signatures and pointer coercions.
  - Added explicit failure coverage for unsupported `println`.
  - Added actor spawn coverage proving mailbox allocation is emitted.
- Updated `runtime/native/src/core/kain_runtime_memory.c` and `runtime/native/include/kain_runtime_memory.h`
  - Added helper-owned allocation metadata adjacent to payloads.
  - `__kain_alloc` now records total payload size.
  - `__kain_realloc` now preserves bytes, zero-fills only newly exposed bytes when requested, guards size overflow, treats `NULL` as alloc, and fails in a controlled way for invalid/non-helper pointers instead of pretending correctness.
- Updated `runtime/fixtures/validate_all.sh`
  - The fixture lane now distinguishes startup fixtures from executable LLVM fixtures.
  - LLVM fixtures are compiled, linked, checked for required IR evidence, executed, and validated by deterministic exit code.
- Added executable LLVM fixtures under `runtime/fixtures/`
  - `llvm_heap_memory/` proves heap alloc/realloc zero-growth semantics.
  - `llvm_actor_message/` proves actor bootstrap plus mailbox send execution.
  - `llvm_world_pipeline/` proves world/patch/converge/orchestrate execution in a linked LLVM/native binary.
- Updated runtime docs, validation docs, `ARCHITECTURE.md`, and this memory file
  - The repo now explicitly distinguishes:
    - backend IR/codegen proof,
    - runtime-native conformance harness proof,
    - end-to-end generated LLVM executable proof.
  - Docs also now state that `runtime/conformance/run_all.sh --backend llvm` is not the executable LLVM proof lane.

Validation completed:

- `cargo test -p kain-sys-codegen --test llvm_codegen_test -- --nocapture`
- `./runtime/fixtures/validate_all.sh`
- `./runtime/validate_native_runtime.sh`

Validation notes:

- `cargo test -p kain-core --test ptr_type_test -- --nocapture` still reports unrelated pre-existing failures in the TS-memory lane (`ts_backend_validation_rejects_raw_memory_ops`, `ts_memory_lowering_resolves_alignof_and_storage_nodes`, `ts_memory_lowering_uses_union_layout_metadata`). Those failures are outside this LLVM/native hardening slice.
- Full-repo `cargo fmt` is still blocked by pre-existing trailing whitespace in `crates/ue5-shaders/src/validation.rs`, so this pass used targeted `rustfmt` on the files touched by the LLVM/runtime work.

Design decisions:

- Kept the public helper ABI names/signatures unchanged; hardened semantics behind the existing ABI instead of widening the surface.
- Scoped truthful realloc semantics to helper-owned allocations only. Foreign pointers still fail fast instead of getting a false correctness guarantee.
- Used fixtures for real LLVM proof instead of trying to make the native runtime conformance harness pretend it was executing generated LLVM programs end to end.
- Kept the actor executable fixture minimal and focused on spawn/send execution instead of pulling in broader actor-state semantics that are not required for this proof lane.

Current risks:

- The executable LLVM proof lane now exists, but it is still targeted. It proves heap helpers, actor bootstrap/send, and compiler-owned intent execution, not broad full-language parity.
- The `send` parser path is now aligned with the real AST shape, but older docs/examples elsewhere in the repo may still show stale actor syntax assumptions.
- `__kain_realloc` correctness is now real for helper-owned allocations, but broader foreign allocator interop remains intentionally unsupported.

Recommended next step:

- If LLVM hardening continues, extend the executable fixture lane before widening conformance claims: add one negative executable or CLI-level proof for strict LLVM failure cases, then decide whether more actor semantics or additional helper ABI surfaces deserve end-to-end fixtures.

## 2026-04-11 - cookiecutter lab now bundles quine, Game of Life, Mandelbrot, and a tiny Lisp into one Kain benchmark artifact

The repo now has a single `labs/cookiecutter` benchmark lane meant to stress Kain across metaprogramming, stateful simulation, math-heavy looping, and recursive data-structure evaluation in one place.

What changed:

- Added `labs/cookiecutter/main.kn`
  - Implemented a combined Kain program that:
    - generates a standalone quine source and verifies key structural markers
    - simulates Conway's Game of Life on a flattened 24x16 grid with explicit double-buffering
    - renders an ASCII Mandelbrot set with fixed-point integer math for interpreter-safe execution
    - evaluates a tiny closure-capable Lisp with lists, hashes, arithmetic, `define`, and `lambda`
  - Added an HTML showcase/report path so the lab reads as one bundled benchmark instead of four unrelated snippets.
- Added `labs/cookiecutter/README.md`
  - Documented the run command, output bundle, and the current runtime caveat.
- Added committed inspection artifacts under `labs/cookiecutter/outputs`
  - Included quine output, Life frame dump and SVG/PNG, Mandelbrot ASCII and SVG/PNG, Lisp report, and a combined showcase HTML/report.

Validation completed:

- `./target/debug/kain run /home/ephemara/Dev/Kain/labs/cookiecutter/main.kn`
- Successful native interpreter output currently reports:
  - `source_bytes=3985`
  - `alive_generation_0=16`
  - `alive_generation_7=31`
  - `max_iterations=32`
  - `(add-seven 35)=42`
- Verified committed quine artifacts are byte-identical and 3985 bytes each.
- Rendered and visually checked the committed Game of Life and Mandelbrot PNG previews.

Design decisions:

- Used fixed-point Mandelbrot math instead of float-heavy casting because the current Kain runtime lane is still fragile around some int/float coercion patterns.
- Kept the Game of Life grid explicit and double-buffered instead of compressing state into clever bit-packing; this lab is meant to prove semantics and readability first.
- Made the Lisp interpreter intentionally small but real: closures, nested evaluation, lists, and hash-shaped data are present without turning the lab into a full language project.

Current risks:

- In this lab, authored `@extern fn write_file(path: String, content: String) -> Unit` calls do not currently materialize new files reliably under `kain run`, even though the benchmark computation and console summaries succeed. The committed `outputs/` bundle is therefore the current inspection surface.
- The file-I/O contract across Kain still appears split: runtime/native code returns result-shaped write failures, while authored surfaces in some places still present `Unit`/direct-string contracts.

Recommended next step:

- Unify the authored and native runtime contracts for `write_file` and `read_file`, then rerun `labs/cookiecutter/main.kn` as a direct end-to-end artifact emitter so the committed outputs can become generated truth rather than companion artifacts.

## 2026-04-11 - native interpreter control-flow and builtin surface are more consistent, and the current Brainfuck lab is now failing on bad expectations rather than the original language bug

The Brainfuck investigation produced one real interpreter bug, one real builtin-surface mismatch, and one misleading test-harness problem.

What changed:

- Updated `crates/kain-core/src/runtime.rs`
  - Fixed native interpreter `elif` execution by teaching `Expr::If` to evaluate `ElseBranch::ElseIf` chains instead of silently dropping them.
  - Added native method dispatch for `String.len()` so the runtime matches the existing typechecker surface for string length.
  - Aligned interpreter `char_at` with the existing native runtime/string contract by returning an empty string for negative or out-of-range access instead of interpreter-only `none`.
- Updated `crates/kain-core/src/stdlib.rs`
  - Added missing stdlib registry entries for `ord` and `chr` so the registry/documented builtin surface matches the runtime/type globals more closely.
- Added `crates/kain-core/src/runtime_tests.rs` and wired it from `crates/kain-core/src/lib.rs`
  - Added focused regressions covering `elif` execution, `String.len()` runtime support, `char_at` out-of-range behavior, and stdlib exposure of `ord`/`chr`.

Validation completed:

- `cargo test -p kain-core runtime_tests -- --nocapture`
- inline interpreter repro now returns `2` for a direct `if/elif` chain on `opcode == "+"`
- inline interpreter repro now returns `3` for `"abc".len()`
- inline interpreter repro now returns an empty string for `char_at("abc", 99)`
- `cargo run -q -p cli -- run labs/brainfuck/main.kn`
- independent local reference Brainfuck execution confirmed that the current fixture programs themselves produce:
  - `hello_world.bf` -> `A[bbe\x1dPehbZ\x1e\t`
  - `echo_input.bf` -> `K`
  - `alphabet_pair.bf` -> `9:`

Design decisions:

- Did not force a repo-wide type-surface migration for `read_file` or `char_at` yet. Existing Kain sources use both direct-string and result-style assumptions, so the safe first step was to fix the interpreter/runtime mismatches that were clearly on the critical path without breaking broad call-site surface area.
- Treated the Brainfuck lab as a semantics probe, not as infallible truth. After the interpreter fixes, the remaining failures came from incorrect expected outputs in the harness, not from disagreement with an independent Brainfuck interpreter.

Current risks:

- `read_file` is still a deeper contract split across the repo: some surfaces assume `String`, others declare `Result<String, String>`, and the runtime behavior is not consistently modeled end-to-end yet.
- The LLVM lane remains a separate blocker for any clean “Brainfuck proves Kain” claim; this pass only addressed the native interpreter/runtime semantics.
- The current Brainfuck fixture names/expected outputs are misleading for proof purposes until they are corrected or the fixture programs are replaced with canonical ones.

Recommended next step:

- Decide the canonical Kain contract for file/string builtins, especially `read_file`, then align runtime, typechecker, stdlib declarations, and any existing Kain wrappers together in one deliberate pass. After that, either fix the Brainfuck harness expectations or replace the fixtures with canonical programs before using it as a Turing-completeness proof artifact.

## 2026-04-11 - Ouroboros selfhost tooling now resolves repo roots from the current checkout and can run its main control-plane paths on Linux without `M:\Code\...` defaults

The Ouroboros/selfhost control plane had drifted into a Windows-only operator loop: absolute `M:\Code\...` roots were baked into the manifest runner, repair tooling, inventory extraction, status scripts, and operator docs, and the phase runner still launched steps through `cmd /c` and PowerShell-specific lanes.

What changed:

- Added `ouroboros/tools/ouroboros_pathing.py`
  - Introduced shared workspace discovery for `repo_root` and `ouroboros_root`.
  - Supports explicit `OUROBOROS_ROOT` / `KAIN_REPO_ROOT` overrides but otherwise resolves from the current checkout layout.
  - Added platform-aware executable-name handling for `kain` vs `kain.exe`.
- Updated `ouroboros/tools/selfhost_pipeline/run_pipeline.py`
  - Replaced hardcoded default manifest/output roots with checkout-relative resolution.
  - Added manifest-default template expansion seeded from discovered roots plus `sys.executable`.
  - Reworked step execution around structured `argv` lists, optional working directories, and artifact-log emission instead of `cmd /c` / PowerShell launch assumptions.
  - Made stage2 binary detection check both Linux and Windows binary names.
- Updated `ouroboros/docs/selfhost/pipeline_manifest.json`
  - Replaced absolute drive-letter defaults with `{ouroboros_root}` templates.
  - Converted `analyze`, `phase2-core`, and `phase2-full` steps to data-driven `argv` execution with explicit `cwd` and `artifact_log` fields.
  - Removed the PowerShell-script dependency from the core/full lane definitions by letting the runner execute Cargo checks directly.
- Updated `ouroboros/tools/selfhost_repair/repair_runner.py` and `ouroboros/scripts/extract_selfhost_inventory.py`
  - Switched default roots to the shared resolver so repair and inventory extraction follow the active checkout instead of a fixed Windows path.
- Added `ouroboros/scripts/selfhost_workspace_status.py`
  - Ported the machine-readable workspace status surface to Python so Linux operators no longer depend on `selfhost_workspace_status.ps1`.
- Updated `ouroboros/automation/config/pipeline.config.json`, `ouroboros/tools/selfhost_pipeline/README.md`, `ouroboros/tools/selfhost_repair/README.md`, `ouroboros/docs/selfhost/phase2-current-status.md`, and `ouroboros/docs/selfhost/repairs/repair_workflow.md`
  - Repointed operator commands and path references to repo-relative, Linux-safe entrypoints.
- Updated the legacy PowerShell helpers under `ouroboros/scripts/*.ps1`
  - They still exist for Windows users, but now derive their defaults from `PSScriptRoot` instead of fixed `M:\Code\OuroborosV2\...` paths and recognize both `kain` and `kain.exe`.

Validation completed:

- `python -m py_compile ouroboros/tools/ouroboros_pathing.py ouroboros/tools/selfhost_pipeline/run_pipeline.py ouroboros/tools/selfhost_repair/repair_runner.py ouroboros/scripts/extract_selfhost_inventory.py ouroboros/scripts/selfhost_workspace_status.py`
- `python ouroboros/tools/selfhost_pipeline/run_pipeline.py list`
- `python ouroboros/scripts/selfhost_workspace_status.py`
- `python ouroboros/tools/selfhost_repair/repair_runner.py analyze --validation skip --input-root ouroboros/out/selfhost/phase2 --repaired-root /tmp/kain_phase2_repaired_test --repair-docs ouroboros/docs/selfhost/repairs --report-json /tmp/kain_phase2_repair_report.json --report-md /tmp/kain_phase2_repair_report.md`
- `python -m json.tool ouroboros/docs/selfhost/pipeline_manifest.json`
- `python -m json.tool ouroboros/automation/config/pipeline.config.json`
- `python ouroboros/tools/selfhost_pipeline/run_pipeline.py run --lane analyze --out-dir /tmp/kain_pipeline_out`
  - this now resolves roots correctly on Linux and fails honestly on missing `phase2` artifacts instead of on bad Windows path assumptions

Design decisions:

- Treated the manifest as the long-term source of truth for lane execution and moved more behavior into structured `argv` / `cwd` / `artifact_log` fields instead of scattering platform-specific wrapper scripts.
- Kept the PowerShell helpers for Windows continuity, but removed them from the main manifest-driven Linux operator path.
- Used checkout-relative discovery first and env overrides second so local development works with no extra setup while CI or split-repo operators can still pin roots explicitly.

Current risks:

- Some older Ouroboros reports and secondary docs still mention `M:/Code/...`; the live tooling path is fixed, but not every archival note has been normalized.
- The manifest runner now owns more of the Cargo check flow directly, so future step kinds should stay structured instead of slipping back toward shell-string wrappers.
- There are still unrelated absolute-path references elsewhere in the broader repo outside the main Ouroboros selfhost control plane.

Recommended next step:

- If Linux is now the primary dev lane, continue normalizing the remaining live non-Ouroboros absolute-path assumptions in runtime tests and helper tooling, but keep that as a separate pass from the now-working selfhost control plane.

## 2026-04-11 - phase1 selfhost is no longer blocked by the opaque `dyn Any + Send + Sync` host-object carrier in `kain-core`

Phase1 strict selfhost had collapsed to a single remaining blocker in `kain-core`: the Rust importer treated the runtime's opaque host-object carrier (`Arc<dyn Any + Send + Sync>`) as a fatal trait-object erasure even though this seam is intentionally just an opaque payload boundary, not a dynamic-dispatch trait API that phase1 needs to preserve exactly.

What changed:

- Updated `crates/kain-import/src/rust/transformer.rs`
  - Added `trait_object_is_opaque_any_carrier(...)` to detect the narrow `dyn Any` carrier family, including wrapped forms like `Arc<dyn Any + Send + Sync>`.
  - Stopped emitting `trait_object_lowering` diagnostics for that narrow family while leaving the fatal diagnostic in place for real dynamic-dispatch trait-object lowering such as `dyn std::fmt::Write`.
  - Added a regression proving the importer stays strict for normal dyn traits but does not flag the opaque Any carrier.

Validation completed:

- `cargo test -p kain-import records_dyn_trait_lowering_diagnostics -- --nocapture`
- `cargo test -p kain-import does_not_flag_opaque_any_trait_object_carriers -- --nocapture`
- `cargo run -q -p cli --bin kain -- selfhost phase1 --inventory-dir ouroboros/docs/selfhost/inventories --output-dir /tmp/kain_phase1_probe`

Design decisions:

- Did not weaken the selfhost allowlist or globally permit trait-object lowering.
- Treated only the `Any` carrier seam as acceptable because phase1 uses it as opaque host-state transport, not as trait-object behavior that must survive into Kain semantics.

Current risks:

- This makes phase1 honest for the current `kain-core` host-object seam, but it does not mean Kain now has general `dyn trait` semantics.
- If more trait-object families appear in the selfhost slice later, they should be handled case-by-case instead of by broadening this exception.

Recommended next step:

- Push phase2 again after phase1 goes green; the next blockers should now be real downstream import/codegen/workspace issues rather than the last phase1 strict-import policy tripwire.

## 2026-04-11 - compiler-owned intent suite is now coherent across parser, typechecker, runtime, bundles, driver, and LSP

The original quartet is no longer the right mental model. Kain now ships a five-part compiler-owned intent suite: `law`, `patch`, `converge`, `world`, and `orchestrate`.

What changed:

- Updated `crates/kain-core/src/ast.rs`, `parser.rs`, `types.rs`, `runtime.rs`, `runtime_contract.rs`, and `realtime_app_bundle.rs`
  - Added first-class `law` declarations and runtime-callable `Value::Law`.
  - Tightened `law` semantics to Bool-returning invariant declarations emitted into `laws[]` in both bundle families.
  - Removed world-state name leakage from the global type environment; world state is now accessed only through the world value.
  - Relaxed `world` validation from “all four surfaces required” to “at least one surface required,” while keeping duplicate-surface rejection.
  - Made `converge` selector-less `fast` lanes valid declaration-ordered defaults.
  - Made `verify random(n)` executable for real call args plus deterministic synthesized samples, with a hard typecheck fence around the supported scalar/tuple/array/option subset.
  - Tightened `orchestrate` into a strict top-level typed pipeline and removed silent Rust/Python/Node fallback into normal Kain function dispatch.
- Updated `crates/kain-core/tests/compiler_owned_intent_test.rs`
  - Added coverage for `law`, world-state leakage, sparse worlds, selector-less fast lanes, executable converge verification, strict orchestrate rejection cases, and runtime-label enforcement.
- Updated `crates/kain-driver/src/lib.rs` and `crates/kain-driver/src/native_app.rs`
  - Replaced native-ui-only world selection heuristics with target-aware world selection.
  - Native desktop targets resolve against `native_ui`, web targets against `web`, and UE5 targets against `ue5`.
  - Explicit `--root` world selection now fails if the selected world does not declare the required surface for the active adapter target.
- Updated `crates/cli/src/lsp.rs`
  - Added `law` to keyword/symbol surfacing and kept the full intent-suite declaration set visible to editor tooling.
- Updated `docs/kainplan/08_COMPILER_OWNED_INTENT_QUARTET.md` and `ARCHITECTURE.md`
  - Reframed the old quartet doc into the current intent-suite contract.
  - Recorded the new five-declaration doctrine and the target-aware world-selection behavior.

Validation completed:

- `cargo test -p kain-core --test compiler_owned_intent_test`
- `cargo test -p kain-driver --lib world`
- `cargo check -p cli`

Design decisions:

- Kept the declarations contextual rather than globally reserving `law`, `patch`, `converge`, `world`, and `orchestrate`.
- Chose to make `verify random(n)` honest and executable now instead of leaving it as metadata-only aspiration.
- Chose to make `orchestrate` stricter instead of trying to infer bundle metadata from arbitrary expression shapes.

Current risks:

- The parser still requires at least one `fast` lane in every `converge`, even when a `spec`-only form could be meaningful later.
- `verify random(n)` is intentionally bounded to a scalar-centric synthesis subset; richer structural generation is still future work.
- The full `cargo test -p kain-driver --lib` suite still contains unrelated package/network-heavy failures outside this feature lane, so the validated surface here is the focused world-selection/native-app path plus the full core intent suite.

Recommended next step:

- Decide whether to relax the “at least one fast lane” parser rule for `converge`, and if so, align that parser choice with bundle emission, runtime dispatch, and compiled/codegen lanes together.

## 2026-04-11 - LLVM now lowers world, patch, converge, and orchestrate directly enough for Omega to run without the wrapper path

The previous Omega note is no longer current. The LLVM backend in `crates/kain-sys-codegen` now materializes compiler-owned intent items directly instead of requiring `main()` to route around them.

What changed:

- Updated `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - Registered `world` items as real LLVM struct types plus singleton globals with once-only init functions.
  - Added function registration and body emission for `patch`, `converge`, and `orchestrate` items so they lower as callable symbols.
  - Added identifier lowering for world handles so `let omega = Omega` and `let studio = Studio` materialize the singleton pointer instead of failing as undefined locals.
  - Added `Expr::StageCall` lowering and direct-call reuse so `kain/rust/python/node fn(...)` stage syntax compiles through the LLVM lane when the function resolves locally.
- Updated `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - Added a focused regression covering `world` singleton init, `patch` mutation, `converge` lowering, `orchestrate` lowering, and direct `main()` execution through that path.
- Updated `labs/omega/omega.kn`
  - Removed the fallback `omega_pipeline_runtime(...)` route from `main()`.
  - `main()` now directly touches `Omega`, runs patch calls, and returns `omega_pipeline(...)`.
- Updated `crates/ue5/src/codegen_ue5.rs` and `crates/kain-host/src/lib.rs`
  - Fixed adjacent exhaustiveness fallout from the newer `Law` surface so local validation builds could proceed farther instead of failing on unrelated missing match arms.

Validation completed:

- `cargo test -p kain-sys-codegen llvm_generates_world_patch_converge_and_orchestrate_paths -- --nocapture`
- `./runtime/compile_native_runtime.sh`
- generated current LLVM for `/home/ephemara/Dev/Kain/labs/omega/omega.kn` through a local Rust harness using `kain-core` + `kain-sys-codegen`
- linked `/home/ephemara/Dev/Kain/labs/omega/generated/omega.ll` against `generated/native_runtime/debug/*.o`
- `/home/ephemara/Dev/Kain/labs/omega/generated/omega` exits with code `145`
- generated current LLVM for `/home/ephemara/Dev/Kain/smoketest/compiler_owned_intent/compiler_owned_intent.kn` and confirmed direct `Studio` world init lowering is present in `generated/compiler_owned_intent.ll`

Design decisions:

- Kept LLVM stage-call lowering simple for now: local `StageCall` lowers as a direct function call in the native lane. That is enough to unblock authored `orchestrate` flows like Omega while preserving the existing typed stage syntax in source.
- Treated `converge` as a direct callable lowering of its spec lane in LLVM. This keeps compiled behavior deterministic and aligned with the semantic contract until the backend grows capability-aware lane selection or verification instrumentation.
- Validated the backend through current-library harnesses and direct LLVM/native linking because the repo's `cli` package currently has separate feature-gating issues unrelated to this LLVM change.

Current risks:

- LLVM `StageCall` is now useful for local lowering, but it is not yet a true native bridge implementation of Python/Node/Rust host execution semantics.
- `converge` lowering still takes the spec lane directly; it does not yet encode runtime lane selection or verification sampling in native codegen.
- The `cli` crate still has unrelated package/feature issues around optional UE5 surfaces, so `cargo build -p cli` is not the clean validation path for this work today.

Recommended next step:

- Decide whether native LLVM should keep `StageCall` as a direct local-call lowering or grow explicit bridge shims for Python/Node/Rust runtime dispatch, then implement the same decision for `converge` lane selection so the compiled lane matches interpreter semantics more closely.

## 2026-04-11 - omega lab now compiles and runs in both interpret and LLVM lanes, with a runtime-safe entrypoint around current compiler-owned-intent LLVM gaps

The modernized `labs/omega/omega.kn` now works as an actual runnable lab file instead of only an interpreter-only proof.

What changed:

- Updated `labs/omega/omega.kn`
  - Kept the modern Omega thesis in current Kain syntax: `world`, `component`, `patch`, `converge`, `orchestrate`, actor syntax, and regular structs/functions.
  - Added a plain `omega_pipeline_runtime(...)` path used by `main()` so the file can compile all the way through the current LLVM/native executable lane.
  - Kept the compiler-owned declarations in the file for runtime-contract / realtime-bundle emission and authored-surface proof, while avoiding the exact call paths that the LLVM backend does not lower yet.

Validation completed:

- `./target/debug/kain run /home/ephemara/Dev/Kain/labs/omega/omega.kn` -> `145`
- `./runtime/compile_native_runtime.sh`
- `./target/debug/kain build /home/ephemara/Dev/Kain/labs/omega/omega.kn --target rust --output /home/ephemara/Dev/Kain/labs/omega/generated/omega.rs`
- `./target/debug/kain build /home/ephemara/Dev/Kain/labs/omega/omega.kn --target llvm --output /home/ephemara/Dev/Kain/labs/omega/generated/omega.ll`
- `/home/ephemara/Dev/Kain/labs/omega/generated/omega` exits with code `145`

Design decisions:

- Treated the old file as a thesis to port, not as syntax to transliterate.
- Chose to preserve `world` / `patch` / `converge` / `orchestrate` in the authored file, but routed `main()` through plain functions for the compiled lane because the LLVM backend currently fails on direct `world` handle materialization and direct `orchestrate` call lowering.

Current risks:

- This is a real current backend gap, not an Omega-specific bug: the existing compiler-owned-intent smoke also fails LLVM codegen when `main()` does `let studio = Studio`, and direct `orchestrate` calls in the compiled lane can lower to undefined symbols during link.
- Omega therefore proves the mixed semantic surface and the native executable lane together, but not yet direct end-to-end execution of compiler-owned intent through LLVM.

Recommended next step:

- Fix LLVM/codegen support for world handle materialization and orchestrate entrypoint emission so authored `world` / `patch` / `converge` / `orchestrate` can execute directly in native compiled binaries without the runtime-safe wrapper path.

## 2026-04-11 - architecture doc now states plainly that Kain is an executable language, not only a manifest/orchestration layer

The durable repo overview had drifted toward the packaging/materialization story and was underselling what `crates/kain-core` already does directly.

What changed:

- Updated `ARCHITECTURE.md`
  - Reframed Kain as a compiled multi-target language toolchain, executable semantic runtime, and embeddable host stack.
  - Added an explicit semantic execution flow section describing `kain-core` as a real execution lane for functions, blocks, closures, control flow, async/await, actors, UI expression evaluation, and runtime execution of `patch` / `converge` / `orchestrate`.
  - Tightened the language around host bridges so future agents do not mistake external adapters for proof that Kain itself is only config glue.
  - Added guardrails stating that Kain-expressible logic should remain in Kain unless the capability is truly platform- or ecosystem-owned.

Design decisions:

- Anchored the architecture wording to actual `kain-core` behavior rather than repo-wide packaging ambitions, because future agents were at risk of reasoning from the wrong center of gravity.
- Kept the bundle/materialization story intact, but repositioned it as downstream consumption of compiler/runtime truth instead of the whole identity of the language.

Current risks:

- The repo still contains a large amount of packaging, adapter, and target-specific work, so future docs can drift back toward an adapter-first framing if they are written from `kain-driver` outward instead of from `kain-core` semantics outward.
- Some ambitious domains are still partly bridge-driven, so agents need to distinguish "Kain can execute real logic" from "every subsystem is already first-class native syntax."

Recommended next step:

- When future major language/runtime features land, record both the in-language execution surface and the emitted bundle/adapter surface together so the repo overview stays balanced.

## 2026-04-11 - kain-3d now has a first-class authored primitive pipeline with stable resource ids

The 3D runtime no longer treats primitives as a few isolated helper meshes spread across scene setup and host wrappers.

What changed:

- Added `crates/kain-3D/src/primitive.rs`
  - Introduced `PrimitiveShape`, `PrimitiveDefinition`, and `PrimitiveLibrary` as the crate-owned authored primitive seam.
  - Added high-fidelity procedural builders for plane, box, UV sphere, quad sphere, cylinder, cone, capsule, and torus.
  - Aligned the library with the existing DCC mesh contract using stable `mesh://primitives/authored/*` resource URIs and `mesh://primitives/authored/definitions` as the document root.
- Updated `crates/kain-3D/src/authoring.rs`
  - Routed `Geometry::plane`, `Geometry::box_mesh`, and `Geometry::uv_sphere` through the shared primitive builder path instead of keeping duplicate local mesh logic.
  - Added `Geometry::cylinder`, `Geometry::cone`, `Geometry::capsule`, `Geometry::torus`, and `Geometry::quad_sphere`.
  - Added `Scene::add_primitive_definition(...)` and `Scene::add_primitive_library(...)` so authoring scenes can register primitive libraries with durable metadata.
- Updated `crates/kain-3D/src/host.rs` and `crates/kain-3D/src/prelude.rs`
  - Exposed the richer primitive set to authored Kain source through new `zen3d` runtime bindings for `quad_sphere`, `cylinder`, `cone`, `capsule`, and `torus`.
- Updated `crates/kain-3D/src/scene.rs`
  - Switched builtin cube, plane, and UV sphere scene meshes to consume the shared primitive pipeline instead of hand-maintained duplicate mesh builders.
- Updated `ARCHITECTURE.md`
  - Recorded that the authored primitive seam now belongs to `kain-3D` under the viewport/runtime contract lane.

Validation completed:

- `cargo test -p kain-3d --lib -- --nocapture`
- `cargo test -p kain-ui-native viewport_ -- --nocapture`

Design decisions:

- Chose a crate-owned primitive definition layer instead of only adding more `Geometry::*` helpers so future DCC authoring, scene contracts, and app-level mesh documents can point at stable primitive ids and URIs.
- Kept the runtime-facing host API function-based for now because it gives authored `.kn` code immediate access to the new shapes without first widening Kain reflection with a variant-heavy primitive descriptor schema.
- Reused the shared primitive builders inside `scene.rs` so cube/plane/sphere quality and winding rules only live in one place.

Current risks:

- The primitive pipeline is still CPU-authored geometry; there is not yet a GPU tessellation / displacement / remesh lane behind these definitions.
- The Kain-side prelude exposes constructors, but it does not yet expose the full `PrimitiveDefinition` / `PrimitiveLibrary` metadata model directly to `.kn` code.
- Subdivision-ready means topology intent and support-loop density today, not Catmull-Clark or ZBrush-class remeshing guarantees by itself.

Recommended next step:

- Make primitive definitions first-class bundle/runtime resources so Zen and app-level DCC lanes can select, instantiate, and edit authored primitive ids directly instead of flattening them immediately into anonymous meshes.

## 2026-04-10 - manipulator math moved into kain-3d and the host prelude now uses real extern 3D bindings

The 3D lane no longer leaves core viewport edit math stranded inside `kain-ui-native`.

What changed:

- Updated `crates/kain-3D/src/interaction.rs` and `crates/kain-3D/src/lib.rs`
  - Added `ManipulatorSnapSettings` plus `apply_manipulator_drag(...)` as the reusable 3D interaction contract.
  - Centralized screen-drag translation/rotation/scale math in `kain-3D` instead of keeping it host-local.
  - Added constrained drag handling for screen, axis, and plane manipulator modes, plus local-vs-world basis resolution and snap application.
  - Added focused tests for screen drag movement, snap behavior, axis scale positivity, and local-axis translation under object rotation.
- Updated `crates/kain-ui-native/src/lib.rs`
  - Switched the viewport manipulation path to call `kain-3D`’s shared drag helper instead of its own private transform math.
  - Kept `kain-ui-native` in the host-forwarding role: it still owns input capture and viewport chrome, but the transform result is now computed by the 3D crate.
- Updated `crates/kain-3D/src/prelude.rs`
  - Replaced fake Kain-bodied `__zen3d_*` helper implementations with `@extern fn` declarations that match the native bindings installed by `Kain3dSession`.
  - This fixed the existing `kain-3d` host test failure where the generated prelude was constructing placeholder 3D values instead of binding to the Rust-native helpers cleanly.
- Updated `ARCHITECTURE.md`
  - Recorded that manipulator drag math is now a `kain-3D` responsibility under the viewport contract lane.

Validation completed:

- `cargo test -p kain-3d --lib -- --nocapture`
- `cargo test -p kain-ui-native viewport_ -- --nocapture`

Design decisions:

- Chose a shared drag API first instead of adding more ad hoc viewport behaviors in `kain-ui-native`, because the current weakness was ownership drift more than missing host chrome.
- Kept the first integration using `ManipulatorAxis::Screen` in the native UI lane so the host change stays low-risk while the 3D crate can already support axis/plane-constrained drags for the next pass.
- Used `@extern fn` in the 3D prelude because the helper functions are truly native host bindings; fake Kain return bodies were hiding that seam and were fragile under typechecking.

Current risks:

- `kain-ui-native` still defaults to screen-space drag activation; it is not yet choosing gizmo axes from handle hits, so the richer axis/plane drag support in `kain-3D` is only partially exercised by the current host.
- Rotation still lands in Euler component updates, which is acceptable for the current transform model but not a final professional-grade manipulator representation.

Recommended next step:

- Use the GPU/CPU picking paths to identify active gizmo handles and feed real `ManipulatorAxis` selections into `apply_manipulator_drag(...)`, so the native viewport graduates from screen-drag transforms to actual axis/plane gizmo interaction.

## 2026-04-10 - intent_forge_quartet adds a real native 3D executable smoke for the compiler-owned intent system

The repo now has a richer visual proof for the compiler-owned intent quartet under `smoketest/3D/intent_forge_quartet`.

What changed:

- Added `smoketest/3D/intent_forge_quartet/smoke.kn`
  - Authors a studio-style native shell with a central `viewport3d`, left tool rack, right inspector rail, and bottom activity strip.
  - Uses `world IntentForge`, two `patch` declarations, one `converge`, and one `orchestrate` in the same executable app.
  - Keeps a deterministic `main()` result of `105` so `kain run` and the packaged native-ui path can be checked together.
- Added `smoketest/3D/intent_forge_quartet/run_smoke.py`
  - Runs `kain run` on the authored file.
  - Builds the packaged native-ui executable.
  - Verifies generated runtime contract, realtime bundle, and native app bundle artifacts.
- Added `smoketest/3D/intent_forge_quartet/launch_native_app.sh`
  - Linux-friendly launcher for rebuilding and opening the packaged executable.

Design decisions:

- The smoke intentionally uses a richer editor-like shell rather than the earlier minimal quartet proof so the feature is exercised inside a UI shape that reads like an actual DCC/tool app.
- The smoke stays inside currently proven native-ui and viewport primitives instead of inventing a separate rendering lane just for the quartet demo.

Current risks:

- The scene still uses the current generic viewport scene lane, so this is primarily a packaging-and-shell proof rather than a custom geometry-runtime proof.
- `smoketest/3D/` is globally ignored in the repo, so these source files need explicit force-add handling until the broader ignore policy is cleaned up.

## 2026-04-09 - Linux LLVM and raw-native runtime lanes now validate end-to-end

The native runtime's Linux surface is no longer blocked at the public-header and validation-harness level.

What changed:

- Updated `runtime/native/include/*` and `runtime/native/src/core/*`
  - Removed Win32-only outer gating from the shared runtime-contract, realtime, UI, asset, graphics, and UI-runtime headers so Linux builds can see the same ABI contract types.
  - Kept the Win32-specific host structs and platform host APIs gated, but moved generic helpers and shared ABI types into the cross-platform surface.
  - Replaced the Unix `usleep(...)` path in `kain_runtime_core.c` with `nanosleep(...)` so the runtime compiles cleanly under modern POSIX feature levels.
- Added Linux runtime support sources
  - `runtime/native/src/platform/linux/kain_runtime_linux_shared.c` now owns Linux env/path/vector helpers plus `_putenv_s`/`Sleep`-adjacent compatibility through the shared base shims.
  - `runtime/native/src/platform/linux/kain_runtime_linux_graphics.c` provides the Linux implementation of `kain_win32_gl_surface_supports_graphics_bundle(...)` so the graphics validation lane can stay source-compatible while the host-specific OpenGL path remains Windows-only.
- Hardened runtime validation and conformance on Linux
  - Normalized runtime shell scripts to LF and taught the fixture runner to prefer the repo-local `target/debug/kain` or `target/release/kain` before falling back to PATH.
  - Switched the native smoke fixtures to current frontend-valid `fn main() -> Int: return 0` programs and made the LLVM fixture path require the final executable, not just the emitted `.ll`.
  - Updated reflection, diagnostics, UI, graphics, and actor conformance harnesses so they compile and run against Linux sources instead of hard-coded Win32 helper objects.
  - Reworked `runtime/validate_native_runtime.sh` so it validates the actual Linux runtime loop: CLI build, native runtime compile, LLVM/raw-native fixtures, and full conformance.
- Updated user-facing/runtime metadata
  - `runtime/native_runtime.toml` and `runtime/native_runtime_metadata.json` now advertise Linux in the core raw-native lane and include the Linux-specific source set / thread dependency.
  - `crates/cli/src/main.rs` now prints Linux/macOS install-refresh guidance instead of a PowerShell-only message when the active binary comes from `target/`.

Validation completed on this Linux host:

- `cargo build -p cli`
- `./target/debug/kain doctor`
- minimal `kain build -t llvm` producing and running a native executable
- `./runtime/compile_native_runtime.sh`
- `./runtime/fixtures/validate_all.sh`
- `./runtime/conformance/run_all.sh`
- `./runtime/validate_native_runtime.sh`

Current risks:

- The core raw-native/LLVM/runtime-contract lanes are validated on Linux, but the platform-host services in `native_runtime.toml` (`platform.app-host`, `platform.input`, `gfx.viewport`) are still explicitly Windows-only. Linux support is real for the shared runtime substrate, not yet for the Win32 desktop host layer.
- Several runtime/conformance C files still emit warnings under clang on Linux, but the suite passes.

Recommended next step:

- Add a non-Win32 native host provider for app-host/input/viewport services so the higher-level packaged native desktop lane can advertise Linux parity without relying on Win32-only service entries.

## 2026-04-09 - compiler-owned formatter landed in kain-core and the CLI

Kain now has a first compiler-owned source formatter instead of relying on ad hoc source repair or manual style normalization.

What changed:

- Added `crates/kain-core/src/formatter.rs`
  - Introduced `FormatOptions`, `format_source`, and `format_source_with_options`.
  - The formatter parses source through the real frontend and prints canonical Kain syntax from the AST.
  - Script-mode formatting detects the parser's synthetic top-level `fn main()` wrapper and emits authored top-level statements instead of leaking the lowering detail back into user source.
  - Preserves BOM and shebang prologues.
- Updated compiler-facing surfaces
  - `crates/kain-core/src/lib.rs` now exports the formatter.
  - `crates/kain-driver/src/lib.rs` now exposes `DriverSession::format_source` plus a crate-level `format_source` helper.
  - `crates/cli/src/lib.rs` re-exports the formatter entrypoint for the command layer.
- Updated CLI
  - `crates/cli/src/main.rs` now exposes `kain format` with `kain fmt` alias plus `--check` and `--write`.
  - Default formatter mode prints canonical source to stdout without the normal compiler banner so it can be used safely in pipes and editor integrations.
- Added focused tests
  - `crates/kain-core/src/formatter.rs` now includes formatter tests for functions/structs, script-mode top-level statements, JSX/components, gameplay tags, and shebang preservation.

Design decisions:

- Kept formatting compiler-owned and AST-driven so editors, CLIs, and future LLM tooling all consume the same printer of record.
- Chose explicit runtime errors over lossy printing for grammar corners that the current frontend cannot round-trip safely.
- Kept v1 intentionally thin at the CLI layer: the command delegates to the driver/core formatter instead of owning any syntax rules.

Current risks:

- Comments are not preserved yet because the lexer discards comment tokens before parsing. `kain format` will currently remove authored comments.
- Some rare AST shapes still error intentionally in v1 instead of formatting, including empty blocks and a few non-round-trippable expression forms.
- `cargo test -p cli --lib -- --nocapture` still fails on an unrelated pre-existing test: `selfhost::tests::indent_repaired_block_matches_nested_selfhost_layout`.

Recommended next step:

- Add frontend trivia ownership so comments survive parse/format round-trips, then grow formatter coverage tests around more advanced constructs before pushing editors or auto-format-on-save flows to depend on it heavily.

## 2026-04-09 - root universal installer now bundles clang into the repo toolchain

The repo now has a root cross-platform bootstrap entrypoint at `install_kain.py`.

What changed:

- Added `install_kain.py`
  - Detects Linux, macOS, or Windows at runtime.
  - Resolves `clang` from the repo toolchain, `KAIN_CLANG_PATH`, PATH, or common platform install locations.
  - Falls back to platform package managers when `clang` is missing:
    - Linux: `apt-get`, `dnf`, `yum`, `pacman`, `zypper`, `apk`
    - macOS: `brew`
    - Windows: `winget`, `choco`, `scoop`
  - Bundles clang back into the repo under `toolchain/llvm/bin`:
    - Unix-like systems symlink the discovered LLVM tools into the repo-local toolchain bin dir.
    - Windows mirrors the relevant `clang` / `llvm` / `lld` executables and DLLs into the repo-local toolchain bin dir.
  - Writes `toolchain/llvm/kain_bundle_manifest.json` so future agents can see where the current bundled toolchain came from.
  - Builds `cargo build --release -p cli`, installs `kain` and `kn` into the cargo bin dir, and emits activation scripts under `generated/kain-env.sh` and `generated/kain-env.ps1`.
- Updated docs
  - `README.md` now points at the root installer as the first bootstrap step.
  - `toolchain/README.md` documents that the installer repopulates `toolchain/llvm/bin`.
  - `ARCHITECTURE.md` adds the new installer to common commands and fresh-clone guidance.

Design decisions:

- Kept the installer as a single root Python script so Linux, macOS, and Windows all share one bootstrap path.
- Chose repo-local clang bundling over env-only discovery because too much of the repo still assumes `toolchain/llvm/bin/clang*` exists.
- Emitted activation scripts instead of directly mutating user shell profiles in v1. That keeps the installer deterministic and avoids hidden shell-specific side effects.

Current risks:

- The package-manager install paths are best-effort. Some machines will still need manual LLVM setup, especially when `sudo`, `winget`, or `brew` is unavailable.
- Windows bundling currently mirrors the relevant LLVM bin files rather than managing a full versioned LLVM drop under `toolchain/llvm`.
- The older `scripts/sync-kain-source-of-truth.ps1` path still exists, so the repo now has both a Windows-specific sync script and the new universal installer until that consolidation happens.

Recommended next step:

- Make the Windows PowerShell sync path delegate to `install_kain.py` or share one manifest-driven bootstrap core so the repo only has one real installer contract.

## 2026-04-08 - compiler-owned intent quartet landed across parser, runtime, bundles, and driver root selection

Kain picked up the first full pass of the compiler-owned intent quartet: `patch`, `converge`, `world`, and `orchestrate`.

What changed:

- Updated `crates/kain-core/src/ast.rs`, `parser.rs`, and `types.rs`
  - Added new top-level item forms for `patch`, `converge`, `world`, and `orchestrate`.
  - Added `Expr::StageCall` for typed stage-runtime syntax such as `rust fn_name(...)`.
  - Added typed-item support, world surface validation, required-v1 world surfaces, patch mutation-path collection, patch undo-mode classification, converge signature checking, and orchestration stage descriptors.
- Updated `crates/kain-core/src/runtime.rs`
  - Registered and executed `patch`, `converge`, and `orchestrate` as real runtime values.
  - Added patch transaction recording with mutation paths and undo mode.
  - Added converge lane dispatch plus test-lane verification against `spec`.
  - Preserved concrete test failure messages in `run_tests` so converge mismatch diagnostics survive the harness boundary.
- Updated `crates/kain-core/src/runtime_contract.rs` and `realtime_app_bundle.rs`
  - Added explicit `patches[]`, `converges[]`, `worlds[]`, and `orchestrations[]` sections.
  - Added capability / requirement keys for `patch.transactions`, `converge.dispatch`, `world.native-ui`, `world.viewport3d`, `world.web`, `world.ue5`, and `orchestrate.pipeline`.
- Updated downstream consumers
  - `crates/kain-driver/src/lib.rs` and `crates/kain-driver/src/native_app.rs` now resolve native-ui roots from a single `world`'s `native_ui` surface and reject ambiguous multi-world inputs without an explicit selection.
  - `crates/web`, `crates/gpu`, `crates/kain-sys-codegen`, and `crates/ue5` were patched for the new `ResolvedType::Future` / `Expr::StageCall` / `TypedItem` exhaustiveness fallout so the feature compiles through the wider toolchain.
- Added focused validation
  - `crates/kain-core/tests/compiler_owned_intent_test.rs`
  - new driver/native-app unit coverage for single-world auto-root and multi-world rejection
  - `smoketest/compiler_owned_intent` plus an `allinone` manifest entry
  - `docs/kainplan/08_COMPILER_OWNED_INTENT_QUARTET.md`

Design decisions:

- Kept the new starters contextual at legal item boundaries instead of reserving them globally.
- Treated the quartet as bounded semantic declarations, not expression-wide grammar rewrites.
- Required all four `world` surfaces in v1 to keep projection coverage explicit instead of leaving partial adapter truth ambiguous.
- Kept `orchestrate` stage-runtime labels semantic in v1; the runtime still dispatches through existing function execution rather than invoking external bridges directly.

Current risks:

- The new feature lane is covered by focused tests, but full `cargo test -p kain-driver --lib` still includes unrelated long-running / networked / pre-existing failures outside this implementation slice.
- `smoketest/compiler_owned_intent/run_smoke.ps1` was added but not executed in this Linux session.
- `world` root selection is currently wired through native-ui/realtime root discovery; deeper per-adapter activation logic for viewport/web/UE5 remains future work.

Recommended next step:

- Make `world` an explicit first-class selection target across more CLI/package flows and teach `orchestrate` stage runtimes to hand off into the real Rust/Python/Node bridge crates instead of stopping at semantic labels.

## 2026-04-08 - kain-core now performs real executable-body semantic checks

The language core picked up the first meaningful semantic-trust pass instead of only walking bodies for syntax-shape validation.

What changed:

- Updated `crates/kain-core/src/types.rs`
  - Expanded the type environment to track global symbols, method signatures, and enum variant payloads.
  - Added real semantic checking for executable bodies: `let` bindings, assignments, returns, calls, method calls, conditionals, loops, `match`, `await`, async blocks, and core low-level memory expressions.
  - Added `ResolvedType::Future(Box<ResolvedType>)` and taught the checker to understand `impl Future<T>`, `async ...`, and `await ...`.
  - Added compatibility-aware builtins for shader/runtime semantics that the stricter checker now depends on, including `Void`, `Vec4`, `vec2` / `vec3` / `vec4`, `dispatch_thread_id`, tuple swizzles like `.x`, and `StorageBuffer<T>` indexing.
  - Added early semantic errors for return-type mismatches, incompatible `match` arm result types, and duplicate boolean match arms.
- Added `crates/kain-core/tests/semantic_typecheck_test.rs`
  - Locks in the new behavior with focused tests for return checking, `match` arm validation, duplicate boolean-arm rejection, and typed async/await acceptance.
- Updated `ARCHITECTURE.md`
  - Documented that `kain-core` now performs executable-body semantic validation before downstream bundle/codegen lanes consume the typed program.

Design decisions:

- Kept the public `TypedProgram` / `TypedItem` surface stable for downstream crates in this phase instead of forcing a broad typed-IR migration immediately.
- Chose a permissive semantic checker that errors on clear known mismatches but still falls back to `Unknown` for unsupported or backend-specific language corners, so the wider repo does not break all at once.
- Treated shader/runtime builtins as compiler-known semantic symbols rather than leaving them as implicit runtime-only behavior.

Current risks:

- The checker is materially stronger, but it is still not a full “typed IR everywhere” system yet; many paths still degrade to `Unknown` instead of proving precise types.
- Full `cargo test -p kain-core --lib --tests` on this machine still shows unrelated/pre-existing failures outside this patch:
  - `language_features::tests::default_profile_keeps_struct_literals_disabled`
  - `stdlib::tests::test_find_stdlib_from_env_var` when `KAIN_STDLIB_PATH` is already exported in the shell
  - two `realtime_app_bundle` tests around viewport parsing / duplicate scene emission
- Warning-capable diagnostics and hardening of non-exhaustive `match` are still future work; this patch validates arm agreement and obvious duplicate bool arms, but it does not yet introduce a formal warnings channel.

Recommended next step:

- Add a typed-body IR layer on top of this semantic pass and thread its results into monomorphization, runtime-contract emission, and downstream codegen so fewer language paths need to fall back to `Unknown`.

## 2026-04-06 - Windows bootstrap now falls back to installed LLVM and Python 3.12

Fresh-clone Windows setup now has a more durable path when the repo-local LLVM drop is missing and the machine default Python is newer than the pinned PyO3 lane supports.

What changed:

- Updated `scripts/sync-kain-source-of-truth.ps1`
  - Added `Resolve-ClangPath` so the install/sync flow no longer assumes `toolchain\llvm\bin\clang.exe` exists in every clone.
  - The script now prefers an already-set `KAIN_CLANG_PATH`, then the repo-local toolchain path, then `clang` on PATH, then `C:\Program Files\LLVM\bin\clang.exe`.
  - Added `Resolve-Python312Path` so the sync flow can discover a compatible Python 3.12 interpreter for the current `pyo3 0.20.x` dependency line.
  - The script now seeds both session PATH and persisted user PATH with the installed `kain` binary directory plus the resolved LLVM/Python directories when available, and it persists `PYO3_PYTHON` alongside the existing KAIN resource roots.
- Updated `ARCHITECTURE.md`
  - Added durable `Common Errors` notes covering fresh clones without vendored LLVM binaries and the Python 3.14 versus PyO3 0.20 mismatch / `python312.dll` runtime requirement.

Setup notes validated on this machine:

- Installed LLVM via `winget` and resolved `clang.exe` at `C:\Program Files\LLVM\bin\clang.exe`.
- Installed Python 3.12 alongside an existing Python 3.14 and pointed `PYO3_PYTHON` at the 3.12 interpreter.
- `cargo build -p cli` now succeeds on Windows when `KAIN_CLANG_PATH` and `PYO3_PYTHON` are set to those resolved installs.
- `target\debug\kain.exe --help` and `target\debug\kain.exe doctor` both run once the Python 3.12 directory is on PATH.

Current risks:

- The repo still documents the vendored LLVM drop as expected, so other scripts or docs may still assume `toolchain\llvm\bin\clang.exe` exists until they are similarly refreshed.
- The workspace still pins `pyo3 0.20.x`; future machines with only Python 3.13+ or 3.14 will keep hitting the same build/runtime mismatch unless they install Python 3.12 or the dependency line is upgraded.

Recommended next step:

- Upgrade the workspace's PyO3 dependency when practical, then simplify the Windows bootstrap once Python 3.13+ support is officially available in the pinned dependency line.

## 2026-04-02 - official UE5 authoring docs pipeline added under unreal_plugins/OfficialDocs

The repo now has a dedicated UE5-facing docs set aimed at teaching plugin authoring with Kain as a UE5 DSL and codegen pipeline.

What changed:

- Added `unreal_plugins/OfficialDocs/README.md`
  - Introduces the UE5-only documentation lane, the current crate ownership split, and the recommended reading order.
- Added `unreal_plugins/OfficialDocs/01-Getting-Started.md`
  - Establishes the DSL mental model, minimal `KAIN.toml` shape, first build flow, and the role of The Oracle.
- Added `unreal_plugins/OfficialDocs/02-KAIN-TOML-And-Project-Layout.md`
  - Documents the UE5 manifest shape, module layout patterns, and recommended source organization.
- Added `unreal_plugins/OfficialDocs/03-Language-To-UE5-Mapping.md`
  - Explains how Kain constructs map to UE5 runtime output: actors, components, subsystems, structs, enums, replication, RPCs, async tasks, and state machines.
- Added `unreal_plugins/OfficialDocs/04-Editor-UI-And-Tools.md`
  - Covers Slate, Details, viewports, toolbars, asset editors, editor modules, and reactive editor bindings.
- Added `unreal_plugins/OfficialDocs/05-Shaders-Materials-And-Graphs.md`
  - Covers shader authoring, material graph generation, graph editor/runtime systems, and current shader-manifest caveats.
- Added `unreal_plugins/OfficialDocs/06-Blueprints-GAS-And-Config.md`
  - Covers Blueprint generation, the staged maturity of GAS support, and developer settings/config generation.
- Added `unreal_plugins/OfficialDocs/07-Imports-Injection-And-Migration.md`
  - Frames `kain inject` plus Rust/TS/C imports as UE5 adoption accelerators instead of the main product headline.
- Added `unreal_plugins/OfficialDocs/08-Examples-Feature-Matrix-And-Limits.md`
  - Summarizes the strongest example plugins, a high-level feature matrix, and current known limits worth preserving in future docs and marketing.

Design decisions:

- The docs intentionally position Kain as a UE5 DSL and codegen system rather than trying to explain the entire compiler at once.
- The docs keep strong separation between:
  - production-ready core UE5 codegen
  - advanced but real adjacent lanes
  - partially wired or staged features such as broader GAS phases
- The docs are example-driven and lean on `unreal_plugins/*` as proof instead of only crate-internal claims.

Current risks:

- The new docs are broad and product-facing, but they are still a first-pass foundation rather than a complete reference for every single attribute or crate submodule.
- Future doc passes should expand exact syntax coverage for:
  - editor attributes
  - graph schemas and runtime graphs
  - shader and material authoring details
  - config attribute variants

Recommended next step:

- Add a second-pass UE5 docs expansion with deeper syntax reference pages and a dedicated "cookbook" section built from the strongest example plugins such as `Example_Comprehensive`, `Example_Graph`, `Example_Shader`, `FluidFlow`, and `MetaFitter`.

## 2026-03-29 - self constructor/type normalization now covers Self_ migration artifacts

The repair engine picked up a narrower normalization pass for `Self_` forms that show up in migration drafts and still trip the parser.

What changed:

- `crates/kain-repair/src/engine.rs`
  - Expanded `normalize_self_constructor_syntax` from a bare `Self:`/`Self :` rewrite into a line-aware pass that also handles `Self_` artifacts in constructor and type positions.
  - The pass now normalizes low-risk punctuation-adjacent forms such as `Self_:` / `Self_ :` / `Self_::`, `-> Self_`, `: Self_`, `(Self_`, ` Self_)`, and comma-adjacent variants.
- `crates/kain-repair/tests/fixtures/kain_repair_reserved_self.kn`
  - Added `Self_`-shaped constructor and return-type examples alongside the existing reserved-identifier drift case.
- `crates/kain-repair/tests/repair_fixtures.rs`
  - Updated assertions to prove `Self_` is normalized back into parser-safe `Self` / `Self::` forms.

Behavior now covered:

- `fn Self_(value: Int) -> Self_` -> `fn Self(value: Int) -> Self`
- `Self_:build(type)` -> `Self::build(type)`
- `Result<Self_, Self_>` -> `Result<Self, Self>`
- `Self_(left, right)` -> `Self(left, right)`

Notes:

- No tests or `cargo check` were run.
- This is intentionally conservative: it only rewrites obvious migration artifacts in places where `Self_` is acting like a bogus constructor/type token, not arbitrary identifiers.

## 2026-03-29 - nested declaration placement now flattens parser-hostile blocks

The repair engine now has a deterministic pass for nested declaration blocks that migration drafts tuck inside other declarations and the parser rejects outright.

What changed:

- `crates/kain-repair/src/engine.rs`
  - Added `flatten_nested_declaration_placement`, a line-oriented pass that detects nested `enum` / `struct` / `trait` / `impl` headers and lifts the whole declaration block back to top-level placement by stripping the surrounding indentation.
- `crates/kain-repair/src/registry.rs`
  - Registered the new rule as a safe class pass and placed it after declaration-header normalization.
- `crates/kain-repair/src/lib.rs`
  - Added `FixKind::FlattenNestedDeclarationPlacement` and a `flatten_nested_declarations` profile toggle.
- `crates/kain-repair/tests/fixtures/kain_repair_nested_declarations.kn`
- `crates/kain-repair/tests/repair_fixtures.rs`
  - Added fixture coverage for nested `struct`, `impl`, and `enum` declarations under an outer `enum`.

Behavior now covered:

- Nested `struct ...:` blocks inside an `enum ...:` are flattened to top-level `struct ...:` blocks.
- Nested `impl ...:` blocks inside an `enum ...:` are flattened to top-level `impl ...:` blocks.
- Nested `enum ...:` blocks inside an `enum ...:` are flattened to top-level sibling declarations.

This should eliminate parser failures where proof-tree output shows declarations embedded in declaration bodies, and it should move any remaining failure to the next seam: actual semantic restructuring, invalid block contents, or other non-declaration syntax errors.

Notes:

- No tests or `cargo check` were run.
- The pass is deliberately mechanical. It does not try to rebuild module semantics; it only gets obviously hostile nested declaration placement out of the parser's way.
>>>>>>> master
=======
- New Kain 3D pass (2026-04-14): unified 3D frame diagnostics across software and WGPU renderers by adding a shared `frame_diagnostics_for_scene(...)` helper. Both backends now emit scene name, viewport summary, composition summary, and camera-source metadata in `RenderFrame`, which makes backend comparisons and headless tooling easier. Validation is still blocked by the local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).
>>>>>>> fc43bb11 (Unify 3D frame diagnostics across renderers)
