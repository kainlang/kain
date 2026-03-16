# Finish Plan: Kain 3D Pipeline Retirement

## Summary

Retirement target is the **Kain UI editor lane**, not the raw native lab. The finishing renderer is **WGPU**, while the current software rasterizer remains a fallback/reference path and bring-up harness. For selection, layers, gizmos, and undo, the plan uses a **Zen-first bridge**: Kain owns viewport/render/picking/manipulator behavior, and Zen remains the persisted state and history authority for the retirement milestone.

The result of this plan is a native Kain UI editor with:
- GPU viewport as the default renderer
- generalized picking/raycast
- working translate/rotate/scale manipulators
- Zen entity/layer persistence bridge
- hardened undo/redo through Zen transactions
- software renderer still available for tests, fallback, and artifact parity

## Implementation Changes

### 1. Replace the active viewport renderer with a backend model and make WGPU the default
- Add a backend-neutral render layer in `crates/kain-3D`:
  - `RenderBackend` trait
  - `RenderSceneSnapshot`
  - stable mesh/material/light/instance IDs
  - `ViewportFrame` as the shared output contract for editor viewports
- Keep `SoftwareRenderer` as a fallback implementation of `RenderBackend`; do not remove it.
- Add `WgpuRenderer` as the default editor renderer:
  - forward-lit opaque pass
  - depth buffer
  - directional and point lights
  - per-instance transforms
  - optional wireframe/debug overlay
  - viewport resize handling and persistent GPU resources
- Use compiler-owned shader artifacts as the source of truth:
  - Kain shader/material source compiles through the existing GPU artifact path
  - runtime/editor host consumes materialized shader artifacts for WGPU
  - do not hand-author a separate renderer-only shading language path
- Define the retirement renderer feature set explicitly:
  - standard lit material
  - unlit material
  - vertex color support
  - normal support
  - depth-tested particles
  - scene background/environment gradient
- Keep the raw native lab as the renderer bring-up harness, but it is not the retirement artifact.

### 2. Add a generalized picking and interaction layer in `kain-3D`
- Introduce reusable picking interfaces:
  - `PickingRay`
  - `PickingHit`
  - `PickTargetId`
  - `PickingQuery`
  - `PickingService`
- Implement two picking paths:
  - CPU ray/triangle picking against the scene snapshot for fallback/tests
  - GPU object-ID picking pass for the WGPU viewport as the default editor path
- `PickingHit` must include:
  - scene instance ID
  - optional Zen entity ID
  - optional layer ID
  - world-space hit position
  - hit normal
  - hit distance
- Route viewport input through a single editor interaction controller:
  - hover
  - press/drag/release
  - marquee readiness
  - modifier keys
  - viewport focus/capture state
- Do not bury picking logic in `kain-ui-native`; `kain-ui-native` should forward viewport input/events into `kain-3D` interaction services.

### 3. Add a real gizmo/manipulator system
- Add editor-facing manipulator types in `kain-3D`:
  - `ManipulatorMode` = translate / rotate / scale
  - `ManipulatorSpace` = local / world
  - `ManipulatorAxis` = x / y / z / plane / screen
  - `ManipulatorState` = idle / hover / active drag
  - `ManipulatorDelta`
- Manipulator rendering and hit logic must use the same picking layer as scene selection.
- Support these behaviors for retirement:
  - axis and plane translation
  - single-axis rotation
  - uniform and axis scale
  - local/world toggle
  - optional snap settings for move/rotate/scale
- The Kain UI viewport owns the tool chrome and hotkeys; `kain-3D` owns manipulator math and command emission.
- Manipulator actions must emit semantic scene commands, not mutate Zen or viewport state ad hoc.

### 4. Bridge `kain-3D` scene state into Zen ECS, layers, and persistence
- Add a formal bridge contract between `kain-3D` and Zen:
  - `ZenEntityBinding`
  - `ZenLayerBinding`
  - `ZenSceneBridge`
  - `ZenViewportPayload`
  - `SceneCommand`
  - `SceneTransaction`
- Kain owns:
  - scene graph snapshot
  - renderer instance IDs
  - picking/manipulator commands
  - viewport camera/controller state
