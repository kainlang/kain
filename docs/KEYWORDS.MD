# Kain Keyword Dictionary

> **111 keywords · 58 hard lexer · 51 contextual · 2 operator aliases**
> **Date:** 2026-06-08
> **Source:** CATALOG.MD, `crates/core/src/lexer.rs`, `crates/core/src/parser.rs`, `crates/core/src/ast.rs`
> **Exercised:** `benchmark/cases_v2/keyword_crucible.kn` (108/110 — `emit` and `receive` reserved but no parser rules)
> **Definitive Reference:** `benchmark/cases_v2/fusion_chain.kn` (all 7 layers), `benchmark/cases_v2/keyword_crucible.kn`
> **Rule Book:** `docs/RULEBOOK.md`

______________________________________________________________________

## Quick Index

| Keyword | Kind | Group | Layer | Doc |
|---------|------|-------|-------|-----|
| `actor` | Contextual | Systems — L7 | L7 | `ACTOR.MD` |
| `after` | Contextual | Stage Graph — L4 | L4 | `ORCHESTRATE.MD` |
| `and` | Operator Alias | Operator Aliases | L0 | — |
| `as` | Contextual | Core Control & Binding | L0 | — |
| `async` | Contextual | Effects | L0 | `EFFECTS.MD` |
| `Async` | Hard Lexer | Effects | L0 | `EFFECTS.MD` |
| `await` | Contextual | Core Control & Binding | L0 | — |
| `axiom` | Contextual | Machine Stones — L6 | L6 | `AXIOM.MD` |
| `break` | Hard Lexer | Core Control & Binding | L0 | — |
| `by` | Contextual | Stage Graph — L4 | L4 | `ORCHESTRATE.MD` |
| `capability` | Contextual | Dispatch — L3 | L3 | `CONVERGE.MD` |
| `collapse` | Contextual | Systems — L7 | L7 | `OWNERSHIP.MD` |
| `component` | Contextual | Components & UI | UI | `COMPONENT.MD` |
| `comptime` | Contextual | Compile-Time | L0 | — |
| `compute` | Contextual | GPU & Shaders | L0 | `SHADER_GPU.MD` |
| `const` | Hard Lexer | Core Control & Binding | L0 | — |
| `continue` | Hard Lexer | Core Control & Binding | L0 | — |
| `converge` | Contextual | Dispatch — L3 | L3 | `CONVERGE.MD` |
| `dampen` | Contextual | Temporal — L5 | L5 | `RESONATE.MD` |
| `decay` | Contextual | Systems — L7 | L7 | `OWNERSHIP.MD` |
| `defer` | Contextual | Core Control & Binding | L0 | — |
| `deps` | Contextual | Stage Graph — L4 | L4 | `ORCHESTRATE.MD` |
| `dispatch` | Contextual | GPU & Shaders | L0 | `SHADER_GPU.MD` |
| `else` | Hard Lexer | Core Control & Binding | L0 | — |
| `elif` | Hard Lexer | Core Control & Binding | L0 | — |
| `emit` | Hard Lexer | Reserved | — | — |
| `entangle` | Contextual | State Authority — L1 | L1 | `ENTANGLE.MD` |
| `enum` | Hard Lexer | Types, Modules & Visibility | L0 | — |
| `every` | Contextual | Temporal — L5 | L5 | `PULSE.MD` |
| `fallback` | Contextual | Stage Graph — L4 | L4 | `ORCHESTRATE.MD` |
| `false` | Hard Lexer | Built-in Literals | L0 | — |
| `fanout` | Contextual | Systems — L7 | L7 | `OWNERSHIP.MD` |
| `fast` | Contextual | Dispatch — L3 | L3 | `CONVERGE.MD` |
| `fn` | Hard Lexer | Core Control & Binding | L0 | — |
| `for` | Hard Lexer | Core Control & Binding | L0 | — |
| `fragment` | Contextual | GPU & Shaders | L0 | `SHADER_GPU.MD` |
| `from` | Contextual | Foreign & Import | L0 | `C.MD`, `PYTHON.MD` |
| `GPU` | Hard Lexer | Effects | L0 | `EFFECTS.MD`, `SHADER_GPU.MD` |
| `guarantee` | Contextual | Machine Stones — L6 | L6 | `AXIOM.MD` |
| `guarded` | Contextual | Stage Graph — L4 | L4 | `ORCHESTRATE.MD` |
| `if` | Hard Lexer | Core Control & Binding | L0 | — |
| `impl` | Hard Lexer | Types, Modules & Visibility | L0 | — |
| `import` | Contextual | Foreign & Import | L0 | `PYTHON.MD` |
| `in` | Hard Lexer | Core Control & Binding | L0 | — |
| `include` | Contextual | Foreign & Import | L0 | `C.MD` |
| `IO` | Hard Lexer | Effects | L0 | `EFFECTS.MD` |
| `jitter` | Contextual | Temporal — L5 | L5 | `PULSE.MD` |
| `law` | Contextual | State Integrity — L2 | L2 | `LAW.MD` |
| `let` | Hard Lexer | Core Control & Binding | L0 | — |
| `loop` | Hard Lexer | Core Control & Binding | L0 | — |
| `macro` | Contextual | Compile-Time | L0 | — |
| `match` | Hard Lexer | Core Control & Binding | L0 | — |
| `mod` | Hard Lexer | Types, Modules & Visibility | L0 | — |
| `mut` | Hard Lexer | Core Control & Binding | L0 | — |
| `native_ui` | Contextual | State Authority — L1 | L1 | `COMPONENT.MD` |
| `none` | Hard Lexer | Built-in Literals | L0 | — |
| `observe` | Contextual | Systems — L7 | L7 | `OWNERSHIP.MD` |
| `on` | Hard Lexer | Systems — L7 | L7 | `ACTOR.MD` |
| `or` | Operator Alias | Operator Aliases | L0 | — |
| `orchestrate` | Contextual | Stage Graph — L4 | L4 | `ORCHESTRATE.MD` |
| `patch` | Contextual | State Integrity — L2 | L2 | `PATCH.MD` |
| `policy` | Contextual | Stage Graph — L4 | L4 | `ORCHESTRATE.MD` |
| `pub` | Hard Lexer | Types, Modules & Visibility | L0 | — |
| `pulse` | Contextual | Temporal — L5 | L5 | `PULSE.MD` |
| `Pure` | Hard Lexer | Effects | L0 | `EFFECTS.MD` |
| `random` | Contextual | Dispatch — L3 | L3 | `CONVERGE.MD` |
| `Reactive` | Hard Lexer | Effects | L0 | `EFFECTS.MD` |
| `receive` | Hard Lexer | Reserved | — | — |
| `render` | Contextual | Components & UI | UI | `COMPONENT.MD` |
| `requires` | Contextual | Stage Graph — L4 | L4 | `ORCHESTRATE.MD` |
| `residency` | Contextual | Stage Graph — L4 | L4 | `ORCHESTRATE.MD` |
| `resonate` | Contextual | Temporal — L5 | L5 | `RESONATE.MD` |
| `return` | Hard Lexer | Core Control & Binding | L0 | — |
| `self` | Hard Lexer | Types, Modules & Visibility | L0 | — |
| `Self` | Hard Lexer | Types, Modules & Visibility | L0 | — |
| `send` | Hard Lexer | Systems — L7 | L7 | `ACTOR.MD` |
| `shader` | Contextual | GPU & Shaders | L0 | `SHADER_GPU.MD` |
| `share` | Contextual | Systems — L7 | L7 | `OWNERSHIP.MD` |
| `shatter` | Contextual | Machine Stones — L6 | L6 | `SHATTER.MD` |
| `single_writer` | Contextual | State Authority — L1 | L1 | `ENTANGLE.MD` |
| `spec` | Contextual | Dispatch — L3 | L3 | `CONVERGE.MD` |
| `spawn` | Hard Lexer | Systems — L7 | L7 | `ACTOR.MD` |
| `stage` | Contextual | Stage Graph — L4 | L4 | `ORCHESTRATE.MD` |
| `state` | Contextual | Systems — L7 / Components & UI | L7 | `ACTOR.MD`, `COMPONENT.MD` |
| `struct` | Hard Lexer | Types, Modules & Visibility | L0 | — |
| `surface` | Contextual | State Authority — L1 | L1 | `COMPONENT.MD`, `WORLD.MD` |
| `teleport` | Contextual | Machine Stones — L6 | L6 | `TELEPORT.MD` |
| `test` | Contextual | Testing | L0 | — |
| `to` | Contextual | Machine Stones — L6 | L6 | `TELEPORT.MD` |
| `trait` | Hard Lexer | Types, Modules & Visibility | L0 | — |
| `transfer` | Contextual | Stage Graph — L4 | L4 | `ORCHESTRATE.MD` |
| `true` | Hard Lexer | Built-in Literals | L0 | — |
| `type` | Hard Lexer | Types, Modules & Visibility | L0 | — |
| `ue5` | Contextual | State Authority — L1 | L1 | — |
| `uniform` | Contextual | GPU & Shaders | L0 | `SHADER_GPU.MD` |
| `Unsafe` | Hard Lexer | Effects | L0 | `EFFECTS.MD` |
| `use` | Hard Lexer | Types, Modules & Visibility | L0 | — |
| `var` | Hard Lexer | Core Control & Binding | L0 | — |
| `verify` | Contextual | Dispatch — L3 | L3 | `CONVERGE.MD` |
| `vertex` | Contextual | GPU & Shaders | L0 | `SHADER_GPU.MD` |
| `via` | Contextual | Machine Stones — L6 | L6 | `TELEPORT.MD` |
| `viewport3d` | Contextual | State Authority — L1 | L1 | — |
| `web` | Contextual | State Authority — L1 | L1 | — |
| `weak` | Contextual | Systems — L7 | L7 | `OWNERSHIP.MD` |
| `when` | Contextual | Dispatch — L3 / Machine Stones — L6 | L3/L6 | `CONVERGE.MD`, `AXIOM.MD` |
| `where` | Contextual | Types, Modules & Visibility | L0 | — |
| `while` | Hard Lexer | Core Control & Binding | L0 | — |
| `with` | Hard Lexer | Core Control & Binding | L0 | — |
| `workgroup` | Contextual | GPU & Shaders | L0 | `SHADER_GPU.MD` |
| `world` | Contextual | State Authority — L1 | L1 | `WORLD.MD` |

______________________________________________________________________

## Legend

| Kind | Meaning |
|------|---------|
| **Hard Lexer** | Dedicated `TokenKind` entry in `lexer.rs` — keyword in ALL positions |
| **Contextual** | Parser recognizes it as keyword only in specific syntactic positions; may be used as identifier elsewhere |
| **Operator Alias** | Word-form alias for a symbolic operator (`&&` → `and`, `\|\|` → `or`) |
| **Reserved** | TokenKind exists in lexer but NO parser production rule — using it causes a parse error |

