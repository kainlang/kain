# sys — System Backend Crates Reference

> **Last Updated:** 2026-03-01
> **Status:** Production (C++, Rust). LLVM backend present but `inkwell` dep temporarily disabled; `codegen_llvm.rs` emits textual LLVM IR instead.

---

## Purpose

System compilation targets for KAIN — generating low-level native code:

| Target | Backend | Output |
|---|---|---|
| `llvm` | `LlvmGen` | Textual LLVM IR (`.ll`) |
| `rust` | `RustGen` | Rust source + `Cargo.toml` |
| `cpp` | `CppGen` | C++17 source |

---

## Source Files

| File | Size | Purpose |
|---|---|---|
| `codegen_llvm.rs` | 66KB | `LlvmGen` — LLVM IR text emission (inkwell disabled) |
| `codegen_rust.rs` | 34KB | `RustGen` — Rust transpiler + Cargo.toml gen |
| `codegen_cpp.rs` | 33KB | `CppGen` — C++17 transpiler |

---

## Public API

```rust
pub use codegen_llvm::generate as generate_llvm;  // -> KainResult<String>
pub use codegen_rust::generate as generate_rust;  // -> KainResult<String>
pub use codegen_cpp::generate  as generate_cpp;   // -> KainResult<String>
```

All backends take `&TypedProgram` and return `KainResult<String>`.

---

## LLVM Backend (`codegen_llvm.rs`, 66KB)

> **Note:** `inkwell` (the Rust LLVM bindings) is **commented out** in `Cargo.toml` with the note "Temporarily disabled to unblock build". The feature flag `llvm = []` exists but is empty.
> The backend emits **textual LLVM IR** rather than using the inkwell IR builder API.

### Key Structures
- `LlvmGen` — stateful generator with type table, function table, global string table, local variable counter
- Emits `.ll` IR directly as strings

### KAIN → LLVM IR

| KAIN | LLVM IR |
|---|---|
| `fn foo(x: Int) -> Int` | `define i64 @foo(i64 %x) { ... }` |
| `struct Foo { x: Int }` | `%Foo = type { i64 }` |
| `let x = 42` | `%x = alloca i64; store i64 42, i64* %x` |
| `x + y` | `%tmp = add i64 %x, %y` |
| Actors | `%ActorMailbox` struct + `@actor_send` calls |
| Closures | Lambda → function pointer + closure struct |
| Ref counting | `__kain_rc_incref`, `__kain_rc_decref` inline |

### Actor Support

Actors generate:
- `%ActorName_State` — struct type for state fields
- `@ActorName_send` — message dispatch function
- `@ActorName_new` — constructor

### Low-Level Memory Helpers

`write_low_level_memory_helpers()` emits:
```llvm
define i8* @__kain_alloc(i64 %size) { ... }
define i8* @__kain_realloc(i8* %ptr, i64 %size) { ... }
; __kain_ptr_offset, __kain_mem_load, __kain_mem_store
; __kain_union_wrap, __kain_union_get, __kain_union_set
; __kain_bitfield_get, __kain_bitfield_set
```

---

## Rust Backend (`codegen_rust.rs`, 34KB)

Full Rust transpiler. The output is valid Rust that compiles with `rustc` / `cargo build`.

### KAIN → Rust

| KAIN | Rust |
|---|---|
| `fn foo(x: Int) -> Int` | `fn foo(x: i64) -> i64` |
| `fn foo() with Async` | `async fn foo()` |
| `fn foo() with Unsafe` | `unsafe fn foo()` |
| `struct Foo { x: Int }` | `#[derive(Debug, Clone)] struct Foo { x: i64 }` |
| `enum Color { Red, Green(Int) }` | `enum Color { Red, Green(i64) }` |
| `impl Foo { fn bar() }` | `impl Foo { fn bar() { ... } }` |
| `match x { 1 => ... }` | `match x { 1 => ... }` |
| `let x: Int = 5` | `let x: i64 = 5;` |
| `x?` | `x?` |
| `await f()` | `f().await` |

### Type Mapping (`map_type`)

```
Int → i64
Float → f64
Bool → bool
String → String
Char → char
Array<T> → Vec<T>
Option<T> → Option<T>
Result<T,E> → Result<T,E>
Tuple([T]) → (T1, T2, ...)
Ref<T> → &T
RefMut<T> → &mut T
Ptr<T> → *const T
PtrMut<T> → *mut T
```

### Low-Level Helpers

`write_low_level_memory_helpers()` emits inline Rust unsafe helper functions for:
- `__kain_alloc`, `__kain_realloc`
- `__kain_ptr_offset`, `__kain_mem_load`, `__kain_mem_store`
- `__kain_union_wrap`, `__kain_union_get`, `__kain_union_set`
- `__kain_bitfield_get`, `__kain_bitfield_set`

### Cargo.toml Generation

`gen_cargo_toml(name, deps)` emits a minimal `Cargo.toml` for the transpiled project:

```toml
[package]
name = "my-project"
version = "0.1.0"
edition = "2021"
```

---

## C++ Backend (`codegen_cpp.rs`, 33KB)

Modern C++17 output. **Not** UE5 — use `ue5` crate for Unreal output.

### KAIN → C++17

| KAIN | C++ |
|---|---|
| `fn foo(x: Int) -> Int` | `int64_t foo(int64_t x) { ... }` |
| `struct Foo { x: Int }` | `struct Foo { int64_t x; };` |
| `enum Color { Red, Green(Int) }` | `enum class Color { Red, Green };` + `std::variant<...>` |
| `impl Foo { fn bar() }` | `class Foo { ... };` with methods |
| `Option<T>` | `std::optional<T>` |
| `Array<T>` | `std::vector<T>` |
| `String` | `std::string` |
| `match` | `if`/`else if` chains (no KAIN→C++ `switch` currently) |

### Notable Feature

`gen_block_with_implicit_return(block, implicit_return: bool)` — handles KAIN's "last expression is return value" semantics by optionally prepending `return` to the last statement.

### Low-Level Helpers

`write_low_level_memory_helpers()` emits inline C++ helpers matching the same `__kain_*` contract as Rust and LLVM backends:

```cpp
inline void* __kain_alloc(size_t size) { return malloc(size); }
inline void* __kain_realloc(void* ptr, size_t size) { return realloc(ptr, size); }
// __kain_ptr_offset, __kain_mem_load, __kain_mem_store
// __kain_union_wrap, __kain_union_get, __kain_union_set
// __kain_bitfield_get, __kain_bitfield_set
```

### Tests

- `test_type_mapping` — verifies core KAIN type → C++ type mappings

---

## Known Gaps

| Gap | Affects |
|---|---|
| LLVM `inkwell` dep disabled | `kain build -t llvm` produces textual IR only, not binary `.o` / ELF |
| No `Cargo.toml` workspace integration for Rust output | Generated Rust is standalone, not integrated into a workspace |
| No PCH / `#include` deduplication in C++ | May produce redundant headers |

---

## Dependencies

| Crate | Role |
|---|---|
| `kain-core` | `TypedProgram`, `AST`, low-level memory lowering |
| `inkwell` (DISABLED) | Would enable real LLVM module compilation |
