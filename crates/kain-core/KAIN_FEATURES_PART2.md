# KAIN Language Features - Part 2
## Parser, Runtime, Type System, and Standard Library

> **Companion to KAIN_FEATURES_PART1.md** — This document covers the parser capabilities, runtime system, resolved type system, shader analysis, standard library, and testing infrastructure.

---

## Table of Contents

1. [Parser & Grammar](#1-parser--grammar)
2. [Type System (Resolved)](#2-type-system-resolved)
3. [Runtime & Interpreter](#3-runtime--interpreter)
4. [Shader Features](#4-shader-features)
5. [Standard Library](#5-standard-library)
6. [Source Tracking](#6-source-tracking)
7. [Testing Infrastructure](#7-testing-infrastructure)
8. [Feature Summary](#8-feature-summary)

---

## 1. Parser & Grammar

### 1.1 Parser Architecture

**File:** `Kain/crates/kain-core/src/parser.rs`

The KAIN parser is a **recursive descent parser** with:
- **Python-style indentation** (INDENT/DEDENT tokens)
- **Operator precedence climbing** for binary expressions
- **Attribute-driven dispatch** (@material_graph, @graph_editor, etc.)
- **Error recovery** with file:line:col diagnostics
- **Span tracking** for every AST node

### 1.2 Top-Level Item Parsing

The parser can parse **25+ top-level item types**:

| Item Type | Keyword | Example |
|-----------|---------|---------|
| **Function** | `fn` | `fn add(a: Int, b: Int) -> Int: return a + b` |
| **Async Function** | `async fn` | `async fn fetch_data() -> String: ...` |
| **Component** | `component` | `component Button(text: String) -> UI: <button>{text}</button>` |
| **Shader** | `shader` | `shader fragment ColorTint(uv: Vec2) -> Vec4: ...` |
| **Actor** | `actor` | `actor Player: state health: Float = 100.0` |
| **Struct** | `struct` | `struct Point { x: Float, y: Float }` |
| **Enum** | `enum` | `enum Color { Red, Green, Blue }` |
| **Trait** | `trait` | `trait Drawable { fn draw(self): ... }` |
| **Impl** | `impl` | `impl Drawable for Circle { ... }` |
| **Type Alias** | `type` | `type Vec2 = (Float, Float)` |
| **Use** | `use` | `use std::collections::HashMap` |
| **Mod** | `mod` | `mod utils` |
| **Const** | `const` | `const PI: Float = 3.14159` |
| **Comptime** | `comptime` | `comptime { println("Compile-time!") }` |
| **Macro** | `macro` | `macro debug!(expr) { println("{}", expr) }` |
| **Test** | `test` | `test "addition": assert(1 + 1 == 2)` |
| **Material Graph** | `@material_graph` | `@material_graph PBR: input albedo: Texture2D` |
| **Material Function** | `@material_function` | `@material_function Fresnel: ...` |
| **Graph Editor** | `@graph_editor` | `@graph_editor DialogueGraph: ...` |
| **Graph Runtime** | `@graph_runtime` | `@graph_runtime DialogueSystem: ...` |
| **State Machine** | `@state_machine` | `@state_machine CombatAnimations: ...` |
| **Async Task** | `@async_task` | `@async_task MeshGenerator: ...` |
| **Editor Module** | `@editor_module` | `@editor_module WeaponEditor: ...` |
| **Gameplay Tags** | `@gameplay_tags` | `@gameplay_tags namespace Abilities: ...` |
| **Gameplay Ability** | `@ability` | `@ability struct FireballAbility: ...` |
| **Gameplay Effect** | `@gameplay_effect` | `@gameplay_effect struct BurnEffect: ...` |
| **Gameplay Cue** | `@gameplay_cue` | `@gameplay_cue struct ExplosionCue: ...` |

### 1.3 Expression Parsing

The parser supports **40+ expression types** with full operator precedence:

#### Literals
- **Int**: `42`, `-100`, `0xFF`, `0b1010`, `0o755`
- **Float**: `3.14`, `-0.5`, `1e-10`
- **String**: `"hello"`, `'world'`
- **FString**: `f"Hello {name}!"` (interpolated strings)
- **Bool**: `true`, `false`
- **None**: `none`

#### Operators

**Binary Operators** (with precedence):
- **Arithmetic**: `+`, `-`, `*`, `/`, `%`, `**` (power)
- **Comparison**: `==`, `!=`, `<`, `>`, `<=`, `>=`
- **Logical**: `and`, `or`
- **Bitwise**: `&`, `|`, `^`, `<<`, `>>`
- **Assignment**: `=`, `+=`, `-=`, `*=`, `/=`
- **Range**: `..`, `..=` (inclusive)

**Unary Operators**:
- **Negation**: `-expr`
- **Logical NOT**: `not expr`
- **Bitwise NOT**: `~expr`
- **Reference**: `&expr`, `&mut expr`
- **Dereference**: `*expr`

#### Complex Expressions
- **Function Call**: `func(arg1, arg2, named: value)`
- **Method Call**: `obj.method(args)`
- **Field Access**: `obj.field`
- **Index**: `arr[i]`
- **Struct Literal**: `Point { x: 1, y: 2 }`
- **Enum Variant**: `Color::Red`, `Option::Some(42)`
- **Array Literal**: `[1, 2, 3]`
- **Tuple Literal**: `(a, b, c)`
- **Range**: `0..10`, `0..=10`
- **If Expression**: `if cond: then_block else: else_block`
- **Match Expression**: `match x: 0 => "zero", _ => "other"`
- **Lambda**: `|x, y| x + y`
- **Reference**: `&value`, `&mut value`
- **Dereference**: `*ptr`
- **Cast**: `value as Float`
- **Try**: `result?`
- **Await**: `await future`
- **Spawn Actor**: `spawn Player { health: 100.0 }`
- **Send Message**: `send actor <- Damage { amount: 10.0 }`
- **Comptime**: `comptime { expr }`
- **Block**: `{ stmts }`
- **JSX**: `<div>{content}</div>`
- **Parenthesized**: `(expr)`

### 1.4 Statement Parsing

**Statement Types**:
- **Let Binding**: `let x: Int = 42`
- **Var Binding**: `var x: Int = 42` (mutable)
- **Expression Statement**: `println("hello")`
- **Return**: `return value`
- **Break**: `break`, `break value`
- **Continue**: `continue`
- **For Loop**: `for item in collection: body`
- **While Loop**: `while condition: body`
- **Loop**: `loop: body` (infinite loop)
- **Item Declaration**: Nested functions, structs, etc.

### 1.5 Type Parsing

**Type Syntax**:
- **Named Types**: `Int`, `Float`, `String`, `Vec<T>`
- **Tuple Types**: `(Int, Float, String)`
- **Array Types**: `[Int; 10]` (fixed size)
- **Slice Types**: `[Int]` (dynamic)
- **Reference Types**: `&T`, `&mut T`, `&'a T`
- **Function Types**: `fn(Int, Float) -> String with IO`
- **Option Shorthand**: `T?` → `Option<T>`
- **Result Shorthand**: `T!E` → `Result<T, E>`
- **Inferred**: `_`
- **Never**: `!`
- **Unit**: `()`
- **Impl Trait**: `impl Iterator<Item = T>`

### 1.6 Pattern Matching

**Pattern Types**:
- **Wildcard**: `_`
- **Literal**: `42`, `"hello"`, `true`
- **Binding**: `x`, `mut x`
- **Struct**: `Point { x, y }`, `Point { x, .. }`
- **Tuple**: `(a, b, c)`
- **Enum Variant**: `Some(x)`, `None`, `Color::Red`
- **Slice**: `[first, rest @ ..]`
- **Or Pattern**: `A | B | C`
- **Range**: `1..10`, `1..=10`

### 1.7 Attribute Parsing

**Attribute Syntax**: `@attr`, `@attr(arg1, arg2)`

**Supported Attributes**:
- `@wasm`, `@js`, `@inline`, `@extern`
- `@component`, `@shader`, `@actor`
- `@material_graph`, `@material_function`
- `@graph_editor`, `@graph_runtime`
- `@state_machine`, `@async_task`
- `@editor_module`
- `@gameplay_tags`, `@ability`, `@gameplay_effect`, `@gameplay_cue`
- `@replicated`, `@savegame`, `@transient`
- `@blueprint`, `@blueprint_callable`, `@blueprint_pure`
- `@tick`, `@beginplay`
- `@slate`, `@details`, `@viewport`, `@toolbar`

### 1.8 JSX Parsing (React-like UI)

**JSX Node Types**:
- **Element**: `<tag attr="value">children</tag>`
- **Self-Closing**: `<img src="path" />`
- **Component**: `<Button text="Click" />`
- **Expression**: `{variable}`
- **Text**: Plain text content
- **For Loop**: `for item in list: <li>{item}</li>`
- **If Statement**: `if cond: <div>yes</div> else: <div>no</div>`
- **Fragment**: `<>multiple children</>`

### 1.9 Special Parsing Features

#### Indentation-Based Blocks
KAIN uses Python-style indentation with INDENT/DEDENT tokens:
```kain
fn example():
    let x = 1
    if x > 0:
        println("positive")
    else:
        println("non-positive")
```

#### Operator Precedence Climbing
Binary expressions are parsed with correct precedence:
1. `**` (power)
2. `*`, `/`, `%`
3. `+`, `-`
4. `<<`, `>>`
5. `&`
6. `^`
7. `|`
8. `==`, `!=`, `<`, `>`, `<=`, `>=`
9. `and`
10. `or`

#### Error Recovery
Parser provides file:line:col diagnostics:
```
src/main.kn:15:8: Expected ':' after function signature
```

---

## 2. Type System (Resolved)

### 2.1 Resolved Type Representation

**File:** `Kain/crates/kain-core/src/types.rs`

After parsing, types are **resolved** into a canonical representation:

```rust
pub enum ResolvedType {
    Unit,
    Bool,
    Int(IntSize),
    Float(FloatSize),
    String,
    Char,
    Array(Box<ResolvedType>, usize),
    Slice(Box<ResolvedType>),
    Tuple(Vec<ResolvedType>),
    Option(Box<ResolvedType>),
    Result(Box<ResolvedType>, Box<ResolvedType>),
    Ref { mutable: bool, inner: Box<ResolvedType> },
    Function { params: Vec<ResolvedType>, ret: Box<ResolvedType>, effects: EffectSet },
    Struct(String, HashMap<String, ResolvedType>),
    Enum(String, Vec<(String, ResolvedType)>),
    Generic(String),
    Never,
    Unknown,
}
```

### 2.2 Integer Types

**IntSize Variants**:
- **Signed**: `I8`, `I16`, `I32`, `I64`, `I128`, `Isize`
- **Unsigned**: `U8`, `U16`, `U32`, `U64`, `U128`, `Usize`

**Default**: `Int` → `I64`

### 2.3 Float Types

**FloatSize Variants**:
- `F32` (32-bit float)
- `F64` (64-bit float)

**Default**: `Float` → `F64`

### 2.4 Built-in Types

**Automatically Registered**:
- `Int` → `ResolvedType::Int(IntSize::I64)`
- `Float` → `ResolvedType::Float(FloatSize::F64)`
- `Bool` → `ResolvedType::Bool`
- `String` → `ResolvedType::String`
- `Vec2` → `ResolvedType::Tuple([F32, F32])`
- `Vec3` → `ResolvedType::Tuple([F32, F32, F32])`

### 2.5 Type Checking

**Type Checker Features**:
- **Two-pass checking**: Register types first, then check items
- **Scope management**: Push/pop scopes for nested blocks
- **Type inference**: Infer types from expressions
- **Effect tracking**: Track side effects in function types
- **Generic detection**: Single uppercase letter or `_T` style
- **Struct field resolution**: HashMap of field names to types
- **Enum variant resolution**: HashMap of variant names to payload types

### 2.6 Type Environment

**TypeEnv** manages:
- **Scopes**: Stack of variable bindings
- **Types**: Global type registry (structs, enums, aliases)
- **Span Mapper**: For error reporting with file:line:col
- **Filename**: Current file being checked

**Operations**:
- `push_scope()` / `pop_scope()`: Manage lexical scopes
- `define(name, ty)`: Add variable to current scope
- `lookup(name)`: Find variable or type
- `type_error(msg, span)`: Create formatted error

### 2.7 Typed AST

**TypedProgram** contains:
- `TypedFunction`: Function with resolved type and effects
- `TypedStruct`: Struct with field types
- `TypedEnum`: Enum with variant payload types
- `TypedComponent`: Component with prop types
- `TypedShader`: Shader with input/output types
- `TypedActor`: Actor with state types
- `TypedConst`: Const with resolved type
- Plus: Macro, Use, Impl, Test, TypeAlias, MaterialGraph, GraphEditor, etc.

### 2.8 Syntax Error Detection

**Enum vs Struct Validation**:
The type checker detects incorrect use of `::` on struct types:

```kain
struct Point { x: Float, y: Float }

// ERROR: Cannot use '::' on struct type 'Point'
let p = Point::x  // Should be: point.x
```

Error message:
```
Cannot use '::' on struct type 'Point'. Use '.' for field access instead.
Example: point.x (not Point::x)
```

This validation runs on:
- Function bodies
- Actor handlers
- Actor methods
- Struct methods

---

## 3. Runtime & Interpreter

### 3.1 Runtime System

**File:** `Kain/crates/kain-core/src/runtime.rs`

The KAIN runtime provides **immediate execution** without compilation:

**Features**:
- **Interpreter**: Execute KAIN code directly from AST
- **REPL Support**: Interactive evaluation
- **Prototyping**: Rapid testing without build step
- **Debugging**: Step through code execution

### 3.2 Execution Model

**Runtime Capabilities**:
- Variable binding and lookup
- Function calls
- Expression evaluation
- Control flow (if, match, loops)
- Actor message passing
- Comptime evaluation

### 3.3 Value Representation

**Runtime Values**:
- Primitives: Int, Float, Bool, String
- Collections: Array, Tuple
- Structs: Field-based records
- Enums: Tagged unions
- Functions: Closures with captured environment
- Actors: Concurrent entities with mailboxes

### 3.4 Use Cases

**When to Use Runtime**:
- `kain run script.kn` — Immediate execution
- REPL sessions — Interactive development
- Testing — Quick validation
- Prototyping — Rapid iteration

**When to Use Compiler**:
- Production builds — Optimized binaries
- UE5 plugins — C++ codegen
- WASM/JS — Web deployment
- GPU shaders — HLSL/SPIR-V codegen

---

## 4. Shader Features

### 4.1 Shader Analysis

**File:** `Kain/crates/kain-core/src/shader_analysis.rs`

**ShaderComplexity** tracks:
- `alu_ops`: Arithmetic/logic operations
- `texture_samples`: Texture reads
- `branches`: Conditional branches

**Usage**:
```bash
kain build shader.kn --target usf --analyze
```

**Output**:
```
[MyShader] ALU: 45, Tex: 3, Branches: 2
```

### 4.2 Shader Stages

**Supported Stages**:
- `shader vertex` — Vertex shader
- `shader fragment` — Fragment/pixel shader
- `shader compute` — Compute shader
- `shader surface` — Surface shader (UE5)

### 4.3 Shader Inputs

**Uniform Declaration**:
```kain
shader compute Example(thread_id: Vec3):
    uniform grid_size: Int @0
    uniform noise_scale: Float @1
    buffer output: RWBuffer<Float> @2
```

**Binding Slots**: `@N` specifies register binding

### 4.4 Shader Targets

**Compilation Targets**:
- **SPIR-V**: Cross-platform GPU (→ WGSL, GLSL, Metal via naga)
- **HLSL**: DirectX shaders
- **USF**: Unreal Engine 5 shaders
- **WGSL**: WebGPU shaders (via SPIR-V)

---

## 5. Standard Library

### 5.1 Stdlib Organization

**File:** `Kain/crates/kain-core/src/stdlib.rs`

**Location**: `Kain/stdlib/ue5/*.kn`

**Discovery Order**:
1. `KAIN_STDLIB_PATH` environment variable
2. Walk up from exe location looking for `stdlib/ue5/`
3. Walk up from CWD looking for `stdlib/ue5/`
4. Explicit `KAIN.toml` override

### 5.2 Stdlib Categories

**12 Stdlib Files** (200+ functions):

| File | Functions | Coverage |
|------|-----------|----------|
| `shaders.kn` | 100+ | PBR, noise, color grading, UV ops, volumetric, SSS, post-processing, ray marching, SDF, procedural |
| `actor.kn` | 30+ | Actor lifecycle, transform, attachment, velocity, component access |
| `gameplay.kn` | 20+ | Damage, health, XP, inventory, cooldowns, buffs, loot, quests |
| `utilities.kn` | 20+ | Math helpers, remap, interpolation, random, string formatting |
| `world.kn` | 20+ | World queries, spawning, traces, debug drawing, game mode access |
| `skeletal_mesh.kn` | 20+ | Montages, bone manipulation, sockets, morph targets |
| `materials.kn` | 15+ | Material parameter control, dynamic materials, parameter collections |
| `particles.kn` | 15+ | Niagara variable binding, system control, pooling |
| `components.kn` | 10+ | Common component struct definitions (Health, Inventory, Movement, Combat) |
| `patterns.kn` | 12+ | Shared type definitions (LootRarity, BuffType, DamageType, WeaponStats) |
| `math.kn` | 11+ | Vector math, rotation, interpolation, type aliases |
| `common.kn` | 3+ | Core engine bindings (GetWorldDeltaSeconds, GetActorLocation) |

### 5.3 Stdlib Loading

**Automatic Prepending**:
All stdlib files are automatically prepended to every compilation. No imports needed.

**Example**:
```kain
// Your code
actor Player:
    state health: Float = 100.0
    
    on TakeDamage(amount: Float):
        // apply_damage is from stdlib/ue5/gameplay.kn
        health = apply_damage(health, amount, 0.0)
```

### 5.4 Compression Ratios

**Stdlib Impact**:
- **Base**: 1 line KAIN → 5-8 lines C++
- **With Stdlib**: 1 line KAIN → 20+ lines C++ (shader functions, gameplay patterns, actor bindings)

**Example**:
```kain
let f = fresnel_schlick(cos_theta, f0)  // 1 line
```
→ 8 lines HLSL with full Fresnel-Schlick implementation

### 5.5 Stdlib Functions (Sample)

**Shader Functions** (`shaders.kn`):
- `fresnel_schlick(cos_theta, f0)` — Fresnel reflection
- `perlin_noise(p)` — Perlin noise
- `fbm(p, octaves)` — Fractional Brownian motion
- `voronoi(p)` — Voronoi noise
- `sdf_sphere(p, radius)` — Sphere SDF
- `ray_march(origin, direction, max_steps)` — Ray marching
- `pbr_lighting(albedo, roughness, metallic, normal, view)` — PBR lighting

**Gameplay Functions** (`gameplay.kn`):
- `apply_damage(hp, dmg, armor)` — Damage calculation
- `calculate_xp(level, base_xp)` — XP calculation
- `check_cooldown(last_time, cooldown)` — Cooldown check
- `roll_loot(rarity, luck)` — Loot generation

**Actor Functions** (`actor.kn`):
- `GetActorLocation()` — Get actor position
- `SetActorLocation(loc)` — Set actor position
- `GetActorRotation()` — Get actor rotation
- `AttachToComponent(comp, socket)` — Attach to component

---

## 6. Source Tracking

### 6.1 Span System

**File:** `Kain/crates/kain-core/src/span.rs`

**Span** tracks source code locations:
```rust
pub struct Span {
    pub start: usize,  // Byte offset
    pub end: usize,    // Byte offset
}
```

**Operations**:
- `Span::new(start, end)` — Create span
- `span.merge(other)` — Merge two spans
- `span.to_range()` — Convert to Range<usize>

### 6.2 Spanned Values

**Spanned<T>** attaches span to any value:
```rust
pub struct Spanned<T> {
    pub value: T,
    pub span: Span,
}
```

**Usage**:
```rust
let token = Spanned::new(TokenKind::Ident("x".into()), Span::new(0, 1));
```

### 6.3 SpanMapper

**File:** `Kain/crates/kain-core/src/diagnostics.rs`

**SpanMapper** converts byte offsets to file:line:col:
```rust
pub struct SpanMapper {
    line_starts: Vec<usize>,
}
```

**Usage**:
```rust
let loc = span_mapper.span_to_location(span, "main.kn");
// → SourceLocation { file: "main.kn", line: 15, col: 8 }
```

### 6.4 Error Reporting

**Formatted Errors**:
```
src/main.kn:15:8: Type mismatch: expected Int, found Float
```

**Error Creation**:
```rust
fn type_error(&self, message: impl Into<String>, span: Span) -> KainError {
    let loc = self.span_mapper.span_to_location(span, self.filename);
    let formatted = format!("{}:{}:{}: {}", loc.file, loc.line, loc.col, message.into());
    KainError::type_error(formatted, span)
}
```

---

## 7. Testing Infrastructure

### 7.1 Stdlib Tests

**File:** `Kain/crates/kain-core/src/stdlib_tests.rs`

**Test Coverage**:
- Stdlib discovery (env var, walk-up, explicit path)
- File loading and parsing
- Error handling (missing files, invalid paths)
- Integration with compiler pipeline

### 7.2 Test Organization

**Test Categories**:
- **Unit Tests**: Individual function testing
- **Integration Tests**: Full pipeline testing
- **Property Tests**: Generative testing (future)
- **Regression Tests**: Bug fix validation

### 7.3 Test Syntax

**KAIN Test Syntax**:
```kain
test "addition works":
    assert(1 + 1 == 2)

test "division by zero":
    let result = 10 / 0
    assert(result.is_err())
```

**Test Runner**:
```bash
kain build --target test
```

### 7.4 Test Results

**Output Format**:
```
Running 3 tests...
✓ addition works
✓ division by zero
✗ multiplication fails
  Expected: 6, Got: 5

2 passed, 1 failed
```

---

## 8. Feature Summary

### 8.1 Parser Capabilities

| Feature | Status | Notes |
|---------|--------|-------|
| **Functions** | ✅ | With generics, effects, attributes |
| **Async Functions** | ✅ | `async fn` syntax |
| **Components** | ✅ | React-like UI with JSX |
| **Shaders** | ✅ | Vertex, fragment, compute, surface |
| **Actors** | ✅ | Erlang-style concurrency |
| **Structs** | ✅ | With methods, generics, attributes |
| **Enums** | ✅ | Unit, tuple, struct variants |
| **Traits** | ✅ | With default implementations |
| **Impls** | ✅ | Trait impls and inherent impls |
| **Type Aliases** | ✅ | Generic type aliases |
| **Macros** | ✅ | Declarative macros |
| **Comptime** | ✅ | Compile-time execution |
| **Tests** | ✅ | Built-in test framework |
| **Material Graphs** | ✅ | UE5 material system |
| **Graph Editors** | ✅ | UE5 graph editor system |
| **State Machines** | ✅ | Animation state machines |
| **Async Tasks** | ✅ | Thread pool tasks |
| **Editor Modules** | ✅ | UE5 editor extensions |
| **GAS Integration** | ✅ | Gameplay Ability System |

### 8.2 Type System Capabilities

| Feature | Status | Notes |
|---------|--------|-------|
| **Primitives** | ✅ | Int, Float, Bool, String, Char |
| **Collections** | ✅ | Array, Slice, Tuple |
| **References** | ✅ | `&T`, `&mut T`, lifetimes |
| **Functions** | ✅ | With effect tracking |
| **Structs** | ✅ | Named fields with types |
| **Enums** | ✅ | Tagged unions |
| **Generics** | ✅ | Type parameters |
| **Option/Result** | ✅ | `T?`, `T!E` shorthand |
| **Never Type** | ✅ | `!` for diverging functions |
| **Impl Trait** | ✅ | `impl Trait` syntax |
| **Type Inference** | ✅ | `_` placeholder |
| **Effect System** | ✅ | `with Pure`, `with IO` |

### 8.3 Runtime Capabilities

| Feature | Status | Notes |
|---------|--------|-------|
| **Interpreter** | ✅ | Direct AST execution |
| **REPL** | ✅ | Interactive evaluation |
| **Variable Binding** | ✅ | Let/var statements |
| **Function Calls** | ✅ | With closures |
| **Control Flow** | ✅ | If, match, loops |
| **Actor System** | ✅ | Message passing |
| **Comptime Eval** | ✅ | Compile-time execution |

### 8.4 Stdlib Capabilities

| Category | Functions | Status |
|----------|-----------|--------|
| **Shaders** | 100+ | ✅ |
| **Actor** | 30+ | ✅ |
| **Gameplay** | 20+ | ✅ |
| **Utilities** | 20+ | ✅ |
| **World** | 20+ | ✅ |
| **Skeletal Mesh** | 20+ | ✅ |
| **Materials** | 15+ | ✅ |
| **Particles** | 15+ | ✅ |
| **Components** | 10+ | ✅ |
| **Patterns** | 12+ | ✅ |
| **Math** | 11+ | ✅ |
| **Common** | 3+ | ✅ |

### 8.5 Tooling Capabilities

| Tool | Status | Notes |
|------|--------|-------|
| **Parser** | ✅ | Recursive descent with precedence climbing |
| **Type Checker** | ✅ | Two-pass with effect tracking |
| **Interpreter** | ✅ | Direct AST execution |
| **Compiler** | ✅ | 15+ targets (WASM, LLVM, UE5, etc.) |
| **LSP** | ✅ | Language server protocol |
| **Error Reporting** | ✅ | file:line:col diagnostics |
| **Span Tracking** | ✅ | Full source location tracking |
| **Stdlib Loading** | ✅ | Automatic discovery and prepending |
| **Test Runner** | ✅ | Built-in test framework |
| **Shader Analysis** | 🚧 | Complexity analysis (WIP) |

---

## 9. Code Examples

### 9.1 Full Language Feature Demo

```kain
// Imports
use std::collections::HashMap

// Constants
const MAX_HEALTH: Float = 100.0

// Type Alias
type Vec2 = (Float, Float)

// Enum
enum ItemRarity:
    Common
    Rare
    Epic
    Legendary

// Struct
struct Item:
    name: String
    rarity: ItemRarity
    value: Int

// Trait
trait Drawable:
    fn draw(self):
        println("Drawing...")

// Impl
impl Drawable for Item:
    fn draw(self):
        println(f"Drawing {self.name}")

// Function with effects
fn calculate_damage(base: Float, multiplier: Float) -> Float with Pure:
    return base * multiplier

// Async function
async fn fetch_data(url: String) -> String with IO:
    let response = await http_get(url)
    return response

// Component (React-like)
component Button(text: String, on_click: fn()) -> UI:
    <button onclick={on_click}>
        {text}
    </button>

// Shader
shader fragment ColorTint(uv: Vec2) -> Vec4:
    uniform base_color: Vec3 @0
    uniform albedo_map: Sampler2D @1
    
    let tex_color = sample(albedo_map, uv).rgb
    return vec4(tex_color * base_color, 1.0)

// Actor (Erlang-style)
actor Player:
    state health: Float = MAX_HEALTH
    state position: Vec2 = (0.0, 0.0)
    
    on TakeDamage(amount: Float):
        health = health - amount
        if health <= 0.0:
            Multicast_Die()
    
    on Multicast_Die():
        println("Player died!")

// Material Graph
@material_graph PBRMaterial:
    input albedo: Texture2D
    input roughness: Float = 0.5
    input metallic: Float = 0.0
    
    let base = texture_sample(albedo).rgb
    
    base_color = base
    roughness = roughness
    metallic = metallic

// Test
test "damage calculation":
    let dmg = calculate_damage(10.0, 2.0)
    assert(dmg == 20.0)

// Comptime
comptime:
    println("This runs at compile time!")
    let x = 1 + 1
    assert(x == 2)
```

---

## 10. Comparison with Part 1

**KAIN_FEATURES_PART1.md** covers:
- AST structure (Item, Expr, Stmt, Type, Pattern)
- Language constructs (functions, components, shaders, actors)
- UE5-specific features (materials, graphs, GAS)

**KAIN_FEATURES_PART2.md** (this document) covers:
- Parser implementation and capabilities
- Type system (resolved types, type checking)
- Runtime system (interpreter, execution model)
- Shader analysis
- Standard library (organization, functions, loading)
- Source tracking (spans, error reporting)
- Testing infrastructure

**Together**, these documents provide a **complete reference** for the KAIN language implementation in `kain-core`.

---

## 11. Future Enhancements

### 11.1 Planned Features

- **Full Shader Analysis**: Complete ALU/texture/branch counting
- **Property-Based Testing**: Generative test framework
- **Trait Type Checking**: Full trait resolution and checking
- **Incremental Compilation**: Cache type-checked modules
- **LSP Enhancements**: Better autocomplete, refactoring
- **Debugger Integration**: Step-through debugging
- **Profiler**: Runtime performance analysis

### 11.2 Stdlib Expansion

**Planned Categories**:
- **AI**: Behavior trees, pathfinding, decision making
- **Physics**: Collision, forces, constraints
- **Audio**: Sound playback, mixing, effects
- **Networking**: Replication, RPCs, serialization
- **UI**: Widget library, layout, styling
- **Animation**: Blending, IK, procedural
- **VFX**: Particle systems, trails, decals

---

## Conclusion

KAIN's `kain-core` crate provides a **complete language implementation** with:
- **Powerful parser** supporting 25+ item types and 40+ expression types
- **Robust type system** with effect tracking and generic support
- **Flexible runtime** for immediate execution and prototyping
- **Rich standard library** with 200+ functions across 12 categories
- **Comprehensive tooling** for error reporting, testing, and analysis

This foundation enables KAIN to compile to **15+ targets** (WASM, LLVM, UE5, GPU shaders) while maintaining a **clean, expressive syntax** inspired by Rust, Python, Lisp, and Zig.

For AST structure and language constructs, see **KAIN_FEATURES_PART1.md**.
For codegen backends and UE5 integration, see crate-specific documentation in `Kain/crates/*/CRATE_REFERENCE.md`.