______________________________________________________________________

## Decision Ladder Map

```
                    ┌──────────────────────────────────┐
                    │ "C/OS boundary?"   → include     │
                    │ "Python interop?"  → import      │
                    │ "GPU kernel?"      → shader      │
                    │ "UI component?"    → component   │
                    ├──────────────────────────────────┤
LAYER 7: SYSTEMS    │ actor · collapse · observe · decay
                    │ spawn · send · on · share · fanout
                    ├──────────────────────────────────┤
LAYER 6: MACHINE    │ axiom · shatter · teleport
  STONES            │ guarantee · via · to
                    ├──────────────────────────────────┤
LAYER 5: TEMPORAL   │ pulse · resonate · every · jitter
                    │ dampen
                    ├──────────────────────────────────┤
LAYER 4: STAGE      │ orchestrate · stage · after · deps
  GRAPH             │ residency · transfer · guarded · by
                    │ requires · policy · fallback
                    ├──────────────────────────────────┤
LAYER 3: DISPATCH   │ converge · spec · fast · verify
                    │ random · when · target · capability
                    ├──────────────────────────────────┤
LAYER 2: STATE      │ patch · law
  INTEGRITY         │
                    ├──────────────────────────────────┤
LAYER 1: STATE      │ world · entangle · single_writer
  AUTHORITY         │ surface · native_ui · viewport3d
                    │ web · ue5
                    ├──────────────────────────────────┤
LAYER 0: PLAIN      │ fn · let · mut · var · const
  CODE              │ if · else · elif · match · for
                    │ while · loop · break · continue
                    │ return · defer · await · in · with
                    │ as · type · struct · enum · trait
                    │ impl · pub · mod · use · self
                    │ Self · true · false · none
                    │ Pure · IO · async · Async · GPU
                    │ Reactive · Unsafe
                    │ and · or
```

______________________________________________________________________

## DOC REFERENCE MAP

| Group | Deep-Dive Doc |
|-------|--------------|
| State Authority — L1 | `WORLD.MD`, `ENTANGLE.MD` |
| State Integrity — L2 | `LAW.MD`, `PATCH.MD` |
| Dispatch — L3 | `CONVERGE.MD` |
| Stage Graph — L4 | `ORCHESTRATE.MD` |
| Temporal — L5 | `PULSE.MD`, `RESONATE.MD` |
| Machine Stones — L6 | `AXIOM.MD`, `SHATTER.MD`, `TELEPORT.MD` |
| Systems — L7 | `ACTOR.MD`, `OWNERSHIP.MD` |
| Effects | `EFFECTS.MD` |
| Components & UI | `COMPONENT.MD` |
| GPU & Shaders | `SHADER_GPU.MD` |
| Foreign & Import | `C.MD`, `PYTHON.MD` |
| Rule Book | `RULEBOOK.md` |

______________________________________________________________________

# GROUP 1: Core Control & Binding (20 keywords)

## `fn`

**Kind:** Hard Lexer
**Category:** Core Control & Binding
**Layer:** L0

Function declaration. The universal callable unit in Kain. Functions are first-class values, support generics with `where` clauses, and declare their effect capabilities via `with Effect`. The function body is a block expression — the trailing expression is the return value. `fn` is the fallback in the decision ladder: for any problem that fits a higher layer (patch, converge, orchestrate, etc.), use that instead.

```kn
fn add(a: Int, b: Int) -> Int with Pure:
    return a + b

fn generic_sum<T: Numeric>(a: T, b: T) -> T with Pure where T: Default:
    return a + b
```

> **See:** `docs/RULEBOOK.md` (Layer 0), `docs/EFFECTS.MD`, `patch`, `converge`

______________________________________________________________________

## `let`

**Kind:** Hard Lexer
**Category:** Core Control & Binding
**Layer:** L0

Immutable variable binding. Introduces a named slot initialized with a value. The binding is read-only for its scope. Contrast with `mut` (mutable) and `var` (rebindable). `let` is the default choice for temporary values.

```kn
let x: Int = 42
let msg = "hello"  // type inferred
let result = compute(value)
```

> **See:** `mut`, `var`, `const`

______________________________________________________________________

## `mut`

**Kind:** Hard Lexer
**Category:** Core Control & Binding
**Layer:** L0

Mutable variable qualifier. Applied to a `let` binding to allow mutation of the bound value through assignment. Unlike `var` (which rebinds the name), `mut` mutates the value in place.

```kn
let mut counter: Int = 0
counter = counter + 1
```

> **See:** `let`, `var`, `const`

______________________________________________________________________

## `var`

**Kind:** Hard Lexer
**Category:** Core Control & Binding
**Layer:** L0

Rebindable variable declaration. Like `let` but allows the variable name to be rebound to a new value (not mutation, but name re-binding). Used in older-style Kain code and in some benchmarks for explicit rebind semantics.

```kn
var acc: Int = 0
var index: Int = 0
while index < 10:
    acc = acc + index
    index = index + 1
```

> **See:** `let`, `mut`, `const`

______________________________________________________________________

## `const`

**Kind:** Hard Lexer
**Category:** Core Control & Binding
**Layer:** L0

Compile-time constant declaration. Binds a name to a compile-time-evaluated value. Constants are inlined at use sites. They must be typed and initialized with a constant expression (no runtime computation).

```kn
const MODULUS: Int = 1000000007
const CELL_COUNT: Int = 8
const LABEL: String = "keyword-crucible"
```

> **See:** `let`, `comptime`

______________________________________________________________________

## `if`

**Kind:** Hard Lexer
**Category:** Core Control & Binding
**Layer:** L0

Conditional branching. `if` is an expression in Kain — it returns a value. Works with `elif` (else-if) and `else` branches. The conditional must be a `Bool`.

```kn
let mode: CrucibleMode = if val == 0:
    CrucibleMode::Scalar
elif val == 1:
    CrucibleMode::Vectorized
else:
    CrucibleMode::Hybrid
```

> **See:** `elif`, `else`, `match`

______________________________________________________________________

## `else`

**Kind:** Hard Lexer
**Category:** Core Control & Binding
**Layer:** L0

Fallback branch for `if` and `elif` chains. Evaluated when all preceding conditions are false. Required when `if` is used as an expression and the type must be determined.

```kn
let score = if v < 0:
    0
else:
    v
```

> **See:** `if`, `elif`

______________________________________________________________________

## `elif`

**Kind:** Hard Lexer
**Category:** Core Control & Binding
**Layer:** L0

Else-if conditional. Chains an additional condition after an `if` or `elif`. Short for "else if" — each `elif` is checked only when all prior conditions are false.

```kn
if v < 10:
    "small"
elif v < 100:
    "medium"
elif v < 1000:
    "large"
else:
    "huge"
```

> **See:** `if`, `else`

______________________________________________________________________

## `match`

**Kind:** Hard Lexer
**Category:** Core Control & Binding
**Layer:** L0

Pattern matching expression. Dispatches on the variant of an enum or the shape of a value. Each arm is `Pattern => Expression`. Match is exhaustive — all variants must be covered. Returns a value.

```kn
fn describe(mode: CrucibleMode) -> String with Pure:
    match mode:
        CrucibleMode::Scalar     => "scalar"
        CrucibleMode::Vectorized => "vectorized"
        CrucibleMode::Parallel   => "parallel"
        CrucibleMode::Hybrid     => "hybrid"
```

> **See:** `if`, `enum`

______________________________________________________________________

## `for`

**Kind:** Hard Lexer
**Category:** Core Control & Binding
**Layer:** L0

Iteration over a sequence. `for x in sequence:` iterates over each element, binding it to `x`. Supports range syntax (`0..N`), arrays, and any iterable type. Also used in JSX for rendering lists (`for item in list:` inside `render <...>`).

```kn
for lane in [1, 2, 3, 4]:
    acc = (acc + lane) % modulus

for i in 0..10:
    result = result * i
```

> **See:** `in`, `while`, `loop`, `component`

______________________________________________________________________

## `while`

**Kind:** Hard Lexer
**Category:** Core Control & Binding
**Layer:** L0

Conditional loop. Repeatedly executes the body as long as the `Bool` condition is true. Supports `break` and `continue` for flow control.

```kn
var i: Int = 0
while i < iterations:
    acc = (acc + i) % modulus
    i = i + 1
```

> **See:** `for`, `loop`, `break`, `continue`

______________________________________________________________________

## `loop`

**Kind:** Hard Lexer
**Category:** Core Control & Binding
**Layer:** L0

Infinite loop with break value. `loop:` executes a block until a `break` with an optional value exits it. The `loop` expression evaluates to the value passed to `break`. Useful when you need a loop that yields a computed value on exit.

```kn
let result: Int = loop:
    defer cleanup()
    if done:
        break final_value
    step()
```

> **See:** `while`, `break`, `continue`, `defer`

______________________________________________________________________

## `break`

**Kind:** Hard Lexer
**Category:** Core Control & Binding
**Layer:** L0

Loop exit with optional value. Exits the innermost enclosing `loop`, `while`, or `for` loop. Can carry a value that becomes the result of a `loop` expression. Also works with labeled loops for breaking out of nested loops.

```kn
let found: Int = loop:
    if condition:
        break result
    iterate()

while true:
    if done: break
```

> **See:** `loop`, `while`, `for`, `continue`

______________________________________________________________________

## `continue`

**Kind:** Hard Lexer
**Category:** Core Control & Binding
**Layer:** L0

Skip to next loop iteration. Jumps to the loop condition check (for `while`) or the next element (for `for`). Any pending `defer` expressions still execute before the jump.

```kn
while i < iterations:
    if i % 5 == 0:
        defer acc = (acc + i) % modulus
        i = i + 1
        continue
    process(i)
```

> **See:** `break`, `loop`, `while`, `for`, `defer`

______________________________________________________________________

## `return`

**Kind:** Hard Lexer
**Category:** Core Control & Binding
**Layer:** L0

Early exit from a function with an optional value. Returns from the current function immediately. Any pending `defer` expressions run in LIFO order before control transfers out.

```kn
fn clamp(v: Int, lo: Int, hi: Int) -> Int:
    if v < lo: return lo
    if v > hi: return hi
    return v
```

> **See:** `defer`, `fn`

______________________________________________________________________

## `defer`

**Kind:** Contextual
**Category:** Core Control & Binding
**Layer:** L0

Block-scoped LIFO cleanup statement. The deferred expression runs when the current block exits — by fallthrough, `return`, `break`, or `continue`. Multiple defers execute in reverse declaration order (stack discipline). The deferred value is evaluated eagerly; execution is deferred.

