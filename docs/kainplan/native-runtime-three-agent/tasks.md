# Native Runtime Three-Agent Tasks

## Overview

This task plan turns the trimmed native blocker list in [runtime/limitations.md](/M:/Code/Kain/runtime/limitations.md) into a parallel execution plan for three agents.

The target is not "more features."
The target is a credible native runtime lane with a clean contract spine, a real editor-facing host surface, and enough validation and documentation discipline that future work does not collapse back into template-local glue.

This plan assumes:

- `Alpha` owns core native runtime contracts and substrate work
- `Delta` owns native editor, viewport, workspace, and packaging integration
- `Charlie` owns blocker mapping, validation harness preparation, documentation continuity, and merge hygiene

This plan is written for implementation-focused agents.
Do not mark a lane complete because a type, header, or shell UI exists.
A lane is complete only when the vertical slice for that lane is real and wired through the existing native runtime path.

## Ground Truth And Evidence

The plan is based on the current native runtime surfaces and existing documentation, especially:

- [limitations.md](/M:/Code/Kain/runtime/limitations.md)
- [tasks.md](/M:/Code/Kain/runtime/tasks.md)
- [ARCHITECTURE.md](/M:/Code/Kain/ARCHITECTURE.md)
- [kain_runtime_services.h](/M:/Code/Kain/runtime/native/include/kain_runtime_services.h)
- [kain_runtime_viewport_win32.c](/M:/Code/Kain/runtime/native/src/platform/win32/kain_runtime_viewport_win32.c)
- [kain_runtime_contract.c](/M:/Code/Kain/runtime/native/src/core/kain_runtime_contract.c)
- [kain_runtime_reflection.c](/M:/Code/Kain/runtime/native/src/core/kain_runtime_reflection.c)
- [kain_ui_runtime.c](/M:/Code/Kain/runtime/native/src/ui/kain_ui_runtime.c)

Current reality that this plan must respect:

- the native runtime already has a service-registry and diagnostics spine
- the Win32 viewport host already bundles contract, realtime, graphics, UI, and asset loading in one large host path
- graphics and compute execution exist, but scheduling, resource residency, and render-graph surfaces are still too host-shaped
- native UI exists, but editor-grade widgets, workspace layout, and dock-aware authoring are not yet the canonical runtime truth
- source-driven scene and widget authoring are still weaker than the repo's desired "Kain-first" doctrine

## Execution Rules

- keep compiler-owned semantic truth in compiler and driver lanes; this plan is for native runtime ownership only
- do not smuggle new blocker work into template code, smoke-only glue, or platform-specific ad hoc helpers
- prefer canonical headers, structured descriptors, registries, and receipts over loose helper functions
- keep Win32 as the proving lane, but do not hardcode Win32-only semantics into the public contract surface
- do not let Delta invent an editor-side object model that bypasses Alpha's contract layer
- keep Charlie honest about status; "contracts only" is not "implemented"

## Lane Ownership

### Alpha

Alpha owns the contract and substrate blockers:

- blocker 1: scene handles and mutation APIs
- blocker 2: transactional scene deltas
- blocker 3: scene queries
- blocker 9: render graph
- blocker 10: resource residency
- blocker 11: compute scheduling
- blocker 12: runtime reflection queries
- blocker 15: device and backend reflection
- blocker 16: native interoperability and ingestion

Primary code ownership:

- `runtime/native/include/`
- `runtime/native/src/core/`
- `runtime/native/src/asset/`
- native-facing shared bundle or contract emitters in crates only when the runtime contract cannot be expressed otherwise

### Delta

Delta owns the editor and presentation blockers:

- blocker 4: viewport input contracts
- blocker 5: multi-viewport, multi-window, docking, and layout persistence
- blocker 6: editor-grade UI primitives
- blocker 7: source-driven scene/runtime documents
- blocker 8: source-driven widget/layout authoring
- blocker 14: native packaging and launch profiles

Primary code ownership:

- `runtime/native/src/platform/win32/`
- `runtime/native/src/ui/`
- `runtime/native/include/` for UI, viewport, and workspace-facing declarations
- `crates/kain-driver/` and `crates/kain-ui/` only where native packaging or source-authored runtime surfaces need compiler/driver support

### Charlie

Charlie owns support and continuity:

- blocker matrix and file-to-blocker mapping
- validation harness preparation and smoke-plan updates
- architecture and memory updates when lane ownership or runtime doctrine materially changes
- merge hygiene, acceptance checklists, and explicit status reporting

Primary code and doc ownership:

- `docs/kainplan/native-runtime-three-agent/`
- `runtime/conformance/`
- `smoketest/` where narrow native runtime smoke coverage needs to be extended
- [ARCHITECTURE.md](/M:/Code/Kain/ARCHITECTURE.md)
- [MEMORY.md](/M:/Code/Kain/MEMORY.md)

## Dependency Rules

- Alpha defines public contract shapes before Delta relies on them
- Delta may prototype internal wiring in parallel, but must not freeze public runtime semantics before Alpha does
- Charlie does not invent runtime behavior; Charlie records, validates, and tightens the plan around implemented truth
- if a task needs compiler or driver help, keep that work minimal and only in service of the native runtime contract

