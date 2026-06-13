# L5 — Temporal Semantics: pulse + resonate

**Spec Document for `src/L5_temporal.kn`**
**Date:** 2026-06-12
**Self-Host Kainc Compiler**

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [AST Representation](#2-ast-representation)
3. [Rust Bootstrap Reference](#3-rust-bootstrap-reference)
4. [Parser Status](#4-parser-status)
5. [Typechecker Plan](#5-typechecker-plan)
6. [Codegen Plan](#6-codegen-plan)
7. [Runtime Contract](#7-runtime-contract)
8. [Implementation Tasks](#8-implementation-tasks)
9. [Dependencies](#9-dependencies)
10. [Test Plan](#10-test-plan)

---

## 1. Architecture Overview

### 1.1 Timed Recurrence (pulse) vs Reactive Tripwires (resonate)

L5 Temporal covers two complementary constructs that together form the **compiler-owned temporal and reactive surface** of Kain:

| Aspect | `pulse` | `resonate` |
|--------|---------|------------|
| **Nature** | Timed recurrence — fires on a schedule | Reactive tripwire — fires on state change |
| **Trigger** | Wall-clock time (`every Nms`) | World field write (value changed) |
| **Dampening** | `jitter Nms` (timing tolerance) | `dampen Nms` (debounce window) |
| **Body locals** | `pulse_tick`, `pulse_dt_ms`, `pulse_missed` | `resonate_old_i64`, `resonate_new_i64`, `resonate_fired` |
| **Scheduling** | OS scheduler thread (async) | Inline after store (sync) |
| **State access** | Read/write any world | Read/write any world except own trigger |
| **Effect permissions** | IO, Async, GPU, Reactive, Unsafe, Alloc, Panic | Same (all effects allowed) |
| **Interpreter** | No-op (native-only) | No-op (native-only) |
| **Runtime primitive** | Slot table, 64 entries, fire wrapper, scheduler thread | Slot table, 128 entries, dampening window, reentry guard |

### 1.2 The Temporal Relationship View

```
pulse ──── writes to world ────▶ resonate triggers (if world field watched)
  │                                  │
  │ p ulse_tick              resonate_old_i64 / resonate_new_i64
  │ pulse_dt_ms              resonate_fired
  │ pulse_missed
  │                                  │
  └──▶ body execution ──────▶ handler execution (post-store)
       (scheduler thread)        (inline, after field write)
```

The causal chain:
1. `pulse` fires (scheduler thread determines it's time)
2. Pulse body executes, potentially writing to world fields
3. World field stores trigger `resonate` guards (after each store)
4. Resonate handler executes if should_fire returns 1

### 1.3 Decision Ladder Context

| Question | Construct |
|----------|-----------|
| "Timed recurrence?" | `pulse` |
| "React to state change?" | `resonate` |
| "Both — pulse drives writes, resonate processes changes?" | `pulse` + `resonate` |
| "One-shot timer?" | `async` + `sleep` or `ask_timeout` |
| "Event-driven callback?" | `resonate` or actor `on` |

---

## 2. AST Representation

### 2.1 Pulse Node Layout

The Rust bootstrap stores pulse as `Item::Pulse(PulseDef)`:

```rust
pub struct PulseDef {
    pub name: String,
    pub interval: PulseDuration,
    pub jitter: Option<PulseDuration>,
    pub body: Block,
    pub visibility: Visibility,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

pub struct PulseDuration {
    pub value: i64,
    pub unit: Ident,
    pub span: Span,
}
```

The self-host compiler uses a flat integer array representation. For `AST_ITEM_PULSE` (constant 19):

```
data[0] = name_idx       (string-table index of pulse name)
data[1] = interval_value (i64, the numeric duration)
data[2] = interval_unit  (string-table index: "ms", "ns", "us", "s", "tick")
data[3] = has_jitter     (0 or 1)
data[4] = jitter_value   (i64, only if has_jitter == 1)
data[5] = jitter_unit    (string-table index, only if has_jitter == 1)
data[N] = body_idx       (AST index of the pulse body block)
```

**Current parser state** (blades/kain/src/parser.kn:2954-2968):
```
parse_pulse_item:
  - parses optional name token
  - parses body block after colon
  - stores ONLY [body_idx] in data

REQUIRED ADDITIONS:
  - parse "every" keyword
  - parse interval value + unit
  - optionally parse "jitter" keyword + value + unit
  - push all fields to data[]
```

### 2.2 Resonate Node Layout

Rust bootstrap:

```rust
pub struct ResonateDef {
    pub name: String,                 // auto-generated: "resonate__World__field"
    pub target: ResonateEndpoint,     // dotted path to world field
    pub dampen: Option<PulseDuration>, // optional dampen window
    pub body: Block,
    pub visibility: Visibility,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

pub struct ResonateEndpoint {
    pub segments: Vec<String>,       // ["World", "field"]
    pub span: Span,
}
```

Self-host layout for `AST_ITEM_RESONATE` (constant 20):

```
data[0] = name_idx          (string-table index of auto-generated name)
data[1] = target_seg_count  (number of segments in dotted path)
data[2..] = target segment name indices
data[N] = dampen_value      (i64, 0 if no dampen)
data[N+1] = dampen_unit     (string-table index, empty if no dampen)
data[M] = body_idx          (AST index of handler body block)
```

**Current parser state** (parser.kn:2972-2986):
```
parse_resonate_item:
  - parses optional name token
  - parses body block after colon
  - stores ONLY [body_idx] in data

REQUIRED ADDITIONS:
  - parse world.field target path (dotted identifier chains, min 2 segments)
  - optionally parse "dampen" keyword + value + unit
  - generate synthetic name from target path
  - push target and dampen fields to data[]
```

### 2.3 PulseDuration Encoding

Duration units and their integer encodings for AST storage:

| Unit | Encoding | Nanoseconds |
|------|----------|-------------|
| `ns` | 0 | `value * 1` |
| `us` | 1 | `value * 1000` |
| `ms` | 2 | `value * 1_000_000` |
| `s` | 3 | `value * 1_000_000_000` |
| `tick` | 4 | `value * 1` (platform-relative) |
| `ticks` | 5 | Same as tick |

The codegen will need a function `pulse_duration_to_ns(value: Int, unit_encoding: Int) -> Int` to convert the AST representation to nanoseconds for the runtime call.

### 2.4 Body Locals Tracking

Both pulse and resonate bodies receive auto-injected locals that MUST be tracked in the typechecker:

**pulse locals:**
| Local | Type | Description |
|-------|------|-------------|
| `pulse_tick` | `Int` (I64) | Monotonic counter, starts at 0 |
| `pulse_dt_ms` | `Int` (I64) | Actual elapsed ms since last beat |
| `pulse_missed` | `Int` (I64) | Missed beats count, >= 0 |

**resonate locals:**
| Local | Type | Description |
|-------|------|-------------|
| `resonate_old_i64` | `Int` (I64) | Value before the write |
| `resonate_new_i64` | `Int` (I64) | Value after the write |
| `resonate_fired` | `Bool` | Always true in handler body |

These must be injected into the scope before typechecking the body. The Rust bootstrap does this in `check_pulse()` (types.rs:5804-5830) by calling `env.define("pulse_tick", ...)` within a new scope.

---

## 3. Rust Bootstrap Reference

### 3.1 Pulse in the Bootstap

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/ast.rs` | 148, 309-326 | `Item::Pulse(PulseDef)`, `PulseDef`, `PulseDuration` structs |
| `crates/core/src/parser.rs` | 1026 | `parse_pulse()` dispatch in parse_item |
| `crates/core/src/parser.rs` | 1854-1913 | `parse_pulse()` full implementation — parses name, every, interval, optional jitter, body |
| `crates/core/src/parser.rs` | 1880-1908 | `parse_pulse_duration()` — reads integer token + ident token |
| `crates/core/src/types.rs` | 4665 | `check_pulse()` dispatch |
| `crates/core/src/types.rs` | 5804-5830 | `check_pulse()` — validates duration > 0, injects pulse_tick/dt/missed locals, checks body with broad effects |
| `crates/core/src/types.rs` | 5888-5910 | `validate_pulse_duration()`, `pulse_duration_unit_is_valid()` |
| `crates/core/src/runtime_contract.rs` | 161-168 | `RuntimePulseContract` struct |
| `crates/core/src/runtime_contract.rs` | 1288-1311 | `collect_pulse_contracts()`, `runtime_pulse_contract()` |
| `crates/core/src/runtime_contract.rs` | 1383-1392 | `pulse_duration_to_millis()` |
| `crates/core/src/runtime.rs` | 4186, 4308 | `Item::Pulse(_)` — no-op in interpreter |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 15161-15281 | `compile_pulse()` — body function + fire wrapper + snapshot |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 15699-15706 | Entry preamble — `kain_machine_pulse_start()` call |
| `runtime/native/include/machine_stones.h` | 19-42 | Public API: snapshot, start, stop, fire count |
| `runtime/native/src/core/machine_stones.c` | 287-491 | Full pulse scheduler implementation |

### 3.2 Resonate in the Bootstrap

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/ast.rs` | 148 | `Item::Resonate(ResonateDef)` |
| `crates/core/src/parser.rs` | 1019 | `parse_resonate()` dispatch |
| `crates/core/src/parser.rs` | 1915-2008 | `parse_resonate()` — parses endpoint, dampen, body; generates name |
| `crates/core/src/types.rs` | 4663 | `check_resonate()` dispatch |
| `crates/resonate/src/lib.rs` | 1-158 | `ResonanceTarget`, `DampenWindow`, `ResonancePlan`, `directly_mutates_target()` — the resonant crate |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | ~15282-15450 | `compile_resonate()` — handler function emission |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | ~15451-15600 | `emit_resonance_after_store()` — guard + handler call after world field store |
| `runtime/native/include/stdlib_abi.h` | Resonate ABI: `abi_resonate_should_fire_i64`, `f64`, `abi_resonate_exit` |
| `runtime/native/src/core/stdlib_abi.c` | Slot table, dampening, reentry guard, telemetry counters |

### 3.3 Key Validation Rules from the Bootstrap

#### Pulse validation (types.rs:5804-5830):

```rust
fn check_pulse(env: &mut TypeEnv, pulse: &PulseDef) -> KainResult<TypedPulse> {
    validate_pulse_duration(env, &pulse.interval, "pulse interval")?; // value must be > 0
    if let Some(jitter) = &pulse.jitter {
        validate_pulse_duration(env, jitter, "pulse jitter")?; // jitter must be > 0
    }
    // Body typechecked with pulse_tick, pulse_dt_ms, pulse_missed in scope
    // All effects allowed: IO, Async, GPU, Reactive, Unsafe, Alloc, Panic
    env.with_scope(|env| {
        env.define("pulse_tick", ResolvedType::Int(IntSize::I64));
        env.define("pulse_dt_ms", ResolvedType::Int(IntSize::I64));
        env.define("pulse_missed", ResolvedType::Int(IntSize::I64));
        check_block_semantics(env, &pulse.body, &ctx)
    })?;
    Ok(TypedPulse { ast: pulse.clone() })
}
```

#### Resonate validation (crates/resonate/src/lib.rs + types.rs):

```rust
fn check_resonate(env: &mut TypeEnv, resonate: &ResonateDef) -> KainResult<TypedResonate> {
    let target_type = resolve_resonate_endpoint_type(env, &resonate.target)?;
    let direct_mutation_paths = collect_patch_mutation_paths_from_block(&resonate.body);
    let target = ResonanceTarget::new(resonate.target.segments.clone())?;
    let plan = ResonancePlan::new(resonate.name.clone(), target, dampen, direct_mutation_paths);

    // Anti-self-feedback check
    if plan.directly_mutates_target() {
        return Err(env.type_error("...cannot write to own trigger field..."));
    }
    // Body typechecked with resonate_old_i64, resonate_new_i64, resonate_fired in scope
    // All effects allowed
    Ok(TypedResonate { ast: resonate.clone(), plan, ... })
}
```

The `ResonancePlan::directly_mutates_target()` method (resonate/src/lib.rs:121-127):
```rust
pub fn directly_mutates_target(&self) -> bool {
    let target = self.target.authored_path();
    self.direct_mutation_paths
        .iter()
        .any(|path| path == &target || path.starts_with(&(target.clone() + ".")))
}
```

---

## 4. Parser Status

### 4.1 Current State (Self-Host)

The current parsers at `blades/kain/src/parser.kn` are minimal stubs:

**`parse_pulse_item`** (line 2954):
```
- Recognizes "pulse" keyword → calls parse_pulse_item
- Parses optional name identifier
- Parses colon + body block
- Stores: [body_idx]
- MISSING: every, interval, jitter parsing
```

**`parse_resonate_item`** (line 2972):
```
- Recognizes "resonate" keyword → calls parse_resonate_item
- Parses optional name identifier
- Parses colon + body block
- Stores: [body_idx]
- MISSING: world.field target path, dampen parsing
```

Neither construct has any clause parsing for:
- Duration values + units
- Optional "jitter" clause for pulse
- Target endpoint (dotted path) for resonate
- Optional "dampen" clause for resonate
- Synthetic name generation for resonate from target path

### 4.2 Required Parser Additions

#### For `parse_pulse_item`:

```
1. Advance past "pulse" token
2. Parse name identifier → name_idx
3. Expect "every" contextual keyword
4. parse_pulse_duration():
   a. Parse integer literal → interval_value
   b. Parse unit identifier ("ms", "ns", "us", "s", "tick", "ticks") → interval_unit_idx
5. If optional "jitter" keyword follows:
   a. Parse another duration: value + unit → jitter_value, jitter_unit_idx
6. Expect ":" colon
7. Parse body block → body_idx
8. Assemble data: [name_idx, interval_value, interval_unit_idx, has_jitter, jitter_value?, jitter_unit_idx?, body_idx]
```

The `parse_pulse_duration` helper:
```kn
fn parse_pulse_duration(st: ParserState) -> ParseResult:
    let value_tok: Token = parser_current(st)
    if value_tok.kind != TOKEN_INT:
        return ParseResult { state: st, node: -1 }
    let value: Int = int_from_string(value_tok.text)
    st = parser_advance(st)
    let unit_tok: Token = parser_current(st)
    if unit_tok.kind != TOKEN_IDENT:
        return ParseResult { state: st, node: -1 }
    let unit_ir: InternResult = parser_intern(st, unit_tok.text)
    st = unit_ir.state
    // Return encoded (value << 32) | unit_index as packed int, or use two slots
```

Alternative: use two data slots per duration (value + unit_index).

#### For `parse_resonate_item`:

```
1. Advance past "resonate" token
2. Parse dotted path (min 2 idents connected by dots):
   a. Parse first ident → push to segments
   b. While next is ".":
      - Advance past "."
      - Parse ident → push to segments
   c. Validate segments length >= 2
3. Generate synthetic name: "resonate__" + segments.join("_")
4. If optional "dampen" keyword follows:
   a. Parse duration: value + unit → dampen_value, dampen_unit_idx
5. Expect ":" colon
6. Parse body block → body_idx
7. Assemble data: [name_idx, seg_count, seg_idx0, seg_idx1, ..., dampen_value, dampen_unit_idx, body_idx]
```

The dotted path parser:
```kn
fn parse_resonate_endpoint(st: ParserState) -> ParserResult:
    var segs: Array<Int> = []
    let first: Token = parser_current(st)
    if first.kind != TOKEN_IDENT:
        return ...error...
    let ir: InternResult = parser_intern(st, first.text)
    segs.push(ir.index)
    st = ir.state
    while parser_check(st, TOKEN_DOT):
        st = parser_advance(st)
        let seg: Token = parser_current(st)
        if seg.kind != TOKEN_IDENT:
            return ...error...
        let ir2: InternResult = parser_intern(st, seg.text)
        segs.push(ir2.index)
        st = ir2.state
    if len(segs) < 2:
        return ...error("resonate target must be World.field")...
    return ParseResult { state: st, node: pack_segments(segs) }
```

---

## 5. Typechecker Plan

### 5.1 Common Validation for Pulse

The typechecker for pulse bodies must:

1. **Validate interval value > 0** — reject zero or negative intervals
2. **Validate interval unit** — must be one of ns, us, ms, s, tick, ticks
3. **Validate jitter if present** — jitter value must be > 0, unit must be valid
4. **Inject body locals** — `pulse_tick: Int`, `pulse_dt_ms: Int`, `pulse_missed: Int` into body scope
5. **Validate body with broad effects** — allow IO, Async, GPU, Reactive, Unsafe, Alloc, Panic
6. **Ensure pulse_tick references are read-only** — pulse_tick should not be assigned to

The current stub at `types.kn:1627-1635` (`check_pulse_resonate_stub`) correctly sets effects to all-permissive:
```kn
effects: EFF_PURE or EFF_IO or EFF_GPU or EFF_ASYNC or EFF_REACTIVE or EFF_UNSAFE or EFF_ALLOC or EFF_PANIC
```

### 5.2 Common Validation for Resonate

The typechecker for resonate handlers must:

1. **Resolve target endpoint** — validate that target is a dotted path resolving to a real world field
2. **Store target type** — record field type for old/new value locals (i64, f64, Bool, etc.)
3. **Validate dampen if present** — dampen value must be >= 0, valid unit
4. **Inject body locals** — `resonate_old_i64: Int`, `resonate_new_i64: Int`, `resonate_fired: Bool`
5. **Self-feedback check** — collect all mutation paths in the body, reject if any directly writes to the trigger field
6. **Validate body with broad effects** — same set as pulse

### 5.3 Self-Feedback Detection

The most critical resonate validation: a handler must not write to its own trigger field.

Implementation plan for `collect_patch_mutation_paths_from_block`:

```kn
fn collect_mutation_targets(node: AstNode) -> Array<String>:
    var targets: Array<String> = []
    collect_mutation_targets_recursive(node, targets)
    return targets

fn collect_mutation_targets_recursive(node: AstNode, targets: Array<String>):
    let kind: Int = node.kind
    if kind == AST_STMT_EXPR:
        let inner: AstNode = ...get child from node.data...
        collect_mutation_targets_recursive(inner, targets)
    elif kind == AST_EXPR_ASSIGN:
        let lhs: AstNode = ...get left-hand side...
        if is_world_field_path(lhs):
            targets.push(...format path as string...)
    elif kind == AST_EXPR_BLOCK:
        for child in node.data:
            collect_mutation_targets_recursive(child, targets)
    elif kind == AST_STMT_LET:
        if has_value(node):
            collect_mutation_targets_recursive(node.value, targets)
    # Also recurse into if/for/while/loop/match branches
```

After collecting the target path (e.g., `"Authority.signal"`), check:
```kn
fn directly_mutates_target(mutation_paths: Array<String>, target_path: String) -> Bool:
    var i: Int = 0
    while i < len(mutation_paths):
        let p: String = mutation_paths[i]
        if p == target_path:
            return true
        if starts_with(p, target_path + "."):
            return true
        i = i + 1
    return false
```

### 5.4 Locals Type Resolution

For `pulse_tick`, `pulse_dt_ms`, `pulse_missed` — these are `Int` (I64).

For `resonate_old_i64`, `resonate_new_i64` — these are `Int` (I64), but the actual type depends on the target field. If the field is `Float`, the codegen uses f64 variants. The typechecker always exposes them as `Int` (I64) for the body; the codegen handles type coercion for old/new.

For `resonate_fired` — `Bool`, always true in the handler.

### 5.5 Effects Computation

Both pulse and resonate bodies accept ALL effects. The current stub is correct:

```kn
effects: EFF_PURE or EFF_IO or EFF_GPU or EFF_ASYNC or EFF_REACTIVE or EFF_UNSAFE or EFF_ALLOC or EFF_PANIC
```

No further effect validation is needed — these are the most permissive constructs in the language.

---

## 6. Codegen Plan

### 6.1 Pulse Codegen

The LLVM codegen must emit **two functions** per pulse:

**Function 1: Body Function**
```
define void @__kain_pulse_body_<name>(i64 %pulse_tick_arg, i64 %pulse_dt_ms_arg, i64 %pulse_missed_arg)
```

- Arguments match the three auto-injected locals
- Body is compiled as a `void(i64, i64, i64)` function
- Arguments are stored in alloca'd slots so body references to pulse_tick/dt/missed work

**Function 2: Fire Wrapper**
```
define void @__kain_pulse_fire_<name>()
```

- Allocates three output slots (pulse.tick.out, pulse.dt.out, pulse.missed.out)
- Calls `@kain_machine_pulse_snapshot(i64 <token>, i64 <interval_ns>, i64 <jitter_ns>, i64* ...)`
- Loads returned values
- Calls body function with loaded values

**Entry Preamble Registration:**

In the main() entry preamble (after world init, before any user code), emit:
```
call i64 @kain_machine_pulse_start(i64 <token>, i64 <interval_ns>, i64 <jitter_ns>, void ()* @__kain_pulse_fire_<name>)
```

**Self-host implementation plan:**

The codegen needs:
1. `compile_pulse_body_textual(name: String, body_ast_idx: Int)` — emits body function with three i64 params
2. `compile_pulse_fire_wrapper(name: String, interval_value: Int, interval_unit_idx: Int, jitter_value: Int, jitter_unit_idx: Int)` — emits fire wrapper with snapshot call
3. Helper `pulse_token(name: String) -> Int` — stable 64-bit hash for slot identification
4. Helper `pulse_duration_to_ns(value: Int, unit: Int) -> Int` — unit encoding → nanoseconds

Duration to nanoseconds lookup:
```kn
fn duration_to_ns(value: Int, unit_encoding: Int) -> Int:
    # unit_encoding: 0=ns, 1=us, 2=ms, 3=s, 4=tick, 5=ticks
    if unit_encoding == 0: return value              # ns
    elif unit_encoding == 1: return value * 1000     # us
    elif unit_encoding == 2: return value * 1000000  # ms
    elif unit_encoding == 3: return value * 1000000000  # s
    else: return value                               # tick/ticks = 1ns (platform-relative)
```

### 6.2 Resonate Codegen

The LLVM codegen emits **one handler function** per resonate:

**Handler Function:**
```
define void @__kain_resonate_<name>(i64 %resonate_old_i64, i64 %resonate_new_i64, i1 %resonate_fired)
```

- Three parameters matching the auto-injected locals
- Body is compiled as `void(i64, i64, bool)`

**World Field Store Integration:**

After every world field store, if that field has a resonate binding:
1. Capture old value before store
2. Emit store instruction
3. Emit guard call: `@abi_resonate_should_fire_i64(i8* %target_ptr, i64 <dampen_ns>, i64 %old_val, i64 %new_val)`
4. Branch on result: if non-zero → call handler + `@abi_resonate_exit(i8* %target_ptr)`

The key data structure is a lookup table mapping field paths to `ResonateBinding`:
```kn
struct ResonateBinding:
    target_path: String
    dampen_ns: Int
    handler_name: String
```

This table is built during codegen initialization (from typed resonate items) and consulted during world field store lowering.

### 6.3 Stack-Safety for Pulse Locals

The three pulse locals (tick, dt_ms, missed) are stack-allocated in the fire wrapper. They must not escape — they are only valid within the pulse body. The codegen should ensure no reference to these locals persists after the body returns. Since the fire wrapper calls the body function directly, and the body function's locals are alloca'd in its own frame, no escaping is possible by construction.

### 6.4 Resonate Reentry Safety

The reentry guard is handled by the native ABI (`abi_resonate_should_fire_common` checks `active_depth`). The codegen must always call `@abi_resonate_exit` after the handler returns to decrement `active_depth`. This is handled by emitting the exit call immediately after the handler call in the fire block.

---

## 7. Runtime Contract

### 7.1 Pulse Runtime Functions

Declared in `runtime.kn` machine stones section (line 213-221):

```kn
push(funcs, rtf("kain_machine_pulse_start", "i64", ["i64", "i64", "i64", "i8*"], RT_MACHINE))
push(funcs, rtf("kain_machine_pulse_snapshot", "void", ["i64", "i64", "i64", "i64*", "i64*", "i64*"], RT_MACHINE))
push(funcs, rtf("kain_machine_pulse_stop_all", "void", [], RT_MACHINE))
push(funcs, rtf("kain_machine_pulse_total_fire_count", "i64", [], RT_MACHINE))
push(funcs, rtf("kain_machine_pulse_tick", "i64", [], RT_MACHINE))
```

| Function | Signature | Purpose |
|----------|-----------|---------|
| `kain_machine_pulse_start` | `i64(i64 token, i64 interval_ns, i64 jitter_ns, i8* fire_fn)` | Register pulse with scheduler |
| `kain_machine_pulse_snapshot` | `void(i64 token, i64 interval_ns, i64 jitter_ns, i64* out_tick, i64* out_dt_ms, i64* out_missed)` | Compute timing values |
| `kain_machine_pulse_stop_all` | `void()` | Stop scheduler and join thread |
| `kain_machine_pulse_total_fire_count` | `i64()` | Total fires across all pulses |
| `kain_machine_pulse_tick` | `i64()` | Monotonic tick of current fire |

### 7.2 Resonate ABI Functions

Declared at the LLVM module level (not through runtime.kn):

```llvm
declare i64 @abi_resonate_should_fire_i64(i8*, i64, i64, i64)
declare i64 @abi_resonate_should_fire_f64(i8*, i64, double, double)
declare void @abi_resonate_exit(i8*)
```

| Function | Purpose |
|----------|---------|
| `abi_resonate_should_fire_i64` | Check if handler should fire for i64 field; returns 1=fire, 0=absorb |
| `abi_resonate_should_fire_f64` | Same for f64 (Float) fields |
| `abi_resonate_exit` | Decrement active_depth after handler completes |

### 7.3 Pulse Contract Metadata

Each pulse produces a contract entry used for runtime introspection:

```kn
struct RuntimePulseContract:
    name: String
    interval: String         # "8ms"
    interval_ms: Int         # 8
    jitter: Option<String>   # Some("1ms"), None
    body_ownership_ops: Bool # has collapse/observe/decay
    body_teleports: Bool     # has teleport expressions
```

### 7.4 Resonate Contract Metadata

```kn
struct RuntimeResonanceContract:
    name: String
    target: String           # "Authority.signal"
    target_type: String      # "Int", "Bool", "Float"
    dampen: String           # "0ms", "32ms"
    dampen_ns: Int           # 0, 32000000
    handler_symbol: String   # "__kain_resonate_resonate__Authority__signal"
```

### 7.5 Capabilities Emitted

From the Rust bootstrap — emitted automatically when constructs are present:

| Condition | Capability | Description |
|-----------|------------|-------------|
| Any pulse exists | `time.pulse` | "Program declares first-class temporal pulse execution beats." |
| Any pulse exists | `time.hardware-timer` | "Pulse contracts can lower to native timer-backed scheduling lanes." |
| Any resonate exists | `state.resonate` | "Program declares compiler-owned shadow-patch reactivity over world state stores." |

---

## 8. Implementation Tasks

### 8.1 Parser Tasks

| # | Task | File | Priority |
|---|------|------|----------|
| P1 | `parse_pulse_duration(st) -> ParseResult` — parse integer + unit, return packed or two-slot encoding | parser.kn | HIGH |
| P2 | `parse_pulse_item` — add every keyword, interval parsing, jitter parsing, body | parser.kn | HIGH |
| P3 | `parse_resonate_endpoint(st) -> ParseResult` — parse dotted path with min 2 segments | parser.kn | HIGH |
| P4 | `parse_resonate_item` — add endpoint parsing, dampen parsing, synthetic name generation | parser.kn | HIGH |
| P5 | Unit validation helpers: `is_valid_duration_unit(unit: String) -> Bool` | parser.kn | MEDIUM |

### 8.2 Typechecker Tasks

| # | Task | File | Priority |
|---|------|------|----------|
| T1 | Extract pulse data from AST node (interval, jitter, body indices) | types.kn | HIGH |
| T2 | Validate pulse interval > 0, valid unit | types.kn | HIGH |
| T3 | Validate pulse jitter if present > 0, valid unit | types.kn | HIGH |
| T4 | Inject pulse_tick/pulse_dt_ms/pulse_missed into body scope before typecheck | types.kn | HIGH |
| T5 | Extract resonate data (target segments, dampen, body) from AST node | types.kn | HIGH |
| T6 | Resolve resonate target field: validate world field exists and type is known | types.kn | HIGH |
| T7 | Inject resonate_old_i64/resonate_new_i64/resonate_fired into body scope | types.kn | HIGH |
| T8 | Self-feedback detection: collect mutation paths in body, reject writes to trigger field | types.kn | CRITICAL |
| T9 | Validate dampen >= 0 and valid unit for resonate | types.kn | MEDIUM |
| T10 | Replace current `check_pulse_resonate_stub` with dedicated `check_pulse` and `check_resonate` | types.kn | HIGH |

### 8.3 Codegen Tasks

| # | Task | File | Priority |
|---|------|------|----------|
| C1 | Pulse duration-to-ns conversion helper | codegen.kn | HIGH |
| C2 | Pulse body function emission (`__kain_pulse_body_<name>`) | codegen.kn | HIGH |
| C3 | Pulse fire wrapper emission (`__kain_pulse_fire_<name>`) with snapshot call | codegen.kn | HIGH |
| C4 | Pulse registration in entry preamble (`kain_machine_pulse_start`) | codegen.kn | HIGH |
| C5 | Resonate handler function emission (`__kain_resonate_<name>`) | codegen.kn | HIGH |
| C6 | Resonate binding table: collect all resonances, build field→binding lookup | codegen.kn | HIGH |
| C7 | World field store lowering: check resonance bindings, emit guard + handler call | codegen.kn | HIGH |
| C8 | Resonate exit call after handler | codegen.kn | HIGH |
| C9 | Stable hash function for pulse tokens | codegen.kn | MEDIUM |

---

## 9. Dependencies

### 9.1 Pulse → L1 (World)

Pulse bodies read and write world state directly. The typechecker must have working world field resolution before pulse can be fully validated. The body expression `Authority.signal = value` requires:

- World name resolution (world `Authority` exists)
- Field resolution (signal is a valid field of Authority)
- Field type checking (value type matches field type)

This is already handled by the existing world typechecking — pulse just needs the body to typecheck in a context where worlds are visible.

### 9.2 Resonate → L1 (World)

Resonate is impossible without worlds. The target endpoint `World.field` must resolve to an actual world state field. Required dependencies:

- World name resolution
- Field name resolution within world
- Field type extraction (for old/new value typing)
- Entangle compatibility (resonate fires before entangle propagation; the codegen emits resonate call before entangle propagation code)

### 9.3 Pulse + Resonate → Codegen Infrastructure

Both constructs require:
- Working LLVM function emission (for body/handler functions)
- Working extern function declarations (for runtime calls)
- Working world field store lowering (for resonate)
- Working entry preamble emission (for pulse start registration)

### 9.4 No Mutual Dependencies

Pulse and resonate are independent of each other. They can be implemented in any order, or simultaneously. They only share the `PulseDuration` type for schedule/dampen intervals.

---

## 10. Test Plan

### 10.1 Unit Tests (kain check)

| Test | Description | Expect |
|------|-------------|--------|
| `pulse_minimal` | `pulse x every 16ms:` with empty body | Types valid |
| `pulse_with_body` | Pulse body writes to world field | Types valid |
| `pulse_with_jitter` | `pulse x every 16ms jitter 2ms:` with body | Types valid |
| `pulse_negative_interval` | `pulse x every -1ms:` | Type error |
| `pulse_bad_unit` | `pulse x every 16xyz:` | Parse error |
| `pulse_zero_interval` | `pulse x every 0ms:` | Type error |
| `pulse_no_every` | `pulse x 16ms:` | Parse error |
| `pulse_body_locals` | Body references `pulse_tick`, `pulse_dt_ms`, `pulse_missed` | Types valid |
| `resonate_minimal` | `resonate World.field dampen 0 ms:` | Types valid |
| `resonate_with_dampen` | `resonate W.f dampen 32ms: body` | Types valid |
| `resonate_no_dampen` | `resonate W.f:` (implied 0ms) | Types valid |
| `resonate_bad_target` | `resonate field:` (single segment) | Parse error |
| `resonate_self_feedback` | Handler writes to own trigger field | Type error |
| `resonate_shadow_write` | Handler writes to different field of same world | Types valid |
| `resonate_body_locals` | References `resonate_old_i64`, `resonate_new_i64`, `resonate_fired` | Types valid |

### 10.2 Compilation Tests (kain build --target llvm)

| Test | Description |
|------|-------------|
| `pulse_compile` | Single pulse compiles to valid LLVM IR |
| `pulse_multi` | Multiple pulses all produce fire wrappers |
| `pulse_no_jitter` | Pulse without jitter — interval_ns only |
| `resonate_compile` | Single resonate produces handler function |
| `resonate_multi` | Multiple resonances on same world, different fields |
| `pulse_resonate_fusion` | Pulse writes to field that has resonate handler; verify both fire wrapper and handler are emitted |

### 10.3 Runtime Tests (kain run --target llvm)

| Test | Description | Verification |
|------|-------------|-------------|
| `pulse_fires` | Pulse with body that increments a world counter | `runtime_machine_pulse_total_fire_count()` > 0 after pause |
| `pulse_tick_monotonic` | Pulse captures pulse_tick in world field | Tick increases between consecutive fires |
| `resonate_fires` | Write to world field with resonate handler | `native_resonate_fire_count()` increments |
| `resonate_absorb` | Rapid writes within dampen window | `native_resonate_absorb_count()` increments |
| `pulse_resonate_chain` | Pulse writes → resonate handler processes | Both telemetry counters increment |
| `resonate_self_feedback_blocked` | Attempt to write to own trigger | Runtime does not re-enter (absorb_count increments, no crash) |

### 10.4 Integration Tests

The existing benchmark cases serve as integration tests:

| File | Constructs |
|------|-----------|
| `benchmark/cases_v2/fusion_chain.kn` | pulse + resonate + world + entangle + patch + law + converge + orchestrate + actor + shatter + teleport + collapse/observe/decay |
| `benchmark/cases_v2/keyword_crucible.kn` | pulse (line ~365), resonate (line ~360) among 108 keywords |
| `blades/test/machine-stones/src/main.kn` | pulse + axiom + shatter + teleport |
| `smoketest/src/semantics/pulse.kn` | pulse + shatter + teleport |
| `smoketest/src/semantics/resonate.kn` | resonate + world + entangle |

### 10.5 Reference: fusion_chain.kn Usage

From `benchmark/cases_v2/fusion_chain.kn` — the canonical L5 usage:

```kn
// Pulse — timed clock driver (line 210-211)
pulse fusion_tick_driver every 8 ms jitter 1 ms:
    FusionAuthority.pulse_ticks = FusionAuthority.pulse_ticks + pulse_tick + 1

// Resonate — reactive tripwire (line 198-204)
resonate FusionAuthority.signal dampen 0 ms:
    FusionAuthority.last_old = resonate_old_i64
    FusionAuthority.last_new = resonate_new_i64
    FusionAuthority.shadow = fusion_signal_pipeline(
        resonate_new_i64 + FusionAuthority.tick,
        FusionAuthority.tick
    )
```

### 10.6 Reference: keyword_crucible.kn Usage (L5 sections)

The keyword crucible at `benchmark/cases_v2/keyword_crucible.kn` exercises pulse (~line 365) and resonate (~line 360) as part of the 108-keyword stress test. These patterns should be replicated and expanded in `L5_temporal.kn` tests.
