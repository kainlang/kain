# Alpha Phase 2 Contract Freeze

## Scope

This document freezes the Phase 2 Alpha surface for native graphics and execution contracts.

The active lane is upstream native runtime work in `M:/Code/Kain/runtime/native`.
Kain and the compiler still own semantic authoring truth.
The native runtime now owns a canonical execution-facing contract family for:

- render graph structure
- native resource residency
- compute scheduling and synchronization

This freeze is for Delta and Charlie consumption.
Do not bypass these structs with new host-local orchestration tables unless Alpha explicitly replaces the contract family.

## Source Of Truth

Primary headers:

- [kain_runtime_graphics.h](/M:/Code/Kain/runtime/native/include/kain_runtime_graphics.h)

Primary implementation:

- [kain_runtime_realtime.c](/M:/Code/Kain/runtime/native/src/core/kain_runtime_realtime.c)

## Frozen ABI Surface

Render graph:

- `KainRuntimeGraphicsPassKind`
- `KainRuntimeGraphicsAttachmentKind`
- `KainRuntimeGraphicsLifetimeKind`
- `KainRuntimeGraphicsBarrierKind`
- `KainRuntimeGraphicsAttachmentDescriptor`
- `KainRuntimeGraphicsRenderPassDescriptor`
- `KainRuntimeGraphicsRenderDependencyDescriptor`
- `KainRuntimeGraphicsRenderGraphContract`

Residency:

- `KainRuntimeGraphicsResidencyKind`
- `KainRuntimeGraphicsResidencyResourceDescriptor`
- `KainRuntimeGraphicsResidencyContract`

Scheduling:

- `KainRuntimeGraphicsQueueKind`
- `KainRuntimeGraphicsScheduleStepDescriptor`
- `KainRuntimeGraphicsScheduleBarrierDescriptor`
- `KainRuntimeGraphicsComputeSchedule`

Bundle integration:

- `KainRuntimeGraphicsBundle.render_graph`
- `KainRuntimeGraphicsBundle.residency`
- `KainRuntimeGraphicsBundle.primary_schedule`
- `KainRuntimeGraphicsValidation.has_render_graph_contract`
- `KainRuntimeGraphicsValidation.render_graph_valid`
- `KainRuntimeGraphicsValidation.has_residency_contract`
- `KainRuntimeGraphicsValidation.residency_valid`
- `KainRuntimeGraphicsValidation.has_compute_schedule_contract`
- `KainRuntimeGraphicsValidation.compute_schedule_valid`
- `KainRuntimeGraphicsExecutionState.schedule_step_count`
- `KainRuntimeGraphicsExecutionState.schedule_barrier_count`
- `KainRuntimeGraphicsExecutionState.schedule_key`

Public entrypoints:

- `kain_runtime_graphics_render_graph_init`
- `kain_runtime_graphics_residency_init`
- `kain_runtime_graphics_compute_schedule_init`
- `kain_runtime_graphics_render_graph_is_valid`
- `kain_runtime_graphics_residency_is_valid`
- `kain_runtime_graphics_compute_schedule_is_valid`
- `kain_runtime_graphics_format_contract_summary`

## Current Behavior

Phase 2 contracts are synthesized from existing bundle truth during graphics-bundle load.
The runtime does this in [kain_runtime_realtime.c](/M:/Code/Kain/runtime/native/src/core/kain_runtime_realtime.c):

- `kain_runtime_graphics_synthesize_render_graph`
- `kain_runtime_graphics_synthesize_residency`
- `kain_runtime_graphics_synthesize_compute_schedule`

Current synthesis rules:

- compute shader metadata becomes a `primary_compute` pass and schedule step when compute artifacts exist
- the first render scene becomes a `primary_scene_render` pass when a scene and viewport are present
- present becomes an explicit `present` pass when a render scene exists
- color, depth, swapchain, and first writable compute output become attachment descriptors when applicable
- material and compute bindings become residency resources
- residency bytes are estimated from binding type, access mode, and dispatch size
- a minimal transfer -> compute -> graphics chain is synthesized when the bundle supports it

## Invariants Delta Should Rely On

- the graphics bundle is the only native bundle surface that currently carries these execution contracts
- the runtime always initializes `render_graph`, `residency`, and `primary_schedule` through the shared graphics bundle path
- validation now treats render graph, residency, and compute schedule as first-class readiness checks
- execution summaries and compute execution state expose schedule counts and the primary schedule key
- the runtime contract is bundle-driven and reusable across host lanes; it is not a Win32-only struct family

## Honest Limits

- these are synthesized runtime contracts, not compiler-authored explicit tables yet
- attachment formats, residency byte estimates, and barrier reasons are minimal heuristics meant to stabilize the ABI shape, not final backend scheduling truth
- runtime reflection is not yet fully widened to enumerate every residency resource or schedule node through a live query service
- there is still no native scene registry or Delta-owned workspace shell consuming these contracts end to end

## Anti-Patterns

- do not add separate viewport-local render-pass tables in `runtime/native/src/platform/win32/`
- do not encode editor panel behavior directly into render graph or schedule structs
- do not add new execution semantics only in summary strings or diagnostics without landing the struct fields first
- do not treat synthesized values as compiler guarantees; if a field becomes authored later, widen the compiler and keep the runtime ABI stable

## Next Alpha Move

- replace synthesized attachment, residency, and barrier details with compiler-authored tables when the driver emits them
- widen runtime inspection so tools can query residency resources and schedule nodes through the same reflection family introduced in Phase 1
