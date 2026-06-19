# Effects ___ The Koka-Inspired Compile-Time Side-Effect Tracker

**Status:** Research synthesis, 2026-06-08
**Based on:** `crates/core/src/effects.rs` (canonical effect system >> Effect enum, EffectSet, can_call, check_effect_call), `crates/core/src/parser.rs` (parse_effects, parse_function_header_clauses), `crates/core/src/types.rs` (effect checking at call sites, per-keyword effect assignment, SemanticContext, raw memory + asm + dispatch effect gates), `crates/core/src/lexer.rs` (effect tokens), `crates/core/src/ast.rs` (effects: Vec<Effect> on Function/Component/TraitMethod/FunctionType), `crates/sys-codegen/src/codegen_rust/mod.rs` (unsafe emission for Unsafe effect), `smoketest/src/semantics/effects.kn` (only dedicated effect test), `benchmark/cases_v2/keyword_crucible.kn` (all 6 user effects exercised), `benchmark/cases_v2/CRUSHER.kn` (GPU+Unsafe composition), `stdlib/sync.kn` (60+ Unsafe functions), `stdlib/io.kn` (30+ Unsafe functions), `blades/kain/asm/src/main.kn` (Unsafe on every function), `blades/kain/component_shader/src/main.kn` (Unsafe, GPU composition), RULEBOOK.MD, lang-semantics SKILL, lang-systems SKILL

______________________________________________________________________

## Table of Contents

