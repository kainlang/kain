# Comptime — Zig-Style Compile-Time Execution (and the GPU Metadata DSL)

**Status:** Research synthesis, 2026-06-08
**Based on:** `crates/core/src/comptime.rs` (compile-time evaluator — 201 lines, full interpreter integration), `crates/core/src/parser.rs` (parse_comptime_block, expression comptime), `crates/core/src/ast.rs` (ComptimeBlock, ComputeMetadata, shader metadata extraction), `crates/core/src/types.rs` (TypedComptime, in_comptime flag), `crates/driver/src/lib.rs` (CompilerPhase::Comptime — runs BEFORE typecheck), `crates/sys-codegen/` (all backends treat Expr::Comptime as transparent), `crates/semantic/symbol_corpus/comptime.kn` (only non-GPU comptime example), `blades/c/vulkan_v2/src/fluid.comp.kn` (GPU compute metadata), `blades/cuda/mcp/src/search_kernel.kn` (GPU compute metadata with streams), `blades/three-d/zender/src/sculpt/kernels/brush_kernels.kn` (sculpt kernel with 12 tensors + 12 streams), `blades/example/src/main.kn` (comptime const declaration), ~55 GPU shader files using comptime metadata blocks, RULEBOOK.MD, lang-semantics SKILL, lang-systems SKILL

---

## Table of Contents

