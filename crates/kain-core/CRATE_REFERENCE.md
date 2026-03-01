# kain-core — KAIN Language Core Reference

> **Last Updated:** 2026-03-01
> **Status:** Production — the heart of the compiler. All backends depend on this crate.

---

## Purpose

The language core. Provides the lexer, parser, AST, type checker, effect system, standard library loader, monomorphizer, interpreter/runtime, and the full low-level memory + ABI pipeline that every backend and importer uses.

---

## Architecture

```
Source (.kn)
    ↓ Lexer (logos)
Token stream
    ↓ Parser (chumsky, 272KB)
Program (AST)
    ↓ comptime::eval_program
Program (comptime expanded)
    ↓ types::check
TypedProgram
    ↓ monomorphize::monomorphize
MonomorphizedProgram
    ↓ low_level_memory::lower_typed_program_memory_for_target
Lowered MonomorphizedProgram
    ↓
Backend crate (web / sys / gpu / ue5 / …)
```

---

## Source Files

| File | Size | Purpose |
|---|---|---|
| `parser.rs` | 272KB | Recursive-descent parser — the largest file in the codebase |
| `runtime.rs` | 138KB | Tree-walking interpreter for `kain run` / `kain test` |
| `low_level_memory.rs` | 88KB | Low-level memory semantics, ABI layout, helper lowering |
| `monomorphize.rs` | 66KB | Generic instantiation, type unification, async lowering |
| `ast.rs` | 67KB | AST definitions — all node types |
| `stdlib.rs` | 26KB | Data-driven stdlib loader with target-profile mapping |
| `types.rs` | 24KB | Type checker, `TypeEnv`, `ResolvedType`, `TypedProgram` |
| `error.rs` | 25KB | `KainError`, `DiagnosticBuilder`, error kinds |
| `diagnostics.rs` | 14KB | `SpanMapper`, `Diagnostics`, ariadne-backed error rendering |
| `effects.rs` | 5KB | Effect system — `Effect` enum, `EffectSet`, call checking |
| `low_level_abi.rs` | 9KB | C ABI policies, compiler flavors, arithmetic conversions |
| `low_level_memory_metadata.rs` | 2KB | `CSourceLayoutMetadata` — pragma pack / aligned attr tracking |
| `language_features.rs` | 7KB | Data-driven `LanguageCapabilities` registry |
| `comptime.rs` | 5KB | Compile-time expression evaluator (Zig-style comptime) |
| `lexer.rs` | 11KB | `Lexer` using `logos` |
| `stdlib.rs` | 26KB | Stdlib loading with per-target profile system |
| `asm_ir.rs` | 2KB | `AsmProgram`, `AsmBlock`, `AsmInstr`, `ParityTraceFrame` |
| `shader_analysis.rs` | 1KB | Shader complexity analysis helpers |
| `span.rs` | 1KB | `Span` type (byte offset range) |

---

## Compilation Targets (`CompileTarget`)

Defined in `lib.rs`, used by every crate:

```
Wasm | Js | Ts | Hybrid | Llvm | Rust | Cpp
Ue5 | Ue5Editor | Usf | Spirv | Hlsl
Interpret | Test | Ks
```

Total: **15 targets**. `from_str()` accepts aliases (e.g. `"unreal"` → `Ue5`, `"kainscript"` → `Ks`).

---

## AST — `Item` Variants

Every top-level KAIN construct is an `Item`:

| Variant | KAIN syntax |
|---|---|
| `Function` | `fn name(args) -> Type with Effects` |
| `Component` | `component Name(props) -> UI with Reactive` |
| `Shader` | `shader Name(inputs) -> Fragment with GPU` |
| `Actor` | `actor Name: state, handlers` |
| `Struct` | `struct Name: fields` |
| `Enum` | `enum Name: variants` |
| `Impl` | `impl Name: methods` |
| `TypeAlias` | `type Name = ...` |
| `Trait` | `trait Name: methods` |
| `Const` | `const NAME: Type = value` |
| `Use` | `use path::to::thing` |
| `Macro` | `macro name!(args)` |
| `Mod` | `mod name: items` |
| `Comptime` | `comptime: block` |
| `Test` | `test "description": block` |
| `GraphRuntime` | `@graph_runtime graph Name` |
| `StateMachine` | `@state_machine Name: states` |
| `AsyncTask` | `@async_task Name` |
| `EditorModule` | `@editor_module Name` |
| `GameplayTags` | `@gameplay_tags namespace Name` |
| `GameplayAbility` | `@ability struct Name` |
| `GameplayEffect` | `@effect struct Name` |
| `GameplayCue` | `@gameplay_cue struct Name` |
| `AbilityTask` | `@ability_task struct Name` |
| `TargetActor` | `@target_actor struct Name` |

