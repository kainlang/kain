# Kain Memory

- New Kain 3D pass (2026-04-16): `FrameDiagnostics` now carries structured scene-composition cues (`scene_role`, `scene_scale`, and `scene_profile`) alongside the existing flat summary string, so renderer output can be queried without parsing one concatenated label. This is a tooling-focused uplift for native inspectors and scene browsers.
- Validation attempt: `cargo test -p kain-3d --lib` could not finish here because the repo-local Windows GNU toolchain still fails during build-script linking (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).

- New Kain 3D pass (2026-04-16): `SceneBounds` now exposes a coarse composition profile (`linear` / `planar` / `stacked` / `volumetric`), and `SceneCompositionSummary::brief_label()` surfaces that profile alongside the existing scale, aspect, and density cues. This makes scene diagnostics better at telling native tooling whether a scene is a corridor, a flat stage, or a fuller volumetric composition at a glance.
- New Kain 3D pass (2026-04-16): `SceneCompositionSummary` now also emits a coarse scene-role cue (`study` / `lookdev` / `showcase` / `environment` / `anomaly`), giving native tooling a one-word read on whether a composition is a small study, a presentation set, an FX-heavy environment, or a black-hole-style special case. The role cue is folded into the brief label so smoke logs and inspectors get the classification for free.
- Validation attempt: `cargo test -p kain-3d scene::tests::composition_profile_label_distinguishes_flat_and_volumetric_scenes -- --nocapture` still fails before the test binary can link because the repo-local Windows GNU toolchain cannot resolve `-lgcc_eh` and `-lgcc` from `x86_64-w64-mingw32-gcc`.
- Validation attempt for the new role cue: `cargo test -p kain-3d scene::tests::scene_role_label_tracks_scene_complexity_signals -- --nocapture` hit the same repo-local Windows GNU linker gap while building build-script dependencies, not a scene-logic failure.

- New Kain 3D pass (2026-04-16): software rendering now distinguishes visible vs. fully culled instances in `FrameDiagnostics`, so tooling can see when an authored object was completely clipped/backfaced instead of only inferring success from the final image. Added a regression test that pushes a triangle behind the camera and expects it to land in `culled_instances`.
- Validation attempt: `cargo test -p kain-3d renderer::tests -- --nocapture` still hits the repo-local Windows GNU linker gap before the test binary can link, because `x86_64-w64-mingw32-gcc` cannot resolve `-lgcc_eh` and `-lgcc`.