```kn
fn process() -> Int:
    let buf: ptr<Int> = alloc_zeroed(N, "Int")
    defer decay buf
    collapse buf:
        mem_store(buf, 42, "Int")
        0
    let v = observe buf:
        mem_load(buf, "Int")
    return v  // defer runs here: decay buf
```

> **See:** `collapse`, `decay`, `return`, `break`, `continue`

______________________________________________________________________

## `await`

**Kind:** Contextual
**Category:** Core Control & Binding
**Layer:** L0

Await a future value. Blocks the current execution context until an `async` function's returned future completes. Used in both synchronous contexts (blocking wait) and async contexts (yield). The awaited expression must be an `Async` future.

```kn
async fn fetch_value(id: Int) -> Int with Async:
    return id * 2

fn use_async(v: Int) -> Int:
    let fut = fetch_value(v)
    return await fut
```

> **See:** `async`, `Async`, `EFFECTS.MD`

______________________________________________________________________

## `in`

**Kind:** Hard Lexer
**Category:** Core Control & Binding
**Layer:** L0

Iteration introducer for `for` loops. Separates the loop variable from the iterable expression: `for var in iterable:`. Also used in JSX for list rendering inside `component`.

```kn
for worker in 0..WORKER_COUNT:
    results[worker] = compute(worker, data)

// JSX usage
render <list>
    for item in items:
        <text value={item} />
</list>
```

> **See:** `for`, `component`

______________________________________________________________________

## `with`

**Kind:** Hard Lexer
**Category:** Core Control & Binding
**Layer:** L0

Effect annotation introducer. Used on `fn` declarations to specify effect capabilities: `fn name() -> Type with Effect1, Effect2`. Also used in generic `where` clauses (less common) and in older code for effect lists.

```kn
fn pure_compute(v: Int) -> Int with Pure:
    return v * 2

fn side_effect() with IO:
    println("hello")

fn unsafe_read(p: ptr<Int>) -> Int with Unsafe:
    return mem_load(p, "Int")
```

> **See:** `Pure`, `IO`, `Unsafe`, `Async`, `GPU`, `Reactive`, `EFFECTS.MD`

______________________________________________________________________

## `as`

**Kind:** Contextual
**Category:** Core Control & Binding
**Layer:** L0

Type cast and alias introducer. Performs explicit numeric type conversion (`val as Int`), and introduces import/include aliases (`include <stdio.h> as libc`, `import json as py_json`).

```kn
let clamped: Int = index as Int  // numeric cast
let narrow: UInt = wide as UInt

// Import alias
include <stdio.h> as libc
import json as py_json
```

> **See:** `include`, `import`, `type`

______________________________________________________________________

# GROUP 2: Types, Modules & Visibility (10 keywords)

## `type`

**Kind:** Hard Lexer
**Category:** Types, Modules & Visibility
**Layer:** L0

Type alias declaration. Creates a synonym for an existing type. Does not create a new type — just a shorthand. Useful for giving semantic meaning to primitive types.

```kn
type CrucibleScore = Int
type CrucibleFlag = Bool
type Distance = Float
```

> **See:** `struct`, `enum`

______________________________________________________________________

## `struct`

**Kind:** Hard Lexer
**Category:** Types, Modules & Visibility
**Layer:** L0

Product type declaration. Defines a named struct with typed fields. Structs are the primary data aggregation construct. Supports `impl` blocks for methods. Contrast with `shatter struct` for SoA layout.

```kn
struct CruciblePacket:
    id:      Int
    payload: Int
    phase:   Int
    hot:     Bool

let pkt = CruciblePacket {
    id:      1,
    payload: 42,
    phase:   7,
    hot:     true
}
```

> **See:** `enum`, `impl`, `trait`, `shatter`

______________________________________________________________________

## `enum`

**Kind:** Hard Lexer
**Category:** Types, Modules & Visibility
**Layer:** L0

Sum type (tagged union) declaration. Defines an enumeration with named variants. Variants can carry data. Match exhaustively covers all variants.

```kn
enum CrucibleMode:
    Scalar
    Vectorized
    Parallel
    Hybrid

enum Option<T>:
    Some(value: T)
    None
```

> **See:** `struct`, `match`, `type`

______________________________________________________________________

## `trait`

**Kind:** Hard Lexer
**Category:** Types, Modules & Visibility
**Layer:** L0

Interface/abstract base type declaration. Defines a set of method signatures that implementing types must provide. Traits can have default implementations. Methods use `_self: Self_` as the receiver convention.

```kn
trait CrucibleMetric:
    fn score(_self: Self_) -> Int:
        return 0  // default implementation

trait CrucibleStable:
    fn bias(_self: Self_) -> Int:
        return 1
```

> **See:** `impl`, `struct`, `where`

______________________________________________________________________

## `impl`

**Kind:** Hard Lexer
**Category:** Types, Modules & Visibility
**Layer:** L0

Implementation block. Adds methods to a struct (`impl Type:`) or implements a trait for a type (`impl Trait for Type:`). Methods receive `_self: Self_` and access fields through it.

```kn
impl CruciblePacket:
    fn weighted(_self: Self_) -> Int:
        let base = (_self.id * 13) + (_self.payload * 7) + (_self.phase * 3)
        return base % MODULUS

impl CrucibleMetric for CruciblePacket:
    fn score(_self: Self_) -> Int:
        return ((_self.id * 11) + _self.payload + 17) % MODULUS
```

> **See:** `trait`, `struct`, `Self`

______________________________________________________________________

## `pub`

**Kind:** Hard Lexer
**Category:** Types, Modules & Visibility
**Layer:** L0

Visibility modifier. Makes an item (function, struct, module, etc.) publicly accessible outside its parent module. Without `pub`, items are module-private.

```kn
pub fn exportable_fn() -> Int:
    return 42

pub mod crucible_util:
    pub fn clamp(v: Int, lo: Int, hi: Int) -> Int:
        if v < lo: return lo
        if v > hi: return hi
        return v
```

> **See:** `mod`, `use`

______________________________________________________________________

## `mod`

**Kind:** Hard Lexer
**Category:** Types, Modules & Visibility
**Layer:** L0

Module declaration. Defines a named namespace containing functions, types, and submodules. Modules can be `pub` to export their contents. Supports inline block syntax.

```kn
pub mod crucible_util:
    pub fn clamp(v: Int, lo: Int, hi: Int) -> Int:
        if v < lo: return lo
        if v > hi: return hi
        return v
```

> **See:** `pub`, `use`, `self`

______________________________________________________________________

## `use`

**Kind:** Hard Lexer
**Category:** Types, Modules & Visibility
**Layer:** L0

Import path declaration. Brings symbols from modules, crates, or C FFI into scope. Supports `use std::module::Symbol`, `use c::function`, and `use crate::path`.

```kn
use std::runtime
use std::actor
use std::intent
use std::machine
```

> **See:** `mod`, `include`, `import`, `C.MD`

______________________________________________________________________

## `self`

**Kind:** Hard Lexer
**Category:** Types, Modules & Visibility
**Layer:** L0

Current module reference and receiver convention baseline. In `use` paths, `self` refers to the current module. In method definitions, `Self_` (with trailing underscore) is the receiver type; the convention is `_self: Self_` as the method receiver parameter name.

```kn
// In use paths — current module
use crucible_util::self

// Method receiver (convention: `_self: Self_`)
fn score(_self: Self_) -> Int:
    return _self.field
```

> **See:** `Self`, `use`, `impl`

______________________________________________________________________

## `Self`

**Kind:** Hard Lexer
**Category:** Types, Modules & Visibility
**Layer:** L0

Receiver type alias inside `impl` blocks. Refers to the type being implemented. Always appears as `Self_` (with trailing underscore) in Kain code to distinguish it from Rust's `Self` convention. The receiver parameter is written `_self: Self_`.

```kn
impl CruciblePacket:
    fn weighted(_self: Self_) -> Int:
        let base = (_self.id * 13) + (_self.payload * 7) + (_self.phase * 3)
        return base % MODULUS
```

> **See:** `self`, `impl`, `trait`

______________________________________________________________________

# GROUP 3: Built-in Literals (3 keywords)

## `true`

**Kind:** Hard Lexer
**Category:** Built-in Literals
**Layer:** L0

Boolean true literal. One of the two `Bool` values. Used in conditions, assignments, and return values.

```kn
let flag: Bool = true
if flag == true:
    proceed()

fn always_valid() -> Bool:
    return true
```

> **See:** `false`, `none`

______________________________________________________________________

## `false`

**Kind:** Hard Lexer
**Category:** Built-in Literals
**Layer:** L0

Boolean false literal. The complement of `true`. Represents a false condition or negative result.

```kn
let done: Bool = false
if ok == false:
    return error

fn is_invalid(v: Int) -> Bool:
    return v < 0
```

> **See:** `true`, `none`

______________________________________________________________________

## `none`

**Kind:** Hard Lexer
**Category:** Built-in Literals
**Layer:** L0

Null/nil literal for `Option<T>` and optional types. Represents the absence of a value. Can be compared with `==`. The `none` literal is the safe alternative to null pointers.

```kn
let nullable: Option<Int> = none
if nullable == none:
    acc = (acc + 1) % modulus
```

> **See:** `true`, `false`, `Option`

______________________________________________________________________

# GROUP 4: Effects (7 keywords)

## `Pure`

**Kind:** Hard Lexer
**Category:** Effects
**Layer:** L0

Pure effect annotation. Declares that a function has no side effects — no IO, no memory mutation, no raw pointer access, no GPU dispatch, no async operations. The compiler can reason about purity for optimization and reordering. A `Pure` function depends only on its inputs.

```kn
fn add(a: Int, b: Int) -> Int with Pure:
    return a + b

fn fold(values: [Int], modulus: Int) -> Int with Pure:
    var acc: Int = 0
    for v in values:
        acc = (acc + v) % modulus
    return acc
```

> **See:** `IO`, `Unsafe`, `Async`, `GPU`, `Reactive`, `EFFECTS.MD`

______________________________________________________________________

## `IO`

**Kind:** Hard Lexer
**Category:** Effects
**Layer:** L0

Input/Output effect annotation. Declares that a function performs I/O operations: console output, filesystem access, network communication, or any external system interaction. Functions with `IO` cannot be called from `Pure` contexts without explicit effect handling.

```kn
fn log_signal(value: Int) with IO:
    let msg = "signal=" + str(value)
    println(msg)

fn read_config(path: String) -> String with IO:
    return read_file(path)
```

> **See:** `Pure`, `Unsafe`, `EFFECTS.MD`

______________________________________________________________________

## `async`

