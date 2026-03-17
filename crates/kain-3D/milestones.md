# Kain GPU-First Platform Program

## Summary

This board turns the GPU-first roadmap into a parallel execution plan. `SPIR-V` remains the canonical native GPU payload inside a compiler-owned `Kain shader bundle`. `WGSL`, `HLSL`, `USF`, and future formats are derived outputs, not independent truths.

The implementation boundary is still `Kain + ZenDCC`:
- `Kain` owns authored scene/tool/shader intent, compiler artifacts, and runtime contracts.
- `ZenDCC` owns persistence, entity/layer state, and flagship DCC integration until Kain-native editor persistence replaces it.

Program order is still fixed at the milestone level:
1. product rule and ownership
2. shader bundle system
3. GPU-first viewport and present path
4. scene/runtime bundle truth
5. editor substrate minimum core
6. modern renderer features
7. compute-native world
8. procedural object model
9. scale and streaming
10. hot reload
11. flagship proof apps

This file exists to answer a different question:
`what can multiple agents do right now without stepping on each other?`

## Current Status Snapshot

### Done enough to build on

- `ShaderArtifactBundle` exists in `kain-core` and is emitted by `kain-driver`.
- `RealtimeAppBundle` exists and is emitted beside runtime contracts.
- `kain-3D` has a real backend seam via `RenderBackend`.
- `WgpuRenderer` exists and is active in the native UI lane.
- `kain-ui-native` can resolve viewport scene/material/shader references from realtime sidecars.
- Raw native runtime can load a realtime bundle sidecar and expose it in the viewport/sculpt lanes.
- CPU picking, GPU picking, and first-pass gizmo rendering exist.

### Still transitional

- WGPU is still consuming bundle-derived `WGSL` rather than direct runtime `SPIR-V` module loading.
- The GPU viewport path is not fully retired to a pure zero-copy presented surface everywhere.
- `RealtimeAppBundle` scene/material/shader refs are not yet the only truth across `kain-ui-native`, raw native, and Zen.
- Gizmos are not yet a full manipulator drag/transaction system.
- Zen bridge, layer bindings, and undo/redo authority are not wired end-to-end.
- Modern material/resource ownership is still early.

### Program position

- `Milestone 0`: effectively active as policy
- `Milestone 1`: mostly implemented, not retired
- `Milestone 2`: partially implemented
- `Milestone 3`: partially implemented
- `Milestone 4`: started
- `Milestone 5+`: mostly ahead

## Non-Negotiables

- Canonical unit is a `Kain shader bundle`, not a loose `.spv` file.
- `SPIR-V` is the canonical native GPU payload inside that bundle.
- `WGSL`, `HLSL`, and `USF` are derived outputs and must not become independent truths.
- No long-term reliance on handwritten renderer-local shader blobs.
- No divergent scene/material/shader truths across hosts.
- No flagship demo work unless it graduates reusable compiler/runtime/editor services.

## Workstream Board

Each workstream below is intentionally shaped so one agent can own it without creating merge chaos in the others.

### WS1: Shader Bundle Retirement

Status: `ACTIVE`
Milestone target: `1`
Primary ownership:
- `crates/kain-core`
- `crates/kain-driver`
- `crates/gpu`
- `crates/kain-3D`

Goal:
- Retire transitional renderer-local shader assumptions.

Scope:
- Make `ShaderArtifactBundle` the only production shader source for `kain-3D`.
- Remove any remaining handwritten viewport/material shader blobs that bypass bundle loading.
- Move reflection/resource layout usage out of renderer-local assumptions and into bundle metadata.
- Keep derived `WGSL` only as a compatibility bridge until runtime-native SPIR-V consumption is practical.

Key files:
- `M:/Code/Kain/crates/kain-core/src/shader_artifact.rs`
- `M:/Code/Kain/crates/kain-driver/src/lib.rs`
- `M:/Code/Kain/crates/cli/src/gpu_artifacts.rs`
- `M:/Code/Kain/crates/kain-3D/src/shader_bundle.rs`
- `M:/Code/Kain/crates/kain-3D/src/wgpu_renderer.rs`

Retirement criteria:
- No production viewport/material/compute path depends on renderer-local shader strings.
- Bundle reflection is used for resource layout resolution.
- Cross-backend artifact emission is covered by tests.

### WS2: GPU Viewport Retirement

Status: `ACTIVE`
Milestone target: `2`
Primary ownership:
- `crates/kain-3D`
- `crates/kain-ui-native`

Goal:
- Make the default editor viewport unambiguously GPU-first.

Scope:
- Finish the presented WGPU surface path.
- Keep readback only for capture/export/test fallback.
- Expose capability labels precisely: `wgpu-surface`, `wgpu-readback`, `software`.
- Remove fuzzy fallback reporting and implicit renderer choice.

Key files:
- `M:/Code/Kain/crates/kain-ui-native/src/lib.rs`
- `M:/Code/Kain/crates/kain-3D/src/wgpu_renderer.rs`