- New Kain 3D pass (2026-04-16): `SceneCompositionSummary::brief_label()` now includes an explicit scene-scale cue (`miniature` / `room-scale` / `studio-scale` / `world-scale`), and the `material_atrium_smoke` JSON payload now carries that scale as structured metadata. This gives 3D tooling one more quick-read signal for composition quality without re-deriving bounds heuristics downstream.
- Validation attempt: `cargo test -p kain-3d scene::tests::scene_scale_label_tracks_bounds_radius -- --nocapture` and `rustfmt --edition 2021 --check crates\\kain-3D\\src\\scene.rs crates\\kain-3D\\src\\lib.rs crates\\kain-3D\\src\\bin\\material_atrium_smoke.rs` both hit repo-local/environment issues before a clean green could be proven. The test run failed at link time because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`; the rustfmt check also surfaced pre-existing formatting differences elsewhere in `crates/kain-3D` and `crates/kain-ui-native` plus trailing whitespace in `crates/ue5-shaders/src/validation.rs`.

- New Kain 3D pass (2026-04-16): `material_atrium_smoke` now emits a structured `diagnostics.composition` payload alongside the existing brief label, including summary counts, framing distance, viewport aspect ratio, and bounds span/center data. This makes the 3D smoke report much easier for tooling to consume without re-deriving scene structure from screenshots or renderer internals.
- Validation attempt: `cargo check -p kain-3D --bin material_atrium_smoke` still fails in this repo-local Windows GNU toolchain before the crate can finish compiling because build-script linking cannot resolve `-lgcc_eh` and `-lgcc` from `x86_64-w64-mingw32-gcc`.
- New Kain 3D pass (2026-04-16): `SceneCompositionSummary` now counts directional and point lights in addition to meshes/materials/instances/animations/emitters/terrain, and the brief scene label surfaces those light counts when present. This makes dense lookdev or lighting-heavy scenes read more truthfully in renderer diagnostics and keeps the density cue aligned with actual authored scene complexity.
- Validation attempt: `cargo test -p kain-3d composition_summary_density_label_tracks_authoring_scale -- --nocapture` still fails before the test binary can link because the repo-local Windows GNU toolchain cannot find `-lgcc_eh` and `-lgcc`.
- The Kain 3D pipeline is a live fleet initiative now, and its steering should stay spec-first.
- The intended build path is native, GPU-aware 3D capability that can grow toward DCC-class tools like ZBrush, Substance Painter, and UE5-style workflows.
- Use Codex CLI through the coding-agent skill for pipeline tasks unless the user asks for another harness.
- If Codex reports a usage-limit error, verify the actual CLI output before assuming any seat-switch workaround.
- The user wants frequent updates while the pipeline is active, especially when branches, specs, or heartbeat behavior change.
- Kaino should keep the heartbeat/operator guidance current in this workspace so future passes stay aligned.
- New Kain 3D pass (2026-04-16): the WGPU renderer now preserves the same frame diagnostics as the software renderer, including scene name, viewport summary, composition summary, camera source, and catalog resolution metadata for scene renders. This closes a tooling gap where GPU-backed 3D frames were less self-describing than CPU-backed frames.
- Validation attempt: `cargo test -p kain-3d wgpu_renderer::tests::aligns_readback_rows_to_wgpu_requirement -- --nocapture` failed before reaching the 3D test because the repo-local Windows GNU toolchain still cannot link build scripts (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).
- New Kain 3D pass (2026-04-16): `SceneCompositionSummary::brief_label()` now carries a coarse scene-density cue (`sparse` / `balanced` / `dense`) based on authored meshes, instances, emitters, and terrain surfaces. This makes scene diagnostics better at signaling when a composition is small enough for quick iteration versus crowded enough to need more careful framing or tooling.
- Validation attempt: `cargo test -p kain-3d scene::tests::composition_summary_density_label_tracks_authoring_scale -- --nocapture` and `cargo test -p kain-3d scene::tests::scene_bounds_and_framed_camera_follow_scene_composition -- --nocapture` both failed before the tests could run because the repo-local Windows GNU toolchain still cannot link build scripts (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).
- New Kain 3D pass (2026-04-16): `SceneCompositionSummary::brief_label()` now spells out viewport shape as `portrait` / `square` / `landscape` instead of only raw aspect ratio, and the 3D scene tests now cover that banding helper. This makes renderer diagnostics easier to scan during scene-composition work without changing the underlying framing math.
- Validation attempt: `cargo test -p kain-3d scene::tests -- --nocapture` still hits the repo-local Windows GNU linker gap (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`) before the `kain-3d` test binary can link.
- 2026-04-15 bootstrap update: `kain selfhost bootstrap` now exists as the owned hand-written lane entrypoint, `src/KAIN.toml` is the manifest contract, `src/build_selfhost.sh` is just a wrapper, and the bootstrap report machinery now emits JSON/Markdown under `src/.selfhost/reports/`.
- The bootstrap harness is partially green: `--combine-only` passes and writes the combined source artifact, but `--emit-llvm-only` currently hard-fails inside the owned `src/core` source set with parser errors concentrated in `runtime.kn` and `types.kn`. The immediate blocker is language/source compatibility, not the CLI wrapper or report plumbing.
- Added a 3D platform uplift in `crates/kain-3D`: primitive libraries now export richer scene metadata (`definition_count`, `definition_ids`, and startup primitive display name) when registered into an authoring scene, which makes the library more self-describing for tooling and runtime composition.
- Added `SceneDescription::composition_summary(...)` plus a shared bounds helper in `crates/kain-3D`, so tooling can ask a scene for counts and framing data in one pass instead of re-deriving it ad hoc.
- Validation was blocked by the local Windows GNU toolchain, not by the change itself. `cargo test -p kain-3d scene::tests::scene_bounds_and_framed_camera_follow_scene_composition -- --nocapture` failed while linking build scripts because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): tightened default scene framing in `crates/kain-3D` so the auto-camera distance now scales with field of view instead of using a fixed radius multiplier. Added a regression test for the new framing helper to prove tighter FOVs push the camera farther back. Validation hit a repo-env Windows GNU linker gap, not a code failure: `cargo test -p kain-3d framed_camera_distance_scales_with_field_of_view` failed while building dependencies because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): scene bounds now include particle emitters, not just meshes/terrain/black holes, so auto-framing keeps volumetric FX inside the camera composition. Added a regression test proving an emitter-only scene still produces bounds and a framed camera pose. Validation was blocked by the same local Windows GNU linker gap, not by the scene logic: `cargo test -p kain-3d scene::tests` failed while building dependencies because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): `SceneCompositionSummary` now has a human-readable `brief_label()`/`Display` form, so 3D tooling and logs can describe a scene's composition without reformatting counts ad hoc. Added a regression assertion that `to_string()` matches the brief label. Validation was again blocked by the local Windows GNU linker gap, not the code change: `cargo test -p kain-3d scene::tests::scene_bounds_and_framed_camera_follow_scene_composition -- --nocapture` failed while building dependencies because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): auto-framed camera placement now scales its framing direction with the scene's horizontal and vertical extents instead of always biasing toward a fixed diagonal offset, and a new regression test covers tall-scene framing so vertical compositions stay above the scene center. This should behave better on wide or asymmetrical 3D compositions while keeping the same bounds-driven camera target. Validation hit the same repo-local Windows GNU linker gap before the test binary could build: `cargo test -p kain-3d scene::tests -- --nocapture` failed because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-14): `SceneBounds` now exposes a span() helper and `SceneCompositionSummary::brief_label()` includes the full XYZ span alongside radius. This makes scene logs and tooling more spatially descriptive without re-deriving extents at each call site. Added a regression assertion that the label includes span text and that `span()` equals `half_extents * 2.0`.
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
- New Kain 3D pass (2026-04-14): `SceneCompositionSummary` now carries the viewport aspect ratio and includes it in `brief_label()`, so frame diagnostics can report the actual render shape alongside bounds and camera fit instead of leaving aspect implicit. Added a regression assertion that the summary label includes `aspect 1.00:1` for the default path. Validation pending.
- New Kain 3D pass (2026-04-16): `SceneCompositionSummary::density_label()` now accounts for materials, animations, and black-hole presence in addition to meshes, instances, emitters, and terrain, so the sparse/balanced/dense cue better reflects actual scene complexity. The regression test now covers material/animation-heavy balanced scenes and black-hole-heavy dense scenes. Validation was blocked by the same local Windows GNU linker gap before the focused test binary could link: `cargo test -p kain-3d scene::tests::composition_summary_density_label_tracks_authoring_scale -- --nocapture` failed because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-16): `crates/kain-3D` now carries catalog resolution metadata through `FrameDiagnostics` for catalog renders, so frame logs can distinguish exact scene hits from aliases and default fallbacks instead of dropping that context after resolution. The software renderer also now preserves that metadata on the returned frame, which makes alias/default debugging easier for tooling and smoke reports. Validation hit the same local Windows GNU linker gap before the focused test binary could finish linking: `cargo test -p kain-3d renderer::tests -- --nocapture` failed while building dependencies because `x86_64-w64-mingw32-gcc` could not find `-lgcc_eh` and `-lgcc`.
- New Kain 3D pass (2026-04-16): auto-framed camera placement now uses an aspect-aware framing direction helper in `crates/kain-3D`, so the camera bias adapts more predictably to wide vs. tall compositions instead of using one hardcoded diagonal. Added a regression test for the direction helper. Validation was blocked by the repo-local Windows GNU linker gap when trying to run `cargo test -p kain-3d scene::tests`, and repo-wide `cargo fmt --all --check` is still blocked by trailing whitespace in `crates/ue5-shaders/src/validation.rs`.
- New Kain 3D pass (2026-04-16): the authored primitive library now publishes a single stable summary string plus subdivision-ready counts into scene metadata (`primitive_library.summary`, `primitive_library.subdivision_ready_count`), so 3D tooling can surface the catalog as one readable contract instead of reconstructing it from separate fields. This is a small but useful step toward better native inspectors and scene composition UX.
- New Kain 3D pass (2026-04-16): the `material_atrium_smoke` report now preserves catalog-resolution diagnostics in its JSON payload (`requested_name`, `resolved_name`, and resolution kind), so smoke consumers can distinguish exact, alias, and default scene resolution without re-parsing renderer internals. Validation of the crate still hits the local Windows GNU linker gap before the test binary can link (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).
- New Kain 3D pass (2026-04-16): fixed the WGPU renderer's camera-resolution plumbing by passing `RenderResolution` into the internal camera resolver, so the GPU 3D path can auto-frame scenes using the actual viewport size instead of a missing local variable. The repo-local Windows GNU toolchain still blocks full `cargo check` / `cargo test` validation here (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`), so the next best follow-up is to run the same crate checks in a host with a working Windows GNU or compatible toolchain.
- New Kain 3D pass (2026-04-16): `material_atrium_smoke` now emits structured scene-composition tags in its JSON payload (`scene_role`, `scene_profile`, `scene_density`) instead of only relying on the human-readable brief label. This makes the smoke report easier for inspectors and downstream automation to query without parsing a concatenated string. Validation still hit the repo-local Windows GNU linker gap before `cargo test -p kain-3d` could link (`x86_64-w64-mingw32-gcc` missing `-lgcc_eh` and `-lgcc`).
# 2026-04-15 - Ouroboros now has an explicit owned bootstrap/native control-plane contract