## Shared Deliverable

The first shared vertical slice is:

1. a source-authored native scene document resolves into a canonical runtime-owned scene package
2. the native host opens a docked multi-panel workspace with at least one live viewport and one inspector-style panel
3. viewport input flows through stable runtime contracts instead of direct host-specific glue
4. scene selection and query results round-trip through native scene handles and transactional scene deltas
5. graphics and compute work route through a runtime-owned render and scheduling surface rather than one-off host orchestration
6. packaging emits a native launch profile that can reopen the same workspace with deterministic runtime state

Do not expand scope beyond this slice until the slice is real.

## Phase 0: Charlie Baseline And Blocker Matrix

- [ ] 0.1 Create a blocker-to-code map under `docs/kainplan/native-runtime-three-agent/`
  - Record each blocker from [limitations.md](/M:/Code/Kain/runtime/limitations.md)
  - Map current owning files, known gaps, and likely landing zones
  - Mark each blocker as `missing`, `partial`, `host-shaped`, or `contract-only`
  - _Owner: Charlie_

- [ ] 0.2 Record the current native vertical-slice baseline
  - Document what the current Win32 viewport path can actually do
  - Record where UI bundle loading, realtime bundle loading, graphics validation, and asset ingestion already exist
  - Keep the writeup truthful about bundled-but-not-editor-grade behavior
  - _Owner: Charlie_

- [ ] 0.3 Prepare validation commands without running heavy suites
  - Collect crate-level and runtime-specific commands relevant to Alpha and Delta work
  - Mark all heavy validation as pending user-approved testing mode
  - Include lightweight compile or smoke expectations if they do not cross the repo testing-policy line
  - _Owner: Charlie_

- [ ] 0.4 Define the first-vertical-slice acceptance sheet
  - Write one acceptance checklist that all three agents share
  - Use explicit statuses: `pending`, `contracts only`, `wired`, `vertically proven`
  - _Owner: Charlie_

## Phase 1: Alpha Contract Spine

- [ ] 1.1 Add canonical native scene identity types and handles
  - Define stable runtime handle types for scene instances, entities, meshes, materials, lights, cameras, volumes, and instancers
  - Keep handles opaque at the ABI surface
  - Land declarations under `runtime/native/include/`
  - _Owner: Alpha_

- [ ] 1.2 Add transactional scene-delta receipts
  - Define mutation requests and receipts for create, update, delete, attach, detach, and selection-affecting edits
  - Include ids, timestamps or sequence markers, and failure reporting fields
  - Keep this ABI independent from any single editor widget implementation
  - _Owner: Alpha_

- [ ] 1.3 Add native scene-query contracts
  - Define picking, raycast, bounds query, visibility query, and selection-mask query request and result structs
  - Ensure the API can support viewport-bound queries without embedding Win32 message details
  - _Owner: Alpha_

- [ ] 1.4 Add device and backend reflection descriptors
  - Define runtime-facing descriptors for backend type, feature support, display modes, GPU capabilities, and hotplug-sensitive device identity
  - Thread them into the existing service-registry and compatibility surfaces where appropriate
  - _Owner: Alpha_

- [ ] 1.5 Add runtime reflection query surfaces for scene and resource inspection
  - Extend reflection APIs so tools can inspect runtime-owned scene objects, resources, and binding layouts
  - Keep runtime inspection distinct from compiler reflection artifacts while allowing them to be correlated
  - _Owner: Alpha_

- [ ] 1.6 Add native ingestion descriptors
  - Define the canonical path for compiler-emitted bundles and staged host assets to enter the native lane
  - Prefer descriptor-driven asset and bundle ingestion over ad hoc environment-variable-only resolution
  - _Owner: Alpha_

## Phase 2: Alpha Graphics And Execution Contracts

- [ ] 2.1 Define render-graph contract tables
  - Add pass, dependency, attachment, lifetime, and capture-hook descriptors
  - Keep the first version minimal and sufficient for the shared vertical slice
  - _Owner: Alpha_

- [ ] 2.2 Define native resource residency descriptors
  - Add typed descriptors for buffers, images, transient pools, streaming uploads, residency state, and budget inspection
  - Make these queryable through the runtime reflection surface
  - _Owner: Alpha_

- [ ] 2.3 Define compute scheduling and synchronization contracts
  - Add dispatch-chain, queue, barrier, and cross graphics/compute synchronization descriptors
  - Avoid host-local implicit sequencing
  - _Owner: Alpha_

- [ ] 2.4 Wire the new contract descriptors into existing native runtime core files
  - Extend current core runtime modules rather than creating a disconnected second runtime model
  - Prefer a narrow integration path through existing contract, services, reflection, graphics, and compatibility modules
  - _Owner: Alpha_

- [ ] 2.5 Publish the Alpha contract freeze for Delta consumption
  - Record the headers, structs, invariants, and anti-patterns Delta must build against
  - Mark any still-scaffolded fields honestly
  - _Owner: Alpha with Charlie recording_