---

## AST — `Expr` Variants (selected)

Core expressions plus the full low-level tier:

**High-level:** `Int`, `Float`, `String`, `FString`, `Bool`, `None`, `Ident`, `Binary`, `Unary`, `Call`, `MethodCall`, `Field`, `Index`, `Struct`, `EnumVariant`, `Array`, `Tuple`, `Range`, `If`, `Match`, `Lambda`, `Cast`, `Try`, `Await`, `Spawn`, `SendMsg`, `Comptime`, `JSX`, `Block`, `Ref`, `Deref`

**Low-level (new):**
| Expr | Meaning |
|---|---|
| `AddrOf { value, pointee_ty }` | `addr_of(x)` / `&x` |
| `PtrOffset { pointer, offset, element_ty }` | `ptr_offset(p, i)` |
| `MemLoad { pointer, load_ty }` | `mem_load(p)` / `*p` |
| `MemStore { pointer, value, store_ty }` | `mem_store(p, v)` / `*p = v` |
| `SizeOfType { target }` | `sizeof_type("T")` — layout-backed |
| `AlignOfType { target }` | `alignof_type("T")` — layout-backed |
| `Alloca { ty }` | Explicit stack allocation |
| `Uninit { ty }` | Explicit uninitialized storage |
| `Alloc { size, ty, zeroed }` | Heap alloc (`malloc`/`calloc`) |
| `Realloc { pointer, size, ty, zeroed_new }` | Heap realloc |
| `AggregateInit { ... }` | C-style designated struct initializer |

---

## `Type` Enum

```
Unit | Bool | Int | Float | String | Char
Array(T, N) | Slice(T) | Option(T) | Result(T,E)
Tuple([T]) | Function { params, return_type, effects }
Named(String) | Generic(String) | Impl(String)
Ref(T) | RefMut(T) | Ptr(T) | PtrMut(T)   ← low-level
```

`ResolvedType` mirrors this with concrete sizes: `IntSize` (I8..I128+U8..U128+Isize/Usize), `FloatSize` (F32/F64).

---

## Effect System

**8 effects:**

| Effect | Meaning |
|---|---|
| `Pure` | No side effects |
| `IO` | File / Network / Console |
| `Async` | Can `await` |
| `GPU` | Runs on graphics hardware |
| `Reactive` | Triggers UI updates |
| `Unsafe` | Breaks safety guarantees — can call anything |
| `Alloc` | Memory allocation |
| `Panic` | Can abort |

**`EffectSet`** tracks a set of active effects. `can_call()` enforces the lattice. `Unsafe` is the top element. `Pure` is the bottom.

---

## Low-Level Memory Pipeline

### Layout Registry

`StructLayoutInfo` tracks per-struct:
- Total `size` and `align`
- Per-field `offset` list
- Bitfield widths, storage bits, storage align
- Packed / union flags

### Backend Capabilities

`BackendMemoryCapabilities` per `CompileTarget`:
- `supports_raw_pointers` — LLVM/Cpp/Rust: yes; Wasm/JS/TS: via runtime bridge
- `supports_alloca` — native targets only
- `emits_helpers` — JS/TS/Wasm/UE5: emit `__kain_*` runtime calls

### Lowering Passes

- `lower_typed_program_memory_for_target(program, target)` — full program pass
- `validate_typed_program_memory_support(program, target)` — validation-only
- `lower_addr_of_memory`, `lower_union_aggregate_fields`, `lower_bitfield_access` — per-node

### Runtime Helper Names

```
__kain_addr_of          __kain_ptr_offset       __kain_mem_load
__kain_mem_store        __kain_alloc            __kain_realloc
__kain_bind_local       __kain_field_ptr        __kain_index_ptr
__kain_union_wrap       __kain_union_get        __kain_union_set
__kain_bitfield_get     __kain_bitfield_set
```

