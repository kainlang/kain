# kain-import — C & Rust Importer Reference

> **Last Updated:** 2026-03-01
> **Status:** C importer production-grade. Rust importer active development. C++ / Python stubs planned.

---

## Purpose

Universal import system. Takes foreign-language source code, transforms it to KAIN IR, which can then be compiled to any KAIN backend target. The primary use cases are:

1. **C import** — import SDKs, game source, embedded firmware into KAIN
2. **Rust import** — reflexive import bootstrap for self-hosting KAIN in KAIN
3. **Assembly import** — via `kain-asm` (separate crate, feeds into same AST types)

---

## Architecture

```
Foreign Source
      ↓
  Language Parser  (lang-c / syn)
      ↓
 Transformer  (C: 131KB  |  Rust: 50KB)
      ↓
   common/  (c_registry, identifier_registry, type_mapper, preprocessor)
      ↓
 KAIN Program  (kain-core AST)
      ↓
 Any backend target
```

---

## Source Files

| File | Size | Purpose |
|---|---|---|
| `c/transformer.rs` | 131KB | `CTransformer` — C AST → KAIN AST (core of C importer) |
| `rust/transformer.rs` | 50KB | `RustTransformer` — syn AST → KAIN AST |
| `c/parser.rs` | 28KB | lang-c wrapper with preprocessor, pack-stack tracking |
| `rust/types.rs` | 11KB | `RustTypeMapper` — Rust types → KAIN types |
| `common/c_registry.rs` | 11KB | Data-driven C → KAIN type/operator mapping tables |
| `common/identifier_registry.rs` | 3KB | `StableIdentifierRenamer` — domain-scoped name dedup |
| `common/type_mapper.rs` | 3KB | Shared type utilities |
| `common/preprocessor.rs` | 2KB | `#pragma pack` / `#include` preprocessing helpers |
| `rust/mod.rs` | 5.5KB | Rust importer public API + reflexive bootstrap docs |
| `rust/parser.rs` | 545B | `syn::parse_file` wrapper |
| `c/types.rs` | 2.7KB | C primitive type→KAIN type helpers |
| `cpp/mod.rs` | 602B | C++ importer stub (future: tree-sitter-cpp) |
| `lib.rs` | 3.3KB | Public API surface + feature-gated module declarations |

---

## Cargo Features

```toml
[features]
default = ["c", "rust"]
c       = ["dep:lang-c"]
rust    = ["dep:syn", "dep:proc-macro2"]
cpp     = []        # Future
python  = []        # Future
```

---

## C Importer (`CTransformer`)

### Construction

```rust
CTransformer::new()
CTransformer::with_language_capabilities(caps)
CTransformer::with_language_capabilities_and_layout_metadata(caps, metadata)
```

`CSourceLayoutMetadata` carries per-file `#pragma pack` / `__attribute__((aligned))` information parsed from source.

### C Constructs → KAIN IR

| C construct | KAIN IR |
|---|---|
| `int`, `long`, `short`, etc. | `Type::Named("Int")` (data-driven via `C_TYPE_NAME_ALIASES`) |
| `char`, `unsigned char` | `Type::Named("Char")` |
| `float`, `double` | `Type::Named("Float")` |
| `void` | `Type::Unit` |
| `T*`, `T[]` | `Type::Ptr(T)` |
| `struct { ... }` | `Struct` item with optional `C_UNION_ATTR` / `C_BITFIELD_ATTR` metadata |
| `union { ... }` | `Struct` + `c_union` attribute |
| Bitfield `int x : 3` | Field with `c_bitfield(width, signed, storage_bits, storage_align, pack_align, explicit_align)` attribute |
| `enum { A, B }` | `Enum` item |
| `typedef` | `TypeAlias` item |
| `fn(T) -> U` | `Type::Function` |
| `&x`, `&arr[i]`, `&obj.field` | `Expr::AddrOf` |
| `ptr + i`, `ptr - i` | `Expr::PtrOffset` |
| `*ptr`, `ptr[i]` (read) | `Expr::MemLoad` |
| `*ptr = v`, `ptr[i] = v` | `Expr::MemStore` |
| `sizeof(T)` | `Expr::SizeOfType` |
| `_Alignof(T)` | `Expr::AlignOfType` |
| Local fixed arrays | `Expr::Alloca` |
| Uninitialized locals | `Expr::Uninit` |
| `malloc` / `calloc` | `Expr::Alloc` |
| `realloc` | `Expr::Realloc` |
| Designated initializers | `Expr::AggregateInit` |

### Layout Metadata Tracking

`c/parser.rs` tracks:
- `#pragma pack(N)` — simple push
- `#pragma pack(push, id, N)` / `#pragma pack(pop, id)` — named stack
- `__attribute__((packed))` — field packed flag
- `__attribute__((aligned(N)))` — explicit align override
- `_Alignof(T)` — queried through layout registry

### Data-Driven Operator Registry (`c_registry.rs`)

