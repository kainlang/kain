# KAIN Runtime Harvest Plan - 2026-04-11

## Intent

This document defines how Kain should harvest value from the third-party runtimes currently staged under `runtime/thirdparty/`.

The goal is not to embed these runtimes unchanged.

The goal is also not to rewrite them from scratch.

The goal is to edit the vendored files in place for Kain, preserve the strongest implementation machinery they already contain, and progressively Kainize them behind Kain-owned runtime contracts.

That means:

- keep the imported code under `runtime/thirdparty/`
- patch the files directly where that produces better Kain behavior
- add thin Kain-owned wrapper and adapter layers around them
- never let foreign public APIs become the real public API of Kain

## Hard Rules

1. Kain owns language semantics.  
   No foreign runtime is allowed to define what Kain code means.

2. Kain owns the kernel.  
   Scheduler policy, diagnostics, tracing, permissions, capability validation, and runtime lifecycle stay Kain-owned.

3. Vendor edits are allowed.  
   Imported runtime files may be edited directly when the edit materially improves Kain integration, observability, sandboxing, or performance.

4. Vendor edits must still be disciplined.  
   Preserve recognizable upstream structure, add Kain-specific comments only where needed, and keep a patch ledger so future agents can tell what changed and why.

5. No foreign runtime should leak through as the default extension story.  
   Every integration must sit behind Kain service families and Kain value contracts.

## Current Inventory

## 1. QuickJS

Status: strongest current candidate for real integration

Relevant files:

- [runtime/thirdparty/quickjs/quickjs.h](/home/ephemara/Dev/Kain/runtime/thirdparty/quickjs/quickjs.h:366)
- [runtime/thirdparty/quickjs/quickjs.c](/home/ephemara/Dev/Kain/runtime/thirdparty/quickjs/quickjs.c:43)

Current value:

- real runtime/context lifecycle
- memory limits and GC threshold controls
- explicit atom/string/value APIs
- custom class and finalizer hooks
- module loader hooks
- script/module eval
- strong embeddable shape for a C runtime

Harvest priority: highest

## 2. Wren

Status: promising embed API, incomplete current import

Relevant files:

- [runtime/thirdparty/wren/src/include/wren.h](/home/ephemara/Dev/Kain/runtime/thirdparty/wren/src/include/wren.h:307)
- [runtime/thirdparty/wren/src/vm/wren_vm.c](/home/ephemara/Dev/Kain/runtime/thirdparty/wren/src/vm/wren_vm.c:44)

Current value:

- clean VM configuration API
- host-controlled module loading
- foreign method and foreign class binding model
- compact embedder ergonomics

Current blocker:

- the current import is not complete enough to build cleanly as-is
- it references missing files such as `wren_opcodes.h`, `wren_opt_meta.h`, and `wren_opt_random.h`

Harvest priority: medium-high after inventory completion

## 3. mruby

Status: useful architecture source, incomplete current import

Relevant files:

- [runtime/thirdparty/mruby/include/mruby/compile.h](/home/ephemara/Dev/Kain/runtime/thirdparty/mruby/include/mruby/compile.h:179)
- [runtime/thirdparty/mruby/include/mruby/version.h](/home/ephemara/Dev/Kain/runtime/thirdparty/mruby/include/mruby/version.h:40)
- [runtime/thirdparty/mruby/src/gc.c](/home/ephemara/Dev/Kain/runtime/thirdparty/mruby/src/gc.c:11)

Current value:

- compact embedded VM design
- configurable value representation ideas
- GC arena patterns
- good examples of embedded-first public APIs

Current blocker:

- only a small subset of source files is present
- current tree is not a full mruby runtime

Harvest priority: medium

## 4. Lua

Status: valuable reference implementation, incomplete current import

Relevant files:

- [runtime/thirdparty/lua/lua.h](/home/ephemara/Dev/Kain/runtime/thirdparty/lua/lua.h:163)
- [runtime/thirdparty/lua/lvm.c](/home/ephemara/Dev/Kain/runtime/thirdparty/lua/lvm.c:1)
- [runtime/thirdparty/lua/ltable.c](/home/ephemara/Dev/Kain/runtime/thirdparty/lua/ltable.c:1)

Current value:

- table/hash runtime design
- coroutine API and execution model
- strong minimal C embedding philosophy
- fast VM discipline and interpreter structure

Current blocker:

- this is not a complete Lua import
- only selected internals and a couple of `.c` files are present

Harvest priority: medium

## 5. CPython

Status: idea source only in current state

Relevant files:

- [runtime/thirdparty/cpython/Objects/dictobject.c](/home/ephemara/Dev/Kain/runtime/thirdparty/cpython/Objects/dictobject.c:1)

Current value:

- dictionary/object-map implementation ideas
- resizing and lookup strategy ideas

Current blocker:

- current import is a fragment, not an embeddable runtime lane

Harvest priority: narrow and targeted

## Vendor-Edit Policy

The operating rule for `runtime/thirdparty/` is:

