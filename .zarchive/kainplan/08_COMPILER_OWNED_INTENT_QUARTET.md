# 08 Compiler-Owned Intent Suite

This document captures the current implementation contract for Kain's compiler-owned intent suite:

- `law`
- `patch`
- `converge`
- `world`
- `orchestrate`

These forms stay contextual at the declaration level instead of becoming globally reserved language keywords. The goal is not more punctuation. The goal is to let `kain-core` own semantic truth that other stacks normally scatter across build scripts, runtime glue, editor heuristics, or adapter-local conventions.

## Canonical Shapes

```kain
law manifold(mesh: Mesh) -> Bool:
    return mesh.edge_count >= 0

patch move_vertex(scene: Scene, vertex_id: Int, to: Vec3) -> Vec3:
    scene.vertices[vertex_id].position = to
    return to

converge solve(value: Int) -> Int:
    spec reference:
        return value + 1
    fast runtime_lane when capability("converge.dispatch"):
        return value + 1
    fast default_lane:
        return value + 1
    verify random(1024)

world editor:
    state scene: Scene = initial_scene()
    surface native_ui => EditorShell
    surface viewport3d => "EditorPreview"
    surface web => RemoteInspector

orchestrate bake(value: Int) -> Int:
    let a: Int = kain solve(value)
    let b: Int = rust polish(a)
    let c: Int = python validate(c)
    let d: Int = node package(c)
    return d
```

## Semantic Rules

- `law`
  - Typechecks like a function body.
  - Must return `Bool`.
  - Registers as a callable runtime value and emits into `laws[]` in both bundle families.
- `patch`
  - Typechecks like a function body.
  - Collects compiler-trackable mutation paths from assignable lvalues.
  - Infers `undo_mode` as `reversible` or `best_effort`.
  - Emits enough metadata for patch records, replay, and collaboration events.
- `converge`
  - Requires exactly one `spec` lane and at least one `fast` lane.
  - Fast-lane selection is deterministic and declaration-ordered.
  - Selector-less `fast` lanes are valid default candidates.
  - `verify random(n)` now executes for the real call arguments and for `n` deterministic synthesized samples.
  - Synthesized verification values are intentionally bounded in v1:
    - `Bool`
    - `Int` / `UInt`
    - `Float`
    - `Char`
    - tuples, arrays, and options composed only from those types
  - Unsupported parameter or return types fail typechecking when `verify random(n)` is present.
- `world`
  - Requires at least one surface, not all four.
  - Duplicate surface kinds are rejected.
  - Surface declarations are sparse and authoritative; bundles emit exactly what the source declared.
  - World state names do not leak into the global type environment. State is accessed through the world value itself.
- `orchestrate`
  - Is a strict linear pipeline.
  - Only top-level stage steps of the form `let binding: Type = <runtime> function(args)` are allowed.
  - Nested or branch-local stage calls are rejected so emitted stage metadata matches runtime execution exactly.
  - Runtime labels mean what they say:
    - `kain` dispatches to normal Kain functions
    - `rust` dispatches only to registered native functions
    - `python` dispatches only through Python bridge helpers
    - `node` dispatches only through Node bridge helpers

## Bundle Contracts

`kain-core` now emits explicit intent-suite sections in both compiler-owned bundle families.

- `RuntimeContractBundle`
  - `patches[]`
  - `laws[]`
  - `converges[]`
  - `worlds[]`
  - `orchestrations[]`
- `RealtimeAppBundle`
  - `patches[]`
  - `laws[]`
  - `converges[]`
  - `worlds[]`
  - `orchestrations[]`

Important emitted details:

- `converges[].verify_random_count` is the real enforced verification count, not dead metadata.
- `worlds[].surfaces[]` are sparse and authoritative.
- `orchestrations[].stages[]` only describe legal top-level stage steps.
- `laws[]` expose callable invariant declarations to runtimes and adapters.

Required capability and requirement keys used by the suite:

- `patch.transactions`
- `law.invariants`
- `converge.dispatch`
- `world.native-ui`
- `world.viewport3d`
- `world.web`
- `world.ue5`
- `orchestrate.pipeline`

## Driver Behavior

`kain-driver` now resolves world selection against the active target instead of treating `world` as a native-ui-only hint.

- Native desktop targets still keep the convenience behavior:
  - if exactly one world declares `native_ui`, it becomes the default active world
  - if multiple worlds declare `native_ui`, the caller must pass `--root`
- Web targets resolve against `web` surfaces.
- UE5 targets resolve against `ue5` surfaces.
- Explicit `--root` values may refer to either a world name or a component name.
- If a selected world does not declare the required surface for the active adapter target, bundle compilation fails instead of silently guessing.

## Validation

Focused validation for the suite now lives in:

- `crates/kain-core/tests/compiler_owned_intent_test.rs`
  - parse/typecheck for all five declarations
  - direct law runtime calls
  - world-state leakage regression
  - sparse world validation
  - selector-less converge lane selection
  - executable `verify random(n)` success and failure paths
  - strict orchestrate pipeline rejection cases
  - runtime-label enforcement for `rust`, `python`, and `node`
- `crates/kain-driver/src/lib.rs`
  - native-ui root selection
  - web target world selection
  - explicit-world target-surface rejection
- `crates/kain-driver/src/native_app.rs`
  - native-app root discovery and active-world propagation

## Known Limits

- `converge` still requires at least one `fast` lane in the parser.
- `verify random(n)` only synthesizes the bounded scalar/tuple/array/option subset listed above.
- Some compiled/codegen lanes still lower stage calls more simply than the interpreter; this doc describes the source/runtime contract owned by `kain-core`, not every backend's final implementation depth.