Retirement criteria:
- Default editor viewport is presented by GPU, not readback-first.
- Readback is explicit and opt-in.
- Smoke lab proves `wgpu-surface` end to end.

### WS3: Realtime Bundle Truth

Status: `ACTIVE`
Milestone target: `3`
Primary ownership:
- `crates/kain-core`
- `crates/kain-driver`
- `crates/cli`
- `crates/kain-ui-native`
- `runtime/native`
- `ZenDCC`

Goal:
- Make `RealtimeAppBundle` the only scene/material/shader truth.

Scope:
- Finish `RealtimeAppBundle` schema for scenes, materials, assets, shader refs, tool caps, and requirements.
- Ensure `kain-ui-native`, raw native, and Zen consume the same bundle fields.
- Remove default scene/material lookup paths that bypass bundle metadata.
- Make host startup/runtime validation check for missing required bundle capabilities.

Key files:
- `M:/Code/Kain/crates/kain-core/src/realtime_app_bundle.rs`
- `M:/Code/Kain/crates/kain-driver/src/native_app.rs`
- `M:/Code/Kain/crates/cli/src/main.rs`
- `M:/Code/Kain/crates/kain-ui-native/src/lib.rs`
- `M:/Code/Kain/runtime/native/include/kain_runtime_realtime.h`
- `M:/Code/Kain/runtime/native/src/core/kain_runtime_realtime.c`
- `M:/Code/Kain/runtime/native/src/platform/win32/kain_runtime_viewport_win32.c`
- `M:/Code/Kain/runtime/native/src/platform/win32/kain_runtime_sculpt_win32.c`

Retirement criteria:
- The same compiled scene/material bundle runs in native UI, raw native, and Zen-hosted lanes.
- Shader/material bindings resolve through bundle metadata, not host-local fallbacks.
- Runtime validation fails clearly when required bundle capabilities are absent.

### WS4: Editor Substrate Core

Status: `ACTIVE`
Milestone target: `4`
Primary ownership:
- `crates/kain-3D`
- `crates/kain-ui-native`
- `ZenDCC`

Goal:
- Finish the minimum professional interaction loop.

Scope:
- Complete manipulator drag logic for translate/rotate/scale.
- Add stable transaction boundaries for viewport edits.
- Add `ZenSceneBridge`, `ZenEntityBinding`, `ZenLayerBinding`, `ViewportSelectionState`, and `SceneTransaction`.
- Wire viewport selection, gizmos, layers, and undo/redo through the same state path.

Key files:
- `M:/Code/Kain/crates/kain-3D/src/interaction.rs`
- `M:/Code/Kain/crates/kain-3D/src/renderer.rs`
- `M:/Code/Kain/crates/kain-3D/src/wgpu_renderer.rs`
- `M:/Code/Kain/crates/kain-ui-native/src/lib.rs`
- `M:/ZenDCC/crates/zen-kain-api`

Retirement criteria:
- Pick, transform, visibility, and undo/redo stay synchronized between viewport and Zen layer state.
- Gizmos are interactive, not visual-only.

### WS5: Modern Renderer Core

Status: `READY`
Milestone target: `5`
Primary ownership:
- `crates/kain-3D`
- `crates/gpu`
- `crates/kain-core`

Goal:
- Push the renderer from demo shading to DCC-grade material/resource ownership.

Scope:
- Add textures, PBR, normal/roughness/metalness, environment lighting, instancing.
- Replace scalar material structs with compiled resource-backed material instances.
- Add async uploads, visibility culling, and frame/render graph structure.

Key files:
- `M:/Code/Kain/crates/kain-3D/src/scene.rs`
- `M:/Code/Kain/crates/kain-3D/src/wgpu_renderer.rs`
- `M:/Code/Kain/crates/kain-core/src/shader_artifact.rs`

Retirement criteria:
- Asset-backed materials render through compiler-owned shader/material bundles.
- Renderer quality is good enough for a serious viewport, not just smoke scenes.

### WS6: Compute-Native World

Status: `READY`
Milestone target: `6`
Primary ownership:
- `crates/kain-core`
- `crates/gpu`
- `crates/kain-3D`

Goal:
- Make compute artifacts first-class so terrain/simulation tools stop being bespoke demos.

Scope:
- Introduce `ComputeArtifactBundle`.
- Define reflection, dispatch layout, residency, scheduling, tiling, and cache identity.
- First domains: heightfields, erosion, sediment, masks, scatter, voxel/SDF ops.

Retirement criteria:
- Terrain and simulation systems run through compiled compute artifacts, not custom tool code.

### WS7: Procedural Object Model

Status: `PARKED`
Milestone target: `7`
Primary ownership:
- `crates/kain-3D`
- `ZenDCC`

Goal:
- Make procedural DCC objects first-class runtime concepts.

Scope:
- Generators, deformers, cloners, splines, fields, effectors, modifiers, tags.
- Runtime-owned evaluation order and dependency invalidation.

Retirement criteria:
- Procedural modeling and motion-graphics flows are compact at the authored layer.

### WS8: Scale and Streaming