The durable selfhost direction is now split cleanly into two lanes under the same Ouroboros control plane: the existing Rust mirror/reference lane and the hand-written owned bootstrap/native lane. The Rust mirror lane remains useful as donor, oracle, and repair infrastructure, but the hand-written lane is now the explicit promotion target for real selfhost.

What changed:

- Updated `ouroboros/docs/selfhost/pipeline_manifest.json`
  - Added `owned-bootstrap`, `owned-native`, and `owned-ouroboros` lanes beside the existing `phase2-*` lanes.
  - Added default path contracts for `src/KAIN.toml`, `src/.selfhost/`, the native runtime manifest, the runtime build script, and the first owned artifact outputs.
  - Recorded consumes/produces, success criteria, and validation commands for each owned lane so the control plane can track the hand-written bootstrap path without inventing a second planner.
- Updated `ouroboros/docs/selfhost/ouroboros-v2-selfhost-pipeline.md`
  - Reframed the selfhost docs around two lanes instead of only the Rust mirror lane.
  - Added owned-lane gates for manifest/runtime resolution, owned compiler emission, native self-build, and ouroboros parity.
- Updated `ARCHITECTURE.md`
  - Replaced the old mirror-only selfhost description with an explicit two-lane model.
  - Made `src/KAIN.toml` the canonical hand-written compiler contract and `runtime/native_runtime.toml` the canonical native runtime contract.
  - Recorded the bootstrap boundary: Rust may remain the thin host for manifest/filesystem/process/reporting work during bootstrap, but it should not stay the permanent owner of parser/typechecker/lowering/codegen once the hand-written lane is alive.
  - Added new operator notes for `kain selfhost bootstrap` and for false-green prevention under `src/.selfhost/`.