**Kind:** Contextual
**Category:** Effects
**Layer:** L0

Asynchronous function modifier. Declares that a function returns a future and may suspend execution. When combined with `Async` effect, the function body can use `await` to wait on other futures. The caller receives a future value that can be resolved with `await`.

```kn
async fn crucible_async_mix(v: Int, seed: Int) -> Int with Async:
    let result = crucible_mix(v, seed)
    return result

fn resolve_async(v: Int, seed: Int) -> Int:
    let fut = crucible_async_mix(v, seed)
    return await fut
```

> **See:** `Async`, `await`, `EFFECTS.MD`

______________________________________________________________________

## `Async`

**Kind:** Hard Lexer
**Category:** Effects
**Layer:** L0

Async effect annotation. When combined with `async fn`, declares that the function operates in an asynchronous execution context. The effect gates the use of `await` and async runtime features. `Async` is a capability effect — callers must also be async-capable.

```kn
async fn fetch_data(id: Int) -> Result<Data, Error> with Async:
    let raw = await request(id)
    return parse(raw)
```

> **See:** `async`, `await`, `EFFECTS.MD`

______________________________________________________________________

## `GPU`

**Kind:** Hard Lexer
**Category:** Effects
**Layer:** L0

GPU compute effect annotation. Declares that a function dispatches GPU work or accesses GPU resources. Required for any function that uses the `dispatch` keyword. Typically combined with `Unsafe` for full GPU dispatch capability.

```kn
fn run_kernel(data: ptr<Float>) -> Int with GPU, Unsafe:
    dispatch "shader::Kernel::compute" [32, 32, 1]
    return abi_cuda_last_status()
```

> **See:** `dispatch`, `shader`, `compute`, `Unsafe`, `EFFECTS.MD`, `SHADER_GPU.MD`

______________________________________________________________________

## `Reactive`

**Kind:** Hard Lexer
**Category:** Effects
**Layer:** L0

Reactive effect annotation. Declares that a function reacts to state changes or events. Used with components that handle UI events and with functions that participate in reactive data flow. Signals that the function may be re-executed when observed state changes.

```kn
fn crucible_reactive_score(v: Int) -> Int with Reactive:
    return crucible_mod(v * 37 + 11, MODULUS)

component Button() with Reactive:
    render <box>
        <text value="click me" />
    </box>
```

> **See:** `resonate`, `component`, `EFFECTS.MD`

______________________________________________________________________

## `Unsafe`

**Kind:** Hard Lexer
**Category:** Effects
**Layer:** L0

Unsafe effect annotation. Declares that a function uses raw pointers, inline assembly, ABI calls, atomics, or any other operation that bypasses Kain's safety guarantees. Required for `collapse`/`observe`/`decay` on raw pointers, `asm`, `mem_load`, `mem_store`, `atomic_store`, and foreign ABI calls.

```kn
fn read_raw(cells: ptr<Int>) -> Int with Unsafe:
    return mem_load(ptr_offset(cells, 0, "Int"), "Int")

fn ownership_chain() -> Int with Unsafe:
    let buf: ptr<Int> = alloc_zeroed(8, "Int")
    collapse buf:
        mem_store(buf, 42, "Int")
        0
    let v = observe buf:
        mem_load(buf, "Int")
    decay buf
    return v
```

> **See:** `collapse`, `observe`, `decay`, `asm`, `GPU`, `EFFECTS.MD`

______________________________________________________________________

# GROUP 5: State Authority — LAYER 1 (8 keywords)

## `world`

**Kind:** Contextual
**Category:** State Authority — L1
**Layer:** L1

Named, compiler-owned state container. A `world` holds global state with `state` fields and optional UI surfaces (`surface native_ui => Component`). Worlds are the authority in the dual-world pattern: one world owns mutable state, another mirrors it via `entangle`. World state is accessed by path: `WorldName.field`.

```kn
world CrucibleAuthority:
    state signal:        Int = 1
    state epoch:         Int = 0
    state shadow:        Int = 0
    state ack:           Int = 0
    state teleport_land: Int = 0
    state pulse_count:   Int = 0
    surface web => CrucibleView
```

> **See:** `entangle`, `surface`, `state`, `patch`, `WORLD.MD`

______________________________________________________________________

## `entangle`

**Kind:** Contextual
**Category:** State Authority — L1
**Layer:** L1

Compiler-owned state coupling between world fields. Declares that two world fields stay synchronized under a policy (currently `single_writer`). The compiler tracks propagation counts, enforces single-writer semantics, and generates the sync machinery. A field can only participate in one entangle.

```kn
entangle CrucibleAuthority.signal <-> CrucibleMirror.signal_copy with single_writer
entangle CrucibleAuthority.epoch  <-> CrucibleMirror.epoch_copy  with single_writer
```

> **See:** `world`, `single_writer`, `ENTANGLE.MD`

______________________________________________________________________

## `single_writer`

**Kind:** Contextual
**Category:** State Authority — L1
**Layer:** L1

Entanglement policy keyword. Used in `entangle` declarations to specify that only one world (the authority) may write to the entangled field pair. The mirror world receives propagated updates but cannot write back. This is the currently-supported entangle policy.

```kn
entangle Authority.signal <-> Mirror.signal_copy with single_writer
```

> **See:** `entangle`, `world`, `ENTANGLE.MD`

______________________________________________________________________

## `surface`

**Kind:** Contextual
**Category:** State Authority — L1
**Layer:** L1

World-to-UI binding declaration. Declares that a `world` should present a specific `component` on a specific platform surface (`native_ui`, `web`, `viewport3d`, `ue5`). The surface keyword establishes the rendering backend for that world's UI.

```kn
world CrucibleAuthority:
    state signal: Int = 1
    surface web => CrucibleView

world CrucibleMirror:
    state signal_copy: Int = 1
    surface native_ui => CrucibleView
```

> **See:** `world`, `component`, `native_ui`, `web`, `viewport3d`, `ue5`, `COMPONENT.MD`

______________________________________________________________________

## `native_ui`

**Kind:** Contextual
**Category:** State Authority — L1
**Layer:** L1

Native desktop UI surface target. Used in `surface native_ui => Component` to bind a world's view to the native (Vulkan/winit) rendering backend. Typically used for the authority world in the dual-world pattern.

```kn
world CrucibleAuthority:
    state signal: Int = 1
    surface native_ui => CrucibleView
```

> **See:** `surface`, `world`, `web`, `COMPONENT.MD`

______________________________________________________________________

## `viewport3d`

**Kind:** Contextual
**Category:** State Authority — L1
**Layer:** L1

3D viewport surface target. Used in `surface viewport3d => Component` to bind a world to a 3D rendering viewport. Indicates the surface targets the 3D renderer runtime rather than 2D UI.

```kn
world SceneWorld:
    state camera: Vec3 = vec3(0, 0, 5)
    surface viewport3d => SceneView
```

> **See:** `surface`, `world`, `native_ui`

______________________________________________________________________

## `web`

**Kind:** Contextual
**Category:** State Authority — L1
**Layer:** L1

Web surface target. Used in `surface web => Component` to bind a world's view to a web/HTML rendering backend. Typically used for the mirror world in the dual-world pattern — the authority gets `native_ui`, the mirror gets `web` for remote/read-only inspection.

```kn
world CrucibleMirror:
    state signal_copy: Int = 1
    surface web => CrucibleView
```

> **See:** `surface`, `world`, `native_ui`

______________________________________________________________________

## `ue5`

**Kind:** Contextual
**Category:** State Authority — L1
**Layer:** L1

Unreal Engine 5 surface target. Used in `surface ue5 => Component` to bind a world's view to the Unreal Engine 5 rendering bridge. For projects that integrate Kain logic with UE5 for visualization or game logic surfaces.

```kn
world UnrealBridge:
    state game_state: Int = 0
    surface ue5 => UnrealHUD
```

> **See:** `surface`, `world`, `native_ui`, `viewport3d`

______________________________________________________________________

# GROUP 6: State Integrity — LAYER 2 (2 keywords)

## `patch`

**Kind:** Contextual
**Category:** State Integrity — L2
**Layer:** L2

Journaled, tracked mutation on world state. A `patch` function records mutations in the patch journal for auditability, replay, and telemetry (`patch_journal_count()`). Patches typically increment an epoch counter on the world to signal "state changed" to entangle propagation, resonate handlers, and orchestrate stages. Use `patch` instead of plain assignment when the mutation should be visible to the semantic stack.

```kn
patch crucible_commit_signal(authority: CrucibleAuthority, value: Int) -> Int:
    authority.signal = value
    authority.epoch  = authority.epoch + 1
    return authority.epoch

patch crucible_reset(authority: CrucibleAuthority) -> Int:
    authority.signal  = 1
    authority.epoch   = 0
    authority.shadow  = 0
    authority.ack     = 0
    return 0
```

> **See:** `world`, `law`, `orchestrate`, `PATCH.MD`

______________________________________________________________________

## `law`

**Kind:** Contextual
**Category:** State Integrity — L2
**Layer:** L2

Invariant predicate returning `Bool`. A `law` declares a condition that must hold for correctness — bounds, ranges, valid states, domain constraints. Laws are compiler-witnessable, can be invoked from orchestrate stages (`stage check: law my_law(value)`), and can be queried via `law_status()` and `law_is_valid_status()`. A `law` is the compiler-owned alternative to ad-hoc `if` checks.

```kn
law crucible_signal_valid(v: Int) -> Bool:
    return v >= 0 and v < CRUCIBLE_MODULUS

law crucible_epoch_valid(e: Int) -> Bool:
    return e >= 0

law crucible_in_range(s: Int, lo: Int, hi: Int) -> Bool:
    return s >= lo and s < hi
```

> **See:** `patch`, `orchestrate`, `guarded`, `LAW.MD`

______________________________________________________________________

# GROUP 7: Dispatch — LAYER 3 (8 keywords)

## `converge`

**Kind:** Contextual
**Category:** Dispatch — L3
**Layer:** L3

Spec-plus-fast-lanes dispatch construct. A `converge` block defines exactly one `spec` reference lane and one or more `fast` lanes gated by `target(...)` or `capability(...)` selectors. The runtime probes capabilities, scans fast lanes in order, and falls back to spec if no fast lane matches. `verify random(N)` fuzz-tests fast lanes against the spec. Use for platform-specific optimizations, strategy selection, or algorithmic alternatives.

```kn
converge crucible_fast_mix(v: Int, seed: Int) -> Int:
    spec reference:
        return crucible_mix_scalar(v, seed)
    fast closed_lane when target("llvm"):
        return crucible_mix_closed(v, seed)
    fast bit_lane when capability("cpu.x86.avx2"):
        return crucible_mix_bitwise(v, seed)
    verify random(4)
```