- Zen owns for the retirement milestone:
  - persisted entity identity
  - layer metadata
  - visibility/lock flags
  - rename/delete/select state
  - undo/redo transaction history
- The bridge must map `kain-3D` instance IDs to Zen entity IDs and keep that mapping stable across:
  - viewport redraws
  - GLB reloads
  - selection changes
  - undo/redo replay
- Use the existing Zen contract crates to carry the bridge metadata instead of inventing a separate one-off payload shape.
- The Kain UI editor retirement build must be able to:
  - load a Kain-authored scene
  - expose it as Zen entities/layers
  - select from viewport or layers panel
  - transform via gizmo
  - rename / visibility / lock through the layer model
  - undo / redo safely

### 5. Harden undo/redo as transactions, not mesh-local hacks
- Do not treat undo as viewport-local state.
- Introduce command-based history boundaries:
  - `BeginTransaction`
  - `ApplyCommand`
  - `CommitTransaction`
  - `RollbackTransaction`
- Use transaction categories:
  - selection
  - transform
  - layer metadata
  - visibility/lock
  - mesh edit
  - scene import/remove
- For retirement, transform and layer operations must round-trip through Zen’s undo/redo path.
- Mesh editing can use snapshot-backed deltas at first, but the command boundary must be the same as transforms/layers so the model can evolve.
- Explicitly reject direct mutation paths that bypass transactions in the editor viewport.

## Public Interfaces and Contracts

These additions should be treated as the new stable seams:
- `RenderBackend`, `WgpuRenderer`, `RenderSceneSnapshot`, `ViewportFrame`
- `PickingRay`, `PickingHit`, `PickingService`, `PickTargetId`
- `ManipulatorMode`, `ManipulatorSpace`, `ManipulatorState`, `ManipulatorDelta`
- `SceneCommand`, `SceneTransaction`
- `ZenEntityBinding`, `ZenLayerBinding`, `ZenSceneBridge`, `ZenViewportPayload`

Ownership rules:
- `crates/kain-3D`: renderer backend abstraction, scene snapshot, picking, manipulators, viewport command model
- `crates/kain-ui-native`: viewport host, input forwarding, tool UI, editor shell integration
- `M:/ZenDCC/crates/zen-kain-api` and Zen-side viewport/layer crates: entity/layer binding, persistence mapping, undo sink/source

## Test Plan

### Unit and crate-level tests
- WGPU renderer builds and draws a scene snapshot with depth and lights.
- CPU picking and GPU ID-picking return the same target for deterministic test scenes.
- Manipulator math produces correct transform deltas for axis, plane, and rotation interactions.
- Scene-to-Zen binding keeps stable IDs across redraw and snapshot rebuild.
- Transaction rollback restores pre-edit state exactly.

### Integration tests
- Kain UI editor viewport loads a GLB scene and renders through WGPU.
- Clicking a mesh selects the same object in the Zen layers model.
- Dragging translate/rotate/scale gizmos updates both viewport transform and Zen layer/entity state.
- Undo and redo restore selection, transforms, and layer flags without desync.
- Visibility and lock changes from the layers panel are reflected in the viewport and preserved through undo/redo.

### Retirement acceptance scenarios
- No software renderer is the active viewport path in the Kain UI editor.
- A user can load a scene, pick objects, transform them with gizmos, and see layer state stay synchronized.
- Undo/redo is stable over repeated transform and visibility operations.
- The same scene can still be rendered by the software path for fallback parity tests.

## Assumptions and Defaults

- **GPU backend:** WGPU is the retirement renderer.
- **Retirement artifact:** the Kain UI editor lane, not the raw native lab.
- **State model:** Zen remains the persistence/undo/layer authority for the retirement milestone.
- The software renderer stays in-tree as fallback/reference and test oracle.
- The raw native C runtime is not the retirement viewport host for this milestone; it remains a secondary lane and compatibility target.
- Shader/material runtime artifacts should stay compiler-owned and flow through the existing Kain GPU artifact pipeline rather than introducing a parallel handwritten shader asset path.