`C_BINARY_OPERATOR_MAPPINGS` and `C_BUILTIN_TYPE_SPECIFIERS` are static const tables — no scattered match arms. Adding new C operator or type mappings requires only a table row, not code changes.

`CBinaryOperatorResolution` distinguishes:
- Supported mappings (direct `BinaryOp` emit)
- Unsupported assignment operators (lowered to separate load/store)
- Compound assignment lowering operators

### Identifier Registry

`StableIdentifierRenamer` provides domain-scoped rename deduplication:
- Domains: `Value`, `Type`, `Field`, `Variant`
- Avoids KAIN keyword collisions
- Produces stable names across incremental imports

---

## Rust Importer (`RustTransformer`)

Uses `syn 2.0` with `full` feature — full Rust AST including all expression forms.

### Rust Items → KAIN Items

| Rust | KAIN |
|---|---|
| `fn foo()` | `Function` (unsafe → `Effect::Unsafe`, async → `Effect::Async`) |
| `struct Foo { ... }` | `Struct` |
| `struct Foo(T)` | `Struct` (tuple variant) |
| `struct Foo;` | `Struct` (unit) |
| `enum Foo { A, B(T), C { x: T } }` | `Enum` with unit/tuple/struct variants |
| `impl Foo { fn bar() }` | `Impl` with methods |
| `impl Trait for Foo` | `Impl` with trait noted |
| `const FOO: T = v` | `Const` |
| `static FOO: T = v` | `Const` |
| `type Foo = Bar` | `TypeAlias` |
| `mod foo { ... }` | `Mod` (inline) |
| `mod foo;` | noted for CLI multi-file handling |
| `use ...`, `extern crate ...` | skipped (structural resolution) |
| `trait Foo { ... }` | stub noted |
| `macro_rules! ...` | skipped |

### Rust Types → KAIN Types (`RustTypeMapper`)

| Rust type | KAIN type |
|---|---|
| `bool` | `Bool` |
| `String`, `str`, `&str` | `String` |
| `f32`, `f64` | `f32`, `f64` |
| `u8..u128`, `i8..i128`, `usize`, `isize` | Matching KAIN numeric types |
| `Vec<T>` | `Array<T>` |
| `Option<T>` | `Option<T>` |
| `Result<T,E>` | `Result<T,E>` |
| `Box<T>`, `Arc<T>`, `Rc<T>`, `Cell<T>`, `RefCell<T>` | Transparent (inner T) |
| `HashMap<K,V>`, `BTreeMap<K,V>` | `Map<K,V>` |
| `HashSet<T>`, `BTreeSet<T>` | `Set<T>` |
| `&T`, `&mut T` | `Type::Ref` / `Type::RefMut` (lifetime erased) |
| `*const T`, `*mut T` | `Type::Ptr` / `Type::PtrMut` (low-level layer) |
| `impl Trait`, `dyn Trait` | `Type::Impl("Trait")` |
| `()` | `Type::Unit` |
| `!` | `Type::Never` |

### Reflexive Import Bootstrap (Self-Hosting Pipeline)

The Rust importer's primary purpose beyond ordinary FFI is enabling KAIN to import its own compiler source:

```
kain import-rust ./crates/kain-core/src   →  kain-core.kn
kain import-rust ./crates/ue5/src         →  ue5.kn
... (all crates)
    ↓
kain build -t rust kain-core.kn           →  kain-core-generated.rs
    ↓
cargo build (kain-core-generated.rs)      →  if tests pass: self-hosting proven
```

---

## Public API

```rust
// C importer
pub fn import_c_file(path: &Path) -> Result<Program>
pub fn import_c_dir(dir: &Path, options: CImportOptions) -> Result<Program>
pub fn import_c_from_source(source: &str, filename: &str) -> Result<Program>

// Rust importer
pub fn import_rust_file(path: &Path) -> Result<Program>
pub fn import_rust_dir(dir: &Path, flat: bool) -> Result<Program>
pub fn import_rust_project(paths: &[&Path]) -> Result<Program>
```

---

## ABI Corpus Tests

Data-driven conformance suite in `tests/abi_corpus/`:

| Fixture | Coverage |
|---|---|
| `pragma_pack.c` | `#pragma pack(1)` layout — sizeof = 5 |
| `aligned_attr.c` | `__attribute__((aligned(16)))` |
| `named_pack_stack.c` | Push/pop pack stack by name |
| `bitfield_promotion.c` | Integer promotion through bitfield read |
| `union_pair.c` | Non-scalar union member reinterpretation |

To add a new fixture: drop a `.c` file in `tests/abi_corpus/`, add a row to `manifest.json`. No Rust test code needed.

---

## Tests

- `tests/c_abi_conformance.rs` — C importer ABI correctness
- `tests/c_abi_corpus.rs` — manifest-driven corpus runner
- `common/c_registry.rs` inline tests (4) — type mapping, operator resolution
