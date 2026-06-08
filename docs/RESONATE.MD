# Resonate — Compiler-Owned Reactive Tripwire

**Status:** Research synthesis, 2026-06-08
**Based on:** `crates/resonate/src/lib.rs`, `crates/core/src/parser.rs`, `crates/core/src/ast.rs`, `crates/core/src/types.rs`, `crates/core/src/runtime_contract.rs`, `crates/core/src/runtime.rs`, `crates/core/src/realtime_app_bundle.rs`, `crates/sys-codegen/src/codegen_llvm/mod.rs`, `runtime/native/include/stdlib_abi.h`, `runtime/native/src/core/stdlib_abi.c`, `stdlib/intent.kn`, `smoketest/src/semantics/resonate.kn`, `benchmark/cases_v2/resonate.kn`, `benchmark/cases_v2/fusion_chain.kn`, `benchmark/cases_v2/keyword_crucible.kn`, `blades/python/24_tet/src/resonate_py_effects.kn`, CATALOG.MD, RULEBOOK.MD, lang-semantics SKILL, lang-systems SKILL

---

## Table of Contents

1. [What is Resonate?](#what-is-resonate)
2. [The Core Idea — Compiler-Owned Reactive Tripwire](#the-core-idea)
3. [Syntax Reference](#syntax-reference)
4. [How It Works — Full Pipeline](#how-it-works)
5. [The Body Environment — resonate_old_i64, resonate_new_i64, resonate_fired](#the-body-environment)
6. [Dampening Semantics](#dampening-semantics)
7. [The Self-Feedback Rule](#the-self-feedback-rule)
8. [Effect Model](#effect-model)
9. [Runtime Contract Metadata](#runtime-contract-metadata)
10. [LLVM Codegen — Guarded Post-Store Call](#llvm-codegen)
11. [Native C Runtime — Slot Table, Dampening, Reentry Guard](#native-c-runtime)
12. [Stdlib Telemetry API](#stdlib-telemetry-api)
13. [The Resonate Patterns](#the-resonate-patterns)
14. [Resonate + Pulse](#resonate--pulse)
15. [Under-Utilized Ideas — Creative Semantic Abuse](#under-utilized-ideas)
16. [Combining with Other Keywords](#combining-with-other-keywords)
17. [Native ABI Reference](#native-abi-reference)
18. [Validation Ladder](#validation-ladder)
19. [Z3 Proofs](#z3-proofs)
20. [Source Anchors](#source-anchors)
21. [Anti-Patterns](#anti-patterns)
22. [Quick Reference](#quick-reference)

---

## What is Resonate?

`resonate` is a **top-level declaration** in Kain that establishes a **compiler-owned reactive tripwire with dampening** — a block of code that fires when a watched world state field changes value, with optional dampening to absorb rapid re-fires. It is **not** a timer callback, an event handler, an actor message handler, or a polling loop. It is a **first-class reactive semantic** where the compiler and runtime own the observation graph, the reentry guard, and the dampening window.

```kn
resonate FxWorld.distortion_drive dampen 32ms:
    let new_drive: Int = resonate_new_i64
    if new_drive < 500:
        FxWorld.output_gain = new_drive * 2
```

The key insight: **`resonate` replaces `if old != new { handler() }` polling or hand-rolled observer registries with a compiler-owned tripwire.** The runtime manages the slot table, the dampening clock, the reentry guard, and the old/new value snapshot — all bounded by Z3 for slot capacity and dampen-window correctness. Your code gets `resonate_old_i64`, `resonate_new_i64`, and `resonate_fired` as guaranteed locals bound to the actual mutation event.

Resonate sits in **LAYER 5: TEMPORAL** of Kain's decision ladder, alongside `pulse`. While `pulse` fires on a recurring timed schedule, `resonate` fires reactively when state changes.

---

## The Core Idea

### What Resonate IS NOT

- ❌ Not a timer callback (that's `pulse`)
- ❌ Not an event handler / callback registry (that's hand-rolled ad-hoc code)
- ❌ Not an actor `on` message handler (that's `actor` message dispatch)
- ❌ Not a polling loop (`if old != new { ... }` is manual work)
- ❌ Not a `while` loop that checks state changes (that's a CPU spin)
- ❌ Not a reactive data pipeline (that's `orchestrate`)
- ❌ Not an async/await watcher (no state-machine overhead, no wake/poll cycle)
- ❌ Not a manual observer pattern (no subscribe/unsubscribe, no heap registries)

### What Resonate IS

- ✅ A **compiler-owned reactive tripwire** — the compiler inserts the guard call after every world field store
- ✅ A **dampened debounce mechanism** — `dampen Nms` absorbs rapid re-fires within a time window
- ✅ A **self-feedback-proof design** — the typechecker rejects handlers that directly mutate their own target field
- ✅ A **correct-by-construction body environment** — `resonate_old_i64`, `resonate_new_i64`, `resonate_fired` are guaranteed locals
- ✅ A **Z3-proven bounded slot table** — 128 max resonate bindings
- ✅ A **reentry-guarded native ABI** — `abi_resonate_should_fire_i64()` / `abi_resonate_should_fire_f64()` check slot active_depth before allowing fire
- ✅ A **synchronous fire model** — the handler runs in the same call stack as the store, not deferred to a scheduler
- ✅ **Broad effect permissions** — IO, Async, GPU, Reactive, Unsafe, Alloc, Panic are all allowed
- ✅ **Direct world access** — handlers can read and write any world state (except their own trigger field)
- ✅ **Full telemetry** — fire count, absorb count, mutation count, last target, last old/new values, last dampen_ns

### The Relationship View

| Construct | Domain-Locked View | Relationship View |
|---|---|---|
| `pulse` | "Frame clock / game loop" | **Temporal driver** — recurring beat with Z3-proven arithmetic and jitter tolerance |
| `resonate` | "Reactive tripwire on world state" | **Reactive trigger** — fires on value change with dampening, reentry guard, and self-feedback rejection |
| `actor on` | "Message handler" | **Concurrent message consumer** — actor mailbox dispatch, async, not state-reactive |
| `entangle` | "State mirror" | **Compile-time coupling** — propagates writes to mirror fields, not a handler trigger |
| `patch` | "Journaled mutation" | **Transactional write** — journal records, epoch bumps, no reactive payload |

The key distinction:

| Behavior | resonate | pulse | actor `on` |
|---|---|---|---|
| **Trigger** | State change (value comparison) | Wall-clock time | Incoming message |
| **Scheduling** | Synchronous, in-store | Asynchronous, scheduler thread | Actor mailbox dispatch |
| **Dampening** | ✅ `dampen Nms` (debounce) | ✅ `jitter Nms` (tolerance) | ❌ |
| **Body locals** | `resonate_old_i64`, `resonate_new_i64`, `resonate_fired` | `pulse_tick`, `pulse_dt_ms`, `pulse_missed` | Message arguments |
| **Self-feedback guard** | ✅ Typechecker rejects writes to own target | ✅ N/A (no target field) | ❌ N/A |
| **Reentry guard** | ✅ Native ABI `active_depth` check | ✅ N/A (single-threaded fire) | ❌ N/A |
| **Effect permissions** | All (IO, Async, GPU, Reactive, Unsafe, Alloc, Panic) | All | Actor-scoped |
| **Telemetry** | `resonate_fire_count`, `resonate_absorb_count` | `runtime_machine_pulse_total_fire_count()` | Actor-specific |

---

## Syntax Reference

### Canonical Syntax

```kn
resonate <World.field> [dampen <duration>]:
    <body block>
```

Where:
- `<World.field>` is a dotted path to a world state field (e.g., `FxWorld.distortion_drive`)
- `dampen` is an optional contextual keyword
- `<duration>` is `Integer Unit` (e.g., `0ms`, `32ms`, `1s`)
- `<body block>` is a standard block with `resonate_old_i64`, `resonate_new_i64`, `resonate_fired` locals

### Concrete Examples

```kn
// Simple fire-on-change — no dampening
resonate Authority.signal dampen 0 ms:
    Authority.last_old = resonate_old_i64
    Authority.last_new = resonate_new_i64

// Dampened — fires at most once per 16ms even with rapid writes
resonate FxWorld.distortion_drive dampen 32ms:
    let new_drive: Int = resonate_new_i64
    if new_drive < 500:
        FxWorld.output_gain = new_drive * 2

// Second window — fires at most once per second
resonate FxWorld.lfo1_rate dampen 16ms:
    let new_rate: Int = resonate_new_i64
    let sync_rate: Int = new_rate * 2
    if sync_rate <= 500:
        FxWorld.tremolo_rate = sync_rate

// No dampening — every write fires (use for critical state)
resonate SafetyWorld.heartbeat dampen 0 ms:
    SafetyWorld.last_heartbeat = resonate_new_i64
```

### Grammar (from parser.rs)

```text
resonate <endpoint> [dampen <duration>]: <block>

endpoint = ident ( "." ident )+     // at least 2 segments
duration = integer unit              // unit in ns, us, ms, s, tick, ticks
```

The parser flow:
1. Expects contextual keyword `resonate`
2. Calls `parse_resonate_endpoint()` — parses a dot-separated ident chain with minimum 2 segments
3. If `dampen` contextual keyword follows, calls `parse_pulse_duration()` to parse the dampen window
4. Expects colon
5. Parses block
6. Generates a synthetic name `resonate__{target}__{segments}` from the target path
7. Returns `Item::Resonate(ResonateDef { name, target, dampen, body, ... })`

```rust
// ast.rs
pub struct ResonateDef {
    pub name: String,                     // auto-generated: "resonate__World__field"
    pub target: ResonateEndpoint,         // dotted path to world field
    pub dampen: Option<PulseDuration>,    // optional dampen window
    pub body: Block,
    pub visibility: Visibility,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

pub struct ResonateEndpoint {
    pub segments: Vec<String>,            // ["World", "field"]
    pub span: Span,
}
```

### Endpoint Requirements

Resonate endpoints must be **dotted paths with at least 2 segments**. The parser explicitly requires this:

```
Resonate target must be a stable dotted lvalue path like World.state
```

This is because resonate only makes sense as a tripwire on world fields. A bare identifier or single-segment path cannot participate in a resonate.

---

## How It Works — Full Pipeline

### Source to Runtime Flow

```
.kn resonate declaration
  → parser.rs: parse_resonate()
    → ResonateDef { name, target, dampen, body } in AST
  → types.rs: check_resonate()
    → Resolves endpoint type, validates dampen, checks self-feedback rule
    → Creates scope with resonate_old_i64, resonate_new_i64, resonate_fired
    → ResonancePlan { target, dampen, direct_mutation_paths }
    → TypedResonate { ast, plan, target_type, target_type_name }
  → runtime_contract.rs: collect_resonance_contracts()
    → RuntimeResonanceContract { name, target, target_type, dampen, dampen_ns, handler_symbol, direct_mutation_paths }
  → runtime.rs: register -> ensures state.resonate capability
  → codegen_llvm/mod.rs: collect_native_resonances()
    → NativeResonanceInfo { target, dampen_ns, handler_symbol }
  → codegen_llvm/mod.rs: compile_resonate()
    → Emits __kain_resonate_{name}(old_i64, new_i64, fired_bool) handler function
  → codegen_llvm/mod.rs: world field store lowering
    → After each world field store, looks up native_resonance_binding(path)
    → If binding found, calls emit_resonance_after_store()
    → Emits: abi_resonate_should_fire_i64() guard -> branch -> handler call -> abi_resonate_exit()
  → runtime/native: stdlib_abi.c
    → abi_resonate_should_fire_common() — slot lookup, dampening check, reentry guard, fire/absorb counters
    → abi_resonate_exit() — decrements active_depth on the slot
```

### At Compile Time

The typechecker (`types.rs: check_resonate`) performs:

1. **Endpoint resolution** — resolves target field via `resolve_resonate_endpoint_type()`, must be a world field
2. **Self-feedback detection** — collects all patch mutation paths in the body, checks if any directly mutate the target field
3. **Dampen validation** — validates dampen value >= 0, valid unit (ns/us/ms/s/tick/ticks)
4. **Body typechecking** — body is compiled as a unit-returning block with broad effects
5. **Local injection** — `resonate_old_i64`, `resonate_new_i64`, `resonate_fired` are defined in the body scope

```rust
fn check_resonate(env: &mut TypeEnv, resonate: &ResonateDef) -> KainResult<TypedResonate> {
    let target_type = resolve_resonate_endpoint_type(env, &resonate.target)?;
    let target_type_name = describe_type(&target_type);
    let direct_mutation_paths = collect_patch_mutation_paths_from_block(&resonate.body);
    let target = ResonanceTarget::new(resonate.target.segments.clone())?;
    let dampen = match &resonate.dampen {
        Some(dampen) => DampenWindow::new(dampen.value, &dampen.unit)?,
        None => DampenWindow::none(),
    };
    let plan = ResonancePlan::new(resonate.name.clone(), target, dampen, direct_mutation_paths);

    if plan.directly_mutates_target() {
        return Err(env.type_error(
            "resonate '{}' directly mutates its own target '{}'; \
             route feedback through a different patch/world path or dampened orchestrate stage",
            resonate.name, resonate.target.authored_path()
        ));
    }

    // Body typechecked with resonate_old_i64, resonate_new_i64, resonate_fired in scope
    // All effects allowed: IO, Async, GPU, Reactive, Unsafe, Alloc, Panic
    // ...
}
```

**The self-feedback rejection is critical.** The `ResonancePlan::directly_mutates_target()` method checks whether any path in `direct_mutation_paths` equals or starts with the target path. If a handler tries to write to its own trigger field, the compiler rejects it at typecheck time.

### At Runtime (Interpreter)

The interpreter runtime (`runtime.rs`) handles resonate as a **capability-only registration** — the handler body never executes in interpreter mode:

```rust
Item::Resonate(_) | TypedItem::Resonate(_) => {
    self.ensure_active_capability(kain_resonate::RESONATE_CAPABILITY_KEY.to_string());
}
```

This is intentional. Resonate requires the native ABI slot table that only exists in the LLVM-compiled runtime. Like `pulse`, resonate is a **native-only semantic** in practice — the body executes after the world field store, not in interpretation.

### At Runtime (Native LLVM)

The LLVM codegen handles resonate through a **guarded post-store pattern**:

1. **World field store detection** — When the codegen compiles a store to a world struct field, it calls `native_resonance_binding(field_path)` to check if a resonate is registered on that field
2. **Old value capture** — If a resonate binding exists, the old value is captured before the store (for `i64` fields this comes from a load of the existing field value)
3. **Store** — The new value is written to the world field
4. **Guard + handler call** — `emit_resonance_after_store()` emits the should_fire check, conditional branch, handler call, and exit call

The LLVM IR emitted looks like:

```llvm
; 1. Capture old value before store
%old_val = load i64, i64* @Authority_signal_ptr

; 2. Store new value
store i64 %new_val, i64* @Authority_signal_ptr

; 3. Guard: should_fire?
%target_ptr = getelementptr ... @".str.Authority.signal"
%should_fire = call i64 @abi_resonate_should_fire_i64(
    i8* %target_ptr, i64 <dampen_ns>, i64 %old_val, i64 %new_val)
%should_fire_bool = icmp ne i64 %should_fire, 0
br i1 %should_fire_bool, label %fire, label %done

fire:
  call void @__kain_resonate_resonate__Authority__signal(
      i64 %old_val, i64 %new_val, i1 true)
  call void @abi_resonate_exit(i8* %target_ptr)
  br label %done

done:
  ; continue with next code
```

---

## The Body Environment — resonate_old_i64, resonate_new_i64, resonate_fired

Every resonate body has exactly three auto-injected locals:

### `resonate_old_i64: Int` (I64)
- **The value before the write** — the old state of the watched field
- **Zero for initial state** (no prior write) for non-i64 types that are zero-extended
- Use for: comparison, differential computation, detecting direction of change

### `resonate_new_i64: Int` (I64)
- **The value after the write** — the new state of the watched field
- **Always the value just stored** by the triggering write
- Use for: processing the new state, cascading to other fields, running pipelines

### `resonate_fired: Bool`
- **Always true** in the handler body (the guard already checked should_fire)
- Provided for correctness — any code path that checks this will always see `true`
- Use for: conditional logic that semantically depends on "we actually fired"

### For Non-i64 Types

The LLVM codegen handles type coercion:

| Field Type | old value | new value | ABI function |
|---|---|---|---|
| `i64` | Actual old value | Actual new value | `abi_resonate_should_fire_i64(target, dampen, old, new)` |
| `i1` (Bool) | 0 (zero-extended) | zext(value) to i64 | `abi_resonate_should_fire_i64(target, dampen, 0, zext)` |
| `double` (Float) | fptosi to i64 | fptosi to i64 | `abi_resonate_should_fire_f64(target, dampen, old_f64, new_f64)` |
| Other | 0 | 0 | `abi_resonate_should_fire_i64(target, dampen, 0, 0)` |

For `f64` targets, `abi_resonate_should_fire_f64` is used with the original double values, so the native ABI can compare floating-point changes correctly.

### Canonical Body Usage

```kn
resonate Authority.signal dampen 0 ms:
    // Record the change
    Authority.last_old = resonate_old_i64
    Authority.last_new = resonate_new_i64

    // The fired flag is always true here
    if resonate_fired:
        Authority.fired_count = Authority.fired_count + 1

    // Process — run a pipeline based on the new value
    Authority.shadow = run_pipeline(resonate_new_i64 + Authority.tick)
    Authority.signal_shadow = (
        resonate_new_i64 * 53 + resonate_old_i64 * 17 + 7
    ) % MODULUS
```

---

## Dampening Semantics

### What Dampening Does

A dampen window (`dampen Nms`) prevents a resonate handler from re-firing within a specified time window. This is **debouncing** — if the target field changes rapidly (e.g., every 1ms from a control signal), the handler fires at most once per dampen window.

```kn
// With 32ms dampening, rapid writes at 1ms intervals only fire every 32ms
resonate FxWorld.distortion_drive dampen 32ms:
    let new_drive: Int = resonate_new_i64
    // ...expensive computation that should not run 1000 times per second...
```

### Zero Dampening

`dampen 0 ms` means **no dampening** — every write that changes the value triggers the handler. This is common for critical state tracking where every mutation must be observed:

```kn
// Every write to signal fires the handler
resonate Authority.signal dampen 0 ms:
    Authority.last_new = resonate_new_i64
```

### How Dampening Works in the Native ABI

In `stdlib_abi.c`, the `abi_resonate_should_fire_common()` function performs:

```c
static int64_t abi_resonate_should_fire_common(
    const char* target, int64_t dampen_ns, int64_t old_value, int64_t new_value
) {
    // 1. Record mutation (telemetry — always increments)
    g_kain_native_resonate_mutation_count += 1;

    // 2. Find/create slot (linear probe up to ABI_RESONATE_SLOT_MAX)
    slot = abi_resonate_slot_for(target);

    // 3. Reentry guard: if already in a handler for this target, absorb
    if (slot->active_depth > 0) {
        g_kain_native_resonate_absorb_count += 1;
        return 0;  // absorbed — don't fire
    }

    // 4. Dampening check: if within dampen window since last fire, absorb
    now_ns = abi_resonate_now_ns();
    if (dampen_ns > 0 && slot->last_fire_ns > 0 &&
        now_ns - slot->last_fire_ns < dampen_ns) {
        g_kain_native_resonate_absorb_count += 1;
        return 0;  // absorbed — within dampen window
    }

    // 5. Fire: update slot state, increment fire counter
    slot->last_fire_ns = now_ns;
    slot->active_depth += 1;
    g_kain_native_resonate_fire_count += 1;
    return 1;  // fire the handler
}
```

The logic:

| Condition | Action | Counter |
|---|---|---|
| Target doesn't match any slot | Absorb (return 0) | absorb_count++ |
| Slot already active (reentrant) | Absorb (return 0) | absorb_count++ |
| Within dampen window since last fire | Absorb (return 0) | absorb_count++ |
| First fire or outside dampen window | Fire (return 1) | fire_count++ |

### Absorb vs Fire

The telemetry distinguishes two counters:

- **`resonate_fire_count`** — number of times handlers actually executed (should_fire returned 1)
- **`resonate_absorb_count`** — number of times a potential fire was suppressed by dampening or reentry

The sum `fire_count + absorb_count` equals `mutation_count` (total number of times any field was mutated through the should_fire path). This gives full visibility into how much work the resonate system is absorbing vs dispatching.

### Dampen Unit Conversion (to nanoseconds)

| Unit | 1 unit in ns | Formula |
|---|---|---|
| `ns` | 1 | `value` (as-is) |
| `us` | 1,000 | `value * 1_000` |
| `ms` | 1,000,000 | `value * 1_000_000` |
| `s` | 1,000,000,000 | `value * 1_000_000_000` |
| `tick` / `ticks` | 1,000,000 | `value * 1_000_000` (treated as ms) |

---

## The Self-Feedback Rule

### Why Self-Feedback is Rejected

If a resonate handler could write to its own trigger field, it would cause **infinite recursion**: writing the target → triggers resonate → handler writes target → triggers resonate again → ...

The typechecker prevents this at compile time:

```rust
if plan.directly_mutates_target() {
    return Err(env.type_error(
        "resonate '{}' directly mutates its own target '{}'; \
         route feedback through a different patch/world path or dampened orchestrate stage",
        ...
    ));
}
```

### How It's Enforced

1. `check_resonate()` calls `collect_patch_mutation_paths_from_block()` which walks the body AST and collects all paths that are written via `patch` or direct world field assignment
2. `ResonancePlan::directly_mutates_target()` checks if any collected path **equals** the target path or **starts with** `target + "."` (for struct field sub-paths)
3. If any match is found, the compiler returns a type error — the resonate is rejected

### What You Write Instead

```kn
// ❌ WRONG: self-feedback (compiler rejects)
resonate Auth.signal dampen 0 ms:
    Auth.signal = Auth.signal + 1    // DIRECTLY MUTATES OWN TARGET

// ✅ RIGHT: write to a different field (shadow)
resonate Auth.signal dampen 0 ms:
    Auth.shadow = Auth.signal + 1    // writes shadow, not signal

// ✅ RIGHT: write through a different path
resonate Auth.signal dampen 0 ms:
    let result = run_pipeline(Auth.signal)
    Auth.processed = result          // different field
```

The self-feedback rule applies to **direct** mutation of the target field path. Writing to other world fields (including the target world's different fields) is perfectly fine.

---

## Effect Model

Resonate bodies are permitted the broadest effects of any Kain construct, on par with `pulse`:

| Effect | Why Allowed |
|--------|-------------|
| `IO` | Console output, logging, file I/O during reactive handlers |
| `Async` | Spawning async tasks from a reactive trigger |
| `GPU` | Dispatch calls from reactive state changes |
| `Reactive` | Writing to other world fields that may have their own resonate handlers |
| `Unsafe` | Raw memory operations, pointer arithmetic, atomics |
| `Alloc` | Heap allocation within handler bodies |
| `Panic` | Fatal error handling within reactive handlers |

**Resonate bodies accept ALL effects** — they are the most permissive semantic constructs in Kain, alongside `pulse`. This is because code that reacts to state changes needs full access to the program's capabilities.

---

## Runtime Contract Metadata

When a resonate is compiled, it produces a `RuntimeResonanceContract`:

```rust
pub struct RuntimeResonanceContract {
    pub name: String,                    // "resonate__Authority__signal"
    pub target: String,                  // "Authority.signal"
    pub target_type: String,             // "Int", "Bool", etc.
    pub dampen: String,                  // "0ms", "32ms", "1s"
    pub dampen_ns: u64,                  // 0, 32000000, 1000000000
    pub handler_symbol: String,          // "__kain_resonate_resonate__Authority__signal"
    pub direct_mutation_paths: Vec<String>,  // paths the handler writes to
}
```

### Capabilities Emitted

When any resonate is present in the program:

| Capability Key | Provider | Description |
|---|---|---|
| `state.resonate` | `kain-core.runtime` | "Program declares compiler-owned shadow-patch reactivity over world state stores." |
| `reactivity.shadow-patch` | `crates/resonate` | "Resonate handlers are statically tied to mutation paths with dampened native firing semantics." |
| `reactivity.dampen-window` | `crates/resonate` | "Resonate handlers use dampen windows for absorption-based debouncing." |

### Runtime Service Binding

```rust
if summary.resonances > 0 {
    bindings.push(runtime_service_binding(
        "state.resonate",
        if matches!(target, CompileTarget::C | CompileTarget::Llvm) {
            "runtime/native"
        } else {
            "kain-resonate"
        },
        runtime_lane_name(target),
    ));
}
```

### Contract Collection

All resonances (including those nested in modules) are collected, sorted by target then name, and included in the program's runtime contract manifest:

```rust
fn collect_resonance_contracts(items: &[TypedItem]) -> Vec<RuntimeResonanceContract> {
    let mut contracts = Vec::new();
    collect_resonance_contracts_into(items, &mut contracts);
    contracts.sort_by(|left, right| {
        left.target
            .cmp(&right.target)
            .then(left.name.cmp(&right.name))
    });
    contracts
}
```

---

## LLVM Codegen — Guarded Post-Store Call

### Three Locations Affected

The LLVM codegen touches three areas for resonate:

#### 1. Handler Function Compilation (`compile_resonate`)

```rust
fn compile_resonate(&mut self, resonate: &TypedResonate) -> KainResult<()> {
    let handler_symbol = Self::resonance_handler_symbol(&resonate.ast.name);
    // "__kain_resonate_resonate__Authority__signal"
    let params = Self::resonance_context_params(resonate.ast.span);
    // Parameters: resonate_old_i64, resonate_new_i64, resonate_fired (Bool)
    let resolved_type = ResolvedType::Function {
        params: vec![
            ResolvedType::Int(IntSize::I64),  // resonate_old_i64
            ResolvedType::Int(IntSize::I64),  // resonate_new_i64
            ResolvedType::Bool,               // resonate_fired
        ],
        ret: Box::new(ResolvedType::Unit),
        effects: EffectSet::new(),
    };
    self.compile_named_callable(
        &handler_symbol, &resonate.ast.attributes,
        &params, None, &resonate.ast.body,
        &resolved_type, resonate.ast.span,
    )
}
```

Each handler is compiled as an internal `void(i64, i64, bool)` function. The parameters match the auto-injected body locals.

#### 2. World Field Store Post-Hook

When the codegen emits a world field store, it checks for resonate bindings:

```rust
let resonance_binding = field_path
    .as_ref()
    .and_then(|path| self.native_resonance_binding(path));

// Capture old value BEFORE the store (if i64 and has resonate or patch)
let needs_old_i64 = field_ty == "i64"
    && (self.current_patch_name.is_some() || resonance_binding.is_some());
```

After the store is emitted:

```rust
if let Some(binding) = resonance_binding.as_ref() {
    self.emit_resonance_after_store(
        binding, &field_ty,
        old_value.as_deref(), old_f64_value.as_deref(), &rhs,
    )?;
}
```

#### 3. Guard + Handler Call (`emit_resonance_after_store`)

The full LLVM IR emitted:

```llvm
; Type dispatch for old/new value handling
; i64 path:
%should_fire = call i64 @abi_resonate_should_fire_i64(
    i8* %target_ptr, i64 <dampen_ns>, i64 %old_val, i64 %new_val)

; i1 (Bool) path — zext to i64 first
%zext = zext i1 %new_val to i64
%should_fire = call i64 @abi_resonate_should_fire_i64(
    i8* %target_ptr, i64 <dampen_ns>, i64 0, i64 %zext)

; f64 (Float) path — fptosi + f64 variant
%old_i64 = fptosi double %old_f64 to i64
%new_i64 = fptosi double %new_f64 to i64
%should_fire = call i64 @abi_resonate_should_fire_f64(
    i8* %target_ptr, i64 <dampen_ns>, double %old_f64, double %new_f64)

; Other types — zeroed
%should_fire = call i64 @abi_resonate_should_fire_i64(
    i8* %target_ptr, i64 <dampen_ns>, i64 0, i64 0)

; Branch on result
%should_fire_bool = icmp ne i64 %should_fire, 0
br i1 %should_fire_bool, label %fire, label %done

fire:
  call void @__kain_resonate_{name}(i64 %old, i64 %new, i1 true)
  call void @abi_resonate_exit(i8* %target_ptr)
  br label %done

done:
  ; continue
```

### Declarations

At module level, all ABC functions are declared once:

```llvm
declare i64 @abi_resonate_should_fire_i64(i8*, i64, i64, i64)
declare i64 @abi_resonate_should_fire_f64(i8*, i64, double, double)
declare void @abi_resonate_exit(i8*)
```

### Native Resonance Information Collection

During codegen initialization, all resonate declarations are collected into `NativeResonanceInfo`:

```rust
struct NativeResonanceInfo {
    target: String,         // "Authority.signal"
    dampen_ns: u64,         // 0, 32000000, etc.
    handler_symbol: String, // "__kain_resonate_resonate__Authority__signal"
}
```

These are stored in the codegen context and looked up during world field store lowering via `native_resonance_binding(path)`.

---

## Native C Runtime — Slot Table, Dampening, Reentry Guard

### Slot Table

```c
#define ABI_RESONATE_SLOT_MAX 128

typedef struct KainNativeResonanceSlot {
    char target[256];           // "Authority.signal"
    uint64_t last_fire_ns;      // timestamp of last fire (nanoseconds)
    int64_t active_depth;       // reentry guard counter
} KainNativeResonanceSlot;

static KainNativeResonanceSlot g_kain_native_resonance_slots[ABI_RESONATE_SLOT_MAX];
```

**Design:** 128 linear-probed slots. Slots are lazily created when a target is first seen (in `abi_resonate_slot_for()`). Once occupied, the slot persists for the lifetime of the program.

### Telemetry Counters

```c
static int64_t g_kain_native_resonate_mutation_count = 0;  // total should_fire calls
static int64_t g_kain_native_resonate_fire_count = 0;      // handlers actually executed
static int64_t g_kain_native_resonate_absorb_count = 0;    // suppressed by dampen/reentry
static char g_kain_native_resonate_last_target[256] = "";  // last target path
static int64_t g_kain_native_resonate_last_old_i64 = 0;    // last old value
static int64_t g_kain_native_resonate_last_new_i64 = 0;    // last new value
static int64_t g_kain_native_resonate_last_dampen_ns = 0;  // last dampen window
```

### Slot Lookup

```c
static KainNativeResonanceSlot* abi_resonate_slot_for(const char* target) {
    // Linear probe all 128 slots
    // If found by target match: return slot
    // If first empty slot found: initialize and return
    // If no slot found (all 128 occupied): return NULL (absorb)
}
```

**Three outcomes:**
- **Match found** — return existing slot
- **Empty slot found** — first touch for this target: init and return
- **All 128 slots full** — return NULL (the mutation is absorbed/ignored)

### The should_fire Pipeline

```
abi_resonate_should_fire_i64() / f64()
  → abi_resonate_should_fire_common()
    → find/create slot for target
    → if slot is NULL: absorb (increment absorb_count, return 0)
    → if slot->active_depth > 0: absorb (reentry guard, return 0)
    → if within dampen window: absorb (return 0)
    → else: fire (set last_fire_ns = now, active_depth += 1, fire_count++, return 1)
```

### Exit

```c
void abi_resonate_exit(const char* target) {
    KainNativeResonanceSlot* slot = abi_resonate_slot_for(target);
    if (slot != NULL && slot->active_depth > 0) {
        slot->active_depth -= 1;   // release reentry guard
    }
}
```

### Reentry Guard (active_depth)

The `active_depth` counter prevents a resonate handler from triggering itself recursively. If code inside the handler writes to the same world field (or entangle-propagates back to it), the should_fire check sees `active_depth > 0` and absorbs the mutation.

This is a **reentrancy safety** mechanism, not a dampening mechanism. Dampening is about time; reentry guard is about stack safety.

---

## Stdlib Telemetry API

The stdlib exposes resonate telemetry through `std::intent`:

### Low-Level ABI Functions (@extern)

| Function | Signature | Description |
|---|---|---|
| `abi_resonate_mutation_count` | `() -> Int` | Total should_fire calls (fired + absorbed) |
| `abi_resonate_fire_count` | `() -> Int` | Total handler fires (returned 1) |
| `abi_resonate_absorb_count` | `() -> Int` | Total absorptions (dampened + reentry + no-slot) |
| `abi_resonate_last_target` | `() -> String` | Last target path that was checked |
| `abi_resonate_last_old_i64` | `() -> Int` | Last old value passed to should_fire |
| `abi_resonate_last_new_i64` | `() -> Int` | Last new value passed to should_fire |
| `abi_resonate_last_dampen_ns` | `() -> Int` | Last dampen_ns value used |

### Public Wrappers

```kn
use std::intent

// All wrappers are direct passthroughs
pub fn native_resonate_mutation_count() -> Int:
    return abi_resonate_mutation_count()

pub fn native_resonate_fire_count() -> Int:
    return abi_resonate_fire_count()

pub fn native_resonate_absorb_count() -> Int:
    return abi_resonate_absorb_count()

pub fn native_resonate_last_target() -> String:
    return abi_resonate_last_target()

pub fn native_resonate_last_old_i64() -> Int:
    return abi_resonate_last_old_i64()

pub fn native_resonate_last_new_i64() -> Int:
    return abi_resonate_last_new_i64()

pub fn native_resonate_last_dampen_ns() -> Int:
    return abi_resonate_last_dampen_ns()
```

### Usage in Kain

```kn
fn prove_resonate_fired() -> Int:
    let before = resonate_fire_count()
    // ... trigger a world mutation ...
    let delta = resonate_fire_count() - before
    if delta < 1:
        return -10    // resonate never fired — error code
    return 0

fn prove_resonate_absorbed() -> Int:
    let absorb_before = resonate_absorb_count()
    // ... rapid writes within dampen window ...
    let delta = resonate_absorb_count() - absorb_before
    if delta < 1:
        return -15    // dampening never triggered
    return 0
```

---

## The Resonate Patterns

### Pattern 1: Simple Watch (No Dampening)

Fire on every change, record old and new values:

```kn
resonate Authority.signal dampen 0 ms:
    Authority.last_old = resonate_old_i64
    Authority.last_new = resonate_new_i64
```

**When to use:** Critical state tracking, debugging, logging, any scenario where every mutation must be observed.

### Pattern 2: Dampened Watch (Debounced)

Fire at most once per dampen window:

```kn
resonate FxWorld.distortion_drive dampen 32ms:
    let new_drive: Int = resonate_new_i64
    FxWorld.distortion_output = clamp_output(new_drive)
```

**When to use:** Expensive computations (audio DSP parameter updates, 3D scene updates, network-triggered recomputation) where rapid changes should be consolidated.

### Pattern 3: Multi-Resonate (Multiple Fields, Same World)

```kn
world FxWorld:
    state lfo1_rate: Int = 23
    state distortion_drive: Int = 500
    state filter_cutoff: Int = 1000
    state tremolo_rate: Int = 10

resonate FxWorld.lfo1_rate dampen 16ms:
    let new_rate: Int = resonate_new_i64
    let sync_rate: Int = new_rate * 2
    if sync_rate <= 500:
        FxWorld.tremolo_rate = sync_rate

resonate FxWorld.distortion_drive dampen 32ms:
    let new_drive: Int = resonate_new_i64
    FxWorld.filter_cutoff = clamp(2000 - new_drive * 2, 200, 20000)

resonate FxWorld.filter_cutoff dampen 16ms:
    let new_cut: Int = resonate_new_i64
    // (does NOT write to filter_cutoff — writes to a shadow instead)
    FxWorld.filter_output = run_filter(new_cut)
```

**When to use:** Audio DSP, game parameters, simulation controls — any system with multiple interdependent parameters that each need reactive handling.

### Pattern 4: Resonate + Patch (Journaled Change + Reactive Handler)

```kn
patch strike_signal(authority: Authority, value: Int) -> Int:
    authority.signal = value
    return value

resonate Authority.signal dampen 0 ms:
    Authority.shadow = mutate(resonate_new_i64)
```

The patch journals the change; the resonate handler computes a derived value. The two are separate concerns — patch owns journaling, resonate owns reaction.

### Pattern 5: Resonate + Orchestrate (Pipeline Execution in Handler)

```kn
orchestrate fusion_signal_pipeline(value: Int, tick: Int) -> Int:
    stage host_mix: cpu fusion_mix(value + tick) when capability("cpu.scalar")
        residency host transfer none policy telemetry_prefer_cpu
    stage fast_mix: converge fusion_fast_mix(host_mix, tick)
        deps [host_mix] residency host policy static
    return fast_mix

resonate FusionAuthority.signal dampen 0 ms:
    FusionAuthority.last_old = resonate_old_i64
    FusionAuthority.last_new = resonate_new_i64
    FusionAuthority.shadow = fusion_signal_pipeline(
        resonate_new_i64 + FusionAuthority.tick,
        FusionAuthority.tick
    )
```

**When to use:** When the reactive handler needs multi-stage computation (CPU → GPU → law → converge), where `orchestrate` provides the stage graph and `resonate` provides the trigger.

### Pattern 6: Resonate + Entangle (Cascaded Reactivity)

When a world field is both entangled and has a resonate handler, the propagation chain becomes:

```kn
world FusionAuthority:
    state signal: Int = 1
    state shadow: Int = 0

world FusionMirror:
    state signal_copy: Int = 1

entangle FusionAuthority.signal <-> FusionMirror.signal_copy with single_writer

resonate FusionAuthority.signal dampen 0 ms:
    // This fires BEFORE entangle propagation
    FusionAuthority.shadow = process(resonate_new_i64)
```

After the write: resonate fires → shadow updated → entangle propagates → mirror updated. The resonate handler processes the value before it reaches the mirror.

### Pattern 7: Fusion Chain — Full Causal Chain

From the `fusion_chain.kn` benchmark — the full causal chain:

```
world mutation (patch)
  → resonate fires (tripwire layer)
    → patch commits new state (orchestrate in handler)
      → entangle propagates to mirror
        → pulse ticks observe mirror
          → actor receives ask() with live mirror value
```

```kn
resonate FusionAuthority.signal dampen 0 ms:
    FusionAuthority.last_old = resonate_old_i64
    FusionAuthority.last_new = resonate_new_i64
    FusionAuthority.shadow = fusion_signal_pipeline(
        resonate_new_i64 + FusionAuthority.tick,
        FusionAuthority.tick
    )
```

The handler records old/new values, then runs an orchestrate pipeline, and writes the result to a shadow field. Actors downstream read the shadow instead of the raw signal.

---

## Resonate + Pulse

Resonate and pulse are LAYER 5: TEMPORAL siblings. They complement each other:

| Aspect | Pulse | Resonate |
|---|---|---|
| **Trigger** | Wall-clock time, recurring | State change, reactive |
| **Body locals** | `pulse_tick`, `pulse_dt_ms`, `pulse_missed` | `resonate_old_i64`, `resonate_new_i64`, `resonate_fired` |
| **Dampening** | `jitter Nms` (timing tolerance) | `dampen Nms` (debounce window) |
| **Scheduling** | Scheduler thread (async) | Inline after store (sync) |
| **State access** | Read/write any world | Read/write any world except own trigger |
| **When to use** | Any recurring beat | Any state-change observer |

### Pulse Writes Triggering Resonate

The most common fusion: a pulse writes to a world field, and that write triggers a resonate handler:

```kn
pulse param_beat every 8ms:
    FxWorld.distortion_drive = compute_drive(pulse_tick)

resonate FxWorld.distortion_drive dampen 32ms:
    let new_drive = resonate_new_i64
    FxWorld.distortion_output = clamp_output(new_drive)
```

The pulse is the temporal driver; the resonate is the reactive processor. The pulse computes a new value; the resonate applies it with dampening.

### Pulse + Resonate + Entangle (Trinity)

Pulse drives writes → resonate processes → entangle mirrors:

```kn
world Authority:
    state signal: Int = 0
    state shadow: Int = 0
world Mirror:
    state signal_copy: Int = 0

entangle Authority.signal <-> Mirror.signal_copy with single_writer

pulse signal_gen every 16ms:
    Authority.signal = compute_signal(pulse_tick)

resonate Authority.signal dampen 0 ms:
    Authority.shadow = process(resonate_new_i64)
```

Every 16ms: pulse computes signal → writes to Authority.signal → resonate fires → shadow updated → entangle propagates to Mirror.

### Choosing Between Pulse and Resonate

Use **pulse** when:
- You need a recurring timed beat
- You own the timing (frame clock, LFO, health beat)
- You want `pulse_tick` / `pulse_dt_ms` / `pulse_missed` timing locals

Use **resonate** when:
- You need to react to external or non-periodic state changes
- You want dampening (debouncing) of rapid changes
- You want `resonate_old_i64` / `resonate_new_i64` / `resonate_fired` change locals
- The trigger is a world state write, not a clock

Use **both** when:
- A pulse drives periodic writes and a resonate processes the changes
- The pulse is the temporal driver and the resonate is the reactive processor

See also: [PULSE.MD](PULSE.MD) for the full pulse reference.

---

## Under-Utilized Ideas — Creative Semantic Abuse

### 1. Resonate as State-Machine Guard

A resonate handler can act as an invariant guard — if a field changes to an invalid state, the handler can panic or escalate:

```kn
resonate SafetyWorld.pressure dampen 0 ms:
    let p: Int = resonate_new_i64
    if p < 0 or p > 1000:
        SafetyWorld.alarm = 1
        panic("pressure out of bounds: " + str(p))
```

**Under-explored:** Using resonate as a **runtime invariant enforcer** that watches critical state and escalates on violation.

### 2. Resonate as Audit Trail Trigger

Every mutation to a watched field is journaled via resonate:

```kn
resonate AuditWorld.transaction_id dampen 0 ms:
    let old_val = resonate_old_i64
    let new_val = resonate_new_i64
    AuditWorld.audit_log[AuditWorld.audit_index] = (
        (old_val * 1000003) + new_val
    )
    AuditWorld.audit_index = (
        (AuditWorld.audit_index + 1) % 256
    )
```

**Under-explored:** Using resonate as a **write journal** that records every mutation to an audit log. Since resonate fires synchronously, the audit trail is always complete.

### 3. Resonate as Cross-World Notifier

A resonate on one world can propagate state to a completely different world (no entangle needed):

```kn
resonate SourceWorld.value dampen 0 ms:
    TargetWorld.derived = process(resonate_new_i64)
```

**Under-explored:** Using resonate as an **ad-hoc coupling** mechanism between worlds. No entangle declaration needed — the handler body explicitly writes to the target world. This gives more control than entangle (which is 1:1 field coupling) while still being compiler-witnessed.

### 4. Resonate + Actor: Reactively Spawn Workers

```kn
resonate WorkWorld.pending dampen 10ms:
    if resonate_new_i64 > 10:
        let worker = spawn WorkerActor(threshold = resonate_new_i64)
        send(worker, "ProcessBatch", resonate_new_i64)
```

**Under-explored:** Using resonate to reactively spawn actors when a workload threshold is crossed. The dampen window prevents spawning storms.

### 5. Resonate as Health Check Escalation

```kn
resonate HealthWorld.critical_count dampen 1s:
    if resonate_new_i64 > 0:
        let count = resonate_new_i64
        if count > 5:
            HealthWorld.status = 3  // CRITICAL
        else if count > 0:
            HealthWorld.status = 2  // WARNING
```

**Under-explored:** Multi-tier health escalation where the dampen window prevents flapping between states.

### 6. Resonate for Adaptive Quality Scaling

```kn
resonate PerformanceWorld.frame_time_ms dampen 500ms:
    let ft = resonate_new_i64
    if ft > 33:     // below 30 FPS
        QualityWorld.texture_resolution = 512
    elif ft > 16:   // below 60 FPS
        QualityWorld.texture_resolution = 1024
    else:
        QualityWorld.texture_resolution = 2048
```

**Under-explored:** Using resonate to reactively adjust visual quality based on frame-time telemetry.

---

## Combining with Other Keywords

### Resonate + World

The foundation. Every resonate watches a world state field:

```kn
world Auth:
    state signal: Int = 1
    state shadow: Int = 0

resonate Auth.signal dampen 0 ms:
    Auth.shadow = process(resonate_new_i64)
```

### Resonate + Patch

Patches trigger resonate handlers. Every `patch` that assigns to a world field will automatically trigger the resonate guard after the store:

```kn
patch strike(authority: Auth, value: Int) -> Int:
    authority.signal = value    // triggers resonate after store
    return authority.signal

resonate Auth.signal dampen 0 ms:
    Auth.shadow = process(resonate_new_i64)
```

### Resonate + Entangle

Resonate fires **before** entangle propagation. The handler runs on the authority side; entangle propagates to the mirror side. They coexist at zero cost — the compiler emits the resonate call before the entangle propagation code:

```kn
entangle Auth.signal <-> Mirror.signal_copy with single_writer

resonate Auth.signal dampen 0 ms:
    Auth.shadow = process(resonate_new_i64)
    // After this handler: entangle propagates signal to Mirror.signal_copy
```

### Resonate + Orchestrate

Resonate handlers can run orchestrate pipelines. This is the "reactive pipeline" pattern — the trigger is a state change, the response is a multi-stage computation:

```kn
orchestrate signal_pipeline(value: Int, tick: Int) -> Int:
    stage mix: cpu mix(value + tick) when capability("cpu.scalar")
        residency host transfer none policy static
    return mix

resonate Auth.signal dampen 0 ms:
    Auth.shadow = signal_pipeline(
        resonate_new_i64 + Auth.tick,
        Auth.tick
    )
```

### Resonate + Converge

Resonate + converge gives reactive dispatch with fast lanes:

```kn
converge process_signal(value: Int) -> Int:
    spec reference:
        return mix(value)
    fast llvm when target("llvm"):
        return fast_mix(value)

resonate Auth.signal dampen 0 ms:
    Auth.shadow = process_signal(resonate_new_i64 + Auth.counter)
```

### Resonate + Teleport

Resonate handlers can teleport data cross-world:

```kn
resonate Source.data dampen 0 ms:
    let value: Int = resonate_new_i64
    teleport value from Source to Dest via direct_bus
```

### Resonate + Pulse

Pulse drives the writes; resonate processes them (see [Resonate + Pulse](#resonate--pulse)):

```kn
pulse tick every 16ms:
    Auth.signal = Auth.signal + 1

resonate Auth.signal dampen 0 ms:
    Auth.shadow = process(resonate_new_i64)
```

### Resonate + Law

Laws can be used inside resonate handlers to validate the new value:

```kn
law value_in_bounds(v: Int) -> Bool:
    return v >= 0 and v < MODULUS

resonate Auth.signal dampen 0 ms:
    if law_status(value_in_bounds(resonate_new_i64)):
        Auth.shadow = process(resonate_new_i64)
    else:
        Auth.error_count = Auth.error_count + 1
```

### Resonate + Actor

Resonate handlers can send messages to actors or spawn new ones:

```kn
actor Worker:
    state total: Int = 0
    on Process(count: Int):
        total = total + count

resonate Auth.signal dampen 10ms:
    if resonate_new_i64 > THRESHOLD:
        let worker = spawn Worker(total = 0)
        send(worker, "Process", resonate_new_i64)
```

---

## Native ABI Reference

The native C ABI for resonate consists of three core functions plus telemetry accessors:

### Core Functions

| Function | Signature | Description |
|---|---|---|
| `abi_resonate_should_fire_i64` | `(const char* target, int64_t dampen_ns, int64_t old_value, int64_t new_value) -> int64_t` | Check if resonate handler should fire for an i64 field; returns 1 to fire, 0 to absorb |
| `abi_resonate_should_fire_f64` | `(const char* target, int64_t dampen_ns, double old_value, double new_value) -> int64_t` | Check if resonate handler should fire for an f64 field; returns 1 to fire, 0 to absorb |
| `abi_resonate_exit` | `(const char* target) -> void` | Call after handler body completes; decrements active_depth on the slot |

### Telemetry Accessors

| Function | Signature | Description |
|---|---|---|
| `abi_resonate_mutation_count` | `() -> int64_t` | Total number of should_fire calls (fires + absorbs) |
| `abi_resonate_fire_count` | `() -> int64_t` | Total number of handler fires |
| `abi_resonate_absorb_count` | `() -> int64_t` | Total number of absorptions (dampened + reentry + no-slot) |
| `abi_resonate_last_target` | `() -> const char*` | Last target path that was checked |
| `abi_resonate_last_old_i64` | `() -> int64_t` | Last old value passed to should_fire |
| `abi_resonate_last_new_i64` | `() -> int64_t` | Last new value passed to should_fire |
| `abi_resonate_last_dampen_ns` | `() -> int64_t` | Last dampen_ns value used |

### Slot Table Constants

```c
#define ABI_RESONATE_SLOT_MAX 128

typedef struct KainNativeResonanceSlot {
    char target[256];           // target path string
    uint64_t last_fire_ns;      // last fire timestamp (nanoseconds)
    int64_t active_depth;       // reentry guard depth
} KainNativeResonanceSlot;
```

### should_fire Pipeline

```c
static int64_t abi_resonate_should_fire_common(
    const char* target,
    int64_t dampen_ns,
    int64_t old_value,
    int64_t new_value
) {
    // 1. Increment mutation counter (telemetry)
    g_kain_native_resonate_mutation_count += 1;

    // 2. Record telemetry values
    abi_copy_text(g_kain_native_resonate_last_target, ..., target);
    g_kain_native_resonate_last_old_i64 = old_value;
    g_kain_native_resonate_last_new_i64 = new_value;
    g_kain_native_resonate_last_dampen_ns = dampen_ns;

    // 3. Find/create slot
    slot = abi_resonate_slot_for(target);
    if (slot == NULL) {
        g_kain_native_resonate_absorb_count += 1;
        return 0;  // no slot available — absorb
    }

    // 4. Reentry guard
    if (slot->active_depth > 0) {
        g_kain_native_resonate_absorb_count += 1;
        return 0;  // already in handler — absorb
    }

    // 5. Dampening check
    now_ns = abi_resonate_now_ns();
    dampen_window = (uint64_t)dampen_ns;
    if (dampen_window > 0 && slot->last_fire_ns > 0 &&
        now_ns - slot->last_fire_ns < dampen_window) {
        g_kain_native_resonate_absorb_count += 1;
        return 0;  // within dampen window — absorb
    }

    // 6. Fire
    slot->last_fire_ns = now_ns;
    slot->active_depth += 1;
    g_kain_native_resonate_fire_count += 1;
    return 1;
}
```

---

## Validation Ladder

```powershell
# 1. Typecheck (handlers do NOT execute in interpreter)
kain check <file.kn> --target llvm

# 2. Compile + run (handlers execute in native LLVM)
kain run <file.kn> --target llvm

# 3. Verify resonate fired (check telemetry)
kain run <file.kn> --target llvm
# Add a test with resonate_fire_count() delta guard

# 4. Validate dampening
kain run <file.kn> --target llvm
# Check resonate_absorb_count() > 0 for dampened handlers in rapid-write loops

# 5. For benchmark proof
python benchmark/run.py --case resonate --languages kain --runs 1 --warmups 0
python benchmark/run.py --case fusion_chain --languages kain --runs 1 --warmups 0

# 6. For full semantic stress
python benchmark/run.py --case keyword_crucible --languages kain --runs 1 --warmups 0

# 7. Check runtime contract
# The output runtime_contract.json will show resonances section with
# target, target_type, dampen, dampen_ns, handler_symbol, and direct_mutation_paths
```

### Smoketest

```kn
// smoketest/src/semantics/resonate.kn
use std::intent
use std::runtime

world SmokeResonateAuthority:
    state signal: Int = 1
    state signal_shadow: Int = 0
    state dampen_probe: Int = 0
    state dampen_shadow: Int = 0
    state last_old: Int = 0
    state last_new: Int = 0
    surface web => SmokeResonatePanel

world SmokeResonateMirror:
    state signal_copy: Int = 1
    state dampen_copy: Int = 0
    surface web => SmokeResonatePanel

entangle SmokeResonateAuthority.signal <-> SmokeResonateMirror.signal_copy with single_writer
entangle SmokeResonateAuthority.dampen_probe <-> SmokeResonateMirror.dampen_copy with single_writer

resonate SmokeResonateAuthority.signal dampen 0 ms:
    SmokeResonateAuthority.last_old = resonate_old_i64
    SmokeResonateAuthority.last_new = resonate_new_i64
    SmokeResonateAuthority.signal_shadow = process(resonate_new_i64)

resonate SmokeResonateAuthority.dampen_probe dampen 1 s:
    SmokeResonateAuthority.dampen_shadow = SmokeResonateAuthority.dampen_probe + 1000

pub fn smoke_resonate_lane() -> Int:
    let fire_before = resonate_fire_count()
    let absorb_before = resonate_absorb_count()

    // Trigger resonate via patch on signal
    let _ = smoke_resonate_commit(SmokeResonateAuthority, 21)
    if resonate_fire_count() <= fire_before:
        return 4   // resonate never fired

    // Test dampening — two rapid writes, second should absorb
    let _ = smoke_resonate_commit_dampen(SmokeResonateAuthority, 40)
    let first_shadow = SmokeResonateAuthority.dampen_shadow
    let _ = smoke_resonate_commit_dampen(SmokeResonateAuthority, 41)
    if SmokeResonateAuthority.dampen_shadow != first_shadow:
        return 10  // dampener didn't absorb
    if resonate_absorb_count() <= absorb_before:
        return 12  // absorb count didn't increment
    return 0
```

### Telemetry Delta Guard Pattern

The canonical pattern from the fusion chain benchmark:

```kn
let resonate_before = resonate_fire_count()
let entangle_before = entangle_propagation_count()
let teleport_before = runtime_machine_teleport_count()
let patch_before = patch_journal_count()
let orchestrate_before = orchestrate_stage_count()

// ... run causal chain ...

if resonate_fire_count() - resonate_before < 1:
    return -10     // resonate never fired
if entangle_propagation_count() - entangle_before < 1:
    return -11     // entangle never propagated
```

Error codes from the fusion chain:
- `-10` = resonate never fired
- `-11` = entangle never propagated
- `-12` = teleport never happened
- `-13` = patch journal overflow
- `-14` = orchestrate never ran

---

## Z3 Proofs

### 1. Resonate Slot Capacity

**Currently unproven** — the slot table (`ABI_RESONATE_SLOT_MAX = 128`) is a bounded constant, but no Z3 proof file exists yet for the resonate slot capacity invariant. This is an area for future formal verification work, similar to the entangle registry proofs.

**Expected invariants to prove:**
- Slot count never exceeds `ABI_RESONATE_SLOT_MAX` (128)
- Slot target string copy fits within 256-byte buffer
- `active_depth` never goes negative (decrement-only after increment)
- `absorb_count + fire_count == mutation_count` (counters are consistent)

### 2. Known Z3 Data Catalog Findings

The Z3 data catalog (`runtime/native/src/core/z3/data/`) flags the resonate code:

| Finding | Risk | Details |
|---|---|---|
| `abi_resonate_should_fire_common` | 30 | Multiple arithmetic sites: timestamp subtraction, dampen window comparison, slot pointer arithmetic |
| `g_kain_native_resonate_slots` | 30 | Global mutable slot array accessed without lock — but this is by design (single-threaded mutation context for world field stores) |
| `g_kain_native_resonate_fire_count` increment | 55 | Counter state mutates without lock or atomic primitive — **Note:** In practice, world field stores are single-threaded in the current execution model, so non-atomic increments are correct; multi-threaded world access would need atomic counters |

---

## Source Anchors

### Resonate Crate (`crates/resonate/`)

| File | Content |
|---|---|
| `crates/resonate/src/lib.rs` | `ResonanceTarget`, `DampenWindow`, `DampenUnit`, `ResonancePlan`, `ResonancePlan::directly_mutates_target()`, `RESONATE_CAPABILITY_KEY` |

### Parser & AST (`crates/core/`)

| File | Lines | Content |
|---|---|---|
| `crates/core/src/ast.rs` | 150-151, 332-358 | `Item::Resonate(ResonateDef)`, `ResonateDef`, `ResonateEndpoint` |
| `crates/core/src/parser.rs` | 1026-1029 | `parse_resonate()` dispatch |
| `crates/core/src/parser.rs` | 1902-1931 | `parse_resonate()` full function |
| `crates/core/src/parser.rs` | 2255-2275 | `parse_resonate_endpoint()` — minimum 2 segments |

### Typechecker

| File | Lines | Content |
|---|---|---|
| `crates/core/src/types.rs` | 4666 | `check_resonate()` dispatch |
| `crates/core/src/types.rs` | 5871-5920 | `check_resonate()` full function — endpoint resolution, self-feedback detection, body typechecking |

### Runtime Contract

| File | Lines | Content |
|---|---|---|
| `crates/core/src/runtime_contract.rs` | 36, 171-181 | `RuntimeResonanceContract` struct |
| `crates/core/src/runtime_contract.rs` | 603-616 | `state.resonate` + `reactivity.shadow-patch` + `reactivity.dampen-window` capabilities |
| `crates/core/src/runtime_contract.rs` | 892-901 | Runtime service binding for `state.resonate` |
| `crates/core/src/runtime_contract.rs` | 1038-1040 | Contract item listing |
| `crates/core/src/runtime_contract.rs` | 1316-1353 | `collect_resonance_contracts()`, `runtime_resonance_contract()`, handler symbol |

### Runtime (Interpreter)

| File | Lines | Content |
|---|---|---|
| `crates/core/src/runtime.rs` | 4187-4189 | `Item::Resonate` → capability registration only (no interpretation) |
| `crates/core/src/runtime.rs` | 4309-4311 | `TypedItem::Resonate` → capability registration only |

### Realtime App Bundle

| File | Lines | Content |
|---|---|---|
| `crates/core/src/realtime_app_bundle.rs` | 1103-1128 | `RealtimeResonanceBinding`, collection, emission |
| `crates/core/src/realtime_app_bundle.rs` | 2407-2410 | Capability requirements for realtime bundles |
| `crates/core/src/realtime_app_bundle.rs` | 2519-2522 | Runtime requirements: `state.resonate`, `reactivity.shadow-patch`, `reactivity.dampen-window` |

### LLVM Codegen

| File | Lines | Content |
|---|---|---|
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 110-115 | `NativeResonanceInfo` struct |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 657 | `native_resonances` field in codegen context |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 11108-11195 | `emit_resonance_after_store()` — full LLVM IR emission |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 13405-13430 | `resonance_handler_symbol()`, `collect_native_resonances()`, `native_resonance_binding()` |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 14586-14588 | ABI function declarations |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 15393-15427 | `resonance_context_params()` — handler parameter generation |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 15429-15441 | `compile_resonate()` — handler function compilation |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 18861-18943 | World field store resonate binding lookup + old value capture + post-store emission |

### Native C Runtime

| File | Content |
|---|---|
| `runtime/native/include/stdlib_abi.h` | `abi_resonate_should_fire_i64`, `abi_resonate_should_fire_f64`, `abi_resonate_exit`, `abi_resonate_mutation_count`, `abi_resonate_fire_count`, `abi_resonate_absorb_count`, `abi_resonate_last_target`, `abi_resonate_last_old_i64`, `abi_resonate_last_new_i64`, `abi_resonate_last_dampen_ns` |
| `runtime/native/src/core/stdlib_abi.c` | Full slot table implementation: `KainNativeResonanceSlot`, `abi_resonate_slot_for()`, `abi_resonate_should_fire_common()`, `abi_resonate_exit()`, all telemetry accessors |

### Stdlib

| File | Content |
|---|---|
| `stdlib/intent.kn` | `abi_resonate_mutation_count`, `abi_resonate_fire_count`, `abi_resonate_absorb_count`, `abi_resonate_last_target`, `abi_resonate_last_old_i64`, `abi_resonate_last_new_i64`, `abi_resonate_last_dampen_ns` externs + public wrappers |

### Z3 Proofs

| File | Content |
|---|---|
| *(none yet)* | Resonate slot capacity, string copy, and counter consistency proofs are future work |

### Examples

| File | Usage |
|---|---|
| `smoketest/src/semantics/resonate.kn` | Smoke test: dual resonate on signal (0ms) + dampen_probe (1s), entangle, orchestrate, full telemetry delta guards |
| `benchmark/cases_v2/resonate.kn` | **Apex:** Resonate god-mode benchmark — 5 cases: fire_core, dampen_window, orchestrate_fusion, converge_llvm, raw_memory + world + entangle + patch + law + converge + orchestrate + collapse/observe/decay |
| `benchmark/cases_v2/fusion_chain.kn` | Multi-layer causal chain where resonate is the first reactive layer after world mutation |
| `benchmark/cases_v2/keyword_crucible.kn` | Case 3 "Semantic Full" — resonate as one of 108 keywords, telemetry delta guards |
| `blades/python/24_tet/src/resonate_py_effects.kn` | Real-world audio DSP: resonate on `lfo1_rate` (16ms dampen) and `distortion_drive` (32ms dampen) in a modular effects engine |

---

## Anti-Patterns

### ❌ Using Resonate When Pulse Is Correct

```kn
// WRONG: resonate for a recurring timed beat (no state change trigger)
resonate Clock.signal dampen 0 ms:
    Clock.signal = Clock.signal + 1

// RIGHT: pulse owns temporal beats
pulse clock_tick every 16ms:
    Clock.signal = Clock.signal + 1
```

**Why:** Resonate fires when state changes; pulse fires on a schedule. If you need a recurring beat, use pulse. If you need to react to state changes, use resonate.

### ❌ No Dampening on Fast-Changing Values

```kn
// WRONG: every write fires handler — 1000+ fires/second
resonate FxWorld.slider_value dampen 0 ms:
    FxWorld.processed = expensive_computation(resonate_new_i64)

// RIGHT: dampened to 33ms (~30fps update rate)
resonate FxWorld.slider_value dampen 33ms:
    FxWorld.processed = expensive_computation(resonate_new_i64)
```

**Why:** Without dampening, a rapidly-changing control (mouse drag, audio param, sensor input) can trigger the handler hundreds of times per second. A well-chosen dampen window absorbs redundant fires while keeping the response smooth.

### ❌ Using Actor `on` Instead of Resonate

```kn
// WRONG: actor message to handle state change
actor StateWatcher:
    on StateChanged(new_val: Int):
        World.processed = compute(new_val)

send(watcher, "StateChanged", new_value)

// RIGHT: resonate handles state change directly
resonate World.value dampen 0 ms:
    World.processed = compute(resonate_new_i64)
```

**Why:** If the trigger is a world state change, resonate is the correct construct. Actor messages add indirection, require an actor instance, and don't have the self-feedback guard or dampening. Actor `on` is for concurrent message dispatch, not reactive state observation.

### ❌ Self-Feedback Loop (Compile Error)

```kn
// WRONG: writing to own trigger field — compiler rejects
resonate Auth.signal dampen 0 ms:
    Auth.signal = Auth.signal + 1

// RIGHT: write to a different field
resonate Auth.signal dampen 0 ms:
    Auth.shadow = Auth.signal + 1
```

**Why:** The typechecker explicitly rejects this. All resonate bodies are statically checked for direct writes to their own target path.

### ❌ Assuming Resonate Handlers Are Async/Deferred

```kn
// WRONG: assuming handler runs "later"
resonate Auth.signal dampen 0 ms:
    Auth.shadow = process(resonate_new_i64)
// ... code here runs AFTER the handler completes (it's synchronous!)
```

Resonate handlers are **synchronous** — they run in the same call stack as the world field store, before any subsequent code. This means:
- The handler's side effects are visible immediately after the store
- If the handler panics, the store is fully complete
- There is no scheduler, no queue, no deferral

### ❌ Not Checking Telemetry

```kn
// WRONG: no proof resonate actually fired
fn test():
    let _ = strike_signal(Authority, 21)
    // should check resonate_fire_count delta!

// RIGHT: telemetry delta guard
fn test() -> Int:
    let before = resonate_fire_count()
    let _ = strike_signal(Authority, 21)
    if resonate_fire_count() - before < 1:
        return -10    // resonate never fired
    return 0
```

Without `resonate_fire_count()` deltas, you have no proof the handler actually executed.

### ❌ Using Resonate for Pure Data Pipelines

```kn
// WRONG: resonate as a data processing pipeline (use orchestrate instead)
resonate Input.value dampen 0 ms:
    Output.a = step_a(resonate_new_i64)
    Output.b = step_b(Output.a)
    Output.c = step_c(Output.b)

// RIGHT: orchestrate for multi-stage pipelines
orchestrate data_pipeline(value: Int) -> Int:
    stage a: cpu step_a(value)
    stage b: cpu step_b(a) deps [a]
    stage c: cpu step_c(b) deps [b]
    return c
```

**Why:** Orchestrate gives you typed stage graphs with residency, transfer policies, fallback, and telemetry. Resonate gives you a reactive trigger with dampening. If the focus is the pipeline, use orchestrate. If the focus is the reactive trigger, use resonate.

### ❌ Multiple Fast Resonate Handlers Beyond Slot Capacity

```kn
// POTENTIAL ISSUE: 128+ resonate handlers = slot exhaustion
// (first 128 register, rest silently absorb)
```

The slot table has `ABI_RESONATE_SLOT_MAX = 128` slots. If a program declares more than 128 resonate handlers, the excess handlers silently absorb all mutations. This is a known limitation and the target of future Z3 proof work.

### ❌ Resonate for Non-World Fields

```kn
// WRONG: resonate on a local variable or struct field
struct Data:
    value: Int = 0

fn test():
    let d = Data { value: 1 }
    // No way to resonate on d.value — only world fields!
```

**Why:** Resonate only works on world state fields. Local variables and plain struct fields don't have the world store lowering that inserts the should_fire guard.

---

## Quick Reference

| Aspect | Detail |
|---|---|
| **Syntax** | `resonate World.field [dampen <value><unit>]: <body>` |
| **Dampen units** | `ns`, `us`, `ms`, `s`, `tick`, `ticks` |
| **Body locals** | `resonate_old_i64: Int` (value before write), `resonate_new_i64: Int` (value after write), `resonate_fired: Bool` (always true) |
| **Self-feedback** | ❌ Compiler rejects direct writes to own target field |
| **Reentry guard** | ✅ Native ABI `active_depth` check prevents recursive firing |
| **Dampening** | Optional `dampen Nms` — absorbs re-fires within time window |
| **Effect permissions** | IO, Async, GPU, Reactive, Unsafe, Alloc, Panic — **all effects allowed** |
| **Trigger** | World field store (synchronous, in-store) |
| **Interpreter** | No-op — capabilities registered but handlers not interpreted |
| **LLVM output** | One handler per resonate: `__kain_resonate_{name}(i64, i64, i1)` |
| **LLVM store hook** | `emit_resonance_after_store()` — post-store guard + branch + handler + exit |
| **Slot table** | 128 linear-probed `KainNativeResonanceSlot` entries |
| **Telemetry** | `resonate_fire_count`, `resonate_absorb_count`, `resonate_mutation_count`, `resonate_last_target`, `resonate_last_old_i64`, `resonate_last_new_i64`, `resonate_last_dampen_ns` |
| **Capabilities** | `state.resonate`, `reactivity.shadow-patch`, `reactivity.dampen-window` |
| **Runtime contract** | `RuntimeResonanceContract { name, target, target_type, dampen, dampen_ns, handler_symbol, direct_mutation_paths }` |
| **Z3 proofs** | Future work: slot capacity, string copy, counter consistency |
| **Canonical example** | `benchmark/cases_v2/resonate.kn` — 5 cases across fire/dampen/orchestrate/converge/memory |
| **Canonical pattern** | Resonate + world + patch + telemetry delta guard |
| **Stdlib** | `std::intent` — all resonate telemetry wrappers |
| **Native ABI** | `stdlib_abi.h` — `abi_resonate_should_fire_i64`, `abi_resonate_should_fire_f64`, `abi_resonate_exit` |
| **Source** | `crates/resonate/src/lib.rs` (plan/target/dampen), `crates/core/src/ast.rs` (ResonateDef), `crates/core/src/types.rs` (check_resonate), `runtime/native/src/core/stdlib_abi.c` (slot table) |