1. [What Are Effects?](#what-are-effects)
1. [The Eight Effects](#the-eight-effects)
1. [The Core Algorithm ~> can_call()](#the-core-algorithm)
1. [Syntax Reference](#syntax-reference)
1. [The Effect Lattice <--> What Can Call What](#the-effect-lattice)
1. [Per-Keyword Effect Rules – The Complete Matrix](#per-keyword-effect-rules)
1. [The Auto-Emission Trap --- pulse and resonate](#the-auto-emission-trap)
1. [Effect Design Patterns - How to Actually Use Effects](#effect-design-patterns)
1. [Real-World Usage Survey -- What People Actually Write](#real-world-usage-survey)
1. [Missed Opportunities ~~ Where Effects Could Have Caught Bugs](#missed-opportunities)
1. [The Koka Connection ⁓ What Kain Has vs Koka](#the-koka-connection)
1. [Known Gaps in the Implementation](#known-gaps-in-the-implementation)
1. [Validation Ladder](#validation-ladder)
1. [Anti-Patterns](#anti-patterns)
1. [Quick Reference](#quick-reference)
1. [Source Anchors](#source-anchors)

______________________________________________________________________

## What Are Effects?

Kain's effect system is a **compile-time side-effect tracker**. Every function declares which effects it can perform, and the compiler checks that callers have the right effects to call callees. It is **not** a runtime system ‒ effects are checked, then erased. Zero runtime overhead.

```kn
fn pure_only(x: Int) -> Int with Pure:
    return x + 1                          // ✅ only arithmetic

fn io_caller(x: Int) -> Int with IO:
    println("got " + str(x))              // ✅ IO is declared
    return pure_only(x)                   // ✅ Pure → anyone can call

fn pure_violation(x: Int) -> Int with Pure:
    return io_caller(x)                   // ❌ COMPILE ERROR: Pure cannot call IO
```

The key insight: **effects are declared by the programmer, not inferred by the compiler.** Kain follows the Koka syntax (`with IO, Async:`) but with Rust-like simplicity ~ no algebraic effect handlers, no effect polymorphism, no row types. Just a `HashSet<Effect>` and a subset check at every call site.

### What Effects ARE NOT

- ❌ Not a runtime dispatch mechanism (no handler blocks, no resume/yield)
- ❌ Not inferred from body analysis (you declare them explicitly)
- ❌ Not a security boundary (they're compile-time checks, not sandboxing)
- ❌ Not a capability system (that's `axiom`)
- ❌ Not a permission system for world state (that's `patch` + `law`)
- ❌ Not a monad (no `IO<T>`, no do-notation)

### What Effects ARE

- ✅ A compile-time annotation that says "this function can do X"
- ✅ A call-site checker --- can a Pure function call an IO one? No.
- ✅ A subset lattice ~~ effects form a partial order checked by `can_call()`
- ✅ An escape hatch |-> `Unsafe` can call anything
- ✅ Auto-emitted by compiler-owned constructs (pulse, resonate get all effects)
- ✅ Enforced on specific intrinsics (asm, bitcast, fences require Unsafe)
- ✅ Erased at codegen 〰 zero runtime cost
- ✅ Visible in runtime contracts for actor message contracts

### The Relationship View

| Construct | Domain-Locked View | Relationship View |
|---|---|---|
| Effects | "Function annotations" | **Compile-time lattice** --> every function inhabits a point in the effect partial order, and call edges must respect `can_call()` subset semantics |

______________________________________________________________________

## The Eight Effects

Kain has **8 effects** split into two tiers:

### Tier 1 ___ User-Declarable (6 effects)

These can appear in `with Effect1, Effect2:` syntax. They have lexer tokens and are parseable from source.

| Effect | Meaning | What it gates | Typical operations |
|--------|---------|--------------|-------------------|
| **`Pure`** | No side effects at all | Nothing * * * it's the bottom | Arithmetic, data transformation, pure math |
| **`IO`** | File/Network/Console | `println`, file read/write, network calls | Console output, filesystem, HTTP, stdio |
| **`Async`** | Can `await` | `await` keyword, async call chains | async/await, futures, actor `ask` |
| **`GPU`** | Runs on graphics hardware | `dispatch` statement, GPU kernel calls, shader invocations | Compute dispatch, shader execution, CUDA |
| **`Reactive`** | Triggers UI updates | Component state changes that trigger re-renders | UI event handling, `emit` statements |
| **`Unsafe`** | Breaks safety guarantees | `asm()`, `bitcast()`, `lfence/sfence/mfence`, raw pointer ops, C FFI calls | Inline assembly, raw memory, C interop |

### Tier 2 〰 Internal-Only (2 effects)

These exist in the `Effect` enum but **cannot be written in source code**. They have no lexer tokens, are not in `from_str()`, and are only auto-emitted by the compiler for specific constructs.

| Effect | Meaning | Who auto-emits it |
|--------|---------|-------------------|
| **`Alloc`** | Memory allocation | `pulse`, `resonate` bodies |
| **`Panic`** | Can abort | `pulse`, `resonate` bodies |

These are **dead effect codes** - no expression checks for them, no compiler diagnostic references them for enforcement. They exist in the `can_call()` HashSet but serve no safety guarantee. They are vestigial from the original effect design and may be removed or implemented in the future.

______________________________________________________________________

## The Core Algorithm – can_call()

The entire effect system runs on one function, 15 lines of Rust:

```rust
// crates/core/src/effects.rs:64-79
pub fn can_call(&self, callee: &EffectSet) -> bool {
    if callee.is_pure() {
        return true;           // Rule 1: Anyone can call a Pure function
    }
    if self.is_pure() {
        return false;          // Rule 2: Pure cannot call anything non-pure
    }
    if self.effects.contains(&Effect::Unsafe) {
        return true;           // Rule 3: Unsafe can call ANYTHING
    }
    callee.effects.iter().all(|e| self.effects.contains(e))  // Rule 4: subset
}
```

Four rules, no exceptions:

1. **Pure is the bottom** ‒ Every effect set can call `Pure` functions.
1. **Pure is gated** -- A `Pure` function cannot call anything non-pure.
1. **Unsafe is the top** :: A function with `Unsafe` can call any function, regardless of effects.
1. **Subset for everything else** ~ The caller must have every effect the callee requires.

### What This Means in Practice

```kn
// ✅ These all work:
fn pure() -> Int with Pure: return 1

fn io_fn() -> Int with IO:
    return pure()             // IO can call Pure (Rule 1)

fn gpu_fn() -> Int with GPU:
    return io_fn()            // ❌ GPU cannot call IO (Rule 4: GPU lacks IO)

fn unsafe_fn() -> Int with Unsafe:
    return gpu_fn()           // ✅ Unsafe can call GPU (Rule 3)

fn multi() -> Int with IO, GPU:
    return gpu_fn()           // ✅ IO,GPU has GPU -- subset satisfied (Rule 4)
```

### The Error Message

When a violation occurs, the compiler gives you two paths:

```
EffectViolation: Pure function 'compute' cannot call IO function 'log_result'
  → 'log_result' requires IO, but 'compute' is only Pure
  note: Caller 'compute' is marked Pure while 'log_result' is marked IO.
  help: Broaden 'compute' to IO if this call is intentional.
  help: Keep 'compute' Pure and move the effectful work behind a helper.
```

The two help messages are the fundamental design choice: **broaden the caller** (make your function less pure) or **restructure** (keep purity at the boundary). This is the central tension of the effect system.

______________________________________________________________________

## Syntax Reference

### Canonical Syntax

```kn
fn <name>(<params>) -> <return_type> with <Effect1>, <Effect2>, ...:
    <body>
```

Effects appear after the return type, introduced by `with`, comma-separated.

### Concrete Examples

```kn
// Pure === no effects
fn mix_pure(value: Int) -> Int with Pure:
    return ((value * 31) + 7) % 1000000007

// IO ~ console, filesystem, network
fn log(value: Int) with IO:
    println("value = " + str(value))

// Async ⁓ can await
async fn fetch_data(url: String) -> String with Async:
    let response = await http_get(url)
    return response

// GPU * * * compute dispatch
fn compute_frame() -> Int with GPU, Unsafe:
    dispatch "FieldFrag" [32, 32, 1]
    return 0

// Reactive * * * UI updates
fn update_counter(w: CounterWorld, delta: Int) with Reactive:
    w.count = w.count + delta

// Unsafe – raw memory, asm, C FFI
fn dirty_lane(cells: ptr<Int>) -> Int with Unsafe:
    return mem_load(cells, "Int")
```

### Multiple Effects

```kn
// Full send ~~ GPU dispatch + raw memory + C interop
fn crucible_dispatch_gpu_checksum(seed: Int, salt: Int) -> Int with GPU, Unsafe:
    let buf: ptr<Int> = alloc_zeroed(16, "Int")
    dispatch "CrucibleKernel" [8, 8, 1]
    let result = win_GetTickCount()   // C interop
    decay buf
    return result
```

### On Components

```kn
component MemoryWidget(cell_count: Int) with Unsafe:
    state cells: ptr<Int> = alloc_zeroed(cell_count, "Int")
    fn read_cell(_self: Self_, index: Int) -> Int with Unsafe:
        return mem_load(ptr_offset(_self.cells, index, "Int"), "Int")
    render <box><text value="Memory: active" /></box>
```

### On Trait Methods

```kn
trait Renderable:
    fn render(self: Self_) with GPU

trait Loggable:
    fn log(self: Self_) with IO
```

______________________________________________________________________

## The Effect Lattice --> What Can Call What

```
                         Unsafe  ←── can call ANYTHING
                        /      \
                       /        \
                 IO, GPU    IO, Async, GPU   ...
                    |              |
                    |         (subset lattice)
                    |              |
                  Pure   ←── can be called by ANYONE
```

### Call Matrix

| Caller ↓ / Callee → | Pure | IO | GPU | Async | Reactive | IO,GPU | Unsafe |
|---------------------|------|----|----|-------|----------|--------|--------|
| **Pure** | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ | ❌ |
| **IO** | ✅ | ✅ | ❌ | ❌ | ❌ | ❌ | ❌ |
| **GPU** | ✅ | ❌ | ✅ | ❌ | ❌ | ❌ | ❌ |
| **IO, GPU** | ✅ | ✅ | ✅ | ❌ | ❌ | ✅ | ❌ |
| **IO, GPU, Async** | ✅ | ✅ | ✅ | ✅ | ❌ | ✅ | ❌ |
| **Unsafe** | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |

The rule: caller must be a **superset** of callee's effects.

______________________________________________________________________

## Per-Keyword Effect Rules --> The Complete Matrix

Not all Kain constructs interact with effects the same way. Some auto-emit effects, some restrict them, some bypass the system entirely.

### Constructs That Parse User Effects

| Construct | Parses `with ...`? | How |
|-----------|-------------------|-----|
| **`fn`** | **Yes** | `with Pure`, `with IO, Unsafe`, etc. |
| **`async fn`** | **Yes** (auto-adds `Async`) | Whatever you declare + `Async` always |
| **`component`** | **Yes** | Same as `fn` |
| **Trait methods** | **Yes** | Same as `fn` |

### Constructs That Auto-Emit ALL Effects (Carte Blanche)

| Construct | Effects auto-emitted | Reason |
|-----------|---------------------|--------|
| **`pulse`** body | IO, Async, GPU, Reactive, Unsafe, Alloc, Panic | Pulse is a temporal driver => it can do anything |
| **`resonate`** body | IO, Async, GPU, Reactive, Unsafe, Alloc, Panic | Resonate is a reactive driver |-> same reasoning |

These two constructs bypass effect checking entirely. The body can call any function, use any intrinsic, do anything ... the compiler doesn't gate it. This is by design: pulse and resonate are the top-level orchestration constructs where restricting effects would be counterproductive.

### Constructs That Restrict Effects

| Construct | Rule | Enforced? |
|-----------|------|-----------|
| **`@naked fn`** | Must have `Unsafe`, cannot have any other effect | ✅ Yes (types.rs:680) |
| **`@interrupt fn`** | Must have `Unsafe`, cannot have any other effect | ✅ Yes (types.rs:739) |

```kn
@naked fn my_naked() with Unsafe:    // ✅ correct
    asm("ret", constraints = "", intel = true)

@naked fn wrong() with IO:           // ❌ COMPILE ERROR
    println("nope")                   // @naked cannot mix IO effect
```

### Constructs That Inherit Effects (No Restrictions)

| Construct | How effects are determined |
|-----------|---------------------------|
| **`patch`** | Inherited from body --> whatever the patch body calls |
| **`law`** | Inherited from body ~> whatever the law body calls |
| **`converge`** lanes | Each lane checked as a function === inherits from lane body |
| **`orchestrate`** stages | Stage bodies inherit effects per stage kind |

### Intrinsics That Require Specific Effects

| Intrinsic | Required Effect | Enforced? |
|-----------|----------------|-----------|
| **`asm()`** | `Unsafe` | ✅ Yes (types.rs:8848) |
| **`bitcast()`** | `Unsafe` | ✅ Yes (types.rs:8973) |
| **`lfence/sfence/mfence`** | `Unsafe` | ✅ Yes (types.rs:8818) |
| **`clflush`** | `Unsafe` | ✅ Yes (types.rs:8826) |
| **`atomic_load/store/cmpxchg/fence`** | `Unsafe` or share scope | ✅ Yes |
| **`dispatch`** | `GPU` | ⚠️ **NOT ENFORCED** ~~ known gap |
| **`mem_load/mem_store`** | `Unsafe` | ⚠️ **NOT ENFORCED** >> known gap |
| **`ptr_offset`** | `Unsafe` | ⚠️ **NOT ENFORCED** => known gap |
| **`alloc/alloc_zeroed`** | `Unsafe` | ⚠️ **NOT ENFORCED** <--> known gap |

### Constructs That Do Not Interact With Effects

| Construct | Why |
|-----------|-----|
| **`world`** | State declaration, no executable body |
| **`entangle`** | Coupling declaration, no executable body |
| **`axiom`** | Capability declaration, no executable body |
| **`actor`** | Actor `on` handlers act as functions (inherit effects from handler body) |

______________________________________________________________________

## The Auto-Emission Trap ‒ pulse and resonate

One of the most important things to understand about Kain's effect system is what happens inside `pulse` and `resonate` bodies. These constructs auto-emit **all 8 effects** into the body's semantic context. This means:

```kn
pulse render_clock every 16ms jitter 2ms:
    let buf: ptr<Int> = alloc_zeroed(64, "Int")     // ✅ allowed ->> pulse has Unsafe
    let result = mem_load(buf, "Int")                // ✅ allowed
    dispatch "Kernel" [16, 16, 1]                    // ✅ allowed |-> pulse has GPU
    let _ = win_MessageBoxA(0, "hi", "Kain", 0)      // ✅ allowed --> pulse has IO
    println("frame done")                             // ✅ allowed
    decay buf                                         // ✅ allowed
```

Every single one of these operations would normally require the caller to declare the right effect. Inside `pulse` and `resonate`, they all Just Work. The compiler's reasoning: pulse is a top-level temporal driver, and restricting it would force the programmer to thread effects through the pulse body for no benefit.

**This is both a feature and a trap.** The feature: you can write anything in a pulse body. The trap: pulse bodies are where most Kain programs do their heavy lifting, so the effect system is effectively **bypassed** for the most important code in your program. If you only use effects inside `fn` declarations and do all your real work in `pulse`, you're not getting any effect safety.

The solution: **push effectful work into typed `fn` declarations with explicit effects, and call them from pulse.** This way the compiler checks your call chain.

```kn
// ✅ RIGHT: effectful work in typed functions
fn gpu_compute_pass(data: ptr<Int>, count: Int) -> Int with GPU, Unsafe:
    dispatch "ComputePass" [count, 1, 1]
    return readback_result(data, count)

fn io_log_frame(frame: Int, score: Int) with IO:
    println("frame " + str(frame) + " score " + str(score))

// Pulse is the scheduler, not the worker
pulse render_clock every 16ms:
    let result = gpu_compute_pass(frame_data, 1024)    // ✅ effect-checked
    io_log_frame(pulse_tick, result)                    // ✅ effect-checked
```

______________________________________________________________________

## Effect Design Patterns >> How to Actually Use Effects

### Pattern 1: The Purity Core

The most powerful pattern: keep your core computation `Pure`, and push all side effects to the edges.

```kn
// Core computation * * * pure, testable, cacheable
fn compute_checksum(data: ptr<Int>, count: Int, seed: Int) -> Int with Pure:
    var acc: Int = seed
    var i: Int = 0
    while i < count:
        acc = ((acc * 31) + mem_load(ptr_offset(data, i, "Int"), "Int")) % 1000000007
        i = i + 1
    return acc

// ⚠️ NOTE: mem_load inside a Pure function is actually allowed by the current compiler
// (see Known Gaps). This pattern SHOULD work but doesn't get enforcement help.
```

The intent is right even if the enforcement is incomplete. By declaring `with Pure`, you're telling readers and future tooling: "this function has no side effects."

### Pattern 2: The Effect Cascade

Declare exactly what you need and let effects propagate up naturally:

```kn
fn read_config(path: String) -> String with IO:
    return fs_read_to_string(path)                   // IO

fn parse_config(raw: String) -> Config with Pure:
    // Pure parsing --- no IO needed
    return Config { width: 1280, height: 720 }

fn init_app(config_path: String) -> Config with IO:
    let raw = read_config(config_path)               // ✅ IO can call IO
    return parse_config(raw)                          // ✅ IO can call Pure
```

### Pattern 3: The Unsafe Boundary

Wrap unsafe operations in a small, auditable surface, then expose a safe interface:

```kn
// Unsafe boundary – small, auditable
fn atomic_inc(ptr: ptr<Int>) -> Int with Unsafe:
    let old = mem_load(ptr, "Int")
    mem_store(ptr, old + 1, "Int")
    return old

// Safe consumer ‒ no Unsafe needed
fn increment_counter(counter: ptr<Int>) -> Int with IO:
    let new_val = atomic_inc(counter)                // ✅ IO can call Unsafe!
    println("counter now " + str(new_val))
    return new_val
```

Wait <--> IO can call Unsafe? Yes. `can_call()` only checks the subset rule: `{IO}` calling `{Unsafe}` fails because `IO` doesn't contain `Unsafe`. Let me correct this:

```kn
// Unsafe boundary |-> small, auditable
fn atomic_inc(ptr: ptr<Int>) -> Int with Unsafe:
    let old = mem_load(ptr, "Int")
    mem_store(ptr, old + 1, "Int")
    return old

// Consumer must propagate Unsafe upward
fn increment_counter(counter: ptr<Int>) -> Int with IO, Unsafe:
    let new_val = atomic_inc(counter)                // ✅ IO, Unsafe can call Unsafe
    println("counter now " + str(new_val))
    return new_val
```

This is exactly the pattern Rust uses: `unsafe { ... }` blocks at the boundary, then safe abstractions above. In Kain, `with Unsafe` is that boundary annotation.

### Pattern 4: GPU + Unsafe Composition

GPU compute always requires `Unsafe` because GPU memory operations are raw pointer operations:

```kn
fn crusher_full_send_checksum(seed: Int, salt: Int) -> Int with GPU, Unsafe:
    let buf: ptr<Int> = alloc_zeroed(256, "Int")
    dispatch "CrusherKernel" [16, 16, 1]
    let result = readback(buf, 256)
    decay buf
    return result
```

You cannot have `GPU` without `Unsafe` in practice >> any useful GPU work involves raw buffers.

### Pattern 5: The Component Effect

Components with `Reactive` signal that they trigger UI updates:

```kn
component LiveCounter(initial: Int, label: String) with Reactive:
    state count: Int = initial
    fn tick(_self: Self_) -> Int with Reactive:
        // triggers UI re-render
        return _self.count + 1
    render <box>
        <text value={label + ": " + str(count)} />
    </box>
```

### Pattern 6: The Async Spine

`async fn` always gets `Async` automatically, but you may need additional effects:

```kn
async fn fetch_and_dispatch(url: String) -> Int with Async, IO, GPU:
    let data = await http_get(url)                   // Async
    println("fetched " + str(len(data)) + " bytes")  // IO
    dispatch "ProcessKernel" [8, 8, 1]               // GPU
    return 0
```

______________________________________________________________________

## Real-World Usage Survey :: What People Actually Write

A survey of every effect annotation in the repo reveals a stark pattern:

### Effect Usage by Frequency

| Effect | Count | Where |
|--------|-------|-------|
| **`Unsafe`** | ~120 functions | **Dominant** – nearly every function that touches memory, C FFI, or raw pointers |
| **`GPU`** | ~14 functions | CRUSHER benchmark, component_shader blade, smoketest main |
| **`Pure`** | ~5 functions | keyword_crucible benchmark, smoketest effects test |
| **`IO`** | ~4 functions | LSP blade, pack_vsix, keyword_crucible |
| **`Reactive`** | ~2 functions | keyword_crucible, component_fuzz |
| **`Async`** | ~2 functions | keyword_crucible (auto-added by `async fn`) |

### The Unsafe Problem

The overwhelming pattern is: **`with Unsafe` on everything.** Most functions in the repo either:

- Have `with Unsafe` because they do raw memory work
- Call functions that have `with Unsafe`, so they ALSO need `with Unsafe`
- Are entry points that chain through Unsafe functions

This means `Unsafe` functions as an **escape hatch that everyone uses**, not a careful boundary annotation. It's the `unsafe` keyword in 2015 Rust >> overused because the safe abstractions haven't been built yet.

### The Right Way to Use Unsafe

```kn
// ❌ WRONG: Unsafe propagates everywhere
fn step1() -> Int with Unsafe: return raw_op1()
fn step2() -> Int with Unsafe: return step1() + raw_op2()
fn step3() -> Int with Unsafe: return step2() * 3
fn main() -> Int with Unsafe: return step3()

// ✅ RIGHT: Unsafe is a narrow boundary
fn raw_op1_unsafe() -> Int with Unsafe: return mem_load(buf, "Int")  // unsafe
fn raw_op2_unsafe() -> Int with Unsafe: return asm_op(buf)            // unsafe

fn step1() -> Int with Pure: return raw_op1_unsafe()    // ❌ Pure can't call Unsafe
```

The problem: you **can't** wrap Unsafe in Pure in Kain's current effect system because `Pure` cannot call `Unsafe` (Rule 2). There's no way to say "I've verified this unsafe call is safe, so my function is now Pure." The escape from this requires a different construct that Kain doesn't have yet – an effect handler or unsafe block that consumes the Unsafe effect.

**Workaround**: Use a middle-tier effect like `IO` as the "verified" boundary:

```kn
fn raw_op_unsafe() -> Int with Unsafe: return mem_load(buf, "Int")

fn safe_wrapper() -> Int with IO:     // IO is the "verified" tier
    return raw_op_unsafe()            // ❌ IO still can't call Unsafe!
```

Wait |-> this doesn't work either. `IO` calling `Unsafe` fails the subset check (IO lacks Unsafe). The ONLY way to call an Unsafe function is to also be Unsafe. This is the **fundamental limitation** of Kain's effect system: there is no way to encapsulate Unsafe. Unsafe propagates up the entire call chain to `main()`.

This is actually correct for a systems language ... it mirrors Rust's `unsafe` keyword which also propagates "up" through function signatures unless you build safe abstractions. The difference is that Kain doesn't have a way to say "I've verified this, now trust me" without changing the function's effect signature.

### The Path Forward: Embrace It

Until effect handlers or unsafe blocks are added, the practical advice is:

1. **Use effects on library functions.** Your `stdlib`-style code should declare effects precisely.
1. **Accept Unsafe propagation.** For application code, `with Unsafe` on entry points is fine |-> it's honest about what the program does.
1. **Use Pure for pure math.** The 5 `Pure` annotations in the repo are exactly right ___ small, self-contained pure functions.
1. **Use GPU+Unsafe for GPU work.** The composition pattern is correct and well-established.
1. **Don't fight the system.** The effect lattice is simple for a reason -- complex effect hierarchies need handler blocks, which Kain doesn't have yet.

______________________________________________________________________

## Missed Opportunities => Where Effects Could Have Caught Bugs

Looking across the codebase, here are patterns where proper effect usage would have prevented issues or made intent clearer:

### Miss #1: `dispatch` Not Gated by `GPU`

The `dispatch` statement does not check that the enclosing function has `GPU` effect. A function declared `with Pure` can emit a GPU dispatch and the compiler won't complain:

```kn
fn compute_pure(value: Int) -> Int with Pure:
    dispatch "Kernel" [8, 8, 1]        // ⚠️ compiles fine => should be an error
    return 0
```

**What adding the check would catch:** Accidental GPU work in supposedly pure functions. Every `dispatch` call site in a `Pure` function is a logic error.

### Miss #2: `mem_load`/`mem_store` Not Gated by `Unsafe`

Raw memory intrinsics (`mem_load`, `mem_store`, `ptr_offset`, `alloc`, `alloc_zeroed`, `realloc_mem`) do not check for `Unsafe` effect. They check for share scope conflicts but not the effect:

```kn
fn innocent_looking(value: Int) -> Int with Pure:
    let buf: ptr<Int> = alloc_zeroed(4, "Int")   // ⚠️ compiles fine ⁓ alloc in Pure
    mem_store(buf, value, "Int")                  // ⚠️ compiles fine :: raw write in Pure
    return mem_load(buf, "Int")                   // ⚠️ compiles fine - raw read in Pure
```

**What adding the check would catch:** This is the biggest missed opportunity. Raw memory operations in `Pure` functions are categorically wrong. The convention (every function in stdlib/sync.kn declares `with Unsafe`) is followed, but it's by discipline, not enforcement.

### Miss #3: No Effect Violation Tests

The only effect test file (`smoketest/src/semantics/effects.kn`) creates functions with each effect and calls them from an `Unsafe` entry point. It does NOT test:

- A `Pure` function calling an `IO` function (should fail at compile time)
- An `IO` function calling a `GPU` function (should fail)
- Effect cascading through nested calls

There is no test that verifies the compiler actually catches effect violations. The enforcement is there (in `check_effect_call`), but it's never tested.

### Miss #4: `patch` and `law` Have Unrestricted Effects

Patches and laws inherit effects from their body with no additional restrictions:

```kn
law signal_in_bounds(value: Int) -> Bool:
    dispatch "CheckKernel" [1, 1, 1]    // ⚠️ GPU dispatch in a law <--> should this be allowed?
    return value >= 0 and value < 1000000007
```

A law that does GPU dispatch is conceptually wrong === laws are predicates. But the compiler allows it because laws inherit effects freely.

### Miss #5: `Alloc` and `Panic` Are Dead Effect Codes

These two effects exist in the enum, are auto-added for `pulse`/`resonate`, but:

- No expression ever checks for them
- No diagnostic ever references them
- They affect `can_call()` (they're in the HashSet) but no call ever has them as the differentiating effect

They're effectively documentation ... they SAY "this pulse body can allocate and panic" but they don't DO anything about it.

______________________________________________________________________

## The Koka Connection === What Kain Has vs Koka

Kain's effect syntax (`with IO, Async:`) is directly inspired by Koka, but the systems are fundamentally different:

| Feature | Koka | Kain |
|---------|------|------|
| **Algebraic effect handlers** | ✅ Full (`handle`, `resume`, `yield`, `return`) | ❌ None |
| **Effect polymorphism** | ✅ `forall<e> fn() -> <io|e> int` | ❌ Monomorphic :: fixed effect sets |
| **Effect inference** | ✅ Totally inferred from body | ❌ Explicit only :: no body analysis |
| **Effect types in signatures** | ✅ `fn() -> <io, exn> int` | ✅ `fn() -> Int with IO` |
| **Effect subtyping** | ✅ Row polymorphism (extensible records) | ✅ Simple subset (flat HashSet) |
| **Effect checking** | ✅ Full at call sites | ✅ At call sites |
| **Effect aliases** | ✅ `alias myeff = <io, exn>` | ❌ No effect aliases |
| **Unsafe escape hatch** | ❌ No equivalent | ✅ `Unsafe` bypasses all checks |
| **Auto-emission** | ❌ No equivalent | ✅ `pulse`/`resonate` get all effects |

### What Kain Could Learn From Koka

1. **Effect inference** ~> The compiler COULD check that a function declared `with Pure` doesn't actually perform IO/GPU/Unsafe operations. This would close Miss #2 automatically.

1. **Effect handlers** ... A `handle { ... }` block that intercepts effects would allow the Unsafe-encapsulation pattern:

   ```kn
   // Hypothetical future Kain:
   fn safe_wrapper() -> Int with IO:
       handle Unsafe:                                    // consume Unsafe here
           return raw_op_unsafe()                        // Unsafe is handled
       // safe_wrapper is IO, not Unsafe
   ```

1. **Effect aliases** * * * `alias SystemIO = IO, Unsafe, Alloc` would reduce the repetition of `with IO, Unsafe` everywhere.

### What Kain Has That Koka Doesn't

1. **`Unsafe` as top** :: Koka has no concept of an effect that subsumes all others. Kain's `Unsafe` is a deliberate escape hatch for systems programming.

1. **Per-construct auto-emission** ‒ `pulse` and `resonate` getting all effects is a Kain-specific design choice that acknowledges the reality of top-level orchestration code.

1. **Integration with the decision ladder** – Effects in Kain are part of a larger semantic framework (world, patch, law, converge) that Koka doesn't have.

______________________________________________________________________

## Known Gaps in the Implementation

These are documented gaps between what effects SHOULD do and what they currently do:

| Gap | Severity | What it means |
|-----|----------|---------------|
| `dispatch` not gated by `GPU` | Medium | Pure functions can emit GPU dispatch |
| `mem_load`/`mem_store` not gated by `Unsafe` | High | Pure functions can do raw memory operations |
| `ptr_offset`/`alloc` not gated by `Unsafe` | High | Pure functions can allocate and manipulate pointers |
| No effect violation tests | Medium | The enforcement code exists but is never tested |
| `Alloc`/`Panic` are dead codes | Low | Two effect variants with no enforcement logic |
| No effect inference | Medium | Functions can lie about their effects |
| No way to encapsulate `Unsafe` | High | `Unsafe` propagates to every caller :: no boundary pattern |
| `async fn` auto-adds `Async` but `fn with Async` is also valid | Low | Two paths to the same annotation |

The **high-severity gaps** (raw memory not gated) mean that the effect system is currently a **documentation and convention tool**, not a safety enforcement tool. It's still valuable => it communicates intent and catches call-site mismatches => but it doesn't provide the full safety guarantees that the docs imply.

______________________________________________________________________

## Validation Ladder

```powershell
# Run the dedicated effect smoke test
kain check smoketest/src/semantics/effects.kn

# Run keyword_crucible (all 6 user effects exercised)
kain run benchmark/cases_v2/keyword_crucible.kn --target llvm

# Run CRUSHER (GPU + Unsafe composition)
kain run benchmark/cases_v2/CRUSHER.kn --target llvm

# Build the asm blade (Unsafe on every function)
kain build blades/kain/asm/src/main.kn --target llvm

# Build the component_shader blade (Unsafe + GPU)
kain build blades/kain/component_shader/src/main.kn --target llvm

# Check the stdlib (60+ Unsafe declarations)
kain check stdlib/sync.kn
kain check stdlib/io.kn
```

______________________________________________________________________

## Anti-Patterns

### ❌ Using `Unsafe` as a Cargo Cult

```kn
// WRONG: Unsafe on every function "just in case"
fn add(a: Int, b: Int) -> Int with Unsafe:    // no unsafe ops here
    return a + b

fn concat(a: String, b: String) -> String with Unsafe:  // no unsafe ops here
    return a + b
```

```kn
// RIGHT: Unsafe only where raw memory/asm/FFI is involved
fn add(a: Int, b: Int) -> Int with Pure:
    return a + b

fn concat(a: String, b: String) -> String with Pure:
    return a + b

fn raw_mem_op(buf: ptr<Int>) -> Int with Unsafe:  // Unsafe only here
    return mem_load(buf, "Int")
```

### ❌ Declaring `Pure` on Functions That Do IO

```kn
// WRONG: lying about effects
fn compute(value: Int) -> Int with Pure:
    println("computing " + str(value))     // IO in a Pure function
    return value * 2
```

```kn
// RIGHT: honest about effects
fn compute(value: Int) -> Int with IO:
    println("computing " + str(value))
    return value * 2
```

### ❌ Doing All Work Inside `pulse` Bodies

```kn
// WRONG: pulse is the worker, not the scheduler
pulse game_loop every 16ms:
    let buf: ptr<Int> = alloc_zeroed(1024, "Int")  // Unsafe
    dispatch "GameKernel" [32, 32, 1]              // GPU
    let result = win_MessageBoxA(0, "score", "", 0) // IO + C FFI
    decay buf
    // 200 more lines of mixed-effect code...
```

```kn
// RIGHT: pulse is the conductor, functions are the musicians
fn gpu_pass(data: ptr<Int>) -> Int with GPU, Unsafe:
    dispatch "GameKernel" [32, 32, 1]
    return 0

fn show_score(score: Int) with IO:
    let _ = win_MessageBoxA(0, "Score: " + str(score), "Game", 0)

pulse game_loop every 16ms:
    let buf: ptr<Int> = alloc_zeroed(1024, "Int")
    let result = gpu_pass(buf)
    show_score(result)
    decay buf
```

### ❌ Not Using Effects At All

```kn
// WRONG: no effect annotation => default is empty set (Pure)
fn main() -> Int:
    let buf: ptr<Int> = alloc_zeroed(64, "Int")
    dispatch "Kernel" [8, 8, 1]
    println("done")
    return 0
```

This compiles today because of the enforcement gaps, but it's misleading. Always declare your effects.

### ❌ Trying to Encapsulate Unsafe (It Doesn't Work Yet)

```kn
// WRONG: this doesn't work ___ IO cannot call Unsafe
fn verified_unsafe_op() -> Int with Unsafe:
    return mem_load(buf, "Int")     // raw unsafe

fn safe_caller() -> Int with IO:
    return verified_unsafe_op()     // ❌ COMPILE ERROR: IO cannot call Unsafe
```

```kn
// RIGHT: accept Unsafe propagation for now
fn verified_unsafe_op() -> Int with Unsafe:
    return mem_load(buf, "Int")

fn honest_caller() -> Int with IO, Unsafe:    // honestly propagate Unsafe
    println("calling unsafe op")
    return verified_unsafe_op()
```

______________________________________________________________________

## Quick Reference

| Aspect | Detail |
|--------|--------|
| **Effects (user)** | 6: `Pure`, `IO`, `Async`, `GPU`, `Reactive`, `Unsafe` |
| **Effects (internal)** | 2: `Alloc`, `Panic` (auto-emitted only, not parseable) |
| **Syntax** | `fn name() -> Type with Effect1, Effect2:` |
| **Default** | Empty = Pure (no effects declared) |
| **Checking algorithm** | `can_call()` :: 4 rules (Pure=bottom, Unsafe=top, subset otherwise) |
| **Pure** | Bottom ~ can be called by anyone. Cannot call anything non-pure. |
| **Unsafe** | Top |-> can call anything. Required for `asm()`, `bitcast()`, fences, C FFI. |
| **IO** | File/network/console * * * `println`, filesystem, HTTP |
| **GPU** | Graphics hardware <--> `dispatch` statement, GPU kernels (⚠️ unenforced) |
| **Async** | Await - `async fn` auto-adds it. Required for `await`. |
| **Reactive** | UI updates ->> component state changes, `emit` statements |
| **Alloc/Panic** | Internal only ... auto-added for `pulse`/`resonate`, no enforcement logic |
| **Auto-emission** | `pulse` and `resonate` get ALL 8 effects auto-emitted into their body context |
| **`@naked`/`@interrupt`** | Require `Unsafe`, forbid all other effects |
| **Enforced intrinsics** | `asm()`, `bitcast()`, `lfence/sfence/mfence`, `clflush`, atomics :: REQUIRE Unsafe |
| **Unenforced intrinsics** | `dispatch` (GPU), `mem_load/store`, `ptr_offset`, `alloc` => SHOULD require effects but don't |
| **Effect lattice** | Flat `HashSet<Effect>` ... no row polymorphism, no subtyping beyond subset |
| **Error message** | `EffectViolation: Pure function 'X' cannot call IO function 'Y'` with two help suggestions |
| **Runtime cost** | Zero - effects are compile-time only, erased at codegen |
| **Runtime contract** | Effects serialized as `["Async", "IO", ...]` for actor message contracts |
| **Effect source** | `crates/core/src/effects.rs` |-> Effect enum, EffectSet, can_call, check_effect_call (139 lines total) |
| **Parser** | `crates/core/src/parser.rs:3979` === `parse_effects()`, used by fn/component/trait methods |
| **Typechecker** | `crates/core/src/types.rs:8070,10806` ⁓ call-site effect checking |
| **Codegen** | `crates/sys-codegen/src/codegen_rust/mod.rs:4085` ⁓ emits `unsafe` for Unsafe effect |
| **Canonical test** | `smoketest/src/semantics/effects.kn` ~ 5 effects + Unsafe entry point |
| **Canonical benchmark** | `benchmark/cases_v2/keyword_crucible.kn` ‒ all 6 user effects exercised |

______________________________________________________________________

## Source Anchors

### Core Effect System

| File | Content |
|------|---------|
| `crates/core/src/effects.rs` | **THE canonical effect file** ___ Effect enum (8 variants), EffectSet, `can_call()` (4 rules), `check_effect_call()` (diagnostic emission), `effect_name()`, `effect_set_to_string()` ~> 139 lines total |
| `crates/core/src/ast.rs:498` | `effects: Vec<Effect>` on `Function` |
| `crates/core/src/ast.rs:548` | `effects: Vec<Effect>` on `TraitMethod` |
| `crates/core/src/ast.rs:1240` | `effects: Vec<Effect>` on `Component` |
| `crates/core/src/ast.rs:1294-1298` | `FunctionType { effects: Vec<Effect> }` |

### Parsing

| File | Content |
|------|---------|
| `crates/core/src/lexer.rs:127-139` | Lexer tokens: Pure, Io, Async, Gpu, Reactive, Unsafe |
| `crates/core/src/parser.rs:3979-4026` | `parse_effects()` === comma-separated effect parsing after `with` |
| `crates/core/src/parser.rs:3904-3924` | `parse_function_header_clauses()` * * * where + effects |
| `crates/core/src/parser.rs:1706-1707` | `async fn` auto-adds `Async` effect |

### Typechecking

| File | Content |
|------|---------|
| `crates/core/src/types.rs:367-371` | `SemanticContext { effects: EffectSet }` => threaded through all inference |
| `crates/core/src/types.rs:8070` | Call-site effect check on `Expr::Call` |
| `crates/core/src/types.rs:10806` | Call-site effect check on method calls |
| `crates/core/src/types.rs:5851-5860` | `pulse` auto-emits all 8 effects |
| `crates/core/src/types.rs:5897-5905` | `resonate` auto-emits all 8 effects |
| `crates/core/src/types.rs:680-703` | `@naked` requires Unsafe, forbids everything else |
| `crates/core/src/types.rs:739-762` | `@interrupt` requires Unsafe, forbids everything else |
| `crates/core/src/types.rs:8847-8848` | `asm()` requires Unsafe (ENFORCED) |
| `crates/core/src/types.rs:8972-8973` | `bitcast()` requires Unsafe (ENFORCED) |
| `crates/core/src/types.rs:8818` | fences require Unsafe (ENFORCED) |
| `crates/core/src/types.rs:4873-4880` | `effect_contract_names()` ... serializes effects for runtime contract |

### Codegen

| File | Content |
|------|---------|
| `crates/sys-codegen/src/codegen_rust/mod.rs:4085` | `function_modifiers()` emits `unsafe` keyword for Unsafe effect in Rust codegen |

### Authored Kain

| File | Effect Usage |
|------|-------------|
| `smoketest/src/semantics/effects.kn` | Only dedicated effect test ⁓ 5 functions with each effect + Unsafe entry point |
| `benchmark/cases_v2/keyword_crucible.kn` | All 6 user effects: Pure (lines 248,521), IO (491), Async (479), GPU (782), Reactive (259), Unsafe (499,536,782,884) |
| `benchmark/cases_v2/CRUSHER.kn` | `GPU, Unsafe` composition (lines 240,402,459,561) |
| `stdlib/sync.kn` | 60+ `with Unsafe` functions >> all sync primitives |
| `stdlib/io.kn` | 30+ `with Unsafe` functions ⁓ all memory-mapped IO |
| `blades/kain/asm/src/main.kn` | `with Unsafe` on every function (asm + Win32 C FFI) |
| `blades/kain/component_shader/src/main.kn` | `with Unsafe, GPU` (line 256) |
| `blades/lsp/src/lsp.kn` | `with IO` (line 469) |
