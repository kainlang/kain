# Research Synthesis: Kain Self-Host Compiler Architecture

**Produced by:** kain-explorer agent  
**Date:** 2026-06-12  
**Source documents:** 8 research docs in `X:/blades/kain/research/` (273KB total, ~12,500 lines)  
**Master spec:** `SELFHOST-KN.MD` (Version 2.0, ~87KB, ~1,727 lines)

---

## Table of Contents

1. [Document-by-Document Theses](#1-document-by-document-theses)
2. [Key Architectural Decisions Cross-Reference](#2-key-architectural-decisions-cross-reference)
3. [Source Files Referenced in crates/](#3-source-files-referenced-in-crates)
4. [Kain Source Files Expected to Exist](#4-kain-source-files-expected-to-exist)
5. [TODO Sections and Acknowledged Gaps](#5-todo-sections-and-acknowledged-gaps)
6. [Cross-References Between Documents](#6-cross-references-between-documents)
7. [Contradictions and Conflicts](#7-contradictions-and-conflicts)
8. [Coverage Gaps](#8-coverage-gaps)
9. [Alignment with SELFHOST-KN.MD](#9-alignment-with-selfhost-knmd)
10. [Overall Research Quality Assessment](#10-overall-research-quality-assessment)
11. [Phase Plan Breakdown](#11-phase-plan-breakdown)
12. [Critical Open Questions](#12-critical-open-questions)

---

## 1. Document-by-Document Theses

### 01-lexer-parser-ast.md (~59KB)

**Nuclear thesis:** The Rust bootstrap's 113KB parser (102 token kinds, 38 item variants, 64 expression kinds, 16 operator precedence levels) must be replicated in Kain using flat `Array<AstNode>` (no Box/Arc recursion) for direct LLVM mapping, value semantics, and cache locality. The self-host compiler exercises approximately 40% of the full language surface (25 of 38 item variants, 45 of 64 expression variants, 0 of 7 semantic layers L1-L7).

**Key decisions:**
- Flat array AST with integer indices instead of recursive `Box<enum>` trees
- Logos-powered lexer replaced by hand-written DFA or `use std::text`
- Pratt operator precedence parser with 16 levels, 20+ unary operators, 11 compound assignment operators
- Indent processing as a post-lexer pass on the raw token vector (not integrated into lexer)
- JSX token handling via special `<` / `</` / `>` token recognition
- Bracket depth tracking suppresses newlines inside `()`, `[]`, `{}`
- Contextual keywords (51) recognized via Ident matching in specific parser positions, not as dedicated TokenKind variants

**Estimated self-host cost:** ~7,500 lines Kain (lexer 500, token 150, ast 500, parser 3000, Pratt 500, JSX 500, typecheck 1500, codegen 2000, jit 300, compiler 200)

**TODO/gaps identified:**
- `>>` injection for nested generics `Vec<Vec<Int>>` -- identified as a known subtlety
- `**` (power operator) currently left-associative by implementation despite semantic expectation
- 3 of 23 markscript JIT opcodes skipped (OP_PUSH_NATIVE_HANDLE, OP_SET_SENSITIVITY, OP_POP_NATIVE_HANDLE)
- Test parsing not detailed
- `emit` and `receive` have lexer TokenKind entries but NO parser production rules (reserved keywords)

---

### 02-typechecker-types.md (~78KB)

**Nuclear thesis:** The self-host typechecker only needs ~10% of the Rust bootstrap's 15,625-line `types.rs` -- approximately 1,500-2,000 lines of Kain. The radical simplification is possible because the self-host compiler uses only Layer 0 constructs (fn, struct, enum, trait, impl, let, match, etc.) with a small `Unsafe` bridge to LLVM-C. It does NOT need world, patch, law, converge, orchestrate, actor, component, shader, pulse, resonate, or async typechecking. L1-L7 constructs are stubbed as structs with basic field validation.

**Key decisions:**
- 4-pass pipeline: predeclare (register type names) -> register signatures (resolve fields) -> re-register (recursive types) -> check expressions
- 20 `ResolvedType` variants (7 primitives, 4 compound, 3 stdlib, 2 named, 1 function, 3 special)
- Nominal typing for structs (name match, not structural)
- Numeric promotion is universal (any Int size matches any Float size)
- `Unknown` type acts as leniency escape valve
- `Generic(_)` is compatible with everything during typechecking
- Effect lattice: Unsafe (TOP) -> IO/GPU/Async/Reactive -> Pure (BOTTOM)
- 4 rules for effect checking: Pure callee->anyone, Pure caller->Pure only, Unsafe caller->anything, else subset check
- Self-host compiler only checks Pure and Unsafe effects; IO is used for file operations

**TODO/gaps identified:**
- 6 operations that SHOULD enforce Unsafe currently are NOT enforced: `mem_load`, `mem_store`, `ptr_offset`, `alloc`, `alloc_zeroed`, `realloc_mem`
- `dispatch "key" [...]` requires GPU effect but is NOT enforced
- The Rust bootstrap has 700+ line effect checking for asm/bitcast/fences/memory operations; self-host must replicate
- Pass 3 is NOT a fixpoint -- a single re-try only. If it fails, the item is permanently skipped
- Stub strategy for worlds/actors/components/patch/law/converge/orchestrate/pulse/resonate/axiom/shatter/teleport is described conceptually but not fully specified

---

### 03-llvm-codegen-jit.md (~87KB)

**Nuclear thesis:** The self-host compiler has two viable paths: Path A (textual LLVM IR emission via string formatting, same as the existing Rust codegen) and Path B (LLVM-C API OrcJIT via `include <llvm-c/Core.h> as llvm`). The Rust codegen is 21,289 lines of pure string formatting -- trivially reimplementable in Kain. Path B provides in-memory JIT compilation with full LLVM optimization passes.

**Key decisions:**
- LLVM-C API accessed via Kain's first-class C header import (`include <llvm-c/Core.h> as llvm`) -- no Rust glue, no llvm-sys, no binding generation
- All LLVM-C types are opaque `ptr<Byte>` in Kain
- C ABI policy in `low_level_abi.rs` provides LP64/LLP64 platform-aware type sizes
- Type mapping: 12 scalar types, 12 compound types, 5 special runtime types
- Alloca-store-load pattern for mutable variables; SSA registers for immutable let
- Phi nodes for if/else value production
- GEP for struct field access
- Loop stack with continue/break labels for while/for/loop/break/continue
- DWARF debug metadata for --debug mode

**TODO/gaps identified:**
- 200+ runtime C functions need `declare` statements emitted at module top
- C `long` is 32-bit on Windows (LLP64) vs 64-bit on Linux (LP64) -- the C ABI policy handles this
- The doc explicitly says the Rust codegen "does NOT link against LLVM" -- it generates text `.ll` files and shells out to clang
- Path B's OrcJIT integration is speculative and lacks a proven implementation in Kain (the markscript JIT proves the W^X path, not OrcJIT)

---

### 04-cli-driver-selfhost.md (~70KB)

**Nuclear thesis:** The Rust bootstrap CLI (crates/cli, crates/driver, crates/blades) provides a complete production-grade foundation with 26+ subcommands, an embeddable `DriverSession` with two-level caching, and multi-target codegen dispatch (17 compile targets including LLVM, C, Rust, C++, WASM, SPIR-V, PTX, HLSL, WGSL, JS/TS, UE5). The Kain self-host compiler must replicate the pipeline boundaries but delegates non-core functions to Rust DLL (Phase 1), then progressively replaces them.

**Key decisions:**
- Phase roadmap: 0 (starter CLI exists) -> 1 (Rust DLL bridge) -> 2 (Kain lexer+parser) -> 3 (Kain typechecker) -> 4 (Kain codegen via LLVM-C FFI) -> 5 (pure Kain, ouroboros)
- `DriverSession` caches frontend results (source bundle + checked frontend) keyed by content hash
- 17 compile targets dispatched via `match target { ... }` after monomorphization
- Error pipeline: `KainError` -> `SpanMapper` -> `Diagnostics::format_error()` -> stderr
- For self-host, the equivalent uses flat `Array<Diagnostic>` with token-index spans (no separate SpanMapper)
- The existing `main.kn` and `cli.kn` in `blades/kain/src/` already provide Phase 0 starter CLI

**TODO/gaps identified:**
- Phase timeline estimated at 12 weeks (revised from earlier 8 weeks)
- The existing Phase 0 CLI only has 5 subcommands; the full Rust CLI has 26+
- Bridge DLL strategy is described but not implemented
- `selfhost phase1` and `selfhost phase2` exist as Rust CLI subcommands but are not yet wired to Kain

---

### 05-runtime-contract-ffi.md (~77KB)

**Nuclear thesis:** The self-host compiler does NOT need to replicate the native C runtime in Kain. It emits LLVM IR that calls into the EXISTING `kain_runtime.lib` (47+ C files, ~50-service table). The compiler must get exactly 5 things right: emit `declare` statements, map types to LLVM types matching C ABI, emit `call` instructions with right args, untag tagged immediates, and materialize C return values.

**Key decisions:**
- libclang-powered C header import: `include <windows.h> as win` extracts 605 functions; `include <vulkan/vulkan.h> as vk` extracts 755 functions
- Three-tier fallback for header extraction: libclang (primary) -> lang_c AST (pure-Rust fallback) -> regex (last resort)
- Five resolution strategies for `include <>`: KAIN.toml -> blade discovery -> sibling .h+.c -> system header registry -> runtime-owned headers
- Companion `.c` discovery: replaces `.h` with `.c` and compiles as translation unit
- Full runtime ABI table documented: ~80 functions across 7 domains (core, stdlib, actor, memory, ownership, machine stones, GPU/converge/orchestrate, init/shutdown, Python interop, JSON/array/map/string utilities)
- `@extern` ABI contract: argument untagging, String <-> const char* conversion, return value materialization
- Calling convention variants: win64, vectorcall, sysv64, naked, section

**TODO/gaps identified:**
- The full runtime ABI table is incomplete for JSON/Array/Map/String utilities (table 5.10 was truncated)
- "Companion .c discovery" is described but edge cases (header with multiple .c files, .h with no .c) are not documented
- Python interop runtime functions listed but not mapped to specific python runtime files
- GPU dispatch and converge/orchestrate runtime functions are listed but their implementations are not documented

---

### 06-jit-markscript-metal-architecture.md (~54KB)

**Nuclear thesis:** The self-host compiler has TWO proven JIT paths sharing the same W^X trampoline: Path A (markscript-style direct x86-64 emission via `asm("...")` + `vm_map` + `vm_protect`) proven in 670 lines of working Kain, and Path B (OrcJIT via `include <llvm-c/Orc.h> as llvm_orc`) for full optimization. All 19 JIT primitives are proven in metal.kn benchmark cases 0-5 and 10.

**Key decisions:**
- Fixed register assignment (RAX=accumulator, RBX=callee-saved operand, RBP=frame pointer) -- no register allocator
- Software operand stack at RBP-relative offsets (not native push/pop) to avoid LLVM clobber-save conflicts
- Two-pass jump fixup for forward/backward/bidirectional jumps
- `shatter struct CacheStore` (SoA layout) for L1-friendly hash scans in the JIT cache
- 17 self-tests proving x86-64 emission, W^X lifecycle, trampoline, and fixup resolution
- markscript-style JIT starts instantly (no LLVM dependency); OrcJIT provides optimization for hot paths
- Path A uses RWX pages (pragmatic choice for prototype); the strict W^X sequence (RW -> write -> RX) is the production goal

**TODO/gaps identified:**
- 3 of 23 markscript bytecodes skipped (OP_PUSH_NATIVE_HANDLE, OP_SET_SENSITIVITY, OP_POP_NATIVE_HANDLE)
- markscript JIT uses RWX pages rather than stricter RW->RX sequence for Path A
- OrcJIT Path B has ZERO working Kain code -- it's a design spec, not a proven implementation
- The markscript JIT is for a custom bytecode VM, not for LLVM IR; Path A for the compiler would be fundamentally different
- No instruction selection or register allocation for non-trivial Kain functions

---

### 07-markscript-fusion-contract.md (~84KB)

**Nuclear thesis:** Markscript is not bolted onto the self-host compiler -- it IS the compiler's orchestration layer. Every operation that isn't lexing, parsing, typechecking, LLVM IR emission, or JIT execution belongs to markscript: build pipeline, config validation, test runner, REPL, watch mode, process lifecycle, documentation generation, and CI. Fusion eliminates ~3,230 lines of infrastructure code that would otherwise need to be written.

**Key decisions:**
- Embedded markscript VM (20 public functions in `std::markscript`, 7,500 lines total engine)
- 9 compiler-specific IVT handlers registered (compile check, compile codegen, compile jit, test run, test report, build link, build package, selfhost phase1, selfhost phase2)
- Build config as markscript tables (replaces KAIN.toml, Makefile, JSON config)
- @schema validation for all build config tables
- `orchestrator.kn` (~500 lines) is the sole integration file between compiler core and markscript
- Encapsulation boundary: compiler MUST NOT call internal markscript functions directly (prohibited list of 7 operations)
- Handler ID range 200-299 reserved for compiler (avoids collision with markscript core 1-12, BETA 13-50, GAMMA 51-59, DELTA 71-78)

**TODO/gaps identified:**
- The "build.md replaces build.kn" claim contradicts SELFHOST-KN.MD which lists build.kn in the file manifest
- The "eliminate Bazel" claim is aggressive and acknowledges build.kn as the project authority in other docs
- Markscript `@schema` validation is described at the concept level but the self-host's specific schema is incomplete
- The embedding API contract documents 20 functions but only ~12 are actually used by the 9 handlers
- `orchestrator.kn` is fully specified (~535 lines of Kain code) but NOT yet implemented
- The markscript engine is 7,500 lines of BLM dependency that the compiler must import

---

### SELFHOST-KN.MD (~87KB)

**Nuclear thesis:** This is the master spec. The zero-Rust, pure-Kain, LLVM-native self-compiling Kain compiler. Phase 1-4 uses ONLY Layer 0 Kain (fn, struct, enum, trait, impl, include, ptr, collapse/observe/decay, Pure/Unsafe/IO effects, asm, defer, use, let, match, if, for, while) -- 14 of 111 keywords. Phase 5+ gradually adopts Layers 1-7. The total is ~14,350 lines of Kain vs ~392K lines of Rust bootstrap code. The ouroboros bootstrap sequence kills both Rust AND Bazel.

**Key decisions:**
- Two-version strategy: Phase 1-4 uses Layer 0 only; Phase 5+ adds higher-layer constructs
- 25-file file manifest with per-file line estimates aggregated to ~14,350 lines total
- 18 crate categories mapped with elimination strategy (64% of Rust crates eliminated, 64% of line count)
- The Death List: 43 of 67 crates eliminated from self-host compiler (remainder are stdlib/runtime/GPU/UE5/LSP that stay in Rust)
- The Bazel Death Plan: BUILD files -> cascading empty -> removed. Compiler IS its own build system.
- The Triple Kill: Rust (67 crates, ~392K lines) + Bazel (73+ BUILD files, Java server) + TOML/YAML/JSON config sprawl
- Markscript fusion (Appendix H) eliminates ~3,230 lines of infrastructure
- 46% of Rust bootstrap code eliminated by not using L1-L7 constructs in the compiler
- 18% eliminated by LLVM-C FFI (no sys-codegen Rust crate needed)

**TODO/gaps identified:**
- v1.0 -> v2.0 corrections: Rust file count corrected from 658 to 660, line count from ~2.7M to ~392K, C runtime from 129 to 69 non-test files, crate elimination from 93% to 64%, self-host size from ~25K to ~14,350
- Known uncertainties: C runtime exact file count (52 core + 17 module + ~130 test), LLVM-C API surface (depends on LLVM version), markscript line counts (~7,500 approximate), Phase 12-week estimate
- "Appendix E stays list" vs "Death list contradiction with Appendix E" recorded -- resolved but messy
- The ouroboros bootstrap sequence in Appendix D is truncated (128 more lines not shown)

---

## 2. Key Architectural Decisions Cross-Reference

### The Big Architecture Decisions

| Decision | 01-LexParse | 02-Typecheck | 03-Codegen | 04-CLI | 05-RuntimeFFI | 06-JIT | 07-Fusion | SH-KN |
|----------|-------------|--------------|------------|--------|---------------|--------|-----------|-------|
| Flat array AST | Core thesis | Mentioned | Referenced | — | — | Referenced | — | Referenced |
| 4-pass typecheck pipeline | — | Core thesis | — | Mentioned | — | — | — | Referenced |
| Layer 0 only (Phase 1-4) | 40% surface claim | 90% reduction claim | — | — | — | — | — | Core thesis |
| Text-based LLVM IR (Path A) | — | — | Core thesis | Referenced | Referenced | — | — | Referenced |
| LLVM-C API codegen (Path B) | — | — | Core thesis | — | Referenced | Referenced | Referenced | Core thesis |
| MarkScript orchestration | — | — | — | Not mentioned | — | Not mentioned | Core thesis | Appendix H |
| DLL bridge (Phase 1) | — | — | — | Core thesis | — | — | Referenced | Core thesis |
| Dual JIT (markscript + OrcJIT) | — | — | Referenced | — | — | Core thesis | Referenced | Referenced |
| Stub strategy for L1-L7 | — | Core thesis | — | — | — | — | — | Core thesis |
| Rust crate death list | — | — | — | Referenced | — | — | — | Core thesis |
| C ABI policy (LP64/LLP64) | — | — | Core thesis | — | Core thesis | — | — | — |
| `include <llvm-c/Core.h>` FFI | — | — | Core thesis | — | Referenced | — | — | Core thesis |

### Phase Roadmaps Compared

| Doc | Phase Count | Duration | Key Differentiator |
|-----|-------------|----------|-------------------|
| 01-lexer-parser | 6 phases (P1-P6) | Not estimated | Feature-level phases (lexer, items, exprs, JSX, typecheck, codegen) |
| 02-typechecker | Implicit (4 pass pipeline) | Not estimated | Pipeline-stage phases |
| 04-cli-driver | 6 phases (0-5) | 12 weeks | DLL bridge, gradual replacement from outside in |
| 07-fusion | Implicit (markscript-first) | Not estimated | "Fuse then build core" strategy |
| SELFHOST-KN | 5 phases (1-5) | 12 weeks | Master phase plan with DLL bridge -> pure Kain -> ouroboros |

**Critical discrepancy:** Doc 04 and SELFHOST-KN agree on a 6-phase/5-phase plan with DLL bridge dependency. Doc 01 defines 6 DIFFERENT phases (purely feature-level: lexer, items, statements/expressions, JSX, typechecker, codegen/JIT). Doc 02 phases by pipeline stage. Doc 07 implicitly assumes markscript fusion happens FIRST, then the compiler core is built. **The phase plans are not aligned across documents.**

---

## 3. Source Files Referenced in crates/

### Compiler Core

| File | Size | Referenced By | Purpose |
|------|------|---------------|---------|
| `crates/core/src/lexer.rs` | 16KB | 01 | 102 token kinds, 58 hard keywords, Logos-powered lexer |
| `crates/core/src/ast.rs` | 62KB | 01 | 38 item variants, 64 expression variants, 14 types, 9 patterns |
| `crates/core/src/parser.rs` | 113KB | 01 | ~150 parse functions, Pratt engine, significant whitespace |
| `crates/core/src/types.rs` | 15,625 lines | 02 | Typechecker: 25 TypedItem variants, 20 ResolvedType variants |
| `crates/core/src/effects.rs` | 139 lines | 02 | Effect enum, 4 rules lattice |
| `crates/core/src/monomorphize.rs` | 2,078 lines | 02 | Generic instantiation, unify(), substitute_type() |
| `crates/core/src/runtime_contract.rs` | 3,330 lines | 05 | RuntimeContractBundle JSON emission |
| `crates/core/src/low_level_abi.rs` | ~300 lines | 03, 05 | C ABI policy table (LP64 vs LLP64) |
| `crates/core/src/error.rs` | — | 01 | KainError, KainResult, DiagnosticCode |
| `crates/core/src/span.rs` | — | 01 | Span, SpanMapper |

### Codegen

| File | Size | Referenced By | Purpose |
|------|------|---------------|---------|
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 21,289 lines | 03, 05 | Textual LLVM IR emitter (string builder) |
| `crates/sys-codegen/src/lib.rs` | — | 03 | Crate entry |
| `crates/core/src/low_level_memory.rs` | ~200 lines | 03 | Structural layout, bitfield packing |

### Driver & CLI

| File | Size | Referenced By | Purpose |
|------|------|---------------|---------|
| `crates/driver/src/lib.rs` | 5,370 lines | 04 | DriverSession, compile pipeline with caching |
| `crates/driver/src/llvm_ir.rs` | — | 03 | LLVM IR reachability analysis |
| `crates/cli/src/kain_launcher.rs` | 7,882 lines | 04 | CLI argument parsing, subcommand dispatch |
| `crates/cli/src/selfhost.rs` | 7,080 lines | 04 | Phase 1 Rust-to-Kain mirroring |
| `crates/cli/src/selfhost_bootstrap.rs` | 1,503 lines | 04 | Phase 2 roundtrip bootstrap |
| `crates/cli/src/llvm_native_stage.rs` | 19KB | 03, 04 | Native artifact staging, runtime contracts |
| `crates/blades/src/lib.rs` | 2,462 lines | 04 | Workspace discovery, blade resolution |

### FFI & Imports

| File | Size | Referenced By | Purpose |
|------|------|---------------|---------|
| `crates/c-ffi/src/` | 6,500 lines | 05 | libclang extraction, three-tier fallback |
| `crates/c-ffi/src/lib.rs` | — | 05 | INCLUDE_REGEX, resolve_library_spec() |
| `crates/c-ffi/src/extract.rs` | — | 05 | Three-tier extraction orchestration |
| `crates/c-ffi/src/libclang_extract.rs` | 551 lines | 05 | libclang AST walk |
| `crates/c-ffi/src/generate.rs` | 952 lines | 05 | .kn module output generation |

### Total crates/ footprint referenced: ~15 key files across 6 crate groups

Notable: Only ~15 of 67 crates are deeply referenced by the research documents. The remaining ~52 crates (GPU emitters, WASM, Python bridge, LSP, etc.) are mentioned in passing or not at all.

---

## 4. Kain Source Files Expected to Exist

### The Full Expected Tree (from SELFHOST-KN.MD + Doc 04)

```
blades/kain/
├── build.kn                     (project authority)
├── KAIN.toml                    (manifest)
├── src/
│   ├── main.kn                  (entry point - exists as Phase 0 starter)
│   ├── cli.kn                   (CLI argument parser - exists as Phase 0 starter)
│   ├── driver.kn                (compilation driver)
│   ├── workspace.kn             (workspace discovery)
│   ├── lexer.kn                 (character-by-character tokenizer)
│   ├── lexer_unicode.kn         (Unicode classification)
│   ├── literals.kn              (literal parsing)
│   ├── parser.kn                (recursive descent parser)
│   ├── pratt_parser.kn          (Pratt expression parser)
│   ├── ast.kn                   (AST node type definitions)
│   ├── types.kn                 (typechecker - 4 passes)
│   ├── effects.kn               (effect system: Pure, IO, Unsafe)
│   ├── monomorphize.kn          (generic instantiation)
│   ├── codegen.kn               (LLVM-C IR emission)
│   ├── jit.kn                   (OrcJIT execution)
│   ├── target.kn                (target initialization)
│   ├── optimizer.kn             (LLVM optimization passes)
│   ├── runtime.kn               (native runtime header imports)
│   ├── bridge.kn                (Rust DLL bridge for Phase 1-3)
│   ├── diagnostics.kn           (error reporting & formatting)
│   ├── import_c.kn              (C header import)
│   ├── import_python.kn         (Python import)
│   ├── import_rust.kn           (Rust crate import)
│   ├── modules.kn               (module resolution)
│   ├── selfhost.kn              (self-host pipeline orchestration, Phase 5)
│   ├── context.kn               (LLVM context management)
│   ├── platform.kn              (platform detection, Phase 5)
│   ├── orchestrator.kn          (markscript fusion, ~535 lines in Doc 07)
│   └── build_config.kn          (auto-generated by mks gen --target kain)
├── lib/
│   ├── intrinsics.kn            (self-host runtime intrinsics)
│   ├── collections.kn           (minimal Array/Map/Stack)
│   └── format.kn                (string formatting)
└── research/                    (8 existing docs + this synthesis)
```

**Total files:** 28 in `src/`, 3 in `lib/` = **31 expected Kain source files** for the self-host compiler core

**Currently existing (Phase 0):** `src/main.kn` and `src/cli.kn` -- only 2 of 31 files

The discrepancy between Doc 07 (which says build.md replaces build.kn and KAIN.toml) and SELFHOST-KN (which lists both build.kn and KAIN.toml) means the file manifest is still in flux.

---

## 5. TODO Sections and Acknowledged Gaps

### Documented TODOs

| Doc | Section | TODO Description | Severity |
|-----|---------|------------------|----------|
| 01 | §3.5 | `**` (power) is left-associative by implementation despite semantic expectation of right-associativity | Minor (bug) |
| 01 | §2.5 | 51 contextual keywords documented but no unified validation table | Medium |
| 02 | §3.4 | 6 Unsafe operations (mem_load, mem_store, ptr_offset, alloc, alloc_zeroed, realloc_mem) not enforced | **High (safety gap)** |
| 02 | §3.4 | `dispatch "key"` requires GPU effect but NOT enforced | Medium |
| 02 | §2 | Stub strategy for L1-L7 described at concept level, not fully specified | **High (arch gap)** |
| 03 | §2.3 | Path B OrcJIT has ZERO working Kain code | **Critical** |
| 04 | §1 | Phase timeline revised from 8 weeks to 12 weeks | Medium |
| 04 | §1 | Phase 0 CLI only has 5 subcommands; Rust CLI has 26+ | Medium |
| 05 | §5.10 | Runtime ABI table for JSON/Array/Map/String utilities is truncated | Low (incomplete doc) |
| 05 | §3.4 | Companion .c discovery edge cases not documented | Low |
| 06 | §3.1 | 3 of 23 markscript opcodes JIT-skipped | Low |
| 06 | §3.5 | markscript JIT uses RWX pages (not strict W^X) | Medium (security) |
| 06 | §5 | OrcJIT path has ZERO implementation | **Critical** |
| 07 | §2 | markscript fusion eliminates build.kn/KAIN.toml but SELFHOST-KN lists them as files | **High (contradiction)** |
| SH-KN | §18.4 | 18 corrections applied from v1.0 to v2.0 -- file counts, line counts, elimination rates all wrong in v1.0 | **Significant (v1 inaccuracy)** |
| SH-KN | §18.5 | 5 known uncertainties: C runtime file count, LLVM-C API surface, markscript line counts, phase estimate, keyword reference location | Medium |
| SH-KN | Appx D | Ouroboros bootstrap sequence document truncated | Low |

### Acknowledged Gaps (Not TODOs, but Explicitly Acknowledged)

| Doc | Gap | Why It's OK |
|-----|-----|-------------|
| 02 | Self-host doesn't typecheck L1-L7 | Compiler doesn't use these constructs; stubs suffice for parsing |
| 03 | Rust codegen is 21K lines of string formatting (not LLVM API) | This makes it EASIER to reimplement in Kain (string concat is native) |
| 04 | Full Rust CLI has 26+ subcommands | Only ~5 core subcommands needed for Phase 0-4; rest can stay in Rust DLL |
| 05 | Runtime contract not replicated in Kain | Compiler just emits LLVM `declare` -- runtime is C library, not Kain |
| SH-KN | 64% of Rust crates eliminated, not 93% | v2.0 correction: the earlier estimate was wrong; 36% of crates stay as runtime/stdlib bridges |

---

## 6. Cross-References Between Documents

### Explicit Cross-References

| Source Doc | References | Nature | Correct? |
|------------|-----------|--------|----------|
| 01 | SELFHOST-KN.MD | "Parent spec" in header | Yes -- it's the master spec |
| 02 | SELFHOST-KN.MD | Usage matrix says "verified against research docs 01-05" | **N/A** (doc 02 references SELFHOST-KN) |
| 03 | 06-jit-markscript-metal | "blades/kain/research/selfhost_jit_llvm_architecture.md" -- but actual doc 06 title is different | **Incorrect -- "selfhost_jit_llvm_architecture.md" doesn't exist; the doc is named "06-jit-markscript-metal-architecture.md"** |
| 03 | 07-markscript-fusion | References "blades/kain/research/selfhost_jit_llvm_architecture.md" again | **Same stale reference** |
| 04 | SELFHOST-KN.MD | "Reference: docs/BUILD_PROJECTS.MD, docs/RULEBOOK.md, docs/KEYWORDS.MD" | Correct |
| 05 | SELFHOST-KN.MD | "Parent" reference | Correct |
| 05 | SHATTER.MD, TELEPORT.MD, SYSTEMS_PROGRAMMING.MD | Source list | Correct |
| 06 | 03-llvm-codegen-jit.md | Source list | Correct |
| 06 | metal.kn, SYSTEMS_PROGRAMMING.MD | Source list | Correct |
| 07 | SELFHOST-KN.MD | "Appendix H defines the thesis; this document is the executable implementation" | **SELFHOST-KN Appendix H is the MarkScript fusion section; Doc 07 is indeed the expansion. Cross-reference correct.** |
| 07 | CHANGELOG.md, MARKSCRIPT.MD | "Related documents" | Correct |
| SH-KN | 01-05 | "verified against research docs 01-05" | Correct |
| SH-KN | 01-lexer-parser-ast.md | Listed in files consulted | Correct |
| SH-KN | 02-typechecker-types.md | Listed in files consulted | Correct |
| SH-KN | 03-llvm-codegen-jit.md | Listed in files consulted | Correct |
| SH-KN | 04-cli-driver-selfhost.md | Listed in files consulted | Correct |
| SH-KN | 05-runtime-contract-ffi.md | Listed in files consulted | Correct |

### Stale/Missing Cross-References

| Doc 03 references | Actual | Issue |
|-------------------|--------|-------|
| `selfhost_jit_llvm_architecture.md` | `06-jit-markscript-metal-architecture.md` | Doc 03 was written before doc 06 was finalized. The `selfhost_jit_llvm_architecture.md` filename appears to be either (a) a ghost file that doesn't exist, or (b) an earlier working title that was renamed to the current doc 06. |
| `blades/kain/reference/` | Same path | Correct |

### Cross-Document Agreement Table

| Topic | Docs That Agree | Docs That Disagree | Verdict |
|-------|----------------|-------------------|---------|
| Layer 0 only for bootstrap | 01, 02, 04, SH-KN | None | **Strong consensus** |
| Flat array AST | 01, 02, 03, SH-KN | None (06 is markscript-specific) | **Strong consensus** |
| 4-pass typecheck pipeline | 02, SH-KN | 04 mentions "pipeline" differently | **Agree** (04 at higher level) |
| LLVM-C FFI via include | 03, 05, 06, SH-KN | None | **Strong consensus** |
| MarkScript fusion | 07, SH-KN (Appx H) | 04 doesn't mention it at all | **Doc 04 gap** -- CLI driver research doesn't reference markscript |
| Phase timeline | 04=12wk, SH-KN=12wk | 01, 02, 07 not estimated | **Partial agreement** |
| Dependencies on markscript engine | 07 (core), SH-KN (appendix) | 01, 02, 03, 04, 05, 06 not mentioned | **Doc 07 is the only one that treats markscript as non-negotiable** |
| Rust crate elimination | SH-KN=64% | None dispute | **Authoritative source is SH-KN** |

---

## 7. Contradictions and Conflicts

### Contradiction 1: Build System Interface
- **Doc 07 (fusion):** "This document replaces build.kn, KAIN.toml, Makefile, CMakeLists.txt, package.json, custom test harness, custom REPL, custom watch mode, Rust cargo test, CI pipeline YAML, Bazel BUILD files -- ALL with markscript tables and intents."
- **SELFHOST-KN.MD:** Lists `build.kn` and `KAIN.toml` as files in the manifest (Section 3.2), describes `build.kn` as "project authority"
- **Doc 04:** Describes the Rust CLI's workspace resolution via `build.kn` and `KAIN.toml`, does not mention markscript replacement
- **Verdict:** **Contradiction.** Does the self-host compiler use `build.kn`/`KAIN.toml` (as SELFHOST-KN and 04 say) or `build.md`/markscript tables (as 07 says)? These are different file formats with different semantics. The project authority file is unresolved.

### Contradiction 2: Build Eliminates Bazel vs. Bazel Builds the Compiler
- **SH-KN §13 (Bazel Death Plan):** Build files -> empty -> removed. The compiler IS its own build system.
- **SH-KN §18 (Verified):** References `kain_bazel`, `bazel build`, Bazel server in the same document
- **Reality:** The current development workflow REQUIRES Bazel to build the Rust bootstrap compiler that the Kain self-host compiler depends on. The Bazel Death Plan is Phase 5+ aspirational, not current reality.
- **Verdict:** **Tension, not contradiction.** SH-KN acknowledges Bazel is needed NOW but aims to eliminate it. The Bazel Death Plan is a roadmap, not a current state claim. But no document clearly states "Bazel is needed for Phases 0-4 and eliminated in Phase 5."

### Contradiction 3: Phase Plans Don't Align
- **Doc 01:** 6 phases (P1-P6) based on feature depth (lexer, items, exprs, JSX, typecheck, codegen)
- **Doc 04:** 6 phases (Phase 0-5) based on replacement depth (starter, DLL bridge, Kain parser, Kain typechecker, Kain codegen, pure Kain)
- **SELFHOST-KN:** 5 phases (Phase 1-5) based on pipeline depth
- **Verdict:** **Misaligned numbering.** Doc 04 Phase 2 = "Kain parser" = Doc 01 P1-P4. Doc 04 Phase 4 = "Kain codegen" = Doc 01 P6. The same progression is described differently. This is a naming/numbering issue, not a semantic contradiction, but it WILL cause confusion when agents reference "Phase 2."

### Contradiction 4: CLI Dispatch Philosophy
- **Doc 04:** Describes CLI dispatch directly via `match cfg.subcommand` in `main.kn` with `CliConfig struct`, `parse_args()`, `if cfg.subcommand == SUBCMD_RUN:` chains
- **Doc 07:** Describes CLI dispatch through markscript IVT handlers: `mks_register(vm, "compile check", HANDLER_COMPILE_CHECK)` then `markscript.mks_run_file("build.md")`
- **Verdict:** **Different architectural approaches.** Doc 04 uses a traditional CLI pattern (parse args, match dispatch, call functions). Doc 07 uses the markscript embedding pattern (register handlers in VM, run markscript files, intents dispatch to handlers). These are mutually exclusive design choices for the same component.

### Contradiction 5: Self-Host Compiler Size Claims
- **Doc 01:** ~7,500 lines for lexer+parser+AST+typecheck+codegen+JIT+compiler driver
- **Doc 07:** ~12,500 lines compiler core + ~500 lines orchestrator = ~13,000 total
- **SELFHOST-KN:** ~14,350 lines total (Phase 1-4) + ~2,000 (Phase 5) = ~16,350 aspirational
- **Verdict:** **Partial disagreement.** Doc 01 omits CLI, workspace, bridge, diagnostics, imports, and modules from its count. Doc 07 includes orchestrator but counts markscript engine separately. SELFHOST-KN is the most comprehensive. Doc 01's "~7,500" is an undercount -- it doesn't include the 17+ other files.

### Contradiction 6: markscript as Dependency
- **Doc 07:** "0 lines of new code" for build system, config, test runner, REPL, watch mode -- uses existing markscript engine
- **Reality:** The markscript engine is 7,500 lines of Kain code that must be compiled and linked. While it's "existing code," it IS a dependency with its own maintenance burden, testing requirements, and compatibility surface.
- **Verdict:** **Tension, not contradiction.** Doc 07's claim of "0 new lines" is technically correct for the infrastructure subsystems but potentially misleading about the total complexity.

---

## 8. Coverage Gaps

### Compiler Subsystems with NO Research Document

| Missing Subsystem | Where It's Mentioned | Why It's a Gap |
|-------------------|---------------------|----------------|
| **GPU/Shader compilation** | Doc 04 (subcommands), Doc 05 (runtime ABI) | SPIR-V, PTX, HLSL, WGSL emission are 4 of 17 compile targets. No research on how the self-host compiler would handle shader items. |
| **WASM/JS/TS codegen** | Doc 04 (compilation targets) | WASM, JS, TS, Hybrid are 4 of 17 compile targets. No coverage. |
| **Python interop** | Doc 04 (subcommands), Doc 05 (runtime ABI) | `import python_module`, `from module import name`, Python bridge runtime functions -- no standalone research. |
| **C header import pipeline** | Doc 05 (section 3) | Covered within doc 05 but no standalone research. The libclang extraction pipeline is one of the most complex subsystems. |
| **LSP/editor protocol** | Doc 04 (subcommand) | The `kain lsp` subcommand is a full tokio-based LSP server. No research on how to self-host this. |
| **Error formatting/repair** | Doc 04 (subcommands: repair, doctor) | The `kain repair` and `kain doctor` subsystems are mentioned but have zero research coverage. |
| **Formatter** | Doc 04 (subcommand: format/fmt) | `kain fmt` with `--check` and `--write` modes. No research on how Kain formats itself. |
| **Package management** | Doc 04 (subcommands: packages init/add/install/publish, registry) | Full package management with registry. No research. |
| **Amalgamator** | Doc 04 (subcommand: amalgamate) | Pack/unpack/inspect capsules. No research. |
| **Self-host bootstrap pipeline** | Doc 04 (selfhost phase1/phase2), SH-KN (various) | The Rust-to-Kain mirroring and roundtrip bootstrap pipeline is described operationally but has no dedicated research document. |
| **Ouroboros verification** | SH-KN (Appendix D, truncated) | The circular bootstrap verification is the end goal but has almost no design research. |
| **Blades workspace resolution** | Doc 04 (kain_blades crate) | How `blades/` directories become compilable workspaces. Referenced but not researched. |
| **LLVM optimization pass pipeline** | Doc 03 (optimizer.kn mentioned), SH-KN (optimizer.kn listed) | Only ~200 lines estimated but LLVM's optimization pipeline is deep. No research on which passes to run. |

### Summary of Coverage

| Area | Doc | Coverage Depth |
|------|-----|---------------|
| Lexer/Parser/AST | 01 | **Deep** -- all token kinds, item/expr/stmt variants, precedence, JSX |
| Typechecker | 02 | **Deep** -- all 20 ResolvedType variants, 4-pass pipeline, effects, monomorphization |
| LLVM codegen | 03 | **Deep** -- type mapping, emission patterns, ABI policy, runtime declares |
| CLI architecture | 04 | **Deep** -- all 26+ subcommands, DriverSession pipeline, error flow |
| Runtime contract | 05 | **Deep** -- all ABI tables, @extern contract, libclang pipeline, service table |
| JIT execution | 06 | **Deep** -- markscript JIT (proven), OrcJIT (speculative), metal primitives |
| MarkScript fusion | 07 | **Deep** -- embedding API, 9 handlers, build config schema, orchestrator |
| SELFHOST-KN spec | SH-KN | **Deep** -- master spec, file manifest, usage matrix, death list, phase plan |
| GPU codegen | (none) | **No coverage** -- mentioned in passing, no research |
| WASM/JS/TS codegen | (none) | **No coverage** |
| Python interop | (none) | **No coverage** -- runtime ABI listed but no architecture |
| LSP | (none) | **No coverage** |
| Formatter | (none) | **No coverage** |
| Package mgmt | (none) | **No coverage** |
| Amalgamator | (none) | **No coverage** |
| Self-host bootstrap | (partial) | **Partial** -- in doc 04 and SH-KN but no separate research |
| Ouroboros proof | (none) | **No coverage** -- end goal, no design |

---

## 9. Alignment with SELFHOST-KN.MD

### Documents That Align Well

| Doc | Alignment | Notes |
|-----|-----------|-------|
| 01 (lexer/parser/AST) | **Strong** | Deeply sourced from the spec's architecture. Exercise 40% surface claim matches spec's Phase 1-4 usage matrix. |
| 02 (typechecker) | **Strong** | Layer 0 only approach, 4-pass pipeline, effect checking all align with spec. The "stub strategy" is directly from the spec. |
| 03 (LLVM codegen) | **Strong** | Path A/B both described in spec. LLVM-C FFI via `include` is core to spec. Type mapping aligns. |
| 04 (CLI/driver) | **Strong** | Phase roadmap matches spec (DLL bridge -> progressive replacement). Command hierarchy matches Rust CLI. |
| 05 (runtime contract) | **Strong** | "Compiler doesn't replicate runtime" thesis aligns perfectly. @extern contract, ABI tables, service table all from spec. |
| 06 (JIT execution) | **Strong** | markscript JIT + OrcJIT dual path is in spec. W^X lifecycle aligns. Metal primitives prove spec's claims. |

### Documents With Tensions

| Doc | Alignment | Tension |
|-----|-----------|---------|
| 07 (markscript fusion) | **Partial** | Two tensions: (1) Doc 07 claims markscript replaces build.kn/KAIN.toml; spec lists them as files. (2) Doc 07 puts markscript at the center of orchestration; the spec Appendix H describes fusion as optional/aspirational. |
| SELFHOST-KN v1.0 -> v2.0 corrections | **N/A** | The spec v1.0 had significant errors (wrong file counts, line counts, elimination rates). v2.0 corrected these. The research docs were written against v2.0 but some inherit v1.0 assumptions. |

### Construct Count Alignment

| Source | Constructs Used | Count |
|--------|----------------|-------|
| Doc 01 claim | ~40% of full surface (25/38 items, 45/64 exprs) | ~25 items |
| Doc 02 claim | ~10% of typechecker (Layer 0 only) | ~16 constructs |
| Doc 04 (implicit) | Layer 0 + bridge + include | ~16 constructs |
| SH-KN §2.3 | Exactly 14 constructs listed | 14 constructs |
| SH-KN §5.1 | 16 constructs in usage matrix (fn, struct, enum, trait, impl, const, include, ptr, collapse/observe/decay, asm, defer, use, let, if/elif/else, for, while, match) | 16 constructs |

**Verdict:** The actual self-host construct count is 14-16 (all Layer 0), aligning closely with the spec's claim of 14. Doc 01's "40% of full surface" refers to parser complexity, not construct count. **All documents align on the core thesis of Layer 0 only.**

---

## 10. Overall Research Quality Assessment

### Strengths

1. **Exceptional depth:** Each document goes far beyond surface-level description. Doc 01 lists every single TokenKind variant (102 of them). Doc 02 lists every type compatibility rule. Doc 03 shows exact LLVM IR output for each codegen pattern. Doc 04 lists all 26+ CLI subcommands. Doc 05 documents ~80 runtime functions with exact signatures. Doc 06 shows bytecode-level JIT emission.

2. **Source-anchored:** Every document traces claims back to specific source files in `crates/`. Doc 01 cites exact line ranges in lexer.rs, parser.rs, ast.rs. Doc 02 gives exact line counts (15,625 lines in types.rs). Doc 03 references specific codegen functions. This makes claims verifiable.

3. **Cross-references:** Documents actively cite each other and the master spec. The reference chains are mostly consistent.

4. **Honest about gaps:** Doc 02 explicitly lists which Unsafe operations are NOT enforced. Doc 06 admits the OrcJIT path has zero implementation. SH-KN documents 18 corrections from v1.0 to v2.0. The `orchestrator.kn` in Doc 07 is described as "not yet implemented."

5. **Architectural maturity:** The documents don't just describe WHAT exists; they explain WHY the architecture works. The "Layer 0 only" thesis is justified by circular dependency reasoning. The flat array AST is justified by cache locality and LLVM mapping. The stubs strategy is justified by incrementality.

### Weaknesses

1. **Stale/inconsistent cross-reference in Doc 03:** References `selfhost_jit_llvm_architecture.md` which doesn't exist (the file is named `06-jit-markscript-metal-architecture.md`). This is a bug.

2. **Doc 07 unaligned with rest of research:** The markscript fusion contract is written as if it's the final authority ("All future compiler orchestration work references this document as canonical truth"), but the other research docs (especially 04) don't assume markscript fusion. Doc 07 reads like a standalone manifesto rather than a collaborating research document.

3. **Doc 01-05 inconsistent phase numbering:** Doc 01 uses P1-P6 (feature depth). Doc 04 uses Phase 0-5 (replacement depth). SH-KN uses Phase 1-5. If an agent is told "implement Phase 2," which set of phases do they follow?

4. **Underestimation risk for codegen complexity:** Doc 03 estimates codegen.kn at 2,000-3,000 lines. The Rust equivalent is 21,289 lines. While Kain-to-LLVM-C API calls ARE more concise than string formatting, the LLVM-C API is complex. 3,000 lines may be an underestimate for a production-quality emitter.

5. **Underestimation risk for typechecker:** Doc 02 estimates 1,500-2,000 lines for the self-host typechecker. The Rust types.rs is 15,625 lines. While the self-host only needs ~10% of the constructs, the effect checking alone requires 700+ lines in Rust. 1,500-2,000 lines may be tight.

6. **No risk analysis for the DLL bridge:** Doc 04 and SH-KN describe the Rust DLL bridge (Phase 1) as the transitional strategy. But making the Rust compiler a C-ABI DLL (`cdylib`) is a non-trivial engineering task -- the Rust compiler has no existing C API surface. This risk is unaddressed.

7. **No contingency for markscript as a dependency:** Documents treat markscript as a solved problem. If the markscript engine changes its embedding API or has bugs, the self-host compiler is blocked. No fallback strategy is described.

8. **File manifest disagreements:** Doc 07's file list differs from SH-KN's. Doc 07 has `orchestrator.kn` and `build_config.kn` (auto-generated). SH-KN has `platform.kn` and `selfhost.kn`. Neither lists `pratt_parser.kn` or `lexer_unicode.kn` from Doc 01. **The total expected file count varies from 25 (SH-KN) to 31 (this synthesis) to 36+ (if all Doc 01 files are included).**

### Scoring

| Criteria | Score (1-10) | Notes |
|----------|-------------|-------|
| Depth | 9 | Exceptional detail on covered topics |
| Source grounding | 9 | Every claim traceable to source files |
| Cross-references | 7 | Mostly consistent, but Doc 03 has stale references |
| Gaps acknowledged | 8 | Most gaps documented, some understated |
| Actionability | 7 | Implementation-ready for covered topics, ambiguous for phase numbering |
| Consistency | 6 | Phase numbering, file manifests, markscript role disagree |
| Completeness | 5 | ~12 of ~23 compiler subsystems covered (50% gap) |
| **Overall** | **7.3/10** | Excellent depth where it covers, significant coverage gaps |

---

## 11. Phase Plan Breakdown

### Consolidated Phase Map

| Phase | Doc 01 | Doc 04 | SELFHOST-KN | What Exists | What Gets Built |
|-------|--------|--------|-------------|-------------|-----------------|
| 0 | — | Phase 0 | — | `main.kn` + `cli.kn` (5 subcommands) | Starter CLI |
| 1 | P1 (Lexer ~500L) | Phase 1 | Phase 1 | Nothing for lexer | Rust DLL bridge |
| 2 | P2-P4 (Items+Exprs+JSX ~3,500L) | Phase 2 | Phase 2 | Nothing | Kain lexer + parser |
| 3 | P5 (Typecheck ~1,500L) | Phase 3 | Phase 3 | Nothing | Kain typechecker |
| 4 | P6 (Codegen+JIT ~2,000L) | Phase 4 | Phase 4 | Nothing | Kain codegen (LLVM-C FFI) |
| 5 | — | Phase 5 | Phase 5 | Nothing | Pure Kain compiler, ouroboros |

### What Exists NOW (Phase 0)

- `blades/kain/src/main.kn` (~150 lines) -- CLI entry point
- `blades/kain/src/cli.kn` (~200 lines) -- CLI argument parser
- `blades/kain/KAIN.toml` -- manifest
- `blades/kain/build.kn` -- project authority

### What Needs to Be Built (Phases 1-5)

**Phase 1** (no Kain code needed -- configured existing Rust CLI bridge):
- Rust `cdylib` compilation from bootstrap crates
- `bridge.kn` -- FFI declarations for DLL functions
- `driver.kn` -- pipeline orchestration calling Rust DLL

**Phase 2** (~4,500 lines Kain):
- `lexer.kn`, `lexer_unicode.kn`, `literals.kn` (~1,100 lines)
- `ast.kn` (~500 lines)
- `parser.kn`, `pratt_parser.kn` (~3,500 lines)
- `modules.kn` (~300 lines)

**Phase 3** (~2,700 lines Kain):
- `types.kn` (~2,000 lines)
- `effects.kn` (~200 lines)
- `monomorphize.kn` (~500 lines)
- `diagnostics.kn` (~300 lines)
- `import_c.kn`, `import_python.kn`, `import_rust.kn` (~700 lines total)

**Phase 4** (~3,500 lines Kain):
- `codegen.kn` (~3,000 lines) -- the big one
- `jit.kn` (~300 lines)
- `target.kn` (~100 lines)
- `optimizer.kn` (~200 lines)
- `runtime.kn` (~200 lines)
- `context.kn` (~100 lines)

**Phase 5** (~2,500 lines Kain):
- `selfhost.kn` (~500 lines) -- orchestrate-based pipeline
- `platform.kn` (~100 lines)
- `orchestrator.kn` (~500 lines) -- markscript fusion
- `build_config.kn` (auto-generated)
- Stdlib mirror expansion (~1,400 lines)

### Critical Dependencies

1. **Phase 1 depends on:** Rust bootstrap compiling as `cdylib` -- UNVERIFIED
2. **Phase 2 depends on:** Phase 1 bridge working for remaining pipeline -- STATED BUT NOT VERIFIED
3. **Phase 4 depends on:** LLVM-C headers being parseable by libclang -- LIKELY BUT UNVERIFIED
4. **Phase 5 depends on:** All previous phases working correctly -- OBVIOUS
5. **Ouroboros depends on:** Phase 5 compiler producing identical output to the compiler that compiled it -- THE ENTIRE POINT

---

## 12. Critical Open Questions

### Technical Questions

1. **Can the Rust compiler be compiled as a `cdylib`?** The Rust bootstrap is 67 crates with complex dependency graph (tokio, clap, inkwell, etc.). Making this a C-compatible DLL with a stable ABI is non-trivial. No document addresses this risk.

2. **Does `include <llvm-c/Core.h> as llvm` actually work?** Doc 03 asserts this is the core mechanism. The doc references the existing `include <windows.h>` (605 functions) and `include <vulkan/vulkan.h>` (755 functions) as proof. But LLVM-C headers may have different characteristics (typedef patterns, inline functions, macros). This needs empirical verification.

3. **How does OrcJIT Path B work when there's zero Kain code for it?** Doc 06 says "ZERO working Kain code" for the OrcJIT path. Doc 07 assumes it works. The entire dual-JIT architecture depends on this.

4. **What happens if markscript changes its API?** The `std::markscript` embedding API is the sole integration surface. If markscript evolves (new opcodes, changed function signatures, different VM behavior), the self-host compiler breaks. No compatibility contract is documented.

5. **How does the flat Array<AstNode> self-host?** The self-host compiler is itself written in Kain. The flat array AST (Array<AstNode>) is a Kain Array, not a Rust Vec. The Kain compiler must allocate, grow, index, and traverse these arrays efficiently. Kain's Array must be capable of holding ~50,000+ nodes for a large Kain file. Is this proven?

### Architectural Questions

6. **Is markscript fusion a hard dependency or optional?** Doc 07 presents it as non-negotiable ("Markscript IS the compiler's orchestration layer"). Doc 04 describes CLI dispatch without markscript. SELFHOST-KN puts fusion in Appendix H (optional appendix). Which is correct?

7. **Does the self-host compiler use build.md or build.kn?** Doc 07 says build.md (markscript tables). SELFHOST-KN says build.kn. These are different file formats. The project authority file determines what `kainc build .` discovers.

8. **Who owns the phase number definitions?** If one agent reads "Phase 2" from Doc 01 (P2 = items parsing) and another reads "Phase 2" from Doc 04 (Phase 2 = Kain parser), they'll build different things. The phase numbering MUST be unified before implementation begins.

9. **What is the minimum viable self-host?** The docs describe a ~14,350-line compiler. But is there a smaller, testable milestone? What's the smallest Kain compiler that can compile a single arithmetic expression to LLVM IR? This MVP is not defined.

10. **When does the Rust bootstrap stop being used?** The Bazel Death Plan says Phase 5. But Phase 1-4 still depend on the Rust DLL. Is there a point where the Kain compiler can compile a subset of itself while still using the Rust DLL for the rest? "Partial self-hosting" is not addressed.

### Death List Questions (from SH-KN §12)

11. **43 of 67 crates eliminated -- which 24 stay?** The death list says 64% are eliminated (43 crates). The 24 crates that stay include: stdlib implementations, GPU runtime, Python runtime, UE5 codegen, WASM emitter, LSP, error/semantic intelligence, file watcher, platform init, host bridge, omni/fabric manifest, repair, selfhost, package manager, registry, amalgamator. These are NOT eliminated -- they remain as Rust crates or are migrated later. **The 64% elimination figure counts lines of code, not crates, and the remaining 36% includes many complex subsystems.**

---

## Appendix: Document Metadata

| # | Document | File Size | Est. Lines | Status | Primary Author |
|---|----------|-----------|------------|--------|---------------|
| 01 | 01-lexer-parser-ast.md | 60,800 | ~1,280 | Complete | Agent 1 |
| 02 | 02-typechecker-types.md | 78,465 | ~1,590 | Complete | Agent 2 |
| 03 | 03-llvm-codegen-jit.md | 86,817 | ~1,800 | Complete | Agent 3 |
| 04 | 04-cli-driver-selfhost.md | 70,080 | ~1,397 | Complete | Agent 4 |
| 05 | 05-runtime-contract-ffi.md | 76,764 | ~1,483 | Complete | Agent 5 |
| 06 | 06-jit-markscript-metal-architecture.md | 53,584 | ~1,155 | Complete | (unlabeled) |
| 07 | 07-markscript-fusion-contract.md | 83,698 | ~2,000+ | Complete | (unlabeled) |
| SH-KN | SELFHOST-KN.MD | 87,143 | ~1,727 | v2.0 Master | Architect |

---

*End of 00-AGENT_SYNTHESIS.md -- 538 lines*
*Generated by kain-explorer research agent, 2026-06-12*
