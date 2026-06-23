# Patch ->> Journaled Transactional Mutation

**Status:** Research synthesis, 2026-06-08
**Based on:** `crates/core/src/ast.rs`, `crates/core/src/parser.rs`, `crates/core/src/types.rs`, `crates/core/src/runtime_contract.rs`, `crates/core/src/realtime_app_bundle.rs`, `crates/core/src/runtime.rs`, `crates/sys-codegen/src/codegen_llvm/mod.rs`, `runtime/native/include/stdlib_abi.h`, `runtime/native/src/core/stdlib_abi.c`, `stdlib/intent.kn`, `smoketest/src/semantics/patch.kn`, `benchmark/cases_v2/fusion_chain.kn`, `benchmark/cases_v2/keyword_crucible.kn`, `benchmark/cases/pulse_teleport_decay_mesh/main.kn`, `crates/core/tests/compiler_owned_intent_test.rs`, `crates/entangle/src/lib.rs`, GLOSSARY.MD, RULEBOOK.MD, LAW.MD, ENTANGLE.MD

______________________________________________________________________

## Table of Contents

1. [What is Patch?](#what-is-patch)
1. [The Core Idea === Journaled Transactional Mutation](#the-core-idea)
1. [Syntax Reference](#syntax-reference)
1. [How It Works ⁓ Full Pipeline](#how-it-works)
1. [The Patch Patterns \_\_\_ How to Use It](#the-patch-patterns)
1. [Patch + Law](#patch--law)
1. [Journal and Undo Semantics](#journal-and-undo-semantics)
1. [Undo Mode Inference](#undo-mode-inference)
1. [Under-Utilized Ideas => Creative Semantic Abuse](#under-utilized-ideas)
1. [Combining with Other Keywords](#combining-with-other-keywords)
1. [Stdlib Telemetry API](#stdlib-telemetry-api)
1. [Runtime Contract Metadata](#runtime-contract-metadata)
1. [Native ABI Reference](#native-abi-reference)
1. [LLVM Codegen](#llvm-codegen)
1. [Z3 Proofs](#z3-proofs)
1. [Validation Ladder](#validation-ladder)
1. [Anti-Patterns](#anti-patterns)
1. [Quick Reference](#quick-reference)
1. [Source Anchors](#source-anchors)

______________________________________________________________________

## What is Patch?

`patch` is a **top-level declaration** in Kain that provides **journaled transactional mutation** of world state. It is part of **LAYER 2: STATE INTEGRITY** in Kain's decision ladder ... the same layer as `law`. A patch records every old→new value transition in a journal, supports undo via the native ABI, and maintains a journal count that serves as telemetry proof that the mutation occurred.

```kn
patch commit_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value                // journaled: old→new recorded
    authority.epoch = authority.epoch + 1    // journaled: also recorded
    return authority.signal
```

A `patch` is **not** a function with a fancy name. It is a **compiler-owned mutation construct** with transaction semantics. When you write `patch`, you're telling the compiler: "this block mutates world state, and every mutation must be journaled for undo, replay, and telemetry."

Where a normal `fn` is "here's a computation," a `patch` is "here's a unit of state change."

______________________________________________________________________

## The Core Idea ⁓ Journaled Transactional Mutation

### What Patch IS NOT

- ❌ Not a `fn` ⁓ a patch has journaling, undo support, transaction semantics, and a journal count. `fn` has none of these. A `fn` that mutates world state bypasses the journal entirely.
- ❌ Not a `law` -- a law is a pure predicate that returns `Bool`. A patch mutates world state and can return any type. They complement each other: law guards → patch mutates.
- ❌ Not a raw world assignment 〰 writing `World.field = value` outside a patch is allowed but is **not journaled**. There is no undo, no telemetry counter, no replay history.
- ❌ Not a database transaction === patches are in-memory journal entries, not ACID SQL transactions. There is no rollback to arbitrary points, only `undo_last`.
- ❌ Not a concurrent-safe primitive 〰 the native C journal has a proven concurrent slot-overwrite race (see Z3 Proofs). Patch journaling is single-threaded by design.
- ❌ Not a `patch` file format \<--> despite the name, there is no relationship to `.patch` diff files. This is a Kain language construct.

### What Patch IS

- ✅ A **journaled mutation declaration** ⁓ every field write inside a patch records old→new in the journal
- ✅ A **transactional unit** ~> patches have `begin → record → commit` semantics via the native ABI
- ✅ **Undoable** === `abi_patch_undo_last()` reverses the last committed patch by returning the old value
- ✅ **Telemetry-visible** ~> `patch_journal_count()` proves the patch fired, with delta guards in benchmarks
- ✅ **World-aware** --> patch bodies are typechecked with `in_patch = true`, giving full world state access
- ✅ **Auto-inferred undo mode** ~> the typechecker infers `Reversible` or `BestEffort` from the body structure
- ✅ **A runtime contract item** * * * patches appear in `RuntimePatchContract` with mutation paths, undo mode, and collaboration events
- ✅ **An orchestrate stage kind** * * * `stage name: patch fn_name(args)` is a first-class pipeline stage
- ✅ **Entangle-triggering** \_\_\_ writing to a world field in a patch automatically triggers entangle propagation
- ✅ **Capped at 256 journal entries** \<--> a Z3-proven constant in the native runtime

### Where Patch Sits in the Decision Ladder

| Layer | Concept | Keyword |
|-------|---------|---------|
| LAYER 1: STATE AUTHORITY | "Global named state?" | `world` |
| LAYER 2: STATE INTEGRITY | "Journaled mutation?" | **`patch`** |
| LAYER 2: STATE INTEGRITY | "Invariant predicate?" | `law` |
| LAYER 3: CROSS-BOUNDARY | "State mirroring?" | `entangle` |
| LAYER 6: ORCHESTRATION | "Multi-stage pipeline?" | `orchestrate` |

The RULEBOOK says: *"A `patch` is not 'a function that writes to a world.' It is a journaled, undoable, telemetry-instrumented mutation unit."*

### Relationship View

| Construct | Domain-Locked View | Relationship View |
|---|---|---|
| `patch` | "Journaled world mutation" | **Transactional mutation unit** => begin/record/commit semantics, undoable, Z3-proven bounded journal, telemetry counter, auto-inferred undo mode, entangle-triggering, orchestrate-compatible |
| `law` | "Invariant predicate" | **Guard** 〰 patches call `law_status(law())` before mutating, ensuring invariants hold before journaled writes |
| `fn` | "Plain computation" | **No journal** \_\_\_ a `fn` that mutates world state does so without journaling, undo, or telemetry |
| `world` | "Global named state" | **Mutation target** ~~ patches write to world fields, which are the only mutable storage for patches |

______________________________________________________________________

## Syntax Reference

### Canonical Syntax

```kn
patch <name>(<params>) -> <return_type>:
    <body>
```

The return type is **optional** ->> unlike `law` which requires `-> Bool`, patches can return any type or nothing.

```kn
// With return type
patch commit_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value
    authority.epoch = authority.epoch + 1
    return authority.signal

// Without return type (unit-returning)
patch apply_mute(authority: AudioWorld) -> Int:
    authority.gain = 0
    authority.muted = true
    return 1

// Void (implied Unit)
patch noop_check(authority: Authority):
    authority.tick = authority.tick + 1
```

### Concrete Examples

```kn
// Simple single-field mutation
patch commit_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value
    return authority.signal

// Multi-field patch |-> all writes are journaled
patch commit_full_state(authority: Authority, value: Int, health: Int) -> Int:
    authority.signal = value
    authority.epoch = authority.epoch + 1
    authority.health = health
    return authority.epoch

// Patch that calls a law for guard
patch guarded_set(authority: Authority, value: Int) -> Int:
    if law_status(signal_in_bounds(value)) < 0:
        return -1
    authority.signal = value
    authority.epoch = authority.epoch + 1
    return 0

// Patch that reads from entangled mirror
patch sync_from_mirror(authority: Authority, mirror: Mirror) -> Int:
    authority.signal = mirror.signal_copy
    authority.epoch = authority.epoch + 1
    return authority.signal

// Zero-returning patch used in hot loops
patch commit_pulse(authority: PulseAuthority, value: Int, ledger_delta: Int) -> Int:
    authority.signal = value
    authority.epoch = authority.epoch + 1
    authority.ledger = (authority.ledger + ledger_delta + authority.epoch + 11) % MODULUS
    return authority.signal
```

### Grammar (from parser.rs)

```text
patch <name> ( <params> ) [ -> <return_type> ] : <block>
```

The parser (`parse_patch()` at line ~1727 of `parser.rs`):

1. Expects contextual keyword `patch`
1. Parses the name via `parse_ident()`
1. Expects `(`
1. Parses params via `parse_params()`
1. Expects `)`
1. Checks for optional `->` -- if present, parses the return type
1. Expects `:`
1. Parses the body block
1. Returns `Item::Patch(PatchDef { name, params, return_type, body, visibility, attributes, span })`

Key difference from `fn` parsing: the return type is optional. This is the same as `fn` but different from `law` (which requires `-> Bool`).

### What the Compiler Enforces

| Rule | Enforced | What Happens If You Break It |
|------|----------|------------------------------|
| Return type is optional | ✅ Parser | Accepts both `-> Type` and bare block |
| Body can use world state | ✅ `in_patch = true` | Allowed ~ world reads/writes work in patch bodies |
| Generics are not supported | :: | `patch_function_view` hardcodes `generics: vec![]` |
| Undo mode is inferred | ✅ Typechecker | `Reversible` if no best-effort effects, `BestEffort` otherwise |
| Mutation paths are collected | ✅ Typechecker | Every `.field =` assignment in the body is collected |
| Visibility (pub, private) | ✅ Parser | Works as expected |
| Attributes stable | ✅ Parser | Works |
| Effects are tracked | ✅ Typechecker | Patch can declare IO, Async, GPU, Unsafe effects |

______________________________________________________________________

## How It Works ~ Full Pipeline

### Source to Runtime Flow

```
.kn patch declaration
  → parser.rs: parse_patch()
    → PatchDef { name, params, return_type, body, visibility, attributes, span }
  → types.rs: check_patch()
    → Sets env.in_patch = true (world-aware context)
    → Calls check_function on patch_function_view(patch)
    → collect_patch_mutation_paths_from_block(&patch.body)
    → infer_patch_undo_mode() ⁓ checks body for best-effort effects
    → TypedPatch { ast, resolved_type, effects, mutation_paths, undo_mode }
  → runtime_contract.rs: collect_patch_contracts()
    → RuntimePatchContract { name, mutation_paths, replay_log_schema,
        invalidation_keys, collaboration_event, undo_mode }
  → realtime_app_bundle.rs: collect_patch_bindings()
    → RealtimePatchBinding { name, mutation_paths, undo_mode, collaboration_event }
  → codegen_llvm/mod.rs: compile_patch()
    → Sets current_patch_name, delegates to compile_named_callable()
    → Emits LLVM IR with abi_patch_begin/record_i64/commit calls
  → runtime/native: abi_patch_begin() / abi_patch_record_i64() / abi_patch_commit()
    → Records KainNativePatchJournalEntry in bounded array (max 256)
    → Enables abi_patch_undo_last() and abi_patch_journal_count()
```

### At Compile Time (Typechecker)

The typechecker (`types.rs: check_patch` at line ~5750) performs these steps:

1. **Sets `env.in_patch = true`** ~~ this gives the body access to world state that a normal `fn` body would not have
1. **Creates a function view** via `patch_function_view(patch)` ‒ wraps the `PatchDef` as a `Function` (with no generics, optional return type)
1. **Calls `check_function`** – the same typechecker path used for `fn`, `law`, `converge`, and `orchestrate`
1. **Collects mutation paths** >> `collect_patch_mutation_paths_from_block()` walks every statement and expression in the body, finds all field assignments (`expr.field = value`), and records them as `"struct.field"` strings
1. **Infers undo mode** => checks if any statement in the body contains runtime best-effort effects (see [Undo Mode Inference](#undo-mode-inference))

```rust
fn check_patch(env: &mut TypeEnv, patch: &PatchDef) -> KainResult<TypedPatch> {
    let old_patch = env.in_patch;
    env.in_patch = true;
    let res = (|| {
        let typed_fn = check_function(env, &patch_function_view(patch))?;
        let mutation_paths = collect_patch_mutation_paths_from_block(&patch.body);
        let undo_mode = if patch_body_requires_best_effort(&patch.body) {
            PatchUndoMode::BestEffort
        } else {
            PatchUndoMode::Reversible
        };
        Ok(TypedPatch {
            ast: patch.clone(),
            resolved_type: typed_fn.resolved_type,
            effects: typed_fn.effects,
            mutation_paths,
            undo_mode,
        })
    })();
    env.in_patch = old_patch;
    res
}
```

**Key insight:** The typechecker does NOT modify the function body to add journaling calls. Journaling is added at the LLVM codegen level (for native) and in the interpreter runtime. The typechecker's job is to validate types, collect mutation paths, and infer undo mode.

### At Runtime (Interpreter)

The interpreter runtime (`runtime.rs`) tracks patches through several data structures:

```
rf patches: HashMap<String, PatchDef>       :: patch definitions by name
rf patch_undo_modes: HashMap<String, String> - "reversible" | "best_effort"
rf active_patch_frames: Vec<ActivePatchFrame> --- current in-flight transaction
rf patch_records: Vec<PatchRuntimeRecord>     |-> committed patch history
rf patch_replay_catalog: Vec<ReplayablePatchRecord> 〰 replayable records
rf replayable_patch_history: Vec<ReplayablePatchRecord> :: full history
rf undone_patch_records: Vec<ReplayablePatchRecord> ->> undone records
```

**Lifecycle of a patch call in the interpreter:**

1. **`register_patch_value()`** \_\_\_ on program load, the patch is registered in the `patches` HashMap and defined as `Value::Patch(name)` in the environment
1. **Call-time dispatch** \_\_\_ when the patch name is called as a function, the interpreter looks up `Value::Patch(name)`, retrieves the `PatchDef`, and interprets the body
1. **`begin_active_patch()`** --- before body execution, pushes an `ActivePatchFrame` with the patch name, undo mode, and an empty changes list
1. **`record_patch_change()`** -- during body execution, every world field write calls this, recording the path, target, old value, and new value
1. **`finish_active_patch()`** ~~ after body execution, pops the frame, sorts+deduplicates mutation paths, creates a `PatchRuntimeRecord` and a `ReplayablePatchRecord`, pushes them to their respective histories, clears the undone records stack, and records a collaboration event (`"patch.{name}.applied"`)

```rust
fn begin_active_patch(&mut self, name: &str) {
    let undo_mode = self.patch_undo_modes.get(name)
        .cloned()
        .unwrap_or_else(|| "best_effort".to_string());
    self.active_patch_frames.push(ActivePatchFrame {
        name: name.to_string(),
        mutation_paths: Vec::new(),
        undo_mode,
        changes: Vec::new(),
    });
}
```

### At Runtime (Native LLVM)

The LLVM codegen (`codegen_llvm/mod.rs: compile_patch` at line ~15378) wraps the patch function body with:

1. **`current_patch_name` tracking** => stores the patch name so that field writes inside the body can emit `abi_patch_record_i64()` calls
1. **`compile_named_callable()` delegation** => emits the standard LLVM function prologue, local allocation, expression lowering, and return
1. **Journal instrumentation** ‒ the codegen emits calls to:
   - `abi_patch_begin(patch_name)` at function entry
   - `abi_patch_record_i64(patch_name, path, old_value, new_value)` after every field write
   - `abi_patch_commit(patch_name)` at function exit (before return)

The LLVM IR generated looks like:

```llvm
define i64 @patch_commit_signal(...) {
entry:
  %0 = call i64 @abi_patch_begin(i8* @".patch.commit_signal")
  ; ... body code with field writes ...
  store i64 %value, i64* %signal_field_ptr
  %1 = call i64 @abi_patch_record_i64(i8* @".patch.commit_signal",
                                        i8* @".path.authority.signal",
                                        i64 %old_signal, i64 %value)
  ; ... more body ...
  %2 = call i64 @abi_patch_commit(i8* @".patch.commit_signal")
  ret i64 %result
}
```

______________________________________________________________________

## The Patch Patterns ~> How to Use It

### Pattern 1: Simple Mutation (The Canonical Pattern)

The simplest use: write to a world field and return the new value.

```kn
patch commit_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value
    return authority.signal
```

This is the "hello world" of patches. Three things happen:

1. `authority.signal` is assigned `value` (journaled)
1. The patch returns the new signal value
1. The journal records the old→new transition

### Pattern 2: Multi-Field Patch

Patches can mutate multiple world fields in one transaction. All mutations are journaled.

```kn
patch commit_full_state(authority: Authority, value: Int, health: Int) -> Int:
    authority.signal = value
    authority.epoch = authority.epoch + 1
    authority.health = health
    return authority.epoch
```

The mutation paths collected by the typechecker: `["authority.signal", "authority.epoch", "authority.health"]`.

### Pattern 3: Epoch Bump (The Required Pattern)

Every patch that mutates state should bump an epoch counter. This is how the compiler and downstream constructs (resonate, entangle, orchestrate) observe that state changed.

```kn
patch commit_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value
    authority.epoch = authority.epoch + 1    // ALWAYS bump epoch
    return authority.signal
```

**Why epoch matters:**

- Resonate handlers fire on field writes \<--> if you write the same value to `signal` repeatedly, the resonate handler may not fire. An epoch bump guarantees a visible state change.
- Entangle propagation records the propagation count ~ epoch bumps increment the counter.
- Telemetry deltas (`patch_journal_count()`) prove the patch fired.
- Orchestrate stages can depend on epoch increments as pipeline progress signals.

### Pattern 4: Law-Guarded Patch (The Law + Patch Trinity)

A law validates the value before the patch writes it. This is the canonical LAYER 2 pattern.

```kn
law signal_in_bounds(value: Int) -> Bool:
    return value >= 0 and value < MODULUS

patch guarded_set(authority: Authority, value: Int) -> Int:
    if law_status(signal_in_bounds(value)) < 0:
        return -1              // law violation ~> reject
    authority.signal = value   // safe to write
    authority.epoch = authority.epoch + 1
    return 0
```

The chain: **law guards → patch mutates → entangle propagates**. The law prevents bad data from entering the patch, and the patch (via entangle) ensures the mirror never sees invalid state.

### Pattern 5: Patch in a Hot Loop (The Apex Pattern)

From `benchmark/cases/pulse_teleport_decay_mesh/main.kn` * * * a patch called 54000 times in a loop:

```kn
patch commit_pulse(authority: PulseAuthority, value: Int, ledger_delta: Int) -> Int:
    authority.signal = value
    authority.epoch = authority.epoch + 1
    authority.ledger = (authority.ledger + ledger_delta + authority.epoch + 11) % PULSE_MODULUS
    return authority.signal

// In main(): 54000 iterations calling commit_pulse
let committed = commit_pulse(authority, staged, moved.salt + lane)
```

This is the **apex patch usage** ~~ maximum throughput, maximum journal pressure. The native journal has a global capacity limit of 256 entries. When already full (from previous cases), `patch_delta` will be 0, and the benchmark accepts this by verifying `patch_before >= 256`:

```kn
if patch_delta < 1 and patch_before < 256:
    return -13
```

### Pattern 6: Patch as Orchestrate Stage

From `benchmark/cases_v2/keyword_crucible.kn` ~ patch as a stage in an orchestrate pipeline:

```kn
orchestrate crucible_signal_pipeline(value: Int, epoch: Int) -> Int:
    stage host_base: cpu crucible_mix(value + epoch) ...
    stage fast_lane: converge crucible_fast_mix(host_base, epoch) ...
    stage law_check: law crucible_signal_valid(host_base) ...
    stage world_score: world crucible_world_score(...) ...
    stage gpu_tune: gpu crucible_fast_mix(...) ...
    stage patch_step: patch crucible_commit_ack(CrucibleAuthority)
        deps [world_score, gpu_tune]
        requires law_check
        residency host
        policy telemetry_prefer_cpu
        fallback degrade crucible_orch_degrade
    stage final_out: dispatch crucible_dispatch_style(...)
        deps [patch_step, gpu_tune]
```

**Key details:**

- `patch crucible_commit_ack(CrucibleAuthority)` ... the stage kind is `patch`, and the call passes the world as argument
- `deps [world_score, gpu_tune]` ->> patch stage depends on prior computation stages
- `requires law_check` \<--> the law stage must pass before the patch runs
- `residency host` === patches run on the host (they need access to world state)
- `fallback degrade` --- if the patch fails, use a degrade function

### Pattern 7: Reset Patch (Bulk State Reinitialization)

A patch that resets multiple fields to initial values:

```kn
patch crucible_reset(authority: CrucibleAuthority) -> Int:
    authority.signal        = 1
    authority.epoch         = 0
    authority.shadow        = 0
    authority.ack           = 0
    authority.teleport_land = 0
    return 0
```

All five mutations are journaled. The patch serves as a single-call state reset mechanism.

### Pattern 8: Patch Calling Patches (Chained Mutation)

Patches can call other patches, creating a mutation chain:

```kn
commit_signal(authority, 42)    // first patch <--> journals signal write
commit_ack(authority)            // second patch |-> journals ack increment
```

Each patch in the chain has its own journal entry. The total journal count is the sum of all patch invocations.

______________________________________________________________________

## Patch + Law

Patch and law are the two pillars of LAYER 2: STATE INTEGRITY. They are designed to work together.

### The Relationship

| Aspect | `law` | `patch` |
|--------|-------|---------|
| Purpose | Invariant predicate | Journaled mutation |
| Returns | `Bool` (required) | Any type (optional) |
| Mutates | Never | Always (world fields) |
| Journaled | No | Yes => every assignment |
| Undoable | No | Yes ~ `abi_patch_undo_last()` |
| Telemetry | `law_status()` (Bool→Int) | `patch_journal_count()` (total) |
| Orchestrate stage | `law fn(args)` | `patch fn(args)` |
| `in_patch` context | `true` | `true` |
| Z3 proofs | None | 2 proofs (journal capacity, concurrent slot) |

### The Trinity: Law Guards → Patch Mutates → Entangle Propagates

The most important pattern for state integrity:

```kn
// 1. LAW --> defines the invariant
law signal_in_bounds(value: Int) -> Bool:
    return value >= 0 and value < MODULUS

// 2. PATCH ... guards with law, then mutates
patch guarded_set(authority: Authority, value: Int) -> Int:
    if law_status(signal_in_bounds(value)) < 0:
        return -1              // law rejected the value
    authority.signal = value   // mutation is journaled
    authority.epoch = authority.epoch + 1
    return 0

// 3. ENTANGLE => propagates the mutation
entangle Authority.signal <-> Mirror.signal_copy with single_writer
```

**The guarantee:** If the law passes, the patch writes to the world. The write triggers entangle propagation to the mirror. The mirror never sees invalid data because the law checked it before the patch committed.

### Law in Patch Body

Laws can be called from within a patch body. The `in_patch = true` context means both law and patch bodies have the same world-access privileges:

```kn
patch commit(authority: Authority, value: Int) -> Int:
    if law_status(signal_in_bounds(value)) < 0:
        return -1
    if law_status(health_positive(authority.health)) < 0:
        return -2
    authority.signal = value
    authority.epoch = authority.epoch + 1
    return 0
```

### Patch in Law Body

Laws can also call patches, though this is unusual ->> a law that calls a patch is no longer a pure predicate:

```kn
law check_and_reset(authority: Authority) -> Bool:   // conceptually impure
    crucible_reset(authority)
    return true
```

This is allowed by the typechecker but violates the convention that laws should be pure. The compiler does not enforce purity for laws (effect permissions are tracked but not restricted).

______________________________________________________________________

## Journal and Undo Semantics

### The Journal Data Structure

The native C runtime maintains a flat array of journal entries:

```c
#define ABI_PATCH_JOURNAL_MAX 256

typedef struct KainNativePatchJournalEntry {
    char patch_name[128];    // name of the patch ("commit_signal")
    char path[256];          // mutation path ("authority.signal")
    int64_t old_value;       // value before mutation
    int64_t new_value;       // value after mutation
    int active;              // 1 if this entry is active (not yet cleared)
    int committed;           // 1 if the patch was committed
    int undone;              // 1 if this entry was undone
} KainNativePatchJournalEntry;

static KainNativePatchJournalEntry g_kain_native_patch_journal[ABI_PATCH_JOURNAL_MAX];
static int64_t g_kain_native_patch_journal_count = 0;
static char g_kain_native_active_patch[128] = "";
```

**Key characteristics:**

- Maximum 256 entries (Z3-proven capacity)
- Flat array, not a ring buffer – once full, `abi_patch_record_i64()` returns `-3`
- Entries are never removed => `undo` only marks entries as `undone = 1`
- The journal is NOT thread-safe |-> the concurrent slot-overwrite Z3 proof documents a known race

### Transaction Lifecycle

```
begin → record* → commit
```

**1. `abi_patch_begin(patch_name)`** >> copies the patch name into the global active-patch slot. Returns 0.

**2. `abi_patch_record_i64(patch_name, path, old_value, new_value)`** - creates a journal entry:

- Checks `g_kain_native_patch_journal_count < ABI_PATCH_JOURNAL_MAX` → returns `-3` if full
- Selects the entry at `g_kain_native_patch_journal[g_kain_native_patch_journal_count]`
- Copies patch_name, path, old_value, new_value
- Sets `active = 1`, `committed = 0`, `undone = 0`
- Increments `g_kain_native_patch_journal_count`
- Returns 0

**3. `abi_patch_commit(patch_name)`** * * * marks all uncommitted entries for this patch name as committed:

- Iterates through the journal
- Sets `committed = 1` for matching `active && !committed` entries
- Clears the active-patch slot
- Returns the number of entries committed

### Undo

**`abi_patch_undo_last()`** --> finds the most recent committed entry that is not yet undone:

- Scans the journal backward from the end
- Returns the `old_value` of the first matching entry and sets `undone = 1`
- Returns 0 if no undoable entry exists

**Important:** The undo function returns the old value but does NOT write it back to the world. The caller is responsible for actually reverting the state. This is by design ⁓ the undo function provides the data needed for reversion, but the reversion itself must be done by the application (typically by calling the patch with the old value).

```kn
fn revert_last(authority: Authority) -> Int:
    let old_value = abi_patch_undo_last()
    if old_value == 0 and patch_journal_count() > 0:
        return -1   // no undoable entry found
    return commit_signal(authority, old_value)  // re-apply with old value
```

### Journal Count

**`abi_patch_journal_count()`** returns the total number of journal entries recorded (committed + uncommitted, undone + not undone). This is a monotonic counter :: it only increases.

### Last Path

**`abi_patch_last_path()`** returns the mutation path of the last journal entry. Returns empty string if no entries exist.

### Journal Capacity and Telemetry

The journal has a hard limit of 256 entries. When the journal is full:

- New `abi_patch_record_i64()` calls return `-3`
- `patch_journal_count()` still reports 256 (or whatever the current count is)
- Telemetry delta guards must account for this: `if patch_delta < 1 and patch_before < 256: return -13`

This is the standard pattern from the fusion chain:

```kn
let patch_before = patch_journal_count()
// ... run operation that calls patches ...
let patch_delta = patch_journal_count() - patch_before
// Accept 0 only if journal was already full
if patch_delta < 1 and patch_before < 256:
    return -13    // error: patch never fired
```

______________________________________________________________________

## Undo Mode Inference

The typechecker automatically infers a patch's undo mode by scanning its body for "best-effort effects" ⁓ operations that cannot be cleanly reverted.

### The Two Modes

| Mode | Description | When Inferred |
|------|-------------|---------------|
| `Reversible` | The patch can be cleanly undone | Default --> body has no fanout, IO, GPU, or other runtime effects |
| `BestEffort` | The patch may have irreversible side effects | Body contains fanout, IO, GPU dispatch, or other runtime effects |

### How Inference Works

The function `infer_patch_undo_mode()` (at `runtime.rs` line ~7812) calls `block_contains_runtime_best_effort_effects()`, which walks every statement in the patch body:

```rust
fn infer_patch_undo_mode(patch: &PatchDef) -> String {
    if block_contains_runtime_best_effort_effects(&patch.body) {
        "best_effort".to_string()
    } else {
        "reversible".to_string()
    }
}
```

The following statement kinds trigger BestEffort mode:

| Statement | Reason |
|-----------|--------|
| `fanout` | Parallel dispatch with side effects |
| `dispatch` (GPU) | GPU kernel launch * * * cannot revert |
| IO effect in expression | File/network operations cannot be undone |
| Async effect in expression | Async operations may have external side effects |
| GPU effect in expression | GPU memory operations cannot be cleanly reverted |

### Examples

```kn
// Reversible ___ pure field assignments only
patch reversible_patch(authority: Authority, value: Int) -> Int:
    authority.signal = value
    authority.epoch = authority.epoch + 1
    return 0

// BestEffort ‒ contains IO (print side effect)
patch logging_patch(authority: Authority, value: Int) -> Int with IO:
    println("setting signal to: " + str(value))
    authority.signal = value
    authority.epoch = authority.epoch + 1
    return 0

// BestEffort <--> contains fanout (parallel dispatch)
patch parallel_patch(authority: Authority, values: ptr<Int>, count: Int) -> Int:
    fanout i in 0..count:
        mem_store(ptr_offset(values, i, "Int"), authority.signal, "Int")
    authority.epoch = authority.epoch + 1
    return 0
```

### What BestEffort Means for the Runtime

- The patch is still journaled – all field writes are still recorded
- The undo mode is metadata-only in the runtime contract (`undo_mode: "best_effort"`)
- The runtime does NOT prevent undo :: `abi_patch_undo_last()` works on BestEffort patches too
- The mode signals to tooling: "this patch's journal entries may not be sufficient for clean rollback"

### Testing

From `crates/core/tests/compiler_owned_intent_test.rs`:

```rust
#[test]
fn runtime_contract_marks_calling_patches_as_best_effort() {
    // The best_effort_patch_source has a patch that calls next_value()
    let typed = parse_and_typecheck(best_effort_patch_source()).expect("typecheck");
    let bundle = emit_runtime_contract_bundle(&typed, CompileTarget::Rust);
    assert_eq!(bundle.patches[0].undo_mode, "best_effort");
}
```

______________________________________________________________________

## Under-Utilized Ideas --> Creative Semantic Abuse

### 1. Patch as Audit Log

Every patch records old→new values for every field write. This is a built-in audit log for any world state change. You can combine `patch_journal_count()` with `abi_patch_last_path()` to trace the mutation history:

```kn
fn audit_state(authority: Authority) -> Int:
    let count = patch_journal_count()
    let last_path = abi_patch_last_path()
    // ... record count and path for audit trail ...
    return count
```

**Under-explored:** A dedicated audit actor that subscribes to patch collaboration events and builds a structured mutation history for inspection.

### 2. Patch as Time-Travel Debugger

Because patches record old→new transitions, you can build a time-travel debugger that:

1. Captures the journal state at snapshot points
1. Replays patches by calling them again
1. Uses `abi_patch_undo_last()` to step backward through mutations

```kn
fn snapshot_journal() -> Int:
    return patch_journal_count()  // capture current position

fn rollback_to(snapshot: Int, authority: Authority) -> Int:
    while patch_journal_count() > snapshot:
        let old_value = abi_patch_undo_last()
        if old_value == 0:
            return -1
        commit_signal(authority, old_value)
    return 0
```

**Under-explored:** The `replayable_patch_history` and `patch_replay_catalog` in the interpreter runtime already collect `ReplayablePatchRecord` entries with full old→new change data. These are currently unused in user-facing APIs 〰 they're the foundation for a structured replay/rollback system.

### 3. Patch as Collaboration Event Source

Every patch commit produces a collaboration event (`"patch.{name}.applied"`) that is stored in the runtime's `patch_collaboration_events` vector. This is metadata that tools, orchestrators, and debuggers can consume:

```kn
// Runtime produces:
// PatchCollaborationEvent { event_id: "patch.commit_signal.applied",
//                          patch_name: "commit_signal",
//                          mutation_paths: ["authority.signal", "authority.epoch"],
//                          undo_mode: "reversible" }
```

**Under-explored:** Exposing collaboration events via the stdlib so user code can subscribe to patch events and react to them (similar to resonate, but for patch completion rather than field writes).

### 4. Multi-Patch Atomicity Through Orchestrate

Orchestrate stages can sequence patches with dependencies, providing a form of atomic multi-patch workflow:

```kn
orchestrate fund_transfer(from: Account, to: Account, amount: Int) -> Int:
    stage deduct: patch withdraw(from, amount) residency host
    stage credit: patch deposit(to, amount)
        deps [deduct]
    stage verify: law balance_check(from, to)
        deps [credit]
    return verify
```

If the `deduct` patch succeeds but `deposit` fails, the orchestrate runtime can trigger the degrade path, which could call an undo function. **Under-explored:** Automatic rollback of prior patch stages when a downstream stage fails.

### 5. Patch as Cross-World Migration Trigger

When a patch writes to a world that has entangle, the propagation automatically carries the mutation to the mirror world. A single patch can update multiple worlds through the entangle graph:

```kn
patch update_player(player: PlayerWorld, x: Int, y: Int) -> Int:
    player.x = x          // triggers entangle → PlayerMirror.x_copy
    player.y = y          // triggers entangle → PlayerMirror.y_copy
    player.epoch = player.epoch + 1  // triggers entangle → PlayerMirror.epoch_copy
    return 0
```

**Under-explored:** A patch that intentionally writes to multiple entangled fields to trigger cascading updates across a multi-world topology.

### 6. Patch Journal as Proof of Work

The monotonic journal count (`abi_patch_journal_count()`) can serve as a proof-of-work counter. If a benchmark test expects exactly N patch invocations, the journal delta is the proof:

```kn
fn prove_patches_fired(expected: Int) -> Int:
    let count = patch_journal_count()
    if count < expected:
        return -1   // not enough patches fired
    return count
```

This is the standard pattern in all fusion chain benchmarks \_\_\_ every patch-dependent test guards its correctness with `patch_journal_count()` deltas.

______________________________________________________________________

## Combining with Other Keywords

### Patch + World

The foundation. Every patch writes to at least one world. Worlds are the only mutation target for patches.

```kn
world Authority:
    state signal: Int = 1
    state epoch: Int = 0
    surface native_ui => Panel

patch commit_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value
    authority.epoch = authority.epoch + 1
    return authority.signal
```

### Patch + Law

Laws guard patch mutations. See [Patch + Law](#patch--law) above for the full treatment.

```kn
law signal_in_bounds(value: Int) -> Bool:
    return value >= 0 and value < MODULUS

patch guarded_set(authority: Authority, value: Int) -> Int:
    if law_status(signal_in_bounds(value)) < 0:
        return -1
    authority.signal = value
    authority.epoch = authority.epoch + 1
    return 0
```

### Patch + Entangle

Every world field write inside a patch triggers entangle propagation for any entangled fields. The patch is the mutation side of the authority→mirror coupling.

```kn
entangle Authority.signal <-> Mirror.signal_copy with single_writer

patch commit_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value     // triggers entangle to Mirror.signal_copy
    authority.epoch = authority.epoch + 1
    return authority.signal
```

### Patch + Resonate

Writing to a world field in a patch triggers resonate handlers on that field. The patch writes, then resonate fires, then entangle propagates – all in the same mutation chain.

```kn
resonate Authority.signal dampen 4ms:
    Authority.shadow = resonate_new_i64 * 2  // processing after patch writes

patch commit_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value    // triggers resonate handler above
    authority.epoch = authority.epoch + 1
    return authority.signal
```

### Patch + Orchestrate

Patch is a first-class orchestrate stage kind. See [Pattern 6: Patch as Orchestrate Stage](#pattern-6-patch-as-orchestrate-stage) for the full pipeline example.

```kn
orchestrate pipeline(input: Int) -> Int:
    stage check: law my_invariant(input)
    stage commit: patch commit_signal(Authority, input)
        deps [check]
    return commit
```

### Patch + Converge

Converge lanes can select different patches based on capability:

```kn
converge commit_lane(authority: Authority, value: Int) -> Int:
    spec reference:
        return commit_signal(authority, value)
    fast fast_commit when capability("patch.fast"):
        return commit_signal_fast(authority, value)
    verify random(4)
```

### Patch + Actor

Actors can call patches on world state:

```kn
actor StateManager:
    on SetSignal(reply_to: P, value: Int):
        let result = commit_signal(Authority, value)
        send reply_to.Done(status = result)
```

### Patch + Pulse

Patches can be called from pulse bodies to periodically update world state:

```kn
pulse tick_every every 16ms:
    let new_value = (Authority.signal * 17 + 31) % MODULUS
    commit_signal(Authority, new_value)
```

### Patch + Teleport

Patches can write teleported data into world state:

```kn
fn transfer_and_commit() -> Int:
    let shard = teleport ... from Source to Dest via bus
    commit_signal(Authority, shard.bias + shard.salt)
    return 0
```

### Patch + Shatter

Shatter structs can be used as patch parameters or local variables:

```kn
shatter struct PulseShard:
    bias: Int
    phase: Int
    salt: Int
    hot: Bool

patch commit_shard(authority: Authority, shard: PulseShard) -> Int:
    authority.signal = shard.bias + shard.salt
    authority.epoch = authority.epoch + 1
    return authority.signal
```

______________________________________________________________________

## Stdlib Telemetry API

### Low-Level ABI Functions (private @extern)

| Function | Signature | Description |
|----------|-----------|-------------|
| `abi_patch_journal_count` | `() -> Int` | Total journal entries recorded |
| `abi_patch_last_path` | `() -> String` | Mutation path of the last journal entry |

### Public Native Wrappers

| Function | Signature | Description |
|----------|-----------|-------------|
| `native_patch_journal_count()` | `() -> Int` | Returns `abi_patch_journal_count()` |
| `native_patch_last_path()` | `() -> String` | Returns `abi_patch_last_path()` |
| `native_patch_status(changed: Bool)` | `(Bool) -> Int` | Converts Bool to status Int (1 = changed, 0 = no change) |

### Public High-Level API

| Function | Signature | Description |
|----------|-----------|-------------|
| `patch_journal_count()` | `() -> Int` | How many total journal entries have been recorded |
| `patch_last_path()` | `() -> String` | Path of the most recent journal entry |
| `patch_status(changed: Bool)` | `(Bool) -> Int` | `1` if mutation occurred, `0` if no change |
| `patch_is_noop(status: Int)` | `(Int) -> Bool` | Whether the patch status indicates no change |

### Status Vocabulary

The `patch_status()` function follows the same pattern as `law_status()`, `converge_status()`, and `orchestrate_stage_status()`:

| Concept | Status function | Values |
|---------|----------------|--------|
| Law invariant | `law_status(valid)` | `0` = valid, `-1` = invalid |
| Patch mutation | `patch_status(changed)` | `1` = applied, `0` = no change |
| Converge lane | `converge_status(spec, fast)` | `0` = matched, other = mismatch |
| Orchestrate stage | `orchestrate_stage_status(status)` | `0` = completed |

### Usage in Kain

```kn
use std::intent

fn prove_patch_fired(expected_delta: Int) -> Int:
    let before = patch_journal_count()
    // ... run operation that calls patches ...
    let delta = patch_journal_count() - before
    if delta < expected_delta and before < 256:
        return -1
    return 0
```

### Telemetry Delta Guard (from fusion chain)

The fusion chain benchmark establishes this as mandatory for all patch-dependent tests:

```kn
let patch_before = patch_journal_count()
// ... run causal chain ...
let patch_delta = patch_journal_count() - patch_before
// C runtime patch journal has global capacity limit (256).
// If already full, accept 0 with patch_before >= 256.
if patch_delta < 1 and patch_before < 256:
    return -13
```

Error codes from the fusion chain:

- `-10` = resonate never fired
- `-11` = entangle never propagated
- `-12` = teleport never happened
- **`-13` = patch journal overflow / journal never grew**
- `-14` = orchestrate never ran

### What Is NOT Exposed

There is **no `abi_patch_begin()` or `abi_patch_commit()`** in the stdlib surface. These are LLVM-codegen-level intrinsics that the compiler emits automatically when compiling a patch body. User code cannot manually begin/commit patch transactions => only the `patch` keyword creates journaled transactions.

There is **no `abi_patch_undo_last()`** in the stdlib surface either. Undo is available only through the native ABI (`extern fn abi_patch_undo_last()`) if you declare it manually --- but there's no standard wrapper for it in `std::intent`.

______________________________________________________________________

## Runtime Contract Metadata

### RuntimePatchContract

```rust
#[derive(Debug, Clone, Serialize, Deserialize, PartialEq, Eq)]
pub struct RuntimePatchContract {
    pub name: String,                    // "commit_signal"
    pub mutation_paths: Vec<String>,     // ["authority.signal", "authority.epoch"]
    pub replay_log_schema: Vec<String>,  // ["set:authority.signal", "set:authority.epoch"]
    pub invalidation_keys: Vec<String>,  // ["authority"] >> derived from path root segments
    pub collaboration_event: String,     // "patch.commit_signal"
    pub undo_mode: String,               // "reversible" | "best_effort"
}
```

Every declared patch produces one contract entry, sorted alphabetically by name.

### RealtimePatchBinding

```rust
pub struct RealtimePatchBinding {
    pub name: String,                    // "commit_signal"
    pub mutation_paths: Vec<String>,     // ["authority.signal", "authority.epoch"]
    pub undo_mode: String,               // "reversible" | "best_effort"
    pub collaboration_event: String,     // "patch.commit_signal"
}
```

### Invalidation Keys

The `patch_invalidation_keys()` function extracts root-path segments from mutation paths (the part before `.` or `[`):

```rust
fn patch_invalidation_keys(mutation_paths: &[String]) -> Vec<String> {
    let mut keys = mutation_paths
        .iter()
        .map(|path| {
            let cutoff = path.find(|character| character == '.' || character == '[')
                .unwrap_or(path.len());
            path[..cutoff].to_string()
        })
        .filter(|entry| !entry.is_empty())
        .collect::<Vec<_>>();
    keys.sort();
    keys.dedup();
    keys
}
```

For `mutation_paths = ["authority.signal", "authority.epoch"]`, the invalidation keys are `["authority"]`.

### Capabilities Emitted

When any patch exists in the program, the runtime contract emits the `patch.transactions` capability:

```rust
// From runtime_contract.rs:
assert!(bundle.required_capabilities.iter()
    .any(|capability| capability.key == "patch.transactions"));
```

Similarly, the realtime bundle's `tool_caps` includes `"patch.transactions"`.

The `reactivity.shadow-patch` capability is also emitted when patches exist alongside resonate:

```rust
// "reactivity.shadow-patch" |-> emitted when both patch and resonate exist
assert!(bundle.required_capabilities.iter()
    .any(|capability| capability.key == "reactivity.shadow-patch"));
```

______________________________________________________________________

## Native ABI Reference

### Header (`runtime/native/include/stdlib_abi.h`)

The C ABI provides six patch-related functions:

```c
// Lines 99-104 of stdlib_abi.h

int64_t abi_patch_begin(const char* patch_name);
int64_t abi_patch_record_i64(const char* patch_name, const char* path,
                              int64_t old_value, int64_t new_value);
int64_t abi_patch_commit(const char* patch_name);
int64_t abi_patch_undo_last(void);
int64_t abi_patch_journal_count(void);
const char* abi_patch_last_path(void);
```

### Journal Entry Structure

```c
#define ABI_PATCH_JOURNAL_MAX 256

typedef struct KainNativePatchJournalEntry {
    char patch_name[128];      // Name of the declaring patch
    char path[256];            // Dotted path of the mutated field
    int64_t old_value;         // Value before mutation
    int64_t new_value;         // Value after mutation
    int active;                // 1 = in-flight (not committed)
    int committed;             // 1 = committed
    int undone;                // 1 = undone via abi_patch_undo_last()
} KainNativePatchJournalEntry;

static KainNativePatchJournalEntry g_kain_native_patch_journal[ABI_PATCH_JOURNAL_MAX];
static int64_t g_kain_native_patch_journal_count = 0;
static char g_kain_native_active_patch[128] = "";
```

### Function Semantics

#### `abi_patch_begin`

```c
int64_t abi_patch_begin(const char* patch_name) {
    abi_copy_text(g_kain_native_active_patch, sizeof(g_kain_native_active_patch), patch_name);
    return 0;
}
```

Copies the patch name into the global active-patch slot. Always returns 0.

#### `abi_patch_record_i64`

```c
int64_t abi_patch_record_i64(const char* patch_name, const char* path,
                               int64_t old_value, int64_t new_value) {
    KainNativePatchJournalEntry* entry;
    /* Proof: ...native-stdlib-patch-journal-count-stays-within-capacity.yaml */
    if (g_kain_native_patch_journal_count >= ABI_PATCH_JOURNAL_MAX) {
        return -3;
    }
    entry = &g_kain_native_patch_journal[g_kain_native_patch_journal_count];
    memset(entry, 0, sizeof(*entry));
    abi_copy_text(entry->patch_name, sizeof(entry->patch_name), patch_name);
    abi_copy_text(entry->path, sizeof(entry->path), path);
    entry->old_value = old_value;
    entry->new_value = new_value;
    entry->active = 1;
    entry->committed = 0;
    entry->undone = 0;
    g_kain_native_patch_journal_count += 1;
    return 0;
}
```

Creates a new journal entry. Returns `-3` if the journal is full (≥256 entries).

#### `abi_patch_commit`

```c
int64_t abi_patch_commit(const char* patch_name) {
    int64_t committed = 0;
    int64_t index;
    for (index = 0; index < g_kain_native_patch_journal_count; index++) {
        KainNativePatchJournalEntry* entry = &g_kain_native_patch_journal[index];
        if (entry->active && !entry->committed &&
            (patch_name == 0 || strcmp(entry->patch_name, patch_name) == 0)) {
            entry->committed = 1;
            committed += 1;
        }
    }
    g_kain_native_active_patch[0] = '\0';
    return committed;
}
```

Marks all matching uncommitted entries as committed. Clears the active-patch slot. Returns the number of entries committed.

#### `abi_patch_undo_last`

```c
int64_t abi_patch_undo_last(void) {
    int64_t index;
    for (index = g_kain_native_patch_journal_count - 1; index >= 0; index--) {
        KainNativePatchJournalEntry* entry = &g_kain_native_patch_journal[index];
        if (entry->committed && !entry->undone) {
            entry->undone = 1;
            return entry->old_value;
        }
    }
    return 0;
}
```

Finds the most recent committed+un-undone entry, marks it undone, and returns its old_value. Returns 0 if no entry found.

#### `abi_patch_journal_count`

```c
int64_t abi_patch_journal_count(void) {
    return g_kain_native_patch_journal_count;
}
```

Returns the total number of journal entries recorded (monotonic counter).

#### `abi_patch_last_path`

```c
const char* abi_patch_last_path(void) {
    if (g_kain_native_patch_journal_count <= 0) {
        return string_new("");
    }
    return string_new(g_kain_native_patch_journal[g_kain_native_patch_journal_count - 1].path);
}
```

Returns the mutation path of the last journal entry. Returns empty string if journal is empty.

______________________________________________________________________

## LLVM Codegen

### `compile_patch`

Patch codegen is a thin wrapper over `compile_named_callable`:

```rust
fn compile_patch(&mut self, patch: &TypedPatch) -> KainResult<()> {
    let previous_patch = self.current_patch_name.replace(patch.ast.name.clone());
    let result = self.compile_named_callable(
        &patch.ast.name,
        &patch.ast.attributes,
        &patch.ast.params,
        patch.ast.return_type.as_ref(),
        &patch.ast.body,
        &patch.resolved_type,
        patch.ast.span,
    );
    self.current_patch_name = previous_patch;
    result
}
```

**Key detail:** `self.current_patch_name` is set during patch compilation. The codegen uses this to inject `abi_patch_record_i64()` calls after every field write in the patch body. When the codegen compiles a field assignment expression and `current_patch_name` is set, it emits:

```llvm
; Before the store:
; save old value
%old_val = load i64, i64* %field_ptr

; The store:
store i64 %new_val, i64* %field_ptr

; Journal recording:
%status = call i64 @abi_patch_record_i64(
    i8* @".patch.<name>",
    i8* @".<path>",
    i64 %old_val,
    i64 %new_val
)
```

When `current_patch_name` is `None` (outside a patch), field writes emit only the store without the journal call.

### No Two-Function Fire Wrapper

Unlike `pulse` (which emits a fire wrapper and a snapshot function) and `resonate` (which emits a handler symbol with synthesized params), **patch emits exactly one function** -- the patch body itself, with inline journal instrumentation. There is no separate "begin" or "commit" function. The begin/record/commit calls are embedded directly in the patch's LLVM IR.

______________________________________________________________________

## Z3 Proofs

Two Z3 proofs exist for the native patch journal, both in `runtime/native/src/core/z3/proofs/`:

### 1. Journal Count Stays Within Capacity

**File:** `native-stdlib-patch-journal-count-stays-within-capacity.yaml`

**ID:** `kain.native.core.proofs.native_stdlib_patch_journal_count_stays_within_capacity`

**What it proves:** When `abi_patch_record_i64()` records a new journal entry, the journal capacity guard ensures that the count stays at or below `ABI_PATCH_JOURNAL_MAX = 256`.

**Z3 encoding:**

```
journal_count >= 0
journal_count < 256
→ journal_count + 1 <= 256
```

**Source anchor:** `runtime/native/src/core/stdlib_abi.c`, `abi_patch_record_i64`, line ~1294 (original source line 700).

**Tags:** `native-runtime`, `stdlib`, `patch`, `journal`, `capacity`

### 2. Concurrent Slot Overwrite (Known Race)

**File:** `native-stdlib-patch-journal-concurrent-slot-overwrite.yaml`

**ID:** `kain.native.core.proofs.native_stdlib_patch_journal_concurrent_slot_overwrite`

**What it proves:** The patch journal has a known concurrent-safety issue. Two threads calling `abi_patch_record_i64()` concurrently can read the same `g_kain_native_patch_journal_count` before either increments it, causing both records to claim the same slot. The second write overwrites the first. The journal count advances by 1 instead of 2.

**Z3 encoding (SMT-LIB v2):**

```
initial_count >= 0
initial_count < 255
read_a = initial_count
read_b = initial_count  // same! ~ both read the same count
slot_a = read_a
slot_b = read_b         // same! |-> both claim slot `initial_count`
next_a = read_a + 1
next_b = read_b + 1
// final_count is either next_a or next_b --> only one increment
// safety_invariant would require:
//   final_count = initial_count + 2   (but it's only +1)
//   slot_a != slot_b                  (but they're equal)
```

**This is a documented, intentional non-fix.** The patch journal is designed for single-threaded use. Multi-threaded patch calls must be serialized by the orchestrate layer or other external synchronization.

**Source anchor:** `runtime/native/src/core/stdlib_abi.c`, `abi_patch_record_i64`, line ~1294 (original source line 1191).

**Tags:** `native-runtime`, `stdlib`, `patch`, `journal`, `race`, `lost-update`

______________________________________________________________________

## Validation Ladder

```powershell
# 1. Typecheck * * * catches type errors in patch body, infers undo mode
kain check <file.kn> --target llvm

# 2. Compile + run – patch executes with journaling
kain run <file.kn> --target llvm

# 3. Verify journal telemetry ~~ check patch_journal_count() after patch call
kain run <file.kn> --target llvm
# Add a test that checks patch_journal_count() > 0

# 4. Verify undo ... manually call abi_patch_undo_last() and re-apply
kain run <test_undo.kn> --target llvm

# 5. Check runtime contract ~> patch entries in the contract manifest
kain build <file.kn> --target llvm
# Inspect output runtime_contract.json for patches section

# 6. Orchestrate integration ‒ verify patch stage fires as pipeline step
kain run <orchestrate_file.kn> --target llvm
# Check orchestrate_stage_status for patch stage

# 7. For benchmark proof
python benchmark/run.py --case fusion_chain --languages kain --runs 1 --warmups 0
```

### Smoketest

```kn
// smoketest/src/semantics/patch.kn :: the canonical smoke test
use std::runtime
use std::intent
use std::collections
use law::smoke_validate_range
use types::SmokePacket
use types::SmokeLane
use types::smoke_weighted_checksum

world SmokePatchAuthority:
    state signal: Int = 1
    state epoch: Int = 0
    state health: Int = 100
    surface web => SmokePatchPanel

world SmokePatchMirror:
    state signal_copy: Int = 1
    state epoch_copy: Int = 0
    state health_copy: Int = 100
    surface web => SmokePatchPanel

entangle SmokePatchAuthority.signal <-> SmokePatchMirror.signal_copy with single_writer
entangle SmokePatchAuthority.epoch <-> SmokePatchMirror.epoch_copy with single_writer
entangle SmokePatchAuthority.health <-> SmokePatchMirror.health_copy with single_writer

patch smoke_commit_signal(authority: SmokePatchAuthority, value: Int) -> Int:
    authority.signal = value
    authority.epoch = authority.epoch + 1
    authority.health = int_clamp(authority.health + 1, 0, 1000000)
    return authority.signal

pub fn smoke_patch_lane() -> Int:
    let authority = SmokePatchAuthority
    let committed = smoke_commit_signal(authority, 77)

    // Cross-file call: validate committed signal via law.kn's range validator
    if smoke_validate_range(committed, 0, 1000000007) == false:
        return 1
    if patch_journal_count() < 1:
        return 2
    if entangle_propagation_count() < 1:
        return 3

    // Cross-file call: compute weighted checksum via types.kn
    let probe = SmokePacket { id: committed, lane: SmokeLane::Patch,
                              payload: committed + 1, tag: "patch", hot: false }
    let wc = smoke_weighted_checksum(probe)
    if wc < 0:
        return 4

    return 0
```

Key assertions from the smoketest:

1. `patch_journal_count() < 1` → return 2 (patch didn't journal)
1. `entangle_propagation_count() < 1` → return 3 (entangle didn't propagate from the patch write)
1. Cross-file law validation and checksum computation work

### Test Suite

From `crates/core/tests/compiler_owned_intent_test.rs`:

```rust
#[test]
fn runtime_executes_patch_converge_law_and_orchestrate_and_records_patch_transactions() {
    // ... typecheck and interpret ...
    let result = interpret_with_env(&mut env, &typed).expect("interpret");
    assert_eq!(result, Value::Int(48));

    // Verify patch journaling
    assert_eq!(env.patch_records().len(), 1);
    assert_eq!(env.patch_records()[0].name, "set_counter");
    assert_eq!(env.patch_records()[0].undo_mode, "reversible");
    assert_eq!(env.patch_records()[0].mutation_paths, vec!["studio.counter"]);
    assert_eq!(env.patch_records()[0].changes.len(), 1);
    assert_eq!(env.patch_records()[0].collaboration_event, "patch.set_counter.applied");
}
```

______________________________________________________________________

## Anti-Patterns

### ❌ Using `fn` Instead of `patch` to Mutate World State

```kn
// WRONG <--> fn mutates world without journaling
fn set_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value
    authority.epoch = authority.epoch + 1
    return authority.epoch

// RIGHT ⁓ patch journaled world mutation
patch set_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value
    authority.epoch = authority.epoch + 1
    return authority.epoch
```

The `fn` works at runtime but leaves no journal, no undo capability, no telemetry counter, and no runtime contract entry. The `patch` gives you all of these.

### ❌ Patch Without Epoch Bump

```kn
// WRONG --> no epoch, no observable state change
patch set_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value    // same value as before? resonate won't fire
    return authority.signal

// RIGHT --- always bump epoch
patch set_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value
    authority.epoch = authority.epoch + 1   // guarantees observable mutation
    return authority.signal
```

### ❌ Checking Patch Journal Without Accounting for Capacity

```kn
// WRONG ___ assumes journal always grows
fn verify_patch_fired() -> Int:
    let delta = patch_journal_count() - before
    if delta < 1:
        return -1   // falsely reports failure when journal is full at 256

// RIGHT ___ accept full journal as valid
fn verify_patch_fired(before: Int) -> Int:
    let delta = patch_journal_count() - before
    if delta < 1 and before < 256:
        return -13   // only fail if journal had room
    return 0
```

This is the canonical pattern from the fusion chain. Always check `before < 256` alongside `delta < 1`.

### ❌ Undo Without Re-Applying

```kn
// WRONG === calls undo but doesn't actually revert state
fn bad_undo(authority: Authority) -> Int:
    let old = abi_patch_undo_last()   // marks entry as undone
    // ... but never writes old back to authority.signal
    return 0

// RIGHT <--> undo and re-apply
fn good_undo(authority: Authority) -> Int:
    let old = abi_patch_undo_last()
    if old == 0 and patch_journal_count() > 0:
        return -1    // no undoable entry
    return set_signal(authority, old)   // re-apply with old value
```

### ❌ Using Patch for Read-Only Operations

```kn
// WRONG ‒ patch that never mutates (waste of journal)
patch read_only(authority: Authority) -> Int:
    return authority.signal + authority.epoch

// RIGHT ~~ use fn for read-only
fn read_only(authority: Authority) -> Int:
    return authority.signal + authority.epoch
```

A patch without any field writes still incurs begin/commit overhead in the LLVM codegen (though the native runtime handles a no-write patch gracefully ~~ `abi_patch_commit` returns 0 committed entries).

### ❌ Raw Bool Instead of `patch_status`

```kn
// WRONG -- raw Bool breaks the standard vocabulary
if committed == 0:
    return -1

// RIGHT * * * use patch_status for consistency
if patch_is_noop(patch_status(committed > 0)):
    return -1
```

The `patch_status()`/`patch_is_noop()` pair isn't required for correctness (the Bool check works) but keeps the code consistent with the `std::intent` status vocabulary.

### ❌ Ignoring Undo Mode

```kn
// WRONG --> assuming all patches are reversible
fn assume_undo(patch_name: String) -> Int:
    return abi_patch_undo_last()  // might be best-effort!

// RIGHT |-> check undo mode if it matters
fn safe_undo(authority: Authority) -> Int:
    // For best-effort patches, old values may not represent complete state
    let old = abi_patch_undo_last()
    // Verify the undo was meaningful
    if old == 0:
        return -1
    return commit_signal(authority, old)
```

### ❌ Not Checking Telemetry Downstream

```kn
// WRONG |-> writes to world via patch but never verifies downstream effects
fn fire_and_forget(authority: Authority, value: Int) -> Int:
    return set_signal(authority, value)

// RIGHT |-> verify entangle propagated
fn fire_and_verify(authority: Authority, value: Int) -> Int:
    let e_before = entangle_propagation_count()
    let result = set_signal(authority, value)
    if entangle_propagation_count() - e_before < 1:
        return -11   // entangle never propagated
    return result
```

### ❌ Direct World Assignment Instead of Patch

```kn
// WRONG * * * direct mutation bypasses journal
fn set_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value
    return 0

// RIGHT ⁓ patch journals the mutation
patch set_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value
    authority.epoch = authority.epoch + 1
    return 0
```

Direct world assignment is allowed in `fn` bodies (since `in_patch = true` gives all constructs world access), but it should be reserved for initialization or test setup. Production mutation goes through patches.

### ❕ Not Using Patch for Multi-Field Coherence

```kn
// RISKY >> two separate patches for related state
patch set_x(authority: Authority, x: Int) -> Int:
    authority.x = x
    authority.epoch = authority.epoch + 1
    return 0

patch set_y(authority: Authority, y: Int) -> Int:
    authority.y = y
    authority.epoch = authority.epoch + 1
    return 0

// BETTER ~~ single patch for coherent state update
patch set_position(authority: Authority, x: Int, y: Int) -> Int:
    authority.x = x
    authority.y = y
    authority.epoch = authority.epoch + 1
    return 0
```

A single patch with multiple field writes is one journaled transaction. Two separate patches are two independent journal entries |-> if the first succeeds and the second fails, the world is in an inconsistent state.

______________________________________________________________________

## Quick Reference

| Aspect | Detail |
|--------|--------|
| **Syntax** | `patch <name>(<params>) [-> <Type>]: <block>` |
| **Return type** | Optional (unlike `law` which requires `-> Bool`) |
| **`in_patch` context** | `true` === patch bodies CAN read/write world state |
| **Generics** | Not supported |
| **Undo modes** | `Reversible` (default) or `BestEffort` (auto-inferred from body effects) |
| **Mutation paths** | Automatically collected from body field assignments |
| **Interpreter** | Full support: `ActivePatchFrame`, `PatchRuntimeRecord`, `ReplayablePatchRecord`, undo history |
| **LLVM codegen** | `compile_patch()` → `compile_named_callable()` with `current_patch_name` tracking |
| **LLVM instrumentation** | Inline `abi_patch_begin/record_i64/commit` calls per field write |
| **Journal max** | 256 entries (Z3-proven constant) |
| **Journal type** | Flat array, not ring-buffer --- returns `-3` when full |
| **Thread safety** | Not concurrent-safe (documented Z3 race proof) |
| **Runtime contract** | `RuntimePatchContract` with name, mutation_paths, replay_log_schema, invalidation_keys, undo_mode, collaboration_event |
| **Capabilities** | `patch.transactions` - auto-emitted when any patch exists |
| **Z3 proofs** | Journal capacity (`count <= 256`); Concurrent slot overwrite (known race) |
| **Stdlib module** | `std::intent` |
| **Stdlib functions** | `patch_journal_count()`, `patch_last_path()`, `patch_status(Bool) -> Int`, `patch_is_noop(Int) -> Bool` |
| **Native ABI** | `abi_patch_begin`, `abi_patch_record_i64`, `abi_patch_commit`, `abi_patch_undo_last`, `abi_patch_journal_count`, `abi_patch_last_path` |
| **Orchestrate stage** | `stage <name>: patch <fn_name>(args)` --> kind with `deps`, `requires`, `residency`, `fallback` |
| **Entangle trigger** | Every field write in a patch triggers entangle propagation for entangled fields |
| **Error code (fusion chain)** | `-13` ‒ patch journal overflow / journal never grew |
| **Canonical example** | `smoketest/src/semantics/patch.kn` |
| **Fusion example** | `benchmark/cases_v2/fusion_chain.kn` \<--> patches in 7-layer causal chain with telemetry deltas |
| **Crucible example** | `benchmark/cases_v2/keyword_crucible.kn` ... `crucible_semantic_full_checksum` with patch journal telemetry |
| **Apex example** | `benchmark/cases/pulse_teleport_decay_mesh/main.kn` ‒ `commit_pulse` called 54000 times in hot loop |
| **Best-effort test** | `crates/core/tests/compiler_owned_intent_test.rs` \<--> `best_effort_patch_source()` |

______________________________________________________________________

## Source Anchors

### Core Crate ... AST & Parser

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/ast.rs` | 247-255 | `PatchDef` struct: name, params, return_type, body, visibility, attributes, span |
| `crates/core/src/parser.rs` | 1727-1760 | `parse_patch()` ~~ contextual keyword, optional return type, params, block |

### Core Crate |-> Type System

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/types.rs` | 158-161 | `PatchUndoMode` enum: `Reversible`, `BestEffort` |
| `crates/core/src/types.rs` | 164-170 | `TypedPatch` struct: ast, resolved_type, effects, mutation_paths, undo_mode |
| `crates/core/src/types.rs` | 5672-5687 | `patch_function_view()` --> wraps `PatchDef` as `Function` (no generics, optional return type) |
| `crates/core/src/types.rs` | 5750-5765 | `check_patch()` ‒ `in_patch=true`, function view, mutation paths, undo mode inference |
| `crates/core/src/types.rs` | 6292-6340+ | `collect_patch_mutation_paths_from_block/from_stmt/from_expr` 〰 walks body for field assignments |

### Core Crate \_\_\_ Runtime (Interpreter)

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/runtime.rs` | 445-500 | `ActivePatchFrame`, `ActivePatchChange`, `PatchMutationTarget`, `ReplayablePatchRecord`, `PatchRuntimeRecord`, `PatchCollaborationEvent` |
| `crates/core/src/runtime.rs` | 509-535 | `patches`, `patch_undo_modes`, `active_patch_frames`, `patch_records`, `patch_replay_catalog`, `replayable_patch_history`, `undone_patch_records` |
| `crates/core/src/runtime.rs` | 3956-3960 | `register_patch_value()` ... inserts into patches + patch_undo_modes, defines as Value::Patch |
| `crates/core/src/runtime.rs` | 4020-4030 | `begin_active_patch()` ~ pushes ActivePatchFrame |
| `crates/core/src/runtime.rs` | 4032-4037 | `record_patch_change()` \_\_\_ records path, old_value, new_value |
| `crates/core/src/runtime.rs` | 4039-4090+ | `finish_active_patch()` ⁓ sorts paths, creates PatchRuntimeRecord + ReplayablePatchRecord, pushes history |
| `crates/core/src/runtime.rs` | 7812-7840 | `infer_patch_undo_mode()`, `block_contains_runtime_best_effort_effects()` |
| `crates/core/src/runtime.rs` | 8624-8626 | `runtime_patch_record_value()` ~ wraps PatchRuntimeRecord as Value for inspection |

### Core Crate -- Runtime Contract

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/runtime_contract.rs` | 28 | `patches: Vec<RuntimePatchContract>` field in `RuntimeContractBundle` |
| `crates/core/src/runtime_contract.rs` | 127-133 | `RuntimePatchContract` struct: name, mutation_paths, replay_log_schema, invalidation_keys, collaboration_event, undo_mode |
| `crates/core/src/runtime_contract.rs` | 355 | `collect_patch_contracts()` called during bundle construction |
| `crates/core/src/runtime_contract.rs` | 1184-1215 | `collect_patch_contracts()`, `collect_patch_contracts_into()`, `runtime_patch_contract()` |
| `crates/core/src/runtime_contract.rs` | 1395-1410 | `patch_invalidation_keys()` \_\_\_ extracts root path segments from mutation paths |

### Core Crate --> Realtime App Bundle

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/realtime_app_bundle.rs` | 28 | `patches: Vec<RealtimePatchBinding>` field in `RealtimeAppBundle` |
| `crates/core/src/realtime_app_bundle.rs` | 352-356 | `RealtimePatchBinding` struct: name, mutation_paths, undo_mode, collaboration_event |
| `crates/core/src/realtime_app_bundle.rs` | 823-848 | `collect_patch_bindings()`, `collect_patch_bindings_into()`, `realtime_patch_binding()` |

### LLVM Codegen

| File | Lines | Content |
|------|-------|---------|
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 15378-15390 | `compile_patch()` ... sets `current_patch_name`, delegates to `compile_named_callable()` |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 15071-15377+ | `compile_named_callable()` * * * standard function emission (used by patch, fn, law, converge) |

### Native C Runtime

| File | Lines | Content |
|------|-------|---------|
| `runtime/native/include/stdlib_abi.h` | 99-104 | `abi_patch_begin`, `abi_patch_record_i64`, `abi_patch_commit`, `abi_patch_undo_last`, `abi_patch_journal_count`, `abi_patch_last_path` declarations |
| `runtime/native/src/core/stdlib_abi.c` | 1166 | `#define ABI_PATCH_JOURNAL_MAX 256` |
| `runtime/native/src/core/stdlib_abi.c` | 1170-1180 | `KainNativePatchJournalEntry` struct: patch_name, path, old_value, new_value, active, committed, undone |
| `runtime/native/src/core/stdlib_abi.c` | 1178-1179 | `g_kain_native_patch_journal[256]`, `g_kain_native_patch_journal_count` |
| `runtime/native/src/core/stdlib_abi.c` | 1289-1291 | `abi_patch_begin()` --> copies active patch name |
| `runtime/native/src/core/stdlib_abi.c` | 1294-1309 | `abi_patch_record_i64()` ... records entry with capacity guard, returns -3 if full |
| `runtime/native/src/core/stdlib_abi.c` | 1313-1325 | `abi_patch_commit()` --> marks matching entries committed, clears active slot |
| `runtime/native/src/core/stdlib_abi.c` | 1328-1337 | `abi_patch_undo_last()` --> reverse scan for committed+undone, returns old_value |
| `runtime/native/src/core/stdlib_abi.c` | 1340-1342 | `abi_patch_journal_count()` \<--> returns monotonic count |
| `runtime/native/src/core/stdlib_abi.c` | 1344-1348 | `abi_patch_last_path()` === returns last entry's path or empty string |

### Stdlib

| File | Lines | Content |
|------|-------|---------|
| `stdlib/intent.kn` | 23-26 | `@extern fn abi_patch_journal_count() -> Int`, `@extern fn abi_patch_last_path() -> String` |
| `stdlib/intent.kn` | 127-131 | `native_patch_journal_count()`, `native_patch_last_path()` |
| `stdlib/intent.kn` | 216-218 | `native_patch_status(changed: Bool) -> Int` |
| `stdlib/intent.kn` | 362-365 | `patch_status(changed: Bool) -> Int`, `patch_is_noop(status: Int) -> Bool` |

### Z3 Proofs

| File | Content |
|------|---------|
| `runtime/native/src/core/z3/proofs/native-stdlib-patch-journal-count-stays-within-capacity.yaml` | Proves journal count never exceeds 256 |
| `runtime/native/src/core/z3/proofs/native-stdlib-patch-journal-concurrent-slot-overwrite.yaml` | Proves (documents) concurrent slot overwrite race |

### Examples

| File | Usage |
|------|-------|
| `smoketest/src/semantics/patch.kn` | Canonical smoke test: patch + entangle + law + journal count verification |
| `benchmark/cases_v2/fusion_chain.kn` | `fusion_strike_signal`, `fusion_strike_ack`, `fusion_land_teleport`, `fusion_reset_world_state` ... 4 patches in 7-layer causal chain with telemetry deltas |
| `benchmark/cases_v2/keyword_crucible.kn` | `crucible_commit_signal`, `crucible_commit_ack`, `crucible_land_teleport`, `crucible_reset` \_\_\_ patches in semantic full case + orchestrate stage |
| `benchmark/cases/pulse_teleport_decay_mesh/main.kn` | `commit_pulse` ~~ apex hot-loop patch called 54000 times |
| `crates/core/tests/compiler_owned_intent_test.rs` | Test suite: patch typechecking, undo mode inference, runtime recording, contract emission, realtime bundle |
| `library_of_kain/patch.kn` | Minimal patch example |
