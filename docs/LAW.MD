# Law - Compiler-Owned Invariant Predicate

**Status:** Research synthesis, 2026-06-07
**Based on:** `crates/core/src/ast.rs`, `crates/core/src/parser.rs`, `crates/core/src/types.rs`, `crates/core/src/runtime_contract.rs`, `crates/core/src/realtime_app_bundle.rs`, `crates/core/src/runtime.rs`, `crates/sys-codegen/src/codegen_llvm/mod.rs`, `crates/orchestrate/src/stage.rs`, `stdlib/intent.kn`, `smoketest/src/semantics/law.kn`, `smoketest/src/semantics/orchestrate.kn`, `crates/semantic/symbol_corpus/*.kn`, `blades/experiments/convergence/src/*.kn`, `blades/python/24_tet/src/resonate_py_effects.kn`, RULEBOOK.MD, CATALOG.MD, GLOSSARY.MD, PULSE.MD

______________________________________________________________________

## Table of Contents

1. [What Is Law?](#what-is-law)
1. [The Core Idea ~ Compiler-Owned Invariant Predicate](#the-core-idea)
1. [Syntax Reference](#syntax-reference)
1. [How It Works ... Full Pipeline](#how-it-works)
1. [The Law Patterns :: How to Use It](#the-law-patterns)
1. [Under-Utilized Ideas - Creative Semantic Abuse](#under-utilized-ideas)
1. [Combining with Other Keywords](#combining-with-other-keywords)
1. [Stdlib Telemetry API](#stdlib-telemetry-api)
1. [Runtime Contract Metadata](#runtime-contract-metadata)
1. [LLVM Codegen](#llvm-codegen)
1. [Native C Runtime](#native-c-runtime)
1. [Z3 Proofs](#z3-proofs)
1. [Validation Ladder](#validation-ladder)
1. [Anti-Patterns](#anti-patterns)
1. [Quick Reference](#quick-reference)
1. [Source Anchors](#source-anchors)

______________________________________________________________________

## What Is Law?

`law` is a **top-level declaration** in Kain that defines an **invariant predicate** ___ a function that returns `Bool` and represents a correctness constraint that the compiler, runtime, and orchestrate engine can all see and reason about.

```kn
law value_in_range(v: Int) -> Bool:
    return v >= 0 and v < 1000000007
```

A `law` is **not** a fancy `if` statement. It is a **compiler-owned invariant declaration**. When you write `law`, you're telling the compiler: "this is a constraint on my system that must hold for correctness." The compiler records it in the runtime contract, makes it visible to orchestrate stages, and emits it into telemetry metadata ~ without you writing any of that plumbing yourself.

Where a normal `fn` is "here's a computation," a `law` is "here's a truth about my system."

______________________________________________________________________

## The Core Idea - Compiler-Owned Invariant Predicate

### What Law IS NOT

- ❌ Not an assertion - `law` doesn't abort, crash, or halt. It returns `Bool`, and you decide what to do with the result.
- ❌ Not a runtime guard <--> the compiler doesn't insert automatic checks before every world write. You call laws explicitly.
- ❌ Not a `patch` >> a law has no mutation, no journal, no undo mode. It's a pure predicate.
- ❌ Not a `converge` ~~ there's no spec/fast lane selection, no capability gating, no verification.
- ❌ Not an `axiom` => there's no architecture/capability predicates, no guarantees, no fallback.
- ❌ Not a Z3 proof |-> law returns `Bool` at runtime. It doesn't feed Z3 assertions (that's `std::proof` / `std::z3`).
- ❌ Not a native C construct <--> there's zero `law.h`, `law.c`, or `abi_law_*` code in the entire native runtime.

### What Law IS

- ✅ A **compiler-owned invariant** ... the typechecker enforces `-> Bool` at declaration time
- ✅ A **runtime-visible contract** >> laws appear in the runtime contract bundle alongside worlds, patches, entangles, orchestrates
- ✅ An **orchestrate stage kind** 〰 `stage name: law check_invoke(args)` is a first-class pipeline stage
- ✅ A **pipeline guard** – `requires law_stage` makes other stages depend on the law passing
- ✅ A **telemetry point** * * * `law_status(law_call())` converts `Bool` → `Int` (0 = valid, -1 = invalid)
- ✅ A **world-aware predicate** ->> law bodies are typechecked with `in_patch = true`, giving them access to world state
- ✅ **Pure by convention** ... the typechecker allows effects (IO, Async, GPU) but the intent is a pure predicate
- ✅ **Zero runtime overhead beyond a function call** === compiles to the same LLVM IR as `fn`
- ✅ **Broader effect permissions than `fn`** ... laws can read world state, call patches, and touch entangles

### Where Law Sits in the Decision Ladder

| Layer | Concept | Keyword |
|-------|---------|---------|
| LAYER 2: STATE INTEGRITY | "Invariant predicate?" | **`law`** |
| LAYER 2: STATE INTEGRITY | "Journaled mutation?" | `patch` |
| LAYER 3: CROSS-BOUNDARY | "State mirroring?" | `entangle` |
| LAYER 6: ORCHESTRATION | "Multi-stage pipeline?" | `orchestrate` |

The RULEBOOK says: *"A `law` is not 'an `if` statement that checks bounds.' It is a compiler-owned invariant declaration."*

______________________________________________________________________

## Syntax Reference

### Canonical Syntax

```kn
law <name>(<params>) -> Bool:
    <body>
```

### Concrete Examples

```kn
// Range check => the most common pattern
law signal_in_bounds(value: Int) -> Bool:
    return value >= 0 and value < 1000000007

// Health check
law health_positive(health: Int) -> Bool:
    return health > 0 and health <= 1000000

// Empty check
law name_nonempty(name: String) -> Bool:
    return name != ""

// Multi-field invariant
law cell_in_maze(x: Int, y: Int, width: Int, height: Int) -> Bool:
    return x >= 0 and x < width and y >= 0 and y < height

// Complex boolean logic
law state_machine_valid(phase: Int, mode: Int) -> Bool:
    return (phase == 0 and mode >= 0 and mode <= 3) or
           (phase == 1 and mode >= 0 and mode <= 7)

// Using law inside a law (law composition)
law start_target_valid(start: Int, target: Int, cells: Int) -> Bool:
    return signal_in_bounds(start, cells)
       and signal_in_bounds(target, cells)
       and start != target

// Law that reads world state
law world_invariant(authority: Authority) -> Bool:
    return signal_in_bounds(authority.signal)
```

### What the Compiler Enforces

| Rule | Enforced | What Happens If You Break It |
|------|----------|------------------------------|
| Return type must be `Bool` | ✅ Typechecker | Compile error: `law 'foo' must return Bool, found Int` |
| Parameters can be any type | ✅ Parser | Accepter |
| Body can use world state | ✅ `in_patch = true` | Allowed ->> world reads/writes work in law bodies |
| Effects are tracked, not restricted | ✅ Typechecker | Your law can have IO, but shouldn't |
| Generics are not supported | => | `law_function_view` hardcodes `generics: vec![]` |
| Visibility (pub, private) | ✅ Parser | Works as expected |
| Attributes stable | ✅ Parser | Works |

______________________________________________________________________

## How It Works ->> Full Pipeline

### Source to Runtime Flow

```
.kn law declaration
  → parser.rs: parse_law()
    → Item::Law(LawDef { name, params, return_type, body })
  → types.rs: check_law()
    → Sets env.in_patch = true (world-aware context)
    → Calls check_function on law_function_view(law)
    → Resolves return type, enforces Bool
    → TypedLaw { ast, resolved_type, effects }
  → runtime_contract.rs: collect_law_contracts()
    → RuntimeLawContract { name, symbol, param_types, return_type }
  → realtime_app_bundle.rs: collect_law_bindings()
    → RealtimeLawBinding (same shape)
  → codegen_llvm/mod.rs: compile_law()
    → Delegates to compile_named_callable() (same as fn/patch)
    → Emits normal LLVM IR :: no special instrumentation
  → runtime.rs: register_law_value()
    → Registers in interpreter law table
```

### At Compile Time

The typechecker wraps law into a function view (`law_function_view`) and runs it through the same `check_function` path used for `fn`. Then it enforces the Bool return. There's no special tracking for mutation paths, undo modes, or epoch counters ~> unlike `check_patch`, `check_law` is minimal.

The key detail: **`env.in_patch = true`** is set during law typechecking. This means law bodies compile in the same context as patch bodies --> they can read world state directly.

### At Runtime

**Native (LLVM):** Law compiles to a plain function call. Calling `signal_in_bounds(value)` is the same cost as calling any `fn`. There's no law registry, no C-level dispatch, no instrumentation.

**Interpreter:** Law values are registered in `runtime.laws: HashMap<String, LawDef>` and looked up by name when called. `Value::Law(name)` is a variant in the value enum.

______________________________________________________________________

## The Law Patterns - How to Use It

### Pattern 1: Bounds Guard (The Canonical Pattern)

The simplest use: guard a mutation or computation with a range check.

```kn
law signal_in_bounds(value: Int) -> Bool:
    return value >= 0 and value < 1000000007

fn guarded_set(authority: Authority, value: Int) -> Int:
    if law_status(signal_in_bounds(value)) < 0:
        return -1            // law violation 〰 reject
    authority.signal = value  // safe to write
    return 0
```

**Why law not `if`:** The law is visible in the runtime contract. Any tool can inspect this program and know "signal must be in \[0, MODULUS)". Orchestrate stages can use it. The `law_status` return follows the standard status vocabulary (0 = valid, -1 = invalid).

### Pattern 2: Domain Invariant (The Rat Experiment)

From the convergence rat experiment in `blades/experiments/convergence/src/` ___ **9 laws that validate the simulation's domain model**:

```kn
law rat_cell_in_bounds(index: Int, cell_count: Int) -> Bool:
    return index >= 0 and index < cell_count

law rat_coordinate_in_bounds(x: Int, y: Int, width: Int, height: Int) -> Bool:
    return x >= 0 and y >= 0 and x < width and y < height

law rat_start_target_distinct(start_index: Int, target_index: Int, cell_count: Int) -> Bool:
    return rat_cell_in_bounds(start_index, cell_count)
        and rat_cell_in_bounds(target_index, cell_count)
        and start_index != target_index

law rat_maze_geometry_valid(width: Int, height: Int) -> Bool:
    return width >= 4 and height >= 4

law rat_heat_visible(heat: Int) -> Bool:
    return heat >= 0 and heat < 256
```

These laws describe the world model -- valid maze dimensions, distinct start/target, heat in display range => not just parameter bounds. The main function calls a `rat_law_lane()` at startup to verify all laws are satisfiable before the simulation begins.

**Why law not comments:** These 9 laws are machine-checkable documentation of the simulation's domain constraints. They're in the runtime contract. They can be queried by the orchestrate pipeline. They're not comments that rot 〰 they're runnable predicates.

### Pattern 3: Orchestrate Pipeline Guard (The Pipeline Pattern)

From the fusion chain and 24-tet effects engine === laws as pipeline validation stages:

```kn
orchestrate fx_process_note(slot: Int, velocity: Int, frame: Int, world: FxWorld) -> Int:
    stage lfo1_stage: cpu fx_lfo_sin_scalar(world.phase, world.depth)
    stage law_check: law fx_mix_in_bounds(world.drive)
    stage world_score: world ((slot * 17) + (velocity * 31) + chorus_stage)
        deps [chorus_stage]
        requires law_check
        residency shared policy telemetry_prefer_cpu
    stage patch_stage: patch fx_set_whole_state(world, frame + slot + velocity)
        deps [world_score]
        requires law_check
        residency host policy telemetry_balance_latency
```

**Key pattern:** A single `law_check` stage is declared once and used as a `requires` dependency for **two downstream stages**. The law evaluates `world.drive` once. If it's in bounds, both `world_score` and `patch_stage` proceed. If not, the pipeline degrades or aborts.

**Why law not inline check:** The law becomes a node in the orchestrate DAG. The runtime knows about it, can schedule it, can report its status, and can incorporate it into the pipeline's dependency graph.

### Pattern 4: Checksum-Folding (The Invisible Guard)

From the semantic crucible corpus --- fold `law_status` directly into a checksum so violations are observable without branching:

```kn
let law_status: Int = law_status(signal_in_bounds(patched))
let next_cell: Int = (reply + local_score + law_status) % MODULUS
```

If the law passes, `law_status` returns `0` → contributes nothing to the checksum. If it fails, `-1` shifts the hash. **This proves the law was evaluated** without an explicit `if` check or early return.

This pattern appears in **13 variants** across the semantic corpus. It's the most elegant standalone law usage >> no control flow, just arithmetic.

**Why law not an assert:** This silently folds the inviolation into the program's observable output. A user or test can compare the expected checksum against the actual and detect law violations without any explicit error handling.

### Pattern 5: Pre-Patch Guard (The Law + Patch Trinity)

Law guards what gets patched, patch triggers entangle propagation:

```kn
law signal_in_bounds(value: Int) -> Bool:
    return value >= 0 and value < MODULUS

patch commit_signal(authority: Authority, value: Int) -> Int:
    if law_status(signal_in_bounds(value)) < 0:
        return -1                // law guards the mutation
    authority.signal = value     // patch writes → triggers entangle
    authority.epoch += 1
    return 0
```

The chain: **law guards → patch writes → entangle propagates**. The mirror never sees invalid data because the law checked it before the patch committed.

**Why law not inline `if`:** The law is a named, contract-visible invariant. The same law can be reused across multiple patches, functions, and orchestrate stages. A single change to the law updates all guards.

______________________________________________________________________

## Under-Utilized Ideas >> Creative Semantic Abuse

### 1. Law as a Z3-Feedable Constraint

Law returns `Bool` at runtime, but the same constraint could feed Z3 for compile-time verification:

```kn
// Current: runtime-only invariant
law signal_in_bounds(value: Int) -> Bool:
    return value >= 0 and value < MODULUS

// Conceptual bridge (not yet implemented):
// use std::z3
// fn prove_signal_in_bounds() -> ProofOutcome:
//     let s = z3_solver()
//     let v = z3_int("v")
//     // Prove: no value in [0, MODULUS) satisfies NOT(signal_in_bounds)
//     // i.e., the law implementation is correct
```

A law that is also Z3-provable would be the highest expression of Kain's semantic stack <--> a compile-time verified invariant, not just a runtime predicate.

### 2. Law as an Entangle Pre-Propagation Filter

Entangle currently propagates unconditionally. A law could pre-validate before propagation:

```kn
patch commit_signal(authority: Authority, value: Int) -> Int:
    if law_status(signal_in_bounds(value)) == false:
        return -1
    authority.signal = value    // author writes → entangle propagates to mirror
    return 0                    // mirror never sees invalid data
```

The law is the gatekeeper. The entangle is the highway. Without the law, bad data travels the highway.

### 3. Law as a Multi-Field Relational Invariant

Current patterns check single scalars. The rat experiment comes closest with `rat_start_target_distinct` but there's room for multi-world invariants:

```kn
// Budget constraint: spending can't exceed balance across accounts
law budget_invariant(spent: Int, balance: Int, total_supply: Int) -> Bool:
    return spent >= 0 and spent <= balance and balance <= total_supply

// Temporal ordering: end can't precede start
law time_invariant(start_tick: Int, end_tick: Int) -> Bool:
    return end_tick >= start_tick

// Referential integrity: every reference is valid
law entity_refs_valid(entity_count: Int, refs: ptr<Int>, ref_count: Int) -> Bool:
    return observe refs:
        var i: Int = 0
        while i < ref_count:
            let ref = mem_load(ptr_offset(refs, i, "Int"), "Int")
            if ref < 0 or ref >= entity_count:
                break iter false
            i = i + 1
        continue iter true
```

### 4. Law as a Resonate Handler Guard

A resonate handler fires on state change. Law can validate the new state before it's used downstream:

```kn
resonate Authority.signal validate_signal 4ms:
    let new_signal = resonate_new_i64
    if law_status(signal_in_bounds(new_signal)) == false:
        return               // reject ->> don't propagate
    Mirror.validated_signal = new_signal
```

### 5. Law + Telemetry as Causal Proof

Law is the only semantic construct in the fusion chain without a dedicated telemetry counter. You can prove law was evaluated by folding `law_status` into an existing counter (the checksum-folding pattern) or by checking `law_status` before and after a critical section:

```kn
let law_before = law_status(signal_in_bounds(current_value))
// ... run causal chain that should include a law check ...
let law_after = law_status(signal_in_bounds(new_value))
if law_before < 0 or law_after < 0:
    return -15    // law violation detected
```

This works today with zero runtime changes ->> the law function call IS the telemetry point, and the status check IS the proof.

### 6. Law as a Capability Gate (if capability emitted)

Right now, law emits no capability in the runtime contract. If it did (e.g., `state.invariant`), you could gate orchestrate stages on law capability:

```kn
// Conceptual:
stage verify: law cell_in_bounds(value)
    when capability("state.invariant")
    residency host policy static
```

### 7. Law as a Design-by-Contract Precondition

Currently `requires` only works inside `orchestrate`. A standalone `requires` annotation on `fn` or `patch` would make law a general-purpose pre/post-condition system:

```kn
// Conceptual (not implemented):
fn set_signal(authority: Authority, value: Int) -> Int
    requires signal_in_bounds(value):
    authority.signal = value
    return 0
```

This would enlist the compiler to verify that `signal_in_bounds` is always called (and passes) before `set_signal` executes.

______________________________________________________________________

## Combining with Other Keywords

### Law + World

Laws can read world state directly:

```kn
world FxWorld:
    state drive: Int = 0
    state phase: Int = 0
    surface native_ui => FxPanel

law mix_in_bounds(drive: Int) -> Bool:
    return drive >= 0 and drive < 1000000007
```

### Law + Patch

Laws guard patch mutations:

```kn
law signal_in_bounds(value: Int) -> Bool:
    return value >= 0 and value < MODULUS

patch commit_signal(authority: Authority, value: Int) -> Int:
    if law_status(signal_in_bounds(value)) < 0:
        return -1
    authority.signal = value
    return 0
```

### Law + Orchestrate

Law is a first-class orchestrate stage:

```kn
orchestrate pipeline(input: Int) -> Int:
    stage legal: law my_invariant(input)
    stage work: cpu process(input)
        deps [legal]
    stage write: patch commit(...)
        deps [work]
        requires legal
```

The `requires legal` clause makes `write` dependent on the law stage passing.

### Law + Entangle

Law guards what entangle propagates:

```kn
patch guarded_set(authority: Authority, value: Int) -> Int:
    if law_status(signal_in_bounds(value)) < 0:
        return -1
    authority.signal = value    // propagates to Mirror.signal_copy via entangle
    return 0
```

### Law + Converge

Law can influence lane selection (conceptual but valid):

```kn
law data_quality_valid(quality: Bool) -> Bool:
    return quality == true or quality == false   // trivially true

fn process(value: Int, high_quality: Bool) -> Int:
    let lane = converge compute(value, high_quality)
    if law_status(data_quality_valid(high_quality)) < 0:
        return -1
    return lane
```

### Law + Actor

Laws can be called inside actor handlers:

```kn
actor Validator:
    on Verify(reply_to: P, value: Int):
        if law_status(signal_in_bounds(value)) < 0:
            send reply_to.Result(status = -1)
        else:
            send reply_to.Result(status = 0)
```

### Law + Resonate

Laws can validate changes in resonate handlers:

```kn
resonate Authority.signal guard 4ms:
    let new_val = resonate_new_i64
    if law_status(signal_in_bounds(new_val)) == false:
        return      // reject the change
    Mirror.validated = new_val
```

### Law + Pulse

Laws can be called in pulse bodies:

```kn
pulse check_beat every 1s:
    if law_status(signal_in_bounds(Authority.signal)) < 0:
        Authority.error_state = 1
```

______________________________________________________________________

## Stdlib Telemetry API

### The `law_status` / `law_is_valid_status` Pair

All law-related functions live in `std::intent` (`stdlib/intent.kn`):

```kn
// Low-level
pub fn native_law_status(valid: Bool) -> Int:
    if valid:
        return 0
    return -1

pub fn native_law_is_valid_status(status: Int) -> Bool:
    return status == 0

// Public wrappers
pub fn law_status(valid: Bool) -> Int:
    return native_law_status(valid)

pub fn law_is_valid_status(status: Int) -> Bool:
    return native_law_is_valid_status(status)
```

### Status Vocabulary

`0` = valid (law passed)
`-1` = invalid (law failed)

This mirrors the pattern used by `patch_status`, `converge_status`, and `orchestrate_stage_status` in `std::intent`:

| Concept | Status function | Success value |
|---------|----------------|---------------|
| Law invariant | `law_status(valid)` | `0` |
| Patch mutation | `patch_status(changed)` | `1` (applied) |
| Converge lane | `converge_status(spec, fast)` | `0` (matched) |
| Orchestrate stage | `orchestrate_stage_status(status)` | `0` (completed) |

### No Telemetry Counter

Unlike entangle (`entangle_propagation_count`), patch (`patch_journal_count`), resonate (`resonate_fire_count`), and orchestrate (`orchestrate_stage_count`), **law has no `law_evaluation_count()` function**. This is the only semantic construct in the fusion chain without a dedicated telemetry counter.

Workaround: use the checksum-folding pattern to make law evaluation observable.

______________________________________________________________________

## Runtime Contract Metadata

### RuntimeLawContract

```rust
struct RuntimeLawContract {
    name: String,           // "signal_in_bounds"
    symbol: String,         // "signal_in_bounds" (may diverge for mangling)
    param_types: Vec<String>,  // ["Int"]
    return_type: String,       // "Bool" (always)
}
```

Every declared law produces one contract entry, sorted alphabetically by name.

### What IS NOT Emitted

**No capability.** Unlike entangle (which emits `state.entangle`) and pulse (which emits `time.pulse` + `time.hardware-timer`), law does **not** emit any capability in the runtime contract manifest. There is no `state.invariant` or `state.law` capability. Laws are informational metadata only.

```rust
// Only entanglements emit capabilities:
if summary.entanglements > 0 {
    add_capability("state.entangle", "Program declares compiler-owned state entanglement semantics");
}
```

No equivalent exists for laws.

______________________________________________________________________

## LLVM Codegen

Law codegen is trivial :: it's the **same path as `fn`**:

```rust
fn compile_law(&mut self, law: &TypedLaw) -> KainResult<()> {
    self.compile_named_callable(
        &law.ast.name,
        &law.ast.attributes,
        &law.ast.params,
        Some(&law.ast.return_type),
        &law.ast.body,
        &law.resolved_type,
        law.ast.span,
    )
}
```

This emits a standard LLVM function: prologue, local allocation, expression lowering, return. No special instrumentation, no metadata sections, no law-specific intrinsics.

**No two-function fire wrapper** (unlike pulse). **No snapshot function** (unlike pulse). **No slot table** (unlike pulse). Law compiles to a plain `Bool(...) -> i8` function. That's it.

______________________________________________________________________

## Native C Runtime

**There is none.** A `grep -rlw "law"` across every `.h` and `.c` in `runtime/native/` returns **zero results**.

What does NOT exist:

| File | Would Contain | Status |
|------|---------------|--------|
| `law.h` | C declarations for law registry | **Does not exist** |
| `law.c` | C implementation for law dispatch | **Does not exist** |
| `abi_law_*` | Native ABI functions for law | **Does not exist** |
| `stdlib_abi.c` law section | Telemetry counter | **Does not exist** |
| Z3 law proofs | Bounds/correctness proofs | **Do not exist** |

This is the correct design. Law is a lightweight predicate ~> the entire stack is: Kain source → parser → typechecker → `compile_named_callable` → plain LLVM IR → native function. No runtime substrate needed.

______________________________________________________________________

## Z3 Proofs

There are no Z3 proofs for law. Unlike:

| Construct | Proofs |
|-----------|--------|
| Entangle | Bounded propagation, slot collision, token uniqueness |
| Patch | Journal bounds, undo mode transitions |
| Pulse | Missed-beat bounds (`missed >= 0`, `missed <= advanced`) |
| Machine stones | Token collision, ownership ops |
| **Law** | **None** |

Law doesn't need proofs because it has no native runtime registry, no slot table, no counters, no arithmetic with edge cases. It's a function call. The Z3 proof for a law would be in the user's domain ~~ e.g., "prove the law's Boolean logic is equivalent to its mathematical specification" |-> not in the runtime substrate.

______________________________________________________________________

## Validation Ladder

```powershell
# 1. Typecheck <--> catches non-Bool return type, type errors in body
kain check <file.kn> --target llvm

# 2. Compile + run ->> law executes as a normal function
kain run <file.kn> --target llvm

# 3. Test law explicitly === create a test function that calls law_status
#    and returns non-zero on failure
fn test_law() -> Int:
    if law_status(signal_in_bounds(42)) < 0:
        return 1
    if law_status(signal_in_bounds(-1)) >= 0:
        return 2   // should fail
    return 0

# 4. Check runtime contract - laws appear in the contract manifest
kain build <file.kn> --target llvm
# Inspect output runtime_contract.json for law entries

# 5. Orchestrate integration => verify law stage fires in pipeline
kain run <orchestrate_file.kn> --target llvm
# Check orchestrate_stage_status for law stage

# 6. For benchmark proof
python benchmark/run.py --case fusion_chain --languages kain --runs 1 --warmups 0
```

### Smoketest

```kn
// smoketest/src/semantics/law.kn – the canonical test
use std::runtime
use std::intent

law smoke_signal_in_bounds(value: Int) -> Bool:
    return value >= 0 and value < 1000000007

law smoke_health_positive(health: Int) -> Bool:
    return health > 0 and health <= 1000000

pub fn smoke_law_lane() -> Int:
    let signal_status = law_status(smoke_signal_in_bounds(42))
    if signal_status < 0:
        return 1
    let health_status = law_status(smoke_health_positive(500))
    if health_status < 0:
        return 2
    return 0
```

______________________________________________________________________

## Anti-Patterns

### ❌ Using Law as a Regular Function

```kn
// WRONG --> law must return Bool
law compute_hash(value: Int) -> Int:      // compile error
    return ((value * 31) + 7) % MODULUS

// RIGHT ~ use fn for computation
fn compute_hash(value: Int) -> Int:
    return ((value * 31) + 7) % MODULUS
```

### ❌ Law With Side Effects

```kn
// WRONG - law that logs
law log_and_check(value: Int) -> Bool with IO:
    println("checking: " + str(value))     // side effect!
    return value >= 0

// RIGHT – keep law as a pure predicate
law value_non_negative(value: Int) -> Bool:
    return value >= 0
```

### ❌ Law That Mutates World State

```kn
// WRONG ->> law must not mutate
law check_and_bump(authority: Authority, value: Int) -> Bool:
    authority.epoch = authority.epoch + 1    // mutation inside a law!
    return value >= 0 and value < MODULUS

// RIGHT ~ keep laws read-only
law value_in_bounds(authority: Authority) -> Bool:
    return authority.signal >= 0 and authority.signal < MODULUS
```

### ❌ Declaring a Law and Never Calling It

```kn
// WRONG -- law declared but never used
law signal_in_bounds(value: Int) -> Bool:
    return value >= 0 and value < MODULUS

fn set_signal(authority: Authority, value: Int) -> Int:
    if value < 0 or value >= MODULUS:    // inline check – law not used
        return -1
    authority.signal = value
    return 0

// RIGHT – use the law
fn set_signal(authority: Authority, value: Int) -> Int:
    if law_status(signal_in_bounds(value)) < 0:
        return -1
    authority.signal = value
    return 0
```

### ❌ Inline Bounds Check Instead of Law

```kn
// WRONG ~ invisible invariant
fn set_signal(authority: Authority, value: Int) -> Int:
    if value < 0 or value >= MODULUS:
        return -1
    authority.signal = value
    return 0

// RIGHT ->> compiler-owned invariant
law signal_in_bounds(value: Int) -> Bool:
    return value >= 0 and value < MODULUS

fn set_signal(authority: Authority, value: Int) -> Int:
    if law_status(signal_in_bounds(value)) < 0:
        return -1
    authority.signal = value
    return 0
```

The inline check works but is invisible to the runtime contract, orchestrate stages, and any tooling that inspects program invariants. The law is a named, contract-visible, pipeline-compatible invariant.

### ❌ Raw Bool Check Instead of `law_status`

```kn
// WRONG ->> raw Bool breaks the standard vocabulary
if signal_in_bounds(value) == false:
    return -1

// RIGHT ~ use law_status for consistency
if law_status(signal_in_bounds(value)) < 0:
    return -1
```

The `law_status()` wrapper isn't required for correctness (the Bool check works fine) but using it keeps the code consistent with the rest of the `std::intent` status vocabulary (`patch_status`, `converge_status`, `orchestrate_stage_status`).

### ❕ Not Proving Law Was Evaluated

```kn
// If you never check law_status, you have no proof the law ran.
// The compiler may even optimize it away if the result is unused.

// RIGHT ... prove the law ran:
let _proof = law_status(signal_in_bounds(value))   // silence "unused result" warnings
```

The checksum-folding pattern is the most elegant way to prove evaluation without branching:

```kn
let checksum = (a + b + law_status(signal_in_bounds(value))) % MODULUS
```

______________________________________________________________________

## Quick Reference

| Aspect | Detail |
|--------|--------|
| **Syntax** | `law <name>(<params>) -> Bool: <body>` |
| **Return type** | Must be `Bool` (compile-time enforced) |
| **`in_patch` context** | `true` --> law bodies CAN read world state |
| **Effect permissions** | All effects allowed (IO, Async, GPU, Unsafe --- but shouldn't use them) |
| **Generics** | Not supported |
| **Interpreter** | Full support (`Value::Law`, `HashMap` lookup) |
| **LLVM codegen** | `compile_named_callable()` ~> same as `fn`, plain function IR |
| **Runtime contract** | `RuntimeLawContract { name, symbol, param_types, return_type }` |
| **Native C runtime** | **Zero files** ⁓ no `law.h`, `law.c`, or `abi_law_*` functions |
| **Capability emitted** | None |
| **Z3 proofs** | None |
| **Stdlib module** | `std::intent` |
| **Telemetry functions** | `law_status(Bool) -> Int`, `law_is_valid_status(Int) -> Bool` |
| **Telemetry counter** | None ... no `law_evaluation_count()` exists |
| **Orchestrate stage** | `stage <name>: law <fn_name>(args)` ⁓ kind `OrchestrateStageKind::Law` |
| **Orchestrate guard** | `requires <stage_name>` --> makes a stage depend on the law passing |
| **Checksum-folding** | `let checksum = (a + b + law_status(law_call())) % MOD` ~> elegant evaluation proof |
| **Canonical example** | `smoketest/src/semantics/law.kn` |
| **Orchestrate example** | `smoketest/src/semantics/orchestrate.kn` |
| **Fusion example** | `benchmark/cases_v2/fusion_chain.kn` |
| **Domain-model example** | `blades/experiments/convergence/src/*.kn` ->> 9 laws for maze geometry |
| **Checksum-folding example** | `crates/semantic/symbol_corpus/main.kn` (13 variants) |

______________________________________________________________________

## Source Anchors

### Core Crate

| File | Content |
|------|---------|
| `crates/core/src/ast.rs` | `Item::Law`, `LawDef` struct |
| `crates/core/src/parser.rs` | `parse_law()` --> contextual keyword, params, return type, block |
| `crates/core/src/types.rs` | `check_law()` ‒ `in_patch=true`, function view, Bool enforcement |
| `crates/core/src/runtime_contract.rs` | `RuntimeLawContract`, `collect_law_contracts()` |
| `crates/core/src/realtime_app_bundle.rs` | `RealtimeLawBinding`, `collect_law_bindings()` |
| `crates/core/src/runtime.rs` | `register_law_value()`, interpreter dispatch |

### LLVM Codegen

| File | Content |
|------|---------|
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | `compile_law()` → `compile_named_callable()` |

### Orchestrate

| File | Content |
|------|---------|
| `crates/orchestrate/src/stage.rs` | `OrchestrateStageKind::Law` |

### Stdlib

| File | Content |
|------|---------|
| `stdlib/intent.kn` | `native_law_status`, `law_status`, `native_law_is_valid_status`, `law_is_valid_status` |

### Examples

| File | Usage |
|------|-------|
| `smoketest/src/semantics/law.kn` | Canonical law test: range check + health check + `law_status` |
| `smoketest/src/semantics/orchestrate.kn` | Law stages in orchestrate with `requires` |
| `benchmark/cases_v2/fusion_chain.kn` | Fusion chain: law exists but not telemetry-guarded |
| `crates/semantic/symbol_corpus/main.kn` (13 variants) | Semantic crucible: checksum-folding pattern |
| `blades/experiments/convergence/src/*.kn` | Rat experiment: 9 domain-model laws |
| `blades/python/24_tet/src/resonate_py_effects.kn` | 24-tet effects: law in orchestrate with `requires` |
| `docs/RULEBOOK.md` | "State Integrity" section ___ law + patch + orchestrate |
