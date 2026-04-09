# 08 Compiler-Owned Intent Quartet

This document captures the first-pass implementation contract for the four new compiler-owned intent forms:

- `patch`
- `converge`
- `world`
- `orchestrate`

## Goal

These forms are bounded top-level semantic declarations. They let Kain own intent that other systems usually leave in build scripts, runtime glue, editor heuristics, or engine-local conventions.

## Syntax

Canonical v1 shapes:

```kain
patch move_vertex(scene: Scene, vertex_id: Int, to: Vec3) -> Vec3:
    scene.vertices[vertex_id].position = to
    return to

converge solve(value: Int) -> Int:
    spec reference:
        return value + 1
    fast interpret_lane when target("interpret"):
        return value + 1
    fast runtime_lane when capability("converge.dispatch"):
        return value + 1
    verify random(1024)

world editor:
    state scene: Scene = initial_scene()
    surface native_ui => EditorShell
    surface viewport3d => "EditorPreview"
    surface web => RemoteInspector
    surface ue5 => "EditorBridge"

orchestrate bake(value: Int) -> Int:
    let a: Int = kain solve(value)
    let b: Int = rust polish(a)
    let c: Int = python validate(b)
    let d: Int = node package(c)
    return d
```

## Semantic Rules

- `patch`
  - Typechecks like a function body.
  - Collects compiler-trackable mutation paths from assignable lvalues.
  - Infers `undo_mode` as `reversible` or `best_effort`.
- `converge`
  - Requires exactly one `spec` lane and at least one `fast` lane.
  - Fast-lane selection is deterministic: first matching `fast`, then `spec`.
  - Test lane executes verification against `spec`.
- `world`
  - Requires all four v1 surfaces: `native_ui`, `viewport3d`, `web`, and `ue5`.
  - State slots are typed and initialized at registration time.
  - Surface expressions remain authored truth and flow into bundles directly.
- `orchestrate`
  - Uses typed sequential stage bindings.
  - Stage runtime labels are semantic markers in v1; execution still reuses existing function dispatch.

## Bundle Contracts

`kain-core` now emits explicit sections in both compiler-owned bundle families:

- `RuntimeContractBundle`
  - `patches[]`
  - `converges[]`
  - `worlds[]`
  - `orchestrations[]`
- `RealtimeAppBundle`
  - `patches[]`
  - `converges[]`
  - `worlds[]`
  - `orchestrations[]`

Required capability and requirement keys introduced in this pass:

- `patch.transactions`
- `converge.dispatch`
- `world.native-ui`
- `world.viewport3d`
- `world.web`
- `world.ue5`
- `orchestrate.pipeline`

## Driver Behavior

`kain-driver` now resolves a native UI root from a `world`'s `native_ui` surface when possible.

- If exactly one world declares a `native_ui` surface, that world becomes the default root-selection source.
- If multiple worlds exist, realtime/native-ui flows now fail instead of guessing.
- `--root` can be used as an explicit override and may refer to a world name or a component name.

## Validation

Focused coverage added in this pass:

- `crates/kain-core/tests/compiler_owned_intent_test.rs`
  - parse/typecheck
  - runtime contract emission
  - realtime bundle emission
  - runtime execution
  - converge verification mismatch diagnostics
- `crates/kain-driver/src/lib.rs`
  - single-world root resolution
  - multi-world explicit-selection error
- `crates/kain-driver/src/native_app.rs`
  - native-app root discovery for single-world and multi-world cases
- `smoketest/compiler_owned_intent`
  - `kain run`
  - LLVM artifact staging
  - runtime contract and realtime bundle section assertions

## Known Limits

- `orchestrate` stage runtimes do not yet call external Rust/Python/Node adapters directly; they remain typed semantic labels over current runtime dispatch.
- `world` selection is only wired through native UI root discovery in this pass. Full per-adapter world activation is still future work.
- Existing unrelated driver/lib tests remain outside this feature lane and were not normalized here.