1. [What Is Comptime?](#what-is-comptime)
2. [The Two Faces of Comptime](#the-two-faces-of-comptime)
3. [The Compile-Time Evaluator — How It Actually Works](#the-compile-time-evaluator)
4. [The GPU Metadata DSL — What Everyone Actually Uses](#the-gpu-metadata-dsl)
5. [Syntax Reference](#syntax-reference)
6. [What Comptime CAN Do](#what-comptime-can-do)
7. [What Comptime CANNOT Do](#what-comptime-cannot-do)
8. [What Comptime COULD Do — The Unexplored Territory](#what-comptime-could-do)
9. [The Zig Comparison — What's Missing](#the-zig-comparison)
10. [Comptime vs Const vs Macro](#comptime-vs-const-vs-macro)
11. [Real-World Usage Survey](#real-world-usage-survey)
12. [Validation Ladder](#validation-ladder)
13. [Anti-Patterns](#anti-patterns)
14. [Quick Reference](#quick-reference)
15. [Source Anchors](#source-anchors)

---

## What Is Comptime?

`comptime` is Kain's **compile-time execution** keyword — a Zig-inspired construct that runs code during compilation rather than at runtime. It has two forms:

```kn
// Top-level — runs code at compile time
comptime:
    const LUT_SIZE: Int = 256

// Expression — evaluates and replaces with the result
let value = comptime: 40 + 2   // becomes let value = 42
```

The key insight: **comptime runs the actual Kain runtime interpreter at compile time.** It can call functions, allocate memory, run loops, compute lookup tables — then bake the results into the compiled binary. After evaluation, the comptime wrapper is erased and the result becomes a literal.

But here's the reality: **comptime is almost completely unused for its intended purpose.** The compile-time evaluator works — it can do everything described above — but there is exactly ONE non-GPU usage in the entire repo (a smoke test). Instead, comptime serves as a GPU metadata DSL inside shader compute kernels, where it's NOT evaluated at all but pattern-matched at the AST level for dispatch manifests.

### What Comptime IS NOT

- ❌ Not a code generator (that's `macro`)
- ❌ Not a type-level construct (comptime runs BEFORE typechecking — you can't inspect types)
- ❌ Not a constant expression (that's `const`)
- ❌ Not an effect (there's no `Comptime` effect — comptime blocks don't appear at runtime)
- ❌ Not evaluated for shader GPU metadata (those blocks are AST-pattern-matched, not interpreted)

### What Comptime IS

- ✅ A compile-time code execution engine that runs the Kain interpreter
- ✅ A value baker — replaces expressions with their computed literals
- ✅ A GPU metadata DSL — the `let compute = (...)` pattern inside shaders
- ✅ Pre-typecheck — comptime evaluates BEFORE the typechecker sees the AST
- ✅ Capable of running arbitrary Kain code (functions, loops, allocs, math)
- ✅ Transparent at codegen — every backend (LLVM, Rust, C, WASM, SPIR-V) unwraps it

### The Relationship View

| Construct | Domain-Locked View | Relationship View |
|---|---|---|
| `comptime` | "Shader metadata block" | **Compile-time execution engine** — a full interpreter that runs before typecheck, plus an AST-level pattern-matching DSL for GPU dispatch manifests |

---

## The Two Faces of Comptime

Comptime in Kain has a **split personality**. There's the compile-time evaluator (what it IS) and the GPU metadata DSL (what it's USED FOR). They don't actually talk to each other.

### Face 1: The Compile-Time Evaluator

```
Source code
    │
    ▼
[Parser] → AST with ComptimeBlock / Expr::Comptime nodes
    │
    ▼
[comptime::eval_program()] ← runs the Kain interpreter at compile time
    │
    │  For Item::Comptime(block): eval_block() — run the block, keep side effects
    │  For Expr::Comptime(inner, span): eval_expr() → value_to_expr() → replace
    │
    ▼
[Typechecker] → sees the EVALUATED AST (comptime values are now literals)
    │
    ▼
[Codegen] → Expr::Comptime is transparent (unwraps inner expression)
```

**Location:** `crates/core/src/comptime.rs` — 201 lines. Called from `crates/driver/src/lib.rs` as `CompilerPhase::Comptime`.

### Face 2: The GPU Metadata DSL

```
shader compute MyKernel(id: UVec3) -> Void:
    comptime:                          ← NOT evaluated by eval_program
        let compute = (                  (shader bodies are SKIPPED)
            [8, 8, 1],                  ← instead: pattern-matched by
            [64, 64, 1],                   Shader::explicit_compute_metadata()
            [("buf", "u32", ...), ...],   ← extracts ComputeMetadata struct
            [],
        )

    // ... kernel body ...
```

**Location:** `crates/core/src/ast.rs` — `Shader::explicit_compute_metadata()`, `extract_compute_metadata_from_comptime_block()`, `parse_compute_metadata_expr()`.

These two faces never interact. The GPU metadata is never seen by the compile-time evaluator because `eval_item()` has no match for `Item::Shader`. The metadata is extracted by a completely separate AST traversal.

---

## The Compile-Time Evaluator — How It Actually Works

### The Pipeline

`comptime::eval_program(&mut ast)` is called at phase `CompilerPhase::Comptime` in the driver, BEFORE typechecking. It:

1. **Walks all items** in the program
2. **For `Item::Comptime(block)`**: Evaluates the block body using the runtime interpreter (`crate::runtime::eval_block()`). Side effects (like `const` declarations) are kept in the AST.
3. **For `Expr::Comptime(inner, span)`**: Evaluates the inner expression with `eval_expr(env, inner)`, then **replaces** the comptime expression node with a literal via `value_to_expr(val, span)`.
4. **Recurses** into function bodies, component methods, const values, and all expression sub-nodes.

### `value_to_expr` — The Value Baker

This is the function that converts interpreted values back into AST literals:

```rust
// crates/core/src/comptime.rs
fn value_to_expr(val: Value, span: Span) -> Expr {
    match val {
        Value::Int(n)    => Expr::Int(n, span),
        Value::Float(n)  => Expr::Float(n, span),
        Value::Bool(b)   => Expr::Bool(b, span),
        Value::String(s) => Expr::String(s, span),
        Value::Unit      => Expr::Block(Block { stmts: vec![], span }, span),
        other            => Expr::String(
            format!("<unrepresentable comptime value: {:?}>", other), span),
    }
}
```

Five types survive the bake: `Int`, `Float`, `Bool`, `String`, `Unit`. Everything else becomes a debug string. This means:
- **Arrays, structs, pointers** — cannot be baked (they become `<unrepresentable comptime value: ...>` strings)
- **Function pointers** — cannot be baked
- **World references, actor handles, GPU buffers** — cannot be baked

### The `in_comptime` Flag

The typechecker has an `in_comptime: bool` field on `TypeEnv`, initialized to `false`. It's propagated through `clone_env_for_expr()` but **never actively set to `true`**. The diagnostic system can flag errors with `in_comptime_block = true` for better error messages, but this path is never triggered because comptime evaluation happens before the typechecker.

The comment in `types.rs` is telling:

> "Comptime blocks should be empty/removed by now if fully evaluated, or we check them"

This reveals the design intent: after `comptime::eval_program()`, comptime blocks should be gone (evaluated and removed). The typechecker only sees them as a fallback for blocks the evaluator couldn't handle.

### Image: The evaluation runs like this

```kn
// Source code:
comptime:
    var acc: Int = 0
    var i: Int = 0
    while i < 100:
        acc = acc + i * i
        i = i + 1
    const SUM_OF_SQUARES: Int = acc

fn main() -> Int:
    return SUM_OF_SQUARES   // 328350 — baked at compile time
```

When `eval_program()` encounters the `comptime:` block, it:
1. Enters the interpreter
2. Runs the loop 100 times (at compile time!)
3. Computes `acc = 328350`
4. Leaves `const SUM_OF_SQUARES: Int = 328350` in the AST

When the typechecker sees `fn main()`, `SUM_OF_SQUARES` is already a `const Int = 328350` — no comptime wrapper, no interpreter overhead, just a constant.

---

## The GPU Metadata DSL — What Everyone Actually Uses

This is where 99% of comptime lives. The pattern is:

```kn
shader compute SemanticFusedScoreTopK(id: UVec3) -> Void:
    uniform query_embed: StorageBuffer<u8> @0
    uniform index_matrix: StorageBuffer<u8> @1
    uniform index_weights: StorageBuffer<UInt> @2
    uniform scores: StorageBuffer<UInt> @4

    comptime:
        let compute = (
            [256, 1, 1],              // workgroup_size
            [3200000, 1, 1],          // dispatch_size
            [                          // tensor_plans
                ("query_embed", "u8", ["384"], "input", "kain.shared.buffer"),
                ("index_matrix", "u8", ["100000", "384"], "input", "kain.shared.buffer"),
                ("scores", "u32", ["100000"], "output", "kain.shared.buffer"),
            ],
            [                          // stream_plans
                ("query_embed", "ingress", "per-dispatch", "kain.shared.buffer"),
                ("scores", "egress", "per-dispatch", "kain.shared.buffer"),
            ],
            [],                       // neural_node_plans
        )

    // ... kernel body uses id, uniform bindings ...
```

### The Two Forms

**Legacy form** (3-tuple): `(dispatch, tensors, nodes)`
```kn
let compute = (
    [64, 64, 1],
    [("buf", "u32", ["dispatch.x"], "output", "kain.shared.buffer")],
    [],
)
```

**Extended form** (5-tuple): `(workgroup, dispatch, tensors, streams, nodes)`
```kn
let compute = (
    [8, 8, 1],
    [64, 64, 1],
    [("buf", "u32", ["dispatch.x"], "output", "kain.shared.buffer")],
    [("buf", "egress", "per-dispatch", "kain.shared.buffer")],
    [],
)
```

### How It's Extracted

The extraction is a **pure AST pattern match** — it looks for `Stmt::Let { name: "compute" | "compute_plan", value: Expr::Tuple(...) }` inside `Item::Comptime` blocks within shader bodies. The shader body is never evaluated by the comptime interpreter.

The extracted `ComputeMetadata` struct feeds:

| Backend | What it produces |
|---------|-----------------|
| **SPIR-V** | Dispatch layout declarations, descriptor set layouts |
| **PTX/CUDA** | Kernel launch parameters, grid/block dimensions |
| **HLSL** | `[numthreads()]` attributes, structured buffer declarations |
| **WGSL** | `@workgroup_size()` attributes, storage buffer bindings |
| **Runtime manifest** | JSON dispatch manifest with tensor/stream/node plans |

### The Tensor Plan Tuple

Each tensor plan is a 5-tuple: `("binding", "element_type", ["shape", "dims"], "role", "contract")`

| Field | Example | Meaning |
|-------|---------|---------|
| `key` | `"query_embed"` | Matches the `uniform` binding name |
| `element_type` | `"u8"`, `"f32"`, `"u32"` | C type of buffer elements |
| `shape` | `["384"]`, `["dispatch.x", "3"]` | Dimensions — can reference `dispatch.x/y/z` |
| `role` | `"input"`, `"output"`, `"state"` | Read/write role (default: `"state"`) |
| `contract` | `"kain.shared.buffer"` | Memory contract (default: `"kain.shared.buffer"`) |

### The Stream Plan Tuple

Each stream plan is a 4-tuple: `("binding", "direction", "cadence", "contract")`

| Field | Example | Meaning |
|-------|---------|---------|
| `key` | `"positions"` | Matches the `uniform` binding name |
| `direction` | `"ingress"`, `"egress"` | Data flow direction |
| `cadence` | `"per-dispatch"` | How often data flows (default: `"continuous"`) |
| `contract` | `"kain.shared.buffer"` | Memory contract |

### Where It's Used (55+ blocks)

| Domain | Files | What it declares |
|--------|-------|-----------------|
| **Semantic search (LLM)** | `ml/src/*_kernel.kn`, `crates/semantic/src/*_kernel.kn` | Embedding matrices, index weights, chunk scoring, top-k fusion |
| **CUDA MCP** | `blades/cuda/mcp/src/search_kernel.kn` | 10+ tensors, ingress/egress streams per kernel |
| **Vulkan compute** | `blades/c/vulkan_v2/src/fluid*.comp.kn` | Fluid sim: prev/next buffers, 196K elements |
| **3D sculpting** | `blades/three-d/zender/src/sculpt/kernels/brush_kernels.kn` | 12 tensors + 12 streams per brush kernel — positions, normals, masks, brush params |
| **GPU/CPU pipeline** | `benchmark/cases_v2/gpu_cpu_pipeline.kn` | Orchestrate bridge kernels |
| **Component shader** | `blades/kain/component_shader/src/kernel.comp.kn` | UI-integrated compute shader |
| **Keyword coverage** | `benchmark/cases_v2/keyword_crucible.kn` | Reference comptime metadata block |

---

## Syntax Reference

### Top-Level Comptime Block

```kn
comptime:
    <statements>
```

The block is parsed as a `Block` (indented body with statements). Called from `parse_item()` alongside `fn`, `struct`, `enum`, `actor`, etc.

```kn
comptime:
    const LUT_SIZE: Int = 256
    const LUT_MASK: Int = 255
    var table: ptr<Int> = alloc_zeroed(LUT_SIZE, "Int")
    // ... generate lookup table ...
```

### Expression Comptime

```kn
comptime: <expression>
```

Parsed as `Expr::Comptime(Box<Expr>, Span)`. The expression is evaluated at compile time and replaced with its literal value.

```kn
let answer = comptime: 40 + 2                    // → 42
let magic = comptime: compute_magic(7, 11)       // → result of compute_magic(7, 11)
let len_str = comptime: "hello " + "world"       // → "hello world"
```

### Inside Shaders (The GPU Metadata Form)

```kn
shader compute Name(id: UVec3) -> Void:
    uniform buf0: StorageBuffer<UInt> @0
    uniform buf1: StorageBuffer<UInt> @1

    comptime:
        let compute = (                    // ← must be named "compute" or "compute_plan"
            [wg_x, wg_y, wg_z],           // workgroup size (optional — legacy form omits)
            [dispatch_x, dispatch_y, dispatch_z],  // dispatch grid
            [/* tensor plans */],
            [/* stream plans */],          // optional
            [/* neural node plans */],     // optional
        )

    // kernel body
```

---

## What Comptime CAN Do

Based on the `comptime::eval_program()` implementation and the runtime interpreter it calls:

### ✅ Arithmetic and Logic

```kn
comptime:
    const PI: Float = 3.141592653589793
    const TAU: Float = PI * 2.0
    const ANSWER: Int = 40 + 2
```

### ✅ Function Calls (Pure Functions)

```kn
fn mix(a: Int, b: Int) -> Int with Pure:
    return ((a * 31) + b) % 1000000007

comptime:
    const PRECOMPUTED = mix(42, 17)   // evaluated by interpreter at compile time
```

### ✅ Loops and Computation

```kn
comptime:
    var acc: Int = 0
    var i: Int = 0
    while i < 1000:
        acc = acc + i * i
        i = i + 1
    const SUM_SQUARES_1000: Int = acc
```

### ✅ Memory Allocation

```kn
comptime:
    let table: ptr<Int> = alloc_zeroed(256, "Int")
    var i: Int = 0
    while i < 256:
        mem_store(ptr_offset(table, i, "Int"), i * i, "Int")
        i = i + 1
    // table now contains [0, 1, 4, 9, 16, ...] — at compile time
```

### ✅ String Operations

```kn
comptime:
    const GREETING: String = "Hello, " + "World!"
```

### ✅ Expression Baking

```kn
// Before comptime eval:
fn get_answer() -> Int:
    return comptime: 40 + 2

// After comptime eval (what the typechecker sees):
fn get_answer() -> Int:
    return 42
```

### ✅ Const Declarations

```kn
comptime:
    const VERSION: Int = 3
    const MAX_CONNECTIONS: Int = 64
```

---

## What Comptime CANNOT Do

### ❌ Access the Type System

Comptime runs BEFORE typechecking. You cannot inspect types, generate code based on types, or do type-level metaprogramming.

```kn
// This WON'T work:
comptime:
    // Cannot do: let type_name = type_of(MyStruct)
    // Cannot do: for each field in MyStruct: ...
    // Type information is not available during comptime evaluation
```

### ❌ Read Files at Compile Time

The interpreter doesn't have filesystem access. Use `include "file.h" as alias` for C headers.

```kn
// This WON'T work:
comptime:
    // Cannot do: let config = read_file("config.json")
```

### ❌ Bake Complex Types

`value_to_expr` only handles Int, Float, Bool, String, and Unit. Arrays, structs, pointers, and function references become debug strings.

```kn
comptime:
    let arr = [1, 2, 3]
    // arr cannot be baked — becomes "<unrepresentable comptime value: Array(...)>"
```

### ❌ GPU Dispatch or Hardware Access

The interpreter runs on the CPU at compile time. No GPU, no hardware access, no OS syscalls.

### ❌ Code Generation (That's `macro`)

Comptime evaluates values, not generates code. For hygienic code generation, use Kain's `macro` system.

### ❌ Access Runtime-Only Constructs

World state, actor handles, GPU buffers, network connections — these don't exist at compile time.

---

## What Comptime COULD Do — The Unexplored Territory

This is where it gets interesting. The evaluator works. The interpreter is fully functional. Almost no one uses it. Here's what's possible RIGHT NOW with zero compiler changes:

### Idea 1: Precomputed Lookup Tables

```kn
comptime:
    const SIN_TABLE_SIZE: Int = 256

fn generate_sin_table() -> ptr<Float> with Unsafe:
    let table: ptr<Float> = alloc_zeroed(SIN_TABLE_SIZE, "Float")
    var i: Int = 0
    while i < SIN_TABLE_SIZE:
        let angle: Float = (to_float(i) * 3.141592653589793 * 2.0) / to_float(SIN_TABLE_SIZE)
        // approximate sin via Taylor series at compile time
        let s = angle - (angle * angle * angle) / 6.0 + (angle * angle * angle * angle * angle) / 120.0
        mem_store(ptr_offset(table, i, "Float"), s, "Float")
        i = i + 1
    return table

comptime:
    const SIN_LUT: ptr<Float> = generate_sin_table()
    // 256 precomputed sine values, baked into the binary
    // 0 runtime computation, 0 memory allocation at startup
```

### Idea 2: Compile-Time Hashing

```kn
fn hash_djb2(input: String) -> Int with Pure:
    var hash: Int = 5381
    var i: Int = 0
    while i < len(input):
        hash = ((hash * 33) + ord(input[i])) % 1000000007
        i = i + 1
    return hash

comptime:
    const ASSET_HASH_PLAYER: Int = hash_djb2("player_model")
    const ASSET_HASH_ENEMY: Int = hash_djb2("enemy_model")
    const ASSET_HASH_LEVEL: Int = hash_djb2("level_01")

fn load_asset(hash: Int) -> Asset:
    // Switch on baked constant — zero runtime hashing
    if hash == ASSET_HASH_PLAYER: return load_player()
    elif hash == ASSET_HASH_ENEMY: return load_enemy()
    elif hash == ASSET_HASH_LEVEL: return load_level()
    else: return Asset::default()
```

### Idea 3: Configuration Baking

```kn
struct AppConfig:
    width: Int
    height: Int
    fullscreen: Bool
    vulkan_device: String

fn parse_config(raw: String) -> AppConfig with Pure:
    // Pure string parsing — runs at compile time for baked configs
    return AppConfig { width: 1280, height: 720, fullscreen: false, vulkan_device: "auto" }

comptime:
    const DEFAULT_CONFIG: AppConfig = parse_config("1280x720 windowed auto")
    // AppConfig baked at compile time, zero parsing overhead
```

### Idea 4: Compile-Time Branch Elimination

```kn
comptime:
    const PLATFORM_IS_WINDOWS: Bool = true   // known at compile time

fn platform_specific_code() -> Int with IO:
    if PLATFORM_IS_WINDOWS:
        return win_GetTickCount()
    else:
        return 0
    // The compiler sees: if true: return win_GetTickCount() else: return 0
    // Dead branch eliminated. No runtime check.

fn platform_specific_code() -> Int with IO:
    if PLATFORM_IS_WINDOWS:
        return win_GetTickCount()
    else:
        return 0
    // The compiler sees: if true: return win_GetTickCount() else: return 0
    // Dead branch eliminated. No runtime check.
```

### Idea 5: Type-Safe Enum Tables

```kn
enum ShaderVariant:
    Standard
    HighQuality
    Performance
    Debug

fn variant_name(v: ShaderVariant) -> String with Pure:
    if v == ShaderVariant::Standard: return "standard"
    elif v == ShaderVariant::HighQuality: return "high_quality"
    elif v == ShaderVariant::Performance: return "performance"
    else: return "debug"

comptime:
    const VARIANT_NAMES: [String; 4] = [
        variant_name(ShaderVariant::Standard),
        variant_name(ShaderVariant::HighQuality),
        variant_name(ShaderVariant::Performance),
        variant_name(ShaderVariant::Debug),
    ]
    // String array baked at compile time
```

### Idea 6: Compile-Time Math for Shader Constants

```kn
fn compute_workgroup_count(elements: Int, threads_per_group: Int) -> Int with Pure:
    return (elements + threads_per_group - 1) / threads_per_group

comptime:
    const ELEMENTS: Int = 1000000
    const THREADS: Int = 256
    const GROUPS: Int = compute_workgroup_count(ELEMENTS, THREADS)  // 3907

shader compute Processor(id: UVec3) -> Void:
    // GROUPS is a baked constant — shader knows its dispatch size at compile time
    uniform data: StorageBuffer<UInt> @0

    // ... uses ELEMENTS, THREADS, GROUPS as literals
```

### Idea 7: Conditional Compilation Without Macros

```kn
comptime:
    const HAS_VULKAN: Bool = true    // or: = has_capability("gpu.vulkan")
    const DEBUG_SHADERS: Bool = true

fn init_graphics() with IO, GPU, Unsafe:
    if HAS_VULKAN:
        init_vulkan()
    else:
        init_opengl()

    if DEBUG_SHADERS:
        println("shader debug mode active")
```

### Why These Ideas AREN'T Used Today

1. **No one knows comptime can do this.** The docs focus on GPU metadata. The only example (`comptime.kn`) declares two integer constants.
2. **`value_to_expr` only bakes 5 types.** Arrays, structs, and pointers become debug strings — you can compute a lookup table at compile time but can't bake it as an `[Int; 256]` literal.
3. **No filesystem access.** Zig's `@embedFile` makes comptime great for embedding assets — Kain can't do that.
4. **No type reflection.** Zig's `@typeInfo` enables generic comptime code — Kain can't inspect types.

---

## The Zig Comparison — What's Missing

Kain's comptime is explicitly documented as "Zig-style compile-time execution" (`ast.rs` line 9). Here's the gap:

| Feature | Zig | Kain |
|---------|-----|------|
| **Evaluate expressions at compile time** | ✅ `comptime { ... }` | ✅ `comptime: { ... }` |
| **Call functions at compile time** | ✅ Full language | ✅ Full interpreter |
| **Comptime-known types** | ✅ `comptime_int`, `comptime_float` | ❌ Evaluated to concrete types |
| **Type reflection** | ✅ `@typeInfo`, `@typeName`, `@TypeOf` | ❌ No comptime type access |
| **Code generation from types** | ✅ `@Type`, `@Vector` | ❌ Uses `macro` keyword |
| **Comptime generics** | ✅ `fn foo(comptime T: type)` | ❌ No comptime parameters |
| **`@compileLog`** | ✅ Print values at compile time | ❌ No equivalent |
| **`@compileError`** | ✅ Error with message at compile time | ❌ Would need KainError integration |
| **Embed files** | ✅ `@embedFile("data.bin")` | ❌ Uses `include` keyword |
| **Comptime I/O** | ✅ Limited (`@embedFile`) | ❌ No interpreter filesystem access |
| **Branch elimination** | ✅ Comptime-known conditions optimize | ⚠️ LLVM does it, not the comptime system |
| **Compile-time memory** | ✅ Fixed-size allocations | ⚠️ Interpreter supports alloc but can't persist arrays/structs |

### The Biggest Gaps

1. **No type reflection** — You can't write `comptime: for each field in struct { ... }`. This is Zig's killer comptime feature and Kain's biggest missing piece.

2. **No `@embedFile`** — Can't bake binary assets at compile time. The `include` keyword imports C headers, not binary data.

3. **`value_to_expr` is too limited** — Only 5 types survive the bake. Arrays, structs, and pointers computed at compile time are lost. This single function is the bottleneck — fix it and half the unexplored ideas become possible.

4. **No comptime parameters** — Zig's `fn foo(comptime N: usize)` compiles a specialized version for each N. Kain's macros can do this syntactically, but not with the same compile-time execution semantics.

---

## Comptime vs Const vs Macro

| Aspect | `comptime` | `const` | `macro` |
|--------|-----------|---------|---------|
| **Evaluates** | Yes — runs interpreter at compile time | No — just a constant expression | Yes — syntactic expansion |
| **Can call functions** | ✅ Yes | ❌ Limited to constant expressions | ⚠️ Expands to code, doesn't execute |
| **Can contain statements** | ✅ Yes (loops, allocs, etc.) | ❌ No, single expression | ✅ Yes (any Kain code) |
| **Generates code** | ❌ No (evaluates values) | ❌ No | ✅ Yes (hygienic expansion) |
| **Survives to codegen** | Only as baked literals | As inlined constant | As expanded code |
| **When it runs** | Before typecheck | During typecheck (constant folding) | Before typecheck (expansion) |
| **Primary use** | GPU metadata (current), value baking (intended) | Named constants | DSLs, boilerplate reduction |

**The rule:** Use `comptime` when you want to run code at compile time to produce a value. Use `const` when you just want to name a literal. Use `macro` when you want to generate code.

---

## Real-World Usage Survey

### By the numbers

| Usage | Count | Percentage |
|-------|-------|-----------|
| GPU compute metadata blocks | ~58 | 95% |
| Compile-time constant declarations | ~3 | 5% |
| Compile-time computation (loops, functions, tables) | **0** | **0%** |

### The only non-GPU uses

**`crates/semantic/symbol_corpus/comptime.kn`** — two constants in a comptime block:
```kn
comptime:
    const SMOKE_SURFACE_COUNT: Int = 17
    const SMOKE_ROUTE_MASK: Int = 63
```

**`blades/example/src/main.kn`** — one constant:
```kn
comptime:
    const COMPTIME_NATIVE_SURFACE_COUNT: Int = 11
```

That's it. Two files. Three constants. Zero actual computation.

### The GPU patterns in detail

| Pattern | Count | Example |
|---------|-------|---------|
| Legacy 3-tuple `(dispatch, tensors, nodes)` | ~20 | Fluid sim, early CUDA kernels |
| Extended 5-tuple `(workgroup, dispatch, tensors, streams, nodes)` | ~35 | LLM kernels, sculpt kernels, MCP kernels |
| With neural node plans | ~5 | ML training kernels |
| Single tensor | ~10 | Simple compute shaders |
| 10+ tensors | ~8 | LLM semantic search kernels |
| With stream plans | ~20 | CUDA MCP, sculpt kernels |

---

## Validation Ladder

```powershell
# Run the only non-GPU comptime smoke test
kain check crates/semantic/symbol_corpus/comptime.kn

# Build a shader with comptime metadata (GPU path)
kain build blades/c/vulkan_v2/src/fluid.comp.kn --target spirv

# Build a CUDA kernel with extended comptime metadata
kain build blades/cuda/mcp/src/search_kernel.kn --target cuda

# Build the sculpt kernels (12 tensors + 12 streams)
kain build blades/three-d/zender/src/sculpt/kernels/brush_kernels.kn --target spirv

# Run keyword_crucible (has comptime metadata in shader)
kain run benchmark/cases_v2/keyword_crucible.kn --target llvm

# Run the comptime evaluator tests
cargo test -p kain-core comptime
```

---

## Anti-Patterns

### ❌ Using Comptime for GPU Metadata Outside Shaders

```kn
// WRONG: comptime outside a shader cannot declare compute plans
comptime:
    let compute = ([8, 8, 1], [...], [])  // ignored — not in a shader body
```

### ❌ Expecting Complex Values to Survive the Bake

```kn
// WRONG: arrays become debug strings
comptime:
    const MY_ARRAY: [Int; 3] = [1, 2, 3]  // becomes "<unrepresentable comptime value: Array(...)>"
```

### ❌ Trying to Read Files or Access Types at Comptime

```kn
// WRONG: no filesystem, no type system
comptime:
    // Cannot do: let config = read_file("config.toml")
    // Cannot do: let type_name = type_name_of(MyStruct)
```

### ❌ Using Comptime When `const` Would Suffice

```kn
// WRONG: comptime block for a simple constant
comptime:
    const MAX: Int = 100

// RIGHT: just use const
const MAX: Int = 100
```

### ❌ Using Comptime Where a Macro is Needed

```kn
// WRONG: comptime cannot generate code
comptime:
    // Cannot do: for each field in struct { generate getter function }
```

### ✅ Using Comptime Correctly — The Untapped Potential

```kn
// RIGHT: compile-time computation that bakes a value
comptime:
    const SIN_TABLE_SIZE: Int = 256
    const SIN_TABLE: ptr<Float> = generate_sin_table()

// RIGHT: GPU metadata in shaders
shader compute MyKernel(id: UVec3) -> Void:
    comptime:
        let compute = ([8,8,1], [64,64,1], [("buf","u32",["dispatch.x"],"output","kain.shared.buffer")], [], [])
    // kernel body
```

---

## Quick Reference

| Aspect | Detail |
|--------|--------|
| **Keyword** | `comptime` — contextual keyword |
| **Top-level syntax** | `comptime:\n    <statements>` |
| **Expression syntax** | `comptime: <expression>` |
| **GPU metadata syntax** | `comptime:\n    let compute = (workgroup, dispatch, tensors, streams, nodes)` |
| **AST nodes** | `Item::Comptime(ComptimeBlock)` and `Expr::Comptime(Box<Expr>, Span)` |
| **Typecheck** | `TypedComptime { ast: Block }` — wraps the body, no evaluation |
| **Evaluator** | `crates/core/src/comptime.rs` — 201 lines, runs interpreter before typecheck |
| **Pipeline phase** | `CompilerPhase::Comptime` — BEFORE typecheck, in `driver/src/lib.rs` |
| **value_to_expr** | Converts Int, Float, Bool, String, Unit — other types become debug strings |
| **Codegen** | Transparent — every backend (LLVM, Rust, C, WASM, SPIR-V) unwraps to inner expression |
| **GPU metadata extraction** | `Shader::explicit_compute_metadata()` — AST pattern match, NOT evaluated |
| **Legacy GPU form** | `(dispatch, tensors, nodes)` — 3-tuple |
| **Extended GPU form** | `(workgroup, dispatch, tensors, streams, nodes)` — 5-tuple |
| **Tensor plan** | `("key", "type", ["dims"], "role", "contract")` — 5-tuple |
| **Stream plan** | `("key", "direction", "cadence", "contract")` — 4-tuple |
| **Neural node plan** | `("key", "op", ["inputs"], ["outputs"], stateful)` — 5-tuple |
| **Binding names** | `"compute"` or `"compute_plan"` |
| **Can call functions** | ✅ Yes — full interpreter at compile time |
| **Can run loops** | ✅ Yes |
| **Can allocate memory** | ✅ Yes (interpreter alloc) |
| **Can access types** | ❌ No — comptime runs BEFORE typecheck |
| **Can read files** | ❌ No |
| **Can generate code** | ❌ No — use `macro` |
| **Real usage** | ~58 GPU metadata blocks, 3 constant declarations, 0 compile-time computations |
| **Source** | `crates/core/src/comptime.rs` (evaluator), `crates/core/src/ast.rs:647-800` (GPU metadata extraction) |
| **Bakeable types** | Int, Float, Bool, String, Unit only |
| **Interpreter integration** | Uses `crate::runtime::eval_block()` and `eval_expr()` |
| **Browser builds** | No-op — comptime is `#[cfg(not(feature = "runtime"))]` skipped |

---

## Source Anchors

### Core Evaluator

| File | Content |
|------|---------|
| `crates/core/src/comptime.rs` | Full compile-time evaluator — `eval_program()`, `eval_block_comptime()`, `eval_fn_comptime()`, `value_to_expr()` — 201 lines |
| `crates/driver/src/lib.rs:626-628` | `CompilerPhase::Comptime` — calls `comptime::eval_program(&mut ast)?` |

### Parser

| File | Content |
|------|---------|
| `crates/core/src/lexer.rs:101-102` | `#[token("comptime")] Comptime` — lexer token |
| `crates/core/src/parser.rs:1002` | `TokenKind::Comptime => self.parse_comptime_block()` — item dispatch |
| `crates/core/src/parser.rs:3145-3153` | `parse_comptime_block()` — `comptime:\n    <block>` |
| `crates/core/src/parser.rs:5482-5487` | Expression comptime — `comptime: { <body> }` |

### AST

| File | Content |
|------|---------|
| `crates/core/src/ast.rs:189-190` | `Item::Comptime(ComptimeBlock)` |
| `crates/core/src/ast.rs:1422-1426` | `pub struct ComptimeBlock { body: Block, span: Span }` |
| `crates/core/src/ast.rs:1912-1913` | `Expr::Comptime(Box<Expr>, Span)` |
| `crates/core/src/ast.rs:20-24` | `COMPUTE_PLAN_BINDING_NAMES = &["compute", "compute_plan"]` |
| `crates/core/src/ast.rs:647-800` | `ComputeMetadata`, `ComputeTensorPlan`, `ComputeStreamPlan`, `ComputeNeuralNodePlan`, `Shader::explicit_compute_metadata()`, `extract_compute_metadata_from_comptime_block()`, `parse_compute_metadata_expr()` |

### Typechecker

| File | Content |
|------|---------|
| `crates/core/src/types.rs:48-51` | `pub struct TypedComptime { pub ast: Block }` |
| `crates/core/src/types.rs:4673-4675` | Item::Comptime → TypedComptime (body clone) |
| `crates/core/src/types.rs:9047` | Expr::Comptime → infers inner expression type |
| `crates/core/src/types.rs:1079,1127` | `in_comptime: bool` flag on TypeEnv |
| `crates/core/src/types.rs:1776-1777` | Diagnostic flag for in_comptime_block |

### Codegen

| File | Content |
|------|---------|
| `crates/sys-codegen/src/codegen_llvm/mod.rs:1189` | Item::Comptime — collects pointer let types, no LLVM IR |
| `crates/sys-codegen/src/codegen_llvm/mod.rs:1282,3725,...` (15+ sites) | Expr::Comptime — transparent passthrough to inner expression |
| `crates/sys-codegen/src/codegen_rust/mod.rs:2705` | Expr::Comptime → `self.gen_expr(expr)` |
| `crates/gpu/src/codegen_spirv.rs:2720` | Expr::Comptime → `"Comptime"` string name |
| `crates/script/src/codegen_ks.rs:1305` | Expr::Comptime → transparent passthrough |

### Authored Kain

| File | Usage |
|------|-------|
| `crates/semantic/symbol_corpus/comptime.kn` | **Only non-GPU comptime** — 2 const declarations |
| `blades/example/src/main.kn:83` | 1 const declaration in comptime block |
| `blades/c/vulkan_v2/src/fluid.comp.kn:9` | GPU metadata: 2 tensors, legacy 3-tuple |
| `blades/cuda/mcp/src/search_kernel.kn:21,89` | GPU metadata: 7 tensors + 6 streams, extended 5-tuple |
| `blades/cuda/mcp/src/search_kernel_god.kn:66,235,331,444` | GPU metadata: 4 kernels, extended 5-tuple with streams |
| `blades/three-d/zender/src/sculpt/kernels/brush_kernels.kn:37,154,274,408,505,601` | GPU metadata: 6 brush kernels, 12 tensors + 12 streams each |
| `blades/kain/component_shader/src/kernel.comp.kn:7` | GPU metadata: simple compute shader |
| `ml/src/*_kernel.kn` (~25 blocks) | GPU metadata: LLM semantic search kernel family |
| `crates/semantic/src/*_kernel.kn` (~25 blocks) | GPU metadata: mirrors of ml/ kernel family |
| `benchmark/cases_v2/keyword_crucible.kn` | GPU metadata: reference comptime block |
| `benchmark/cases_v2/keyword_expansion.kn:178` | GPU metadata |
| `benchmark/cases_v2/orchestrate_god.kn:184` | GPU metadata |
| `benchmark/cases_v2/orchestration.kn:148` | GPU metadata |