> **See:** `spec`, `fast`, `verify`, `random`, `when`, `target`, `capability`, `CONVERGE.MD`

______________________________________________________________________

## `spec`

**Kind:** Contextual
**Category:** Dispatch — L3
**Layer:** L3

Reference lane in a `converge` block. Exactly one `spec` lane is required — it defines the ground-truth implementation that fast lanes are verified against. The spec lane has a label (e.g., `reference`) and a function body.

```kn
spec reference:
    return standard_mix(v, seed)
```

> **See:** `converge`, `fast`, `verify`, `CONVERGE.MD`

______________________________________________________________________

## `fast`

**Kind:** Contextual
**Category:** Dispatch — L3
**Layer:** L3

Fast lane in a `converge` block. Each `fast` lane has a label and a `when` guard specifying the target or capability it requires. At least one `fast` lane is required. Fast lanes are probed in declaration order; the first matching one wins.

```kn
fast avx2_lane when capability("cpu.x86.avx2"):
    return vectorized_mix(v, seed)
fast llvm_lane when target("llvm"):
    return closed_form_mix(v, seed)
```

> **See:** `converge`, `spec`, `when`, `target`, `capability`, `CONVERGE.MD`

______________________________________________________________________

## `verify`

**Kind:** Contextual
**Category:** Dispatch — L3
**Layer:** L3

Verification clause in a `converge` block. `verify random(N)` tests the selected fast lane against the spec lane using N random inputs. Reports mismatches via `converge_mismatch_count()`. The verify step runs at startup/lane-selection time.

```kn
verify random(8)
```

> **See:** `converge`, `spec`, `fast`, `random`, `CONVERGE.MD`

______________________________________________________________________

## `random`

**Kind:** Contextual
**Category:** Dispatch — L3
**Layer:** L3

Random-input generator for `verify` in `converge` blocks. `verify random(N)` generates N random test inputs and runs both the spec lane and the selected fast lane, comparing outputs. Any mismatch is recorded in telemetry.

```kn
verify random(4)
verify random(256)  // more exhaustive
```

> **See:** `converge`, `verify`, `CONVERGE.MD`

______________________________________________________________________

## `when`

**Kind:** Contextual
**Category:** Dispatch — L3 / Machine Stones — L6
**Layer:** L3 / L6

Guard introducer for conditional clauses. Used in two contexts:

- **Converge fast lanes:** `fast label when target("llvm")` — selects a fast lane when the target matches.
- **Axiom predicates:** `when target("llvm")`, `when arch("x86_64")`, `when capability("memory.shatter")` — declares capability assumptions.

```kn
// In converge
fast avx_lane when capability("cpu.x86.avx2")

// In axiom
axiom machine_truth:
    when target("llvm")
    when arch("x86_64")
    when capability("memory.shatter")
```

> **See:** `converge`, `fast`, `axiom`, `target`, `capability`, `guarantee`, `CONVERGE.MD`, `AXIOM.MD`

______________________________________________________________________

## `target`

**Kind:** Contextual
**Category:** Dispatch — L3
**Layer:** L3

Compilation target selector for `converge` fast lanes and `axiom` predicates. Takes a string argument like `"llvm"`, `"linux"`, `"windows"`, `"macos"`. Used in `when target("...")` guards to gate code on the compilation backend or OS.

```kn
fast llvm_lane when target("llvm")
fast linux_lane when target("linux")

axiom win_truth:
    when target("windows")
    guarantee "windows-specific behavior"
```

> **See:** `converge`, `when`, `capability`, `axiom`, `CONVERGE.MD`, `AXIOM.MD`

______________________________________________________________________

## `capability`

**Kind:** Contextual
**Category:** Dispatch — L3
**Layer:** L3

Hardware or runtime capability selector. Takes a string like `"cpu.x86.avx2"`, `"gpu.compute"`, `"memory.shatter"`, `"world.teleport"`. Used in `when capability("...")` guards. The runtime probes available capabilities at startup and selects matching fast lanes or validates axiom assumptions.

```kn
fast avx_lane when capability("cpu.x86.avx2")
fast gpu_lane when capability("gpu.compute")

axiom machine_truth:
    when capability("memory.shatter")
    when capability("world.teleport")
```

> **See:** `converge`, `when`, `target`, `axiom`, `CONVERGE.MD`, `AXIOM.MD`

______________________________________________________________________

# GROUP 8: Stage Graph — LAYER 4 (11 keywords)

## `orchestrate`

**Kind:** Contextual
**Category:** Stage Graph — L4
**Layer:** L4

Typed, multi-runtime pipeline declaration. An `orchestrate` block defines a computation graph with `stage` entries that can run on different runtimes (`cpu`, `gpu`, `kain`, `converge`, `law`, `patch`, `world`, `dispatch`, `c`, `python`, `rust`, `node`). Stages declare dependencies (`deps`, `after`), residency (`host`, `shared`, `device`), transfer policies (`none`, `host_to_device`, `device_to_host`, `shared_view`), guards (`guarded by`), fallbacks, and scheduling policies.

```kn
orchestrate crucible_signal_pipeline(value: Int, epoch: Int) -> Int:
    stage host_base: cpu crucible_mix(value + epoch) when capability("cpu.scalar") residency host transfer none policy telemetry_prefer_cpu
    stage fast_lane: converge crucible_fast_mix(host_base, epoch) deps [host_base] residency host policy static
    stage law_check: law crucible_signal_valid(host_base) after fast_lane residency host policy static
    return world_score + patch_step + final_out
```

> **See:** `stage`, `after`, `deps`, `residency`, `transfer`, `guarded`, `by`, `requires`, `policy`, `fallback`, `ORCHESTRATE.MD`

______________________________________________________________________

## `stage`

**Kind:** Contextual
**Category:** Stage Graph — L4
**Layer:** L4

Individual pipeline stage within an `orchestrate` block. Each `stage` has a name, a runtime type (`cpu`, `gpu`, `kain`, `converge`, `law`, `patch`, `world`, `dispatch`, etc.), an expression body, and optional clauses (`deps`, `residency`, `transfer`, `guarded by`, `requires`, `policy`, `fallback`). The stage name becomes a variable in the orchestrate body representing that stage's output.

```kn
stage host_base: cpu crucible_mix(value + epoch) when capability("cpu.scalar") residency host policy static
stage gpu_tune: gpu crucible_fast_mix(world_score + epoch, 7) after world_score residency device transfer host_to_device fallback degrade crucible_orch_degrade
```

> **See:** `orchestrate`, `after`, `deps`, `residency`, `transfer`, `policy`, `ORCHESTRATE.MD`

______________________________________________________________________

## `after`

**Kind:** Contextual
**Category:** Stage Graph — L4
**Layer:** L4

Linear dependency clause for orchestrate stages. Sugar for `deps [stage]`. Declares that the current stage must execute after the named stage completes. Simpler than `deps` when there's only one dependency.

```kn
stage law_check: law crucible_signal_valid(host_base) after fast_lane residency host policy static
```

> **See:** `orchestrate`, `stage`, `deps`, `ORCHESTRATE.MD`

______________________________________________________________________

## `deps`

**Kind:** Contextual
**Category:** Stage Graph — L4
**Layer:** L4

Explicit dependency list for orchestrate stages. Declares that the current stage depends on the output of one or more named stages. The runtime ensures all dependencies complete before the stage executes.

```kn
stage patch_step: patch crucible_commit_ack(CrucibleAuthority) deps [world_score, gpu_tune] requires law_check residency host policy telemetry_prefer_cpu
```

> **See:** `orchestrate`, `stage`, `after`, `ORCHESTRATE.MD`

______________________________________________________________________

## `residency`

**Kind:** Contextual
**Category:** Stage Graph — L4
**Layer:** L4

Data residency declaration for an orchestrate stage. Specifies where the stage's data lives: `host` (CPU memory), `device` (GPU memory), or `shared` (accessible from both). The runtime uses residency to plan transfers and memory allocation.

```kn
stage host_base: cpu ... residency host
stage gpu_tune: gpu ... residency device
stage world_score: world ... residency shared
```

> **See:** `orchestrate`, `stage`, `transfer`, `ORCHESTRATE.MD`

______________________________________________________________________

## `transfer`

**Kind:** Contextual
**Category:** Stage Graph — L4
**Layer:** L4

Data transfer policy for an orchestrate stage. Specifies how data moves between residency domains: `none` (no transfer), `host_to_device`, `device_to_host`, or `shared_view`. The runtime issues the appropriate transfer commands based on residency and transfer declarations.

```kn
stage gpu_tune: gpu ... residency device transfer host_to_device
stage shared_stage: ... residency shared transfer shared_view
```

> **See:** `orchestrate`, `stage`, `residency`, `ORCHESTRATE.MD`

______________________________________________________________________

## `guarded`

**Kind:** Contextual
**Category:** Stage Graph — L4
**Layer:** L4

Capability gate introducer for orchestrate stages. Used with `by` to specify a named `axiom` that must be satisfied for the stage to execute. If the axiom's `when` predicates are not met, the fallback path is taken.

```kn
stage gpu_tune: gpu cb_fast_mix(...) guarded by crucible_silicon_truth fallback degrade crucible_orch_degrade
```

> **See:** `orchestrate`, `stage`, `by`, `axiom`, `fallback`, `ORCHESTRATE.MD`

______________________________________________________________________

## `by`

**Kind:** Contextual
**Category:** Stage Graph — L4
**Layer:** L4

Axiom reference in a `guarded by` clause. Connects an orchestrate stage guard to a named `axiom` that declares the required capabilities.

```kn
guarded by crucible_silicon_truth
```

> **See:** `orchestrate`, `guarded`, `axiom`, `ORCHESTRATE.MD`

______________________________________________________________________

## `requires`

**Kind:** Contextual
**Category:** Stage Graph — L4
**Layer:** L4

Law-invariant requirement for an orchestrate stage. Declares that a `law` stage must have passed for the current stage to execute. The runtime checks the law result before allowing the stage to proceed.

```kn
stage patch_step: patch crucible_commit_ack(...) requires law_check residency host policy telemetry_prefer_cpu
```

> **See:** `orchestrate`, `stage`, `law`, `ORCHESTRATE.MD`

______________________________________________________________________

## `policy`

**Kind:** Contextual
**Category:** Stage Graph — L4
**Layer:** L4

Scheduling policy hint for an orchestrate stage. Values: `static` (fixed schedule), `telemetry_prefer_gpu` (favor GPU), `telemetry_prefer_cpu` (favor CPU), `telemetry_balance_latency` (balance for low latency). The runtime uses these hints for resource allocation and scheduling decisions.

