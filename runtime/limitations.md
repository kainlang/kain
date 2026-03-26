# Native Runtime Blockers

This file tracks the real blockers for the native runtime lane in `runtime/native` and closely related native UI and rendering surfaces.

It is intentionally not a platform wishlist.

If a requirement is really a global language/runtime concern, a cross-host contract, or a future product/domain feature, it should live somewhere else. This file is only for the things that currently block Kain from acting like a serious native editor/runtime host.

## Inclusion Rule

Keep an item here only if it is one of these:

1. a native host/runtime surface that cannot be faked cleanly in template code
2. a renderer or viewport capability the native lane must own directly
3. an editor-grade native UI/workspace primitive that blocks real tool authoring
4. a native resource, scene, or execution contract that must exist below template/app code

Do not keep items here just because they would be nice for the broader Kain ecosystem.

## Current Native Blockers

1. Stable native scene handles and mutation APIs for entities, meshes, materials, lights, cameras, volumes, fields, instancers, and other runtime-owned scene resources.
2. Transactional scene-delta surfaces so native edits can be applied, observed, reverted, serialized, and replayed without bespoke host glue.
3. Native scene query contracts for picking, selection masks, raycasts, BVH queries, visibility checks, and viewport-aligned probe operations.
4. Stable viewport input contracts for mouse, keyboard, pen, touch, shortcuts, camera navigation, manipulators, and tool-context focus routing.
5. Native multi-viewport and multi-window presentation surfaces with docking, focus transfer, overlay routing, and persistent workspace layout state.
6. Editor-grade native UI primitives for inspectors, outliners, trees, tables, property sheets, overlays, command bars, and timeline-capable dock content.
7. Source-driven scene/runtime document surfaces so scenes, launch profiles, runtime requirements, and scene classes do not depend on sidecar-only orchestration.
8. Source-driven widget/layout authoring surfaces for native dock content, overlays, tool chrome, and runtime-bound UI composition.
9. First-class render graph contracts with pass dependencies, transient attachment lifetimes, scheduling order, and frame-debug/capture hooks.
10. Native resource residency primitives for GPU and CPU buffers, images, transient pools, streaming uploads, eviction policy, and budget-aware inspection.
11. Compute scheduling surfaces that let the native lane chain dispatches, synchronize compute and graphics work, and expose execution barriers without host-specific orchestration.
12. Runtime reflection queries for native scene state, resources, shader bindings, widget bindings, and compatibility checks so tools can inspect the live host without bespoke code paths.
13. Deterministic persistence surfaces for autosave, crash recovery, session restore, branch-safe snapshots, and durable workspace state materialization.
14. Native packaging and launch-profile assembly for standalone desktop apps, embedded runtime modules, and reusable native feature packs.
15. Device and backend reflection surfaces for GPUs, displays, input devices, hotplug transitions, backend capability checks, and launch-readiness gating.
16. Native interoperability contracts for asset staging and runtime ingestion so host-imported content and compiler-emitted artifacts enter the native lane through one canonical path.

## Explicitly Out Of Scope For This File

The following do not belong here unless they become direct native-lane blockers:

- cloud, marketplace, fleet, commerce, or object-storage systems
- broad DCC/domain feature catalogs such as photogrammetry, CAD, fabrication, robotics, broadcast, or virtual production
- gameplay frameworks, narrative systems, modding ecosystems, or online service stacks
- identity, entitlement, licensing, billing, or organization-management features
- generalized localization, remote streaming, thin-client, or multi-tenant platform work
- broad asset-format wishlists unless the missing piece is specifically the native ingestion contract

## Policy

If a future template or app hits one of the blockers above, add the missing native runtime surface instead of burying the requirement in template-local manual code.

If the need is broader than the native lane, move it to the correct global runtime or product-planning document instead of inflating this file again.
