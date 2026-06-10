# Entangle — Compiler-Owned State Coupling

**Status:** Research synthesis, 2026-06-07
**Based on:** `crates/entangle/src/lib.rs`, `crates/core/src/parser.rs`, `crates/core/src/types.rs`, `crates/core/src/runtime_contract.rs`, `crates/core/src/runtime.rs`, `crates/core/src/realtime_app_bundle.rs`, `crates/sys-codegen/src/codegen_llvm/mod.rs`, `runtime/native/include/entangle.h`, `runtime/native/src/core/entangle.c`, `runtime/native/src/core/stdlib_abi.c`, `stdlib/intent.kn`, `smoketest/src/semantics/entangle.kn`, `benchmark/cases_v2/fusion_chain.kn`, `blades/experiments/cortical_sieve/`, `blades/python/24_tet/src/resonate_py_effects.kn`, CATALOG.MD, RULEBOOK.MD, lang-semantics SKILL

______________________________________________________________________

## Table of Contents

1. [What is Entangle?](#what-is-entangle)
1. [The Core Idea — Compiler-Owned State Coupling](#the-core-idea)
1. [Syntax Reference](#syntax-reference)
1. [How It Works — Full Pipeline](#how-it-works)
1. [The Policy Layer — SingleWriter](#the-policy-layer)
1. [Endpoint Resolution — Type Matching](#endpoint-resolution)
1. [Runtime Contract Metadata](#runtime-contract-metadata)
1. [Native C Runtime Registry](#native-c-runtime)
1. [LLVM Codegen — Propagation](#llvm-codegen)
1. [Stdlib Telemetry API](#stdlib-telemetry-api)
1. [The Dual-World Authority+Mirror Pattern](#the-dual-world-pattern)
1. [Fusion Patterns](#fusion-patterns)
1. [Under-Utilized Ideas — Creative Semantic Abuse](#under-utilized-ideas)
1. [Combining with Other Keywords](#combining-with-other-keywords)
1. [Compilation and Validation Ladder](#validation-ladder)
1. [Z3 Proofs](#z3-proofs)
1. [Source Anchors](#source-anchors)
1. [Anti-Patterns](#anti-patterns)
1. [Quick Reference](#quick-reference)

______________________________________________________________________

## What is Entangle?

`entangle` is a **top-level declaration** in Kain that establishes **compiler-owned state coupling** between two world fields. When one world field is written (the *authority*), its value is automatically propagated to the coupled world field (the *mirror*) — at zero cost, through the compiler's observer graph, not a heap observer registry.

```kn
entangle Authority.signal <-> Mirror.signal_copy with single_writer
```

The key insight: **this is not event-driven, not a callback, not a message, and not a data pipeline.** It is a **compile-time coupling relationship** that the compiler and runtime own end-to-end. The runtime registry is bounded (128 entries, proven by Z3), and propagation is recorded via a global counter.

The entangle graph lives in `crates/entangle/src/lib.rs` — a portable Rust crate that owns the policy and endpoint graph. Language meaning stays in `kain-core`; target adapters consume these descriptors without inventing a second entanglement model.

______________________________________________________________________

## The Core Idea

### What Entangle Is NOT

- ❌ Not a message channel (that's `teleport` with a channel or actor `send`)
- ❌ Not a callback or event listener (that's `resonate`)
- ❌ Not a data sync pipeline (that's `orchestrate`)
- ❌ Not a mutable global (that's just a `world`)
- ❌ Not manual synchronization code (that's a bug)

### What Entangle IS

- ✅ A **compile-time coupling declaration** between two world field endpoints
- ✅ A **single-writer policy** that prevents mirror writes at the typechecker level
- ✅ A **runtime-propagated value copy** from authority to mirror
- ✅ A **bounded, Z3-proven native registry** (128 max bindings)
- ✅ A **telemetry counter** (`entangle_propagation_count()`) that proves the coupling fired
- ✅ A **portable policy graph** in `crates/entangle` that any target adapter can consume

### The Relationship View

| Construct | Domain-Locked View | Relationship View |
|---|---|---|
| `entangle` | "State mirroring between worlds" | **Compile-time coupling** — any bidirectional state relationship with policy, automatically propagated, with Z3-proven bounded registry and telemetry counters |

When you write `entangle A.field <-> B.field with single_writer`, you are telling the compiler:

> "These two fields are coupled. Field A is the authority. Field B is the mirror. The compiler must ensure that writes to A propagate to B, and that no one can write to B directly."

This is the **antithesis of manual synchronization** — you don't write the propagation code, you don't check that you're writing the right mirror, and you don't wire up event handlers. The compiler owns the coupling.

______________________________________________________________________

## Syntax Reference

### Canonical Syntax

```kn
entangle <Endpoint> <-> <Endpoint> with <Policy>
```

Where:

- `<Endpoint>` is a dotted path like `WorldName.field_name` (at least 2 segments)
- `<->` is the coupling arrow (parsed as `<`, `->`)
- `with` introduces the policy keyword
- `<Policy>` is currently only `single_writer`

### Concrete Examples

```kn
// Basic dual-world coupling
entangle Authority.signal <-> Mirror.signal_copy with single_writer
entangle Authority.epoch <-> Mirror.epoch_copy with single_writer

// Multiple fields per world pair
entangle FxWorld.lfo1_phase <-> FxWorldMirror.lfo1_phase_copy with single_writer
entangle FxWorld.distortion_drive <-> FxWorldMirror.distortion_drive_copy with single_writer
entangle FxWorld.effect_mix <-> FxWorldMirror.effect_mix_copy with single_writer

// Entangle in experiment telemetry worlds
entangle RatTelemetry.best_distance <-> RatTelemetryMirror.best_distance_copy with single_writer
```

### Why `WorldName.field_name`?

Entangle endpoints must be **dotted paths with at least 2 segments**. The parser explicitly requires this:

```
Entangle endpoint must be a stable dotted lvalue path like World.state
```

This is because entangle only makes sense between world fields. A bare identifier or single-segment path cannot participate in an entangle.

### Grammar (from parser.rs)

```text
entangle <left_endpoint> <-> <right_endpoint> with single_writer

left_endpoint = ident ( "." ident )+     // at least 2 segments
right_endpoint = ident ( "." ident )+    // at least 2 segments
```

The parser:

1. Expects contextual keyword `entangle`
1. Parses left endpoint via `parse_entangle_endpoint()` — a dot-separated ident chain with minimum 2 segments
1. Expects `<` then `->`
1. Parses right endpoint
1. Expects `with`
1. Matches `single_writer` policy
1. Returns `Item::Entangle(EntangleDef { left, right, policy, ... })`

______________________________________________________________________

## How It Works — Full Pipeline

### Source to Runtime Flow

```
.kn entangle declaration
  → parser.rs: parse_entangle()
    → EntangleDef { left, right, policy } in AST
  → types.rs: check_entangle()
    → Type-checks endpoints (must be world fields, matching types)
    → Deduplicates endpoints (no field in multiple entangles)
    → TypedEntangle { ast, endpoint_type_name, endpoint_type }
  → runtime_contract.rs: collect_entangle_contracts()
    → RuntimeEntangleContract { authority, mirror, policy, type_name }
  → runtime.rs: register_entangle_value()
    → Registers in EntangleGraph (from kain_entangle crate)
    → Ensures state.entangle capability
  → codegen_llvm/mod.rs: collect_native_entanglements()
    → NativeEntangleBinding for LLVM generation
  → codegen_llvm/mod.rs: emit_entangle_i64_propagation()
    → Generates LLVM IR for i64 propagation
    → Emits abi_entangle_record_i64() call for telemetry
  → runtime/native: entangle_registry_register()
    → Stores in bounded C array (ENTANGLE_MAX_BINDINGS = 128)
  → runtime/native: abi_entangle_record_i64()
    → Increments propagation counter, records last authority/mirror
```

### At Compile Time

The typechecker (`types.rs: check_entangle`) performs these checks:

1. **Endpoint uniqueness** — each endpoint participates in at most one entangle
1. **Endpoint exists** — each endpoint root must be a declared world
1. **Endpoint fields exist** — each field segment must resolve to a valid struct field
1. **Type matching** — both endpoint types must match (after `peel_shared_refs`)
1. **No self-entanglement** — `A.x <-> A.x` is rejected as duplicate endpoint

```rust
fn check_entangle(env: &mut TypeEnv, entangle: &EntangleDef) -> KainResult<TypedEntangle> {
    let left_path = entangle.left.authored_path();
    let right_path = entangle.right.authored_path();

    // 1. Deduplicate endpoints
    for path in [&left_path, &right_path] {
        if !env.entangle_endpoints.insert(path.clone()) {
            return Err("entangle endpoint '{path}' is already coupled");
        }
    }

    // 2. Resolve types
    let left_ty = resolve_entangle_endpoint_type(env, &entangle.left)?;
    let right_ty = resolve_entangle_endpoint_type(env, &entangle.right)?;

    // 3. Type check (peel shared refs)
    if peel_shared_refs(&left_ty) != peel_shared_refs(&right_ty) {
        return Err("entangle endpoint expected {left_ty}, found {right_ty}");
    }

    Ok(TypedEntangle { ... })
}
```

### At Runtime (Interpreter)

The interpreter runtime (`runtime.rs`) handles entangle through:

- **`register_entangle_value`** — registers the binding in the `EntangleGraph` on program load
- **`propagate_entangled_write`** — called after any world field write, propagates to all mirrors
- **`ensure_entangle_write_allowed`** — called before any world field write, blocks mirror writes
- **`assign_entangled_endpoint`** — performs the actual value assignment on the mirror

The `EntangleGraph` (from `crates/entangle`) maintains:

- `bindings` — all registered binding descriptors
- `mirrors_by_authority` — map from authority to its mirrors
- `authority_by_mirror` — map from mirror to its authority
- `claimed_endpoints` — set of all endpoints to prevent duplicates

### At Runtime (Native LLVM)

The LLVM codegen (`codegen_llvm/mod.rs`) handles entangle through:

- **`collect_native_entanglements`** — collects all `NativeEntangleBinding` from the typed AST
- **`emit_entangle_i64_propagation`** — emits LLVM IR that:
  1. Ensures the mirror world is initialized (calls its init function)
  1. Gets a GEP pointer to the mirror field
  1. Stores the propagated value
  1. Calls `abi_entangle_record_i64()` for telemetry
- **`native_entangle_authority_binding`** / **`native_entangle_mirror_binding`** — look up bindings by endpoint path

The LLVM IR generated looks like:

```llvm
call void @__kain_world_Mirror_init()
%mirror_ptr = getelementptr inbounds %Mirror, %Mirror* @Mirror_global, i32 0, i32 <field_index>
store i64 %value, i64* %mirror_ptr
%status = call i64 @abi_entangle_record_i64(i8* @".authority.path", i8* @".mirror.path", i64 %value)
```

______________________________________________________________________

## The Policy Layer — SingleWriter

### Currently Available Policies

| Policy | Value | Description |
|--------|-------|-------------|
| `single_writer` | `"single_writer"` | Authority writes, mirror receives; mirror writes are rejected |

### How Single-Writer Works

1. **Registration**: When `EntangleGraph::register()` is called with `SingleWriter`, it:

   - Adds the mirror to `mirrors_by_authority[authority]`
   - Records the authority in `authority_by_mirror[mirror]`

1. **Write enforcement**: Before any world field write, the interpreter calls `ensure_write_allowed()`:

   - If the endpoint is a mirror (found in `authority_by_mirror`), the write is **denied**
   - Error: `"cannot write entangle mirror '{endpoint}' directly under single_writer; write authority '{authority}' instead"`

1. **Propagation**: After a successful write to an authority endpoint, `propagate_entangled_write()`:

   - Looks up `mirrors_for_authority(endpoint)`
   - Calls `assign_entangled_endpoint()` for each mirror with the new value

### Future Policy Extensibility

The `EntanglePolicy` enum in `crates/entangle/src/lib.rs` is designed for extensibility:

```rust
pub enum EntanglePolicy {
    SingleWriter,
    // Future: MultiWriter, LastWriterWins, Crdt, etc.
}
```

The `EntangleBindingDescriptor` already carries a generic policy field, and the parser is structured to accept new policy keywords via the `kain_entangle::SINGLE_WRITER_POLICY` constant. Adding a new policy requires:

1. Add a variant to `EntanglePolicy`
1. Add the policy string constant
1. Add the parser match arm
1. Implement the propagation/synchronization strategy

______________________________________________________________________

## Endpoint Resolution — Type Matching

### How Endpoints Are Resolved

The function `resolve_entangle_endpoint_type` in `types.rs` walks the dotted path:

1. **Root lookup**: The first segment is looked up via `env.lookup(root)` — must resolve to a world
1. **Field traversal**: Each subsequent segment is resolved as a struct field via `peel_shared_refs` + struct field lookup
1. **Type comparison**: After resolution, both endpoint types are compared with `peel_shared_refs`

### What Types Can Be Entangled

Any type that appears as a world state field can be entangled. The typechecker compares types after stripping shared references. This means:

- `Int` ↔ `Int` ✅
- `Bool` ↔ `Bool` ✅
- `String` ↔ `String` ✅
- `WorldA.signal: Int` ↔ `WorldB.signal: Int` ✅ (same type)
- `WorldA.signal: Int` ↔ `WorldB.name: String` ❌ (type mismatch)
- `WorldA.signal` ↔ `WorldA.signal` ❌ (self-entanglement / duplicate endpoint)

The type comparison uses `peel_shared_refs` to normalize types before comparison:

```rust
if peel_shared_refs(&left_ty) != peel_shared_refs(&right_ty) {
    return Err(env.type_error(
        format!(
            "entangle endpoint expected {}, found {}",
            describe_type(&left_ty),
            describe_type(&right_ty)
        ),
        entangle.right.span,
    ));
}
```

______________________________________________________________________

## Runtime Contract Metadata

When an entangle is compiled, it produces a `RuntimeEntangleContract` that is included in the program's runtime contract bundle:

```rust
pub struct RuntimeEntangleContract {
    pub authority: String,      // "Authority.signal"
    pub mirror: String,         // "Mirror.signal_copy"
    pub policy: String,         // "single_writer"
    pub type_name: String,      // "Int", "Bool", etc.
}
```

This is generated by `collect_entangle_contracts()` which walks all typed items, collects all `TypedItem::Entangle` entries (including those nested in modules), sorts by authority then mirror, and produces the contract list.

The runtime contract also emits the `state.entangle` capability when any entangle is present:

```rust
if summary.entanglements > 0 {
    add_capability(
        kain_entangle::STATE_ENTANGLE_CAPABILITY,  // "state.entangle"
        Some("Program declares compiler-owned state entanglement semantics."),
    );
}
```

### Realtime App Bundle

In `realtime_app_bundle.rs`, each entangle produces a `RealtimeEntangleBinding` for the realtime app manifest:

```rust
fn realtime_entangle_binding(entangle: &TypedEntangle) -> RealtimeEntangleBinding {
    RealtimeEntangleBinding {
        authority: entangle.ast.left.authored_path(),
        mirror: entangle.ast.right.authored_path(),
        policy: entangle.ast.policy.as_str().to_string(),
        type_name: entangle.endpoint_type_name.clone(),
    }
}
```

This ensures the runtime contract and realtime bundles both carry the entangle topology for downstream tooling, inspection, and gate validation.

______________________________________________________________________

## Native C Runtime

### Registry Header (`runtime/native/include/entangle.h`)

```c
#define ENTANGLE_MAX_BINDINGS 128
#define ENTANGLE_MAX_PATH 256
#define ENTANGLE_MAX_POLICY 64
#define ENTANGLE_MAX_TYPE 128

typedef struct KainRuntimeEntangleBinding {
    char authority[ENTANGLE_MAX_PATH];
    char mirror[ENTANGLE_MAX_PATH];
    char policy[ENTANGLE_MAX_POLICY];
    char type_name[ENTANGLE_MAX_TYPE];
} KainRuntimeEntangleBinding;

void entangle_registry_reset(void);
size_t entangle_registry_count(void);
int entangle_registry_register(const char* authority, const char* mirror,
                                const char* policy, const char* type_name);
int entangle_registry_get(size_t index, KainRuntimeEntangleBinding* out_binding);
```

**Constants:**

- `ENTANGLE_MAX_BINDINGS` = 128 — Z3-proven capacity
- `ENTANGLE_MAX_PATH` = 256 — max length for authority/mirror path strings
- `ENTANGLE_MAX_POLICY` = 64 — max length for policy string
- `ENTANGLE_MAX_TYPE` = 128 — max length for type name string

### Registry Implementation (`runtime/native/src/core/entangle.c`)

```c
static KainRuntimeEntangleBinding g_kain_entangle_bindings[ENTANGLE_MAX_BINDINGS];
static size_t g_kain_entangle_binding_count = 0;
```

**Key functions:**

- **`entangle_registry_reset()`** — zeroes all bindings and resets count
- **`entangle_registry_count()`** — returns current binding count
- **`entangle_registry_register()`** — registers a new binding with bounds-checked string copies
- **`entangle_registry_get()`** — retrieves a binding by index

The string copy function `runtime_copy_entangle_text` has Z3 proofs:

- `native-entangle-copy-text-fits-destination-before-null-copy.yaml` — proves copy text fits before null termination
- `native-entangle-register-count-stays-within-capacity.yaml` — proves register count stays within capacity

### Telemetry Counters (`runtime/native/src/core/stdlib_abi.c`)

```c
static int64_t g_kain_native_entangle_propagation_count = 0;
static char g_kain_native_entangle_last_authority[256] = "";
static char g_kain_native_entangle_last_mirror[256] = "";

int64_t abi_entangle_record_i64(const char* authority, const char* mirror, int64_t value) {
    (void)value;
    abi_copy_text(g_kain_native_entangle_last_authority,
                  sizeof(g_kain_native_entangle_last_authority), authority);
    abi_copy_text(g_kain_native_entangle_last_mirror,
                  sizeof(g_kain_native_entangle_last_mirror), mirror);
    g_kain_native_entangle_propagation_count += 1;
    return g_kain_native_entangle_propagation_count;
}

int64_t abi_entangle_propagation_count(void) {
    return g_kain_native_entangle_propagation_count;
}

const char* abi_entangle_last_authority(void) { ... }
const char* abi_entangle_last_mirror(void) { ... }
```

______________________________________________________________________

## LLVM Codegen — Propagation

The LLVM codegen (`codegen_llvm/mod.rs`) handles entangle propagation for native targets.

### Native Entangle Binding Collection

During LLVM codegen initialization, `collect_native_entanglements()` walks the typed AST and creates `NativeEntangleBinding` entries:

```rust
struct NativeEntangleBinding {
    authority: String,
    mirror: String,
    policy: String,
    type_name: String,
}
```

### Propagation Emission

When a world field write is compiled, the codegen checks if the written field is an **authority** endpoint via `native_entangle_authority_binding()`. If it is, it emits `emit_entangle_i64_propagation()` which:

1. Looks up the mirror world's init function
1. Gets a GEP (GetElementPtr) into the mirror's global struct at the correct field index
1. Stores the propagated value
1. Calls `abi_entangle_record_i64()` for telemetry

The field index is determined by `self.field_index(mirror_world, mirror_field)` which looks up the world struct's member layout.

### Lookup Functions

- **`native_entangle_authority_binding(path)`** — finds the binding where `path` is the authority side
- **`native_entangle_mirror_binding(path)`** — finds the binding where `path` is the mirror side (used for write guard generation)

______________________________________________________________________

## Stdlib Telemetry API

The stdlib exposes entangle telemetry through `std::intent`:

### Low-Level ABI Functions (private @extern)

| Function | Signature | Description |
|----------|-----------|-------------|
| `abi_entangle_propagation_count` | `() -> Int` | Total number of entangle propagations |
| `abi_entangle_last_authority` | `() -> String` | Last authority endpoint that propagated |
| `abi_entangle_last_mirror` | `() -> String` | Last mirror endpoint that received propagation |
| `abi_entangle_reset` | `() -> Int` | Reset entangle state |
| `abi_entangle_registered_count` | `() -> Int` | Number of registered entangle bindings |
| `abi_entangle_register` | `(String, String, String, String) -> Int` | Register a binding |
| `abi_entangle_get_authority` | `(Int) -> String` | Get authority for registered index |
| `abi_entangle_get_mirror` | `(Int) -> String` | Get mirror for registered index |
| `abi_entangle_get_policy` | `(Int) -> String` | Get policy for registered index |
| `abi_entangle_get_type_name` | `(Int) -> String` | Get type name for registered index |

### Public Wrappers

| Function | Signature | Description |
|----------|-----------|-------------|
| `entangle_propagation_count()` | `() -> Int` | How many times any entangle propagated |
| `entangle_last_authority()` | `() -> String` | Last authority path |
| `entangle_last_mirror()` | `() -> String` | Last mirror path |
| `entangle_reset()` | `() -> Int` | Reset all entangle state |
| `entangle_registered_count()` | `() -> Int` | How many bindings registered |
| `entangle_has_bindings()` | `() -> Bool` | Whether any bindings exist |
| `entangle_get_authority(index)` | `(Int) -> String` | Query binding details |
| `entangle_get_mirror(index)` | `(Int) -> String` | Query binding details |
| `entangle_get_policy(index)` | `(Int) -> String` | Query binding details |
| `entangle_get_type_name(index)` | `(Int) -> String` | Query binding details |

### Usage in Kain

```kn
use std::intent

fn prove_entangle_fired() -> Int:
    let before = entangle_propagation_count()
    // ... trigger an entangle write ...
    let delta = entangle_propagation_count() - before
    if delta < 1:
        return -11    // entangle never propagated — error code
    return 0
```

This is the canonical **telemetry delta guard** pattern from the fusion chain benchmark:

```kn
let resonate_before = resonate_fire_count()
let entangle_before = entangle_propagation_count()
let teleport_before = runtime_machine_teleport_count()
let patch_before = patch_journal_count()
let orchestrate_before = orchestrate_stage_count()

// ... run the causal chain ...

let entangle_delta = entangle_propagation_count() - entangle_before
if entangle_delta < 1: return -11    // entangle never propagated
```

______________________________________________________________________

## The Dual-World Pattern

### Core Pattern: Authority + Mirror

The canonical entangle pattern pairs two worlds:

```kn
// AUTHORITY: owns mutable state, has native_ui surface
world RenderAuthority:
    state frame: Int = 0
    state particles: Int = 65536
    surface native_ui => RenderPanel

// MIRROR: receives state via entangle, has web surface for read-only
world RenderMirror:
    state frame_copy: Int = 0
    state particles_copy: Int = 65536
    surface web => RenderPanel

// Coupling: single_writer means authority → mirror only
entangle RenderAuthority.frame <-> RenderMirror.frame_copy with single_writer
entangle RenderAuthority.particles <-> RenderMirror.particles_copy with single_writer
```

**Why this pattern exists:**

- The authority world owns mutable state (typically with a `native_ui` surface for the main thread)
- The mirror world receives state via entangle propagation and exposes it through a `web` surface for read-only inspection
- The compiler-owned observer graph handles propagation at zero cost
- Mirror writes are rejected at compile time by the typechecker

### Telemetry World Pattern

```kn
world RatTelemetry:
    state frame: Int = 0
    state best_distance: Int = 0
    state best_lane: Int = 0
    surface native_ui => SpeculativeScentVisualizer

world RatTelemetryMirror:
    state frame_copy: Int = 0
    state best_distance_copy: Int = 0
    state best_lane_copy: Int = 0
    surface web => SpeculativeScentVisualizer

entangle RatTelemetry.frame <-> RatTelemetryMirror.frame_copy with single_writer
entangle RatTelemetry.best_distance <-> RatTelemetryMirror.best_distance_copy with single_writer
entangle RatTelemetry.best_lane <-> RatTelemetryMirror.best_lane_copy with single_writer
```

The world holds experiment telemetry counters and raw buffer pointers. The mirror exposes them for read-only web visualization. Entangle synchronizes them automatically.

### Effects Engine Pattern (3+ fields)

```kn
world FxWorld:
    state lfo1_phase: Int = 0
    state lfo1_rate: Int = 10
    state distortion_drive: Int = 500
    state effect_mix: Int = 16384
    state fx_epoch: Int = 0
    surface native_ui => FxPanel

world FxWorldMirror:
    state lfo1_phase_copy: Int = 0
    state lfo1_rate_copy: Int = 10
    state distortion_drive_copy: Int = 500
    state effect_mix_copy: Int = 16384
    state fx_epoch_copy: Int = 0
    surface web => FxPanel

entangle FxWorld.lfo1_phase <-> FxWorldMirror.lfo1_phase_copy with single_writer
entangle FxWorld.lfo1_rate <-> FxWorldMirror.lfo1_rate_copy with single_writer
entangle FxWorld.distortion_drive <-> FxWorldMirror.distortion_drive_copy with single_writer
entangle FxWorld.effect_mix <-> FxWorldMirror.effect_mix_copy with single_writer
entangle FxWorld.fx_epoch <-> FxWorldMirror.fx_epoch_copy with single_writer
```

### Rules for Naming

The convention in the codebase is to name mirror fields with a `_copy` suffix (`signal_copy`, `frame_copy`, `phase_copy`). This is a convention, not a requirement — any matching type works.

______________________________________________________________________

## Fusion Patterns

### Pattern 1: Entangle + Resonate + Orchestrate (The Trinity)

The most powerful entangle fusion is when a world field is both entangled AND has a resonate handler. The propagation chain becomes:

```kn
world FusionAuthority:
    state signal: Int = 1
    state tick: Int = 0
    surface native_ui => FusionPanel

world FusionMirror:
    state signal_copy: Int = 1
    surface web => FusionPanel

entangle FusionAuthority.signal <-> FusionMirror.signal_copy with single_writer

resonate FusionAuthority.signal dampen 0 ms:
    FusionAuthority.last_old = resonate_old_i64
    FusionAuthority.last_new = resonate_new_i64
    let pipeline_result = fusion_signal_pipeline(
        resonate_new_i64 + FusionAuthority.tick,
        FusionAuthority.tick
    )
    FusionAuthority.shadow = pipeline_result
```

**What happens in one write:**

1. A `patch` writes `FusionAuthority.signal = value` → triggers entangle propagation
1. Entangle propagates `signal` to `FusionMirror.signal_copy` automatically
1. `resonate` fires on the write (after the store) → runs `fusion_signal_pipeline` (an orchestrate)
1. The orchestrate result is written to `FusionAuthority.shadow` (a different field)
1. Downstream actors read `shadow` instead of `signal` — they see the **processed** value

This is cascaded semantic processing — state change → propagate → pipeline → new state → consumption. All three constructs (`entangle`, `resonate`, `orchestrate`) work together without any ad hoc wiring.

### Pattern 2: Entangle + Patch (Journaled Propagation)

Every `patch` that writes to a world field automatically triggers entangle propagation for any entangled fields. The epoch counter pattern ensures propagation is observable:

```kn
patch commit_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value               // triggers entangle → Mirror.signal_copy
    authority.epoch = authority.epoch + 1   // triggers entangle → Mirror.epoch_copy
    return authority.signal
```

### Pattern 3: Entangle + Actor (Read Mirrors, Write Authorities)

Actors can read from mirror worlds (read-only) and write to authority worlds (mutable):

```kn
actor TelemetryConsumer:
    on Consume(reply_to: P):
        // Read from mirror (safe — entangle guarantees freshness)
        let latest = RatTelemetryMirror.best_distance_copy
        // Write to authority (the only allowed write path)
        RatTelemetry.frame = RatTelemetry.frame + 1
        send reply_to.Reply(value = latest)
```

This is the canonical way to use entangle with actors: mirrors are observation points, authorities are mutation targets.

### Pattern 4: Entangle + Component Surfaces (World→UI)

Worlds with `surface native_ui => Component` automatically expose entangled state to UI components:

```kn
component SignalDisplay():
    render <panel title="Signal">
        <text value={Authority.signal} />        // reads authority directly
        <text value={Mirror.signal_copy} />      // reads mirror (entangled)
    </panel>

world Authority:
    state signal: Int = 1
    surface native_ui => SignalDisplay

world Mirror:
    state signal_copy: Int = 1
    surface web => SignalDisplay

entangle Authority.signal <-> Mirror.signal_copy with single_writer
```

### Pattern 5: Entangle + Teleport (Cross-World Handoff Mirrored)

When a `teleport` moves data cross-world, entangle can mirror the result to a third world:

```kn
world Source: state data: Int = 0
world Dest:   state data_copy: Int = 0
world Monitor: state data_shadow: Int = 0

entangle Dest.data_copy <-> Monitor.data_shadow with single_writer

fn transfer(value: Int) -> Int:
    teleport value from Source to Dest via direct_bus
    // Dest.data_copy was just set — entangle propagates to Monitor.data_shadow
    // Monitor can observe the shadow without any teleport awareness
    return 0
```

### Pattern 6: The Telemetry Delta Guard

Always guard semantic tests with telemetry deltas. The fusion chain benchmark establishes this as mandatory for entangle:

```kn
fn verify_entangle_fired() -> Int:
    let before = entangle_propagation_count()
    // ... operation that should trigger entangle ...
    let delta = entangle_propagation_count() - before
    if delta < 1:
        return -11        // specific error code: entangle never propagated
    return 0
```

Error codes from the fusion chain:

- `-10` = resonate never fired
- `-11` = entangle never propagated
- `-12` = teleport never happened
- `-13` = patch journal overflow
- `-14` = orchestrate never ran

______________________________________________________________________

## Under-Utilized Ideas

### 1. Entangle as Experiment Telemetry Decoupling

The converge rat experiment uses entangle to decouple experiment state (authority with raw buffers) from visualization state (mirror with web surface). The authority owns `ptr<Int>` raw maze buffers; the mirror never touches those, only the scalar counters. This is under-utilized because most current examples entangle all fields 1:1.

**Under-explored:** Entangling only a subset of world fields — specifically the "safe for display" ones — while keeping raw buffers isolated in the authority:

```kn
world Experiment:
    state maze_buffer: ptr<Int> = int_to_ptr(0, "Int")   // NOT entangled
    state best_distance: Int = 0                            // entangled
    state frame: Int = 0                                    // entangled
    surface native_ui => Visualizer

world DisplayMirror:
    state best_distance_copy: Int = 0   // only safe-to-display fields
    state frame_copy: Int = 0
    surface web => Visualizer

// Only entangle what's safe to expose
entangle Experiment.best_distance <-> DisplayMirror.best_distance_copy with single_writer
entangle Experiment.frame <-> DisplayMirror.frame_copy with single_writer
```

### 2. Entangle as Multi-Stage Pipeline Coupling

The 24-tet effects engine entangles 5 fields between `FxWorld` and `FxWorldMirror`. Each entangled field represents a different stage in the audio pipeline. An orchestrate stage could read from the mirror (safe snapshot) and write to the authority (mutation target):

```kn
orchestrate fx_pipeline():
    stage read_state: converge fx_read_mirror_state(FxWorldMirror) ...   // read mirror
    stage compute: cpu fx_compute(read_state) ...                         // compute
    stage write: patch fx_write_result(FxWorld, compute) ...             // write authority
    // After write, entangle auto-propagates to FxWorldMirror for next frame
```

**Under-explored:** Using entangle as the **state handoff mechanism between orchestrate stages** — the mirror is a "safe snapshot" of the previous frame's state, and the authority is the "mutation target" for the current frame.

### 3. Entangle + Law: Invariant-Protected Coupling

Current entangle has no law gates. An authority writes, entangle propagates, the mirror receives — no validation. But a law could pre-validate the value before propagation (conceptually):

```kn
law signal_in_bounds(value: Int) -> Bool:
    return value >= 0 and value < 1000000007

patch commit_signal(authority: Authority, value: Int) -> Int:
    if law_status(signal_in_bounds(value)) == false:
        return -1   // law violation — don't write
    authority.signal = value    // entangle propagation follows
    return 0
```

**Under-explored:** Making the law check an explicit guard before the entangled write, ensuring the mirror never receives an invalid value.

### 4. Asymmetric Entangle (Multi-Mirror Fan-Out)

Currently, one authority can have multiple mirrors. But this is **not explicitly explored** in any benchmark. You could have:

```kn
world Authority:
    state signal: Int = 0

world MirrorWeb:
    state signal_copy: Int = 0
    surface web => Panel

world MirrorNative:
    state signal_copy: Int = 0
    surface native_ui => Panel

// One authority, two mirrors — automatic fan-out
entangle Authority.signal <-> MirrorWeb.signal_copy with single_writer
entangle Authority.signal <-> MirrorNative.signal_copy with single_writer
```

**Under-explored:** Multi-mirror fan-out where each mirror has a different surface kind. The authority writes once; both mirrors receive automatically.

### 5. Entangle as Capability Gate Visibility

The `state.entangle` capability is emitted when any entangle exists in the program. This means **the program's capability manifest reveals its coupling topology** to orchestrate stages, runtime contract validation, and deployment tooling.

**Under-explored:** Orchestrate stages that require `state.entangle` as a precondition:

```kn
orchestrate verified_pipeline(value: Int) -> Int:
    stage check: law state_has_entangle(entangle_registered_count() > 0)
    stage process: cpu compute(value) when capability("state.entangle") requires check
    return process
```

______________________________________________________________________

## Combining with Other Keywords

### Entangle + World

The foundation. Every entangle connects two world state fields. Without worlds, there is no entangle.

```kn
world A: state x: Int = 0
world B: state x_copy: Int = 0
entangle A.x <-> B.x_copy with single_writer
```

### Entangle + Patch

Patches trigger entangle propagation. Every `patch` that assigns to a world field will automatically propagate to any entangled mirrors.

```kn
patch set_x(world: A, value: Int) -> Int:
    world.x = value          // entangle propagates to B.x_copy
    return world.x
```

### Entangle + Resonate

Entangle propagation can trigger resonate handlers on the mirror world (if the mirror field has a resonate). Combined with the authority-side `resonate`, you get cascading reactivity.

```kn
resonate Authority.signal dampen 16ms:
    // Fires on authority-side write (before propagation)
    Authority.epoch = Authority.epoch + 1

resonate Mirror.signal_copy dampen 16ms:
    // Fires after entangle propagates to mirror
    Mirror.notified = 1
```

### Entangle + Orchestrate

Orchestrate stages can read from mirrors (safe snapshots) and write to authorities (mutation targets). Entangle handles the coupling between the end of one frame/pipeline stage and the beginning of the next.

### Entangle + Converge

Converge lanes can select different authority/mirror paths based on capability:

```kn
converge state_path() -> (String, String):
    spec reference:
        return ("Authority.signal", "Mirror.signal_copy")
    fast avx2_lane when capability("cpu.x86.avx2"):
        return ("Authority.signal", "Mirror.signal_copy")
```

### Entangle + Teleport

Teleport moves ownership; entangle copies state. They complement each other: teleport for destructive cross-world handoff, entangle for non-destructive state mirroring.

### Entangle + Actor

Actors can read mirror state and write authority state. This is the canonical concurrent access pattern: reads are safe (mirror), writes are controlled (authority → entangle → mirror).

### Entangle + Component / UI Surface

Entangled mirror worlds can have different surface kinds than authorities. Authority has `native_ui`, mirror has `web`. The same state is synchronized for both platforms.

______________________________________________________________________

## Validation Ladder

```powershell
# 1. Typecheck
kain check <file.kn> --target llvm

# 2. Compile + run
kain run <file.kn> --target llvm

# 3. Verify entangle telemetry
kain run <file.kn> --target llvm \
  # Add a test that checks entangle_propagation_count() > 0

# 4. For projects with worlds
kain check <project-dir> --target llvm
kain run <project-dir> --target llvm

# 5. For benchmark proof
python benchmark/run.py --case fusion_chain --languages kain --runs 1 --warmups 0
# fusion_chain has the canonical telemetry delta guard (error code -11 for entangle failure)

# 6. Check runtime contract
# The program's runtime_contract.json will list entanglements section
```

### Smoketest

```kn
// smoketest/src/semantics/entangle.kn
use std::runtime
use std::intent

pub fn smoke_entangle_lane() -> Int:
    let propagation_count = entangle_propagation_count()
    if propagation_count < 0:
        return 1
    return 0
```

______________________________________________________________________

## Z3 Proofs

Two Z3 proofs exist for the native entangle registry in `runtime/native/src/core/z3/proofs/`:

### 1. Native Entangle Copy Text Fits Destination

**File:** `native-entangle-copy-text-fits-destination-before-null-copy.yaml`

**Proves:** When `runtime_copy_entangle_text()` copies authority/mirror/policy/type_name strings into the binding struct, the source string length is always less than the destination capacity (`ENTANGLE_MAX_PATH` = 256, `ENTANGLE_MAX_POLICY` = 64, `ENTANGLE_MAX_TYPE` = 128).

This prevents buffer overflows in the C registry.

### 2. Native Entangle Register Count Stays Within Capacity

**File:** `native-entangle-register-count-stays-within-capacity.yaml`

**Proves:** The number of registered entangle bindings never exceeds `ENTANGLE_MAX_BINDINGS` = 128.

This is checked at runtime (`if (g_kain_entangle_binding_count >= ENTANGLE_MAX_BINDINGS) { return -3; }`) and the Z3 proof verifies the invariant holds for the maximum declarable program size.

______________________________________________________________________

## Source Anchors

### Core Crate (`crates/entangle/`)

| File | Content |
|------|---------|
| `crates/entangle/src/lib.rs` | `EntanglePolicy`, `EntangleEndpointId`, `EntangleBindingDescriptor`, `EntangleGraph`, `EntangleGraphError` |

### Parser & AST (`crates/core/`)

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/ast.rs` | 434-464 | `EntangleDef`, `EntangleEndpoint`, `EntanglePolicy` structs |
| `crates/core/src/parser.rs` | 2206-2260 | `parse_entangle()`, `parse_entangle_endpoint()` |
| `crates/core/src/types.rs` | 6000-6060 | `check_entangle()`, `resolve_entangle_endpoint_type()` |
| `crates/core/src/runtime_contract.rs` | 233-240, 1515-1543 | `RuntimeEntangleContract`, `collect_entangle_contracts()` |
| `crates/core/src/runtime.rs` | 4000-4010, 4506-4530 | `register_entangle_value()`, `propagate_entangled_write()` |
| `crates/core/src/realtime_app_bundle.rs` | 1005-1010 | `realtime_entangle_binding()` |

### LLVM Codegen

| File | Lines | Content |
|------|-------|---------|
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 90, 615, 768 | `NativeEntangleBinding` struct, collection |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 10909-10920 | `native_entangle_authority_binding()`, `native_entangle_mirror_binding()` |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 11288-11320 | `emit_entangle_i64_propagation()` |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 13203-13212 | `collect_native_entanglements()` |

### Native C Runtime

| File | Content |
|------|---------|
| `runtime/native/include/entangle.h` | `KainRuntimeEntangleBinding`, registry API declarations |
| `runtime/native/src/core/entangle.c` | Registry implementation (reset, register, get, count) |
| `runtime/native/src/core/stdlib_abi.c` | `abi_entangle_record_i64()`, `abi_entangle_propagation_count()`, telemetry |

### Stdlib

| File | Lines | Content |
|------|-------|---------|
| `stdlib/intent.kn` | 50-56, 103-124, 154-161, 251-272, 302-309 | Full entangle telemetry API |

### Z3 Proofs

| File | Content |
|------|---------|
| `runtime/native/src/core/z3/proofs/native-entangle-copy-text-fits-destination-before-null-copy.yaml` | Bounds proof for string copy |
| `runtime/native/src/core/z3/proofs/native-entangle-register-count-stays-within-capacity.yaml` | Capacity proof for registry |

### Examples

| File | Usage |
|------|-------|
| `smoketest/src/semantics/entangle.kn` | Smoke test: checks propagation count |
| `library_of_kain/entangle.kn` | Minimal smoke: same pattern |
| `benchmark/cases_v2/fusion_chain.kn` | Apex: entangle + patch + resonate + orchestrate + actor + teleport |
| `benchmark/cases/semantic_singularity/main.kn` | Multiple entangled worlds in semantic stress test |
| `benchmark/cases/semantic_singularity_no_entangle/main.kn` | Control: same stress without entangle |
| `blades/experiments/cortical_sieve/src/neural_entangled_sieve.kn` | Entangle in neural lattice experiment |
| `blades/python/24_tet/src/resonate_py_effects.kn` | 5-field entangle in audio effects engine |
| `blades/experiments/convergence/src/*.kn` | Rat telemetry entangle (experiment/visualization decoupling) |
| `library_of_kain/semantics.kn` | Compact full semantic stack with entangle |

______________________________________________________________________

## Anti-Patterns

### ❌ Manual Sync Instead of Entangle

```kn
// WRONG: manual sync code
fn sync_signals():
    Mirror.signal_copy = Authority.signal
    Mirror.epoch_copy = Authority.epoch
```

```kn
// RIGHT: compiler-owned coupling
entangle Authority.signal <-> Mirror.signal_copy with single_writer
entangle Authority.epoch <-> Mirror.epoch_copy with single_writer
```

### ❌ Using Plain Structs Instead of Worlds

```kn
// WRONG: plain structs + sync code
struct Authority:
    signal: Int = 1
struct Mirror:
    signal_copy: Int = 1
```

```kn
// RIGHT: worlds with entangle
world Authority:
    state signal: Int = 1
    surface native_ui => Panel
world Mirror:
    state signal_copy: Int = 1
    surface web => Panel
entangle Authority.signal <-> Mirror.signal_copy with single_writer
```

### ❌ Copying Instead of Entangling

```kn
// WRONG: manual assign after every write
fn set_signal(value: Int):
    Authority.signal = value
    Mirror.signal_copy = value
```

### ❌ Reading Authority in a Read-Only Context

If you only need display/observation, read the mirror. The mirror is guaranteed fresh by entangle:

```kn
// GOOD: read mirror for display
fn display_signal() -> Int:
    return Mirror.signal_copy

// BAD: bypassing entangle to read authority directly
fn display_signal() -> Int:
    return Authority.signal   // works but defeats the coupling intent
```

### ❌ Entangling Raw Pointers Without Ownership

Raw `ptr<Int>` buffers should NOT be entangled between worlds. Entangle is for value types (`Int`, `Bool`, `String`, structs). Raw pointers should use `teleport` for ownership transfer.

### ❌ Entangling Too Many Fields

Every entangled field incurs LLVM propagation code. If you're entangling 50 fields in a hot loop, the overhead adds up. Only entangle what needs to be mirrored.

### ❌ Not Checking Telemetry

Without `entangle_propagation_count()` deltas, you have no proof the coupling actually fired. Always guard with telemetry in benchmarks and tests.

______________________________________________________________________

## Quick Reference

| Aspect | Detail |
|--------|--------|
| **Syntax** | `entangle A.field <-> B.field with single_writer` |
| **Minimum endpoints** | 2 segments per endpoint (`World.field`) |
| **Policy** | Currently only `single_writer` |
| **Type constraint** | Both endpoints must have the same type (after shared-ref peeling) |
| **Duplicate protection** | Each endpoint can participate in at most one entangle |
| **Max bindings** | 128 (Z3-proven C constant) |
| **LLVM emission** | `emit_entangle_i64_propagation()` — GEP + store + telemetry call |
| **Telemetry** | `entangle_propagation_count()`, `entangle_last_authority()`, `entangle_last_mirror()` |
| **Capability** | `state.entangle` — auto-emitted when any entangle exists |
| **Intepreter** | `propagate_entangled_write()` after every authority field assignment |
| **Runtime contract** | `RuntimeEntangleContract { authority, mirror, policy, type_name }` |
| **Z3 proofs** | String copy fits destination; register count within capacity |
| **Canonical example** | `benchmark/cases_v2/fusion_chain.kn` — full causal chain with telemetry deltas |
| **Canonical pattern** | Dual-world authority+mirror with `surface native_ui` / `surface web` |
| **Stdlib module** | `std::intent` — all telemetry functions |
| **Source** | `crates/entangle/src/lib.rs`, `crates/core/src/ast.rs`, `crates/core/src/types.rs` |