```kn
policy telemetry_prefer_cpu
policy telemetry_balance_latency
policy static
```

> **See:** `orchestrate`, `stage`, `ORCHESTRATE.MD`

______________________________________________________________________

## `fallback`

**Kind:** Contextual
**Category:** Stage Graph — L4
**Layer:** L4

Failure behavior for an orchestrate stage when its guard or requirements fail. Options include `abort` (fail the pipeline), naming another stage (use its output instead), or `degrade <stage>` (run a degraded fallback function).

```kn
fallback degrade crucible_orch_degrade
fallback abort
fallback backup_stage
```

> **See:** `orchestrate`, `stage`, `guarded`, `ORCHESTRATE.MD`

______________________________________________________________________

# GROUP 9: Temporal — LAYER 5 (5 keywords)

## `pulse`

**Kind:** Contextual
**Category:** Temporal — L5
**Layer:** L5

First-class temporal beat declaration. A `pulse` block runs a body on a recurring schedule with jitter tolerance. It fires once immediately on start, then on schedule. Duration units: `ns`, `us`, `ms`, `s`, `tick`, `ticks`. Body locals: `pulse_tick` (monotonic beat counter), `pulse_dt_ms` (actual elapsed time), `pulse_missed` (missed beats count). Pulse bodies accept broad effects: IO, Async, GPU, Reactive, Unsafe.

```kn
pulse crucible_heartbeat every 16 ms jitter 2 ms:
    CrucibleAuthority.pulse_count = CrucibleAuthority.pulse_count + pulse_tick + 1

pulse fx_modulation every 8ms jitter 1ms:
    let dt: Int = pulse_dt_ms
    FxWorld.phase = (FxWorld.phase + advance) % PHASE_MAX
```

> **See:** `every`, `jitter`, `resonate`, `PULSE.MD`

______________________________________________________________________

## `every`

**Kind:** Contextual
**Category:** Temporal — L5
**Layer:** L5

Schedule interval introducer for `pulse` declarations. Specifies the base recurrence period. Followed by a duration value with units: `every 16ms`, `every 1s`, `every 100us`.

```kn
pulse heartbeat every 16 ms jitter 2 ms:
pulse slow_tick every 1s:
```

> **See:** `pulse`, `jitter`, `PULSE.MD`

______________________________________________________________________

## `jitter`

**Kind:** Contextual
**Category:** Temporal — L5
**Layer:** L5

Jitter tolerance clause for `pulse` declarations. Specifies a random offset window around the `every` interval to desynchronize pulses and avoid thundering-herd re-triggering. The runtime adds a random offset within ±jitter/2 of the nominal interval.

```kn
pulse heartbeat every 16 ms jitter 2 ms:
```

> **See:** `pulse`, `every`, `PULSE.MD`

______________________________________________________________________

## `resonate`

**Kind:** Contextual
**Category:** Temporal — L5
**Layer:** L5

Reactive state-change tripwire. A `resonate` block attaches to a world field and fires a handler whenever that field is written. Handler locals: `resonate_new_i64`, `resonate_old_i64`, `resonate_fired`. Dampening (`dampen Nms`) absorbs rapid-fire changes. **Anti-self-feedback rule:** a resonate handler cannot write to its own trigger field — it can write to other world fields.

```kn
resonate CrucibleAuthority.signal dampen 0 ms:
    let new_val: Int = resonate_new_i64
    CrucibleAuthority.shadow = crucible_mod(
        (new_val * 53) + CrucibleAuthority.epoch,
        CRUCIBLE_MODULUS
    )

resonate FxWorld.distortion_drive dampen 32ms:
    let new_drive: Int = resonate_new_i64
    FxWorld.distortion_output = if new_drive < 500: 500 else: 600
```

> **See:** `dampen`, `pulse`, `patch`, `RESONATE.MD`

______________________________________________________________________

## `dampen`

**Kind:** Contextual
**Category:** Temporal — L5
**Layer:** L5

Absorption window for `resonate` handlers. `dampen Nms` creates a time window during which subsequent writes to the trigger field do not re-fire the handler. `dampen 0ms` suppresses only re-entrant self-triggers. The runtime tracks `resonate_absorb_count()` for absorbed firings.

```kn
resonate field dampen 16ms:  // 16ms absorption window
resonate field dampen 0 ms:  // only re-entrant suppression
```

> **See:** `resonate`, `pulse`, `RESONATE.MD`

______________________________________________________________________

# GROUP 10: Machine Stones — LAYER 6 (7 keywords)

## `axiom`

**Kind:** Contextual
**Category:** Machine Stones — L6
**Layer:** L6

Capability assumption declaration. An `axiom` states that certain targets, architectures, or capabilities are available, with a `guarantee` string and a `fallback` for when the assumption fails. Used by orchestrate stages via `guarded by axiom_name`. The runtime validates axiom predicates at startup.

```kn
axiom crucible_machine_truth:
    when target("llvm")
    when arch("x86_64")
    when capability("memory.shatter")
    when capability("world.teleport")
    guarantee "machine supports shatter + teleport + inline asm"
    fallback crucible_axiom_fallback
```

> **See:** `when`, `target`, `capability`, `guarantee`, `fallback`, `guarded`, `by`, `AXIOM.MD`

______________________________________________________________________

## `guarantee`

**Kind:** Contextual
**Category:** Machine Stones — L6
**Layer:** L6

Guarantee string in an `axiom` declaration. A human-readable assertion about what the axiom's predicates collectively guarantee. Used for documentation and runtime introspection.

```kn
guarantee "machine supports shatter + teleport + inline asm"
guarantee "CUDA compute capability >= 7.0"
```

> **See:** `axiom`, `AXIOM.MD`

______________________________________________________________________

## `shatter`

**Kind:** Contextual
**Category:** Machine Stones — L6
**Layer:** L6

Structure-of-Arrays layout intent. Applied as `shatter struct Name:` to declare that the struct should be laid out in SoA form (field lanes contiguous in memory) rather than AoS (struct instances contiguous). The compiler emits SoA metadata; the LLVM lane can lower shattered arrays through `kain_machine_shatter_alloc`. Use for hot data accessed by field-lane rather than by record.

```kn
shatter struct CrucibleShard:
    alpha: Int
    beta:  Int
    gamma: Int
    delta: Int
    alive: Bool

shatter struct Particle:
    position_x: Float
    position_y: Float
    velocity_x: Float
    velocity_y: Float
    alive: Bool
```

> **See:** `struct`, `teleport`, `SHATTER.MD`

______________________________________________________________________

## `teleport`

**Kind:** Contextual
**Category:** Machine Stones — L6
**Layer:** L6

Zero-copy cross-world value transfer. Moves a value from one `world` to another without copying. The source value is marked moved — post-teleport reads are compile errors. The runtime tracks the handoff via `kain_machine_teleport_ptr` or `kain_machine_teleport_note`.

```kn
let shard = CrucibleShard { alpha: 42, beta: 13, gamma: tick, delta: head, alive: true }
let moved = teleport shard from CrucibleAuthority to CrucibleMirror via crucible_shard_bus
```

> **See:** `from`, `to`, `via`, `world`, `TELEPORT.MD`

______________________________________________________________________

## `via`

**Kind:** Contextual
**Category:** Machine Stones — L6
**Layer:** L6

Channel specification for `teleport`. Names the bus or channel through which the cross-world transfer occurs. Cannot be empty.

```kn
teleport shard from Authority to Mirror via crucible_shard_bus
teleport data from WorldA to WorldB via shared_pulse_bus
```

> **See:** `teleport`, `from`, `to`, `TELEPORT.MD`

______________________________________________________________________

## `to`

**Kind:** Contextual
**Category:** Machine Stones — L6
**Layer:** L6

Target world introducer for `teleport`. Specifies the destination world for the cross-world transfer. Must be a distinct, declared world.

```kn
teleport shard from CrucibleAuthority to CrucibleMirror via crucible_shard_bus
```

> **See:** `teleport`, `from`, `via`, `TELEPORT.MD`

______________________________________________________________________

## `from`

**Kind:** Contextual
**Category:** Machine Stones — L6 / Foreign & Import
**Layer:** L6

Source introducer for `teleport` and `import`. In teleport, specifies the source world. In import (`from module import name`), specifies the Python module. In `include`, can appear in local header paths.

```kn
// teleport source
teleport shard from CrucibleAuthority to CrucibleMirror via bus

// Python import
from json import dumps
```

> **See:** `teleport`, `to`, `import`, `TELEPORT.MD`, `PYTHON.MD`

______________________________________________________________________

# GROUP 11: Systems — LAYER 7 (10 keywords)

## `actor`

**Kind:** Contextual
**Category:** Systems — L7
**Layer:** L7

Message-oriented stateful concurrency unit. An `actor` declares a named entity with `state` fields and `on Message(params)` handlers. Actors are spawned with `spawn Actor(field = value)`, communicate via `send target.Message(field = value)` and `ask(actor, "Message", payload)`. Actor telemetry includes scheduler queue depth, enqueue/dequeue counts, and overflow spawns.

```kn
actor CrucibleRelayActor:
    state bias:     Int = 7
    state turns:    Int = 0
    state checksum: Int = 0

    on Compute(reply_to: P, payload: Int):
        self.turns = self.turns + 1
        let result = (payload + self.bias + self.turns) % MODULUS
        self.checksum = (self.checksum + result) % MODULUS
        let child = spawn CrucibleVerifier(min_val = 0)
        send child.Verify(reply_to = reply_to, val = result)
```

> **See:** `state`, `spawn`, `send`, `on`, `ACTOR.MD`

______________________________________________________________________

## `state`

**Kind:** Contextual
**Category:** Systems — L7 / Components & UI
**Layer:** L7

Persistent state field declaration. Used in two contexts:

- **Actor state:** `state field: Type = initial` — durable state that persists across message handler invocations, accessed as `self.field`.
- **Component state:** `state field: Type = initial` — local UI state that persists across render cycles.

```kn
// Actor state
actor Relay:
    state bias: Int = 7
    state turns: Int = 0
    on Compute(reply_to: P, val: Int):
        self.turns = self.turns + 1

// Component state
component Counter():
    state count: Int = 0
    render <text value={str(count)} />
```

> **See:** `actor`, `component`, `world`, `ACTOR.MD`, `COMPONENT.MD`

______________________________________________________________________

## `spawn`

**Kind:** Hard Lexer
**Category:** Systems — L7
**Layer:** L7

Actor instantiation. Creates a new actor instance from an `actor` definition. Requires named initialization arguments matching the actor's state fields. Returns a handle that can be used for `send` and `ask`.

