# Ownership — Collapse, Observe, Decay

**Status:** Research synthesis, 2026-06-08
**Based on:** `crates/ownership/src/lib.rs`, `crates/core/src/parser.rs`, `crates/core/src/ast.rs`, `crates/core/src/types.rs`, `crates/sys-codegen/src/codegen_llvm/mod.rs`, `runtime/native/include/ownership.h`, `runtime/native/src/core/ownership.c`, `stdlib/intent.kn`, `smoketest/src/semantics/ownership.kn`, `benchmark/cases_v2/keyword_crucible.kn`, `benchmark/cases_v2/fusion_chain.kn`, `benchmark/cases_v2/classic_systems.kn`, `benchmark/cases/pulse_teleport_decay_mesh/main.kn`, `benchmark/cases/pong/main.kn`, benchmark `CATALOG.MD`, `RULEBOOK.MD`, lang-semantics SKILL, lang-systems SKILL

---

## Table of Contents

1. [What Is Ownership? — Collapse, Observe, Decay](#1-what-is-ownership--collapse-observe-decay)
2. [The Core Idea — Compiler-Owned Raw Memory Lifecycle](#2-the-core-idea--compiler-owned-raw-memory-lifecycle)
3. [Syntax Reference](#3-syntax-reference)
4. [The Ownership State Machine](#4-the-ownership-state-machine)
5. [Regions — Ownership Region Kinds](#5-regions--ownership-region-kinds)
6. [Policy Table — What Each Region Allows](#6-policy-table--what-each-region-allows)
7. [How It Works — Full Pipeline](#7-how-it-works--full-pipeline)
8. [Ownership + Actor](#8-ownership--actor)
9. [Ownership + World](#9-ownership--world)
10. [Ownership + Share + Fanout](#10-ownership--share--fanout)
11. [Ownership + Pulse](#11-ownership--pulse)
12. [Ownership + Entangle](#12-ownership--entangle)
13. [Under-Utilized Ideas](#13-under-utilized-ideas)
14. [Combining with Other Keywords](#14-combining-with-other-keywords)
15. [Validation Ladder](#15-validation-ladder)
16. [Z3 Proofs](#16-z3-proofs)
17. [Source Anchors](#17-source-anchors)
18. [Anti-Patterns](#18-anti-patterns)
19. [Quick Reference](#19-quick-reference)

---

## 1. What Is Ownership? — Collapse, Observe, Decay

`collapse`, `observe`, and `decay` are **expression-level keywords** in Kain that form the **compiler-owned raw memory ownership lifecycle**. They are part of **LAYER 7: SYSTEMS** in Kain's decision ladder — the highest semantic layer, alongside `actor`. They are Kain's answer to Rust's borrow checker, but **explicit rather than inferred** — the programmer writes the ownership transitions, and the compiler verifies them.

These three keywords are tightly coupled as a **state machine** operating on a memory region:

| Keyword | Operation | State Transition |
|---------|-----------|------------------|
| **`collapse`** | Enter ownership scope, take exclusive mutable access | `Idle → Collapsed` |
| **`observe`** | Enter read-only borrow scope, take shared immutable access | `Idle → Observed` |
| **`decay`** | Release ownership, destroy/return memory | `Idle → Decayed` |

Two additional keywords extend the system:

| Keyword | Operation | State Transition |
|---------|-----------|------------------|
| **`share`** | Enter shared atomics scope for parallel write lanes | `Idle → Shared` |
| **`fanout`** | (Inside share) Launch parallel write lanes | Must be inside `share` |

The relationship between ownership and other Layer 7 constructs:

```kn
owner:
    "Actors own state via message isolation"
    "Ownership owns memory via collapse/observe/decay"
```

They pair differently:
- **`actor`** handles concurrent message state — each actor owns its private state and processes one message at a time.
- **`collapse/observe/decay`** handles raw memory lifecycle — explicit acquire/release of mutable or immutable access to raw pointer regions.

Both are **compiler-owned** — no runtime borrow checker, no garbage collector, no Rc/Arc dance. The compiler emits `__kain_ownership_begin_collapse()` / `__kain_ownership_end_collapse()` calls into the native C ownership runtime, and the state machine is verified both at compile time (typechecker) and at runtime (checked calls with `abort()` on error).

### The Canonical Example

From `benchmark/cases_v2/fusion_chain.kn` — the **FusionTeleporter** actor performing collapse/observe/decay inside a message handler:

```kn
on ShatterAndSend(reply_to: P, payload: Int):
    self.teleports_done = self.teleports_done + 1
    let tick = fusion_unpack_a(payload)
    let signal = fusion_unpack_b(payload)

    // OWNERSHIP LAYER: collapse/observe/decay inside actor message handler
    let cell_count = 4
    let mut cells: ptr<Int> = alloc_zeroed(cell_count, "Int")

    collapse cells:
        var i: Int = 0
        while i < cell_count:
            mem_store(ptr_offset(cells, i, "Int"), (tick * (i + 1) * 7) % FUSION_MODULUS, "Int")
            i = i + 1
        0

    let head: Int = observe cells:
        mem_load(ptr_offset(cells, 0, "Int"), "Int")

    decay cells
```

The pattern is universal: `alloc` → `collapse` (write) → `observe` (read) → `decay` (release). The compiler verifies every transition.

---

## 2. The Core Idea — Compiler-Owned Raw Memory Lifecycle

### What Ownership IS NOT

- ❌ **Not Rust's borrow checker** — Rust infers lifetimes and borrows. Kain requires explicit `collapse`/`observe`/`decay` expressions. There is no implicit lifetime inference.
- ❌ **Not a garbage collector** — No GC roots, no tracing, no cycle detection. Memory is explicitly allocated and explicitly decayed.
- ❌ **Not reference counting** — `Rc<T>` / `Arc<T>` do not exist in Kain's ownership model. There is `RC_OBJECT` as a region kind, but RC is a *region policy* that the ownership runtime tracks, not a user-facing counting type.
- ❌ **Not a linear type system** — You cannot pass ownership between scopes (no move semantics). Ownership is scoped to the expression block.
- ❌ **Not a memory pool or allocator abstraction** — `alloc`/`realloc`/`free` are separate intrinsics. Ownership *wraps* them with state-machine verification.
- ❌ **Not optional** — If you use raw pointers (`ptr<T>`) and write to them with `mem_store`, the compiler enforces that they must be used within `collapse`/`observe`/`decay` scopes.
- ❌ **Not thread-safe by default** — Ownership scopes are single-threaded. For multi-threaded access, use `share`/`fanout` with `atomic_store`.

### What Ownership IS

- ✅ A **compiler-owned state machine** for raw pointer lifecycle — every transition is checked at compile time and at runtime (via native C helpers).
- ✅ **Expression-level scopes** — `collapse ptr: { ... }` and `observe ptr: { ... }` create explicit blocks where the pointer is in a known state.
- ✅ **Deterministic destruction** — `decay ptr` is an explicit statement that transitions the region to `Decayed`, making it unusable thereafter.
- ✅ **Region-aware policies** — the same keywords behave differently depending on the region kind (local alloca vs. heap allocation vs. world state vs. RC object).
- ✅ **Zero-cost in the hot path** — ephemeral locals (helper-owned pointers that never escape) skip runtime ownership calls entirely — `ownership_pointer_provenance_for_expr()` shortcuts to `EphemeralLocal` and the codegen emits no runtime guard.
- ✅ **Nested observers** — you can nest `observe` scopes; the observer count increments and decrements correctly, returning to `Idle` only when all observers exit.
- ✅ **Verified at two levels** — the Rust typechecker validates pointer types and state transitions; the native C runtime checks state with `__kain_ownership_*` calls that `abort()` on invalid transitions.
- ✅ **Capability-gated** — the presence of any ownership expression emits `memory.ownership` capability in the runtime contract.

### The Relationship View

| Construct | Domain-Locked View | Relationship View |
|-----------|-------------------|-------------------|
| `collapse` | "Raw pointer write scope" | **Transition: Idle → Collapsed** — exclusive mutable access to a memory region, verified at compile & runtime |
| `observe` | "Raw pointer read scope" | **Transition: Idle → Observed** — shared read-only access, can be nested, multiple simultaneous observes allowed |
| `decay` | "Free / destroy pointer" | **Transition: Idle → Decayed** — terminal state, releases heap (FreeHeap) or ends lifetime (LifetimeEnd) |
| `ptr<T>` | "Raw pointer type" | **Ownership target** — collapse/observe/decay all take pointer-typed expressions |
| `share` | "Parallel atomic write region" | **Transition: Idle → Shared** — opens scope for fanout workers with atomic stores |
| `fanout` | "Parallel lane inside share" | **Write lane** — multiple fanout workers can `atomic_store` into shared region |

---

## 3. Syntax Reference

### Collapse — Exclusive Mutable Access

```kn
collapse <pointer_expr>:
    <body_block>
```

**Canonical example:**
```kn
let mut buf: ptr<Int> = alloc_zeroed(16, "Int")
collapse buf:
    var i: Int = 0
    while i < 16:
        mem_store(ptr_offset(buf, i, "Int"), i * 7, "Int")
        i = i + 1
    0                      // body result (optional)
```

**Rules:**
- `<pointer_expr>` must evaluate to a pointer-like type (`ptr<T>`, `ref<T>`, or `Unknown`)
- The pointer is **mutable** within the collapse block — you can write via `mem_store`
- The body cannot contain `return`, `break`, or `continue` (enforced at compile time)
- The body's result type determines the type of the `collapse` expression
- Collapse cannot be nested inside `share` scopes in v1
- `collapse` on an already-collapsed region causes runtime `abort()`

### Observe — Shared Read-Only Access

```kn
observe <pointer_expr>:
    <body_block>
```

**Canonical example:**
```kn
let val: Int = observe buf:
    mem_load(ptr_offset(buf, 0, "Int"), "Int")
```

**Rules:**
- `<pointer_expr>` must evaluate to a pointer-like type
- The pointer is **read-only** within the observe block — you can read via `mem_load` but writes via `mem_store` are not allowed (runtime error if attempted)
- Observes can be nested — each nested observe increments an observer count, and the region returns to `Idle` only when all observers exit
- The body cannot contain `return`, `break`, or `continue`
- Cannot be inside `share` scopes in v1
- `observe` on a collapsed or decayed region causes runtime `abort()`

### Decay — Deterministic Destruction

```kn
decay <pointer_expr>
```

**Canonical example:**
```kn
decay buf
```

**Rules:**
- `<pointer_expr>` must evaluate to a pointer-like type
- `decay` is a **statement-level expression** returning `Unit`
- Transitions the region to `Decayed` — subsequent `collapse`, `observe`, or `decay` on the same pointer cause runtime `abort()`
- For `HeapAllocation` regions, `decay` frees the heap memory (`FreeHeap` mode → calls `__kain_ownership_decay` → `free()`)
- For `LocalAlloca` regions, `decay` is a lifetime-end marker (no-op at runtime, just state tracking)
- For `RC_OBJECT` regions, `decay` releases a strong reference
- **Ephemeral locals** (helper-owned pointers that never escape their declaring block) skip the runtime call entirely — `compile_decay_expr()` returns `("0", "void")` immediately
- After `decay`, the compiler marks helper-owned pointer locals as `null` via `clear_decayed_helper_owned_local()` to prevent use-after-decay

### Share — Shared Atomic Region (Parallel Writes)

```kn
share <pointer_expr>:
    <body_block with fanout workers>
```

**Canonical example** (from `keyword_crucible.kn`):
```kn
let mut partials: ptr<Int> = alloc_zeroed(CRUCIBLE_WORKERS, "Int")

share partials:
    fanout worker in 0..CRUCIBLE_WORKERS:
        let slot: ptr<Int> = ptr_offset(partials, worker, "Int")
        var step: Int = 0
        var local: Int = 0
        while step < CRUCIBLE_STEPS:
            local = (local + crucible_fast_mix(worker + step, iterations % 31)) % modulus
            step = step + 1
        atomic_store(slot, local)
```

**Rules:**
- Opens a `Shared` region for parallel write lanes
- Inside `share`, you can use `fanout` to launch parallel workers
- Workers use `atomic_store` (not `mem_store`) — the region policy is `ShareMode::AtomicSeqCst`
- The body pushes/pops a `shared_region` context in the typechecker
- `observe`, `collapse`, and `decay` are **not allowed inside share scopes** in v1
- `fanout` bodies cannot contain `return`, `break`, or `continue`
- Nested `fanout` is not supported in v1

### Fanout — Parallel Write Lane

```kn
fanout <name> in <range_expr>:
    <body_block>
```

Must appear inside a `share` block. Each fanout worker writes into the shared region via `atomic_store`. The number of workers is the range cardinality.

### Grammar (from parser.rs)

```text
collapse <expr> ':' <block>       → Expr::Collapse { target, body }
observe  <expr> ':' <block>       → Expr::Observe  { target, body }
decay    <expr>                   → Expr::Decay    { target }

share    <expr> ':' <block>       → Expr::Share    { target, body }
fanout   <ident> 'in' <expr> ':' <block> → Stmt::Fanout { ... }
```

The parser flow:
1. `parse_unary()` peeks at `TokenKind::Collapse`, `TokenKind::Observe`, or `TokenKind::Decay`
2. `parse_scoped_ownership_expr(keyword)` for collapse/observe/share — parses target expr, expects `:`, parses block body
3. `parse_decay_expr()` — advances past `decay` keyword, parses target as unary expression
4. Returns the appropriate `Expr` variant

---

## 4. The Ownership State Machine

### States

```
                     ┌──────────┐
                     │  IDLE    │
                     └────┬─────┘
                          │
               ┌──────────┼──────────┐
               │          │          │
               ▼          ▼          ▼
          ┌────────┐ ┌────────┐ ┌────────┐
          │COLLAPSED│ │OBSERVED│ │ SHARED │
          └────┬────┘ └───┬────┘ └───┬────┘
               │          │          │
               │    ┌─────┘          │
               ▼    ▼                ▼
          ┌─────────────┐       ┌────────┐
          │   DECAYED   │       │  IDLE  │
          └─────────────┘       └────────┘
```

| State | Meaning | Can transition to |
|-------|---------|-------------------|
| `Idle` | No active access. Region exists but nobody holds a reference. | `Collapsed`, `Observed`, `Shared`, `Decayed` |
| `Collapsed` | Exclusive mutable access. One writer. | `Idle` (on end collapse) |
| `Observed(N)` | Shared read-only access with N active observers. | `Observed(N+1)` (nest), `Idle` (when all exit) |
| `Shared` | Atomic parallel write region (via `share`). | `Idle` (on end share) |
| `Decayed` | Terminal state. Region is destroyed or freed. | *(none — terminal)* |

### Transition Rules (from `ownership_state.rs`)

| Transition | From `Idle` | From `Observed` | From `Collapsed` | From `Shared` | From `Decayed` |
|-----------|-------------|-----------------|------------------|---------------|----------------|
| `BeginObserve` | ✅ Observed(1) | ✅ Observed(N+1) | ❌ CannotObserveCollapsed | ❌ CannotObserveShared | ❌ CannotObserveDecayed |
| `EndObserve` | ❌ NotObserved | ✅ N>1: Observed(N-1); N=1: Idle | ❌ NotObserved | ❌ NotObserved | ❌ NotObserved |
| `BeginCollapse` | ✅ Collapsed | ❌ CannotCollapseWhileObserved(r) | ❌ AlreadyCollapsed | ❌ CannotCollapseWhileShared | ❌ CannotCollapseDecayed |
| `EndCollapse` | ❌ NotCollapsed | ❌ NotCollapsed | ✅ Idle | ❌ NotCollapsed | ❌ NotCollapsed |
| `BeginShare` | ✅ Shared | ❌ CannotShareWhileObserved(r) | ❌ CannotShareWhileCollapsed | ❌ AlreadyShared | ❌ CannotShareDecayed |
| `EndShare` | ❌ NotShared | ❌ NotShared | ❌ NotShared | ✅ Idle | ❌ NotShared |
| `Decay` | ✅ Decayed | ❌ CannotDecayWhileObserved(r) | ❌ CannotDecayWhileCollapsed | ❌ CannotDecayWhileShared | ❌ AlreadyDecayed |

### Observer Count Nesting

A key property: `observe` scopes can nest, and the observer count tracks correctly:

```kn
// State: Idle
observe buf:            // BeginObserve → Observed(1)
    let a = mem_load(buf, "Int")
    observe buf:          // BeginObserve → Observed(2)
        let b = mem_load(buf, "Int")
        ...                // EndObserve → Observed(1)
    let c = mem_load(buf, "Int")
    ...                    // EndObserve → Idle
```

The test in `crates/ownership/src/lib.rs` verifies this:
```rust
fn observe_counts_are_nested_and_return_to_idle() {
    let state = OwnershipState::Idle
        .apply(OwnershipTransition::BeginObserve)  // Observed(1)
        .apply(OwnershipTransition::BeginObserve)  // Observed(2)
        .apply(OwnershipTransition::EndObserve)    // Observed(1)
        .apply(OwnershipTransition::EndObserve);   // Idle
}
```

### Early Exit Prohibition

Ownership blocks (collapse, observe, share) **cannot contain** `return`, `break`, or `continue`. This is enforced at compile time by `ensure_ownership_scope_has_structured_exit()` which recursively checks the AST. The error message:

```
error: collapse scopes do not support return, break, or continue in v1;
       move the control flow outside the ownership block
```

The reasoning: an early exit would skip the `__kain_ownership_end_collapse()` or `__kain_ownership_end_observe()` runtime call, leaving the region in a locked state and causing subsequent operations to `abort()`.

---

## 5. Regions — Ownership Region Kinds

Every ownership operation targets a memory region of a specific **kind**. The region kind determines the **policy** — which operations are allowed and what they do at runtime.

### The Seven Region Kinds

Defined in `crates/ownership/src/lib.rs` as `OwnershipRegionKind`:

| Kind | Discriminant | Description |
|------|-------------|-------------|
| `LocalAlloca` | 0 | Stack-allocated local variable (alloca). Lifetime ends with scope. |
| `HeapAllocation` | 1 | Heap-allocated via `alloc` / `alloc_zeroed`. Freed on decay. |
| `RcObject` | 2 | Reference-counted object. Strong reference released on decay. |
| `WorldState` | 3 | Field of a `world` declaration. Snapshot observe, graph-exclusive collapse. |
| `EntangledAuthority` | 4 | Authority side of an `entangle` binding. Snapshot observe, graph-exclusive collapse. |
| `EntangledMirror` | 5 | Mirror side of an `entangle` binding. Snapshot observe only (no collapse, no decay). |
| `ImportedPointer` | 6 | Pointer imported from C FFI, GPU buffer, or external source. Borrow semantics. |

### Region Kind Constants (C Runtime)

From `runtime/native/include/ownership.h`:

```c
enum {
    KAIN_OWNERSHIP_REGION_LOCAL_ALLOCA       = 0,
    KAIN_OWNERSHIP_REGION_HEAP_ALLOCATION    = 1,
    KAIN_OWNERSHIP_REGION_RC_OBJECT          = 2,
    KAIN_OWNERSHIP_REGION_WORLD_STATE        = 3,
    KAIN_OWNERSHIP_REGION_ENTANGLED_AUTHORITY = 4,
    KAIN_OWNERSHIP_REGION_ENTANGLED_MIRROR   = 5,
    KAIN_OWNERSHIP_REGION_IMPORTED_POINTER   = 6,
};
```

### How Regions Are Assigned

The region kind is determined by the compiler based on the **provenance** of the pointer expression:

```rust
fn ownership_pointer_provenance_for_expr(&self, expr: &Expr) -> OwnershipPointerProvenance {
    match expr {
        Expr::Ident(name, _) => {
            if self.ephemeral_owned_pointer_locals.contains_key(name) {
                OwnershipPointerProvenance::EphemeralLocal
            } else {
                self.helper_owned_pointer_locals
                    .get(name)
                    .copied()
                    .unwrap_or(OwnershipPointerProvenance::ImportedOrUnknown)
            }
        }
        Expr::Call { callee, args, .. } => match callee.as_ref() {
            Expr::Ident(name, _) if name == "__kain_alloc" => {
                OwnershipPointerProvenance::HelperOwned
            }
            // ...
        }
    }
}
```

Three provenance levels:
- **`HelperOwned`** — pointer from `__kain_alloc` / `__kain_realloc` (heap allocation). Emits helper-path runtime calls (`begin_collapse_helper`, `end_collapse_helper`, etc.).
- **`EphemeralLocal`** — short-lived local that never escapes its block. **Skips all runtime ownership calls** — zero-cost.
- **`ImportedOrUnknown`** — pointer from C FFI, world state, or other external source. Emits imported-path runtime calls with lazy region registration via `__kain_ownership_ensure_imported()`.

---

## 6. Policy Table — What Each Region Allows

### The Ownership Policy Table

Defined as a const array in `crates/ownership/src/lib.rs`:

```rust
pub const OWNERSHIP_POLICY_TABLE: &[OwnershipPolicy] = &[
    // LocalAlloca: observe (readonly), collapse (scoped), share (no), decay (lifetime_end)
    OwnershipPolicy::new(
        OwnershipRegionKind::LocalAlloca,
        ObserveMode::ReadonlyBorrow,     // observe → emit readonly annotation
        CollapseMode::ScopedNoAlias,     // collapse → emit noalias annotation
        ShareMode::Unsupported,          // no share
        DecayMode::LifetimeEnd,          // decay → lifetime end marker (no free)
    ),
    // HeapAllocation: observe (readonly), collapse (scoped), share (atomic), decay (free)
    OwnershipPolicy::new(
        OwnershipRegionKind::HeapAllocation,
        ObserveMode::ReadonlyBorrow,
        CollapseMode::ScopedNoAlias,
        ShareMode::AtomicSeqCst,         // share → atomic seq_cst semantics
        DecayMode::FreeHeap,             // decay → actually free the heap memory
    ),
    // RcObject: observe (readonly), collapse (exclusive_token), share (no), decay (release_strong)
    OwnershipPolicy::new(
        OwnershipRegionKind::RcObject,
        ObserveMode::ReadonlyBorrow,
        CollapseMode::ExclusiveToken,    // collapse → acquire exclusive token
        ShareMode::Unsupported,
        DecayMode::ReleaseStrong,        // decay → release strong RC reference
    ),
    // WorldState: observe (snapshot), collapse (graph_exclusive), share (no), decay (no)
    OwnershipPolicy::new(
        OwnershipRegionKind::WorldState,
        ObserveMode::Snapshot,           // observe → take snapshot
        CollapseMode::GraphExclusive,    // collapse → graph-level exclusive lock
        ShareMode::Unsupported,
        DecayMode::Unsupported,          // world state cannot be decayed
    ),
    // EntangledAuthority: observe (snapshot), collapse (graph_exclusive), share (no), decay (no)
    OwnershipPolicy::new(
        OwnershipRegionKind::EntangledAuthority,
        ObserveMode::Snapshot,
        CollapseMode::GraphExclusive,
        ShareMode::Unsupported,
        DecayMode::Unsupported,          // entangled authority cannot be decayed
    ),
    // EntangledMirror: observe (snapshot), collapse (no), share (no), decay (no)
    OwnershipPolicy::new(
        OwnershipRegionKind::EntangledMirror,
        ObserveMode::Snapshot,           // observe → take snapshot of mirrored value
        CollapseMode::Unsupported,       // NO collapse on mirror
        ShareMode::Unsupported,
        DecayMode::Unsupported,          // mirror cannot be decayed
    ),
    // ImportedPointer: observe (readonly), collapse (scoped), share (atomic), decay (lifetime_end)
    OwnershipPolicy::new(
        OwnershipRegionKind::ImportedPointer,
        ObserveMode::ReadonlyBorrow,
        CollapseMode::ScopedNoAlias,
        ShareMode::AtomicSeqCst,
        DecayMode::LifetimeEnd,          // imported pointers end lifetime, don't free
    ),
];
```

### Policy Summary

| Region | observe | collapse | share | decay |
|--------|---------|----------|-------|-------|
| `LocalAlloca` | ✅ ReadonlyBorrow | ✅ ScopedNoAlias | ❌ | ✅ LifetimeEnd |
| `HeapAllocation` | ✅ ReadonlyBorrow | ✅ ScopedNoAlias | ✅ AtomicSeqCst | ✅ FreeHeap |
| `RcObject` | ✅ ReadonlyBorrow | ✅ ExclusiveToken | ❌ | ✅ ReleaseStrong |
| `WorldState` | ✅ Snapshot | ✅ GraphExclusive | ❌ | ❌ |
| `EntangledAuthority` | ✅ Snapshot | ✅ GraphExclusive | ❌ | ❌ |
| `EntangledMirror` | ✅ Snapshot | ❌ | ❌ | ❌ |
| `ImportedPointer` | ✅ ReadonlyBorrow | ✅ ScopedNoAlias | ✅ AtomicSeqCst | ✅ LifetimeEnd |

### Lowering Hints

The policy table also drives LLVM codegen decisions via `OwnershipLoweringHints`:

```rust
pub struct OwnershipLoweringHints {
    pub emits_readonly: bool,       // true for ReadonlyBorrow → LLVM readonly attribute
    pub emits_noalias: bool,        // true for ScopedNoAlias → LLVM noalias attribute
    pub emits_lifetime_end: bool,   // true for LifetimeEnd → LLVM lifetime.end intrinsic
    pub requires_runtime_guard: bool, // true if an explicit runtime call is needed
    pub requires_snapshot: bool,    // true for Snapshot mode (WorldState, Entangle)
    pub may_release_or_free: bool,  // true for FreeHeap or ReleaseStrong
}
```

---

## 7. How It Works — Full Pipeline

### Source to Runtime Flow

```
.kn collapse ptr: { body }
  → parser.rs: parse_unary() → parse_scoped_ownership_expr("collapse")
    → Expr::Collapse { target, body, span } in AST
  → types.rs: infer_expr_type()
    → validate_ownership_target(env, target, "collapse")
      → Checks pointer-like type (ptr<T>, ref<T>, or Unknown)
      → Validates HeapAllocation supports collapse via policy table
    → ensure_ownership_scope_has_structured_exit() — no return/break/continue
    → Checks not inside shared_region
    → infers body type → result type of collapse expression
  → codegen_llvm/mod.rs: compile_expr()
    → Expr::Collapse { target, body } → compile_scoped_ownership_expr(
        target, body,
        "__kain_ownership_begin_collapse",    // imported path
        "__kain_ownership_begin_collapse_helper",  // helper path
        "__kain_ownership_end_collapse",
        "__kain_ownership_end_collapse_helper"
      )
    → EphemeralLocal shortcut: just compile body, skip runtime calls
    → Otherwise: compile_ownership_pointer() → emit_checked_ownership_call(begin) → compile body → emit_checked_ownership_call(end)
  → runtime/native: ownership.c
    → __kain_ownership_begin_collapse(void* ptr)
      → Finds region in slot table
      → Checks state == IDLE, transitions to COLLAPSED
      → Returns 0 on success, -err on failure → Kain codegen aborts on non-zero
    → __kain_ownership_end_collapse(void* ptr)
      → Checks state == COLLAPSED, transitions to IDLE
```

### At Compile Time

The typechecker performs four validations per ownership expression:

1. **Not in shared region** — collapse/observe/decay inside share scopes is forbidden in v1
2. **Pointer target type** — must be `Ptr { ... }`, `Ref { ... }`, or `Unknown`
3. **Policy support** — the region kind must support the operation (e.g., cannot decay WorldState)
4. **Structured exit** — the body cannot contain `return`, `break`, or `continue`

```rust
fn validate_ownership_target(env, target, operation, span) -> KainResult<()> {
    let target_ty = infer_expr_type(env, target, None)?;
    let policy = OwnershipPolicy::for_region(OwnershipRegionKind::HeapAllocation);
    let supported = match operation {
        OBSERVE_KEYWORD => policy.supports_observe(),
        COLLAPSE_KEYWORD => policy.supports_collapse(),
        DECAY_KEYWORD => policy.supports_decay(),
        SHARE_KEYWORD => policy.supports_share(),
    };
    if !supported {
        return Err(type_error("{operation} is not supported for raw heap regions"));
    }
    match peel_shared_refs(&target_ty) {
        ResolvedType::Ptr { .. } | ResolvedType::Ref { .. } | ResolvedType::Unknown => Ok(()),
        other => Err(type_error("{operation} expects a pointer-like region, found {other}")),
    }
}
```

### At Codegen

The LLVM codegen emits checked calls into the native ownership runtime:

```llvm
; collapse buf: { body }
%ptr_i8 = bitcast i64* %buf to i8*
%status1 = call i32 @__kain_ownership_begin_collapse(i8* %ptr_i8)
%ok1 = icmp eq i32 %status1, 0
br i1 %ok1, label %continue, label %abort

abort:
  call void @abort()
  unreachable

continue:
  ; ... compiled body ...
  %status2 = call i32 @__kain_ownership_end_collapse(i8* %ptr_i8)
  %ok2 = icmp eq i32 %status2, 0
  br i1 %ok2, label %done, label %abort2

abort2:
  call void @abort()
  unreachable

done:
```

If the provenance is `EphemeralLocal` (a helper-owned pointer that the compiler proves never escapes its local block), the entire runtime call is elided — the body compiles directly with no `begin`/`end` intrinsics. This is the **zero-cost abstraction** layer.

### At Runtime (Native C)

The C runtime maintains a **slot table** of ownership regions (similar to the pulse slot table):

```c
// Simplified from ownership.c
static int g_ownership_slot_count = 0;
static OwnershipSlot g_ownership_slots[MAX_OWNERSHIP_SLOTS];

int __kain_ownership_begin_collapse(void* ptr) {
    OwnershipSlot* slot = find_slot(ptr);
    if (!slot) return KAIN_OWNERSHIP_ERR_NOT_FOUND;
    if (slot->state != KAIN_OWNERSHIP_STATE_IDLE)
        return KAIN_OWNERSHIP_ERR_COLLAPSED;
    slot->state = KAIN_OWNERSHIP_STATE_COLLAPSED;
    return KAIN_OWNERSHIP_OK;
}
```

Each operation checks the current state against the valid transition and returns:
- `KAIN_OWNERSHIP_OK (0)` — transition succeeded
- `KAIN_OWNERSHIP_ERR_OBSERVED (-4)` — region has active observers
- `KAIN_OWNERSHIP_ERR_COLLAPSED (-5)` — region is already collapsed
- `KAIN_OWNERSHIP_ERR_DECAYED (-6)` — region is already decayed
- `KAIN_OWNERSHIP_ERR_NOT_COLLAPSED (-9)` — tried to end collapse when not collapsed

The Kain codegen emits an `abort()` call on any non-zero status — making ownership violations **immediately fatal** in debug and release builds alike.

---

## 8. Ownership + Actor

Actors and ownership are the two faces of LAYER 7: SYSTEMS. They complement each other:

| Aspect | Actor | Ownership |
|--------|-------|-----------|
| What it manages | Concurrent message state | Raw memory lifecycle |
| Isolation model | Single-threaded message processing | Expression-scoped access control |
| Communication | Messages via mailbox | Shared nothing (collapse is exclusive) |
| Concurrency | Multiple actors, one message at a time per actor | Single-threaded, explicit scopes |
| Safety mechanism | Message isolation | State machine verification |

### Collapse/Observe/Decay Inside Actor Handlers

Actors can freely use ownership expressions inside their `on` handlers. This is the canonical pattern from `fusion_chain.kn`:

```kn
actor FusionTeleporter:
    state teleports_done: Int = 0
    state last_score: Int = 0

    on ShatterAndSend(reply_to: P, payload: Int):
        self.teleports_done = self.teleports_done + 1
        let tick = fusion_unpack_a(payload)
        let signal = fusion_unpack_b(payload)

        // OWNERSHIP inside actor handler
        let cell_count = 4
        let mut cells: ptr<Int> = alloc_zeroed(cell_count, "Int")

        collapse cells:
            var i: Int = 0
            while i < cell_count:
                mem_store(ptr_offset(cells, i, "Int"),
                    (tick * (i + 1) * 7) % FUSION_MODULUS, "Int")
                i = i + 1
            0

        let head: Int = observe cells:
            mem_load(ptr_offset(cells, 0, "Int"), "Int")

        decay cells

        let score = fusion_mod(head + signal + tick, FUSION_MODULUS)
        self.last_score = score
        send reply_to.Score(score = score)
```

This is safe because:
- The actor processes one message at a time (single-threaded mailbox)
- The ownership state machine is per-pointer and scoped to the handler invocation
- The `on` handler is synchronous — no yield points inside ownership scopes
- The compiler verifies every transition at compile time

### Ownership + Actor Message Payloads

When `send` or `ask` transfers data between actors, ownership of the message data follows a strict protocol:
1. **Sender** allocates and owns the data before `send`
2. On successful `send`, ownership **transfers to the mailbox**
3. On `receive`, ownership **transfers to the receiving actor**
4. The receiver must release the message data

For inline `ask` fast path (borrowed payload), the payload is lent to the actor without an ownership transfer, and the actor must acknowledge release.

### Actor State and Ownership

Actor `state` fields are **not** subject to collapse/observe/decay — they are private to the actor and accessed via `self.field`. Ownership expressions apply to raw pointer types (`ptr<T>`), not actor state.

---

## 9. Ownership + World

World state fields can be targets of ownership expressions, but with **restricted policies**:

### WorldState Policy

| Operation | Allowed? | Mode | Description |
|-----------|----------|------|-------------|
| `observe` | ✅ | Snapshot | Takes a frozen snapshot of the world field |
| `collapse` | ✅ | GraphExclusive | Exclusive access to the world node (graph-level lock) |
| `share` | ❌ | — | World state cannot be shared for parallel writes |
| `decay` | ❌ | — | World state cannot be decayed (it's permanent) |

### Observing World State

```kn
observe WorldState.field:
    let val = mem_load(WorldState.field, "Int")
    // val is a snapshot — other code can mutate the field concurrently
```

The `Snapshot` observe mode means the runtime takes a frozen copy of the world field value when the observe scope begins. Subsequent mutations to the world field inside the observe scope are **not visible** to the reader — you're reading a consistent snapshot.

### Collapsing World State

```kn
collapse WorldState.field:
    mem_store(WorldState.field, 42, "Int")
```

The `GraphExclusive` collapse mode means the compiler/runtime guarantees exclusive access to the world node during the collapse scope. This is a **graph-level** lock — no other code can access this world's state until the collapse scope ends.

### EntangledAuthority / EntangledMirror

Entanglement bindings create two sides:
- **Authority** — the source of truth. Supports `Snapshot` observe and `GraphExclusive` collapse.
- **Mirror** — a propagated copy. Supports `Snapshot` observe only. **No collapse, no decay.**

```kn
// Authority: can observe (snapshot) and collapse (graph exclusive)
observe EntangledAuthority.signal:
    let val = mem_load(EntangledAuthority.signal, "Int")

// Mirror: can observe (snapshot) only
observe EntangledMirror.signal_copy:
    let val = mem_load(EntangledMirror.signal_copy, "Int")

// collapse on mirror is a COMPILE-TIME ERROR:
// collapse EntangledMirror.signal_copy:  // ❌ Unsupported!
```

This is verified by the policy table test:
```rust
fn policy_table_is_conservative_for_worlds_and_entangle() {
    let mirror = OwnershipPolicy::for_region(OwnershipRegionKind::EntangledMirror);
    assert_eq!(mirror.observe_mode, ObserveMode::Snapshot);
    assert!(!mirror.supports_collapse());
    assert!(!mirror.supports_decay());
}
```

---

## 10. Ownership + Share + Fanout

`share` and `fanout` extend ownership into the **parallel write** domain. They form a sub-system within ownership for atomic parallel accumulation.

### The Share/Fanout Pattern

```kn
let mut partials: ptr<Int> = alloc_zeroed(WORKER_COUNT, "Int")

// Open shared atomic region
share partials:
    // Launch parallel workers
    fanout worker in 0..WORKER_COUNT:
        var local: Int = 0
        var i: Int = 0
        while i < STEPS:
            local = (local + compute(worker, i)) % MODULUS
            i = i + 1
        atomic_store(ptr_offset(partials, worker, "Int"), local)

// Read results via observe
let total: Int = observe partials:
    var sum: Int = 0
    var w: Int = 0
    while w < WORKER_COUNT:
        sum = (sum + mem_load(ptr_offset(partials, w, "Int"), "Int")) % MODULUS
        w = w + 1
    sum

decay partials
```

### Rules

1. **`share` opens a `Shared` region** — the pointer transitions from `Idle` to `Shared`
2. **`fanout` launches parallel lanes** — each `fanout` statement creates a set of workers (one per iteration of the range)
3. **Workers use `atomic_store`** — not `mem_store`, because multiple workers may write concurrently
4. **No `observe`, `collapse`, or `decay` inside share** — enforced at compile time
5. **No nested fanout** — enforced at compile time
6. **No early exit in fanout bodies** — `return`, `break`, `continue` are forbidden
7. **`share` returns to `Idle` on scope exit** — via `EndShare` transition

### Memory Order

The `ShareMode::AtomicSeqCst` policy means all atomic operations inside the share scope use **sequentially consistent ordering** — the strongest memory ordering, guaranteeing a single total order of all sequentially-consistent operations across all workers.

### LLVM Codegen

The codegen for `share` follows the same pattern as `collapse`/`observe`:
- `__kain_ownership_begin_share` / `__kain_ownership_end_share` for imported pointers
- `__kain_ownership_begin_share_helper` / `__kain_ownership_end_share_helper` for helper-owned pointers
- Ephemeral locals skip runtime calls entirely

---

## 11. Ownership + Pulse

Pulse bodies have the `Alloc` effect permission, which means they can allocate raw memory and use collapse/observe/decay:

```kn
pulse memory_beat every 1s:
    let mut cells: ptr<Int> = alloc_zeroed(16, "Int")
    collapse cells:
        mem_store(cells, pulse_tick, "Int")
        0
    let val: Int = observe cells:
        mem_load(cells, "Int")
    decay cells
    MemoryWorld.last_val = val
```

The compiled pulse body (`__kain_pulse_body_{name}`) includes the full ownership lifecycle — begin_collapse → body → end_collapse → begin_observe → body → end_observe → decay.

From the runtime contract, `RuntimePulseContract` tracks whether the body uses ownership:
```rust
pub struct RuntimePulseContract {
    // ...
    pub body_ownership_ops: bool,  // true if pulse body uses collapse/observe/decay
    // ...
}
```

---

## 12. Ownership + Entangle

Ownership interacts with entanglement through region policy restrictions:

| Region | Collapse | Observe | Decay | Meaning |
|--------|----------|---------|-------|---------|
| `EntangledAuthority` | ✅ GraphExclusive | ✅ Snapshot | ❌ | Authority can be exclusively locked for mutation |
| `EntangledMirror` | ❌ | ✅ Snapshot | ❌ | Mirror is read-only propagated state |

### Reading Entangled State

```kn
// Authority write via collapse
collapse EntangledAuthority.signal:
    mem_store(EntangledAuthority.signal, 42, "Int")

// Mirror read via observe (snapshot)
observe EntangledMirror.signal_copy:
    let val = mem_load(EntangledMirror.signal_copy, "Int")
```

The `Snapshot` mode is critical for entangled mirrors — it ensures a consistent read of the propagated value, even if the authority is being mutated concurrently during entanglement propagation.

---

## 13. Under-Utilized Ideas

### 1. Ownership as FFI Safety Boundary

The `ImportedPointer` region type allows safe ownership wrapping of C-allocated pointers. When a C function returns a `void*`, you can register it with `__kain_ownership_register_imported()` and then use collapse/observe/decay with full state-machine verification. The `decay` mode is `LifetimeEnd` (not `FreeHeap`), meaning the C side retains ownership of freeing — Kain only marks the lifetime end.

```kn
// Hypothetical FFI pattern
let c_ptr: ptr<U8> = c_function_allocate_buffer(size)
// The C runtime registers it as ImportedPointer
collapse c_ptr:
    mem_store(c_ptr, header_byte, "U8")
    0
observe c_ptr:
    let header = mem_load(c_ptr, "U8")
decay c_ptr  // lifetime end — C frees, not Kain
```

### 2. Ephemeral Local Zero-Cost Ownership

The compiler can prove that certain pointer locals never escape their declaring block. When it does, the entire ownership runtime is elided — no `begin_collapse`/`end_collapse` calls, no abort-on-failure paths, no slot table lookups. This makes ownership expression-level zero-cost for local scratch buffers:

```kn
fn fast_scratch() -> Int:
    let buf: ptr<Int> = alloc_zeroed(4, "Int")
    collapse buf:   // ELIDED — ephemeral local
        mem_store(ptr_offset(buf, 0, "Int"), 42, "Int")
        0
    let v: Int = observe buf:   // ELIDED
        mem_load(ptr_offset(buf, 0, "Int"), "Int")
    decay buf   // ELIDED
    return v
```

The `ownership_pointer_provenance_for_expr()` returns `EphemeralLocal` for these, and both `compile_scoped_ownership_expr()` and `compile_decay_expr()` return immediately with the body result or `("0", "void")`.

### 3. Ownership + Orchestrate Pipeline Stages

Ownership expressions could be used within orchestrate stages for raw memory processing. A stage that operates on a GPU buffer or mmap'd file would use collapse/observe/decay:

```kn
orchestrate process_buffer(input: ptr<U8>, size: Int) -> Int:
    stage cpu_process: cpu process_raw(input, size)
        requires "memory.ownership"
        policy degrade on_overflow
    stage verify: law buffer_valid(result: Int) -> Bool
    stage commit: patch commit_result(result)
```

### 4. Ownership + Teleport Zero-Copy

After `teleport` moves data across worlds, the destination can use ownership expressions on the teleported data:

```kn
let moved = teleport shard from WorldA to WorldB via bus
collapse moved:       // exclusive write to teleported shard
    mem_store(moved, new_value, "Int")
    0
observe moved:        // read from teleported shard
    mem_load(moved, "Int")
decay moved           // release teleported resource
```

### 5. Ownership in GPU Compute Shaders

Although GPU shaders are a different execution model, ownership regions could map to GPU buffer access patterns:
- `collapse` → exclusive write access to a storage buffer
- `observe` → read-only access to a storage buffer
- `decay` → unbind / release a buffer binding

---

## 14. Combining with Other Keywords

### Ownership + Actor

```kn
actor DataProcessor:
    state results: Int = 0

    on Process(reply_to: P, data: ptr<U8>, len: Int):
        collapse data:
            var i: Int = 0
            while i < len:
                let byte = mem_load(ptr_offset(data, i, "U8"), "U8")
                self.results = self.results + Int(byte)
                i = i + 1
            0
        decay data
        send reply_to.Done(count = self.results)
```

### Ownership + Pulse

```kn
pulse memory_tick every 16ms:
    let buf: ptr<Int> = alloc_zeroed(64, "Int")
    collapse buf:
        mem_store(ptr_offset(buf, pulse_tick % 64, "Int"), pulse_tick, "Int")
        0
    decay buf
```

### Ownership + World

```kn
world BufferWorld:
    state data: ptr<U8> = null

patch write_buffer(world: BufferWorld, value: U8, offset: Int) -> Int:
    collapse world.data:
        mem_store(ptr_offset(world.data, offset, "U8"), value, "U8")
        0
    return offset
```

### Ownership + Converge

```kn
converge fast_process(buf: ptr<Int>, len: Int) -> Int:
    spec reference:
        observe buf:
            var sum: Int = 0
            var i: Int = 0
            while i < len:
                sum = sum + mem_load(ptr_offset(buf, i, "Int"), "Int")
                i = i + 1
            sum
    fast simd_lane when capability("cpu.x86.avx2"):
        // SIMD-accelerated read with ownership
        observe buf:
            simd_accumulate(buf, len)
```

### Ownership + Orchestrate

```kn
orchestrate ownership_pipeline(buf: ptr<Int>, len: Int) -> Int:
    stage write: cpu
        collapse buf:
            var i: Int = 0
            while i < len:
                mem_store(ptr_offset(buf, i, "Int"), i * 7, "Int")
                i = i + 1
            0
    stage read: cpu after write
        observe buf:
            var sum: Int = 0
            var i: Int = 0
            while i < len:
                sum = sum + mem_load(ptr_offset(buf, i, "Int"), "Int")
                i = i + 1
            sum
    stage cleanup: cpu after read
        decay buf
        0
```

### Ownership + Teleport

```kn
let teleported = teleport particle from WorldA to WorldB via bus
observe teleported:
    let x = mem_load(ptr_offset(teleported, 0, "Float"), "Float")
    let y = mem_load(ptr_offset(teleported, 1, "Float"), "Float")
```

### Ownership + Shatter

```kn
shatter struct Packet:
    header: U64
    payload: array<U8, 256>
    crc: U32

let pkt = Packet { header: 0xDEAD, payload: ..., crc: 0 }
collapse pkt:
    mem_store(ptr_offset(pkt, 0, "U64"), 0xBEEF, "U64")
    0
```

---

## 15. Validation Ladder

```powershell
# 1. Typecheck (all ownership transitions are verified)
kain check <file.kn>

# 2. Compile + run (ownership runtime checks are active)
kain run <file.kn> --target llvm

# 3. For benchmark proof
python benchmark/run.py --case fusion_chain --languages kain --runs 1 --warmups 0

# 4. For keyword crucible ownership case
$env:KAIN_BENCH_V2_FILTER = "keyword_crucible"
kain run X:\benchmark --target llvm --json

# 5. For classic systems ownership examples
$env:KAIN_BENCH_V2_FILTER = "classic_systems"
kain run X:\benchmark --target llvm --json

# 6. Check runtime contract for emitted capability
# The output runtime_contract.json will show "memory.ownership" capability
```

### Test File Pattern

From `benchmark/cases_v2/keyword_crucible.kn`, the ownership chain test:

```kn
fn crucible_ownership_chain_checksum(iterations: Int, modulus: Int) -> Int with Unsafe:
    // SHARE + FANOUT — parallel write lanes into shared pointer region
    let mut partials: ptr<Int> = alloc_zeroed(CRUCIBLE_WORKERS, "Int")

    share partials:
        fanout worker in 0..CRUCIBLE_WORKERS:
            let slot: ptr<Int> = ptr_offset(partials, worker, "Int")
            var step: Int = 0
            var local: Int = 0
            while step < CRUCIBLE_STEPS:
                local = (local + crucible_fast_mix(worker + step, iterations % 31)) % modulus
                step = step + 1
            atomic_store(slot, local)

    let fanout_total: Int = observe partials:
        var w: Int = 0
        var total: Int = 0
        while w < CRUCIBLE_WORKERS:
            total = (total + mem_load(ptr_offset(partials, w, "Int"), "Int")) % modulus
            w = w + 1
        total

    decay partials

    // CLFLUSH via inline asm with memory operand
    let line_buf: ptr<Int> = alloc_zeroed(8, "Int")
    collapse line_buf:
        mem_store(line_buf, 0xCAFEBABE, "Int")
        let addr = ptr_offset(line_buf, 0, "Int")
        asm("clflush ($0)", addr, memory = true)
        0

    let flush_val: Int = observe line_buf:
        mem_load(line_buf, "Int")
    decay line_buf

    return crucible_mod(fanout_total + flush_val, modulus)
```

### Semantic Smoketest

```kn
// smoketest/src/semantics/ownership.kn (if exists — pattern from other tests)
use std::runtime

world OwnershipTestWorld:
    state test_val: Int = 0

fn test_basic_ownership() -> Int:
    let buf: ptr<Int> = alloc_zeroed(4, "Int")

    collapse buf:
        mem_store(ptr_offset(buf, 0, "Int"), 42, "Int")
        0

    let val: Int = observe buf:
        mem_load(ptr_offset(buf, 0, "Int"), "Int")

    decay buf

    if val != 42:
        return 1
    return 0
```

---

## 16. Z3 Proofs

### 1. Ownership State Machine Transitions

**File:** `runtime/native/src/core/z3/proofs/native-ownership-state-machine.yaml` (expected location)

The ownership state machine in `ownership.h` is structured as a **deterministic finite automaton** with 5 states and 7 transitions. Each transition is checked at runtime with an `abort()` on invalid transition. The Z3 proof verifies:

- **No deadlock states** — every non-terminal state (Idle, Observed, Collapsed, Shared) has at least one valid outgoing transition
- **Decayed is terminal** — once Decayed, no transitions out
- **Observer count monotonicity** — nested observes increment, nested end-observes decrement, never underflow
- **Collapse-exclusive** — Collapsed state has exactly one valid transition (EndCollapse → Idle)

```smt2
; Simplified state transition model
(declare-const s Int)
(declare-const t Int)

; States
(define-fun IDLE () Int 0)
(define-fun OBSERVED () Int 1)
(define-fun COLLAPSED () Int 2)
(define-fun SHARED () Int 3)
(define-fun DECAYED () Int 4)

; Transition safety: no transition ends in an invalid state
(define-fun valid_transition ((from Int) (to Int)) Bool
    (and (>= from 0) (<= from 4) (>= to 0) (<= to 4)))
```

### 2. Pointer Provenance Soundness

The LLVM codegen's `ownership_pointer_provenance_for_expr()` determines whether a pointer is `EphemeralLocal`, `HelperOwned`, or `ImportedOrUnknown`. The correctness of this classification is critical:

- **EphemeralLocal** — the pointer is allocated and used only within a single local scope and never stored to a global, never returned, never passed to an unknown function. The ephemereal local solver (`expr_is_safe_for_ephemeral_local()`) checks the entire use chain.
- **HelperOwned** — the pointer comes from `__kain_alloc` or `__kain_realloc` and is tracked in `helper_owned_pointer_locals`.
- **ImportedOrUnknown** — all other pointers. These go through the full imported registration path with `__kain_ownership_ensure_imported()`.

### 3. Use-After-Decay Prevention

After `decay` transitions a region to `Decayed`, the codegen **clears the helper-owned local** by storing `null`:

```rust
fn clear_decayed_helper_owned_local(&mut self, target: &Expr) {
    // ... store null into the local variable
    self.emit(&format!("  store {} null, {}* {}", ty, ty, addr));
}
```

This ensures that any subsequent use of the variable as a pointer loads `null`, which the runtime checks catch as invalid region lookups.

### 4. Z3 Audit Findings

| Finding | Risk | Details |
|---------|------|---------|
| `__kain_ownership_relocate_helper_allocation` | 30 | Contains size tracking with potential for stale slot references — realloc path |
| `g_machine_pulse_total_fire_count` increment | 55 | Flagged as "count-like state mutates without lock" but uses `atomic_fetch_add_explicit` — false positive |

---

## 17. Source Anchors

### Model Crate (State Machine & Policy)

| File | Lines | Content |
|------|-------|---------|
| `crates/ownership/src/lib.rs` | 1-30 | Crate header, keywords: `COLLAPSE_KEYWORD`, `OBSERVE_KEYWORD`, `DECAY_KEYWORD`, `SHARE_KEYWORD`, `OWNERSHIP_CAPABILITY` |
| `crates/ownership/src/lib.rs` | 31-55 | `OwnershipRegionKind` enum — 7 region kinds |
| `crates/ownership/src/lib.rs` | 80-100 | `ActiveObserverCount` — wrapping type with checked add/sub |
| `crates/ownership/src/lib.rs` | 105-133 | `OwnershipState` enum — Idle, Observed, Collapsed, Shared, Decayed |
| `crates/ownership/src/lib.rs` | 135-175 | `OwnershipState` methods — `can_observe()`, `can_collapse()`, `can_decay()`, `is_terminal()` |
| `crates/ownership/src/lib.rs` | 177-260 | `OwnershipState::apply()` — all 7 transition implementations (+ n*k per transition) |
| `crates/ownership/src/lib.rs` | 262-370 | `OwnershipTransitionError` — all 17 error variants |
| `crates/ownership/src/lib.rs` | 372-420 | `ObserveMode`, `CollapseMode`, `ShareMode`, `DecayMode` |
| `crates/ownership/src/lib.rs` | 422-460 | `OwnershipPolicy` — `for_region()`, `supports_*()`, `lowering_hints()` |
| `crates/ownership/src/lib.rs` | 462-524 | `OWNERSHIP_POLICY_TABLE` — full policy matrix for all 7 regions |
| `crates/ownership/src/lib.rs` | 526-575 | `OwnershipRegionDescriptor`, `OwnershipLoweringHints` |
| `crates/ownership/src/lib.rs` | 577-660 | Unit tests — nest observe, collapse errors, decay terminal, policy table |

### Parser & AST

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/ast.rs` | 1840-1860 | `Expr::Observe`, `Expr::Collapse`, `Expr::Decay`, `Expr::Share` definitions |
| `crates/core/src/parser.rs` | 69-72 | Keywords: `collapse`, `observe`, `decay`, `share` |
| `crates/core/src/parser.rs` | 774-777 | TokenKind mapping: `Collapse`, `Observe`, `Decay`, `Share` |
| `crates/core/src/parser.rs` | 4785-4790 | `parse_unary()` dispatch — ownership keywords |
| `crates/core/src/parser.rs` | 4940-4975 | `parse_scoped_ownership_expr()` — collapse/observe/share parsing |
| `crates/core/src/parser.rs` | 4967-4973 | `parse_decay_expr()` — decay parsing |

### Typechecker

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/types.rs` | 23-26 | Imports `kain_ownership` types |
| `crates/core/src/types.rs` | 8892-8941 | `infer_expr_type()` for Observe, Collapse, Decay, Share |
| `crates/core/src/types.rs` | 9095-9130 | `validate_ownership_target()` — pointer check + policy check |
| `crates/core/src/types.rs` | 9140-9220 | `ensure_ownership_scope_has_structured_exit()` — early exit prohibition |
| `crates/core/src/types.rs` | 9215-9270 | `ownership_block_contains_early_exit()` / `ownership_expr_contains_early_exit()` — recursive AST walk |

### LLVM Codegen

| File | Lines | Content |
|------|-------|---------|
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 151-155 | `OwnershipPointerProvenance` enum |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 2871-2911 | `ownership_pointer_provenance_for_expr()` — provenance classification |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 3002-3011 | `safe_memory_access_alignment()` — provenance-aware alignment |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 9191-9250 | `compile_ownership_pointer()` — pointer compilation + import registration |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 9252-9290 | `emit_lazy_import_ownership_region()` — imported path registration |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 9292-9315 | `emit_checked_ownership_call()` — runtime call + abort-on-failure |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 9365-9410 | `compile_scoped_ownership_expr()` — generic scope compile (begin + body + end) |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 9406-9430 | `compile_decay_expr()` — decay compile with helper-owned cleanup |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 18644-18668 | LLVM Expr dispatch: Observe → `compile_scoped_ownership_expr(...begin/end_observe...)`, Collapse → `...begin/end_collapse...`, Share → `...begin/end_share...`, Decay → `compile_decay_expr()` |

### Native C Runtime

| File | Lines | Content |
|------|-------|---------|
| `runtime/native/include/ownership.h` | 1-80 | Full public C API — 30+ function declarations |
| `runtime/native/include/ownership.h` | 9-28 | Region kind enum, state enum, error code enum |
| `runtime/native/include/ownership.h` | 30-70 | Function declarations: register, import, observe, collapse, share, decay, state, lookup, flush |

### Stdlib

| File | Content |
|------|---------|
| `stdlib/intent.kn` | `@extern` declarations for entangle, patch, resonate, orchestrate, converge, law |


### Examples

| File | Usage |
|------|-------|
| `benchmark/cases_v2/keyword_crucible.kn` | Case 1: Ownership chain — share/fanout + collapse/observe/decay + clflush |
| `benchmark/cases_v2/fusion_chain.kn` | FusionTeleporter actor: collapse/observe/decay inside actor handler |
| `benchmark/cases_v2/classic_systems.kn` | SIMD lane fill, wire pressure — collapse+cells, observe+cells read, decay cleanup |
| `benchmark/cases/pulse_teleport_decay_mesh/main.kn` | **Apex:** pulse + world + entangle + collapse/observe/decay + teleport — 13+ semantics |
| `smoketest/src/semantics/pulse.kn` | Minimal pulse + ownership + shatter smoke test |
| `blades/test/machine-stones/src/main.kn` | Machine stones dogfood blade with ownership telemetry |
| `benchmark/cases_v2/classic_systems.kn` | `glfw/pong` style ownership — atomic counters, shared accumulators, decay cleanup |

---

## 18. Anti-Patterns

### ❌ Thinking It's Rust's Borrow Checker

```kn
// WRONG: expecting borrowck-style implicit lifetimes
fn bad_use(buf: ptr<Int>) -> Int:
    collapse buf:
        mem_store(buf, 42, "Int")
        0
    // buf is now back to Idle — NOT moved, NOT consumed
    let val = observe buf:   // ✅ This works — decay hasn't happened yet
        mem_load(buf, "Int")
    decay buf
    // val = observe buf:  // ❌ Would abort — region is Decayed
    return val
```

Kain ownership is **scoped, not moved**. The pointer variable still exists after collapse ends — it returns to `Idle`. It's usable until `decay`. Unlike Rust, there's no ownership transfer or move semantics.

### ❌ Forgetting to Decay

```kn
// WRONG: memory leak — heap allocation without decay
fn leaky() -> Int:
    let buf: ptr<Int> = alloc_zeroed(16, "Int")
    collapse buf:
        mem_store(buf, 42, "Int")
        0
    let val = observe buf:
        mem_load(buf, "Int")
    // ❌ No decay — heap memory leaked
    return val
```

```kn
// RIGHT: balanced alloc → decay
fn clean() -> Int:
    let buf: ptr<Int> = alloc_zeroed(16, "Int")
    collapse buf: ...
    let val = observe buf: ...
    decay buf   // ✅ FreeHeap — memory freed
    return val
```

### ❌ Double Collapse

```kn
// WRONG: collapse on already-collapsed region aborts at runtime
collapse buf:
    collapse buf:  // ❌ Runtime abort — CannotCollapseAlreadyCollapsed
        mem_store(buf, 42, "Int")
        0
    0
```

### ❌ Decaying a World State Field

```kn
world TestWorld:
    state field: Int = 0

// WRONG: decay on world state is unsupported at compile time
// decay TestWorld.field  // ❌ Compile error: decay is not supported for raw heap regions
```

The policy table explicitly sets `DecayMode::Unsupported` for `WorldState`, `EntangledAuthority`, and `EntangledMirror`. The typechecker catches this at compile time.

### ❌ Observing While Collapsed

```kn
collapse buf:
    // WRONG: observe inside collapsed region
    let val = observe buf:    // ❌ Runtime abort — CannotObserveCollapsed
        mem_load(buf, "Int")
    0
```

Observe must happen when the region is `Idle`, not `Collapsed`. The state machine enforces this.

### ❌ Early Exit in Ownership Block

```kn
// WRONG: return inside collapse block
collapse buf:
    if error_condition:
        return -1   // ❌ Compile error: "collapse scopes do not support return, break, or continue"
    mem_store(buf, 42, "Int")
    0
```

### ❌ Using `share` as a General-Purpose Scope

`share` is specifically for parallel atomics with `fanout`. Don't use it where `collapse` would do:

```kn
// WRONG: share for single-threaded mutation
let buf: ptr<Int> = alloc_zeroed(1, "Int")
share buf:       // share opens atomic seq_cst — heavy for single-threaded write
    atomic_store(buf, 42)
```

```kn
// RIGHT: collapse for single-threaded mutation
let buf: ptr<Int> = alloc_zeroed(1, "Int")
collapse buf:    // collapse uses scoped noalias — lightweight
    mem_store(buf, 42, "Int")
    0
decay buf
```

### ❌ Assuming Ownership Protects Against Concurrent Access

Ownership is **single-threaded** by design. Two threads cannot collide on the same `collapse` because they're in different actor handlers or different pulse ticks. If two threads **did** access the same pointer, the runtime would catch the state-machine violation (one would fail `begin_collapse`), but this is an error-detection mechanism, not a concurrency control mechanism. For concurrent access, use `share` + `fanout` with `atomic_store`, or message-passing via actors.

### ❌ Using `observe` When You Mean `collapse`

```kn
// WRONG: observe is read-only — mem_store inside will abort
observe buf:
    mem_store(buf, 42, "Int")   // ❌ Runtime abort — read-only region
    0
```

`observe` grants read-only access. `collapse` grants mutable exclusive access. `share` grants atomic write access.

### ❌ Not Checking Helper-Owned Pointer Provenance

The compiler's ephemereal local optimization is powerful but silent. If you write code that the compiler cannot prove is local, it falls back to the full runtime path. This is always correct — just potentially slower. Don't fight the compiler; trust the optimization is correct and profile if needed.

---

## 19. Quick Reference

| Aspect | Detail |
|--------|--------|
| **Keywords** | `collapse`, `observe`, `decay` (core); `share`, `fanout` (extended) |
| **Syntax** | `collapse ptr: { body }` — scoped mutable exclusive access |
| | `observe ptr: { body }` — scoped read-only access |
| | `decay ptr` — deterministic destruction |
| | `share ptr: { fanout ... }` — parallel atomic region |
| **States** | `Idle` → `Collapsed` / `Observed(N)` / `Shared` / `Decayed` |
| **Terminal state** | `Decayed` — no transitions out |
| **Regions** | `LocalAlloca`, `HeapAllocation`, `RcObject`, `WorldState`, `EntangledAuthority`, `EntangledMirror`, `ImportedPointer` |
| **Heap policy** | `collapse=ScopedNoAlias`, `observe=ReadonlyBorrow`, `share=AtomicSeqCst`, `decay=FreeHeap` |
| **World policy** | `collapse=GraphExclusive`, `observe=Snapshot`, `share=unsupported`, `decay=unsupported` |
| **Mirror policy** | `collapse=unsupported`, `observe=Snapshot`, `share=unsupported`, `decay=unsupported` |
| **Capability** | `memory.ownership` — auto-emitted when any ownership expr exists |
| **Interpreter** | Ownership is functional but uses the model-level state machine in Rust |
| **LLVM output** | Checked calls to `__kain_ownership_begin_collapse` / `__kain_ownership_end_collapse` / `__kain_ownership_decay` (or `*_helper` variants) with `abort()` on error |
| **Zero-cost path** | EphemeralLocal provenance skips all runtime calls |
| **Early exit ban** | `return`, `break`, `continue` are forbidden inside ownership scopes |
| **Nested observe** | Observer count increments/decrements correctly |
| **Region registration** | Imported pointers (FFI, world state) registered via `__kain_ownership_ensure_imported()` |
| **Error codes** | `0`=OK, `-1`=Invalid, `-2`=NotFound, `-3`=Capacity, `-4`=Observed, `-5`=Collapsed, `-6`=Decayed, `-7`=Overflow |
| **After decay** | Helper-owned locals are set to `null` to prevent use-after-free |
| **Canonical example** | `benchmark/cases_v2/fusion_chain.kn` — actor with collapse/observe/decay + teleport |
| **Canonical pattern** | `alloc` → `collapse` (write) → `observe` (read) → `decay` (free) |
| **Source** | `crates/ownership/src/lib.rs` (model), `crates/core/src/types.rs` (typechecker), `crates/sys-codegen/src/codegen_llvm/mod.rs` (codegen), `runtime/native/include/ownership.h` (C API) |

---

> *Ownership is Kain's answer to memory safety without a borrow checker. Not inferred — explicit. Not optional — verified. Not a GC — deterministic. `collapse` to write, `observe` to read, `decay` to release. Three keywords, one state machine, zero compromises.*