Status: `PARKED`
Milestone target: `8`
Primary ownership:
- `crates/kain-3D`
- `runtime/native`
- `ZenDCC`

Goal:
- Support production-scale scenes without app-specific hacks.

Scope:
- Streaming boundaries, terrain chunking, texture streaming, proxy/display modes, async import, multires meshes.

Retirement criteria:
- Heavy scenes remain interactive through runtime-owned streaming and residency logic.

### WS9: Hot Reload

Status: `PARKED`
Milestone target: `9`
Primary ownership:
- `crates/kain-driver`
- `crates/kain-ui-native`
- `ZenDCC`

Goal:
- Make save-to-viewport iteration fast without losing session state.

Scope:
- Bundle identity/versioning.
- Preserve camera, selection, graph focus, active tool, and undo boundaries across reloads.

Retirement criteria:
- Shader/UI/scene edits hot reload through one pipeline while keeping session context.

### WS10: Flagship Proof Apps

Status: `BLOCKED BY 4/5/6`
Milestone target: `10`
Primary ownership:
- `crates/kain-3D`
- `labs`
- `ZenDCC`

Targets:
- `Kain Terrain Lab`
- `Kain Motion/DCC Lab`

Success rule:
- The authored Kain layer stays compact because the platform owns rendering, compute, persistence, and editor substrate.

## Parallel Agent Assignment

Use this if you want to throw multiple agents at the board immediately.

### Agent A: Compiler Artifacts

Owns:
- `WS1`
- schema/tests for `ShaderArtifactBundle`
- bundle reflection/resource layout correctness

Avoid touching:
- viewport host code unless shader consumption is blocked

### Agent B: Viewport GPU Host

Owns:
- `WS2`
- presented surface path
- capability labeling
- smoke lab proof

Avoid touching:
- bundle schemas
- raw native runtime

### Agent C: Bundle Convergence

Owns:
- `WS3`
- sidecar emission/discovery
- host/runtime validation
- scene/material/shader truth across hosts

Avoid touching:
- manipulator math

### Agent D: Editor Interaction

Owns:
- `WS4`
- manipulator drag behavior
- transaction boundaries
- Zen selection/layer bridge

Avoid touching:
- shader compilation pipeline

### Agent E: Renderer Features

Owns:
- `WS5`
- textures/PBR/material instances
- culling/upload/frame graph structure

Avoid touching:
- persistence/undo systems except through stable APIs

### Agent F: Compute/Terrain

Owns:
- `WS6`
- compute bundle model
- terrain/simulation kernels

Avoid touching:
- editor shell layout

## Merge Discipline

To keep multiple agents fast without wrecking the tree:

- One workstream per agent at a time.
- Shared contracts change first, consumers second.
- Any new runtime or bundle field must be added with defaulting or compatibility handling.
- Do not merge demo-only shortcuts into platform seams.
- Each agent should prove the real lane they touched:
  - compiler emission
  - viewport host
  - raw native runtime
  - Zen bridge

## Board Order

The fastest defensible execution order is:

1. Retire `WS1`
2. Retire `WS2`
3. Retire `WS3`
4. Retire `WS4`
5. Run `WS5` and `WS6` in parallel
6. Use that platform to unlock `WS7`, `WS8`, `WS9`
7. Ship `WS10`

## Validation Matrix

### Compiler / bundle

- Golden tests for `ShaderArtifactBundle`
- Golden tests for `RealtimeAppBundle`
- Cross-backend derived output checks

### GPU / viewport

- `cargo check -p kain-3D`
- `cargo check -p kain-ui-native`
- native UI smoke lab with WGPU preferred

### Raw native runtime

- `clang -c .\\runtime\\kain_runtime.c -o .\\runtime\\kain_runtime_smoke.obj`
- `labs/raw_native_world_lab/build.ps1`

### Editor substrate

- Selection sync tests
- Manipulator transaction tests
- Zen bridge tests

### Future compute

- Compile/dispatch/cache tests for terrain and simulation kernels

## Public APIs / Interfaces

- `ShaderArtifactBundle`
- `ShaderArtifactRef`
- `CompiledMaterialDefinition`
- `CompiledMaterialInstance`
- `ComputeArtifactBundle`
- `RealtimeAppBundle`
- `RenderSceneBundle`
- `RuntimeCapabilitySet`
- `RendererCapabilitySet`
- `ZenSceneBridge`
- `ZenEntityBinding`
- `ZenLayerBinding`
- `ViewportSelectionState`
- `SceneTransaction`
- `HotReloadCompatibility`

## Ownership

- `kain-core`: shader/compute/runtime contract schemas and semantic lowering
- `kain-driver`: bundle compilation/materialization and contract emission
- `gpu`: canonical `SPIR-V` generation and derived backend materializers
- `kain-3D`: renderer/runtime consumption of compiled bundles
- `kain-ui` / `kain-ui-native`: editor UI and GPU viewport hosting
- `ZenDCC`: persistence, outliner/layers, undo authority, flagship DCC integration until replaced
