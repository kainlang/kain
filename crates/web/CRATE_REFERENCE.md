# web — Web Backend Crates Reference

> **Last Updated:** 2026-03-01
> **Status:** Production — 5 web codegen backends, all sharing the kain-core `TypedProgram` interface.

---

## Purpose

All web compilation targets for KAIN. Takes a `TypedProgram` from `kain-core` and generates the appropriate web output:

| Target | Backend | Output |
|---|---|---|
| `wasm` | `WasmCompiler` (walrus) | `.wasm` binary bytes |
| `js` | `JSGen` | ES6+ `.js` |
| `ts` | `TSGen` | Strict TypeScript `.ts` |
| `ks` | `KsGen` | KainScript `.ks` (JS + JSDoc) |
| `hybrid` | `HybridGen` | WASM module + JS glue |

---

## Source Files

| File | Size | Purpose |
|---|---|---|
| `codegen_wasm.rs` | 114KB | `WasmCompiler` — walrus IR emission, bump alloc, struct layout |
| `codegen_ts.rs` | 83KB | `TSGen` — strict TypeScript with discriminated unions |
| `codegen_ks.rs` | 58KB | `KsGen` — KainScript (ES2022 + JSDoc) |
| `codegen_js.rs` | 37KB | `JSGen` — plain ES6+ JavaScript |
| `codegen_hybrid.rs` | 15KB | `HybridGen` — WASM + JS bridge |

---

## Public API

All backends expose a single `generate()` function:

```rust
// web/src/lib.rs re-exports:
pub use codegen_wasm::generate as generate_wasm;   // -> KainResult<Vec<u8>>
pub use codegen_js::generate   as generate_js;     // -> KainResult<String>
pub use codegen_ts::generate   as generate_ts;     // -> KainResult<String>
pub use codegen_hybrid::generate as generate_hybrid; // -> KainResult<String>
pub use codegen_ks::generate   as generate_ks;     // -> KainResult<String>
```

All text backends take `&TypedProgram` and return `KainResult<String>`. WASM returns `KainResult<Vec<u8>>`.

---

## WASM Backend (`codegen_wasm.rs`, 114KB)

### Architecture

Uses `walrus` crate for WASM module building — modules have a proper IR rather than raw binary encoding.

Key structures:
- `WasmCompiler` — stateful compiler with symbol table, struct/enum/component layout caches, string pool, lambda table
- `CompilationContext` — per-function locals map, separated from builder to avoid self-borrow

### Memory Model

**Bump allocator** at `heap_ptr` global (8-byte aligned):
```
old_ptr = heap_ptr
heap_ptr = (heap_ptr + size + 7) & ~7
return old_ptr
```

### Struct/Enum Layout

Pre-computed in `compute_struct_layout()` / `compute_enum_layout()` before codegen. Enum layout uses discriminant (u32) + union of variant payloads. `type_size_of()` maps `ResolvedType` to byte widths.

### Lambda Collection

Two-pass: `collect_lambdas_in_block/stmt/expr` pre-scans the AST for `Expr::Lambda`, assigns each a `funcref` table index, then `compile_lambda()` generates the WASM function. Closures captured by value (not reference currently).

### String Pooling

`collect_strings_in_block/stmt/expr` pre-scans for string literals, writes them into WASM data segment, maps each to a `(ptr, len)` pair accessed at runtime.

### JSX Support

`compile_jsx_node()` available — JSX Element, Text, Expression embed, Fragment, If.

### Low-Level Memory

`generate()` calls `lower_typed_program_memory_for_target(program, CompileTarget::Wasm)` before emitting. Union/bitfield nodes become `__kain_union_*` / `__kain_bitfield_*` runtime calls. Verified by the inline test `wasm_generate_handles_lowered_union_and_bitfield_memory_helpers`.

### Dependencies
- `walrus` — WASM IR builder + binary serializer

---

## TypeScript Backend (`codegen_ts.rs`, 83KB)

**`TSGen`** — full TypeScript with strict types.

