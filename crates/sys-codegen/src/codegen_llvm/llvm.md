# LLVM Codegen Architecture & Function Catalog

**`X:/crates/sys-codegen/src/codegen_llvm/`**  
**Status:** Complete wire-level documentation of the Kain→LLVM IR codegen pipeline  
**Generated:** 2026-07-04  
**Basis:** Full read of `mod.rs` (904 KB, ~21,886 lines), `component.rs` (84 KB, ~2,002 lines), and all 10 TSV chunk files

---

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Entry Points: `generate*` Functions](#entry-points-generate-functions)
3. [The `LlvmGenerator` Struct](#the-llvmgenerator-struct)
4. [KNIR Pipeline: Typed Kain AST → LLVM IR Text](#knir-pipeline-typed-kain-ast--llvm-ir-text)
5. [Attributes & Calling Conventions](#attributes--calling-conventions)
6. [Type System Mapping: Kain → LLVM](#type-system-mapping-kain--llvm)
7. [Function & Item Compilation](#function--item-compilation)
8. [Component / Surface / JSX Codegen](#component--surface--jsx-codegen)
9. [Ownership Model: Collapse / Observe / Decay](#ownership-model-collapse--observe--decay)
10. [Tagged Values (Option / Result)](#tagged-values-option--result)
11. [Semantic Constructs: world, entangle, patch, law, resonate, pulse, axiom, shatter, teleport, converge, orchestrate](#semantic-constructs)
12. [Actor Codegen](#actor-codegen)
13. [Chunk 00: Infrastructure (`chunk-00-infra.tsv`)](#chunk-00-infra)
14. [Chunk 02: Ownership (`chunk-02-ownership.tsv`)](#chunk-02-ownership)
15. [Chunk 03: Analysis (`chunk-03-analysis.tsv`)](#chunk-03-analysis)
16. [Chunk 04: Values (`chunk-04-values.tsv`)](#chunk-04-values)
17. [Chunk 05: ABI (`chunk-05-abi.tsv`)](#chunk-05-abi)
18. [Chunk 06: Functions (`chunk-06-functions.tsv`)](#chunk-06-functions)
19. [Chunk 07: Semantics (`chunk-07-semantics.tsv`)](#chunk-07-semantics)
20. [Chunk 08: Statements (`chunk-08-stmts.tsv`)](#chunk-08-statements)
21. [Chunk 09: Expressions (`chunk-09-exprs.tsv`)](#chunk-09-expressions)
22. [Chunk 10: Component Calls (`chunk-10-component_calls.tsv`)](#chunk-10-component-calls)
23. [TSV Column Legend](#tsv-column-legend)

---

## Architecture Overview

### The Big Picture

The `codegen_llvm/` crate is the **LLVM backend** for the Kain compiler. It transforms a fully typechecked `TypedProgram` (the output of `kain_core`'s typechecker) into **textual LLVM IR** — human-readable `.ll` files that can be compiled by `clang` or `llc`.

**Why textual IR?** The design choice explicitly avoids linking against LLVM's C++ library (`libLLVM`) at build time. Instead, the generator produces plain-text LLVM IR. This maximizes portability and reliability: any LLVM toolchain can consume the output without the compiler needing to link a specific LLVM version.

### Two Source Files

| File | Size | Purpose |
|------|------|---------|
| `mod.rs` | 904 KB (~21,886 lines) | The main `LlvmGenerator` struct and all codegen functions: type mapping, expression compilation, statement compilation, function/actor/component/shader emission, ownership lowering, tagged values, ABI lowering, atomics, inline asm, world/entangle/patch/law/resonate/pulse/axiom/shatter/teleport/converge/orchestrate codegen, string operations, const globals, Python imports, JSON interop, debug metadata, crash tables |
| `component.rs` | 84 KB (~2,002 lines) | Component surface codegen: `KainComponentSurface` vtable, JSX→vtable-call lowering, element tree emission, attribute mapping, state persistence (get/set i64/f64/string), frame lifecycle loop, GPU surface extension, pulse/resonate registration |

### 10 TSV Chunk Files

These are **machine-readable function catalogs** that document every function in `mod.rs` and `component.rs`:

| Chunk | File | Scope |
|-------|------|-------|
| 00 | `chunk-00-infra.tsv` | Infrastructure: `LlvmGenerator::new`, `emit`, type mapping, attribute resolution, symbol helpers, registration, JSON/Any tracking — ~134 rows |
| 02 | `chunk-02-ownership.tsv` | Ownership system: collapse/observe/decay analysis, ephemeral optimization, forwarded mem slots, RC management, allocation analysis — ~104 rows |
| 03 | `chunk-03-analysis.tsv` | Analysis passes: ephemeral elision, fixed array candidates, stack shatter candidates, literal map candidates, string analysis, substring detection, manual find_substring pattern matching — ~93 rows |
| 04 | `chunk-04-values.tsv` | Value-level codegen: string literals, tagged values, enum tags, mem load/store, volatile, atomics, pointer arithmetic, ownership calls, fanout, teleport, range bounds — ~88 rows |
| 05 | `chunk-05-abi.tsv` | ABI / cross-cutting: Option/Result variants, async/await, JSON Any encoding, Python bridge varargs, numeric casts, condition coercion, pattern matching, world field paths, entangle, resonate, shatter, patch, teleport, addressable pointers — ~50 rows |
| 06 | `chunk-06-functions.tsv` | Function/item-level compilation: lowered helper calls, JSX, actor ask/emit, signatures, type definitions, const/Python globals, compile_module, debug metadata, crash table — ~46 rows |
| 07 | `chunk-07-semantics.tsv` | Semantic construct compilation: actor turn, component, impl, shader SPIR-V, entangle registration, patch, law, resonate, axiom, pulse, converge, orchestrate, world initializer, machine stones preamble, extern function — ~29 rows |
| 08 | `chunk-08-stmts.tsv` | Statement compilation: compile_block, scope management, RC retain/release, defer, if/elif/else, compile_stmt (850-line dispatcher for let/assignment/while/for/loop/return/break/continue/defers) — ~25 rows |
| 09 | `chunk-09-exprs.tsv` | Expression compilation: numeric builtins (floor/abs/min/max/clamp), direct_call (1053-1509), stage_call, stdout, assert, macro, compile_expr (2400-line dispatcher for 50+ expression kinds), plus ~22 tests — ~39 rows |
| 10 | `chunk-10-component_calls.tsv` | Component calls: type declarations, vtable constants, attribute sets (46 entries), state access (27 entries), element tree (13), frame lifecycle (27), shader surface (5), LLVM intrinsics (4), callback bind (6), component call (7), expr eval (9), flow control (21), stable keys (8), vtable call (6), pulse/resonate (10), setup (7) — ~229 rows |

---

## Entry Points: `generate*` Functions

All in `mod.rs`, the public API surface for LLVM codegen:

| Function | Lines | What it does |
|----------|-------|-------------|
| `generate(program)` | 457-462 | Shortcut: `generate_with_target(program, &host_descriptor)` |
| `generate_with_target(program, target)` | 461-469 | Shortcut: `generate_with_options(program, false, None, "", target)` |
| `generate_llvm_for_target(program, triple)` | 472-484 | Resolves optional target triple string → `generate_with_target` |
| `generate_with_debug(program, source, filename)` | 489-500 | Generates with DWARF debug info |
| `generate_with_debug_for_target(program, source, filename, target_triple)` | 504-518 | Debug + cross-compilation target |
| `generate_with_options(program, debug, source, filename, target)` | 517-544 | **Internal orchestrator**: lowers `TypedProgram` → validates memory support → creates `LlvmGenerator::new()` → calls `gen.compile_module()` → returns `Vec<u8>` |

### The Internal Pipeline (`generate_with_options`)

```
TypedProgram
    │
    ▼
lower_typed_program_memory_for_target(program, target)
    │  (from kain_core)
    ▼
validate_typed_program_memory_support(&lowered)
    │
    ▼
LlvmGenerator::new(target, debug, source, filename)
    │  Allocates all state: locals, functions, strings,
    │  entanglements, pulses, axioms, shatters, worlds, etc.
    ▼
gen.compile_module(&lowered)
    │  The 1000-line main compilation method:
    │  1. Emit module header (target triple, data layout)
    │  2. Emit type declarations (KainActorRef, KainReplyPort, etc.)
    │  3. Collect pointer type hints from entire program
    │  4. Register all struct/enum type definitions
    │  5. Collect native entanglements/resonances/machine stones
    │  6. Pre-scan function signatures
    │  7. Emit extern declarations (C runtime FFI)
    │  8. Emit stdlib externs
    │  9. Register world types and globals
    │  10. Register const globals + Python import globals
    │  11. Compile each TypedItem (functions, actors, components, etc.)
    │  12. Emit struct destructors
    │  13. Emit machine stones entry preamble
    │  14. Emit string globals
    │  15. Emit debug metadata footer
    │  16. Emit crash table
    ▼
LLVM IR text (Vec<u8>)
```

---

## The `LlvmGenerator` Struct

The central state machine for all codegen. Defined in `mod.rs` at line ~860.

### Key Fields

```rust
struct LlvmGenerator {
    // Output
    output: String,                          // Accumulated LLVM IR text
    reg_count: u64,                          // Monotonic SSA register counter (%r0, %r1, ...)
    label_count: u64,                        // Monotonic basic block label counter (L0, L1, ...)

    // Target
    target: LlvmTargetDescriptor,            // Host or cross-compilation target
    debug: bool,                             // DWARF debug info enabled
    source_filename: String,                 // Original .kn source filename

    // Module-level state
    functions: HashMap<String, TypedFunction>,  // All functions by emitted name
    function_callconv: HashMap<String, String>, // Per-function calling convention
    struct_defs: HashMap<String, Vec<(String, String)>>, // Struct name → field (name, llvm_ty)
    value_aggregate: HashSet<String>,         // POD structs (by-value in LLVM)
    unit_only_enums: HashSet<String>,          // Enums with only unit variants (no payload)
    emitted_extern_symbols: HashSet<String>,   // Already-emitted `declare @symbol`
    const_globals: HashMap<String, ConstGlobalInfo>,
    python_import_globals: HashMap<String, PythonImportGlobalInfo>,

    // World state
    world_globals: HashMap<String, WorldGlobalInfo>,
    world_message_handlers: HashMap<String, Vec<String>>,

    // Semantic constructs
    native_entanglements: Vec<NativeEntangleBinding>,
    native_resonances: Vec<NativeResonanceInfo>,
    machine_axioms: Vec<NativeMachineAxiomInfo>,
    machine_pulses: Vec<NativePulseInfo>,
    shattered_structs: HashSet<String>,

    // Per-function state
    current_function: Option<String>,
    current_block: Option<String>,
    entry_preamble_insert_offset: usize,     // Where to insert entry allocas

    // Locals & scopes
    locals: HashMap<String, String>,          // local name → LLVM reg
    ssa_locals: HashMap<String, String>,      // SSA-mapped locals
    local_types: HashMap<String, String>,     // local name → LLVM type
    tagged_value_locals: HashMap<String, String>,
    runtime_array_locals: HashMap<String, String>,
    helper_owned_locals: HashSet<String>,
    borrowed_locals: HashSet<String>,
    scopes: Vec<Scope>,                       // Scope stack for defer/cleanup

    // Analysis & optimization state
    known_i64_literals: HashMap<String, i64>,
    known_nonnegative_i64s: HashSet<String>,
    known_llvm_types: HashMap<String, String>,
    string_length_values: HashMap<String, String>,
    forwarded_mem_slots: HashMap<String, ForwardedMemSlot>,
    ephemeral_candidates: HashSet<String>,
    ephemeral_zero_init_candidates: HashSet<String>,
    fixed_array_candidates: HashSet<String>,
    stack_shatter_candidates: HashSet<String>,
    literal_map_candidates: HashSet<String>,
    shattered_array_locals: HashMap<String, ShatteredArrayLocal>,
    fixed_array_locals: HashMap<String, FixedArrayLocal>,

    // JSON / Any / Python tracking
    json_handle_locals: HashSet<String>,
    json_passthrough_locals: HashSet<String>,
    json_owning_function_returns: HashSet<String>,
    json_carrying_function_returns: HashSet<String>,
    runtime_any_passthrough_locals: HashSet<String>,
    runtime_any_function_returns: HashSet<String>,

    // Ownership
    original_pointer_let_types: HashMap<Span, Type>,
    authored_struct_pointer_locals: HashMap<String, String>,
    known_tagged_i64s: HashSet<String>,       // i64 regs carrying tagged immediates

    // Component codegen (set before compiling each component)
    current_component_name: Option<String>,
    current_component_methods: HashMap<String, TypedFunction>,
    current_component_session: Option<String>,
    current_component_parent: Option<String>,

    // Pulse/resonate registration sentinel tracking
    component_pulse_resonate_init_state: HashSet<String>,

    // Orchestrate trace
    orchestrate_trace_enabled: bool,
    surface_trait_declared: bool,
}
```

### Core Infrastructure Methods

| Method | Lines | Purpose |
|--------|-------|---------|
| `new(target, debug, source, filename)` | 860-973 | Allocate all state fields |
| `save_function_state()` | 975-1031 | Snapshot mutable state → `LlvmFunctionState` |
| `restore_function_state(state)` | 1033-1087 | Restore state from snapshot |
| `reset_for_isolated_function_codegen()` | 1089-1141 | Clear per-function state |
| `emit(line)` | 1569-1593 | Append line to `self.output`; append `!dbg` if debug enabled |
| `is_llvm_instruction_line(line)` | 1597-1616 | Heuristic: indented, not label/comment/directive/declaration |
| `byte_offset_to_line_column(offset)` | 1620-1631 | Binary search on line-start table for DWARF locations |
| `next_reg()` → `String` | 1711-1715 | `%r{self.reg_count}` with increment |
| `next_label()` → `String` | 1717-1721 | `L{self.label_count}` with increment |
| `emit_label(name)` | 1679-1686 | Emit `name:` label, track `current_block`, record entry preamble offset |
| `emit_entry_alloca(reg, ty)` | 1688-1699 | Insert `alloca Ty` at entry (before non-alloca code) |
| `emit_entry_preamble_line(line)` | 1701-1709 | Insert code after entry allocas (for const initializers) |
| `find_attribute(attrs, name)` | 1757-1759 | Linear scan for named attribute |
| `attribute_string_arg(attr)` | 1761-1770 | Extract `String("...")` from `Expr::String` attribute arg |

---

## KNIR Pipeline: Typed Kain AST → LLVM IR Text

Kain uses a **textual KNIR** (Kain Native IR) approach. Instead of building LLVM IR through the LLVM-C API, the codegen constructs LLVM IR as **strings** appended to `self.output`. This means:

1. Every LLVM instruction is emitted as a formatted string
2. SSA registers are managed via `next_reg()` (`%r0`, `%r1`, ...)
3. Basic block labels via `next_label()` (`L0`, `L1`, ...)
4. Entry allocas are tracked and inserted at the correct position via `emit_entry_alloca()`

### Why Textual KNIR?

- **No LLVM library dependency** at build time — only `clang`/`llc` needed at compile time
- **Maximum portability** — any LLVM version can consume the output
- **Debuggable** — `.ll` files are human-readable
- **Version-independent** — doesn't break when LLVM C++ API changes

### SSA Register Convention

```
%r0, %r1, %r2, ...  — Virtual SSA registers (monotonic counter)
L0, L1, L2, ...     — Basic block labels (monotonic counter)
@.str.0, @.str.1, ... — Interned string globals
```

---

## Attributes & Calling Conventions

Located in `mod.rs`, lines 1757-1928. Kain functions support standard attributes that control LLVM IR emission:

### Attribute Constants

```rust
const ATTR_SECTION: &str = "section";
const ATTR_LINK_NAME: &str = "link_name";
const ATTR_C_STRING_RETURN: &str = "c_string_return";
const ATTR_CALLCONV: &str = "callconv";
const ATTR_THREAD_LOCAL: &str = "thread_local";
const ATTR_PACKED: &str = "packed";
const ATTR_NAKED: &str = "naked";
const ATTR_INTERRUPT: &str = "interrupt";
const ATTR_MMIO: &str = "mmio";
```

### Attribute Functions

| Function | Lines | What it does |
|----------|-------|-------------|
| `find_attribute(attrs, name)` | 1757-1759 | Linear search for named attribute in `Attribute[]` |
| `attribute_string_arg(attr)` | 1761-1770 | Extract `String("...")` from `Expr::String` attribute argument |
| `callable_section_name(attrs)` | 1772-1774 | Extract `@section("name")` → section name string |
| `callable_link_name(attrs)` | 1822-1824 | Extract `@link_name("symbol")` → override emitted function name |
| `callable_has_attribute(attrs, name)` | 1826-1828 | Boolean attribute check |
| `callable_is_naked(attrs)` | 1830-1834 | Check for `@naked` attribute (no prologue/epilogue) |
| `callable_is_interrupt(attrs)` | 1836-1840 | Check for `@interrupt` attribute (x86 interrupt handler) |
| `callable_llvm_calling_convention(attrs)` | 1842-1900 | Resolve `@callconv` attribute → LLVM calling convention string |
| `callable_needs_x86_64_abi_surface(attrs)` | 1902-1909 | Detect if function needs x86_64 ABI special handling |
| `callable_symbol_for_name(name)` | 1911-1916 | Resolve authored name → emitted LLVM symbol via HashMap |
| `callable_callconv_for_name(name)` | 1918-1922 | Lookup calling convention for named function |
| `callable_callconv_prefix_for_name(name)` | 1924-1928 | Format `win64cc @name(...)` or `x86_64_sysvcc @name(...)` prefix |

### Supported Calling Conventions

From `callable_llvm_calling_convention()` (lines 1842-1900):
- `"x86_intrcc"` — x86 interrupt handler (paired with `@interrupt`)
- `"win64cc"` — Windows x64 calling convention
- `"x86_64_sysvcc"` — System V AMD64 ABI
- `"x86_vectorcallcc"` — x86 vectorcall
- `"x86_stdcallcc"` — x86 stdcall

### `@section` Handling (Windows COFF TLS)

When target is Windows x64 and section starts with `.tls$`, the section name is normalized:
- `normalize_windows_tls_section_name_for_coff()` (lines 1783-1808) ensures TLS subsections sort before `.tls$ZZZ`
- `windows_tls_subsection_is_live()` (lines 1776-1781) checks byte ordering

### `@link_name` Attribute

Allows renaming the emitted LLVM symbol. Example:
```kain
@link_name("my_custom_symbol")
fn my_function(): ...
```
Emits: `define void @my_custom_symbol()`

---

## Type System Mapping: Kain → LLVM

### Core Type Mapping

| Kain Type | LLVM Type | Notes |
|-----------|-----------|-------|
| `Int` / `I64` / `i64` | `i64` | Default integer type |
| `F32` | `float` | 32-bit float |
| `Float` / `double` | `double` | 64-bit float |
| `Bool` | `i1` | Single bit |
| `String` | `i8*` | Pointer to null-terminated UTF-8 |
| `Void` | `void` | Unit return |
| `Ptr<T>` | `T*` | Pointer to T |
| `Ref<T>` | `T*` | Reference (same as pointer in LLVM) |
| `Array<T>` / `Slice<T>` | `i8*` | Runtime array handle |
| `Option<T>` / `Result<T,E>` | `i8*` | Tagged value (immediate or boxed) |
| `Unit` / `Never` | `void` | Zero-size type |
| Named struct `Foo` | `%Foo*` or `%Foo` | By-ptr if non-POD, by-val if POD (value aggregate) |
| `KainReplyPort` | `%KainReplyPort` | Actor reply port |
| `KainActorId` | `i64` | Actor identifier |
| `KainActorExitReason` | `i32` | Actor exit reason |
| `Vec2` | `%__kain_tuple_double_double` | `{ double, double }` |
| `Vec3` | `%__kain_tuple_double_3` | `{ double, double, double }` |
| `Vec4` | `%__kain_tuple_double_4` | `{ double, double, double, double }` |

### Key Type Functions

| Function | Lines | Purpose |
|----------|-------|---------|
| `map_type_from_ast(ty)` | 2329-2367 | Kain AST type → LLVM type string |
| `map_type_from_str(name)` | 2369-2420 | Kain type name string → LLVM type string |
| `map_impl_type_from_ast(ty)` | 2426-2432 | Resolve `Self_` → struct storage type |
| `ast_type_is_self_alias(ty)` | 2422-2424 | Detect `Self_` / `Self` alias |
| `ast_type_is_string(ty)` | 2441-2446 | Detect `String` / `str` |
| `ast_type_is_int(ty)` | 2524-2526 | Detect `Int` |
| `ast_type_is_json_value(ty)` | 2448-2450 | Detect `JsonValue` |
| `ast_type_is_runtime_any_value(ty)` | 2452-2454 | Detect `Any` |
| `ast_type_is_json_handle_like(ty)` | 2456-2462 | Detect `JsonValue` / `JsonObject` / `JsonArray` |
| `struct_storage_type(name)` | 2118-2125 | `%Name` (by-val) or `%Name*` (by-ptr) |
| `tuple_struct_name_from_types(types)` | 2031-2038 | Generate `%__kain_tuple_<sanitized>` |
| `tuple_struct_storage_type_from_types(types)` | 2040-2047 | By-val or by-ptr depending on POD |
| `llvm_named_type_name(name)` | 2060-2062 | Sanitize type name for LLVM |
| `register_struct_definition(name, fields)` | 2064-2070 | Register in `struct_defs` map |
| `register_value_aggregate_struct(name)` | 2072-2078 | Mark as POD (by-value) |
| `register_tuple_struct(types)` | 2138-2156 | Register + emit LLVM type declaration |
| `llvm_type_is_scalar_value_aggregate_pod(ty)` | 2093-2102 | Check if LLVM type is POD |

### Value Aggregate (POD) Structs

Structs with only scalar fields (int, float, bool) that contain no pointers or RC-managed data are marked as **value aggregates**. These travel **by-value** in LLVM IR (`%StructName` rather than `%StructName*`), enabling:
- Direct register passing
- Fewer alloca/load/store instructions
- Better optimization

---

## Function & Item Compilation

The main compilation dispatch is `compile_typed_items()` (lines 2466-2489), which matches on every `TypedItem` variant:

```
TypedItem::Function    → compile_function()
TypedItem::Extern      → compile_extern_function()
TypedItem::Impl        → compile_impl()
TypedItem::Actor       → compile_actor()
TypedItem::Component   → compile_component() → delegates to component.rs
TypedItem::Shader      → compile_shader()
TypedItem::Const       → register_const_global()
TypedItem::World       → register_world_type_and_global() + compile_world_initializer()
TypedItem::Patch       → compile_patch()
TypedItem::Law         → compile_law()
TypedItem::Resonate    → compile_resonate()
TypedItem::Pulse       → compile_pulse()
TypedItem::Axiom       → compile_axiom()
TypedItem::Converge    → compile_converge()
TypedItem::Orchestrate → compile_orchestrate()
TypedItem::Test        → compile_function()
TypedItem::Mod         → recurse
TypedItem::Import      → register_python_import_global()
```

### `compile_function()` (lines 2851-2864)
Dispatches to:
- `compile_extern_function()` if `@extern` attribute present
- `compile_named_callable()` otherwise

### `compile_extern_function()` (lines 2866-2922)
Emits `declare void @name(params)` with:
- Optional calling convention prefix (`win64cc`, `x86_64_sysvcc`, etc.)
- Optional `@link_name` override
- `@c_string_return` attribute → return type is `i8*` instead of `i64`

### `compile_named_callable()` (lines 1764-2084)
The workhorse for named functions (~320 lines):
1. Emit `define {ret_ty} @{name}({params})` with optional callconv prefix
2. Emit `entry:` label
3. Alloca for each parameter → store
4. Compile function body block
5. Emit debug metadata (if enabled)
6. Record crash table entry (if `-g` enabled)

### `compile_module()` (lines 3153-3279)
The full module compilation pipeline (~126 lines):
1. Module header: `target triple`, `target datalayout`
2. ABI type declarations: `%KainActorRef`, `%KainActorMessage`, `%KainReplyPort`, `%KainActorSpawnConfig`, `%KainCrashEntry`
3. Pointer type hint collection
4. Struct/enum type definitions (recursive)
5. Native entanglement/resonance/machine stone metadata collection
6. Function signature pre-scan
7. Extern declarations (C runtime FFI)
8. Stdlib externs
9. World type/global registration
10. Const/Python global registration
11. Item compilation (`compile_typed_items`)
12. Struct destructor emission
13. Machine stones entry preamble
14. String globals
15. Debug metadata footer (`!llvm.dbg.cu`, `!DICompileUnit`)
16. Crash table (`@__kain_crash_table`)

---

## Component / Surface / JSX Codegen

`component.rs` implements the **Kain Component Surface** contract — the bridge between Kain's `component` declarations and platform UI backends.

### The KainComponentSurface Vtable

The vtable is declared as an opaque struct with 24 `i8*` slots:

```llvm
%KainComponentSurface = type {
    i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*,  ; slots 0-7
    i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*,  ; slots 8-15
    i8*, i8*, i8*, i8*, i8*, i8*, i8*, i8*   ; slots 16-23
}
```

### Vtable Slot Layout

| Slot | Name | Offset Constant | Signature |
|------|------|-----------------|-----------|
| 0 | `session_create` | `OFF_SESSION_CREATE` | `i64(i8*, i8*, i64, i64)*` |
| 1 | `session_destroy` | `OFF_SESSION_DESTROY` | `void(i64)*` |
| 2 | `element_begin` | `OFF_ELEMENT_BEGIN` | `i64(i64, i64, i8*, i8*)*` |
| 3 | `element_end` | `OFF_ELEMENT_END` | `void(i64, i64)*` |
| 4 | `element_set_text` | `OFF_ELEMENT_SET_TEXT` | `void(i64, i64, i8*)*` |
| 5 | `element_set_attr_i64` | `OFF_ELEMENT_SET_ATTR_I64` | `void(i64, i64, i8*, i64)*` |
| 6 | `element_set_attr_f64` | `OFF_ELEMENT_SET_ATTR_F64` | `void(i64, i64, i8*, double)*` |
| 7 | `element_set_attr_string` | `OFF_ELEMENT_SET_ATTR_STRING` | `void(i64, i64, i8*, i8*)*` |
| 8 | `state_get_i64` | `OFF_STATE_GET_I64` | `i64(i64, i8*)*` |
| 9 | `state_set_i64` | `OFF_STATE_SET_I64` | `void(i64, i8*, i64)*` |
| 10 | `begin_frame` | `OFF_BEGIN_FRAME` | `void(i64, double)*` |
| 11 | `end_frame` | `OFF_END_FRAME` | `void(i64)*` |
| 12 | `present` | `OFF_PRESENT` | `void(i64)*` |
| 13 | `poll_event` | `OFF_POLL_EVENT` | `i64(i64)*` |
| 14 | `should_close` | `OFF_SHOULD_CLOSE` | `i64(i64)*` |
| 15 | `window_open` | `OFF_WINDOW_OPEN` | `i64(i64, i8*, i64, i64)*` |
| 16 | `host_pump` | `OFF_HOST_PUMP` | `void(i64)*` |
| 17 | `session_attach_platform` | `OFF_SESSION_ATTACH_PLATFORM` | `void(i64, i8*)*` |
| 18 | `get_gpu_extension` | `OFF_GET_GPU_EXTENSION` | `%KainGpuSurfaceExtension*(i64)*` |
| 19 | `state_get_f64` | `OFF_STATE_GET_F64` | `double(i64, i8*)*` |
| 20 | `state_set_f64` | `OFF_STATE_SET_F64` | `void(i64, i8*, double)*` |
| 21 | `state_get_string` | `OFF_STATE_GET_STRING` | `i8*(i64, i8*)*` |
| 22 | `state_set_string` | `OFF_STATE_SET_STRING` | `void(i64, i8*, i8*)*` |
| 23 | `element_set_callback` | `OFF_ELEMENT_SET_CALLBACK` | `void(i64, i64, i8*, void(i64,i64,i8*)*)*` |

### How Vtable Calls Work

Every UI operation goes through an **indirect vtable call**:

```llvm
; 1. GEP into vtable to get slot pointer
%slot_ptr = getelementptr inbounds %KainComponentSurface, %KainComponentSurface* %surf, i32 0, i32 6

; 2. Bitcast generic i8** to specific function-pointer-pointer type
%fn_ptr_ptr = bitcast i8** %slot_ptr to void (i64, i64, i8*, double)**

; 3. Load the actual function pointer
%fn_ptr = load void (i64, i64, i8*, double)*, void (i64, i64, i8*, double)** %fn_ptr_ptr

; 4. Indirect call through function pointer
call void %fn_ptr(i64 %session, i64 %el, i8* %key, double %val)
```

This is done by `emit_vtable_call()` and `emit_vtable_call_void()`. All 24 slots use uniform `i8*` in the type declaration; bitcast resolves the real function pointer type at each call site.

### JSX Attribute → Surface Key Mapping

`map_jsx_attr_to_surface_key()` maps 35 JSX attribute names to vtable slots:

**f64 attributes (13):** padding, spacing, corner_radius, radius, font_size, opacity, border/border_width, stroke_width, width, height, min, max, step

**string attributes (14):** background→fill_color, fill→fill_color, border_color, stroke→border_color, color/ink_color→ink_color, title, variant, role, align, font_family, distribution→layout.distribution, axis, placeholder, tooltip

**i64 attributes (6):** direction→layout.direction, disabled, checked, selected, tab_index, weight

**text attribute (1):** value → element_set_text directly (no style key wrapper)

**fallback (1):** Unknown attributes → string vtable slot with raw attribute name as key

### Frame Lifecycle Loop

`compile_surface_frame_loop()` emits the complete frame rendering loop:

```
1. resolve surface backend:  call @kain_component_surface_resolve(name)
2. null check:               icmp eq null → panic if no backend
3. create session:            session_create(name, width, height)
4. error check:              icmp slt session_id, 0 → panic on failure
5. attach platform:           alloca [8 x i8] + memset 0 + session_attach_platform
6. open window:               window_open(title, width, height)
7. FRAME LOOP:
   a. host_pump               (PeekMessage/TranslateMessage/DispatchMessage on Win32)
   b. __kain_frame_delta_ms()  (high-res frame timer)
   c. begin_frame(delta_ms)
   d. ComponentName_render(surf, session, parent=0)
   e. end_frame()
   f. present()
   g. should_close()          → icmp eq 0 → keep going or break
8. session_destroy()
9. ret void
```

### Component State Persistence

Component state (`let state: i64 = 0`, `let state: f64 = 0.0`, `let state: String = ""`) is persisted across frames via the vtable:

**First frame detection:**
- `i64`: sentinel `-1` (changed from `0` to avoid collision with valid zero)
- `f64`: sentinel `NaN` (`0x7FF8000000000000`, detected via `fcmp uno`)
- `String`: sentinel `null`

**State init pattern:**
```llvm
%stored = call i64 @state_get_i64_vtable(i64 %session, i8* %key)
%is_first = icmp eq i64 %stored, -1
br i1 %is_first, label %init, label %load

init:
  ; Store initial value via state_set
  call void @state_set_i64_vtable(i64 %session, i8* %key, i64 0)
  br label %load

load:
  %val = phi i64 [0, %init], [%stored, %pred]
```

**Write-back at end of render:**
```llvm
%final = load i64, i64* %state_addr
call void @state_set_i64_vtable(i64 %session, i8* %key, i64 %final)
```

### GPU Surface Extension

Slot 18 (`get_gpu_extension`) returns `KainGpuSurfaceExtension*` or NULL for GPU-capable surfaces:

```llvm
%KainGpuSurfaceExtension = type { i8*, i8* }  ; load_shader(0), set_uniform(1)
```

### Stable Keys

Element identity across frames is maintained via stable key strings of the form:
```
ComponentName:tag:parent_id:sibling_index
```

Built via calls to `str_concat` and `to_string` at runtime.

---

## Ownership Model: Collapse / Observe / Decay

Kain's ownership system (collapse/observe/decay) is lowered to LLVM IR through several layers:

### Runtime ABI Calls

| Function | C Runtime Symbol | Purpose |
|----------|-----------------|---------|
| `emit_checked_ownership_call()` | `__kain_ownership_collapse` / `__kain_ownership_observe` | State machine transition with abort-on-failure |
| `emit_lazy_import_ownership_region()` | `__kain_ownership_ensure_imported` | Lazy import guard for external pointers |
| `emit_helper_owned_local_decay_cleanup()` | `__kain_ownership_decay_helper` | RC release for helper-owned pointers |
| `compile_decay_expr()` | `__kain_ownership_decay` / `__kain_ownership_decay_helper` | Decay statement lowering |
| `compile_scoped_ownership_expr()` | `ownership_begin` / `ownership_end` | Scoped collapse/observe blocks |

### Ephemeral Optimization

The codegen includes a sophisticated ephemeral analysis that detects when a `collapse`/`observe` target is:
1. Only used within the immediate scope
2. Can have its storage elided (using SSA values directly)
3. Can skip zero-initialization (first access is a full-width store)

Functions involved (from chunk-03-analysis.tsv):
- `expr_is_full_width_initial_store_on_target` — detects `MemStore` covering entire target
- `collapse_body_begins_with_full_width_store` — first stmt is full-width store
- `remaining_statements_allow_ephemeral_zero_init_elision` — validates rest of block
- `expr_is_safe_for_ephemeral_local` — recursive safety check (~236 lines)
- `block_is_safe_for_ephemeral_local` — all stmts safe
- `collect_block_ephemeral_candidate_names` — discover candidates
- `collect_block_ephemeral_zero_init_elision_names` — zero-init elision candidates

### Forwarded Memory Slots

When a `mem_load` result is immediately stored (forwarded), the alloc+load+store sequence can be collapsed:

| Function | Purpose |
|----------|---------|
| `scalar_forward_key` | Derive forwarding key from expression |
| `forwardable_mem_pointer_key` | Derive key from pointer expression |
| `forwarded_mem_load_slot` | Check if forwarded slot exists |
| `record_forwarded_mem_store` | Record a forwarded store |
| `clear_current_forwarded_mem_slots` | Clear on scope exit |

### Type Hints for Pointer Provenance

The codegen collects "original pointer types" before Ptr<T>→Int normalization to preserve provenance information:

| Function | Lines | Purpose |
|----------|-------|---------|
| `collect_original_pointer_let_type_hints` | 1143-1149 | Top-level collector |
| `collect_pointer_param_types_from_params` | 1151-1158 | Record Span→Type for Ptr/Ref params |
| `collect_pointer_let_types_from_typed_item` | 1160-1228 | Walk all TypedItem variants |
| `collect_pointer_let_types_from_item` | 1230-1307 | Same for untyped AST items |
| `collect_pointer_let_types_from_block` | 1309-1313 | Walk block statements |
| `collect_pointer_let_types_from_stmt` | 1315-1358 | All statement types |
| `collect_pointer_let_types_from_else_branch` | 1360-1371 | If/elif/else branches |
| `collect_pointer_let_types_from_expr` | 1373-1567 | All expression types (~194 lines) |

---

## Tagged Values (Option / Result)

Kain's `Option<T>` and `Result<T, E>` use a **tagged value** representation:

### Representation

```
Null pointer (i8* null) → None / sentinel
Non-null i8*:
  Lower 3 bits = tag (mask 7)
    1 = Some integer immediate
    2 = Result Ok immediate
    3 = Result Err immediate
    4 = None (for Result/JSON)
  If tag == 0: boxed heap allocation
```

### Immediate Integer Range

```
TAGGED_IMMEDIATE_INT_MIN = -(1 << 60)    = -1152921504606846976
TAGGED_IMMEDIATE_INT_MAX = (1 << 60) - 1 =  1152921504606846975
```

Values in this range are stored **inline** (shifted left 3 bits, OR'd with tag). Larger values are boxed on the heap.

### Tagged Value Functions

| Function | Lines | Purpose |
|----------|-------|---------|
| `emit_tagged_value_handle_bits` | 1329-1336 | `ptrtoint i8* %boxed to i64` |
| `emit_tagged_immediate_tag_bits_from_handle_bits` | 1338-1345 | `and i64 %handle, 7` |
| `compile_tagged_immediate_integer_handle_from_i64` | 1347-1365 | `shl %val, 3` + `or %tag` + `inttoptr` |
| `compile_tagged_immediate_integer_payload_from_i64_bits` | 1367-1380 | `ashr %handle, 3` ± cast |
| `compile_tagged_immediate_borrowed_pointer_handle` | 1382-1403 | Borrowed pointer tagging |
| `emit_tagged_value_tag_load` | 1405-1414 | Boxed: load tag from header |
| `emit_tagged_value_payload_ptr` | 1416-1423 | GEP past 16-byte header |
| `compile_tagged_value_is_tag` | 1425-1516 | Null check + immediate/boxed tag comparison + phi merge |
| `compile_tagged_value_payload_copy` | 1518-1593 | Extract payload (ashr for immediate, GEP+load for boxed) |
| `compile_tagged_value_from_compiled_payload` | 1595-1669 | Range check → immediate or boxed allocation |
| `compile_tagged_box_from_payload` | 1671-1737 | `KAIN_alloc` + store tag + store size + memcpy payload |
| `compile_tagged_box_from_value` | 1739-1797 | Full boxing (delegates to above) |

---

## Semantic Constructs

### World

```llvm
%WorldName = type { field_types... }
@__kain_world_WorldName = global %WorldName zeroinitializer

define void @__kain_init_world_WorldName() {
  ; Lazy init guard (flag check + store 1)
  ; Field initializers
  ; Surface frame loop (if UI)
}
```

### Entangle

```llvm
; Registration
call void @abi_entangle_register(i8* %authority, i8* %mirror, i8* %policy, i8* %type_name)

; Propagation on field write
call i64 @abi_entangle_record_i64(i8* %path, i8* %authority, i64 %new)
; + GEP mirror field + store
```

### Patch

```llvm
call i64 @abi_patch_record_i64(i8* %patch_name, i8* %path, i64 %old, i64 %new)
```

Wrapped in `abi_patch_begin` / `abi_patch_commit`.

### Law

Compiled as a named callable returning `i64` (boolean). The body compiles the law predicate as an expression.

### Resonate

```llvm
define void @__kain_resonate_Name(i64 %old, i64 %new, i64 %fired) { ... }
call void @abi_resonate_register(i8* %target, i64 %dampen_ns, void()* @handler)
```

Tripwire emitted after field store via `emit_resonance_after_store`:
```llvm
%should = call i64 @abi_resonate_should_fire_i64(target, dampen, old, new)
%fire = icmp ne i64 %should, 0
br i1 %fire, label %do_fire, label %skip
do_fire:
  call void @handler(old, new)
  call void @abi_resonate_exit(...)
  br label %skip
```

### Pulse

```llvm
define void @__kain_pulse_body_Name(i64 %tick, i64 %dt, i64 %missed) { ... }
define void @__kain_pulse_fire_Name() { ... }

; Registration
%token = ptrtoint i8* %name_str to i64
call i64 @kain_machine_pulse_start(i64 %token, i64 %interval_ns, i64 %jitter_ns, void()* @fire_handler)
```

### Axiom

```llvm
define i64 @__kain_axiom_accept_Name() {
  ; Call kain_machine_axiom_accept with target, arch, capability bitmask
  %result = call i64 @kain_machine_axiom_accept(i8* %target, i8* %arch, i64 %caps)
  ret i64 %result
}
```

### Shatter

SoA (Structure of Arrays) layout for SIMD-friendly data:

```llvm
; Runtime allocation
%handle = call i8* @kain_machine_shatter_alloc(i64 %field_count, i64 %item_count)

; Lane base pointers
%lane0 = call i8* @kain_machine_shatter_lane_base(i8* %handle, i64 0)
%lane1 = call i8* @kain_machine_shatter_lane_base(i8* %handle, i64 1)

; Stack shatter (fixed-size, stack-allocated)
%lane0 = alloca [element_count x i64], align 8
%lane1 = alloca [element_count x i64], align 8
```

### Teleport

```llvm
; Pointer teleport (zero-copy cross-world handoff)
%result = call i8* @kain_machine_teleport_ptr(i8* %ptr, i8* %src, i8* %dst, i8* %chan)

; Scalar teleport (metadata handoff)
call void @kain_machine_teleport_note(i8* %src, i8* %dst, i8* %chan)
```

### Converge

Multi-lane dispatch with spec + platform-gated fast lanes:

```llvm
; Each lane is a separately compiled callable
define i64 @converge_spec(params) { ... }
define i64 @converge_lane_0_sse2(params) { ... }
define i64 @converge_lane_1_avx2(params) { ... }

; Dispatch function
define i64 @converge_dispatch(params) {
  ; Static check: if compile_target == "native" and capability known, direct call
  ; Runtime check: call abi_converge_select_lane_for_key → switch on lane index
  %lane = call i64 @abi_converge_select_lane_for_key(i8* %key, i64 %shape)
  switch i64 %lane, label %spec [
    i64 0, label %lane_0
    i64 1, label %lane_1
  ]
}
```

### Orchestrate

Multi-stage execution graph:

```llvm
define void @orchestrate_stage(params) {
  ; Compile as named callable with orchestrate barrier metadata
  ; Emit JSON metadata blob for runtime stage graph
}
```

---

## Actor Codegen

### Actor Turn Function

```llvm
define i32 @ActorName_turn(i8* %actor_ptr, i64 %message_tag, i8* %message_ptr) {
entry:
  ; Load actor state from actor_ptr
  ; Switch on message_tag
  switch i64 %message_tag, label %unknown [
    i64 12345678, label %handle_msg1    ; DJBA2 hash of message name
    i64 87654321, label %handle_msg2
  ]
handle_msg1:
  ; Load message fields from message_ptr
  ; Compile handler body
  ret i32 0  ; KAIN_ACTOR_CONTINUE
handle_msg2:
  ; ...
unknown:
  ret i32 1  ; KAIN_ACTOR_UNHANDLED
}
```

### Actor Ask (Request-Reply)

```llvm
; Build KainActorRef + reply port
; call kain_actor_ask_send_ref
; Register reply handler
; On timeout: send error to reply port
```

### Message Tag Hashing

Message names are hashed with DJBA2 at compile time:
```rust
fn hash_message_tag(name: &str) -> u64 { /* DJBA2 hash */ }
fn hash_emit_message_tag(name: &str) -> u64 { /* Same for __emit__ events */ }
```

---

## Inline Assembly

Kain supports inline assembly via the LLVM `call void asm` mechanism:

```llvm
; CPU fence
call void asm sideeffect "mfence", "~{memory}"()

; Cache flush
call void asm sideeffect "clflush ($0)", "~{memory}"(i64 %addr)

; General inline asm
call void asm sideeffect inteldialect "mov eax, $0", "r"(i64 %val)
```

Functions:
- `emit_inline_asm_call()` (lines 1106-1189) — compiles asm template + constraints + operands
- `compile_inline_asm()` (lines 1191-1206) — full asm expression compilation
- `compile_cpu_fence()` (lines 1208-1224) — `mfence`/`lfence`/`sfence` variants
- `compile_cpu_cache_flush()` (lines 1226-1253) — `clflush` with address operand
- `escape_llvm_inline_asm_fragment()` (lines 1094-1104) — escape special chars for LLVM IR

---

## String Handling

### String Literals

String literals are emitted as LLVM global constants:

```llvm
@.str.0 = private unnamed_addr constant [13 x i8] c"hello world\00", align 1
```

Then referenced via GEP:
```llvm
%ptr = getelementptr inbounds [13 x i8], [13 x i8]* @.str.0, i64 0, i64 0
%str = call i8* @string_new(i8* %ptr)
```

### String Concatenation

Known-length concatenation uses fixed-arity runtime helpers:
```llvm
%result = call i8* @str_concat3(i8* %a, i8* %b, i8* %c)
```

For 2 args: `@str_concat`; for 3-10 args: `@str_concat3` through `@str_concat10`; for more: pairwise concatenation.

### String Fast Paths

The codegen includes substantial inline string optimizations:

- `compile_char_at_string_equality_fast_path` — inline byte comparison for char_at() → string equality (~117 lines)
- `compile_byte_at_fast_path` — inline bounds check + GEP + load (~72 lines)
- `compile_known_length_find_substring_inline` — ~294 lines of inline substring search with `memchr`/`memcmp` calls
- `compile_known_length_find_substring_inline_static_two_byte_needle` — optimized 2-byte search using packed i16 compare (~156 lines)
- `detect_manual_find_substring_function` — detects handwritten find_substring patterns and replaces with fast path (~52 lines)

### String RC Management

String values use reference counting:
- `string_literal_release_after_use()` — determines if `rc_release` needed after use
- `compile_string_literal_value()` — tracks release flag
- `compile_static_c_string_literal()` — bare C string pointer (no RC, for extern FFI)

---

## Atomic Operations

Complete lowering of Kain's atomic intrinsics to LLVM atomic instructions:

| Function | LLVM IR Pattern |
|----------|----------------|
| `compile_ordered_atomic_load` | `load atomic i64, i64* %ptr monotonic/acquire/seq_cst, align 8` |
| `compile_ordered_atomic_store` | `store atomic i64 %val, i64* %ptr monotonic/release/seq_cst, align 8` |
| `compile_ordered_atomic_rmw` | `atomicrmw %op i64* %ptr, i64 %val ordering` |
| `compile_ordered_atomic_compare_exchange` | `cmpxchg i64* %ptr, i64 %expected, i64 %desired succ fail` |
| `compile_ordered_atomic_fence` | `fence acquire/release/acq_rel/seq_cst` |

Ordering names: `relaxed(0)`, `acquire(1)`, `release(2)`, `acq_rel(3)`, `seq_cst(4)`

Validation:
- Atomic stores: only relaxed/release/seq_cst
- CAS failure ordering: must not be release/acq_rel, must be ≤ success ordering strength

---

## Crash Forensics & Debug Info

### DWARF Debug Metadata

When `generate_with_debug` is used:
```llvm
!llvm.dbg.cu = !{!0}
!0 = !DICompileUnit(language: DW_LANG_C, file: !1, ...)
!1 = !DIFile(filename: "source.kn", directory: "...")
!2 = !DISubprogram(name: "my_fn", file: !1, line: 42, ...)

; Per-instruction:
;   %r5 = add i64 %r3, %r4, !dbg !3
; !3 = !DILocation(line: 44, column: 5, scope: !2)
```

### Crash Table

When `-g` is enabled, a crash table is emitted:
```llvm
@__kain_crash_table = global [N x %KainCrashEntry] [
  %KainCrashEntry { i8* blockaddress(@fn, %label), i8* getelementptr(...), i64 line, i64 col },
  ...
]
```

The C runtime (`crash_handler.c`) binary-searches this table to map faulting instruction pointers back to `(fn_name, file, line:col)` for human-readable crash reports.

---

## Python Bridge

### Import Globals

```llvm
@__kain_py_import_numpy = global i64 0        ; handle (0 = not yet loaded)
@__kain_init_py_import_numpy_flag = global i1 false

define void @__kain_init_py_import_numpy() {
  ; Guard: load init_flag, br if already done
  ; Call @py_import_with_context(i8* "numpy", i8* %source_file)
  ; Store result in @__kain_py_import_numpy
  ; Set init_flag = true
}
```

Lazy initialization: `emit_python_import_init_call_if_needed()` emits a call to the init function before first use, then subsequent uses just load the global.

### Vararg Calls

`compile_python_bridge_vararg_call()` handles 2-4 argument Python calls:
```llvm
%result = call i64 @py_call_raw_args(i64 %module, i8* %fn_name, i64 %nargs, i64 %arg0, i64 %arg1, ...)
```

---

## JSON Interop

### JSON Any Encoding

Kain has an intricate JSON value encoding for the `std::json` interop:

```llvm
; Integer → shl 3 + or with JSON_ANY_TAG_INT (1)
; Bool → zext i1→i64, shl 3 + or with JSON_ANY_TAG_BOOL (2)
; String → ptrtoint i8*→i64, or with JSON_ANY_TAG_STRING (3)
; Null → JSON_ANY_TAG_NULL (4)
; Double → call @json_box_float(double %val)
```

### JSON Handle Tracking

The codegen tracks JSON handle ownership to know when to release:
- `json_handle_locals` — locals owning JSON handles
- `json_passthrough_locals` — locals borrowing JSON handles
- `json_owning_function_returns` — functions returning owned JSON
- `json_carrying_function_returns` — functions returning borrowed JSON

---

## KNIR Expression Compilation

`compile_expr()` (lines 1738-4158) is the central ~2400-line expression dispatcher covering 50+ expression kinds:

| Expression | LLVM Pattern |
|------------|-------------|
| `Expr::Int(n)` | `i64 n` |
| `Expr::Float(f)` | `double f` |
| `Expr::Bool(b)` | `i1 b` |
| `Expr::String(s)` | `call i8* @string_new(i8* @.str.N)` |
| `Expr::Ident(name)` | Load from alloca or use SSA value |
| `Expr::Binary { op, lhs, rhs }` | `add`, `sub`, `mul`, `sdiv`, `and`, `or`, `xor`, `shl`, `ashr`, `icmp`, `fcmp` |
| `Expr::Unary { op, operand }` | `sub`, `xor -1` (not) |
| `Expr::Call { callee, args }` | `call @callee(args)` |
| `Expr::MethodCall { obj, method, args }` | Inlined method or `call @method(obj, args)` |
| `Expr::Field { expr, field }` | `getelementptr` + `load` |
| `Expr::Index { target, index }` | `getelementptr` with scaled index |
| `Expr::Assign { lhs, rhs }` | Store RHS to LHS alloca |
| `Expr::Struct { name, fields }` | Alloca + GEP each field + store |
| `Expr::Match { scrutinee, arms }` | `switch` + PHI merge |
| `Expr::If { cond, then, else }` | `br i1`, PHI merge |
| `Expr::Block(s)` | `compile_block_with_result` |
| `Expr::Lambda { params, body }` | Nested function definition |
| `Expr::Ref { expr }` | Alloca + store + pointer |
| `Expr::Cast { expr, ty }` | `sext`/`zext`/`trunc`/`fptosi`/`sitofp`/`ptrtoint`/`inttoptr`/`bitcast` |
| `Expr::Bitcast { expr, ty }` | Raw `bitcast` |
| `Expr::PtrOffset { base, offset, stride }` | `getelementptr i8` with scaled offset |
| `Expr::MemLoad { ptr, ty }` | `load ty, ty* %ptr` |
| `Expr::MemStore { ptr, val }` | `store ty %val, ty* %ptr` |
| `Expr::VolatileLoad { ptr }` | `load volatile ty, ty* %ptr` |
| `Expr::VolatileStore { ptr, val }` | `store volatile ty %val, ty* %ptr` |
| `Expr::AtomicLoad { ptr, order }` | `load atomic i64, i64* %ptr ordering` |
| `Expr::AtomicStore { ptr, val, order }` | `store atomic i64 %val, i64* %ptr ordering` |
| `Expr::AtomicRmw { ptr, op, val, order }` | `atomicrmw %op i64* %ptr, i64 %val ordering` |
| `Expr::AtomicCas { ptr, expected, desired, succ, fail }` | `cmpxchg i64* %ptr, i64 %exp, i64 %des succ fail` |
| `Expr::AtomicFence { order }` | `fence ordering` |
| `Expr::InlineAsm { template, constraints, operands, options }` | `call void asm sideeffect [inteldialect] "template", "constraints"(operands)` |
| `Expr::Alloc { count, stride, zeroed }` | `call i8* @KAIN_alloc(i64 size)` |
| `Expr::Collapse { ptr, body }` | `call @__kain_ownership_collapse(i8*)` + body |
| `Expr::Observe { ptr, body }` | `call @__kain_ownership_observe(i8*)` + body |
| `Expr::Decay { ptr }` | `call @__kain_ownership_decay(i8*)` |
| `Expr::Share { expr }` | Fanout via `__kain_fanout_i64` |
| `Expr::Teleport { ptr, src, dst, chan }` | `call @kain_machine_teleport_ptr` |
| `Expr::Spawn { actor, args }` | Actor spawn via `kain_actor_spawn` |
| `Expr::SendMsg { actor, msg }` | Actor send via `kain_actor_send` |
| `Expr::Ask { actor, msg, timeout }` | Actor ask via `kain_actor_ask_send_ref` |
| `Expr::WorldField { world, field }` | `getelementptr @__kain_world_*, i32 0, i32 N` |
| `Expr::As { expr, ty }` | `bitcast` / `ptrtoint` / `inttoptr` |
| `Expr::FString { parts }` | String concat of parts |
| `Expr::Array(items)` | Runtime array via `__kain_runtime_array_new` |
| `Expr::Tuple(items)` | Alloca + GEP each field + store |
| `Expr::Range { start, end, inclusive }` | Lowered to loop bounds |
| `Expr::Await(future)` | `call @abi_future_await_payload_copy` |
| `Expr::Try(expr)` | Tag check + branch + phi |
| `Expr::Return(val)` | `ret ty %val` |
| `Expr::Break` | `br label %loop_exit` |
| `Expr::Continue` | `br label %loop_header` |

---

## KNIR Statement Compilation

`compile_stmt()` (lines 1414-2213) dispatches on ~850 lines:

| Statement | Compilation |
|-----------|------------|
| `Stmt::Let { name, ty, init }` | Alloca + compile init + store; or SSA assignment for immutable scalars |
| `Stmt::Expr(e)` | `compile_expr(e)` |
| `Stmt::Assign { lhs, rhs }` | Compile address of lhs, compile rhs, store |
| `Stmt::While { cond, body }` | Loop header with `br i1`, body, back-edge |
| `Stmt::For { var, iter, body }` | Range-based or iterator-based loop |
| `Stmt::Loop { body }` | Unconditional loop with break/continue |
| `Stmt::Return(val)` | `compile_expr(val)` + `ret ty %val` |
| `Stmt::Break` | `br label %loop_exit` |
| `Stmt::Continue` | `br label %loop_header` |
| `Stmt::Defer { exprs }` | Push to defer stack for scope exit |
| `Stmt::Dispatch { expr, arms }` | `switch` on tag with PHI merge |
| `Stmt::Fanout { start, end, body }` | `__kain_fanout_i64` with worker function |
| `Stmt::Subgroup { ... }` | GPU subgroup operation |
| `Stmt::Item(item)` | Recursively compile item |

### Scope Management

Scopes track local variables for automatic cleanup:
```llvm
; Scope entry
; ... body ...
; Scope exit:
;   rc_release for each string
;   dtor_* for each owned struct
;   json_release for each JSON handle
;   decay for each helper-owned pointer
```

---

## TSV Column Legend

All TSV files use the same 7-column format:

| Column | Header | Description |
|--------|--------|-------------|
| 1 | `function_name` | The Rust function name in the codegen |
| 2 | `kain_concept` | High-level Kain semantic concept |
| 3 | `llvm_ir_pattern` | The LLVM IR pattern emitted (in `[...]` brackets) |
| 4 | `llvm_c_api` | Equivalent LLVM-C API calls (for reference) |
| 5 | `status` | `mapped`, `needs-review`, or implementation status |
| 6 | `lines` | Line numbers in the source file |
| 7 | (implicit) | First column is an 11-character hash |

---

## Chunk 00: Infrastructure (`chunk-00-infra.tsv`)

Source: `mod.rs`, preamble + infrastructure + type mapping  
Rows: ~134

| # | function_name | kain_concept | llvm_ir_pattern | llvm_c_api | status | lines |
|---|--------------|-------------|-----------------|------------|--------|-------|
| 1 | `runtime_symbol_for_stdlib_function` | stdlib/runtime FFI | `@kain_floor_i64`, `@kain_ceil_i64` etc. — stdlib name→C symbol | map Kain fn name to runtime C symbol via match | mapped | 228-235 |
| 2 | `stdlib_function_uses_borrowed_string_param` | stdlib/runtime FFI | `[string param ABI: i8* instead of refcount]` | Which stdlib parameters borrow strings (map_get key, trim, replace) | mapped | 237-245 |
| 3 | `kain_map_codegen_mix_u64` | hash/utility | none — pure helper | 64-bit hash mix (xorshift+multiply) | mapped | 244-254 |
| 4 | `kain_map_codegen_hash_bytes` | hash/utility | none — pure helper | Hash bytes with 64-bit FNV-like rotation hash | mapped | 253-281 |
| 5 | `kain_map_codegen_magic_prefix_state` | hash/utility | none — pure helper | 4-word folded prefix state for static key hash | mapped | 280-301 |
| 6 | `kain_map_codegen_static_key_metadata` | hash/utility | none — pure helper | Compute key_length, key_hash, key_prefix for compile-time map keys | mapped | 300-332 |
| 7 | `llvm_runtime_declaration_is_preemitted` | runtime/FFI pre-emit | `declare @abi_entangle_register`, `@abi_converge_select_lane_for_key`, `@py_call*` — pre-emitted runtime C functions | Match set of pre-emitted C ABI symbols (entangle, converge, Python bridge) | mapped | 331-379 |
| 8 | `python_import_binding_infos` | Python import | `[%PythonImportGlobalInfo globals — init flags + runtime init calls]` | Extract binding infos from Import AST (Module/Member init kinds) | mapped | 378-419 |
| 9 | `llvm_orchestrate_trace_enabled` | orchestrate/env | `[none — env var gate for orchestrate stage telemetry emission]` | Check KAIN_LLVM_ORCHESTRATE_TRACE env var | mapped | 418-444 |
| 10 | `resolve_host_llvm_target_descriptor` | target/LLVM | `[none — uses kain_target crate]` | Returns LlvmTargetDescriptor::host() | mapped | 443-448 |
| 11 | `resolve_llvm_target_for_compile_target` | target/LLVM | `[none — selects triple]` | Maps CompileTarget::BareMetal→"x86_64-unknown-none", host for rest | mapped | 447-458 |
| 12 | `generate` | FULL PROGRAM codegen | `[Entire .ll module: target triple, data layout, functions, globals, metadata]` | LLVMContextCreate → LLVMModuleCreateWithNameInContext → ... (via KNIR text) | mapped | 457-462 |
| 13 | `generate_with_target` | FULL PROGRAM codegen | `[Same as generate, with explicit target descriptor]` | Generate with specific LlvmTargetDescriptor | mapped | 461-469 |
| 14 | `generate_llvm_for_target` | FULL PROGRAM codegen | `[Same as generate, triple selected]` | Generate with optional target triple string | mapped | 472-484 |
| 15 | `generate_with_debug` | FULL PROGRAM + DWARF | `[Same + !dbg, !DICompileUnit, !DIFile, !DISubprogram metadata]` | Generate with DWARF debug info + source filename | mapped | 489-500 |
| 16 | `generate_with_debug_for_target` | FULL PROGRAM + DWARF + target | `[Same as generate_with_debug + target triple]` | Generate with debug + target triple override | mapped | 504-518 |
| 17 | `generate_with_options` | FULL PROGRAM codegen (internal) | `[Orchestrates entire .ll emission: lower → validate → compile]` | lowers TypedProgram → validates memory → runs LlvmGenerator::compile_module | mapped | 517-544 |
| 18 | `codegen_error` | diagnostic | `[none — returns KainError]` | Creates codegen error with semantic enrichment | mapped | 840-855 |
| 19 | `LlvmGenerator::new` | codegen infrastructure | `[none — constructor]` | Allocates all state fields (locals, functions, strings, entanglements, pulses, axioms, shatters, worlds, etc.) | mapped | 860-973 |
| 20 | `save_function_state` | codegen infrastructure | `[none — state snapshot]` | Saves all mutable codegen state into LlvmFunctionState struct | mapped | 975-1031 |
| 21 | `restore_function_state` | codegen infrastructure | `[none — state restore]` | Restores all codegen state from LlvmFunctionState into self | mapped | 1033-1087 |
| 22 | `reset_for_isolated_function_codegen` | codegen infrastructure | `[none — state reset]` | Resets per-function codegen state for isolated function compilation | mapped | 1089-1141 |
| 23 | `collect_original_pointer_let_type_hints` | ptr/collapse/ownership | `[none — populates original_pointer_let_types map from TypedProgram]` | Collect all pointer/ref let types and param types across entire program | mapped | 1143-1149 |
| 24 | `collect_pointer_param_types_from_params` | collapse/ptr param | `[none — records Span→Type for Ptr/Ref params]` | Record original pointer param types before Ptr<T>→Int normalization | mapped | 1151-1158 |
| 25 | `collect_pointer_let_types_from_typed_item` | ALL semantic types traversal | `[none — recursive AST descent into Function, Patch, Law, Converge, Orchestrate, Pulse, Resonate, Component, Shader, Actor, Struct, Impl, Test, Mod]` | Walk all TypedItem variants collecting pointer type hints | mapped | 1160-1228 |
| 26 | `collect_pointer_let_types_from_item` | ALL semantic types (pre-typed) | `[none — same walk on ast::Item]` | Same collector over untyped AST items | mapped | 1230-1307 |
| 27 | `collect_pointer_let_types_from_block` | block traversal | `[none — walks Stmts]` | Walk block statements collecting pointer let type hints | mapped | 1309-1313 |
| 28 | `collect_pointer_let_types_from_stmt` | ALL statement types | `[none — switch on Stmt enum]` | Collect pointer types from let, expr, defer, dispatch, return, break, for, fanout, while, loop, subgroup, item statements | mapped | 1315-1358 |
| 29 | `collect_pointer_let_types_from_else_branch` | if/else | `[none — recursive else branch walk]` | Collect pointers from if/elif/else branches | mapped | 1360-1371 |
| 30 | `collect_pointer_let_types_from_expr` | ALL expression types | `[none — exhaustive Expr match]` | Collect pointer type hints from all expression variants (binary, unary, call, method, field, index, assign, struct, match, if, FString, Array, Tuple, lambda, ref, cast, bitcast, PtrOffset, MemLoad/Store, Volatile, Atomic, InlineAsm, Alloc, Collapse, Observe, Share, Decay, Teleport, Spawn, SendMsg, etc.) | mapped | 1373-1567 |
| 31 | `emit` | LLVM IR text output | `[Appends to self.output String — all LLVM IR lines]` | Push string to output buffer; if debug enabled, appends !dbg metadata suffix | mapped | 1569-1593 |
| 32 | `is_llvm_instruction_line` | LLVM IR detection | `[Heuristic: indented, not label/comment/directive/declaration]` | Detect if a line is a function-body LLVM instruction | mapped | 1597-1616 |
| 33 | `byte_offset_to_line_column` | DWARF/debug | `[!DILocation(line: N, column: N, scope: !N)]` | Convert byte offset to (line, column) using pre-built line-start table | mapped | 1620-1631 |
| 34 | `stmt_span` | debug/source span | `[Extracts Span from any Stmt variant]` | Return source span from any statement enumeration | mapped | 1634-1650 |
| 35 | `remember_emitted_extern_symbols_from_output` | extern/FFI declaration tracking | `[declare @symbol(...) — parse output for already-emitted externs]` | Scan output for declare statements, record symbols in emitted_extern_symbols set | mapped | 1652-1661 |
| 36 | `llvm_declared_symbol` | extern/FFI declaration | `[declare @symbol(...) → extract @name]` | Parse declare line and extract @symbol name | mapped | 1663-1677 |
| 37 | `emit_label` | basic block / control flow | `[label:]` | Emit label, track current_block; record entry label alloca/preamble offsets | mapped | 1679-1686 |
| 38 | `emit_entry_alloca` | alloca/stack | `[%rN = alloca Ty]` | Insert alloca at entry label position (before any non-alloca code) | mapped | 1688-1699 |
| 39 | `emit_entry_preamble_line` | entry code hoisting | `[LLVM instructions inserted after entry allocas]` | Insert code at entry preamble position (const initializers hoisted into entry) | mapped | 1701-1709 |
| 40 | `next_reg` | SSA register counter | `[%r0, %r1, %r2, …]` | Generate next monotonic SSA virtual register name | mapped | 1711-1715 |
| 41 | `next_label` | basic block label | `[L0, L1, L2, …]` | Generate next monotonic block label name | mapped | 1717-1721 |
| 42 | `mark_known_tagged_i64` | ABI/tagged immediate | `[ptrtoint i8* %handle to i64 → known_tagged_i64s set]` | Register i64 as carrying tagged immediate ((v<<3)|1) for untag before C ABI call | mapped | 1726-1728 |
| 43 | `is_known_tagged_i64` | ABI/tagged immediate | `[Check if reg is in known_tagged_i64s set]` | Check if i64 register carries tagged immediate value | mapped | 1731-1733 |
| 44 | `sanitize_type_fragment` | LLVM naming | `[replace non-alphanum with _ for type names in LLVM IR]` | Sanitize a string for use as LLVM type name fragment | mapped | 1735-1740 |
| 45 | `sanitize_symbol_fragment` | LLVM naming | `[replace non-alnum with _{XX} hex for LLVM symbol names]` | Sanitize a string for use as LLVM symbol name | mapped | 1742-1755 |
| 46 | `find_attribute` | attribute/annotation | `[Scan Attribute[] for matching name]` | Find an attribute by name in callable attributes array | mapped | 1757-1759 |
| 47 | `attribute_string_arg` | attribute arg extraction | `[Extract String("...") from attribute arg Expr]` | Extract string argument from attribute (e.g. section name) | mapped | 1761-1770 |
| 48 | `callable_section_name` | @section attribute | `[@llvm.compiler.used or custom section — section("name")]` | Extract section name from callable attributes | mapped | 1772-1774 |
| 49 | `windows_tls_subsection_is_live` | TLS/section (Windows COFF) | `[.tls$ — check if subsection sorts before CRT $ZZZ terminator]` | Check if TLS subsection name sorts before .tls$ZZZ | mapped | 1776-1781 |
| 50 | `normalize_windows_tls_section_name_for_coff` | TLS/section (Windows COFF) | `[.tls$KAIN$suffix — normalize TLS section name for Windows COFF]` | Normalize TLS section name to sort before .tls$ZZZ terminal | mapped | 1783-1808 |
| 51 | `const_section_name` | @section for const globals | `[section("name") — platform-aware section name resolution]` | Resolve section name for constant globals, handling Windows TLS | mapped | 1810-1820 |
| 52 | `callable_link_name` | @link_name attribute | `[@name = @link_name("symbol") — override emitted function name]` | Extract link_name attribute string for symbol renaming | mapped | 1822-1824 |
| 53 | `callable_has_attribute` | attribute check | `[Check if attribute exists]` | Check if callable has a named attribute | mapped | 1826-1828 |
| 54 | `callable_is_naked` | @naked attribute | `[@naked — no prologue/epilogue]` | Check if callable has @naked attribute | mapped | 1830-1834 |
| 55 | `callable_is_interrupt` | @interrupt attribute | `[x86_intrcc — interrupt handler]` | Check if callable has @interrupt attribute | mapped | 1836-1840 |
| 56 | `callable_llvm_calling_convention` | callconv/ABI attribute | `[x86_intrcc, win64cc, x86_64_sysvcc, x86_vectorcallcc, x86_stdcallcc]` | Resolve @callconv attribute to LLVM calling convention string | mapped | 1842-1900 |
| 57 | `callable_needs_x86_64_abi_surface` | ABI/x86_64 special | `[naked, interrupt, callconv != "c" → need ABI surface]` | Detect if callable needs x86_64 ABI surface handling | mapped | 1902-1909 |
| 58 | `callable_symbol_for_name` | symbol name resolution | `[@authored_name → @emitted_symbol or @kain_stdlib_name]` | Resolve callable authored name to emitted LLVM symbol name | mapped | 1911-1916 |
| 59 | `callable_callconv_for_name` | calling convention lookup | `[win64cc, x86_64_sysvcc prefix before define]` | Lookup calling convention for named function | mapped | 1918-1922 |
| 60 | `callable_callconv_prefix_for_name` | calling convention prefix | `[win64cc @name(...) or x86_64_sysvcc @name(...)]` | Format calling convention prefix for function define line | mapped | 1924-1928 |
| 61 | `stable_runtime_hash64` | runtime/stable hash | `[none — FNV-1a 64-bit hash used for stable identifiers]` | FNV-1a 64-bit hash for stable symbol naming | mapped | 1930-1937 |
| 62 | `llvm_i64_literal_for_u64` | LLVM constant | `[i64 literal: i64 value]` | Format u64 as i64 LLVM literal string | mapped | 1939-1941 |
| 63 | `machine_pulse_body_symbol` | pulse/machine stones | `[@__kain_pulse_body_<name> — pulse body function symbol]` | Generate LLVM symbol name for pulse body function | mapped | 1943-1945 |
| 64 | `machine_pulse_fire_symbol` | pulse/machine stones | `[@__kain_pulse_fire_<name> — pulse fire function symbol]` | Generate LLVM symbol name for pulse fire function | mapped | 1947-1949 |
| 65 | `machine_axiom_symbol` | axiom/machine stones | `[@__kain_axiom_accept_<name> — axiom acceptance symbol]` | Generate LLVM symbol name for axiom acceptance check | mapped | 1951-1956 |
| 66 | `machine_pulse_duration_ns` | pulse/machine stones | `[pulse interval in nanoseconds — s/ms/us/ns/tick]` | Convert PulseDuration (e.g. "8ms") to nanoseconds | mapped | 1958-1967 |
| 67 | `machine_axiom_capability_bit` | axiom/capability | `[bitmask: 1<<0=atomic.bitmask, 1<<1=time.hardware-timer, 1<<2=memory.shatter, 1<<3=world.teleport, 1<<8=sse2, 1<<9=avx, 1<<10=avx2, 1<<11=avx512, 1<<12-19=cuda.*, 1<<20=gpu.async_compute]` | Map capability string to bitmask position for axiom runtime checking | mapped | 1969-1994 |
| 68 | `converge_capability_is_cpu_selector` | converge/CPU capability | `[cpu.*, x86.*, sse2, avx, avx2, avx512, fma, bmi2 — CPU selector predicates]` | Detect if a converge fast-lane capability is a CPU selector | mapped | 1996-2012 |
| 69 | `converge_selector_static_eligibility` | converge/static optimization | `[target("llvm"/"native") → Some(true); cpu capability → None (runtime); other → Some(true)]` | Compute static converge lane eligibility (compile-time known vs runtime) | mapped | 2014-2029 |
| 70 | `tuple_struct_name_from_types` | tuple/struct type | `[%__kain_tuple_<sanitized_types> — LLVM type name]` | Generate LLVM type name for tuple struct from field types | mapped | 2031-2038 |
| 71 | `tuple_struct_storage_type_from_types` | tuple/struct type | `[%__kain_tuple_* (by-val) or %__kain_tuple_** (by-ptr) depending on is POD]` | Get LLVM storage type (by-value or pointer) for tuple struct | mapped | 2040-2047 |
| 72 | `builtin_named_tuple_storage_type` | Vec2/3/4 builtins | `[%__kain_tuple_double_double for Vec2, double_3 for Vec3, double_4 for Vec4]` | Map Vec2/Vec3/Vec4 LLVM storage type to double tuple | mapped | 2049-2058 |
| 73 | `llvm_named_type_name` | LLVM naming | `[sanitized name for LLVM %TypeName]` | Sanitize type name for LLVM IR type identifier | mapped | 2060-2062 |
| 74 | `register_struct_definition` | struct/type registration | `[%StructName = type { field_types } — LLVM type declaration]` | Register struct definition in struct_defs map, emit LLVM type declaration | mapped | 2064-2070 |
| 75 | `register_value_aggregate_struct` | value aggregate/POD struct | `[%StructName — by-value for POD structs]` | Mark struct as value aggregate (POD — can travel by-value in LLVM) | mapped | 2072-2078 |
| 76 | `tuple_field_alias_index` | tuple field access | `[x/y/z/w → 0/1/2/3; _0/_1 → numeric; __kain_tuple_N → N]` | Resolve tuple field name to index (x,y,z,w aliases + numeric) | mapped | 2080-2091 |
| 77 | `llvm_type_is_scalar_value_aggregate_pod` | POD type check | `[i1, i8, i32, i64, double, or %StructName in value_aggregate set]` | Check if LLVM type is scalar-value-aggregate POD | mapped | 2093-2102 |
| 78 | `resolved_type_is_scalar_value_aggregate_pod` | POD type check (semantic) | `[Int, Float, Bool, Char, Tuple (all POD), Struct (in value_aggregate)]` | Check if ResolvedType is POD from semantic type perspective | mapped | 2104-2116 |
| 79 | `struct_storage_type` | struct by-val vs by-ptr | `[%StructName (by-val) or %StructName* (by-ptr)]` | Get LLVM storage type for struct (by-value for POD, pointer otherwise) | mapped | 2118-2125 |
| 80 | `actor_message_reply_llvm_type` | actor/message reply | `[Returns actor_message_reply_types Haskell Map String]` | Lookup reply LLVM type for actor message (actor_name, message_name) | mapped | 2127-2136 |
| 81 | `register_tuple_struct` | tuple/struct definition + emit | `[%__kain_tuple_* = type { field_tys } — LLVM type, auto POD]` | Register tuple struct, emit LLVM type, auto-detect POD | mapped | 2138-2156 |
| 82 | `collect_tuple_types_from_ast` | tuple type collection | `[Recursively collect tuple types from AST Type nodes]` | Discover and register all nested tuple types from AST Type | mapped | 2158-2181 |
| 83 | `collect_tuple_types_from_resolved` | tuple type collection | `[Recursively collect tuple types from ResolvedType nodes]` | Discover and register all nested tuple types from resolved type tree | mapped | 2183-2212 |
| 84 | `collect_program_tuple_types` | full program type pass | `[Discover all tuple types in every TypedItem variant]` | Pre-pass discovering all tuple types in the program before codegen | mapped | 2214-2321 |
| 85 | `register_builtin_tuple_structs` | Vec2/3/4 builtins | `[%__kain_tuple_double_double, _double_3, _double_4]` | Register built-in Vec2/Vec3/Vec4 LLVM tuple structs | mapped | 2323-2327 |
| 86 | `map_type_from_ast` | Kain→LLVM type mapping | `[Self_/Self → struct_storage_type; Named → map_type_from_str; Ptr/Ref → *; Array/Slice/Option/Result → i8*; Unit/Never → void; default → i64]` | Map Kain AST type to LLVM type string | mapped | 2329-2367 |
| 87 | `map_type_from_str` | Kain→LLVM type mapping (by name) | `[Int/I64/i64→i64, F32→float, Float/double→double, Bool→i1, String→i8*, Void→void, P→%KainReplyPort, KainActorId→i64, KainActorExitReason→i32, Known struct→%StructName*/%StructName, default→i64]` | Map Kain type name string to LLVM type string | mapped | 2369-2420 |
| 88 | `ast_type_is_self_alias` | Self/Self_ type | `[Self_ or Self → alias for current impl target]` | Detect if type is Self_ or Self alias | mapped | 2422-2424 |
| 89 | `map_impl_type_from_ast` | impl target type | `[Self_ → struct_storage_type(target_name); else → map_type_from_ast]` | Map type inside impl block resolving Self_ to target type | mapped | 2426-2432 |
| 90 | `impl_method_has_authored_self_param` | impl/self param | `[Check first param for Self_/Self/self/_self]` | Detect if impl method has authored self parameter | mapped | 2434-2439 |
| 91 | `ast_type_is_string` | String detection | `[Type::Named { name: "String"|"str" }]` | Check if Kain type is String | mapped | 2441-2446 |
| 92 | `ast_type_is_json_value` | JsonValue detection | `[Type::Named { name: "JsonValue" }]` | Check if Kain type is JsonValue | mapped | 2448-2450 |
| 93 | `ast_type_is_runtime_any_value` | Any type detection | `[Type::Named { name: "Any" }]` | Check if Kain type is runtime Any | mapped | 2452-2454 |
| 94 | `ast_type_is_json_handle_like` | JsonValue/Object/Array detection | `[Type::Named { name: "JsonValue"|"JsonObject"|"JsonArray" }]` | Check if Kain type is JSON-handle-like | mapped | 2456-2462 |
| 95 | `json_function_return_is_aliasing` | JSON/aliasing detection | `[json_object_set* / json_array_push* — aliasing mutators]` | Check if a function name indicates JSON return aliasing | mapped | 2464-2466 |
| 96 | `runtime_array_bridge_any_marker` | runtime Array/slice bridge | `[__kain_runtime_array_bridge_any__ — marker string for Any-element arrays]` | Return marker string for runtime Array/Any bridge type | mapped | 2468-2470 |
| 97 | `scalar_runtime_array_literal_llvm_ty` | runtime Array element type | `[i1→i1, double→double, i8*→i8*, i8/i16/i32/i64→i64]` | Map scalar LLVM types to their runtime array element type | mapped | 2472-2480 |
| 98 | `ast_runtime_array_bridge_any` | Array bridge detection | `[Array/Slice/Tuple → bridge; Named "Any" or "Array<...>" → bridge]` | Check if AST Type needs runtime Array bridge for Any | mapped | 2482-2490 |
| 99 | `resolved_runtime_array_bridge_any` | Array bridge detection (resolved) | `[Array/Slice/Tuple/Unknown → bridge]` | Check if ResolvedType needs runtime Array bridge for Any | mapped | 2492-2500 |
| 100 | `ast_runtime_array_element_llvm_ty` | runtime Array element (from AST) | `[Array<T>/Slice<T> → llvm type of T; bridge any → marker]` | Get element LLVM type for runtime Array from AST type | mapped | 2502-2522 |
| 101 | `ast_type_is_int` | Int detection | `[Type::Named { name: "Int" }]` | Check if Kain type is Int | mapped | 2524-2526 |
| 102 | `resolved_type_is_string` | String detection (resolved) | `[ResolvedType::String]` | Check if ResolvedType is String | mapped | 2528-2530 |
| 103 | `clear_json_local_tracking` | JSON local state | `[Remove name from json_handle_locals and json_passthrough_locals]` | Clear JSON tracking state for a local variable | mapped | 2532-2535 |
| 104 | `clear_runtime_any_local_tracking` | Any local state | `[Remove name from runtime_any_passthrough_locals]` | Clear runtime Any tracking state for a local variable | mapped | 2537-2539 |
| 105 | `local_carries_json_value` | JSON local check | `[Check if local is in json_handle_locals or json_passthrough_locals]` | Check if a local variable carries a JSON handle value | mapped | 2541-2543 |
| 106 | `local_carries_runtime_any_value` | Any local check | `[Check runtime_any_passthrough_locals or python_import_globals]` | Check if a local carries a runtime Any / Python handle value | mapped | 2545-2548 |
| 107 | `call_carries_json_value` | JSON call return | `[json_object_new, json_array_new, json_parse, json_get, json_array_get, kain_shared_buffer/image_info, or json_carrying_function_returns set]` | Check if function call returns JSON handle value | mapped | 2550-2561 |
| 108 | `call_returns_owned_json_value` | JSON owned return | `[same names as call_carries_json_value + json_owning_function_returns set]` | Check if function call returns owned JSON handle (must be released) | mapped | 2563-2574 |
| 109 | `call_carries_runtime_any_value` | Any call return | `[py_import, py_call, py_getattr, kain_tensor_from_py*, kain_image_from_py*, kain_geometry_from_py*, python_import_globals, or runtime_any_function_returns set]` | Check if function call returns runtime Any value (Python/Interop) | mapped | 2576-2609 |
| 110 | `expr_carries_json_value` | JSON expression check | `[Ident→local_carries_json_value; Call→call_carries_json_value; Field→field or object carries; Cast/Bitcast→delegate; Paren→delegate]` | Check if expression produces a JSON handle value | mapped | 2611-2629 |
| 111 | `expr_carries_runtime_any_value` | Any expression check | `[Ident→local_carries_runtime_any; Call→call_carries_runtime_any; Field→field or object carries; Cast/Bitcast→delegate; Paren→delegate]` | Check if expression produces a runtime Any value | mapped | 2631-2649 |
| 112 | `resolved_type_carries_any_passthrough` | Any passthrough check | `[ResolvedType::Unknown → passthrough (for typed-lowered generic params)]` | Check if resolved type carries Any passthrough semantics | mapped | 2651-2653 |
| 113 | `record_struct_any_passthrough_field` | struct Any field | `[struct_any_passthrough_fields ← (struct_name, field_name) for Unknown-type fields]` | Record struct field that carries Any passthrough semantics | mapped | 2655-2665 |
| 114 | `record_struct_runtime_array_field` | struct Array field | `[struct_runtime_array_field_elements ← (struct_name, field_name, element_llvm_ty)]` | Record struct field that carries runtime Array semantics with element type | mapped | 2667-2683 |
| 115 | `field_parent_struct_name` | struct field parent | `[Walk Expr::Ident → lookup locals to find struct name from field access]` | Find parent struct name for a field access expression | mapped | 2685-2707 |
| 116 | `field_carries_any_passthrough` | struct Any field check | `[Check struct_any_passthrough_fields for (struct_name, field)]` | Check if a struct field access carries Any passthrough | mapped | 2709-2715 |
| 117 | `expr_owns_json_value` | JSON ownership check | `[Ident→json_handle_locals; Call→call_returns_owned_json_value; Paren→delegate; Cast/Bitcast→delegate]` | Check if expression owns a JSON handle (must eventually release) | mapped | 2717-2730 |
| 118 | `record_json_local_tracking_from_expr` | JSON tracking | `[owned→json_handle_locals; borrowed→json_passthrough_locals; neither→clear]` | Record JSON tracking state for a local from its initializer expression | mapped | 2732-2742 |
| 119 | `record_runtime_any_local_tracking_from_expr` | Any tracking | `[carries→runtime_any_passthrough_locals; else→clear]` | Record runtime Any tracking state for a local from init expression | mapped | 2744-2750 |
| 120 | `resolved_runtime_array_element_llvm_ty` | runtime Array element (from resolved) | `[Array<T>/Slice<T> → element LLVM type; bridge any → marker]` | Get element LLVM type for runtime Array from ResolvedType | mapped | 2752-2763 |
| 121 | `single_generic_payload` | generic introspection | `[Extract type from "Wrapper<Inner>" string pattern]` | Extract single generic type parameter from type name string | mapped | 2765-2770 |
| 122 | `stringified_runtime_array_element_llvm_ty` | runtime Array element (from string name) | `[Array<T>/Slice<T> → parse inner type; bridge any → marker]` | Get element LLVM type for runtime Array from stringified type name | mapped | 2772-2787 |
| 123 | `runtime_array_literal_item_llvm_ty` | runtime Array literal item | `[Infer element LLVM type from expression in array literal]` | Infer LLVM type of a runtime array literal item expression | mapped | 2789-2834 |
| 124 | `runtime_array_literal_element_llvm_ty` | runtime Array literal | `[Infer common element type across all items in array literal]` | Infer unified element LLVM type from all items in a runtime array literal | mapped | 2836-2849 |
| 125 | `callable_return_runtime_array_element_llvm_ty` | runtime Array function return | `[Resolve return element type from function signature or explicit return type]` | Get element LLVM type for function returning runtime Array | mapped | 2851-2864 |
| 126 | `resolved_tagged_value_success_llvm_ty` | tagged value Option/Result success | `[Option(T) → map_type(T); Result(ok,_) → map_type(ok)]` | Get success payload LLVM type from resolved Option/Result type | mapped | 2866-2872 |
| 127 | `ast_tagged_value_success_llvm_ty` | tagged value Option/Result success (AST) | `[Option<T>/Result<T,_> → map_type_from_ast(T)]` | Get success payload LLVM type from AST Option/Result type | mapped | 2874-2886 |
| 128 | `callable_return_tagged_value_success_llvm_ty` | tagged value function return | `[Resolve from explicit return type or Function{ret} ResolvedType]` | Get tagged value success LLVM type for function return | mapped | 2888-2899 |
| 129 | `tagged_value_expr_success_llvm_ty` | tagged value expr check | `[Ident→tagged_value_locals; Call Callee→tagged_value_function_returns]` | Get tagged value success payload type from expression | mapped | 2901-2910 |
| 130 | `runtime_array_expr_element_llvm_ty` | runtime Array expression type | `[Array/Tuple literal→runtime_array_literal_element; Ident→runtime_array_locals; Call→runtime_array_function_returns; Field→struct_runtime_array_field_elements]` | Get element LLVM type of runtime Array expression | mapped | 2912-2932 |

---

## Chunk 02: Ownership (`chunk-02-ownership.tsv`)

Source: `mod.rs`, ownership/metadata analysis functions  
Rows: ~104

| # | function_name | kain_concept | llvm_ir_pattern | llvm_c_api | status | lines |
|---|--------------|-------------|-----------------|------------|--------|-------|
| 1 | `runtime_symbol_for_stdlib_function` | systems (stdlib mapping) | Function name remapping (i64 call) | N/A — string mapping only | mapped | 234-241 |
| 2 | `stdlib_function_uses_borrowed_string_param` | systems (string ABI) | String parameter classification | N/A — metadata only | mapped | 243-248 |
| 3 | `kain_map_codegen_mix_u64` | systems (hash) | u64 arithmetic (xor, mul, rotate) | N/A — compile-time math | mapped | 250-257 |
| 4 | `kain_map_codegen_hash_bytes` | systems (hash) | u64 hash with arithmetic ops | N/A — compile-time math | mapped | 259-284 |
| 5 | `kain_map_codegen_magic_prefix_state` | systems (hash) | u64 fold with magic constants | N/A — compile-time math | mapped | 286-304 |
| 6 | `kain_map_codegen_static_key_metadata` | systems (hash) | u64 key metadata extraction | N/A — compile-time math | mapped | 306-335 |
| 7 | `llvm_runtime_declaration_is_preemitted` | systems (runtime FFI) | Declaration name classification | N/A — metadata only | mapped | 337-382 |
| 8 | `python_import_binding_infos` | python imports | Import metadata extraction | N/A — metadata only | mapped | 384-422 |
| 9 | `llvm_orchestrate_trace_enabled` | orchestrate | Env var trace enable check | N/A — env var flag | mapped | 424-447 |
| 10 | `resolve_host_llvm_target_descriptor` | systems (target) | N/A — target descriptor resolution | LLVMGetDefaultTargetTriple equivalent | mapped | 449-451 |
| 11 | `resolve_llvm_target_for_compile_target` | systems (target) | target triple string matching | LLVMGetDefaultTargetTriple, LLVMTargetByName | mapped | 453-461 |
| 12 | `generate` | codegen entry | Full LLVM IR module | LLVMPrintModuleToFile | mapped | 463-465 |
| 13 | `generate_with_target` | codegen entry | Full LLVM IR module | LLVMTargetMachineEmitToMemoryBuffer | mapped | 467-472 |
| 14 | `generate_llvm_for_target` | codegen entry | Full LLVM IR module | LLVMTargetMachineEmitToMemoryBuffer | mapped | 478-487 |
| 15 | `generate_with_debug` | codegen entry (debug) | + !dbg metadata, DICompileUnit, DISubprogram | LLVMSetCurrentDebugLocation, LLVMAddNamedMetadataOperand | mapped | 495-503 |
| 16 | `generate_with_debug_for_target` | codegen entry (debug+cross) | + !dbg metadata, DICompileUnit, DISubprogram | LLVMSetCurrentDebugLocation, LLVMTargetMachineEmitToMemoryBuffer | mapped | 510-521 |
| 17 | `generate_with_options` | codegen entry | Lowered program → LLVM IR text | LLVMContextCreate, LLVMModuleCreateWithNameInContext | mapped | 523-547 |
| 18 | `codegen_error` | diagnostics | Error formatting with span | KainError::rich (no LLVM-C) | mapped | 846-858 |
| 19 | `compile_runtime_array_write_value` | runtime array (Array<T>) | compile_expr for runtime array element type | LLVMBuildStore (typed element) | mapped | 862-876 |
| 20 | `record_runtime_array_local_element_ty` | runtime array (Array<T>) | Metadata tracking (no IR emitted) | N/A | mapped | 878-885 |
| 21 | `materialize_runtime_array_element_value` | runtime array (Array<T>) | trunc i64 to smaller / bitcast i64 to double / inttoptr i64 to ptr | type-specific cast instructions | mapped | 887-920 |
| 22 | `record_helper_owned_pointer_local` | collapse/observe/decay (provenance) | Metadata tracking (no IR emitted) | N/A | mapped | 922-934 |
| 23 | `ownership_pointer_provenance_for_expr` | collapse/observe/decay (provenance) | Metadata analysis (no IR emitted) | N/A | mapped | 936-976 |
| 24 | `expr_needs_rc_retain` | collapse/observe/decay (RC management) | Metadata analysis (no IR emitted) | N/A | mapped | 978-982 |
| 25 | `expr_is_raw_pointer_value` | collapse/observe/decay (pointer identity) | Metadata analysis (no IR emitted) | N/A | mapped | 984-1007 |
| 26 | `expr_needs_shared_value_retain` | collapse/observe/decay (shared RC management) | Metadata analysis (no IR emitted) | N/A | mapped | 1009-1038 |
| 27 | `obvious_ast_type_byte_width` | type utils (byte width) | Metadata analysis (no IR emitted) | N/A | mapped | 1040-1043 |
| 28 | `obvious_llvm_type_alignment` | type utils (alignment) | Metadata analysis (no IR emitted) | LLVMGetAlignment equivalent | mapped | 1045-1051 |
| 29 | `ptr_offset_stride_matches_llvm_type` | collapse (ptr_offset) | Metadata analysis (stride check) | N/A | mapped | 1053-1065 |
| 30 | `safe_memory_access_alignment` | collapse/observe (alignment safety) | align N on load/store | LLVMSetAlignment | mapped | 1067-1077 |
| 31 | `target_is_x86_64` | systems (target detection) | target triple matching | N/A | mapped | 1079-1084 |
| 32 | `target_is_windows_x64` | systems (target detection) | target triple matching | N/A | mapped | 1086-1088 |
| 33 | `target_is_bare_metal` | systems (target detection) | target triple matching | N/A | mapped | 1090-1092 |
| 34 | `escape_llvm_inline_asm_fragment` | asm (inline assembly) | String escaping for LLVM IR asm | N/A | mapped | 1094-1104 |
| 35 | `emit_inline_asm_call` | asm (inline assembly) | call void asm [sideeffect] [inteldialect] "template", "constraints"(operands) | LLVMGetInlineAsm (via template) | mapped | 1106-1189 |
| 36 | `compile_inline_asm` | asm (inline assembly) | compile_expr → coerce_to_i64 → emit_inline_asm_call | LLVMGetInlineAsm | mapped | 1191-1206 |
| 37 | `compile_cpu_fence` | systems (fence) | call void asm sideeffect "mfence/lfence/sfence", "~{memory}" | LLVMGetInlineAsm (fence) | mapped | 1208-1224 |
| 38 | `compile_cpu_cache_flush` | systems (cache flush) | call void asm sideeffect "clflush ($0)", "~{memory}"(i64) | LLVMGetInlineAsm (clflush) | mapped | 1226-1253 |
| 39 | `coerce_pointer_value_to_typed_memory_pointer` | collapse/observe (pointer coercion) | bitcast T* to U* / inttoptr i64 to U* | LLVMBuildBitCast, LLVMBuildIntToPtr | mapped | 1255-1280 |
| 40 | `compile_non_ephemeral_typed_memory_pointer` | collapse/observe (typed access) | getelementptr T, T* base, i64 offset | LLVMBuildGEP, LLVMBuildBitCast | mapped | 1282-1348 |
| 41 | `compile_ephemeral_storage_i8_pointer` | observe (ephemeral optimization) | bitcast T* to i8* / getelementptr inbounds + bitcast to i8* | LLVMBuildBitCast, LLVMBuildGEP | mapped | 1350-1448 |
| 42 | `compile_ephemeral_typed_memory_pointer` | observe (ephemeral optimization) | getelementptr T, T* base, i64 offset | LLVMBuildGEP | mapped | 1450-1531 |
| 43 | `scalar_forward_key` | observe (forwarded mem) | Metadata key derivation (no IR emitted) | N/A | mapped | 1533-1568 |
| 44 | `forwardable_mem_pointer_key` | observe (forwarded mem) | Metadata key derivation (no IR emitted) | N/A | mapped | 1570-1609 |
| 45 | `forwarded_mem_load_slot` | observe (forwarded mem load) | Expression analysis (no IR emitted) | N/A | mapped | 1611-1629 |
| 46 | `current_forwarded_mem_load_slot` | observe (forwarded mem load) | Scope lookup (no IR emitted) | N/A | mapped | 1631-1637 |
| 47 | `record_forwarded_mem_store` | observe (forwarded mem store) | Scope insertion (no IR emitted) | N/A | mapped | 1639-1659 |
| 48 | `clear_current_forwarded_mem_slots` | observe (forwarded mem) | Scope clearing (no IR emitted) | N/A | mapped | 1661-1665 |
| 49 | `expr_is_mem_load_surface` | observe (mem_load forwarding) | Expression classification (no IR emitted) | N/A | mapped | 1667-1678 |
| 50 | `stmt_preserves_forwarded_mem_slots` | observe (mem forwarding) | Statement classification (no IR emitted) | N/A | mapped | 1680-1691 |
| 51 | `scalar_ssa_local_type` | type utils (SSA) | Metadata classification (no IR emitted) | N/A | mapped | 1693-1695 |
| 52 | `aggregate_ssa_local_type` | type utils (SSA) | Metadata classification (no IR emitted) | N/A | mapped | 1697-1712 |
| 53 | `value_can_lower_as_ssa_local` | type utils (SSA) | Metadata classification (no IR emitted) | N/A | mapped | 1714-1716 |
| 54 | `stmts_require_addressable_local` | collapse/observe/decay (addressability) | Metadata analysis (no IR emitted) | N/A | mapped | 1718-1722 |
| 55 | `block_requires_addressable_local` | collapse/observe/decay (addressability) | Metadata analysis (no IR emitted) | N/A | mapped | 1724-1726 |
| 56 | `else_branch_requires_addressable_local` | collapse/observe/decay (addressability) | Metadata analysis (no IR emitted) | N/A | mapped | 1728-1740 |
| 57 | `stmt_requires_addressable_local` | collapse/observe/decay (addressability) | Metadata analysis (no IR emitted) | N/A | mapped | 1742-1769 |
| 58 | `expr_requires_addressable_local` | collapse/observe/decay (addressability) | Metadata analysis (no IR emitted) | N/A | mapped | 1771-1949 |
| 59 | `current_scope_marks_ephemeral_candidate` | observe (ephemeral optimization) | Scope lookup (no IR emitted) | N/A | mapped | 1951-1956 |
| 60 | `current_scope_elides_ephemeral_zero_init` | observe (ephemeral optimization) | Scope lookup (no IR emitted) | N/A | mapped | 1958-1963 |
| 61 | `current_known_i64_literals` | type utils (literal tracking) | Scope merge (no IR emitted) | N/A | mapped | 1965-1973 |
| 62 | `current_known_nonnegative_i64s` | type utils (literal tracking) | Scope merge (no IR emitted) | N/A | mapped | 1975-1983 |
| 63 | `current_known_llvm_types` | type utils (type tracking) | Scope merge (no IR emitted) | N/A | mapped | 1985-1994 |
| 64 | `current_scope_marks_fixed_array_candidate` | shatter (fixed array) | Scope lookup (no IR emitted) | N/A | mapped | 1996-2001 |
| 65 | `current_scope_marks_stack_shatter_candidate` | shatter (stack shatter) | Scope lookup (no IR emitted) | N/A | mapped | 2003-2008 |
| 66 | `current_scope_marks_literal_map_candidate` | world/patch (literal map) | Scope lookup (no IR emitted) | N/A | mapped | 2010-2015 |
| 67 | `active_loop_bounds_for` | systems (loop analysis) | Scope lookup (no IR emitted) | N/A | mapped | 2017-2022 |
| 68 | `helper_alloc_storage_layout_with_bindings` | collapse (alloc) | Expression analysis (no IR emitted) | N/A | mapped | 2024-2049 |
| 69 | `helper_alloc_is_single_cell` | collapse (alloc) | Expression analysis (no IR emitted) | N/A | mapped | 2051-2056 |
| 70 | `helper_alloc_scalar_llvm_ty` | collapse (alloc) | Type mapping (stride → scalar type) | N/A | mapped | 2058-2066 |
| 71 | `preferred_ephemeral_storage_element_llvm_ty` | observe (ephemeral alloc) | Type mapping (declared ptr → scalar LLVM) | N/A | mapped | 2068-2086 |
| 72 | `preferred_ephemeral_storage_element_llvm_ty_for_let` | observe (ephemeral alloc) | Type mapping (for let stmt) | N/A | mapped | 2088-2099 |
| 73 | `authored_struct_pointer_llvm_ty` | collapse (typed pointer) | Type mapping (authored ptr → %struct*) | N/A | mapped | 2101-2110 |
| 74 | `record_authored_struct_pointer_local` | collapse (typed pointer) | Metadata tracking (no IR emitted) | N/A | mapped | 2112-2119 |
| 75 | `record_authored_struct_pointer_local_for_let` | collapse (typed pointer) | Metadata tracking (for let stmt) | N/A | mapped | 2121-2132 |
| 76 | `authored_pointer_param_type` | collapse (typed pointer) | Metadata lookup (param type) | N/A | mapped | 2134-2142 |
| 77 | `helper_alloc_stack_storage_shape` | observe (ephemeral stack alloc) | alloca type shape: [N x T] or [N x i8] | LLVMBuildAlloca | mapped | 2144-2168 |
| 78 | `resolve_i64_literal` | type utils (literal folding) | Expression folding (no IR emitted) | N/A | mapped | 2170-2199 |
| 79 | `resolve_zeroed_literal` | type utils (literal folding) | Expression folding (no IR emitted) | N/A | mapped | 2201-2217 |
| 80 | `positive_power_of_two_shift` | type utils (literal folding) | Constant folding (trailing_zeros) | N/A | mapped | 2219-2221 |
| 81 | `positive_i64_literal` | type utils (literal folding) | Expression folding (no IR emitted) | N/A | mapped | 2223-2226 |
| 82 | `expr_is_proven_nonnegative_i64` | type utils (range analysis) | Range analysis (no IR emitted) | N/A | mapped | 2228-2232 |
| 83 | `expr_is_proven_nonnegative_i64_with` | type utils (range analysis) | Range analysis (no IR emitted) | N/A | mapped | 2234-2307 |
| 84 | `debug_mentions_identifier` | debug/forensics | Debug formatting check (no IR emitted) | N/A | mapped | 2309-2311 |
| 85 | `else_branch_has_loop_that_mentions_identifier` | debug/forensics | Debug analysis (no IR emitted) | N/A | mapped | 2313-2330 |
| 86 | `expr_has_loop_that_mentions_identifier` | debug/forensics | Debug analysis (no IR emitted) | N/A | mapped | 2332-2530 |
| 87 | `stmt_has_loop_that_mentions_identifier` | debug/forensics | Debug analysis (no IR emitted) | N/A | mapped | 2532-2569 |
| 88 | `block_has_loop_that_mentions_identifier` | debug/forensics | Debug analysis (no IR emitted) | N/A | mapped | 2571-2576 |
| 89 | `expr_is_exact_target_pointer` | collapse/observe (pointer identity) | Expression classification (no IR emitted) | N/A | mapped | 2578-2587 |
| 90 | `expr_is_ephemeral_target_address` | observe (ephemeral optimization) | Expression classification (no IR emitted) | N/A | mapped | 2589-2613 |
| 91 | `stmt_binds_i64_literal` | type utils (literal tracking) | Expression analysis (no IR emitted) | N/A | mapped | 2615-2628 |
| 92 | `stmt_assigned_identifier_name` | type utils (literal tracking) | Expression analysis (no IR emitted) | N/A | mapped | 2630-2638 |
| 93 | `collect_expr_assigned_identifier_names` | type utils (literal tracking) | Expression traversal (no IR emitted) | N/A | mapped | 2640-2866 |
| 94 | `collect_else_branch_assigned_identifier_names` | type utils (literal tracking) | Expression traversal (no IR emitted) | N/A | mapped | 2868-2884 |
| 95 | `collect_block_assigned_identifier_names` | type utils (literal tracking) | Expression traversal (no IR emitted) | N/A | mapped | 2886-2926 |
| 96 | `clear_loop_variant_literal_facts` | type utils (literal tracking) | Scope mutation (no IR emitted) | N/A | mapped | 2928-2944 |
| 97 | `record_stmt_i64_literal_effects` | type utils (literal tracking) | Scope mutation (no IR emitted) | N/A | mapped | 2946-2961 |
| 98 | `record_stmt_literal_map_effects` | world/patch (literal map) | Scope mutation (no IR emitted) | N/A | mapped | 2963-2995 |
| 99 | `record_stmt_nonnegative_i64_effects` | type utils (range analysis) | Scope mutation (no IR emitted) | N/A | mapped | 2997-3027 |
| 100 | `obvious_llvm_type_byte_width` | type utils (byte width) | Type classification (no IR emitted) | LLVMStoreSizeOfType equivalent | mapped | 3029-3040 |
| 101 | `expr_obvious_llvm_ty` | type utils (type inference) | Type inference (no IR emitted) | N/A | mapped | 3042-3078 |
| 102 | `stmt_binds_obvious_llvm_ty` | type utils (type inference) | Type inference (no IR emitted) | N/A | mapped | 3080-3100 |
| 103 | `expr_is_full_width_initial_store_on_target` | collapse (store optimization) | Expression analysis (header only, incomplete) | N/A | mapped | 3102-3104 |

---

## Chunk 03: Analysis (`chunk-03-analysis.tsv`)

Source: `mod.rs`, analysis passes for ephemeral elision, fixed arrays, stack shatter, literal maps, string analysis  
Rows: ~93

| # | function_name | kain_concept | llvm_ir_pattern | llvm_c_api | status | lines |
|---|--------------|-------------|-----------------|------------|--------|-------|
| 1 | `runtime_symbol_for_stdlib_function` | Stdlib (C ABI bridge) | Maps stdlib fn names to C runtime symbols (kain_floor_i64, etc.) | N/A (name mapping helper) | mapped | 234-241 |
| 2 | `stdlib_function_uses_borrowed_string_param` | Stdlib (string params) | Parameter index check for borrowed string usage in stdlib functions | N/A (parameter inspection) | mapped | 243-248 |
| 3 | `kain_map_codegen_mix_u64` | Map (hash utility) | Pure u64 mixing with wrapping_mul/shift/rotate | N/A (hash algorithm) | mapped | 250-257 |
| 4 | `kain_map_codegen_hash_bytes` | Map (hash utility) | Bytes→u64 hash with Murmur-like mixing | N/A (hash algorithm) | mapped | 259-284 |
| 5 | `kain_map_codegen_magic_prefix_state` | Map (hash utility) | 4-word→u64 folded prefix with magic constants | N/A (hash prefix state) | mapped | 286-304 |
| 6 | `kain_map_codegen_static_key_metadata` | Map (hash utility) | Extracts (key_length, key_hash, key_prefix) from static key string | N/A (metadata extraction) | mapped | 306-335 |
| 7 | `llvm_runtime_declaration_is_preemitted` | Runtime (pre-declared) | Checks if a C ABI function name is pre-emitted in LLVM IR preamble | N/A (name matching) | mapped | 337-382 |
| 8 | `python_import_binding_infos` | Python import | Extracts PythonImportInitKind from Import AST (Module/Member variants) | N/A (binding extraction) | mapped | 384-422 |
| 9 | `llvm_orchestrate_trace_enabled` | Orchestrate (trace) | Reads KAIN_LLVM_ORCHESTRATE_TRACE / KAIN_NATIVE_PROFILE env vars | N/A (env var check) | mapped | 424-447 |
| 10 | `resolve_host_llvm_target_descriptor` | LLVM target (host) | Returns LlvmTargetDescriptor::host() | LLVMGetHostTargetTriple-like | mapped | 449-451 |
| 11 | `resolve_llvm_target_for_compile_target` | LLVM target (cross-compile) | Returns descriptor for BareMetal (x86_64-unknown-none) or host | LLVMGetDefaultTargetTriple-like | mapped | 454-461 |
| 12 | `generate` | Codegen (entry) | Public entry: generate_with_target(program, &host_descriptor) | LLVMPrintModuleToFile-like | mapped | 463-465 |
| 13 | `generate_with_target` | Codegen (entry) | Public: generate_with_options(program, false, None, "", target) | LLVMTargetMachineEmit-like | mapped | 467-472 |
| 14 | `generate_llvm_for_target` | Codegen (cross-target) | Public: generates for optional target triple | LLVMCreateTargetMachine-like | mapped | 478-487 |
| 15 | `generate_with_debug` | Codegen (debug) | Public: generates with DWARF debug metadata from source | LLVMSetCurrentDebugLocation-like | mapped | 495-503 |
| 16 | `generate_with_debug_for_target` | Codegen (cross-debug) | Public: generates with debug info for a target triple | LLVMSetCurrentDebugLocation-like | mapped | 510-521 |
| 17 | `generate_with_options` | Codegen (internal) | Internal: memory lowering → validation → LlvmGenerator::new → compile_module | LLVMContextCreate/ModuleCreate-like | mapped | 523-547 |
| 18 | `codegen_error` | Codegen (diagnostics) | Creates KainError with DiagnosticReport + semantic enrichment | N/A (error construction) | mapped | 846-858 |
| 19 | `expr_is_full_width_initial_store_on_target` | Ownership (ephemeral elision) | Checks Expr::Call/__kain_mem_store or Expr::MemStore storing full width on target pointer | LLVMBuildStore (if matched) | mapped | ~862-891 |
| 20 | `stmt_is_full_width_initial_store_on_target` | Ownership (ephemeral elision) | Delegates to expr_is_full_width_initial_store_on_target via Stmt::Expr | N/A (delegation) | mapped | ~893-909 |
| 21 | `collapse_body_begins_with_full_width_store` | Ownership (ephemeral elision) | Checks if collapse body's first stmt is a full-width store on target | LLVMBuildStore (via analysis) | mapped | ~911-938 |
| 22 | `remaining_statements_allow_ephemeral_zero_init_elision` | Ownership (ephemeral elision) | Iterates remain stmts; allows elision if collapse/full-store, rejects if observe/decay/fn-ref | LLVMBuildStore/LLVMBuildLoad (via pattern) | mapped | ~940-1007 |
| 23 | `expr_is_safe_for_ephemeral_local` | Ownership (ephemeral safety) | Recursive safety check: int/float/string/bool/none/alloca/uninit safe; instructions referencing target not safe | N/A (safety analysis) | mapped | ~1009-1245 |
| 24 | `stmt_is_safe_for_ephemeral_local` | Ownership (ephemeral safety) | Recursive stmt safety: let/expr/defer/dispatch/return/for/while/loop/fanout | N/A (stmt safety) | mapped | ~1247-1286 |
| 25 | `block_is_safe_for_ephemeral_local` | Ownership (ephemeral safety) | All stmts safe wrapper for Block | N/A (block safety) | mapped | ~1288-1293 |
| 26 | `else_branch_is_safe_for_ephemeral_local` | Ownership (ephemeral safety) | Else/ElseIf branch safety for ephemeral locals | N/A (branch safety) | mapped | ~1295-1307 |
| 27 | `remaining_statements_preserve_ephemeral_contract` | Ownership (ephemeral contract) | Iterates remain stmts; accepts decay+observe/collapse if safe; rejects anything touching target after decay | N/A (contract analysis) | mapped | ~1309-1380 |
| 28 | `collect_block_ephemeral_candidate_names` | Ownership (ephemeral candidates) | Collects local names whose helper_alloc + remaining stmts preserve ephemeral contract | N/A (candidate collection) | mapped | ~1382-1416 |
| 29 | `collect_block_ephemeral_zero_init_elision_names` | Ownership (ephemeral zero-init) | Collects locals where zeroed alloc + full-width first store + remaining contract preserved allows eliding init | N/A (zero init candidate) | mapped | ~1418-1467 |
| 30 | `expr_is_fixed_i64_array_literal` | Fixed array (analysis) | Checks if expr is Array or vec![] macro with only i64/bool/cast/binary items | N/A (array literal check) | mapped | ~1469-1487 |
| 31 | `expr_is_safe_fixed_array_use` | Fixed array (analysis) | Recursive safety: len/load index safe; assign/store unsafe on target; rest safe if no target ref | N/A (use safety) | mapped | ~1489-1607 |
| 32 | `stmt_is_safe_fixed_array_use` | Fixed array (analysis) | Stmt wrapper for expr_is_safe_fixed_array_use with dispatch/return/for/while/loop handling | N/A (stmt safety) | mapped | ~1609-1644 |
| 33 | `block_is_safe_fixed_array_use` | Fixed array (analysis) | All stmts safe for fixed array usage | N/A (block safety) | mapped | ~1646-1651 |
| 34 | `collect_block_fixed_array_candidate_names` | Fixed array (analysis) | Collects names where value is fixed_i64_array_literal and all remaining uses are safe | N/A (candidate collection) | mapped | ~1653-1672 |
| 35 | `expr_is_direct_shattered_array_literal` | Shatter (stack candidate) | Checks if expr's struct name is in self.shattered_structs | N/A (shatter check) | mapped | ~1674-1676 |
| 36 | `expr_matches_closed_shatter_field_projection` | Shatter (stack candidate) | Checks Expr::Field(Index(target, idx)) or cast/paren chains targeting the shatter local | N/A (projection match) | mapped | ~1678-1702 |
| 37 | `expr_is_safe_stack_shatter_use` | Shatter (stack candidate) | Recursive safety: field-projection on target safe; len(target) safe; Index on target unsafe; assign to target unsafe | N/A (use safety) | mapped | ~1704-1827 |
| 38 | `stmt_is_safe_stack_shatter_use` | Shatter (stack candidate) | Stmt wrapper for stack shatter safety with dispatch/return/for/while/loop handling | N/A (stmt safety) | mapped | ~1829-1864 |
| 39 | `block_is_safe_stack_shatter_use` | Shatter (stack candidate) | All stmts safe for stack shatter usage | N/A (block safety) | mapped | ~1866-1871 |
| 40 | `collect_block_stack_shatter_candidate_names` | Shatter (stack candidate) | Collects names whose value is direct_shattered_array_literal and all remaining uses are stack-safe | N/A (candidate collection) | mapped | ~1873-1892 |
| 41 | `expr_is_literal_map_seed` | Literal map (analysis) | Checks if expr is map_new() with no args | N/A (seed check) | mapped | ~1894-1900 |
| 42 | `expr_matches_literal_map_set` | Literal map (analysis) | Checks if expr is map_set(target, string_key, int_value) on the map local | N/A (set match) | mapped | ~1902-1911 |
| 43 | `expr_is_safe_literal_map_use` | Literal map (analysis) | Recursive safety: map_get(target, string_key) safe; others safe if no target ref | N/A (use safety) | mapped | ~1913-2025 |
| 44 | `stmt_is_safe_literal_map_use` | Literal map (analysis) | Stmt safety: allows map_set on target + safe expr; otherwise delegates to expr safety | N/A (stmt safety) | mapped | ~2027-2065 |
| 45 | `block_is_safe_literal_map_use` | Literal map (analysis) | All stmts safe for literal map usage | N/A (block safety) | mapped | ~2067-2072 |
| 46 | `collect_block_literal_map_candidate_names` | Literal map (analysis) | Collects names seeded with map_new() and all remaining stmts use literal map pattern | N/A (candidate collection) | mapped | ~2074-2093 |
| 47 | `extract_string_literal` | String (utility) | Extracts String value from Expr::String or Expr::Paren wrapper | N/A (string extraction) | mapped | ~2095-2101 |
| 48 | `extract_static_string_literal` | String (utility) | Extracts string from Expr::String, Paren, or const_globals lookup | N/A (static extraction) | mapped | ~2103-2113 |
| 49 | `is_known_string_ident` | String (utility) | Checks if name is in string_locals or const_globals with is_known_string flag | N/A (ident check) | mapped | ~2115-2122 |
| 50 | `expr_is_known_string` | String (utility) | Recursive: String/FString true; Ident via is_known_string_ident; Binary(Add) if either side known string | N/A (expr check) | mapped | ~2124-2137 |
| 51 | `expr_static_string_bytes` | String (utility) | Gets string bytes from String literal or const_globals | N/A (bytes extraction) | mapped | ~2139-2149 |
| 52 | `collect_string_concat_terms` | String (concat) | Collects leaf terms from nested known-string Binary(Add) expressions into linear vec | N/A (term collection) | mapped | ~2151-2170 |
| 53 | `emit_fixed_arity_string_concat_call` | String (codegen) | Emits call to @str_concat{N}(...) for 3–10 args; returns result reg | LLVMBuildCall (to str_concatN) | mapped | ~2172-2189 |
| 54 | `compile_string_concat_expression` | String (codegen) | Full concat compilation: collects terms, compiles each, emits fixed-arity or pairwise concat with RC release | LLVMBuildCall (str_concatN/pairwise) | mapped | ~2191-2247 |
| 55 | `compile_string_data_pointer_for_byte_view` | String (codegen) | Gets i8* pointer to string data: static literal → @global, ident → compile_expr, or GEP | LLVMBuildGEP (for data pointer) | mapped | ~2249-2281 |
| 56 | `compile_string_length_value` | String (codegen) | Gets i64 length of string: known literal→len, ident→cache or compile, unknown→emit_len_for_pointer_value | LLVMBuildCall (to @len) | mapped | ~2283-2312 |
| 57 | `prime_string_param_length_cache` | String (codegen) | Loads string param ptr then emits len call, caches in string_length_values | LLVMBuildLoad + LLVMBuildCall | mapped | ~2314-2323 |
| 58 | `emit_len_for_pointer_value` | String (codegen) | ptrtoint i8*→i64, then call @len(i64) | LLVMBuildPtrToInt + LLVMBuildCall | mapped | ~2325-2331 |
| 59 | `compile_expr_as_i64` | Numeric (conversion) | Compiles expr, then cast_numeric_value to i64 if not already i64 | LLVMBuildIntCast/LLVMBuildFPToSI | mapped | ~2333-2339 |
| 60 | `compile_ord_builtin` | Stdlib (ord) | Emits call i64 @kain_ord(i8* value) | LLVMBuildCall (to @kain_ord) | mapped | ~2341-2356 |
| 61 | `compile_chr_builtin` | Stdlib (chr) | Emits call i8* @kain_chr(i64 value) | LLVMBuildCall (to @kain_chr) | mapped | ~2358-2363 |
| 62 | `compile_to_int_builtin` | Stdlib (to_int) | i64→identity; i32/i8/i1/double→cast; i8*→call @kain_parse_i64_string; JSON/Any→call @to_int | LLVMBuildCall (to @to_int/kain_parse_i64_string) | mapped | ~2365-2398 |
| 63 | `compile_to_float_builtin` | Stdlib (to_float) | double→identity; i64→@to_float if JSON/Any else cast; i8*→@kain_parse_f64_string | LLVMBuildCall (to @to_float/@kain_parse_f64_string) | mapped | ~2400-2436 |
| 64 | `decompose_char_at_call` | String (char_at) | Decomposes char_at(string, index) call into (text_expr, index_expr) | N/A (call decomposition) | mapped | ~2438-2449 |
| 65 | `compile_char_at_string_equality_fast_path` | String (fast path) | Emits two char_at byte loads, bounds checks, phi merge for char equality | LLVMBuildGEP + LLVMBuildLoad + LLVMBuildICmp + LLVMBuildPhi | mapped | ~2451-2568 |
| 66 | `compile_byte_at_fast_path` | String (fast path) | Emits byte_at: null+index bounds check, GEP+load, phi(-1/byte) | LLVMBuildGEP + LLVMBuildLoad + LLVMBuildPhi | mapped | ~2570-2642 |
| 67 | `compile_find_substring_from_fast_path` | String (fast path) | Compiles find_substring_from(text, needle, start) via known-length inline search | LLVMBuildGEP + LLVMBuildCall (to @memchr/@memcmp) | mapped | ~2644-2672 |
| 68 | `compile_known_length_find_substring_inline` | String (codegen) | ~300-line inline search: null checks, needle fits, memchr loop, static byte tail compare, phi merge | LLVMBuildCall (to @memchr/@memcmp) + LLVMBuildPhi | mapped | ~2674-2968 |
| 69 | `compile_known_length_find_substring_inline_static_two_byte_needle` | String (codegen) | Optimized 2-byte needle search: packed i16 compare instead of memchr tail check | LLVMBuildLoad + LLVMBuildICmp (i16 packed) + LLVMBuildPhi | mapped | ~2970-3126 |
| 70 | `expr_strip_parens` | Utility (expr) | Recursively strips Expr::Paren, Expr::Cast, Expr::Bitcast wrappers | N/A (expr unwrap) | mapped | 3128-3136 |
| 71 | `pattern_binding_name` | Utility (pattern) | Extracts binding name from Pattern::Binding | N/A (pattern extraction) | mapped | 3138-3143 |
| 72 | `expr_is_ident` | Utility (expr) | Checks if stripped expr is Expr::Ident with expected name | N/A (ident check) | mapped | 3145-3147 |
| 73 | `expr_int_literal` | Utility (expr) | Extracts i64 from Expr::Int or unary-neg Int | N/A (int extraction) | mapped | 3149-3159 |
| 74 | `expr_is_zero` | Utility (expr) | Checks if expr_int_literal == Some(0) | N/A (zero check) | mapped | 3161-3163 |
| 75 | `expr_is_len_call_of` | Substring (analysis) | Checks if expr is len(ident) call matching expected ident | N/A (call pattern) | mapped | 3165-3175 |
| 76 | `expr_is_manual_substring_needle_len` | Substring (analysis) | Checks expr = needle_len_binding OR len(needle_name) | N/A (pattern match) | mapped | 3177-3186 |
| 77 | `match_manual_substring_needle_len_binding` | Substring (analysis) | Matches let binding = len(needle_name) stmt | N/A (stmt match) | mapped | 3188-3204 |
| 78 | `stmt_is_manual_substring_empty_needle_guard` | Substring (analysis) | Matches if needle_len == 0: return start pattern | N/A (stmt pattern) | mapped | 3206-3249 |
| 79 | `match_manual_substring_index_init` | Substring (analysis) | Matches let index = start_name binding | N/A (stmt match) | mapped | 3251-3267 |
| 80 | `expr_is_manual_substring_search_bound` | Substring (analysis) | Matches index + needle_len <= len(text) bound check | N/A (expr pattern) | mapped | 3269-3306 |
| 81 | `stmt_is_manual_substring_match_guard` | Substring (analysis) | Matches if starts_with_at(text, index, needle): return index guard | N/A (stmt pattern) | mapped | 3308-3344 |
| 82 | `stmt_is_manual_substring_increment` | Substring (analysis) | Matches index = index + 1 increment statement | N/A (stmt pattern) | mapped | 3346-3367 |
| 83 | `stmt_is_manual_substring_search_loop` | Substring (analysis) | Matches while index + needle_len <= len(text): [match_guard, increment] | N/A (stmt pattern) | mapped | 3369-3401 |
| 84 | `match_manual_substring_miss_return` | Substring (analysis) | Matches return len(text) or return -1 miss behavior | N/A (stmt pattern) | mapped | 3403-3417 |
| 85 | `detect_manual_find_substring_function` | Substring (analysis) | Full 5-stmt pattern: (needle_len_binding?) + empty_guard + index_init + search_loop + miss_return | N/A (function pattern) | mapped | 3419-3471 |
| 86 | `expr_is_direct_string_byte_view` | String (utility) | Checks if stripped expr is String literal or Ident | N/A (bool check) | mapped | 3473-3478 |
| 87 | `compile_direct_string_view_and_length` | String (codegen) | Compiles (ptr, len) pair for direct string byte views | LLVMBuildGEP + LLVMBuildCall (to @len) | mapped | 3480-3492 |
| 88 | `compile_manual_find_substring_call_fast_path` | String (fast path) | Compiles manual find_substring call: empty→start, else inline search with miss shaping | LLVMBuildCall (to @memchr/@memcmp) + LLVMBuildPhi | mapped | 3494-3547 |
| 89 | `map_type` | Type mapping (LLVM IR) | Resolves Kain ResolvedType → LLVM type string (i64, double, i1, i8*, %StructName*, etc.) | LLVMIntType/LLVMPointerType/LLVMStructType-like | mapped | 3549-3585 |
| 90 | `prefer_resolved_param_codegen_type` | Type mapping (param) | Prefers resolved type's LLVM type over authored if authored was i64 but resolved is different | N/A (type preference) | mapped | 3587-3600 |
| 91 | `resolved_struct_pointer_llvm_ty` | Type mapping (ptr) | Extracts struct pointer LLVM type (%StructName*) from Ptr/Ref resolved types | N/A (struct ptr type) | mapped | 3602-3611 |
| 92 | `unit_only_enum_name_from_llvm_ptr` | Type mapping (enum) | Extracts unit-only enum name from LLVM type string (%EnumName*) via unit_only_enums set | N/A (enum name extraction) | mapped | 3613-3618 |

---

## Chunk 04: Values (`chunk-04-values.tsv`)

Source: `mod.rs`, value-level codegen: strings, tagged values, atomics, mem ops, ownership, fanout, teleport  
Rows: ~88

| # | function_name | kain_concept | llvm_ir_pattern | llvm_c_api | status | lines |
|---|--------------|-------------|-----------------|------------|--------|-------|
| 1 | `runtime_symbol_for_stdlib_function` | L0-fn, stdlib symbol resolution | Compile-time string match → runtime symbol name remapping | none (Rust helper) | mapped | 234-241 |
| 2 | `stdlib_function_uses_borrowed_string_param` | L0-fn, stdlib ABI | String param borrow check via match tuple | none (Rust helper) | mapped | 243-248 |
| 3 | `kain_map_codegen_mix_u64` | L0-fn, hash helper | u64 xor-shift-multiply mixing (no IR emission) | none (Rust helper) | mapped | 250-257 |
| 4 | `kain_map_codegen_hash_bytes` | L0-fn, hash helper | u64 rolling hash with Murmur-style mixing (no IR) | none (Rust helper) | mapped | 259-284 |
| 5 | `kain_map_codegen_magic_prefix_state` | L0-fn, hash helper | 4-word folded state with magic constants (no IR) | none (Rust helper) | mapped | 286-304 |
| 6 | `kain_map_codegen_static_key_metadata` | L0-fn, hash helper | Extract (key_length, key_hash, key_prefix) tuple (no IR) | none (Rust helper) | mapped | 306-335 |
| 7 | `llvm_runtime_declaration_is_preemitted` | runtime, external decl tracking | Compile-time string match for pre-emitted symbols | none (Rust helper) | mapped | 337-382 |
| 8 | `python_import_binding_infos` | Python import | Compile-time binding info extraction from Import AST (no IR) | none (Rust helper) | mapped | 384-422 |
| 9 | `llvm_orchestrate_trace_enabled` | orchestrate, env flag | Env var check for `KAIN_LLVM_ORCHESTRATE_TRACE` or `KAIN_NATIVE_PROFILE` | none (Rust helper) | mapped | 424-447 |
| 10 | `resolve_host_llvm_target_descriptor` | LLVM target model | Delegates to `LlvmTargetDescriptor::host()` | LLVMGetHostCPUName, LLVMGetHostCPUFeatures | mapped | 449-451 |
| 11 | `resolve_llvm_target_for_compile_target` | LLVM target model | Match on CompileTarget → `for_triple("x86_64-unknown-none")` for BareMetal | LLVMGetDefaultTargetTriple | mapped | 454-461 |
| 12 | `generate` | LLVM IR generation, public entry | `generate_with_target(program, &host_descriptor)` → produces Vec<u8> | LLVMTargetMachineEmitToMemoryBuffer | mapped | 463-465 |
| 13 | `generate_with_target` | LLVM IR generation | `generate_with_options(program, false, None, "", target)` | LLVMTargetMachineEmitToMemoryBuffer | mapped | 467-472 |
| 14 | `generate_llvm_for_target` | LLVM IR generation, cross-target | Resolves target triple → `generate_with_target` | LLVMTargetMachineEmitToMemoryBuffer | mapped | 478-487 |
| 15 | `generate_with_debug` | LLVM IR generation, DWARF | `generate_with_options(program, true, source, filename, host)` with debug metadata | LLVMAddNamedMetadataOperand, LLVMSetCurrentDebugLocation | mapped | 495-503 |
| 16 | `generate_with_debug_for_target` | LLVM IR generation, DWARF+cross | `generate_with_options` with debug=+target_triple | LLVMAddNamedMetadataOperand, LLVMSetCurrentDebugLocation | mapped | 510-521 |
| 17 | `generate_with_options` | LLVM IR generation, internal | `lower_typed_program_memory` + `validate` + `gen.compile_module` → bytes | LLVMContextCreate, LLVMModuleCreateWithNameInContext | mapped | 523-547 |
| 18 | `codegen_error` | error helper | Creates DiagnosticReport + semantic enrichment | none (Rust helper) | mapped | 846-858 |
| 19 | `compile_enum_tag_load` | enum, tagged values | `getelementptr inbounds %Enum, %Enum* %val, i32 0, i32 0` + `load i64` | LLVMBuildGEP2, LLVMBuildLoad2 | mapped | 862-873 |
| 20 | `intern_string_global_name` | string literal dedup | Allocates `@.str.N` global symbol, stores in HashMap | LLVMAddGlobal | mapped | 875-884 |
| 21 | `compile_string_literal` | string literal | `getelementptr [N x i8]* @.str.N → i8*` + `call i8* @string_new(i8*)` | LLVMBuildGEP2, LLVMBuildCall2, LLVMAddGlobal | mapped | 886-938 |
| 22 | `string_literal_release_after_use` | string literal lifecycle | Checks entry_preamble_insert_offset + scopes depth | none (Rust helper) | mapped | 940-942 |
| 23 | `compile_string_literal_value` | string literal wrapper | Calls compile_string_literal + tracks release flag | LLVMBuildGEP2, LLVMBuildCall2 | mapped | 944-948 |
| 24 | `compile_static_c_string_literal` | C string literal | `getelementptr [N x i8]* @.str.N, i64 0, i64 0` (no string_new call) | LLVMBuildGEP2 | mapped | 950-960 |
| 25 | `concat_strings` | string concatenation | `call i8* @str_concat(i8* %lhs, i8* %rhs)` | LLVMBuildCall2 | mapped | 962-969 |
| 26 | `stringify_value` | value-to-string conversion | `sext i8→i64` or `zext i1→i64` or `call @llvm.fptosi.sat.i64.f64` + `call i8* @to_string(i64)` | LLVMBuildCall2, LLVMBuildSExt | mapped | 971-1005 |
| 27 | `zero_value_for_ty` | zero initializer | Pattern matches type → "0.0", "0", "null", "zeroinitializer" | LLVMConstNull, LLVMConstReal, LLVMConstInt | mapped | 1007-1016 |
| 28 | `compile_expr_for_target_type` | expression lowering, type coercion | Dispatches unwrap/expect/unwrap_or on Option/Result i8*; falls through to ask/None/coerce | LLVMBuildLoad2, LLVMBuildPhi, LLVMBuildCall2 | mapped | 1018-1136 |
| 29 | `coerce_compiled_value_to_target_type` | LLVM type coercion | bitcast ptr→ptr, inttoptr i64→ptr*, load %T from %T*, ptrtoint ptr→i64, bitcast %T→%U | LLVMBuildBitCast, LLVMBuildIntToPtr, LLVMBuildLoad2 | mapped | 1138-1222 |
| 30 | `align_abi_size` | ABI alignment helper | `size.div_ceil(align) * align` | none (pure math) | mapped | 1224-1230 |
| 31 | `abi_layout_for_ty` | ABI layout computation | Recursive struct field size/align from struct_defs; scalar size table | LLVMOffsetOfElement, LLVMStructGetSizeInBits | mapped | 1232-1264 |
| 32 | `compile_bitcast_expr` | bitcast / type punning | `bitcast %src %val to %dst` or `ptrtoint` or `inttoptr` if widths differ | LLVMBuildBitCast, LLVMBuildPtrToInt, LLVMBuildIntToPtr | mapped | 1266-1327 |
| 33 | `emit_tagged_value_handle_bits` | tagged value, immediate encoding | `ptrtoint i8* %boxed to i64` → handle bits | LLVMBuildPtrToInt | mapped | 1329-1336 |
| 34 | `emit_tagged_immediate_tag_bits_from_handle_bits` | tagged immediate decoding | `and i64 %handle, 7` → tag bits (mask 7) | LLVMBuildAnd | mapped | 1338-1345 |
| 35 | `compile_tagged_immediate_integer_handle_from_i64` | tagged immediate encoding | `shl i64 %value, 3` → `or i64 %shifted, %tag` → `inttoptr i64 %tagged to i8*` | LLVMBuildShl, LLVMBuildOr, LLVMBuildIntToPtr | mapped | 1347-1365 |
| 36 | `compile_tagged_immediate_integer_payload_from_i64_bits` | tagged immediate decoding | `ashr i64 %handle_bits, 3` → optional cast to target type | LLVMBuildAShr, LLVMBuildTrunc, LLVMBuildSExt | mapped | 1367-1380 |
| 37 | `compile_tagged_immediate_borrowed_pointer_handle` | tagged borrowed pointer | `ptrtoint i8* %ptr to i64` → `or i64 %ptr_bits, %tag` → `inttoptr i64 %tagged to i8*` | LLVMBuildPtrToInt, LLVMBuildOr, LLVMBuildIntToPtr | mapped | 1382-1403 |
| 38 | `emit_tagged_value_tag_load` | tagged value tag access | `bitcast i8* %boxed to i64*` + `load i64` | LLVMBuildBitCast, LLVMBuildLoad2 | mapped | 1405-1414 |
| 39 | `emit_tagged_value_payload_ptr` | tagged value payload GEP | `getelementptr i8, i8* %boxed, i64 16` → past 16-byte header | LLVMBuildGEP2 | mapped | 1416-1423 |
| 40 | `compile_tagged_value_is_tag` | tagged value tag matching | `icmp eq i8* %boxed, null` + branch → immediate path (and i64,7, icmp eq tag) + boxed path (load i64, icmp eq) → phi merge | LLVMBuildICmp, LLVMBuildPhi, LLVMBuildAnd, LLVMBuildLoad2 | mapped | 1425-1516 |
| 41 | `compile_tagged_value_payload_copy` | tagged value payload extraction | Handle bits → ashr 3 → trunc/sext to target; boxed → GEP to payload + load + align 1 + phi merge | LLVMBuildGEP2, LLVMBuildLoad2, LLVMBuildPhi, LLVMBuildBitCast | mapped | 1518-1593 |
| 42 | `compile_tagged_value_from_compiled_payload` | tagged value boxing | Integer: range check (sge/sle, and, br) → immediate shl/or/inttoptr or boxed KAIN_alloc+store; non-integer: always boxed KAIN_alloc | LLVMBuildICmp, LLVMBuildAnd, LLVMBuildPhi, LLVMBuildCall2 | mapped | 1595-1669 |
| 43 | `compile_tagged_box_from_payload` | tagged heap box allocation | `call i8* @KAIN_alloc(size)` + store tag i64 + store size i64 + memcpy payload via bitcast+load+store | LLVMBuildCall2, LLVMBuildBitCast, LLVMBuildStore | mapped | 1671-1737 |
| 44 | `compile_tagged_box_from_value` | tagged value to box | Delegates to compile_tagged_value_from_compiled_payload if size>0; otherwise alloc+tag only | LLVMBuildCall2, LLVMBuildBitCast, LLVMBuildStore | mapped | 1739-1797 |
| 45 | `compile_runtime_mem_load` | raw memory load (collapse/observe) | Ephemeral: `load %ty, %ty* %typed_ptr, align N`; fallback: compile_non_ephemeral_typed_memory_pointer + load | LLVMBuildLoad2 | mapped | 1799-1846 |
| 46 | `compile_runtime_mem_store` | raw memory store (collapse) | Ephemeral: `store %ty %val, %ty* %typed_ptr`; fallback: same via non_ephemeral | LLVMBuildStore | mapped | 1848-1906 |
| 47 | `compile_runtime_volatile_mem_load` | volatile MMIO load | `load volatile %ty, %ty* %typed_ptr, align N` | LLVMBuildLoad2 | mapped | 1908-1950 |
| 48 | `compile_runtime_volatile_mem_store` | volatile MMIO store | `store volatile %ty %val, %ty* %typed_ptr` | LLVMBuildStore | mapped | 1952-1993 |
| 49 | `compile_atomic_i64_pointer` | atomic pointer preparation | compile_expr → coerce_to_i64_storage → `inttoptr i64 %val to i64*` | LLVMBuildIntToPtr | mapped | 1995-2004 |
| 50 | `atomic_ordering_code_from_expr` | atomic ordering constant | Matches Int/String/Ident → 0=relaxed through 4=seq_cst | none (compile-time eval) | mapped | 2006-2028 |
| 51 | `atomic_ordering_code_from_name` | atomic ordering name lookup | String match: relaxed/acquire/release/acq_rel/seq_cst → 0-4 | none (compile-time) | mapped | 2030-2039 |
| 52 | `atomic_ordering_name_from_code` | atomic ordering reverse lookup | Code 0-4 → string name | none (compile-time) | mapped | 2041-2050 |
| 53 | `validate_atomic_ordering_code` | atomic ordering validation | Range check (0..=4), error otherwise | none (compile-time) | mapped | 2052-2061 |
| 54 | `atomic_ordering_strength` | atomic ordering strength ordering | 0→0, 1→2, 2→3, 3→4, 4→5 | none (compile-time) | mapped | 2063-2071 |
| 55 | `validate_atomic_store_ordering` | atomic store ordering validation | Relaxed/release/seq_cst only; acquire/acq_rel rejected | none (compile-time) | mapped | 2073-2086 |
| 56 | `validate_atomic_compare_exchange_failure_ordering` | atomic CAS ordering validation | Failure ord must not be release/acq_rel, must be ≤ success strength | none (compile-time) | mapped | 2088-2117 |
| 57 | `llvm_atomic_load_ordering` | atomic LLVM ordering string | Code→"monotonic"|"acquire"|"seq_cst" | LLVMAtomicOrdering enum | mapped | 2119-2125 |
| 58 | `llvm_atomic_store_ordering` | atomic LLVM store ordering | Code→"monotonic"|"release"|"seq_cst" | LLVMAtomicOrdering enum | mapped | 2127-2133 |
| 59 | `llvm_atomic_rmw_ordering` | atomic LLVM RMW ordering | Code→"monotonic"|"acquire"|"release"|"acq_rel"|"seq_cst" | LLVMAtomicRMWBinOp + ordering | mapped | 2135-2143 |
| 60 | `llvm_atomic_failure_ordering` | atomic LLVM failure ordering | Code→"monotonic"|"acquire"|"seq_cst" | LLVMAtomicOrdering enum | mapped | 2145-2151 |
| 61 | `compile_ordered_atomic_load` | atomic ordered load | `load atomic i64, i64* %ptr monotonic/acquire/seq_cst, align 8` | LLVMBuildLoad2 + LLVMAtomicOrdering | mapped | 2153-2168 |
| 62 | `compile_ordered_atomic_store` | atomic ordered store | `store atomic i64 %val, i64* %ptr monotonic/release/seq_cst, align 8` | LLVMBuildStore + LLVMAtomicOrdering | mapped | 2170-2189 |
| 63 | `compile_ordered_atomic_rmw` | atomic RMW | `%prev = atomicrmw %op i64* %ptr, i64 %val %ordering` | LLVMBuildAtomicRMW | mapped | 2191-2209 |
| 64 | `compile_ordered_atomic_compare_exchange` | atomic CAS | `%pair = cmpxchg i64* %ptr, i64 %expected, i64 %desired %succ %fail` + `extractvalue %pair, 1` | LLVMBuildCmpXchg | mapped | 2211-2242 |
| 65 | `compile_ordered_atomic_fence` | atomic fence | `fence acquire` / `fence release` / `fence acq_rel` / `fence seq_cst` | LLVMBuildFence | mapped | 2244-2258 |
| 66 | `emit_scaled_byte_offset` | pointer arithmetic | `shl i64 %offset, %shift` if power-of-2, else `mul i64 %offset, %stride` | LLVMBuildShl, LLVMBuildMul | mapped | 2260-2285 |
| 67 | `compile_raw_ptr_offset_i64` | ptr<T> offset | compile_expr → ptrtoint/inttoptr → getelementptr i8, i8* %base, i64 %offset → ptrtoint to i64 | LLVMBuildGEP2, LLVMBuildPtrToInt, LLVMBuildIntToPtr | mapped | 2287-2323 |
| 68 | `compile_ownership_pointer` | ownership (collapse/observe) | compile_expr → bitcast to i8*; emit_lazy_import_ownership_region for Imported/Unknown provenance | LLVMBuildBitCast, LLVMBuildIntToPtr | mapped | 2325-2347 |
| 69 | `emit_lazy_import_ownership_region` | ownership import guard | `call i32 @__kain_ownership_ensure_imported(i8*)` + `icmp eq i32 %status, 0` + br abort/continue | LLVMBuildCall2, LLVMBuildICmp | mapped | 2349-2371 |
| 70 | `emit_checked_ownership_call` | ownership state machine | `call i32 @function_name(i8*)` + `icmp eq i32 %status, 0` + br abort/continue | LLVMBuildCall2, LLVMBuildICmp | mapped | 2373-2391 |
| 71 | `emit_helper_owned_local_decay_cleanup` | ownership decay cleanup | `load i8*` → `icmp ne i8* %ptr, null` → `call __kain_ownership_decay_helper` | LLVMBuildLoad2, LLVMBuildICmp, LLVMBuildCall2 | mapped | 2393-2408 |
| 72 | `clear_decayed_helper_owned_local` | ownership decay nulling | Store null to decayed helper-owned local | LLVMBuildStore | mapped | 2410-2424 |
| 73 | `helper_realloc_source_local_name` | ownership realloc tracking | Extracts source name from `__kain_realloc(ident, ...)` call | none (Rust helper) | mapped | 2426-2438 |
| 74 | `invalidate_helper_owned_local_storage` | ownership storage invalidation | Store null to helper-owned i8* local | LLVMBuildStore | mapped | 2440-2451 |
| 75 | `collect_helper_owned_pointer_mem_store_transfer_locals` | ownership transfer tracking | Recursively collect helper-owned locals in expr | none (Rust helper) | mapped | 2453-2472 |
| 76 | `helper_owned_pointer_mem_store_transfer_locals` | ownership transfer wrap | Calls collect, returns HashSet | none (Rust helper) | mapped | 2474-2478 |
| 77 | `mark_helper_owned_pointer_transfers` | ownership transfer marking | Remove from helper_owned, insert into borrowed_locals | none (Rust state) | mapped | 2480-2486 |
| 78 | `invalidate_consumed_helper_realloc_source_for_let` | ownership let realloc | Invalidate storage of realloc source on let binding | LLVMBuildStore | mapped | 2488-2493 |
| 79 | `invalidate_consumed_helper_realloc_source_for_assignment` | ownership assignment realloc | Invalidate storage of realloc source on assignment | LLVMBuildStore | mapped | 2495-2507 |
| 80 | `compile_scoped_ownership_expr` | collapse/observe scope | Ephemeral: skip; else: `ownership_begin(ptr)` + body + `ownership_end(ptr)` | LLVMBuildCall2 | mapped | 2509-2538 |
| 81 | `compile_decay_expr` | decay statement | `__kain_ownership_decay(ptr)` or `__kain_ownership_decay_helper(ptr)` + clear_helper | LLVMBuildCall2 | mapped | 2540-2557 |
| 82 | `emit_abort_on_nonzero_i32_status` | abort-on-failure guard | `icmp eq i32 %status, 0` → br continue else call abort+unreachable | LLVMBuildICmp, LLVMBuildCall2 | mapped | 2559-2572 |
| 83 | `compile_range_iter_bounds` | for loop range bounds | Compiles range(start, end) or Expr::Range { start, end, inclusive } → add one for inclusive | LLVMBuildAdd, LLVMBuildICmp | mapped | 2574-2628 |
| 84 | `compile_fanout_worker_function` | share/fanout worker | Emitted as `define internal void @worker(i8* %ctx_arg, i64 %index_arg)` with entry label + GEP captures + compile_block | none (standalone function) | mapped | 2630-2700 |
| 85 | `compile_fanout_stmt` | share/fanout statement | Captures locals, creates context struct, calls `__kain_fanout_i64(i64 start, i64 end, i8* ctx, void(i8*,i64)* @worker)` | LLVMBuildCall2 | mapped | 2702-2792 |
| 86 | `compile_payload_pointer_from_value` | value→payload pointer | Entry alloca + store + bitcast to i8*; RC retain for i8* types | LLVMBuildAlloca, LLVMBuildStore, LLVMBuildBitCast | mapped | 2794-2827 |
| 87 | `compile_tagged_payload_copy` | tagged value copy | Entry alloca + bitcast to i8* + `call @abi_future_await_payload_copy(i8*, i8*, i64)` + load result | LLVMBuildCall2, LLVMBuildAlloca, LLVMBuildLoad2 | mapped | 2829-2859 |

---

## Chunk 05: ABI (`chunk-05-abi.tsv`)

Source: `mod.rs`, cross-cutting ABI functions  
Rows: ~50

| # | function_name | kain_concept | llvm_ir_pattern | llvm_c_api | status | lines |
|---|--------------|-------------|-----------------|------------|--------|-------|
| 1 | `compile_native_option_or_result_variant` | Option/Result tagged value | `None`→null; `Some/Ok/Err`→compile_expr → compile_tagged_value/borrowed_pointer_handle | LLVMBuildCall2, LLVMBuildIntToPtr | mapped | 868-913 |
| 2 | `compile_native_variant_function_call` | Some/Ok/Err variants | `Some(val)`: extract literal → borrowed pointer handle, else compile_expr→tagged; `Ok/Err(val)`: same with Result tags | LLVMBuildCall2, LLVMBuildIntToPtr | mapped | 915-973 |
| 3 | `compile_async_block` | async/await Future | `call i8* @abi_future_ready_from_value(i8* %payload_ptr, i64 %size)` | LLVMBuildCall2 | mapped | 975-985 |
| 4 | `extract_immediate_ready_future_payload_expr` | async future inlining | Pattern-match Expr::AsyncBlock/Return/Paren to extract inner payload | none (Rust pattern match) | mapped | 987-996 |
| 5 | `extract_zero_arg_immediate_ready_future_payload` | async future zero-arg | fn returns Future; single return/expr body → extract payload for inlining | none (Rust analysis) | mapped | 998-1025 |
| 6 | `compile_immediate_ready_future_for_target_type` | async future inlining for type | Inline AsyncBlock/Paren/Call payload into target type | LLVMBuildCall2 (via compile_expr_for_target_type) | mapped | 1027-1052 |
| 7 | `compile_await_for_target_type` | await expression | Tries immediate inlining; else `compile_tagged_payload_copy(future, target, "abi_future_await_payload_copy")` | LLVMBuildCall2, LLVMBuildAlloca, LLVMBuildLoad2 | mapped | 1054-1080 |
| 8 | `compile_try_for_target_type` | try operator (`?`) | Test is_success tag; br payload/residual; residual → ret boxed_value; payload → compile_tagged_value_payload_copy | LLVMBuildICmp, LLVMBuildPhi, LLVMBuildCall2 | mapped | 1082-1127 |
| 9 | `coerce_to_i64_storage` | value→i64 coercion | sext i32→i64, zext i1→i64, sext i8→i64, fptosi.sat f64→i64, ptrtoint ptr→i64, KAIN_alloc+store %T+ptrtoint for struct values | LLVMBuildSExt, LLVMBuildZExt, LLVMBuildCall2, LLVMBuildPtrToInt | mapped | 1129-1193 |
| 10 | `coerce_runtime_array_storage_from_compiled` | runtime array storage | bitcast double→i64 for array storage; fallback coerce_to_i64_storage | LLVMBuildBitCast | mapped | 1195-1205 |
| 11 | `expr_returns_json_handle` | JSON handle detection | Delegates to expr_carries_json_value | none (Rust helper) | mapped | 1207-1209 |
| 12 | `encode_json_any_i64_payload` | JSON Any i64 encoding | `shl i64 %value, 3` → `or i64 %shifted, %tag` | LLVMBuildShl, LLVMBuildOr | mapped | 1211-1217 |
| 13 | `compile_json_any_argument` | JSON Any argument | No-match/null → tag; i64→shl3+or; i32/i8→widen+encode; i1→widen+bool tag; double→@json_box_float; i8*→ptrtoint+string tag; ptr→coerce_to_i64 | LLVMBuildShl, LLVMBuildOr, LLVMBuildCall2, LLVMBuildPtrToInt | mapped | 1219-1298 |
| 14 | `compile_runtime_any_argument` | runtime Any argument | Array/Tuple→@json_array_new+push+release; runtime_array→@json_box_runtime_array; JSON/runtime Any passthrough; fallback compile_json_any_argument | LLVMBuildCall2 | mapped | 1300-1378 |
| 15 | `compile_python_bridge_vararg_call` | Python bridge vararg call | Compile 2-4 args + runtime_any; dispatch to py_call_raw_args/py_call_raw_attr/py_call_args/py_call_attr_args; `call i64 @helper(i64 args...)` + release | LLVMBuildCall2 | mapped | 1380-1477 |
| 16 | `compile_json_builtin_call` | JSON builtin operations | `json_object_set(i64, i8*, i64)`; `json_array_push(i64, i64)`; `json_string(i64)→i8*` | LLVMBuildCall2 | mapped | 1479-1536 |
| 17 | `cast_numeric_value` | numeric type cast | ptrtoint→i64; sitofp/trunc/sext/zext/fptosi.sat/fcmp une between i64/i32/i8/i1/double/ptr identity combinations | LLVMBuildPtrToInt, LLVMBuildSIToFP, LLVMBuildTrunc, LLVMBuildSExt, LLVMBuildZExt, LLVMBuildCall2, LLVMBuildFCmp | mapped | 1538-1632 |
| 18 | `emit_saturating_fptosi` | saturating float→int | `call %ty @llvm.fptosi.sat.<ty>.f64(double %val)` | LLVMBuildCall2 (intrinsic) | mapped | 1635-1653 |
| 19 | `coerce_condition_value_to_i1` | condition→i1 coercion | i1 passthrough; i64/i32/i8/double/ptr → cast_numeric_value to i1 | LLVMBuildICmp, LLVMBuildFCmp, LLVMBuildTrunc | mapped | 1655-1670 |
| 20 | `compile_condition_expr` | condition expression wrapper | compile_expr → coerce_condition_value_to_i1 | LLVMBuildICmp, LLVMBuildFCmp | mapped | 1672-1675 |
| 21 | `coerce_binary_operands` | binary operand coercion | Match both to highest type: double→both double, i64→both i64, else pass through | LLVMBuildSIToFP, LLVMBuildSExt, LLVMBuildZExt | mapped | 1677-1709 |
| 22 | `compile_value_eq` | equality comparison | Coerce operands, then `call @deep_eq` for i8*, `fcmp oeq` for double, `icmp eq` for int/ptr types | LLVMBuildICmp, LLVMBuildFCmp, LLVMBuildCall2 | mapped | 1711-1755 |
| 23 | `compile_range_check` | range check (law/converge) | Compile lower bound → `icmp sge`/`fcmp oge`; compile upper → `icmp sle/slt`/`fcmp ole/olt`; chain with `and` | LLVMBuildICmp, LLVMBuildFCmp, LLVMBuildAnd | mapped | 1757-1824 |
| 24 | `compile_pattern_condition` | match pattern condition | Wildcard/Binding→true; Literal→eq; Range→range_check; Or→or chain; Variant→tag match via native tagged is_tag or GEP+load tag | LLVMBuildICmp, LLVMBuildAnd, LLVMBuildOr, LLVMBuildGEP2, LLVMBuildLoad2 | mapped | 1826-1912 |
| 25 | `bind_local_pattern_value` | match pattern binding | Binding→alloca+store; Tuple→GEP each field+load+bind; Struct→GEP+load each named field+bind | LLVMBuildAlloca, LLVMBuildStore, LLVMBuildGEP2, LLVMBuildLoad2 | mapped | 1914-2049 |
| 26 | `bind_variant_pattern_fields` | enum variant field binding | Payload struct GEP from enum slot 1; load i8*; bitcast to payload struct*; GEP each tuple/struct field; bind | LLVMBuildGEP2, LLVMBuildLoad2, LLVMBuildBitCast | mapped | 2051-2154 |
| 27 | `bind_match_pattern` | match pattern top-level | Dispatches to bind_local_pattern_value or bind_variant_pattern_fields; handles i8* native tagged variant with payload copy | LLVMBuildGEP2, LLVMBuildLoad2, LLVMBuildBitCast, LLVMBuildAlloca | mapped | 2156-2215 |
| 28 | `ptr_struct_name` | ptr struct name extraction | Strip `%` prefix and `*` suffix from LLVM type string | none (Rust string op) | mapped | 2217-2223 |
| 29 | `field_index` | struct field index lookup | Linear search struct_defs for field name; tuple field alias fallback for `__kain_tuple*` | none (Rust HashMap) | mapped | 2225-2238 |
| 30 | `native_world_field_path` | world field path resolution | Build "WorldName.field" string if world_globals contains struct_name | none (Rust string op) | mapped | 2240-2246 |
| 31 | `native_entangle_authority_binding` | entangle authority binding lookup | Find NativeEntangleBinding where authority==path | none (Rust iteration) | mapped | 2248-2253 |
| 32 | `native_entangle_mirror_binding` | entangle mirror binding lookup | Find NativeEntangleBinding where mirror==path | none (Rust iteration) | mapped | 2255-2260 |
| 33 | `emit_resonance_after_store` | resonate tripwire after field write | `abi_resonate_should_fire_i64/f64(i8* target, i64 dampen, i64 old, i64 new)` → `icmp ne i64, 0` → br fire/done → `call @handler(old_i64, new_i64)` + `call @abi_resonate_exit` | LLVMBuildCall2, LLVMBuildICmp | mapped | 2262-2346 |
| 34 | `direct_struct_literal_name` | shatter struct literal name | Match Expr::Struct on shattered_structs set | none (Rust pattern) | mapped | 2348-2354 |
| 35 | `shattered_array_expr_struct_name` | shattered array struct name | Check all items have same shattered struct type | none (Rust helper) | mapped | 2356-2369 |
| 36 | `emit_shatter_lane_bases` | shatter lane runtime base | `call i8* @kain_machine_shatter_lane_base(i8* handle, i64 lane_index)` per lane | LLVMBuildCall2 | mapped | 2371-2382 |
| 37 | `emit_stack_shatter_lane_bases` | shatter stack lane base | Entry alloca [element_count x i64] per lane; bitcast to i8* | LLVMBuildAlloca, LLVMBuildBitCast | mapped | 2384-2407 |
| 38 | `populate_shattered_array_literal_lanes` | shatter array literal population | For each element: getelementptr into lane_base[i64] at element_index*8; bitcast i8*→ty*; store field value | LLVMBuildGEP2, LLVMBuildBitCast, LLVMBuildStore | mapped | 2409-2464 |
| 39 | `shattered_index_is_proven_in_bounds` | shatter bounds check | Check literal index in range OR loop bounds upper_exclusive ≤ element_count | none (Rust analysis with Z3 proof) | mapped | 2466-2488 |
| 40 | `shattered_literal_byte_offset` | shatter byte offset | `index * 8` for literal index, None for negative | none (Rust helper) | mapped | 2490-2497 |
| 41 | `compile_shattered_field_ptr` | shatter field pointer | For Index+Field: compute lane GEP from lane_base or call @kain_machine_shatter_lane_ptr; bitcast to field type | LLVMBuildGEP2, LLVMBuildBitCast, LLVMBuildCall2 | mapped | 2499-2613 |
| 42 | `emit_patch_record_i64` | patch journal record | `call i64 @abi_patch_record_i64(i8* patch_name, i8* path, i64 old, i64 new)` | LLVMBuildCall2 | mapped | 2615-2625 |
| 43 | `emit_entangle_i64_propagation` | entangle propagation | GEP mirror world struct field + store i64 + `call @abi_entangle_record_i64(i8*, i8*, i64)` | LLVMBuildGEP2, LLVMBuildStore, LLVMBuildCall2 | mapped | 2627-2661 |
| 44 | `compile_temporary_address` | temporary address alloca | Entry alloca + store expr value; returns (addr, ty) | LLVMBuildAlloca, LLVMBuildStore | mapped | 2663-2670 |
| 45 | `compile_index_address_from_compiled` | index expression address | GEP on pointer type; inttoptr i64→i64* + GEP for i64 base | LLVMBuildGEP2, LLVMBuildIntToPtr | mapped | 2672-2704 |
| 46 | `compile_shattered_array_literal` | shatter array literal | `call i8* @kain_machine_shatter_alloc(i64 field_count, i64 item_count)` + emit_shatter_lane_bases + populate | LLVMBuildCall2 | mapped | 2706-2726 |
| 47 | `compile_teleport_expr` | teleport expression | Pointer: `call i8* @kain_machine_teleport_ptr(i8* ptr, i8* src, i8* dst, i8* chan)`. Scalar: `call void @kain_machine_teleport_note(i8*, i8*, i8*)` | LLVMBuildCall2 | mapped | 2728-2773 |
| 48 | `compile_field_addressable_ptr` | field addressable pointer | Shattered field → compile_shattered_field_ptr; local struct → GEP; authored pointer struct → load ptr+inttoptr+GEP; compiled val → alloca+GEP; returns (field_ptr, field_ty, struct_name_hint, is_mmio) | LLVMBuildGEP2, LLVMBuildBitCast, LLVMBuildLoad2 | mapped | 2775-2907 |
| 49 | `compile_addressable_ptr` | addressable pointer dispatch | Ident→local/const/python_global lookup; Field→compile_field_addressable_ptr; Index→compile_index_address_from_compiled; other→compile_temporary_address | LLVMBuildGEP2, LLVMBuildLoad2, LLVMBuildIntToPtr | mapped | 2909-2952 |

---

## Chunk 06: Functions (`chunk-06-functions.tsv`)

Source: `mod.rs`, function/item-level compilation  
Rows: ~46

| # | function_name | kain_concept | llvm_ir_pattern | llvm_c_api | status | lines |
|---|--------------|-------------|-----------------|------------|--------|-------|
| 1 | `compile_lowered_helper_call` | ptr<T>/mem_load/mem_store/atomic | bitcast/ptrtoint/load/store/call to @__kain_* helpers | LLVMBuildLoad/BuildStore/BuildCall | needs-review | 865-1390 |
| 2 | `jsx_span` | component/JSX | Span extraction from JSXNode | — | needs-review | 1392-1402 |
| 3 | `compile_jsx` | component/JSX | string concat/%KainActorRef/call @component | LLVMBuildCall/BuildGlobalString | needs-review | 1404-1555 |
| 4 | `hash_message_tag` | actor | DJBA2 hash via i64 ops | — | needs-review | 1557-1564 |
| 5 | `hash_emit_message_tag` | actor/emit | DJBA2 hash for __emit__ events | — | needs-review | 1566-1573 |
| 6 | `llvm_type_is_reply_port` | actor | Compare against %KainReplyPort | — | mapped | 1575-1577 |
| 7 | `actor_name_for_handle_type` | actor | Extract actor name from %Name* handle | — | needs-review | 1579-1592 |
| 8 | `compile_actor_handle_ref_value` | actor | extractvalue/getelementptr/load %KainActorRef | — | needs-review | 1594-1626 |
| 9 | `compile_actor_handle_id` | actor | extractvalue on %KainActorRef.0 | — | needs-review | 1628-1641 |
| 10 | `compile_actor_builtin_ask` | actor | ask/ask_timeout: insertvalue store/getelementptr/kain_actor_ask_send_ref | — | needs-review | 1643-1959 |
| 11 | `callable_signature` | fn | ResolvedType function signature extraction | LLVMGetElementType/GetParamTypes | needs-review | 1961-1977 |
| 12 | `extern_callable_signature` | fn/extern | ResolvedType signature for extern functions | LLVMGetElementType/GetParamTypes | needs-review | 1979-1993 |
| 13 | `ast_param_codegen_types` | fn | Map AST params to LLVM types | LLVMInt32Type/LLVMPointerType in wrapper | needs-review | 1995-2017 |
| 14 | `function_codegen_signature` | fn | Detect extern/naked/interrupt, map types | LLVMGetElementType/GetReturnType | needs-review | 2019-2038 |
| 15 | `function_is_extern` | fn/extern | Check for @extern attribute | — | mapped | 2040-2042 |
| 16 | `register_callable_signature` | fn | Register return/param types in HashMap | — | needs-review | 2044-2080 |
| 17 | `prescan_item_signatures` | fn/impl/const | Pre-scan signatures for all TypedItems | — | mapped | 2082-2270 |
| 18 | `register_type_definitions_recursive` | struct/enum/world/actor/component | Emit %type = type { fields } LLVM struct types | LLVMStructCreateNamed/StructSetBody | needs-review | 2272-2464 |
| 19 | `compile_typed_items` | all | Dispatch compile per TypedItem kind | — | mapped | 2466-2489 |
| 20 | `collect_native_entanglements` | entangle | Collect NativeEntangleBinding from items | — | mapped | 2491-2506 |
| 21 | `resonance_handler_symbol` | resonate | __kain_resonate_{name} symbol | — | mapped | 2508-2510 |
| 22 | `collect_native_resonances` | resonate | Collect NativeResonanceInfo from items | — | mapped | 2512-2526 |
| 23 | `native_resonance_binding` | resonate | Find resonance by target path | — | mapped | 2528-2533 |
| 24 | `collect_machine_stone_metadata` | axiom/pulse/shatter | Collect axioms/pulses/shattered structs | — | mapped | 2535-2567 |
| 25 | `collect_component_metadata` | component | Collect dimensions/pulses/resonates per component | — | mapped | 2571-2646 |
| 26 | `register_world_type_and_global` | world | %Emit %WorldName = type {fields}/@__kain_world_* zeroinitializer | LLVMStructCreateNamed LLVMAddGlobal | needs-review | 2648-2688 |
| 27 | `record_actor_state_initializers` | actor | Store state initializer expressions | — | mapped | 2690-2697 |
| 28 | `llvm_constant_initializer_for_expr` | const | Extract constant LLVM initializer from expr | LLVMConstInt/ConstReal | needs-review | 2699-2712 |
| 29 | `register_const_global` | const | @__kain_const_{name} global + optional thread_local | LLVMAddGlobal | needs-review | 2714-2785 |
| 30 | `register_python_import_global` | import (Python) | @__kain_py_import_{name} global i64 + init flag | LLVMAddGlobal | needs-review | 2787-2819 |
| 31 | `register_python_import_globals` | import (Python) | Register all Python import globals recursively | — | mapped | 2821-2826 |
| 32 | `register_python_import_globals_recursive` | import (Python) | Recursive walk to register Python imports | — | mapped | 2828-2853 |
| 33 | `register_const_globals` | const | Register all const globals recursively | — | mapped | 2855-2859 |
| 34 | `register_const_globals_recursive` | const | Recursive walk to register const globals | — | mapped | 2861-2876 |
| 35 | `compile_const_initializer` | const | define void @__kain_init_const_{name}() with init guard | LLVMBuildStore | needs-review | 2878-2949 |
| 36 | `emit_const_init_call_if_needed` | const | Emit call to const init if not yet hoisted | LLVMBuildCall | needs-review | 2951-2976 |
| 37 | `compile_const_load` | const | Load global with lazy init call | LLVMBuildLoad | needs-review | 2978-2986 |
| 38 | `compile_python_import_context_ptr` | import (Python) | Compile importer source file C string | — | mapped | 2988-2992 |
| 39 | `compile_python_import_runtime_call` | import (Python) | call @py_import_with_context / @py_import_from_with_context | LLVMBuildCall | needs-review | 2994-3035 |
| 40 | `compile_python_import_initializer` | import (Python) | define void @__kain_init_py_import_{name}() | LLVMBuildCall | needs-review | 3037-3116 |
| 41 | `emit_python_import_init_call_if_needed` | import (Python) | Emit lazy init call for Python import | LLVMBuildCall | needs-review | 3118-3141 |
| 42 | `compile_python_import_load` | import (Python) | Load i64 from Python import global with lazy init | LLVMBuildLoad | needs-review | 3143-3151 |
| 43 | `compile_module` | all (top-level) | Full module compilation pipeline (header→externs→items→strings→dtors→debug) | LLVMModuleCreateWithNameInContext | needs-review | 3153-3279 |
| 44 | `emit_debug_metadata_footer` | debug/DWARF | !llvm.dbg.cu / !DICompileUnit / !DIFile / !DILocation | LLVMAddNamedMetadataOperand LLVMSetCurrentDebugLocation | needs-review | 3284-3309 |
| 45 | `emit_crash_table` | debug/forensics | @__kain_crash_table = global [N x %KainCrashEntry] | LLVMAddGlobal | needs-review | 3313-3378 |

---

## Chunk 07: Semantics (`chunk-07-semantics.tsv`)

Source: `mod.rs`, semantic construct compilation  
Rows: ~29

| # | function_name | kain_concept | llvm_ir_pattern | llvm_c_api | status | lines |
|---|--------------|-------------|-----------------|------------|--------|-------|
| 1 | `compile_actor` | actor | define i32 @Actor_turn(): actor mailbox switch dispatch + handler bodies | LLVMBuildCall/BuildSwitch | needs-review | 864-1149 |
| 2 | `emit_runtime_abi_types` | runtime/types | %KainActorRef/%KainActorMessage/%KainReplyPort/%KainActorSpawnConfig/%KainCrashEntry | LLVMStructCreateNamed | needs-review | 1151-1163 |
| 3 | `emit_runtime` | runtime | Placeholder comment only | — | mapped | 1165-1167 |
| 4 | `emit_externs` | runtime/FFI | declare void/1648 @print*/@str_*/@kain_*/@py_*/@abi_*/@__kain_* | — | mapped | 1169-1395 |
| 5 | `emit_stdlib_externs` | stdlib | declare for each stdlib function not in skiplist | — | mapped | 1397-1423 |
| 6 | `emit_struct_destructors` | ownership/RC | define void @dtor_StructName(i8*): field RC release loop | — | needs-review | 1425-1478 |
| 7 | `compile_component` | component | Delegates to compile_component_render | — | mapped | 1480-1482 |
| 8 | `compile_impl` | impl | Dispatch to compile_impl_method per method | — | mapped | 1484-1495 |
| 9 | `compile_impl_method` | impl/method | define void @Target_method(): entry alloca self store + arg stores + block compile | LLVMBuildCall | needs-review | 1497-1644 |
| 10 | `find_shader_by_name` | shader | Recursive search for TypedShader by name | — | mapped | 1646-1659 |
| 11 | `collect_shader_spirv_hexes` | shader/SPIR-V | Compile fragment shader to SPIR-V hex via gpu::compile_fragment_to_spirv_hex | — | needs-review | 1661-1686 |
| 12 | `emit_shader_spirv_globals` | shader/SPIR-V | @__kain_spirv_{name} = private unnamed_addr constant [...] | LLVMAddGlobal | needs-review | 1688-1715 |
| 13 | `compile_entangle_registration_function` | entangle | define void @__kain_register_entanglements(): call @abi_entangle_register per binding | LLVMBuildCall | needs-review | 1717-1751 |
| 14 | `should_force_inline_callable` | fn/inline | Check body size and loop absence for alwaysinline | — | mapped | 1753-1762 |
| 15 | `compile_named_callable` | fn (generic) | define void @name(): entry allocas + params + block compile + debug metadata + crash table | LLVMBuildCall/BuildAlloca | needs-review | 1764-2084 |
| 16 | `compile_patch` | patch | Wrap compile_named_callable with abi_patch_begin/commit | LLVMBuildCall @abi_patch_*/abi_patch_commit | needs-review | 2086-2099 |
| 17 | `resonance_context_params` | resonate | Build Param list: resonate_old_i64, resonate_new_i64, resonate_fired | — | mapped | 2101-2135 |
| 18 | `compile_resonate` | resonate | compile_named_callable for __kain_resonate_{name}(old, new, fired) | LLVMBuildCall | needs-review | 2137-2158 |
| 19 | `compile_law` | law | compile_named_callable for law(params) -> Bool | LLVMBuildCall | needs-review | 2160-2170 |
| 20 | `compile_axiom` | axiom | define i64 @__kain_axiom_{name}(): call @kain_machine_axiom_accept with target/arch/capability | LLVMBuildCall | needs-review | 2172-2230 |
| 21 | `compile_pulse` | pulse | define void @body(tick, dt, missed); define void @fire() with pulse_snapshot + pulse_start | LLVMBuildCall @kain_machine_pulse_* | needs-review | 2232-2364 |
| 22 | `compile_converge` | converge | spec+fast lanes as separate callables; switch dispatch with abi_converge_select_lane_for_key + cached lane global | LLVMBuildSwitch/BuildCall | needs-review | 2366-2676 |
| 23 | `compile_world_initializer` | world | define void @__kain_init_world_{name}(): lazy init guard + field initializers + surface frame loop | LLVMBuildStore/BuildCall | needs-review | 2678-2787 |
| 24 | `compile_orchestrate` | orchestrate | compile_named_callable with orchestrate barrier JSON metadata | LLVMBuildCall | needs-review | 2789-2807 |
| 25 | `emit_machine_stones_entry_preamble` | axiom/pulse | Emit axiom accept calls + pulse start calls in main preamble | LLVMBuildCall @kain_machine_*/@kain_machine_pulse_start | needs-review | 2809-2832 |
| 26 | `compile_shader` | shader | compile_named_callable for shader body as function | LLVMBuildCall | needs-review | 2834-2849 |
| 27 | `compile_function` | fn | Dispatch extern → compile_extern_function or compile_named_callable | LLVMBuildCall | needs-review | 2851-2864 |
| 28 | `compile_extern_function` | fn/extern | declare void @name(params) with callconv/link_name | — | mapped | 2866-2922 |

---

## Chunk 08: Statements (`chunk-08-stmts.tsv`)

Source: `mod.rs`, statement compilation  
Rows: ~25

| # | function_name | kain_concept | llvm_ir_pattern | llvm_c_api | status | lines |
|---|--------------|-------------|-----------------|------------|--------|-------|
| 1 | `compile_block` | fn/scope | Emit scope push/pop, defer stack, ephemeral candidate analysis, stmt loop, scope cleanup | LLVMBasicBlockAppend | needs-review | 858-916 |
| 2 | `compile_block_with_result` | fn/scope | Same as compile_block but returns last expr value with retain/release for block result | LLVMBasicBlockAppend | needs-review | 918-1030 |
| 3 | `emit_heap_owned_i8_guard` | ownership/RC | alloca i1 + store 0/1 for heap-owned i8* guard | — | needs-review | 1032-1056 |
| 4 | `emit_rc_retain_if_heap_i8` | ownership/RC | Check i8* != null, call @rc_retain | LLVMBuildCall | needs-review | 1058-1072 |
| 5 | `emit_rc_release_if_heap_i8` | ownership/RC | Check i8* != null, call @rc_release | LLVMBuildCall | needs-review | 1074-1088 |
| 6 | `emit_heap_owned_retain_for_transfer` | ownership/RC | Set guard to 1 for heap-owned i8* transfer | — | mapped | 1090-1111 |
| 7 | `emit_release` | ownership/RC | Dispatch release: i8*→rc_release, %struct*→call @dtor_*, JSON→json_release | LLVMBuildCall | needs-review | 1113-1132 |
| 8 | `emit_release_if_new_object_expr` | ownership/RC | Check if expr is new object to skip release | — | mapped | 1134-1138 |
| 9 | `emit_scope_cleanup_for_vars` | ownership/scope | Emit release for all vars in current scope | — | needs-review | 1140-1143 |
| 10 | `emit_scope_cleanup_for_vars_except` | ownership/scope | Emit release for scope vars except transfer set | — | needs-review | 1145-1190 |
| 11 | `emit_scope_exit` | ownership/scope | Pop scope, emit defers, cleanup | — | mapped | 1192-1195 |
| 12 | `emit_current_scope_defers` | defer | Emit all defers from current defer scope | — | needs-review | 1197-1202 |
| 13 | `emit_all_defer_cleanups` | defer | Emit all defer cleanups across all scopes | — | mapped | 1204-1206 |
| 14 | `emit_defer_cleanups_from_depth` | defer | Emit defers from depth N to end | — | needs-review | 1208-1214 |
| 15 | `emit_defer_exprs` | defer | Emit individual defer expressions | — | needs-review | 1216-1224 |
| 16 | `emit_scope_exit_except` | ownership/scope | Scope exit with exception set for transfer | — | mapped | 1226-1230 |
| 17 | `emit_all_scopes_cleanup` | ownership/scope | Emit cleanup across all scopes | — | mapped | 1232-1235 |
| 18 | `emit_all_scopes_cleanup_except` | ownership/scope | Emit cleanup across all scopes except transfer | — | needs-review | 1237-1283 |
| 19 | `collect_helper_owned_pointer_transfer_locals` | ownership | Collect helper-owned pointer locals at scope boundary | — | mapped | 1285-1329 |
| 20 | `helper_owned_pointer_transfer_locals_for_expr` | ownership | Check which helper-owned locals an expr touches | — | mapped | 1331-1335 |
| 21 | `match_fallback_value_for_type` | match | Generate zero value fallback for match type | — | mapped | 1337-1349 |
| 22 | `is_new_object` | fn/ownership | Detect if expr produces new RC=1 object | — | mapped | 1351-1365 |
| 23 | `compile_if_statement` | if/elif/else | Emit br/icmp/phi for if/elif/else chains | LLVMBuildCondBr/BuildPhi | needs-review | 1367-1412 |
| 24 | `compile_stmt` | all statements | ~850-line dispatcher: let, assignment, while, for, loop, return, break, continue, defers | LLVMBuildAlloca/BuildStore/etc | needs-review | 1414-2213 |

---

## Chunk 09: Expressions (`chunk-09-exprs.tsv`)

Source: `mod.rs`, expression compilation + tests  
Rows: ~39

| # | function_name | kain_concept | llvm_ir_pattern | llvm_c_api | status | lines |
|---|--------------|-------------|-----------------|------------|--------|-------|
| 1 | `compile_numeric_floor_builtin` | std::math/floor | call @llvm.floor.f64 + fptosi.sat.i64 | LLVMBuildCall @llvm.floor.f64 | needs-review | 865-886 |
| 2 | `compile_numeric_abs_builtin` | std::math/abs | fcmp oge + fsub + select (double); icmp sge + sub + select (integer) | LLVMBuildFCmp/BuildICmp/BuildSelect | needs-review | 888-931 |
| 3 | `compile_numeric_min_or_max_builtin` | std::math/min/max | fcmp ole/oge + select (double); icmp sle/sge + select (integer) | LLVMBuildFCmp/BuildICmp/BuildSelect | needs-review | 933-975 |
| 4 | `compile_numeric_clamp_builtin` | std::math/clamp | Chain min+max with value coercion | LLVMBuildFCmp/BuildICmp/BuildSelect | needs-review | 977-1008 |
| 5 | `compile_numeric_min_or_max_builtin_from_values` | std::math/min/max | fcmp/icmp + select from pre-compiled values | LLVMBuildFCmp/BuildICmp/BuildSelect | needs-review | 1010-1051 |
| 6 | `compile_direct_call` | fn/call | call @name(args) with coercion, tagged ptr untagging, struct return, extern ABI | LLVMBuildCall | needs-review | 1053-1509 |
| 7 | `compile_stage_call` | orchestrate | Orchestrate stage call wrapper | LLVMBuildCall | needs-review | 1511-1567 |
| 8 | `compile_expr_as_string_value` | fn/string | Compile expr to string value | — | needs-review | 1569-1574 |
| 9 | `compile_stdout_write_call` | IO/print | call @print_str(i8*, i64) | LLVMBuildCall | needs-review | 1576-1608 |
| 10 | `compile_assert_call` | test/assert | call @runtime_assert with formatted message | LLVMBuildCall | needs-review | 1610-1655 |
| 11 | `compile_macro_call` | macro | Inline macro expansion + compile | LLVMBuildCall | needs-review | 1657-1736 |
| 12 | `compile_expr` | all expressions | ~2400-line dispatcher for 50+ expression kinds: literals, ident, binary, unary, call, field, index, if, match, struct, array, tuple, lambda, block, as, world field, teleport, collapse, observe, share, etc. | LLVMBuildAdd/BuildSub/BuildLoad/BuildStore/etc | needs-review | 1738-4158 |
| 13 | `generate_llvm_or_error` | test helper | Wrap generate for tests | — | mapped | 4169-4171 |
| 14 | `generate_llvm_or_error_with_filename` | test helper | Wrap generate_with_debug for tests | — | mapped | 4173-4188 |
| 15 | `repo_test_path` | test helper | Resolve test Kain source path | — | mapped | 4190-4196 |
| 16 | `lowers_immutable_scalar_lets_as_ssa_values` | test/let | SSA value test for immutable scalar lets | — | mapped | 4199-4228 |
| 17 | `remaps_rounding_builtins_to_runtime_wrappers` | test/stdlib | Floor/ceil/round remapping verification | — | mapped | 4231-4242 |
| 18 | `lowers_extern_cffi_declarations_without_void_parameters` | test/extern C FFI | Extern C declaration lowering test | — | mapped | 4245-4267 |
| 19 | `lowers_extern_cffi_declarations_inside_generated_modules` | test/extern C FFI | Extern in modules test | — | mapped | 4270-4296 |
| 20 | `lowers_impl_self_builder_methods_without_extra_self_parameter` | test/impl | Impl self builder method lowering test | — | mapped | 4299-4328 |
| 21 | `lowers_impl_self_local_copy_without_pointer_bit_corruption` | test/impl | Impl self local copy test | — | mapped | 4331-4374 |
| 22 | `retains_borrowed_string_arguments_before_non_extern_calls` | test/String/RC | Borrowed string arg retain test | — | mapped | 4377-4404 |
| 23 | `lowers_unit_enum_equality_to_tag_comparison` | test/enum | Unit enum tag comparison test | — | mapped | 4407-4426 |
| 24 | `stdlib_string_predicates_materialize_owned_literals_before_runtime_calls` | test/stdlib/String | String predicate materialization test | — | mapped | 4429-4472 |
| 25 | `lowers_len_of_runtime_string_result_to_rc_len_not_array_len` | test/String/RC | Length-of-string test | — | mapped | 4475-4500 |
| 26 | `materializes_c_string_extern_returns_into_owned_kain_strings` | test/C String | C string return materialization test | — | mapped | 4503-4521 |
| 27 | `retains_string_field_assignment_before_releasing_previous_field` | test/String/RC | String field assignment release ordering test | — | mapped | 4524-4553 |
| 28 | `retains_shared_string_when_storing_in_runtime_array` | test/String/RC | String retain in runtime array test | — | mapped | 4556-4583 |
| 29 | `transfers_helper_owned_pointers_through_value_struct_returns` | test/ownership | Helper-owned pointer struct transfer test | — | mapped | 4586-4610 |
| 30 | `transfers_helper_owned_pointers_stored_as_raw_bits` | test/ownership | Helper-owned pointer raw bits test | — | mapped | 4613-4643 |
| 31 | `authored_stdlib_signature_overrides_catalog_any_stub` | test/stdlib | Stdlib signature override test | — | mapped | 4646-4676 |
| 32 | `rejects_invalid_atomic_store_ordering_for_llvm` | test/atomic | Atomic store ordering validation test | — | mapped | 4679-4690 |
| 33 | `rejects_invalid_compare_exchange_failure_shape_for_llvm` | test/atomic | Compare-exchange shape validation test | — | mapped | 4693-4702 |
| 34 | `rejects_compare_exchange_failure_ordering_stronger_than_success_for_llvm` | test/atomic | CAS ordering validation test | — | mapped | 4705-4716 |
| 35 | `debug_metadata_emits_dbg_and_compile_unit` | test/debug/DWARF | Debug metadata emission test | — | mapped | 4719-4753 |
| 36 | `no_debug_metadata_without_flag` | test/debug/DWARF | No debug metadata without -g test | — | mapped | 4756-4771 |
| 37 | `debug_metadata_emits_per_statement_line_numbers` | test/debug/DWARF | Per-statement debug line test | — | mapped | 4774-4810 |
| 38 | `generate_with_debug` | test/debug/DWARF | Debug IR generation integration test | — | mapped | 4812-4825 |

---

## Chunk 10: Component Calls (`chunk-10-component_calls.tsv`)

Source: `component.rs`, full component surface codegen  
Rows: ~229

### Type Declarations (6 entries)

| # | section | function_name | llvm_call | kaintana_target | description |
|---|---------|--------------|-----------|-----------------|-------------|
| 1 | TYPE_DECLARATION | `declare_surface_trait_types` | `%KainComponentSurface = type { i8*, i8*, i8*, i8*, ... 24 total i8* }` | KainComponentSurface struct | Declares the opaque vtable struct type with 24 i8* slots; all function pointers are stored as uniform i8* placeholders until bitcast at call site. |
| 2 | TYPE_DECLARATION | `declare_surface_trait_types` | `%KainGpuSurfaceExtension = type { i8*, i8* }` | KainGpuSurfaceExtension | Declares the GPU extension struct with two slots: load_shader (offset 0) and set_uniform (offset 1). |
| 3 | TYPE_DECLARATION | `declare_surface_trait_types` | `declare %KainComponentSurface* @kain_component_surface_resolve(i8*)` | kain_component_surface_resolve() | Declares the surface registry resolver: takes a name string, returns a surface pointer or NULL. |
| 4 | TYPE_DECLARATION | `declare_surface_trait_types` | `declare void @kain_runtime_panic(i8*)` | kain_runtime_panic() | Declares the fatal-error handler for surface resolution/session failures. |
| 5 | TYPE_DECLARATION | `declare_surface_trait_types` | `declare double @__kain_frame_delta_ms()` | __kain_frame_delta_ms() | Declares the high-resolution frame timer used by begin_frame. Returns double (f64) in milliseconds. |
| 6 | TYPE_DECLARATION | `declare_surface_trait_types` | `%KainComponentCallback = type void (i64, i64, i8*)*` | Generic callback fn ptr | Declares the callback function pointer type: takes session_id, element_id, event_data. |

### Vtable Constants (24 entries)

| # | section | function_name | llvm_call | description |
|---|---------|--------------|-----------|-------------|
| 1 | VTABLE_CONSTANT | (global) | `OFF_SESSION_CREATE = 0` | Slot 0: session_create |
| 2 | VTABLE_CONSTANT | (global) | `OFF_SESSION_DESTROY = 1` | Slot 1: session_destroy |
| 3 | VTABLE_CONSTANT | (global) | `OFF_ELEMENT_BEGIN = 2` | Slot 2: element_begin |
| 4 | VTABLE_CONSTANT | (global) | `OFF_ELEMENT_END = 3` | Slot 3: element_end |
| 5 | VTABLE_CONSTANT | (global) | `OFF_ELEMENT_SET_TEXT = 4` | Slot 4: element_set_text |
| 6 | VTABLE_CONSTANT | (global) | `OFF_ELEMENT_SET_ATTR_I64 = 5` | Slot 5: element_set_attr_i64 |
| 7 | VTABLE_CONSTANT | (global) | `OFF_ELEMENT_SET_ATTR_F64 = 6` | Slot 6: element_set_attr_f64 |
| 8 | VTABLE_CONSTANT | (global) | `OFF_ELEMENT_SET_ATTR_STRING = 7` | Slot 7: element_set_attr_string |
| 9 | VTABLE_CONSTANT | (global) | `OFF_STATE_GET_I64 = 8` | Slot 8: state_get_i64. Sentinel: -1 = first frame. |
| 10 | VTABLE_CONSTANT | (global) | `OFF_STATE_SET_I64 = 9` | Slot 9: state_set_i64 |
| 11 | VTABLE_CONSTANT | (global) | `OFF_BEGIN_FRAME = 10` | Slot 10: begin_frame |
| 12 | VTABLE_CONSTANT | (global) | `OFF_END_FRAME = 11` | Slot 11: end_frame |
| 13 | VTABLE_CONSTANT | (global) | `OFF_PRESENT = 12` | Slot 12: present |
| 14 | VTABLE_CONSTANT | (global) | `OFF_POLL_EVENT = 13` | Slot 13: poll_event |
| 15 | VTABLE_CONSTANT | (global) | `OFF_SHOULD_CLOSE = 14` | Slot 14: should_close |
| 16 | VTABLE_CONSTANT | (global) | `OFF_WINDOW_OPEN = 15` | Slot 15: window_open |
| 17 | VTABLE_CONSTANT | (global) | `OFF_HOST_PUMP = 16` | Slot 16: host_pump |
| 18 | VTABLE_CONSTANT | (global) | `OFF_SESSION_ATTACH_PLATFORM = 17` | Slot 17: session_attach_platform |
| 19 | VTABLE_CONSTANT | (global) | `OFF_GET_GPU_EXTENSION = 18` | Slot 18: get_gpu_extension |
| 20 | VTABLE_CONSTANT | (global) | `OFF_STATE_GET_F64 = 19` | Slot 19: state_get_f64. Sentinel: NaN. |
| 21 | VTABLE_CONSTANT | (global) | `OFF_STATE_SET_F64 = 20` | Slot 20: state_set_f64 |
| 22 | VTABLE_CONSTANT | (global) | `OFF_STATE_GET_STRING = 21` | Slot 21: state_get_string. Sentinel: null. |
| 23 | VTABLE_CONSTANT | (global) | `OFF_STATE_SET_STRING = 22` | Slot 22: state_set_string |
| 24 | VTABLE_CONSTANT | (global) | `OFF_ELEMENT_SET_CALLBACK = 23` | Slot 23: element_set_callback |

### Attribute Sets (46 entries total — f64, string, i64, text, bool, expr)

**f64 attributes (13):**
| # | attr_name | vtable slot | style_key |
|---|-----------|-------------|-----------|
| 1 | padding | slot 6 (f64) | "padding" |
| 2 | spacing | slot 6 (f64) | "spacing" |
| 3 | corner_radius | slot 6 (f64) | "corner_radius" |
| 4 | radius | slot 6 (f64) | "radius" (alias for corner_radius) |
| 5 | font_size | slot 6 (f64) | "font_size" |
| 6 | opacity | slot 6 (f64) | "opacity" |
| 7 | border / border_width | slot 6 (f64) | "border_width" |
| 8 | stroke_width | slot 6 (f64) | "border_width" (alias) |
| 9 | width | slot 6 (f64) | "width" |
| 10 | height | slot 6 (f64) | "height" |
| 11 | min | slot 6 (f64) | "min" |
| 12 | max | slot 6 (f64) | "max" |
| 13 | step | slot 6 (f64) | "step" |

**string attributes (14):**
| # | attr_name | vtable slot | style_key |
|---|-----------|-------------|-----------|
| 14 | background | slot 7 (string) | "fill_color" |
| 15 | fill | slot 7 (string) | "fill_color" |
| 16 | border_color | slot 7 (string) | "border_color" |
| 17 | stroke | slot 7 (string) | "border_color" |
| 18 | color / ink_color | slot 7 (string) | "ink_color" |
| 19 | title | slot 7 (string) | "title" |
| 20 | variant | slot 7 (string) | "variant" |
| 21 | role | slot 7 (string) | "role" |
| 22 | align | slot 7 (string) | "align" |
| 23 | font_family | slot 7 (string) | "font_family" |
| 24 | distribution | slot 7 (string) | "layout.distribution" |
| 25 | axis | slot 7 (string) | "axis" |
| 26 | placeholder | slot 7 (string) | "placeholder" |
| 27 | tooltip | slot 7 (string) | "tooltip" |

**i64 attributes (6):**
| # | attr_name | vtable slot | style_key |
|---|-----------|-------------|-----------|
| 28 | direction | slot 5 (i64) | "layout.direction" |
| 29 | disabled | slot 5 (i64) | "disabled" |
| 30 | checked | slot 5 (i64) | "checked" |
| 31 | selected | slot 5 (i64) | "selected" |
| 32 | tab_index | slot 5 (i64) | "tab_index" |
| 33 | weight | slot 5 (i64) | "weight" |

**text attribute (1):**
| # | attr_name | vtable slot | notes |
|---|-----------|-------------|-------|
| 34 | value | slot 4 (text) | Bypasses key-based attr, goes directly to element_set_text |

**fallback (1):**
| # | condition | vtable slot | notes |
|---|-----------|-------------|-------|
| 35 | unknown attrs | slot 7 (string) | Raw attr name as style_key |

**Additional attribute variants in chunk-10:**
| # | variant | description |
|---|---------|-------------|
| 36 | String value → text | When attr name is "value" with a string value, emits element_set_text directly |
| 37 | String value → i64 attr | Converts known keywords ("vertical"/"column"→1, "horizontal"/"row"→0); unknown→0 |
| 38 | String value → string attr | Default path for string-valued attributes |
| 39 | Bool true → text | Emits text "true" when attr name is "value" |
| 40 | Bool true → f64 attr | Emits "1.0" as double |
| 41 | Bool true → i64/string attr | Emits "1" as i64 or "1" as string |
| 42 | Bool false | No-op (attribute not set) |
| 43 | Expr → text | Evaluates expression and emits as text |
| 44 | Expr → typed attr | Evaluates, coerces to type, emits attr via vtable |
| 45 | Coerce: i1 → i64 | zext i1 %val to i64 |
| 46 | Coerce: i64 → f64 | sitofp i64 %val to double |

### State Access (27 entries)

| # | function_name | llvm_call | description |
|---|--------------|-----------|-------------|
| 1 | `compile_component_state_init` | state_get_f64 via emit_vtable_call (slot 19) | Reads f64 state; sentinel NaN = first frame |
| 2 | `compile_component_state_init` | state_set_f64 via emit_vtable_call_void (slot 20) | Writes f64 initial/updated state |
| 3 | `compile_component_state_init` | state_get_string via emit_vtable_call (slot 21) | Reads string state; sentinel null = first frame |
| 4 | `compile_component_state_init` | state_set_string via emit_vtable_call_void (slot 22) | Writes string state |
| 5 | `compile_component_state_init` | state_get_i64 via emit_vtable_call (slot 8) | Reads i64 state; sentinel -1 = first frame |
| 6 | `compile_component_state_init` | state_set_i64 via emit_vtable_call_void (slot 9) | Writes i64 state |
| 7 | `compile_component_state_init` | alloca (for f64 state addr) | Stack allocation |
| 8 | `compile_component_state_init` | store double %val, double* %addr | Stack write |
| 9 | `compile_component_state_init` | alloca (for i8* string state addr) | Stack allocation |
| 10 | `compile_component_state_init` | store i8* %val, i8** %addr | Stack write |
| 11 | `compile_component_state_init` | alloca (for i64 state addr) | Stack allocation |
| 12 | `compile_component_state_init` | store i64 %val, i64* %addr | Stack write |
| 13 | `compile_component_state_init` | fcmp uno double %v, 0x7FF8000000000000 | NaN sentinel check |
| 14 | `compile_component_state_init` | icmp eq i8* %v, null | Null sentinel check |
| 15 | `compile_component_state_init` | icmp eq i64 %v, -1 | -1 sentinel check |
| 16 | `compile_component_state_init` | phi double [...] | PHI merge for f64 state |
| 17 | `compile_component_state_init` | phi i8* [...] | PHI merge for string state |
| 18 | `compile_component_state_init` | phi i64 [...] | PHI merge for i64 state |
| 19 | `compile_component_state_init` | br i1 %is_first, label %init_block, label %load_block | Branch for sentinel |
| 20 | `compile_component_state_init` | br label %load_block | Unconditional merge branch |
| 21 | `compile_component_state_init` | sitofp i64 %v to double | Type coercion (i64→f64) |
| 22 | `compile_component_render` (write-back) | load double, double* %addr | State read for write-back |
| 23 | `compile_component_render` (write-back) | state_set_f64 via emit_vtable_call_void | Persist f64 state |
| 24 | `compile_component_render` (write-back) | load i8*, i8** %addr | State read for write-back |
| 25 | `compile_component_render` (write-back) | state_set_string via emit_vtable_call_void | Persist string state |
| 26 | `compile_component_render` (write-back) | load i64, i64* %addr | State read for write-back |
| 27 | `compile_component_render` (write-back) | state_set_i64 via emit_vtable_call_void | Persist i64 state |

### Element Tree (13 entries)

| # | function_name | llvm_call | description |
|---|--------------|-----------|-------------|
| 1 | `emit_element_begin` | element_begin via emit_vtable_call (slot 2) | Creates element: session_id, parent_id, kind, stable_key |
| 2 | `emit_element_begin` | compile_static_c_string_literal(kind) | Emits kind string literal |
| 3 | `emit_element_end` | element_end via emit_vtable_call_void (slot 3) | Closes element |
| 4 | `compile_jsx_text` | element_begin + set_text + element_end | Creates "text" element |
| 5-6 | `compile_jsx_text` | element_set_text, element_end | Text content |
| 7-9 | `compile_jsx_expression` | element_begin("text") + set_text + end | Expression result as text |
| 10-11 | `compile_jsx_element` | element_begin(tag) + element_end | JSX element with attributes and children |
| 12 | `compile_jsx_for` | element_begin with child_parent = parent_reg + idx | For-loop iteration elements |
| 13 | `compile_component_render` | All element creation via compile_jsx_to_surface | Root component rendering |

### Frame Lifecycle (27 entries)

| # | step | llvm_call | description |
|---|------|-----------|-------------|
| 1 | Surface resolve | `call %KainComponentSurface* @kain_component_surface_resolve(i8* %name)` | Resolve named surface from registry |
| 2 | Null check | `icmp eq %KainComponentSurface* %surf, null` | Guard unregistered backends |
| 3 | Branch | `br i1 %is_null, label %null_block, label %init_block` | Error or init |
| 4 | Panic | `call void @kain_runtime_panic(i8* %msg)` + `unreachable` | Fatal error on null surface |
| 5 | Unreachable | `unreachable` | After panic |
| 6 | Session create | session_create via slot 0 | Create session with name, width, height |
| 7 | Error check | `icmp slt i64 %session_id, 0` | Negative = error |
| 8 | Branch | `br i1 %session_err, label %fail_label, label %window_init_label` | Error or window init |
| 9 | Session panic | `call void @kain_runtime_panic(i8* %msg)` | Session creation failure |
| 10 | Platform handle | `alloca [8 x i8], align 8` | Zero-initialized platform handle |
| 11 | Memset | `call void @llvm.memset.p0i8.i64(i8* %handle, i8 0, i64 8, i1 false)` | Zero-fill platform handle |
| 12 | Attach platform | session_attach_platform via slot 17 | Attach window handle to session |
| 13 | Window open | window_open via slot 15 | Flag session as open |
| 14 | Host pump | host_pump via slot 16 | Process OS message queue |
| 15 | Frame delta | `call double @__kain_frame_delta_ms()` | High-res frame timer |
| 16 | Begin frame | begin_frame via slot 10 | Start new frame |
| 17 | Render | `call void @ComponentName_render(%KainComponentSurface* %surf, i64 %session, i64 0)` | Root component render |
| 18 | End frame | end_frame via slot 11 | Signal end of frame |
| 19 | Present | present via slot 12 | Present rendered frame |
| 20 | Should close | should_close via slot 14 | Check close signal |
| 21 | Comparison | `icmp eq i64 %should_close, 0` | Invert to keep_going |
| 22 | Loop branch | `br i1 %keep_going, label %frame_loop, label %shutdown` | Loop or exit |
| 23 | Session destroy | session_destroy via slot 1 | Cleanup session |
| 24 | Return | `ret void` | Exit frame loop function |
| 25 | Jump to loop | `br label %frame_loop_label` | Unconditional loop entry |
| 26 | Pulse registration | `call i64 @kain_machine_pulse_start(...)` | Register component pulses before loop |
| 27 | Resonate registration | `call void @abi_resonate_register(...)` | Register component resonates before loop |

### Shader Surface (5 entries in this chunk, plus GPU path entries above)

| # | function_name | llvm_call | description |
|---|--------------|-----------|-------------|
| 1 | Surface resolve (GPU) | `call %KainComponentSurface* @kain_component_surface_resolve(i8* %surface_kind)` | Parameterized by surface_kind |
| 2 | Session create (GPU) | session_create via slot 0 | GPU session with configurable dimensions |
| 17 | emit_gpu_set_uniform | `getelementptr inbounds %KainGpuSurfaceExtension, %KainGpuSurfaceExtension* %ext, i32 0, i32 1` | GEP into GPU extension for set_uniform |
| 18 | emit_gpu_set_uniform | `bitcast i8** %gep to i64 (i64, i32, i8*, i64)**` | Cast to set_uniform function pointer type |
| 19 | emit_gpu_set_uniform | `load i64 (i64, i32, i8*, i64)*, i64 (i64, i32, i8*, i64)** %cast` | Load set_uniform fn ptr |
| 20 | emit_gpu_set_uniform | `bitcast %data_ty* %data to i8*` | Cast uniform data to i8* |
| 21 | emit_gpu_set_uniform | `call i64 %fn(i64 %session, i32 %binding, i8* %data, i64 %size)` | Set GPU uniform |

### LLVM Intrinsics (4 entries)

| # | function_name | llvm_call | description |
|---|--------------|-----------|-------------|
| 1 | compile_surface_frame_loop | `call void @llvm.memset.p0i8.i64(i8* %ptr, i8 0, i64 8, i1 false)` | Zero-init platform handle |
| 3 | compile_component_render | (implicit via emit_entry_alloca) `alloca` | Entry-hoisted stack allocations |
| 4 | compile_component_render | (implicit via emit) `store` | Store function parameters |
| 5 | compile_component_render | (implicit via emit) `ret void` | Return from render |

### Callback Bind (6 entries)

| # | function_name | llvm_call | description |
|---|--------------|-----------|-------------|
| 1 | compile_jsx_callback | compile_static_c_string_literal(event_kind) | Emit event kind string |
| 2 | compile_jsx_callback | compile_expr(fn_expr) | Compile handler to fn pointer |
| 3 | compile_jsx_callback | bitcast %handler_ty %handler_val to %KainComponentCallback | Cast to canonical callback type |
| 4 | compile_jsx_callback | bitcast %KainComponentCallback %cb to i8* | Cast to void* for vtable storage |
| 5 | compile_jsx_callback | element_set_callback via slot 23 | Register callback on element |
| 6 | compile_jsx_attr | Callback detection via matches!(JSXAttrValue::Callback(_, _)) | Route to compile_jsx_callback |

### Component Call (7 entries)

| # | function_name | llvm_call | description |
|---|--------------|-----------|-------------|
| 1 | compile_component_render | `define void @Name_render(%KainComponentSurface* %arg0, i64 %arg1, i64 %arg2, ...)` | Component render function definition |
| 2 | compile_component_render | `store %ty %argN, %ty* %prop.addr` | Store props into alloca slots |
| 3 | compile_jsx_component_call | `declare void @Name_render(%KainComponentSurface*, i64, i64, ...)` | Forward declaration |
| 4 | compile_jsx_component_call | `call void @Name_render(%KainComponentSurface* %surf, i64 %session, i64 %parent, props...)` | Child component render |
| 5 | compile_jsx_component_call | (prop compilation) compile_string_literal / compile_expr / zero_value_for_ty | Prop argument evaluation |
| 6 | compile_jsx_component_call | zero_value_for_ty(_prop_ty) | Default value for missing props |
| 7 | compile_jsx_component_call | compile_jsx_to_surface for children under parent_reg | JSX tree walk after render |

### Expression Evaluation (9 entries)

| # | function_name | llvm_call | description |
|---|--------------|-----------|-------------|
| 1 | compile_jsx_expression | compile_expr(expr) | Evaluate arbitrary Kain expression |
| 2 | compile_jsx_expression | stringify_value(val, ty) | Convert non-string to i8* |
| 3 | compile_jsx_expression | element_set_text via slot 4 (direct i8* path) | Set text for string results |
| 4 | try_inline_component_method | compile_expr calls for method arguments | Evaluate method args |
| 5 | try_inline_component_method | store %ty %val, %ty* %param.addr | Store args into param allocas |
| 6 | try_inline_component_method | compile_block_with_result(&method.body) | Compile method body inline |
| 7 | try_inline_component_method | scopes.push/pop for parameter scope | Scope management |
| 8 | try_inline_component_method | Component method call detection (Expr::Call / Expr::MethodCall) | AST pattern matching |
| 9 | try_inline_component_method | Arg count validation | Compile-time check |

### Flow Control (21 entries)

| # | function_name | llvm_call | description |
|---|--------------|-----------|-------------|
| 1 | compile_jsx_if | compile_expr(condition) | Evaluate condition |
| 2 | compile_jsx_if | icmp ne i64 %val, 0 | Coerce to i1 |
| 3 | compile_jsx_if | br i1 %cond, label %then_block, label %else_block | Conditional branch |
| 4 | compile_jsx_if | br label %done_block | Unconditional merge branch |
| 5 | compile_jsx_if | compile_jsx_to_surface for then/else bodies | Branch body rendering |
| 6 | compile_jsx_for | compile_expr(iter) | Evaluate iterable |
| 7 | compile_jsx_for | call i64 @runtime_array_len(i8* %iter) | Get array length |
| 8 | compile_jsx_for | alloca i64 (index pointer) | Loop index stack allocation |
| 9 | compile_jsx_for | store i64 0, i64* %idx_ptr | Init index to 0 |
| 10 | compile_jsx_for | br label %loop_header | Enter loop header |
| 11 | compile_jsx_for | load i64, i64* %idx_ptr | Load current index |
| 12 | compile_jsx_for | icmp sge i64 %idx, %len | Bounds check |
| 13 | compile_jsx_for | br i1 %done, label %loop_done, label %loop_body | Loop condition branch |
| 14 | compile_jsx_for | call i8* @runtime_array_get(i8* %iter, i64 %idx) | Get item at index |
| 15 | compile_jsx_for | alloca i8* (item address) | Item stack slot |
| 16 | compile_jsx_for | store i8* %item, i8** %item_addr | Store current item |
| 17 | compile_jsx_for | add i64 %parent_reg, %idx | child_parent for stable key |
| 18 | compile_jsx_for | compile_jsx_to_surface for body under child_parent | Render for body |
| 19 | compile_jsx_for | add i64 %idx, 1 | Increment index |
| 20 | compile_jsx_for | store i64 %next_idx, i64* %idx_ptr | Store back |
| 21 | compile_jsx_for | br label %loop_header | Loop back |

### Stable Keys (8 entries)

| # | function_name | llvm_call | description |
|---|--------------|-----------|-------------|
| 1 | emit_stable_key | compile_static_c_string_literal(path_prefix) | "ComponentName:tag" |
| 2 | emit_stable_key | compile_static_c_string_literal(":") | Colon separator |
| 3 | emit_stable_key | call i8* @str_concat(i8* %prefix, i8* %colon) | "ComponentName:tag:" |
| 4 | emit_stable_key | call i8* @to_string(i64 %parent_id) | Parent ID → string |
| 5 | emit_stable_key | call i8* @str_concat(i8* %step1, i8* %parent_str) | "ComponentName:tag:42" |
| 6 | emit_stable_key | compile_static_c_string_literal(":sibling_index") | ":0", ":1", etc. |
| 7 | emit_stable_key | call i8* @str_concat(i8* %step2, i8* %si_str) | Final key |
| 8 | emit_stable_key | (return) | "ComponentName:tag:parent_id:sibling_index" |

### Vtable Call Pattern (6 entries)

| # | function_name | llvm_call | description |
|---|--------------|-----------|-------------|
| 1 | emit_vtable_call | `getelementptr inbounds %KainComponentSurface, %KainComponentSurface* %surf, i32 0, i32 %offset` | GEP into vtable slot |
| 2 | emit_vtable_call | `bitcast i8** %gep to %fn_ptr_ptr_ty` | Cast slot to fn-ptr-ptr type |
| 3 | emit_vtable_call | `load %fn_ptr_ty, %fn_ptr_ptr_ty* %cast` | Load function pointer |
| 4 | emit_vtable_call | `call %ret_ty %fn_ptr(%args)` | Indirect call |
| 5 | emit_vtable_call | (ret_ty extraction) fn_ptr_ty.split('(').next() | Parse return type |
| 6 | emit_vtable_call_void | emit_vtable_call(surface_reg, offset, fn_ptr_ty, args) | Convenience wrapper (void return) |

### Pulse / Resonate Registration (10 entries)

| # | function_name | llvm_call | description |
|---|--------------|-----------|-------------|
| 1 | compile_component_render | `define internal void @__kain_pulse_fire_{name}() { ret void }` | Pulse handler stub |
| 2 | compile_component_render | `define internal void @__kain_resonate_{name}() { ret void }` | Resonate handler stub |
| 3 | emit_component_pulse_resonate_registration | state_get_i64 via slot 8 | Check "{name}:__pulses_init" sentinel |
| 4 | emit_component_pulse_resonate_registration | icmp eq i64 %init_flag, -1 | Sentinel check (-1 = first render) |
| 5 | emit_component_pulse_resonate_registration | br i1 %needs_init, label %register, label %skip | Skip on subsequent frames |
| 6 | emit_component_pulse_resonate_registration | state_set_i64 via slot 9 (value "1") | Mark as registered |
| 7 | emit_component_pulse_resonate_registration | ptrtoint i8* %token_str to i64 | Derive token from string pointer |
| 8 | emit_component_pulse_resonate_registration | call i64 @kain_machine_pulse_start(i64 %token, i64 16_000_000, i64 0, void ()* @handler) | Register pulse (16ms default) |
| 9 | emit_component_pulse_resonate_registration | call void @abi_resonate_register(i8* %target, i64 16_000_000, void ()* @handler) | Register resonate (16ms default) |
| 10 | emit_component_pulse_resonate_registration | br label %skip_block | Merge paths |

### Setup (7 entries)

| # | function_name | llvm_call | description |
|---|--------------|-----------|-------------|
| 1 | compile_component_render | declare_surface_trait_types() call | Ensure surface types declared |
| 2 | compile_component_render | state clear: reg_count, locals, ssa_locals, scopes, etc. | Reset generator state |
| 3 | compile_component_render | emit_label("entry") | Create entry basic block |
| 4 | compile_component_render | current_component_{name,methods,session,parent} set | Set component context |
| 5 | compile_surface_frame_loop | declare_surface_trait_types() call | Ensure types declared |
| 6 | compile_component_render | surface/session/parent assignment from %arg0/%arg1/%arg2 | Bind param registers |
| 7 | compile_jsx_component_call | component_defs lookup | Lookup prop definitions |

---

## Appendix: ABI Constants

From `mod.rs`:

```rust
const ABI_TAGGED_HEADER_BYTES: i64 = 16;
const ABI_TAG_OPTION_NONE_LLVM: i64 = 0;
const ABI_TAG_OPTION_SOME_LLVM: i64 = 1;
const ABI_TAG_RESULT_OK_LLVM: i64 = 2;
const ABI_TAG_RESULT_ERR_LLVM: i64 = 3;
const ABI_TAGGED_IMMEDIATE_MASK_LLVM: i64 = 7;
const ABI_TAGGED_IMMEDIATE_INT_MIN_LLVM: i64 = -(1i64 << 60);
const ABI_TAGGED_IMMEDIATE_INT_MAX_LLVM: i64 = (1i64 << 60) - 1;
const JSON_ANY_TAG_INT_LLVM: i64 = 1;
const JSON_ANY_TAG_BOOL_LLVM: i64 = 2;
const JSON_ANY_TAG_STRING_LLVM: i64 = 3;
const JSON_ANY_TAG_NULL_LLVM: i64 = 4;
const ABI_DEFAULT_ASK_TIMEOUT_MS_LLVM: i64 = 30_000;
const ACTOR_REF_LLVM_TYPE: &str = "%KainActorRef";
const REPLY_PORT_ACTOR_NAME: &str = "KainReplyPort";
const REPLY_PORT_LLVM_TYPE: &str = "%KainReplyPort";
const KAIN_CONVERGE_LANE_MAX_LLVM: usize = 8;
```

## Appendix: File Map

```
X:/crates/sys-codegen/src/codegen_llvm/
├── mod.rs                    (904.3 KB)  → Main LlvmGenerator + all codegen
├── component.rs              (84.0 KB)   → KainComponentSurface vtable + JSX
├── chunk-00-infra.tsv        (29.0 KB)   → Infrastructure catalog (134 rows)
├── chunk-02-ownership.tsv    (12.2 KB)   → Ownership analysis catalog (104 rows)
├── chunk-03-analysis.tsv     (14.9 KB)   → Analysis passes catalog (93 rows)
├── chunk-04-values.tsv       (14.2 KB)   → Value codegen catalog (88 rows)
├── chunk-05-abi.tsv          (9.5 KB)    → ABI codegen catalog (50 rows)
├── chunk-06-functions.tsv    (5.1 KB)    → Function/item catalog (46 rows)
├── chunk-07-semantics.tsv    (3.6 KB)    → Semantic constructs catalog (29 rows)
├── chunk-08-stmts.tsv        (2.7 KB)    → Statement codegen catalog (25 rows)
├── chunk-09-exprs.tsv        (4.7 KB)    → Expression codegen catalog (39 rows)
├── chunk-10-component_calls.tsv (48.6 KB)→ Component surface catalog (229 rows)
└── llvm.md                   (this file) → Complete architecture documentation
```

---

*End of LLVM Codegen Architecture & Function Catalog.*
*Total: ~934 cataloged functions across 10 TSV chunks + architecture documentation.*