`vendor, complete, patch, wrap, validate`

That means:

1. Vendor  
   Keep upstream-ish source trees under `runtime/thirdparty/<runtime>/`.

2. Complete  
   If an imported runtime is missing critical files, complete the import before treating it as a build target.

3. Patch  
   Edit the vendored files directly where Kain needs:
   - memory hooks
   - tracing hooks
   - diagnostics hooks
   - sandboxing checks
   - module resolution hooks
   - capability checks
   - scheduler integration points
   - foreign-object bridging

4. Wrap  
   Expose Kain-owned services and headers outside the vendor tree.

5. Validate  
   Every patched runtime needs:
   - compile validation
   - conformance tests
   - performance baselines
   - ownership and shutdown validation

## What “Edit In Place” Means

Allowed direct edits inside vendor trees:

- patch allocator entrypoints to route through Kain-owned memory and telemetry hooks
- patch error reporting to emit Kain diagnostics
- patch module loading to route through Kain manifests and service lookup
- patch interruption hooks for timeouts, cancellation, and permissions
- patch class/object finalization paths for Kain handle ownership
- patch build toggles and feature masks for smaller or safer Kain-oriented surfaces
- patch thread, IO, and host assumptions that conflict with Kain’s kernel

Not allowed:

- exporting the raw foreign API as the primary Kain extension contract
- duplicating Kain runtime state inside hidden foreign singletons without registry visibility
- letting foreign schedulers or GC policies silently drive core Kain services

## Kain-Owned Service Families To Build Around These Imports

These vendor runtimes should map into explicit Kain families:

- `script.quickjs`
- `script.wren`
- `script.mruby`
- `script.lua`
- `data.dict.experimental`
- later `script.python` through a real full import or host bridge, not the current fragment

Each family should speak:

- Kain diagnostics
- Kain capability validation
- Kain tracing
- Kain value ABI
- Kain permissions and lifecycle

## Harvest Matrix

## QuickJS Harvest Plan

Primary target: `script.quickjs`

What to keep and adapt:

- runtime/context split from [quickjs.h](/home/ephemara/Dev/Kain/runtime/thirdparty/quickjs/quickjs.h:366)
- allocator and memory limit hooks from [quickjs.h](/home/ephemara/Dev/Kain/runtime/thirdparty/quickjs/quickjs.h:369)
- atom and string machinery from [quickjs.h](/home/ephemara/Dev/Kain/runtime/thirdparty/quickjs/quickjs.h:447)
- custom class/finalizer hooks from [quickjs.h](/home/ephemara/Dev/Kain/runtime/thirdparty/quickjs/quickjs.h:520)
- module loading hooks from [quickjs.h](/home/ephemara/Dev/Kain/runtime/thirdparty/quickjs/quickjs.h:947)
- memory accounting and dump surfaces from [quickjs.h](/home/ephemara/Dev/Kain/runtime/thirdparty/quickjs/quickjs.h:428)

What to Kainize first:

- inject Kain allocator wrappers
- inject Kain diagnostics on exception conversion
- replace raw module resolution with Kain-controlled module/capability lookup
- add interruption and execution-budget enforcement
- map JS values to the future Kain runtime value ABI
- attach runtime and context opaque payloads to Kain service/module records

What to reject:

- QuickJS becoming the default dynamic module system for all of Kain
- QuickJS object identity becoming Kain object identity

Best use:

- scriptable host automation
- runtime tool and MCP adapters
- JS-authored plugin logic under Kain policy
- fast iteration lane for dynamic features

## Lua Harvest Plan

Primary targets:

- `core.values` inspiration
- `data.dict.experimental`
- `fiber.experimental`
- eventually `script.lua` if the full runtime is imported

What to keep and adapt:

- table layout and hashing ideas from [ltable.c](/home/ephemara/Dev/Kain/runtime/thirdparty/lua/ltable.c:1)
- VM dispatch and value discipline from [lvm.c](/home/ephemara/Dev/Kain/runtime/thirdparty/lua/lvm.c:1)
- coroutine API shape from [lua.h](/home/ephemara/Dev/Kain/runtime/thirdparty/lua/lua.h:312)
- lean host API philosophy from [lua.h](/home/ephemara/Dev/Kain/runtime/thirdparty/lua/lua.h:163)

What to Kainize first:

- prototype a Kain hash/table implementation informed by Lua’s table design
- prototype a Kain fiber/coroutine lane informed by Lua’s resume/yield shape
- borrow dispatch and tagged-value layout lessons for `core.values`

What to reject:

- exposing the Lua stack API as a Kain public contract
- adopting Lua semantics or metatable semantics as Kain semantics

## Wren Harvest Plan

Primary target: `script.wren`

What to keep and adapt:

