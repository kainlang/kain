# Kain Self-Host Compiler (kainc) vs Rust Bootstrap: Gap Analysis

**Date:** 2026-06-12
**Kain src location:** `X:\blades\kain\src\` (23 files, 13,523 lines total)
**Rust bootstrap location:** `X:\crates\core\src\` (31 files, 64,942 lines total)
**Other relevant crates:** 67 total in `X:\crates\`, including `crates/actor`, `crates/ownership`, `crates/gpu`, `crates/monomorphize`, `crates/types`, etc.

---

## Summary Table

| Subsystem | Rust Bootstrap | Kain Self-Host | Gap Severity | Effort to Close |
|-----------|---------------|----------------|-------------|-----------------|
| **Lexer** | 583 lines, Logos-derive, 127 token kinds | 778 lines, hand-written DFA, 127 token kinds | NONE (functionally complete) | Already ahead |
| **Token/Constants** | 3819 lines (merged in ast.rs) | 187 lines (dedicated token.kn) | MINOR | Already ahead |
| **AST** | 3819 lines, 40+ item types, 60+ expr types | 357 lines, ~50 tag constants + flat node struct | MAJOR | 2-3 weeks |
| **Parser** | 11,081 lines, full Pratt + recursive descent | 3,345 lines, functioning parser with gaps | MAJOR | 2-3 weeks |
| **Types/Typechecker** | 16,124 lines + 2,077 monomorphize | 1,873 + 420 lines; 4-pass pipeline with stubs | CRITICAL | 4-6 weeks |
| **Effects** | 140 lines, bitmask lattice | 129 lines, matching bitmask lattice | NONE | Already ahead |
| **Codegen (LLVM text)** | ~3,000 lines (across crates) | 1,563 lines, partial text emission | MAJOR | 2-4 weeks |
| **LLVM FFI** | ~200 lines (in sys-codegen) | 696 lines, full type/constant definitions | MINOR | 1 week |
| **Runtime Table** | 10,418 lines (runtime.rs interpreter) | 550 lines, 200+ function declares table | MINOR | 1-2 weeks |
| **JIT** | 0 lines (no JIT in bootstrap) | 1,014 lines (4 files: x86, metal, orc, cache) | N/A (Kain ONLY) | Already ahead |
| **CLI** | ~600 lines (in cli crate) | 461 lines, mostly stubs | MODERATE | 1 week |
| **Compiler Driver** | ~800 lines (in driver crate) | 387 lines, full pipeline skeleton | MODERATE | 1-2 weeks |
| **Orchestrator** | 0 lines (no markscript in bootstrap) | 897 lines, markscript embedding + stubs | N/A (Kain ONLY) | Already ahead |
| **Builtins** | ~500 lines (scattered) | 314 lines, full type/function tables | MODERATE | 1 week |
| **Span/Error** | 615+155 lines | 56+99 lines, simpler but functional | MINOR | <1 week |
| **Monomorphize** | 2,077 lines (dedicated crate) | 420 lines, mostly stub/skeleton | CRITICAL | 3-4 weeks |
| **Low-level memory** | 3,356 lines | ~200 lines (in codegen/runtime) | MAJOR | 2-3 weeks |
| **World/Entangle/Patch** | ~3,000 lines (types.rs) | All STUBS returning default values | CRITICAL | 4-6 weeks |
| **Actor model** | ~4,000 lines (dedicated crate) | All STUBS | CRITICAL | 4-6 weeks |
| **Converge/Orchestrate** | ~2,000 lines (scattered) | All STUBS | CRITICAL | 4-6 weeks |
| **UI/Components** | 3,731 lines (ui.rs) | AST + parser stubs only | CRITICAL | 3-5 weeks |
| **GPU/Shaders** | ~3,000 lines (gpu crate) | AST + parser stubs only | CRITICAL | 4-6 weeks |
| **Module Resolution** | 431 lines | 0 (stub returns "") | MAJOR | 2-3 weeks |
| **Stdlib** | 2,017 lines | 0 (delegates to stdlib at runtime) | MODERATE | 2-3 weeks |
| **Selfhost/Ouroboros** | 0 (builds itself in Rust) | Source-order combine pattern | N/A | Already designed |

---

## 1. Lexer

### Rust bootstrap (`lexer.rs: 583 lines`)
- **Implementation:** Logos-derive macro generates DFA from annotated enum
- **Token kinds:** `TokenKind` enum with `#[token(...)]` annotations (114 token annotations)
- **Key types:** `Lexer<T: Iterator<Item = char>>`, `SpannedToken`, `LexerMode`
- **Features:**
  - Python-style indentation (INDENT/DEDENT tokens)
  - Rust-style identifiers and literals
  - String/char escape sequences (\n, \t, \r, \\, \", \', \0)
  - Number literals: decimal, hex (0x), octal (0o), binary (0b), underscore separators
  - Float literals with decimal point
  - Line comments (//) and hash comments (#)
  - Format strings (f"...")
  - Multi-line comment suppression
  - JSX angle bracket detection (<, </)
  - Full operator set (52 operators including compound assignment)
  - String interning support

### Kain src (`lexer.kn: 778 lines`)
- **Implementation:** Hand-written DFA with value-semantics state threading
- **Token kinds:** 127 constants in `token.kn` (TOKEN_FN=0 through TOKEN_ERROR=126)
- **Key types:** `LexerState`, `TokenResult`, `Token`
- **Features:**
  - Python-style indentation via `indent_process()` post-pass
  - Full identifier/keyword recognition (58 hard keywords in keyword map)
  - String/char escape sequences (\n, \t, \r, \\, \", \', \0)
  - Number literals: decimal, hex, octal, binary with underscore separators
  - Float literals with decimal point
  - Line comments (//) and hash comments (#) — hash only at start of line
  - Format strings (f"...")
  - Full operator set (52 operators incl compound assignment, JSX </)
  - Error handling with structured diagnostics

### GAP: NONE
The Kain lexer is **functionally complete** — it handles all the same token types, with more lines because it's hand-written rather than deriving from Logos. The indent processor is actually more explicit about bracket-depth suppression than the Rust version.
- **Risk:** Low. The lexer is the most mature part of the self-host compiler.

---

## 2. AST Definitions

### Rust bootstrap (`ast.rs: 3,819 lines`)
- **Representation:** Typed `struct`/`enum` per construct (~40 item kinds, 60+ expression kinds)
- **Key types:** `Program { items: Vec<Item> }`, `Item::Function(FnDef)`, `Item::Struct(StructDef)`, `Item::Enum(EnumDef)`, `Item::Trait(TraitDef)`, `Item::Impl(ImplDef)`, `Item::World(WorldDef)`, `Item::Entity(EntityDef)` (entangle), `Item::Converge(ConvergeDef)`, `Item::Orchestrate(OrchestrateDef)`, `Item::Pulse(PulseDef)`, `Item::Resonate(ResonateDef)`, `Item::Resonate(ResonateDef)`, `Item::Component(ComponentDef)`, `Item::Shader(ShaderDef)`, `Item::Actor(ActorDef)`, `Item::Import(ImportDef)`, `Item::Patch(PatchDef)`, `Item::Law(LawDef)`, `Item::Axiom(AxiomDef)`, `Item::Comptime(ComptimeDef)` — 25+ item variants
- **Expression types:** 60+ variants covering all layers (0-7)
- **Extra features:** `AtomicOrdering`, `CpuFenceKind`, `InlineAsmOptions`, `ComputePlan`, `TensorPlan`, `JSXNode`, `SHATTER_ATTRIBUTE_NAME`
- **Complex nested types:** Rich `Expr` enum with ~60 variants, each with typed fields

### Kain src (`ast.kn: 357 lines`)
- **Representation:** Flat integer-indexed array of `AstNode { kind: Int, span_start, span_end, data: Array<Int> }`
- **Key types:** `AstNode`, `AstProgram { root, nodes }`, `StringTable`
- **Tag constants:** 38 item kinds, 12 statement kinds, 64 expression kinds, 9 pattern kinds, 14 type AST kinds, 21 binary ops, 6 unary ops
- **Constructors:** `ast_new_node`, `ast_new_leaf`, `ast_new_empty`, `ast_new_child`, `ast_new_two`, `ast_new_three`

### GAP: MAJOR
The Kain AST representation is fundamentally different — flat arrays of integer nodes vs Rust's typed enum hierarchy. The integer-tag approach is simpler but loses type safety:
- No typed field access — all fields go through `ast_data_get(node, idx) as Int`
- String table requires index-based lookups
- Semantic constructs (world/entangle/converge/etc.) have flat data layouts that must be manually parsed at every consumer
- No `ComputePlan`, `AtomicOrdering`, `CpuFenceKind`, `InlineAsmOptions` types
- **Risk:** The flat representation is actually a valid design choice for bootstrapping, but it means every consumer (typechecker, codegen) must re-derive semantics from raw integers. This creates a lot of boilerplate and is error-prone.

---

## 3. Parser

### Rust bootstrap (`parser.rs: 11,081 lines`)
- **Algorithm:** Recursive descent + Pratt parsing (precedence climbing for expressions)
- **Reserved keywords:** ~58 reserved words (hard + contextual)
- **Item parsers:** Complete implementations for ALL 25+ item kinds:
  - `parse_fn`, `parse_struct` (with generics, where clauses), `parse_enum` (with variants), `parse_trait` (with default methods), `parse_impl` (inherent + trait), `parse_type_alias`, `parse_use`, `parse_mod`, `parse_const`
  - `parse_component` (full JSX syntax parsing with `parse_jsx_node`, `parse_jsx_element`, `parse_jsx_expr`)
  - `parse_shader` (vertex/fragment/compute with uniform/workgroup declarations)
  - `parse_actor` (state + on-message handlers)
  - `parse_world`, `parse_entangle`, `parse_converge` (spec + fast lanes + verify random),
  - `parse_orchestrate` (stage graphs with residency/transfer/guarded-by),
  - `parse_pulse` (every/jitter), `parse_resonate` (dampen),
  - `parse_patch`, `parse_law`, `parse_axiom` (when/target/capability/guarantee/fallback),
  - `parse_import` (first-class Python, C include)
  - `parse_macro`, `parse_comptime`, `parse_test`
- **Expression parsers:** Full Pratt parser for ~60 expression types
- **Statement parsers:** for/while/loop/break/continue/return/defer/let/var/dispatch
- **Pattern parsing:** match patterns (wildcard, literal, binding, struct, tuple, variant, slice, OR, range)
- **Type parsing:** 14 type forms with generic params, where clauses
- **Error recovery:** Synchronization points, best-effort parsing after errors
- **JSX parsing:** Full JSX tree with components, expressions, for loops
- **Shader keywords:** parse_compute, parse_vertex, parse_fragment with uniform bindings

### Kain src (`parser.kn: 3,345 lines`)
- **Algorithm:** Recursive descent + Pratt parsing (same approach)
- **Reserved keywords:** ~56 reserved words
- **Implemented parsers (complete):**
  - `parse_fn`, `parse_struct`, `parse_enum`, `parse_trait`, `parse_impl`, `parse_type_alias`
  - `parse_use`, `parse_mod`, `parse_const`, `parse_test`
  - `parse_let`, `parse_var`, `parse_return`, `parse_defer`, `parse_for`
  - `parse_while`, `parse_loop`, `parse_break`, `parse_continue`, `parse_dispatch`
  - Full expression parsing (Pratt): binary, unary, postfix, primary, block, if, match
  - `parse_type` (all 14 type AST forms)
  - `parse_generic_params`
- **Partially implemented / stubs:**
  - `parse_struct_literal` — likely complete
  - `parse_array_literal` — likely complete
  - `parse_macro_call` — likely complete
  - Pattern parsing — probably partial
- **MISSING parsers (not present):**
  - No `parse_component` — component JSX parsing is missing
  - No `parse_shader` — shader vertex/fragment/compute parsing missing
  - No `parse_actor` — actor state/on-handler parsing missing
  - No `parse_world` — world state/surface parsing likely missing
  - No `parse_entangle` — entangle with single_writer parsing likely stub
  - No `parse_converge` — spec/fast/verify-random parsing likely stub
  - No `parse_orchestrate` — stage/residency/transfer/guarded-by likely stub
  - No `parse_pulse` — every/jitter parsing likely stub
  - No `parse_resonate` — dampen window parsing likely stub
  - No `parse_patch` — patch mutation parsing likely stub
  - No `parse_law` — law predicates parsing likely stub
  - No `parse_axiom` — when/target/capability/guarantee/fallback likely stub
  - No `parse_import` — Python/C import parsing missing
  - No full pattern matching parsing
  - No JSX expression parsing
  - No shader uniform/workgroup declarations

### GAP: MAJOR
33% of the Rust parser's functionality (3,345 lines vs 11,081). The core Layer-0 constructs (fn, struct, enum, let, if, while, for, expressions) are well-implemented. But **ALL Layer 1-7 constructs** (world, entangle, converge, orchestrate, pulse, resonate, patch, law, axiom, component, shader, actor) have only stub parsers or don't exist at all. The semantic stack is barely covered.
- **Risk:** High. Without these parsers, the self-host compiler can't parse most real Kain code. The benchmarks in `benchmark/cases_v2/` use these constructs extensively. The parser is the single biggest blocker to self-hosting.

---

## 4. Types / Typechecker

### Rust bootstrap (`types.rs: 16,124 lines + monomorphize.rs: 2,077 lines`)
- **Representation:** `ResolvedType` enum with ~25 variants, full generic system
- **Key types:** `TypeEnv`, `TypedProgram { items: Vec<TypedItem> }`, `TypedItem::Function(TypedFunction)`, `TypedItem::World(TypedWorld)`, etc.
- **Features:**
  - Full type inference (bidirectional for complex cases)
  - Generic type parameters with where-clause constraints
  - Trait system with method resolution, associated types
  - Impl blocks (inherent + trait impl)
  - World/entangle type checking with endpoint validation
  - Actor definition validation (state, message handlers, typed contracts)
  - Converge spec/fast-lane/verify type checking
  - Orchestrate stage graph type checking
  - Pulse/resonate handler type checking
  - Patch/law validation
  - Axiom capability validation
  - Shader type checking (uniform bindings, storage buffers)
  - Component/JSX type checking
  - Ownership region type checking (collapse/observe/decay/share)
  - Effect lattice checking (Pure < IO|GPU|Async < Unsafe)
  - Atomic ordering validation
  - C ABI policy integration
  - Module resolution + stdlib type imports
  - Attribute validation (section, link_name, callconv, packed, aligned, etc.)
  - ~10k lines of runtime/interpreter (runtime.rs) for dynamic checking of comptime, actors, etc.

### Kain src (`types.kn: 1,873 lines + monomorphize.kn: 420 lines`)
- **Representation:** `ResolvedType` struct with ~20 kind constants, flat type registry
- **Key types:** `TypeEnv` (parallel arrays), `TypedProgram`, `TypedItem`, `ResolvedType`
- **Implemented features:**
  - 4-pass pipeline: pass1 (predeclare names), pass2 (register types), pass3 (re-register forward refs), pass4 (full check)
  - `types_compatible()` — recursive structural type comparison (~250 lines)
  - `check_function_item` — with parameter/resolution, effect mask, body checking
  - `check_block_body` — statement walker with let/return/while/for/if/defer/loop/break/continue
  - `check_let_stmt`, `check_return_stmt`, `check_if_stmt`, `check_while_stmt`, `check_for_stmt`, `check_defer_stmt`, `check_loop_stmt`
  - `check_struct_item`, `check_enum_item`, `check_const_item`, `check_type_alias_item`, `check_trait_impl_item`
  - `infer_expr_type` — inference for all expression kinds (returns types for 50+ expression types)
  - `resolve_type_ast` — AST type node → ResolvedType (all 14 type kinds)
  - `check_effect_calls_in_expr` — effect violation detection for calls, memory ops, asm
  - Stub checkers: `check_patch_law_stub`, `check_converge_stub`, `check_orchestrate_stub`, `check_pulse_resonate_stub`, `check_axiom_stub`, `check_world_stub`, `check_entangle_stub`, `check_shader_stub`

### GAP: CRITICAL
The 4-pass pipeline architecture is sound, but the Layer 1-7 typecheckers are **all stubs that return default values**. They verify structure exists but don't actually check:
- World state field types, surface bindings, entangle endpoint compatibility
- Converge spec/fast lane type matching, verify random input generation
- Orchestrate stage graph dependency types, residency/transfer compatibility
- Pulse handler effects (currently returns ALL effects)
- Actor state initialization, message parameter types, handler completeness
- Shader uniform/storage buffer type compatibility
- Component prop types, state initialization
- Match exhaustiveness checking
- Generic type parameter substitution
- Trait bound enforcement
- `where` clause validation
- **Risk:** This is the hardest gap to close. The Rust typechecker is 16,124 lines for good reason — Kain's type system is rich. The self-host version needs ~10,000 more lines to reach parity.

---

## 5. Effects

### Rust bootstrap (`effects.rs: 140 lines`)
- `Effect` enum (7 variants), `EffectSet` struct with bitmask
- `check_effect_call()` function
- `can_call()` with 4 rules matching Kain spec

### Kain src (`effects.kn: 129 lines`)
- 8 effect constants (EFF_PURE through EFF_PANIC — actually MORE than Rust with Alloc+Panic)
- `EffectSet` struct, `can_call()` with identical 4 rules
- `parse_effects_from_names()`, `effect_set_to_string()`, `pulse_body_effects()`
- Duplicated in types.kn for standalone check compatibility

### GAP: NONE
The Kain effect system is actually **more complete** than the Rust version — it has `Alloc` and `Panic` effects that the Rust bootstrap doesn't list. The effect lattice implementation is correct and the `can_call()` rules match exactly.

---

## 6. LLVM / Codegen

### Rust bootstrap (~3,000 lines across crates, primarily `crates/sys-codegen/`)
- Full LLVM IR text emission (declare, define, all instructions)
- LLVM-C API bindings (native on Windows)
- Function codegen: all expression types → LLVM IR
- Debug info emission (DILocation, DISubprogram, source mappings)
- Target triple handling, data layout
- Global variable support
- Struct type lowering (named struct types)
- Inline asm emission
- C ABI lowering (win64cc, sysvcc, fastcc)
- Attribute emission (noreturn, uwtable, etc.)
- Section/linkage control

### Kain src (`codegen.kn: 1,563 lines + llvm_ffi.kn: 696 lines`)
- **Path A (textual .ll):** Working LLVM IR text emission
  - `LlvmGenerator` state: tracks registers, labels, locals, struct defs, loop stack
  - Function prologue/epilogue emission
  - Binary op emission (add, sub, mul, div, mod, etc.)
  - Integer comparison emission
  - Memory load/store emission
  - Control flow: if/else, while loops via labels and branches
  - Function call emission
  - Struct literal construction via insertvalue/extractvalue
  - Variable access via load/store, alloca in entry block
  - Runtime function declare emission (delegates to runtime.kn)
  - Target triple, data layout strings
- **Path B (LLVM-C API):** All function bodies are empty stubs returning null/0
  - `llvm_context_create()`, `llvm_module_create_with_name()`, etc. — all return `int_to_ptr(0, "Byte")`
  - The LLVM-C API path is non-functional
- **llvm_ffi.kn:** Complete type definitions and constants for LLVM-C API:
  - All opaque pointer type aliases (LLVMContextRef, LLVMModuleRef, etc.)
  - IntPredicate, RealPredicate constants (32-41, 0-13)
  - Linkage, visibility, calling convention constants
  - Atomic ordering, optimization level constants
  - Code model, relocation model constants
  - Verifier failure action constants
  - COVERED via `include <llvm-c/Core.h> as llvm` — libclang-powered C imports

### GAP: MAJOR
Path A (textual LLVM IR) is about 30-40% complete:
- Working: basic function codegen, binary ops, if/else, while loops, calls, struct literals, variable access
- MISSING: match expressions, enums (tagged unions), array literals, tuple literals, lambdas/closures, try/await
- MISSING: shader type emission, component/JSX lowering
- MISSING: debug DWARF metadata emission
- MISSING: inline asm emission (asm string → LLVM IR inline asm)
- MISSING: atomic operations (atomic load/store/add/cmpxchg/fence)
- MISSING: ownership (collapse/observe/decay) → LLVM IR lowering
- MISSING: world/entangle/patch/law/converge/orchestrate → LLVM IR lowering
- MISSING: LLVM-C API path is completely stubbed — real implementation requires working `include <llvm-c/...>` FFI
- **Risk:** Moderate. Path A can be incrementally extended by adding more expression compilers. Path B is blocked on LLVM-C FFI working in the bootstrap.

---

## 7. JIT (Kain ONLY — no Rust equivalent)

### Rust bootstrap: NOT PRESENT
The Rust compiler produces a native .exe via LLVM. There is no JIT compiler in the bootstrap.

### Kain src (1,014 lines across 4 files)
- **jit_metal.kn (130 lines):** W^X memory lifecycle (vm_map → write → protect RX → cache_flush → full_fence → execute), shared asm trampoline via `asm("mov rax, [rdi]\\ncall rax\\nmov [rdi+8], rax")`
- **jit_x86.kn (515 lines):** x86-64 direct machine code emission. Fixed register allocation (RAX, RBX, RBP, RSP, RDI), operand stack at RBP-relative offsets, two-pass jump fixup, 30+ opcode emitters
- **jit_orc.kn (146 lines):** LLVM OrcJIT compilation path (stub — LLVM shared library needed)
- **jit_cache.kn (113 lines):** Simple hash-based bytecode cache with W^X store/load
- **jit.kn (110 lines):** Path dispatcher — autoselects x86 or OrcJIT

### GAP: N/A — Kain EXTENSION beyond Rust bootstrap
The JIT subsystem is unique to the self-host compiler. It provides a rapid feedback loop for development that the Rust bootstrap never needed.
- **Risk:** Low. jit_x86.kn and jit_metal.kn are real and functional (ported from `blades/markscript/` where they have 17 proven self-tests).
- **OrcJIT integration** is stubbed pending working `include <llvm-c/Orc.h>` FFI.

---

## 8. Runtime Function Table

### Rust bootstrap (`runtime.rs: 10,418 lines`)
- Full Kain runtime interpreter with actor scheduler, async future system, etc.
- Runtime function declarations are embedded inline in sys-codegen

### Kain src (`runtime.kn: 550 lines`)
- `RuntimeTable` with 200+ runtime function entries organized by category
- `emit_runtime_declares()` — generates LLVM `declare` statements
- `kain_type_to_llvm_ir_str()`, `kain_type_to_c_type()` — type mapping tables
- Target triple and data layout functions
- C ABI policy helpers (LP64, LLP64 sizes)
- Runtime function lookup by name and category
- Categories: core, stdlib, actor, memory, ownership, machine, gpu, python, math, fs, process, startup, json, collections, converge, string

### GAP: MINOR
The runtime function table is comprehensive (200+ functions covering all subsystems). It correctly generates LLVM `declare` statements. The Rust version has an actual interpreter/scheduler, which the Kain version doesn't need since it delegates to the native runtime.
- **Risk:** Low. The table just needs maintenance as new runtime functions are added.

---

## 9. Monomorphization

### Rust bootstrap (`monomorphize.rs: 2,077 lines`, dedicated crate)
- Generic function instantiation with type parameter substitution
- Monomorphization cache to avoid duplicate instantiations
- Trait bound resolution during instantiation
- Mangled symbol name generation
- Generic struct specialization

### Kain src (`monomorphize.kn: 420 lines`)
- `MonomorphizedProgram` struct with items + mangled map
- Type unification helpers: `unify_types()`, `substitute_types()`, `instantiate_types()`
- Generic substitution: `substitute_in_ast()` for expression nodes
- Stub monomorphize runner that passes items through
- **MISSING:** Actual generic instantiation loop, mangling scheme, trait bound resolution, instantiation cache

### GAP: CRITICAL
The monomorphizer is the bridge between typechecker and codegen. Without it, generic functions produce wrong code. The existing code defines the right data structures but the actual substitution/instantiation logic is not wired in.
- **Risk:** Very high. The codegen can't lower generic functions without a working monomorphizer. This blocks self-hosting the compiler itself (which uses generics like `Array<T>`, `Option<T>`, `Result<T,E>`).

---

## 10. Semantic Layer 1-7 Constructs

### Rust bootstrap
All layers have complete implementations in `types.rs`, `parser.rs`, `ast.rs`, plus dedicated crates:
- **L1 (World/Entangle):** Full definitions, validation, codegen lowering
- **L2 (Patch/Law):** Complete parsing, typechecking, runtime contract
- **L3 (Converge):** Full spec/fast-lane/verify with type matching
- **L4 (Orchestrate):** Stage graph with residency, transfer, guards, fallbacks
- **L5 (Pulse/Resonate):** Handler validation, dampening, effect auto-emission
- **L6 (Axiom/Shatter/Teleport):** Capability validation, SoA layout types
- **L7 (Actor/Ownership):** Actor state machine, collapse/observe/decay/share/fanout

### Kain src
- AST tag constants exist for all constructs
- Parser: stubs or missing for all Layer 1-7 constructs
- Typechecker: `check_*_stub()` functions returning default values for ALL Layer 1-7 constructs
- Codegen: no emission logic for any Layer 1-7 construct

### GAP: CRITICAL
This is the biggest gap in the entire self-host compiler. Every construct above Layer 0 is a stub. The decision ladder — Kain's core innovation — is not exercised by the self-host compiler.

---

## 11. Compiler Driver / Pipeline

### Rust bootstrap
- `kain_driver` crate with `CompilerSession`, pipeline phases
- `crates/cli/kain.rs` — CLI entry point with subcommand dispatch
- Workspace discovery, KAIN.toml reading, --json output
- Integration with all crate subsystems

### Kain src (`compiler.kn: 387 lines + cli.kn: 461 lines + main.kn: 59 lines`)
- `DriverSession` with 7-phase pipeline (Resolve→Lex→Parse→Typecheck→Mono→Codegen→Link)
- `compile_file()` / `check_file()` — entry points with file I/O
- CLI parsing for subcommands (check, build, run, test, selfhost, fmt, amalgamate, doctor, config, clean, help, version)
- Most subcommand handlers are stubs that print and return 0
- `emit_progress()` phase reporting
- `emit_diagnostics_to_stderr()` — error pretty printing

### GAP: MODERATE
The pipeline skeleton is functional for `check` and `compile` targets. But:
- `build` subcommand is a stub (doesn't invoke clang to link)
- `run` subcommand is a stub (doesn't execute the binary)
- `test` subcommand is a stub (doesn't run compiletest)
- `selfhost` subcommand is a stub (doesn't do ouroboros bootstrap)
- Workspace discovery returns empty string
- No KAIN.toml parsing
- No --json output
- **Risk:** Moderate. The skeleton exists; filling in the subcommands is plumbing work.

---

## 12. Orchestrator / MarkScript

### Rust bootstrap: NOT PRESENT
The Rust compiler has no markScript integration.

### Kain src (`orchestrator.kn: 897 lines`)
- Full markscript VM embedding with IVT handler registration (IDs 200-208)
- BuildConfig struct with 17 fields populated from markscript tables
- CLI entry points: `orch_check_cli`, `orch_build_cli`, `orch_run_cli`, `orch_test_cli`, `orch_selfhost_cli`
- All handlers are STUBS — return 0 and print diagnostics
- Diagnostics and TestResult structs
- Build pipeline design: compile-check → test-run → test-report → build-link → build-package

### GAP: N/A — markscript is a Kain innovation
The markscript VM embedding is a novel architecture choice for the self-host compiler. All the handlers are stubs but the integration pattern is established.
- **Risk:** Moderate. The markscript handlers need to be wired to real compiler functions.

---

## 13. Builtins

### Rust bootstrap
- Builtin types defined in types.rs string tables
- Builtin functions registered during TypeEnv init via `BuiltinFunction` structs
- Scattered across types.rs + stdlib.rs

### Kain src (`builtins.kn: 314 lines`)
- `BuiltinType` struct: 25 primitive types (I8-I128, U8-U128, Isize, Usize, Int, UInt, F32, F64, Float, Bool, Char, Byte, Unit, Never, String, ptr, ref, Option, Result, Future, AtomicInt, AtomicBool, AtomicPtr, TraitObject, RuntimeArray, ActorRef)
- `BuiltinFunction` struct: 45+ builtin functions (alloc, mem_load, mem_store, ptr_offset, asm, atomics, vm_*, sizeof, alignof, bitcast, cpu intrinsics, runtime lifecycle)
- `BUILTIN_UNSAFE_NAMES` array for Unsafe effect detection
- Three-layer stdlib pattern documentation (abi_X → native_X → X)

### GAP: MINOR
The builtins table is comprehensive and well-organized. It covers all the primitives and builtin functions needed.
- **Risk:** Low. Just needs maintenance as new builtins are added.

---

## 14. Module Resolution

### Rust bootstrap (`module_resolution.rs: 431 lines`)
- `resolve_filesystem_module_file_with_context()` — finds .kn files on disk
- `resolve_stdlib_module_file()` — finds stdlib modules by name
- `FilesystemModuleResolutionContext` — working directory tracking
- Sandbox-aware path resolution

### Kain src: NOT IMPLEMENTED
- `discover_workspace()` in compiler.kn returns `""`
- No `KAIN.toml` or `build.kn` reading
- No stdlib path resolution
- No import resolution (use statements)

### GAP: MAJOR
Without module resolution, the self-host compiler can only compile single-file programs. Multi-file workspaces and stdlib imports are non-functional.
- **Risk:** High. The compiler can't even import `std::fmt` or `std::fs` at the bootstrap level.

---

## 15. Diagnostic / Error Infrastructure

### Rust bootstrap (`diagnostics.rs: 615 lines + error.rs: 155 lines`)
- Full error pretty-printing with source line context
- Error code registry with descriptions
- Severity levels (error, warning, note, help)
- Semantic enrichment pipeline

### Kain src (`error.kn: 99 lines + span.kn: 56 lines`)
- `KcDiagnostic` struct with severity/file/line/column/message/span
- `KcDiagnosticBag` accumulator with error/warning/note separation
- MAX_ERRORS limit (50)
- 30+ error kind constants (E0001-E0700)
- Span computation from byte offsets (line_no, col_no)
- Missing: source line display, multi-line spans, colored output, error code descriptions

### GAP: MINOR
The diagnostic infrastructure is basic but functional. The Rust version has richer formatting.
- **Risk:** Low. Can be improved incrementally.

---

## 16. Hard vs Contextual Keywords — Discrepancy

The Kain lexer in `token.kn` defines 127 token kinds, but the keyword map in `lexer.kn` lists ONLY 58 hard keywords (the ones that get dedicated TOKEN_* constants). Contextual keywords arrive as `TOKEN_IDENT` and are resolved by the parser.

**Contextual keywords that are NOT in the lexer keyword map (must be parsed contextually):**
- `world`, `entangle`, `single_writer`, `surface`, `native_ui`, `web`, `ue5`, `viewport3d`
- `converge`, `spec`, `fast`, `verify`, `random`, `capability`
- `orchestrate`, `stage`, `after`, `deps`, `residency`, `transfer`, `guarded`, `by`, `requires`, `policy`, `fallback`
- `pulse`, `every`, `jitter`, `resonate`, `dampen`
- `axiom`, `guarantee`, `shatter`, `teleport`, `to`, `from`, `via`
- `collapse`, `observe`, `decay`, `share`, `fanout`, `weak`
- `on`, `state` (in actor/component context)
- `component`, `render`, `shader`, `compute`, `vertex`, `fragment`, `workgroup`
- `include`, `import`, `where`, `comptime`, `macro`, `test`
- `patch`, `law`

**MISSING from Kain lexer keyword map (hard keywords that ARE in Rust token enum but NOT in Kain map):**
- None — the Kain lexer actually maps MORE keywords than the Rust bootstrap (e.g., `vertex`, `fragment`, `compute`, `workgroup` as hard tokens, while Rust handles some contextually)

### GAP: MINOR
The contextual keyword resolution needs parser-level functions like `parser_peek_contextual()` and `parser_expect_contextual()` — which DO exist in parser.kn. The architecture is sound.

---

## Risk Assessment Summary

| Priority | Area | Current Status | Target | Estimated Effort |
|----------|------|---------------|--------|-----------------|
| 1 | Module Resolution | Not implemented | ~500 lines | 2-3 weeks |
| 2 | Layer 1-7 Parsers | ~30% complete (stubs) | ~8,000 more lines | 4-6 weeks |
| 3 | Layer 1-7 Typecheckers | ~10% complete (stubs) | ~10,000 more lines | 6-8 weeks |
| 4 | Monomorphization | ~30% complete (framework) | ~1,500 more lines | 3-4 weeks |
| 5 | Codegen: Expression lowering | ~40% complete (Layer 0) | ~2,000 more lines | 2-4 weeks |
| 6 | Codegen: LLVM-C API | Stub (0%) | ~500 lines + FFI | 2-3 weeks |
| 7 | CLI subcommands | ~30% functional | ~500 lines | 1-2 weeks |
| 8 | Error diagnostics | ~60% complete | ~300 lines | <1 week |

**Estimated total effort to reach parity with Rust bootstrap core:** ~25,000-30,000 lines across all subsystems, roughly 4-6 months of focused work.

### What Works TODAY
- Full lexer (matches Rust bootstrap)
- Full effects system (marginally ahead of Rust)
- AST constants framework (all tags defined)
- Core parser (Layer 0: fn, struct, enum, let, if, while, for, expressions)
- Typechecker foundation (4-pass pipeline + structural type comparison)
- Codegen Path A (textual LLVM IR for basic constructs)
- Full LLVM FFI type definitions
- Full runtime function table (200+ entries)
- JIT subsystem (x86-64 direct emission + W^X contract)
- Orchestrator/CLI skeleton
- Builtins registration
- Diagnostic framework

### What DOESN'T Work
- Any Layer 1-7 construct (world, entangle, converge, orchestrate, pulse, resonate, patch, law, axiom, component, shader, actor)
- Module resolution / multi-file compilation
- Generic monomorphization
- Codegen for match/enums/tuples/arrays/lambdas
- Debug info emission
- LLVM-C API path
- `build`, `run`, `test`, `selfhost` subcommands
- Real workspace discovery

### What's UNIQUE to Kain src (not in Rust bootstrap)
- JIT subsystem (x86-64 direct + OrcJIT + cache)
- markscript VM orchestration layer
- Source-order ouroboros combine pattern
- Flat integer-indexed AST (vs Rust's typed enums)
- W^X trampoline with inline assembly