```kn
let relay = spawn CrucibleRelayActor(bias = 7, turns = 0, checksum = 0)
let verifier = spawn CrucibleVerifier(min_val = 0)
let teleporter = spawn CrucibleTeleporter(done = 0, last = 0)
```

> **See:** `actor`, `send`, `on`, `ACTOR.MD`

______________________________________________________________________

## `send`

**Kind:** Hard Lexer
**Category:** Systems — L7
**Layer:** L7

Message delivery to an actor. Sends a named message with named fields to a target actor. The target actor must have a matching `on MessageName(params)` handler. Fields are passed by name.

```kn
send child.Verify(reply_to = reply_to, val = result)
send reply_to.Reply(value = result)
```

> **See:** `actor`, `spawn`, `on`, `ACTOR.MD`

______________________________________________________________________

## `on`

**Kind:** Hard Lexer
**Category:** Systems — L7
**Layer:** L7

Actor message handler declaration. Defines the handler for a specific message type on an actor. The handler has access to the actor's state via `self`. The `reply_to: P` parameter enables the reply port for request/reply patterns.

```kn
on Compute(reply_to: P, payload: Int):
    self.turns = self.turns + 1
    let result = (payload * self.multiplier + self.bias) % MODULUS
    send reply_to.Reply(value = result)

on Verify(reply_to: P, val: Int):
    let ok = val >= self.min_val
    if ok == false:
        send reply_to.Reply(value = -99)
        return
    send reply_to.Reply(value = val)
```

> **See:** `actor`, `spawn`, `send`, `ACTOR.MD`

______________________________________________________________________

## `collapse`

**Kind:** Contextual
**Category:** Systems — L7
**Layer:** L7

Exclusive ownership mutation scope. Declares a region where a `ptr<T>` is under exclusive, single-writer mutation. Inside a `collapse` block, the pointer is exclusively owned — no concurrent reads or writes allowed. The body is a block expression whose trailing value yields the collapse result. Legal only from the `Idle` ownership state.

```kn
collapse cells:
    var i: Int = 0
    while i < count:
        mem_store(ptr_offset(cells, i, "Int"), (i * 17) + 3, "Int")
        i = i + 1
    0  // collapse body yields 0
```

> **See:** `observe`, `decay`, `share`, `OWNERSHIP.MD`

______________________________________________________________________

## `observe`

**Kind:** Contextual
**Category:** Systems — L7
**Layer:** L7

Read-only ownership observation scope. Declares a region where a `ptr<T>` is accessible for reads only. Multiple nested `observe` scopes are allowed (readers can share). The body is a block expression whose trailing value is the observation result. Legal from the `Idle` or `Observed(n)` state.

```kn
let head: Int = observe cells:
    mem_load(ptr_offset(cells, 0, "Int"), "Int")

let total: Int = observe partials:
    var w: Int = 0
    var sum: Int = 0
    while w < WORKER_COUNT:
        sum = (sum + mem_load(ptr_offset(partials, w, "Int"), "Int")) % MODULUS
        w = w + 1
    sum
```

> **See:** `collapse`, `decay`, `share`, `OWNERSHIP.MD`

______________________________________________________________________

## `decay`

**Kind:** Contextual
**Category:** Systems — L7
**Layer:** L7

Deterministic ownership teardown operation. Terminates the lifetime of a `ptr<T>` allocation. After `decay`, the pointer is dead — any further use is a compile error. `decay` is only legal from the `Idle` state (after all observations end). Typically paired with `defer` for RAII-like cleanup.

```kn
decay cells
decay partials
decay line_buf

// Typical pattern with defer:
let buf: ptr<Int> = alloc_zeroed(N, "Int")
defer decay buf
```

> **See:** `collapse`, `observe`, `defer`, `OWNERSHIP.MD`

______________________________________________________________________

## `share`

**Kind:** Contextual
**Category:** Systems — L7
**Layer:** L7

Shared pointer region for parallel write lanes. Wraps a pointer in a `share` block that allows multiple `fanout` lanes to write to disjoint regions concurrently. Guarantees no data races by ensuring each fanout lane writes to a non-overlapping range.

```kn
share partials:
    fanout worker in 0..WORKER_COUNT:
        let slot: ptr<Int> = ptr_offset(partials, worker, "Int")
        var local: Int = 0
        while step < STEPS:
            local = (local + compute(worker+step, iterations)) % modulus
            step = step + 1
        atomic_store(slot, local)
```

> **See:** `fanout`, `collapse`, `observe`, `OWNERSHIP.MD`

______________________________________________________________________

## `fanout`

**Kind:** Contextual
**Category:** Systems — L7
**Layer:** L7

Parallel write lane within a `share` block. Each `fanout` iteration writes to a disjoint region of the shared pointer. The runtime guarantees non-overlapping access ranges, enabling safe parallel writes without data races.

```kn
share buffer:
    fanout worker in 0..WORKER_COUNT:
        let slot = ptr_offset(buffer, worker, "Int")
        atomic_store(slot, compute(worker))
```

> **See:** `share`, `collapse`, `OWNERSHIP.MD`

______________________________________________________________________

# GROUP 12: GPU & Shaders (7 keywords)

## `shader`

**Kind:** Contextual
**Category:** GPU & Shaders
**Layer:** L0

GPU shader/kernel declaration. Introduces a shader item that compiles to SPIR-V, PTX, HLSL, or WGSL. Three variants: `shader vertex`, `shader fragment`, `shader compute`. Each has typed parameters, optional `uniform` bindings, and (for compute) `workgroup` size declaration. Shader bodies use GPU-native types (Vec2, Vec3, Vec4, UVec3, Float, etc.).

```kn
shader vertex CrucibleVertex(position: Vec3, uv: Vec2) -> Vec4:
    uniform offset: Vec3 @0
    let lane = position.x + offset.x
    return vec4(lane, position.y + offset.y + uv.x, position.z + offset.z, 1.0)

shader fragment CrucibleFragment(uv: Vec2) -> Vec4:
    uniform tint: Vec3 @0
    let wave: Float = uv.x * (1.0 - uv.x)
    return vec4(tint.x * wave, tint.y * wave, tint.z * (0.5 + wave), 1.0)

shader compute CrucibleKernel(id: UVec3) -> Void workgroup(8, 8, 1):
    uniform src: StorageBuffer<UInt> @0
    uniform dst: StorageBuffer<UInt> @1
    let lane = src[id.x]
    dst[id.x] = lane + UInt(1)
```

> **See:** `vertex`, `fragment`, `compute`, `uniform`, `workgroup`, `dispatch`, `SHADER_GPU.MD`

______________________________________________________________________

## `vertex`

**Kind:** Contextual
**Category:** GPU & Shaders
**Layer:** L0

Vertex shader variant of `shader`. Processes individual vertices, transforming position data from object space to screen space. Takes vertex attributes as parameters and returns `Vec4` clip-space position.

```kn
shader vertex MyVertex(position: Vec3, uv: Vec2) -> Vec4:
    uniform offset: Vec3 @0
    return vec4(position.x + offset.x, position.y + offset.y + uv.x, position.z + offset.z, 1.0)
```

> **See:** `shader`, `fragment`, `compute`, `SHADER_GPU.MD`

______________________________________________________________________

## `fragment`

**Kind:** Contextual
**Category:** GPU & Shaders
**Layer:** L0

Fragment (pixel) shader variant of `shader`. Processes individual fragments (pixels) during rasterization. Takes interpolated vertex data as parameters and returns `Vec4` color.

```kn
shader fragment MyFragment(uv: Vec2) -> Vec4:
    uniform tint: Vec3 @0
    let wave: Float = uv.x * (1.0 - uv.x)
    return vec4(tint.x * wave, tint.y * wave, tint.z * (0.5 + wave), 1.0)
```

> **See:** `shader`, `vertex`, `compute`, `SHADER_GPU.MD`

______________________________________________________________________

## `compute`

**Kind:** Contextual
**Category:** GPU & Shaders
**Layer:** L0

Compute shader variant of `shader`. Declares a GPU compute kernel that operates on a grid of work items. Takes a `id: UVec3` parameter for the global invocation ID and requires a `workgroup(W, H, D)` declaration. Returns `Void`.

```kn
shader compute MyKernel(id: UVec3) -> Void workgroup(8, 8, 1):
    uniform src: StorageBuffer<UInt> @0
    uniform dst: StorageBuffer<UInt> @1
    dst[id.x] = src[id.x] + UInt(1)
```

> **See:** `shader`, `vertex`, `fragment`, `workgroup`, `uniform`, `dispatch`, `SHADER_GPU.MD`

______________________________________________________________________

## `uniform`

**Kind:** Contextual
**Category:** GPU & Shaders
**Layer:** L0

Shader resource binding declaration. Declares a uniform/buffer parameter at a specific binding slot (`@N`). Inside `shader` items, `uniform Name: Type @N` binds a resource. Types include `Vec3`, `Float`, `StorageBuffer<Type>`, and other GPU-compatible types.

```kn
uniform offset: Vec3 @0
uniform tint: Vec3 @0
uniform src: StorageBuffer<UInt> @0
uniform dst: StorageBuffer<UInt> @1
```

> **See:** `shader`, `compute`, `vertex`, `fragment`, `SHADER_GPU.MD`

______________________________________________________________________

## `workgroup`

**Kind:** Contextual
**Category:** GPU & Shaders
**Layer:** L0

Compute shader workgroup size declaration. Specifies the local workgroup dimensions (threads per group) for a `shader compute` kernel. Written as `workgroup(W, H, D)` after the return type.

```kn
shader compute Kernel(id: UVec3) -> Void workgroup(8, 8, 1):
shader compute LargeKernel(id: UVec3) -> Void workgroup(32, 32, 1):
```

> **See:** `shader`, `compute`, `SHADER_GPU.MD`

______________________________________________________________________

## `dispatch`

**Kind:** Contextual
**Category:** GPU & Shaders
**Layer:** L0

Host-side GPU kernel launch. The `dispatch "compute.key" [x, y, z]` statement launches a compute shader with the given resource key and workgroup counts. The compute key must be a string literal in the form `"shader::KernelName::compute"`. Dimensions are workgroup counts, not thread counts. Requires `with GPU, Unsafe` on the enclosing function.

```kn
dispatch "shader::CrucibleKernel::compute" [16, 1, 1]

fn run_kernel() -> Int with GPU, Unsafe:
    dispatch "shader::MyKernel::compute" [32, 32, 1]
    return abi_cuda_last_status()
```

> **See:** `shader`, `compute`, `GPU`, `Unsafe`, `SHADER_GPU.MD`

______________________________________________________________________

# GROUP 13: Compile-Time (2 keywords)

## `comptime`

**Kind:** Contextual
**Category:** Compile-Time
**Layer:** L0