- clean config object from [wren.h](/home/ephemara/Dev/Kain/runtime/thirdparty/wren/src/include/wren.h:272)
- load-module callback pattern from [wren.h](/home/ephemara/Dev/Kain/runtime/thirdparty/wren/src/include/wren.h:186)
- foreign method/class binding surface from [wren.h](/home/ephemara/Dev/Kain/runtime/thirdparty/wren/src/include/wren.h:203)
- embedder-facing lifecycle from [wren.h](/home/ephemara/Dev/Kain/runtime/thirdparty/wren/src/include/wren.h:307)

What to Kainize first:

- complete the vendor import
- add Kain diagnostics and tracing to configuration callbacks
- route module load and foreign binding through Kain service lookup
- bind Kain-owned handles as foreign objects under explicit ownership rules

What to reject:

- Wren’s object model becoming the general Kain value model
- treating Wren scripts as the main plugin ABI

## mruby Harvest Plan

Primary targets:

- `core.values`
- `core.memory`
- `script.mruby`

What to keep and adapt:

- compact embedded runtime shape
- value and boxing strategies from [mruby/value.h](/home/ephemara/Dev/Kain/runtime/thirdparty/mruby/include/mruby/value.h:1)
- GC arena and region ideas from [mruby/gc.h](/home/ephemara/Dev/Kain/runtime/thirdparty/mruby/include/mruby/gc.h:41)
- compile/load API shape from [compile.h](/home/ephemara/Dev/Kain/runtime/thirdparty/mruby/include/mruby/compile.h:179)

What to Kainize first:

- complete the vendor import if runtime use becomes real
- evaluate whether mruby’s value packing ideas improve Kain’s future runtime value ABI
- adapt arena/GC guard patterns for embedded-script subruntime management

What to reject:

- pulling mruby internals into core Kain before Kain’s own value ABI is defined

## CPython Dict Harvest Plan

Primary target: `data.dict.experimental`

What to keep and adapt:

- dict growth, probing, and cache-locality ideas from [dictobject.c](/home/ephemara/Dev/Kain/runtime/thirdparty/cpython/Objects/dictobject.c:1)

What to Kainize first:

- use CPython dict behavior as a benchmark/reference source for Kain maps
- compare against Lua table-inspired designs before choosing the default runtime map

What to reject:

- any attempt to treat this fragment as a Python runtime integration plan

## Execution Order

## Phase 0 - Provenance And Completeness

Deliverables:

- add a runtime-thirdparty inventory doc with source URL, upstream commit/version, license, completeness state, and intended Kain subsystem
- complete or prune partial imports
- add per-runtime patch ledgers

Exit criteria:

- every third-party tree is either buildable, explicitly reference-only, or removed

## Phase 1 - QuickJS First

Deliverables:

- create a Kain wrapper around the current QuickJS import
- patch QuickJS allocation, diagnostics, and module resolution points
- add a `script.quickjs` service family
- add smoke tests for eval, module import, timeout/cancel, and host object binding

Exit criteria:

- QuickJS is a real Kain runtime service, not just a vendor folder

## Phase 2 - Data Structure Harvest

Deliverables:

- build a Kain experimental map/table benchmark lane using Lua and CPython ideas
- document the chosen default map strategy for Kain runtime values and registries

Exit criteria:

- Kain map/runtime-object storage stops being accidental

## Phase 3 - Wren And mruby Completion Decision

Deliverables:

- decide whether to complete and operationalize Wren and mruby imports
- if yes, finish vendor trees and add Kain wrappers
- if not, mark them as reference-only and keep a smaller curated subset

Exit criteria:

- no ambiguous half-runtime state remains

## Phase 4 - Shared Script Runtime Contract

Deliverables:

- one Kain script-host contract shared by QuickJS, Wren, mruby, and Lua
- common service keys for module load, permissions, diagnostics, tracing, values, and host bindings
- common lifecycle APIs for create, configure, eval, call, collect, shutdown

Exit criteria:

- script runtimes are interchangeable behind Kain-owned contracts

## Build And Validation Requirements

Every runtime harvested into active use must get:

- a manifest entry or companion metadata
- a Kain-owned wrapper header and source file
- compile checks on Linux first, then Windows and macOS as applicable
- lifecycle tests
- ownership and shutdown tests
- cancellation and timeout tests
- diagnostics and tracing validation
- performance baselines

## Repo Changes This Plan Implies

Near-term files and folders that should appear once execution starts:

- `runtime/thirdparty/INVENTORY.md`
- `runtime/thirdparty/<runtime>/PATCHLOG.md`
- `runtime/native/include/kain_runtime_script.h`
- `runtime/native/src/core/kain_runtime_script_*.c`
- `runtime/conformance/script_runtime/`

## Recommended First Slice

The first concrete implementation slice should be:

1. inventory and provenance pass across `runtime/thirdparty/`
2. QuickJS wrapper and service-key design
3. QuickJS vendor patches for allocator, diagnostics, module resolution, and cancellation
4. `script.quickjs` conformance lane
5. table/map benchmark lane using Lua and CPython dict ideas

That gives Kain one real dynamic scripting lane and one real harvested data-structure lane without turning the runtime into an unmanaged pile of foreign engines.
