# Compiler-Owned Intents

Snapshot: May 11, 2026.

`patch`, `law`, `converge`, `world`, `entangle`, and `orchestrate` are first-class Kain
declarations. They are not just names or documentation conventions. The core
runtime, runtime-contract emission, and realtime-bundle emission all treat them
as owned semantic objects.

## Why These Items Get Their Own Page

The point of these declarations is that authored code can describe higher-level
runtime intent directly:

- transactional mutation
- invariants and rules
- dispatch selection
- world/surface selection
- state coupling
- multi-stage cross-runtime pipelines

Those ideas show up in both `crates/kain-core/src/runtime_contract.rs` and
`crates/kain-core/src/realtime_app_bundle.rs`, so they need one canonical home
instead of being scattered across examples.
Those bundle types also carry required capabilities, service bindings, and
target/compatibility metadata alongside the intent-specific sections.

## The Six Intent Families

| Item | What it means | What the compiler emits |
| --- | --- | --- |
| `patch` | a mutation transaction with undo/replay semantics | mutation paths, replay schema, invalidation keys, collaboration event name, undo mode |
| `law` | a rule or invariant declaration | symbol, parameter types, return type |
| `converge` | a dispatcher that selects a lane or implementation | dispatcher symbol, spec lane, fast lanes, selector metadata, verification hints |
| `world` | a state-and-surface projection | state slots, surface kinds, active world selection |
| `entangle` | single-writer state coupling between stable lvalue paths | authority endpoint, mirror endpoint, policy, endpoint type, `state.entangle` capability |
| `orchestrate` | a typed multi-stage pipeline | stage runtime, function name, binding name, optional return type |

## `patch`

`patch` declarations carry the compiler-owned mutation contract.

The runtime contract and realtime bundle both track:

- mutation paths
- undo mode
- collaboration event identity
- invalidation keys
- replay-oriented history

The runtime tests in `crates/kain-driver/src/lib.rs` show that patch history,
undo, and replay are part of the runtime semantics, not just a serialized note.

## `law`

`law` declarations model invariant-style callable semantics.

The emitted contract records:

- the law name
- the stable symbol
- parameter types
- the return type

That makes laws suitable for runtime checks, validator passes, and other
semantic consumers that need a stable rule surface.

The LLVM and direct C backends now lower laws as callable functions, so Kain
code can call authored invariants from native-targeted functions instead of
only seeing laws in sidecar metadata.

## `converge`

`converge` is the dispatch family.

It records:

- a dispatcher symbol
- a spec lane
- zero or more fast lanes
- optional selector information
- optional random verification counts

The important distinction is that the spec lane is the canonical lane, while
the fast lanes are optimization lanes that can still be described explicitly in
the semantic bundle.

## `world`

`world` declarations define the state and surface projection for a target.

The runtime-contract and realtime-bundle layers track:

- state slots
- surface kinds
- the active world

The current surface kinds are:

- `NativeUi`
- `Viewport3d`
- `Web`
- `Ue5`

World selection is target-sensitive. If a program defines more than one world
for the required surface, the driver requires explicit selection instead of
guessing.

## `entangle`

`entangle` declarations model compiler-owned state coupling. V1 supports the
declaration form:

```kain
entangle Physics.player_health <-> UI.health_display with single_writer
```

The left endpoint is the authority, and the right endpoint is the mirror.
Endpoints must be stable dotted lvalue paths with the same resolved storage
type. The interpreter propagates authority writes to the mirror and rejects
direct mirror writes under `single_writer`.

The runtime contract and realtime bundle both track:

- authority endpoint
- mirror endpoint
- policy
- endpoint type
- `state.entangle` capability and requirement metadata

Native target support now has two concrete surfaces:

- LLVM emits `@__kain_register_entanglements`, called from `main`, which passes
  each binding into the native C runtime hook
  `kain_runtime_entangle_register(authority, mirror, policy, type_name)`.
- The direct C backend emits a static `KainCEntangleBinding` metadata table so
  C output preserves the same contract shape even when it is not linked through
  the LLVM runtime-registration path.

Cross-process coupling, distributed replication, and richer write barriers are
still target-adapter layers. They should consume the emitted `entanglements[]`
metadata or the native registry instead of redefining the language meaning.

## `orchestrate`

`orchestrate` is the multi-runtime pipeline item.

The stage runtime model currently includes:

- `Kain`
- `Rust`
- `Python`
- `Node`

Each stage records:

- the runtime lane
- the bound function name
- the binding name used by the pipeline

The interpreter and bundle layers treat that as a typed pipeline, not just an
ordered list of callbacks.

## What To Check In The Code

- `crates/kain-core/src/runtime_contract.rs`
- `crates/kain-core/src/realtime_app_bundle.rs`
- `crates/kain-entangle/src/lib.rs`
- `crates/kain-driver/src/lib.rs`
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
- `crates/kain-sys-codegen/src/codegen_c.rs`
- `crates/kain-core/src/ast.rs`
- `runtime/native/include/kain_runtime_entangle.h`
- `runtime/native/src/core/kain_runtime_entangle.c`

## Practical Rule

If you are documenting one of these items, do not explain it as “just syntax.”
Explain:

- which bundle fields it becomes
- which runtime lane consumes it
- which target or host decides whether it is active