### KAIN → TypeScript Mapping

| KAIN | TypeScript |
|---|---|
| `fn foo(x: Int) -> String` | `function foo(x: number): string` |
| `struct Foo { x: Int }` | `interface Foo { readonly x: number }` + `function Foo(x: number): Foo` |
| `enum Color { Red, Green(Int) }` | Discriminated union: `type Color = { tag: "Red" } \| { tag: "Green"; value: number }` + typed constructors |
| `component Counter(count: Int)` | `function Counter({ count }: { count: number }): React.ReactElement` |
| `impl Foo { fn bar() }` | `class Foo { bar() { ... } }` |
| JSX | Typed React JSX |
| Pattern match | `if`/`else if` chains with type narrowing |

### Type Mapping (`type_to_ts` / `resolved_type_to_ts`)

```
Int/Float → number
String → string
Bool → boolean
Option<T> → T | null
Result<T,E> → T  (E side not exposed — simplification)
Array<T> → T[]
Tuple → [T1, T2, ...]
Function → (...args) => R
Unit → void
```

### Features
- Enum discriminated unions with constructor helpers (`Color.Red()`, `Color.Green(42)`)
- Pattern matching via chained `if (x.tag === "Red")` guards
- JSX full support: element, text, expression, fragment, conditional
- `gen_typed_function` uses `TypedFunction` (post-type-check), `gen_function` uses raw `Function`

### Tests
- `test_string_escaping`, `test_string_with_quotes` — inline string escaping tests

---

## KainScript Backend (`codegen_ks.rs`, 58KB)

KainScript is "the best of both worlds" — pure ES2022 JavaScript that runs anywhere, but with embedded JSDoc types so any TypeScript-aware editor gives full autocomplete.

### Key Difference from TS Backend
- **No TypeScript compiler needed** — output is plain `.js`
- **JSDoc annotations** instead of TypeScript syntax
- Types via `@typedef`, `@param {T}`, `@returns {T}`

### KAIN → KainScript Mapping

| KAIN | KainScript |
|---|---|
| `struct Foo { x: Int }` | `/** @typedef {{ x: number }} Foo */` + `class Foo { constructor(x) { this.x = x; } }` |
| `enum Color { Red, Green(Int) }` | `/** @typedef {{ tag: 'Red' } \| { tag: 'Green', value: number }} Color */` + const variant factories |
| `fn foo(x: Int) -> String` | `/** @param {number} x @returns {string} */ function foo(x) { ... }` |
| `component Button(label: String)` | Documented React-style function returning `Element` |

### Type Doc (`type_jsdoc` / `resolved_jsdoc`)

```
Int → number
String → string
Array<T> → T[]
Option<T> → (T | null)
Result<T,E> → T
Tuple → [T1, T2]
Function → function(...): R
Named("T") → T
```

---

## JavaScript Backend (`codegen_js.rs`, 37KB)

Minimal ES6+ output — no type annotations, no JSDoc.

### KAIN → JS

| KAIN | JavaScript |
|---|---|
| `fn foo(x, y)` | `function foo(x, y) { ... }` |
| `struct Foo { x, y }` | `class Foo { constructor(x, y) { this.x = x; ... } }` |
| `enum` | Object with tag strings + factory functions |
| JSX | `React.createElement(...)` calls |
| `match` | `if`/`else if` chain with tag string checks |

Calls `lower_typed_program_memory_for_target(Js)` and `validate_typed_program_memory_support(Js)` before emitting.

---

## Hybrid Backend (`codegen_hybrid.rs`, 15KB)

Generates **WASM module + JS bridge glue** for full-stack web applications. The WASM module handles computation-heavy code; the JS glue handles DOM, I/O, and coordination.

---

## Dependencies

| Crate | Role |
|---|---|
| `kain-core` | `TypedProgram`, `AST`, effects |
| `walrus` | WASM module IR + binary serialization |
| `serde` / `serde_json` | Metadata (hybrid target) |