---

## ABI Layer (`low_level_abi.rs`)

Data-driven policy table with 10 entries covering:

| Kind | Flavor | Long bits |
|---|---|---|
| LP64 / LLP64 | Generic / GCC / Clang / MSVC | 64 / 32 |

Selection: `$env:KAIN_C_ABI_FLAVOR = "msvc"` (or `gcc`, `clang`, `generic`).

Target mapping: most targets → LP64; `Ue5` / `Ue5Editor` → LLP64.

Also provides: `promoted_integer_bits`, `usual_arithmetic_conversion_type`, `arithmetic_domain_for_type`, `should_apply_usual_arithmetic_conversions`.

---

## Monomorphizer (`monomorphize.rs`)

- Generic function instantiation via `MonoContext::instantiate()`
- Type argument unification: `unify(param_type, arg_type, bindings)`
- Type argument inference: `infer_type_args()` fills in missing type args from call site
- Struct monomorphization: `instantiate_struct()`
- Async function lowering: `lower_async_fn()` — chops async functions at `await` points into a state machine
- Name mangling: `mangle_types()` creates disambiguating suffixes

---

## Standard Library Loader (`stdlib.rs`)

Data-driven profile system:

```rust
TARGET_PROFILE_ORDER: &[(CompileTarget, &[&str])] = &[
    (Ue5, &["ue5", ""]),
    (Wasm, &[""]),
    (Ks, &[""]),   // KainScript shares JS stdlib
    // ...
]
```

Search roots (in priority order):
1. `$KAIN_STDLIB_PATH` env var
2. Sibling `stdlib/` of the compiler binary
3. Workspace `stdlib/`

Per-target profile loads `.kn` files from `stdlib/<profile>/` alphabetically, concatenates them, prepends to user source.

`load_stdlib_for_target(target)` is called at the start of every compilation.

---

## Language Capabilities (`language_features.rs`)

Data-driven `LanguageCapability` flags controlling parser and runtime behavior:

| Capability | Default |
|---|---|
| `ParserStructLiterals` (`Type { field: val }`) | **disabled** |
| `ParserBitwiseAnd/Or/Xor` | enabled |
| `ParserShiftLeft/Right` | enabled |
| `RuntimeBitwiseAnd/Or/Xor/Shl/Shr` | enabled |

Controlled via `LanguageCapabilities::with_override()`. Global default via `DEFAULT_LANGUAGE_CAPABILITIES` (once-cell lazy static).

---

## Diagnostics (`diagnostics.rs`)

- `SpanMapper` — maps byte-offset `Span` values to `(line, col)` with precomputed line-start table
- `Diagnostics` — renders errors with source context and caret (`ariadne`-style)
- `enhance_error_with_location()` — upgrades a codegen error with human-readable `file:line:col`

`DiagnosticCode` enum in `diagnostic_registry.rs` — typed error codes for all error categories.

---

## Runtime (`runtime.rs`, 138KB)

Tree-walking interpreter used by `kain run` and `kain test`. Supports:
- All expression and statement forms
- Actor spawn / message dispatch
- Async/await (coroutine-style)
- Python FFI via pyo3: `py_call("math.sqrt", [16.0])`
- HTTP: `http_get`, `http_post`
- File I/O: `read_file`, `write_file`
- Actor mailbox with `flume` channels

---

## Dependencies

| Crate | Role |
|---|---|
| `logos` | Lexer tokenization |
| `chumsky` | Parser combinators |
| `ariadne` | Error rendering |
| `indexmap` | Ordered maps |
| `petgraph` | Graph algorithms (dependency resolution) |
| `serde` / `serde_json` | Metadata serialization |
| `pyo3` | Python FFI |
| `flume` | Actor channel implementation |
| `tokio` | Async runtime for interpreter |
| `reqwest` | HTTP stdlib functions |
| `once_cell` | Lazy statics |

---

## Tests

- `stdlib_tests.rs` — 10+ stdlib loading tests (env var priority, profile fallback, alphabetical ordering, README exclusion)
- `kain-core/tests/ptr_type_test.rs` — low-level pointer type tests
- `low_level_abi.rs` inline tests — ABI table resolution, flavor parsing
- `diagnostics.rs` inline tests — SpanMapper (8 tests)
- `language_features.rs` inline tests — capability defaults