## Phase 3: Delta Viewport And Workspace Surfaces

- [ ] 3.1 Refactor the Win32 viewport path around stable input contracts
  - Route mouse, keyboard, focus, and camera navigation through contract structs instead of direct host-local assumptions
  - Preserve existing functionality while removing hidden coupling where practical
  - _Owner: Delta_

- [ ] 3.2 Add workspace and dock-graph runtime surfaces
  - Define runtime-owned workspace layout, dock nodes, panel placement, focus transfer, and overlay routing
  - Persist this shape through a deterministic runtime state format
  - _Owner: Delta_

- [ ] 3.3 Add multi-viewport and multi-window coordination
  - Support more than one viewport and non-viewport panels under one runtime-owned workspace model
  - Keep the first slice focused on one main viewport plus one secondary docked panel if needed
  - _Owner: Delta_

- [ ] 3.4 Add editor-grade native UI primitives
  - Implement the minimum credible set for the vertical slice: inspector-style property surface, outliner or object list, command bar or command surface, and overlay-safe panel hosting
  - Do not over-expand into a full widget catalog in phase 1
  - _Owner: Delta_

- [ ] 3.5 Thread selection and query flow into the workspace shell
  - Use Alpha scene handles, query results, and mutation receipts end-to-end
  - The viewport, inspector, and object-list surfaces must share one runtime-owned selection model
  - _Owner: Delta_

## Phase 4: Delta Source-Driven Authoring And Packaging

- [ ] 4.1 Define source-driven scene/runtime document consumption
  - Establish how a source-authored scene or launch profile resolves into native runtime state
  - Keep the runtime-facing schema narrow and aligned with the existing bundle doctrine
  - _Owner: Delta_

- [ ] 4.2 Define source-driven widget/layout consumption
  - Establish how authored dock content, overlays, or native panels resolve into runtime workspace nodes
  - Avoid template-local panel assembly
  - _Owner: Delta_

- [ ] 4.3 Integrate source-authored inputs into the native host launch path
  - Ensure the native host can start from the authored scene and workspace surfaces instead of relying on sidecar-only orchestration
  - Reuse driver outputs where possible
  - _Owner: Delta_

- [ ] 4.4 Add native packaging and launch-profile assembly
  - Define the launch profile materialization shape for the shared vertical slice
  - Include workspace persistence inputs, scene document references, and runtime compatibility gating
  - _Owner: Delta_

- [ ] 4.5 Prove deterministic reopen behavior
  - The packaged native launch profile must reopen the same workspace layout and runtime-owned scene state without bespoke manual setup
  - _Owner: Delta_

## Phase 5: Charlie Validation, Docs, And Merge Tightening

- [ ] 5.1 Add a lane progress tracker
  - Track each Alpha and Delta task with explicit status and evidence links
  - Refuse vague completion claims
  - _Owner: Charlie_

- [ ] 5.2 Extend native runtime conformance or smoke planning
  - Add or update the smallest possible harness family that can prove the shared vertical slice
  - Keep testing commands documented even if heavy validation is waiting on user approval
  - _Owner: Charlie_

- [ ] 5.3 Update architecture docs when the lane becomes materially more real
  - Update [ARCHITECTURE.md](/M:/Code/Kain/ARCHITECTURE.md) if new runtime-owned subsystems, entrypoints, or operator commands become durable
  - Update [MEMORY.md](/M:/Code/Kain/MEMORY.md) with design decisions, risks, and next steps once multiple blockers land
  - _Owner: Charlie_

- [ ] 5.4 Write the first-vertical-slice status report
  - Record what is actually proven versus what remains `contracts only`
  - Include blockers for the next wave without inflating scope
  - _Owner: Charlie_

## Immediate Parallel Start

Start with these three lanes in parallel:

- `Alpha`: 1.1 through 1.6, then 2.1 and 2.2 if the contract shapes are clear
- `Delta`: 3.1 and design-level prep for 3.2 through 3.5 using Alpha's current service and viewport seams as evidence
- `Charlie`: 0.1 through 0.4 and 5.1 immediately

Do not let Delta hard-freeze public scene, selection, or workspace contracts before Alpha publishes the contract freeze in task 2.5.

## Completion Standard

Do not mark this plan complete when:

- new headers exist but the Win32 host still bypasses them
- a dock shell exists but selection, queries, and inspector edits still use host-local state
- packaging exists but does not reopen a deterministic workspace
- render or compute descriptors exist only as comments or dead structs
- reflection claims exist but tools still need bespoke per-panel code paths to inspect runtime state

This plan is complete only when the first shared vertical slice proves:

1. runtime-owned scene handles and scene deltas are real
2. scene queries and selection round-trip through the runtime contract layer
3. a docked native workspace with a live viewport and at least one inspector-style panel is real
4. source-authored scene and workspace inputs reach the native host without sidecar-only glue
5. render and compute execution route through explicit runtime-owned contract surfaces
6. a native launch profile can reopen the same workspace deterministically