Compile-time code execution block. Inside a `shader` item, a `comptime:` block contains metadata for compile-time resource binding, workgroup size override, and GPU artifact descriptor generation. The compiler evaluates the block at compile time to produce shader metadata.

```kn
shader compute CrucibleKernel(id: UVec3) -> Void workgroup(8, 8, 1):
    uniform src: StorageBuffer<UInt> @0
    uniform dst: StorageBuffer<UInt> @1
    comptime:
        let compute = (
            [64, 1, 1],
            [
                ("src", "u32", ["dispatch.x"], "input",  "kain.shared.buffer"),
                ("dst", "u32", ["dispatch.x"], "output", "kain.shared.buffer"),
            ],
            [],
        )
```

> **See:** `shader`, `compute`, `macro`

______________________________________________________________________

## `macro`

**Kind:** Contextual
**Category:** Compile-Time
**Layer:** L0

Syntactic macro definition. Declares a named macro with `name!(param: kind):` syntax. The `!` after the macro name is required — it is a mandatory parser token. Macros perform syntactic expansion at compile time. The body contains the expansion template.

```kn
macro crucible_fold!(x: expr):
    crucible_mod(x, CRUCIBLE_MODULUS)

// Usage:
let folded = crucible_fold!(acc + index)
```

> **See:** `comptime`

______________________________________________________________________

# GROUP 14: Foreign & Import (4 keywords)

## `include`

**Kind:** Contextual
**Category:** Foreign & Import
**Layer:** L0

C header import. Uses `libclang` to extract function signatures from C headers. Two forms: local (`include path/to/header.h as alias`) and system angle-bracket (`include <windows.h> as win`). System headers are resolved through `crates/c-ffi/system_headers.toml`. `libclang` handles WINAPI, SAL annotations, macros, and complex vendor headers without shim headers.

```kn
include <stdio.h> as libc
include <windows.h> as win    // 605 functions extracted
include <vulkan/vulkan.h> as vk  // 755 functions extracted
include native/shim.h as my_abi
```

> **See:** `import`, `as`, `C.MD`

______________________________________________________________________

## `import`

**Kind:** Contextual
**Category:** Foreign & Import
**Layer:** L0

Python host-object import. Brings Python modules into Kain scope. Named Kain arguments lower to Python kwargs automatically. Supports `import module as alias` and `from module import name` forms.

```kn
import json as py_json
import numpy as np
from torch import tensor

fn encode(value: String) -> String:
    return py_json.dumps(value, separators = [",", ":"])
```

> **See:** `from`, `as`, `include`, `PYTHON.MD`

______________________________________________________________________

## `where`

**Kind:** Contextual
**Category:** Types, Modules & Visibility
**Layer:** L0

Generic constraint clause. Adds trait bounds and type constraints beyond what inline bounds can express. Written after the function signature but before the body: `fn name<T>(t: T) -> R with Pure where T: TraitA + TraitB`. The AST stores `where` clauses separately from inline bounds for validation.

```kn
fn crucible_packet_summary<T: CrucibleMetric>(p: T, salt: Int) -> Int with Pure where T: CrucibleStable:
    let s = p.score()
    let b = p.bias()
    return crucible_mod((s * b) + salt, MODULUS)
```

> **See:** `trait`, `fn`, `type`

______________________________________________________________________

# GROUP 15: Components & UI (3 keywords)

## `component`

**Kind:** Contextual
**Category:** Components & UI
**Layer:** UI

Full React-like UI widget declaration. A `component` has typed props, local `state`, methods (with `_self: Self_`), and a `render` body using JSX syntax. JSX supports uppercase-tag component composition (`<OtherComponent />`), lowercase native elements (`<panel>`, `<text>`, `<box>`), `for` loops, `if`/`else` conditionals, and `{expr}` interpolation. Components are NOT tied to worlds — they can exist independently.

```kn
component Counter(initial: Int, label: String):
    state count: Int = initial
    fn increment(_self: Self_) -> Int:
        return _self.count + 1
    render <box>
        <text value={label + ": " + str(count)} />
    </box>

component Dashboard():
    render <panel title="My App">
        <Counter initial={0} label="Saves" />
        <Counter initial={10} label="Edits" />
    </panel>
```

> **See:** `state`, `render`, `surface`, `COMPONENT.MD`

______________________________________________________________________

## `render`

**Kind:** Contextual
**Category:** Components & UI
**Layer:** UI

Component JSX render body introducer. Follows the `render` keyword with a JSX expression tree. Supports lowercase native elements (panel, text, box, stack, list) and uppercase component calls. Inside the JSX tree: `{expr}` interpolation, `for item in list:` loops, `if`/`else` conditionals, and `<Fragment>` for multiple roots.

```kn
component App():
    render <panel title="Keyword Crucible">
        <text value="Hello" />
    </panel>

component TodoList(items: [String]):
    render <stack direction="vertical">
        for item in items:
            <text value={item} />
        if len(items) == 0:
            <text value="No items" />
    </stack>
```

> **See:** `component`, `COMPONENT.MD`

______________________________________________________________________

## `weak`

**Kind:** Contextual
**Category:** Systems — L7
**Layer:** L7

Non-owning pointer annotation. Declares a `ptr<T>` as a weak alias — it does not own the pointee and must not be used for `decay`. Used in `Unsafe` contexts to annotate observer pointers that should not be confused with owning pointers.

```kn
fn crucible_weak_touch(value: Int) -> Int with Unsafe:
    let mut raw: ptr<Int> = alloc_zeroed(1, "Int")
    let weak_alias: ptr<Int> = raw  // weak: non-owning alias
    collapse raw:
        mem_store(raw, value * 3, "Int")
        0
    let loaded: Int = observe raw:
        mem_load(raw, "Int")
    decay raw        // ONLY raw owns — weak_alias is NOT decayed
    let _ = weak_alias  // weak reference still accessible (but must not be used after raw decay)
    return loaded
```

> **See:** `collapse`, `observe`, `decay`, `ptr`, `OWNERSHIP.MD`

______________________________________________________________________

# GROUP 16: Testing (1 keyword)

## `test`

**Kind:** Contextual
**Category:** Testing
**Layer:** L0

Test case declaration. Marks a function as a test case that is compiled and run during `kain test`. Test functions are compiled conditionally (only when testing). They can use `assert`, `assert_eq`, and other test utilities from `std::test`.

```kn
test fn test_addition():
    assert(add(2, 2) == 4)

test fn test_bounds():
    let result = crucible_signal_valid(42)
    assert(result == true)
```

> **See:** `fn`

______________________________________________________________________

# GROUP 17: Operator Aliases (2 keywords)

## `and`

**Kind:** Operator Alias
**Category:** Operator Aliases
**Layer:** L0

Boolean AND operator. Word-form alias for `&&`. Evaluates to `true` only when both operands are `true`. Short-circuits: if the left operand is `false`, the right operand is not evaluated.

```kn
if v >= 0 and v < MODULUS:
    return true

let valid: Bool = a > 0 and b < LIMIT
```

> **See:** `or`

______________________________________________________________________

## `or`

**Kind:** Operator Alias
**Category:** Operator Aliases
**Layer:** L0

Boolean OR operator. Word-form alias for `||`. Evaluates to `true` when either operand is `true`. Short-circuits: if the left operand is `true`, the right operand is not evaluated.

```kn
if s == 0 or b == 0:
    return salt

let flag: Bool = a == none or a == 0
```

> **See:** `and`

______________________________________________________________________

# GROUP 18: Reserved (2 keywords)

## `emit`

**Kind:** Hard Lexer
**Category:** Reserved
**Layer:** N/A

**Reserved keyword.** `emit` has a dedicated `TokenKind::Emit` entry in the lexer (`crates/core/src/lexer.rs`) but there is **no `parse_emit()` production rule** in the parser. Using `emit` as a statement or identifier causes a parse error. Reserved for future use — likely for event emission from actors or components.

```kn
// ⚠ PARSE ERROR — no parser rule yet
// emit event_name(payload)
```

> **See:** `receive`, `keyword_crucible.kn` (gaps comment)

______________________________________________________________________

## `receive`

**Kind:** Hard Lexer
**Category:** Reserved
**Layer:** N/A

**Reserved keyword.** `receive` has a dedicated `TokenKind::Receive` entry in the lexer (`crates/core/src/lexer.rs`) but there is **no `parse_receive()` production rule** in the parser. Using `receive` as a statement or identifier causes a parse error. Reserved for future use — likely for message reception on actor mailboxes.

```kn
// ⚠ PARSE ERROR — no parser rule yet
// receive msg from mailbox
```

> **See:** `emit`, `keyword_crucible.kn` (gaps comment)

______________________________________________________________________

# Appendix: Keyword Groups by Decision Layer

## Layer 0 — Plain Code (34 keywords)

`fn` `let` `mut` `var` `const` `if` `else` `elif` `match` `for` `while` `loop` `break` `continue` `return` `defer` `await` `in` `with` `as` `type` `struct` `enum` `trait` `impl` `pub` `mod` `use` `self` `Self` `true` `false` `none` `where`
*Effects:* `Pure` `IO` `async` `Async` `GPU` `Reactive` `Unsafe`
*Operator Aliases:* `and` `or`
*Reserved:* `emit` `receive`

## Layer UI — Components (3 keywords)

`component` `render` `state` (also L7)

## Layer 1 — State Authority (8 keywords)

`world` `entangle` `single_writer` `surface` `native_ui` `viewport3d` `web` `ue5`

## Layer 2 — State Integrity (2 keywords)

`patch` `law`

## Layer 3 — Dispatch (8 keywords)

`converge` `spec` `fast` `verify` `random` `when` `target` `capability`

## Layer 4 — Stage Graph (11 keywords)

`orchestrate` `stage` `after` `deps` `residency` `transfer` `guarded` `by` `requires` `policy` `fallback`

## Layer 5 — Temporal (5 keywords)

`pulse` `every` `jitter` `resonate` `dampen`

## Layer 6 — Machine Stones (7 keywords)

`axiom` `guarantee` `shatter` `teleport` `via` `to` `from`

## Layer 7 — Systems (10 keywords)

`actor` `state` `spawn` `send` `on` `collapse` `observe` `decay` `share` `fanout`

## Foreign & Import (3 keywords + from)

`include` `import` `from` `as`

## GPU & Shaders (7 keywords)

`shader` `vertex` `fragment` `compute` `uniform` `workgroup` `dispatch`

## Compile-Time (2 keywords)

`comptime` `macro`

## Testing (1 keyword)

`test`

## Operator Aliases (2 keywords)

`and` `or`

______________________________________________________________________

*End of Keyword Dictionary · 110 keywords documented · 108 exercisable · 2 reserved (emit, receive) with no parser rules yet*