Design decisions:

- Kept the C runtime as the canonical native runtime substrate for the owned selfhost lane instead of trying to invent a runtime-free or Rust-hosted definition of native execution.
- Treated the aggregate bootstrap source under `src/.selfhost/phase0/combined/` as an explicit temporary compatibility bridge, not as the end-state module system.
- Chose to model the owned lane in the same Ouroboros manifest as the Rust mirror lane so future agents can compare, validate, and promote both lanes from one data-driven control plane.

Current risks:

- The docs now describe the owned bootstrap lane as the canonical direction, but the implementation still has to keep the emitted artifact set and the manifest fields in sync with those docs.
- The owned manifest and runtime manifest are now separate contracts by design. If either of them drifts from the CLI/bootstrap implementation, operators will get a structurally correct story and an incorrect tool.
- The owned lane will be vulnerable to false greens unless the CLI treats missing fresh artifacts as hard failures even when stale outputs remain under `src/.selfhost/`.

Recommended next step:

- Land and validate `kain selfhost bootstrap` so the owned control-plane entries are exercised by real commands, then add a strict parity check for the expected `src/.selfhost/` artifact family once the first end-to-end native self-build is green.

# 2026-04-14 - Three.js Node FFI lab grew into a sculpt suite with a Rust WASM core

The existing browser proof under `labs/threejs_node_ffi_space_lab/` is no longer only a free-fly sphere scene. It now acts as a small sculpting suite with a manifest-driven universal viewport and a local Rust `wasm32-unknown-unknown` brush kernel.

What changed:

- Added manifest registries for sculpt tools, universal viewport profiles, and the Rust WASM build pipeline.
- Added a local crate under `labs/threejs_node_ffi_space_lab/wasm/sculpt_core/` that exports raw brush deformation over vertex buffers.
- Extended `helpers/space_lab_runtime.mjs` so `npm run build` also compiles the Rust crate, copies `outputs/wasm/sculpt_core.wasm`, and serves `.wasm` with the correct MIME type.
- Split the browser client into clearer ownership layers: runtime model parsing, universal viewport control, WASM bridge, and scene/app shell wiring.
- Replaced the original free-fly-only scene with a universal viewport shell that supports sculpt, orbit, and fly modes over one floating orb in a large Three.js space.

Validation:

- `rustup target add wasm32-unknown-unknown`
- `npm run build:wasm` in `labs/threejs_node_ffi_space_lab`
- `npm run build` in `labs/threejs_node_ffi_space_lab`
- `npm run serve` in `labs/threejs_node_ffi_space_lab`
- `curl -I http://127.0.0.1:4192/wasm/sculpt_core.wasm`

Important behavior notes:

- The sculpt core is intentionally narrow. It mutates vertex positions only; raycasts, UI, normals, and camera policy stay in the browser/Three.js lane.
- The current localhost server for this lab must be restarted after runtime changes or it can keep serving stale MIME behavior for `.wasm`.
- The host-backed Kain JavaScript bridge issue is still unresolved in this checkout, so the validated execution path remains the Node helper commands rather than `kain run`.

Recommended next step:

- Repair the host-backed Kain JavaScript bridge registration so the lab can be executed end-to-end from `src/main.kn`, then decide whether this browser-side sculpt proof should stay a lab or graduate into a broader app archetype.

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
