# Requirements: Kain Self-Host Compiler (kainc)

**Phase:** 1 of 3 — Requirements  
**Created:** 2026-06-12  
**Status:** Draft  
**Next:** /spec/design.md (Phase 2 — Design Agent)  
**Source documents:** 01–07 research specs in `blades/kain/research/`, `SELFHOST-KN.MD`, `docs/RULEBOOK.md`, `docs/STDLIB.md`

---

## Overview

The Kain Self-Host Compiler (kainc) is a **pure-Kain, LLVM-native compiler that can compile itself**. It replaces the Rust bootstrap compiler (~67 crates, ~519K lines) with an approximately 13,000-line Kain compiler organized into 7 independently buildable subsystems. The compiler emits LLVM IR (Path A: textual `.ll` or Path B: LLVM-C API OrcJIT) and links against the existing 47-file native C runtime (`kain_runtime.lib`). The ultimate acceptance criterion is **ouroboros** — kainc compiles its own source code to produce a byte-identical binary, proving the compiler is semantically complete.

The compiler is written in **Layer 0 Kain** (fn, struct, enum, trait, impl, ptr, collapse/observe/decay, Pure/Unsafe/IO effects) with a small Unsafe bridge to the LLVM-C API. The MarkScript VM serves as the compiler's orchestration layer for build, config, test, REPL, and CI — eliminating ~3,000 lines of infrastructure code.

---

## Stakeholders & User Roles

| Role | Description | Impact |
|------|-------------|--------|
| **Kain Developer** | Writes and compiles Kain source files using `kainc` | Primary user; needs fast compile-check-run loop, clear diagnostics, all current bootstrap features |
| **Compiler Contributor** | Extends or maintains the self-host compiler itself | Secondary user; needs readable ~13K-line codebase, stub strategy for Layers 1–7, clear module boundaries |
| **Bootstrap Compiler (Rust)** | The existing Rust compiler that initially compiles kainc | System actor; provides the bridge to bootstrap the self-host, then becomes obsolete |
| **LLVM/Clang Toolchain** | Provides LLVM-C library, OrcJIT, linker, and C ABI compilation | External system dependency; kainc links against libLLVM dynamically |
| **Kain Runtime (C)** | The 47-file native C runtime (`kain_runtime.lib`) that kainc's output links against | External system dependency; stable ABI contract |
| **MarkScript VM** | The embedded VM that provides build, config, test, REPL orchestration | System actor; fused into the compiler as its orchestration layer |
| **Downstream Tooling** | LSP server, IDE plugins, CI systems that consume kainc's JSON diagnostics | Secondary user; needs structured output formats (JSON, machine-readable diagnostics) |

---

## User Stories

### Story 1: Compile a Kain Source File to LLVM IR

**As a** Kain Developer,  
**I want** to run `kainc build src/main.kn --target llvm` and get a compiled executable,  
**So that** I can develop Kain programs end-to-end without the Rust bootstrap.

**Acceptance Criteria:**
1. WHEN a valid `.kn` source file is provided AND the target is `llvm` THEN system SHALL lex, parse, typecheck, monomorphize, and emit valid LLVM IR that links against `kain_runtime.lib`
2. WHEN the LLVM IR is compiled by clang AND linked THEN the resulting binary SHALL produce the same output as the Rust bootstrap compiler for the same source
3. IF the source file contains no errors THEN the compiler SHALL exit with code 0 and produce no stderr output
4. IF the source file contains type errors THEN the compiler SHALL exit with code 1 and emit diagnostics to stderr with filename, line, column, and error message

### Story 2: Typecheck Without Code Generation

**As a** Kain Developer,  
**I want** to run `kainc check src/` to validate types across all source files without waiting for codegen,  
**So that** I can get fast feedback during development.

**Acceptance Criteria:**
1. WHEN `kainc check <path>` is invoked THEN system SHALL lex, parse, and typecheck all `.kn` files in the path WITHOUT running monomorphization or codegen
2. WHEN typechecking completes THEN system SHALL report the count of files checked and total error count
3. WHEN `--json` flag is provided THEN system SHALL output structured JSON diagnostics with span information
4. WHERE the check operation SHALL complete in under 500ms for the compiler's own ~13K-line source tree on a modern x86-64 machine

### Story 3: JIT Execute Kain Code In-Memory

**As a** Kain Developer,  
**I want** to run `kainc run src/main.kn` and have the code JIT-compiled and executed in-memory,  
**So that** I can iterate without producing intermediate object files.

**Acceptance Criteria:**
1. WHEN `kainc run <path>` is invoked THEN system SHALL compile the source to LLVM IR, JIT-compile via OrcJIT, and execute the entry function
2. WHEN the JIT execution completes THEN system SHALL return the exit code of the compiled program
3. WHEN the OrcJIT path is unavailable (no LLVM DLL) THEN system SHALL fall back to the markscript-style x86-64 direct emission Path A for supported platforms
4. WHERE JIT execution overhead (compilation + execution) SHALL be under 1 second for a 100-line Kain program on first run

### Story 4: Self-Host Ouroboros Verification

**As a** Compiler Contributor,  
**I want** to run `kainc selfhost --verify-ouroboros` and have the compiler compile its own source,  
**So that** I can prove the compiler is semantically complete and correct.

**Acceptance Criteria:**
1. WHEN `kainc selfhost --verify-ouroboros` is invoked THEN system SHALL compile the combined compiler source to produce a stage-1 binary
2. WHEN the stage-1 binary is produced THEN system SHALL use it to compile the same source to produce a stage-2 binary
3. WHEN both binaries are produced THEN system SHALL compare them byte-for-byte and report identity or the byte offset of first divergence
4. WHEN the ouroboros verification passes THEN system SHALL exit with code 0 and print "OUROBOROS VERIFIED"
5. WHEN the ouroboros verification fails THEN system SHALL exit with code 1 and report the diff location

### Story 5: Workspace Discovery and Modular Compilation

**As a** Kain Developer,  
**I want** to point `kainc` at a directory and have it discover `KAIN.toml`, `build.kn`, and all source files automatically,  
**So that** I don't need to manually list every file on the command line.

**Acceptance Criteria:**
1. WHEN `kainc build .` is invoked in a directory THEN system SHALL ascend to find the workspace root (first ancestor containing `KAIN.toml`, `kain.toml`, `build.kn`, `platform.kn`, or `.git`)
2. WHEN a workspace root is found THEN system SHALL discover all `.kn` source files under the configured source roots
3. WHEN `use std::module` imports are encountered THEN system SHALL resolve stdlib module paths from the configured stdlib root
4. WHEN `use local::module` imports are encountered THEN system SHALL resolve filesystem-relative module paths

### Story 6: CI-Ready Structured Diagnostics

**As a** Downstream Tooling (LSP, CI),  
**I want** the compiler to produce structured JSON diagnostics with precise span information,  
**So that** I can integrate compiler output into editors and CI pipelines.

**Acceptance Criteria:**
1. WHEN `--json` flag is provided THEN system SHALL output a JSON array of diagnostic objects, each containing: `severity`, `file_path`, `line`, `column`, `message`, `error_kind`, `span_start`, `span_end`
2. WHEN `--json-out <path>` flag is provided THEN system SHALL write the JSON report to the specified file path
3. WHEN compilation fails THEN system SHALL still produce the JSON report containing all accumulated errors (up to MAX_ERRORS=50)

---

## Functional Requirements

The functional requirements are organized by **7 independent subsystems** that can be built in parallel. Each subsystem is identified by a prefix and can be assigned to a separate development stream.

---

### FR-LEX: Lexer Subsystem

> **Research source:** `01-lexer-parser-ast.md` §§2.1–2.6, §2.4 indent processor  
> **Target files:** `lexer.kn`, `token.kn`, `span.kn`  
> **Parallelizable with:** FR-PARSE, FR-JIT, FR-RUNTIME, FR-ORCH (with shared token/span types)

- **FR-LEX.1:** WHEN a Kain source string is provided to the lexer THEN the lexer SHALL produce an `Array<Token>` where each token has `kind: TokenKind`, `text: String`, `line_no: Int`, `col_no: Int`, and `byte_offset: Int`
- **FR-LEX.2:** WHEN the lexer encounters any of the 58 hard keywords (fn, let, mut, if, else, struct, enum, etc.) THEN the lexer SHALL produce the corresponding `TokenKind` variant regardless of syntactic position
- **FR-LEX.3:** WHEN the lexer encounters a contextual keyword (Ident string matching "patch", "law", "world", "converge", etc.) THEN the lexer SHALL produce `TokenKind::Ident(name)` and NOT a dedicated keyword token
- **FR-LEX.4:** WHEN the lexer encounters a `//` or `#` comment THEN the lexer SHALL skip all characters until the next newline or EOF
- **FR-LEX.5:** WHEN the lexer encounters integer literals (decimal, hex `0x`, octal `0o`, binary `0b`) THEN the lexer SHALL parse the value and produce `TokenKind::Int(value)` with underscore separators stripped
- **FR-LEX.6:** WHEN the lexer encounters float literals THEN the lexer SHALL produce `TokenKind::Float(value)`
- **FR-LEX.7:** WHEN the lexer encounters string literals (double-quoted, with escape sequences) THEN the lexer SHALL produce `TokenKind::String(value)` with escape sequences resolved
- **FR-LEX.8:** WHEN the lexer encounters `f"..."` format strings THEN the lexer SHALL produce `TokenKind::FString(raw_text)` — brace parsing deferred to parser
- **FR-LEX.9:** WHEN the lexer encounters `'c'` character literals THEN the lexer SHALL produce `TokenKind::Char(value)` with escape sequences resolved
- **FR-LEX.10:** WHEN the lexer encounters any of the 25 operator symbols (`+`, `-`, `*`, `/`, `%`, `**`, `==`, `!=`, `<`, `>`, `<=`, `>=`, `&&`, `||`, `!`, `&`, `|`, `^`, `~`, `<<`, `>>`, `++`, `--`, `and`, `or`) THEN the lexer SHALL produce the corresponding `TokenKind` variant
- **FR-LEX.11:** WHEN the lexer encounters compound assignment operators (`+=`, `-=`, `*=`, etc.) THEN the lexer SHALL produce the corresponding `TokenKind` variant
- **FR-LEX.12:** WHEN the lexer encounters punctuation (`(`, `)`, `[`, `]`, `{`, `}`, `,`, `.`, `..`, `...`, `:`, `::`, `;`, `->`, `=>`, `@`, `??`, `?.`, `?`, `</`) THEN the lexer SHALL produce the corresponding `TokenKind` variant
- **FR-LEX.13:** WHEN the lexer encounters a newline character followed by whitespace THEN the lexer SHALL produce `TokenKind::Newline(whitespace_string)` capturing the full whitespace after the newline
- **FR-LEX.14:** WHEN the lexer encounters an unrecognized character THEN the lexer SHALL produce a diagnostic error with the character and position
- **FR-LEX.15:** WHERE the lexer SHALL recognize exactly 102 token kinds — 58 hard keywords, 44 non-keyword token kinds
- **FR-LEX.16:** WHEN the indent processor receives a token array THEN it SHALL insert synthetic `Indent` and `Dedent` tokens based on indentation level changes, and append `Eof` at the end

#### FR-LEX: Indent Processor (Post-Lexer Pass)

- **FR-LEX.17:** WHEN a newline occurs inside balanced brackets (`()`, `[]`, `{}`) THEN the indent processor SHALL suppress insertion of INDENT/DEDENT/NEWLINE tokens
- **FR-LEX.18:** WHEN a newline is immediately followed by another newline (blank line) THEN the indent processor SHALL discard both newlines
- **FR-LEX.19:** WHEN indentation increases relative to the current level THEN the indent processor SHALL emit `Indent` and push the new level onto the indent stack
- **FR-LEX.20:** WHEN indentation decreases relative to the current level THEN the indent processor SHALL pop the stack and emit one `Dedent` per level unwound
- **FR-LEX.21:** WHEN any tab character is encountered in whitespace THEN the indent processor SHALL treat it as 4 spaces for indent calculation
- **FR-LEX.22:** WHEN EOF is reached THEN the indent processor SHALL emit `Dedent` for each remaining indent level and append `Eof`

---

### FR-PARSE: Parser & AST Subsystem

> **Research source:** `01-lexer-parser-ast.md` §§3.1–3.7, §4.1–4.8  
> **Target files:** `parser.kn`, `ast.kn`  
> **Parallelizable with:** FR-LEX, FR-TYPE, FR-JIT (shared AST type definitions)

#### FR-PARSE: AST Representation

- **FR-PARSE.1:** WHEN the parser builds the AST THEN the AST SHALL use a flat `Array<AstNode>` representation where parent-child relationships are expressed via integer indices, NOT recursive references
- **FR-PARSE.2:** WHERE the `AstNode` struct SHALL contain: `kind: Int` (tag discriminant), `span_start: Int`, `span_end: Int`, and `data: Array<Int>` (variable-length payload of child indices, literal values, and flags)
- **FR-PARSE.3:** WHERE the AST SHALL support all 38 `Item` kinds, 12 `Stmt` kinds, 64 `Expr` kinds, 9 `Pattern` kinds, 14 `Type` kinds, 21 `BinaryOp` kinds, and 6 `UnaryOp` kinds

#### FR-PARSE: Top-Level Parsing

- **FR-PARSE.4:** WHEN `parse()` is invoked THEN the parser SHALL iterate tokens at indent depth 0, dispatching to `parse_item()` for keyword-started constructs and `parse_stmt()` for everything else
- **FR-PARSE.5:** WHEN top-level statements (not items) are encountered THEN the parser SHALL wrap them in an implicit `pub fn main() { ... }` item
- **FR-PARSE.6:** WHEN `pub`, `@attr`, or a keyword starting an item is encountered THEN the parser SHALL invoke `parse_item()` to produce the corresponding `Item` variant

#### FR-PARSE: Item Parsing

- **FR-PARSE.7:** WHEN `fn` keyword is encountered THEN parser SHALL parse function declarations including generic parameters `<T: Bound>`, parameters `(name: Type)`, return type `-> Type`, effect clause `with Effect`, where clause `where T: Bound`, and indented body
- **FR-PARSE.8:** WHEN `struct` keyword is encountered THEN parser SHALL parse struct declarations with named fields `field: Type` and optional attributes
- **FR-PARSE.9:** WHEN `enum` keyword is encountered THEN parser SHALL parse enum declarations with named variants and optional payload types
- **FR-PARSE.10:** WHEN `trait` keyword is encountered THEN parser SHALL parse trait declarations with method signatures (no bodies) and optional supertraits
- **FR-PARSE.11:** WHEN `impl` keyword is encountered THEN parser SHALL parse impl blocks with method bodies for a named type or trait
- **FR-PARSE.12:** WHEN `use` keyword is encountered THEN parser SHALL parse use declarations with path segments `::` and optional `as` alias
- **FR-PARSE.13:** WHEN `mod` keyword is encountered THEN parser SHALL parse module declarations with optional inline body
- **FR-PARSE.14:** WHEN `type` keyword is encountered THEN parser SHALL parse type alias declarations `type Name = Type`
- **FR-PARSE.15:** WHEN `const` keyword is encountered THEN parser SHALL parse const declarations with type annotation and value expression
- **FR-PARSE.16:** WHEN `comptime` keyword is encountered THEN parser SHALL parse comptime blocks as `Item::Comptime`
- **FR-PARSE.17:** WHEN `macro` keyword is encountered THEN parser SHALL parse macro definitions as `Item::Macro`
- **FR-PARSE.18:** WHEN `test` keyword is encountered THEN parser SHALL parse test definitions as `Item::Test`
- **FR-PARSE.19:** WHEN contextual keyword `include` is encountered THEN parser SHALL parse C header include imports with angle-bracket `<header.h>` or quoted `"header.h"` syntax and `as alias`
- **FR-PARSE.20:** WHEN contextual keyword `import` is encountered THEN parser SHALL parse Python import statements
- **FR-PARSE.21:** WHEN contextual keyword `from` is encountered THEN parser SHALL parse `from module import Name` statements
- **FR-PARSE.22:** WHEN contextual keywords `patch`, `law`, `axiom`, `converge`, `world`, `entangle`, `orchestrate`, `pulse`, `resonate`, `shatter` are encountered THEN parser SHALL parse the corresponding Layer 1–7 items (even though the typechecker will stub them)
- **FR-PARSE.23:** WHEN `component` keyword is encountered THEN parser SHALL parse component declarations with props, state, methods, and JSX render body
- **FR-PARSE.24:** WHEN `shader` keyword is encountered THEN parser SHALL parse shader declarations (vertex, fragment, compute) with uniform bindings and workgroup sizes
- **FR-PARSE.25:** WHEN `actor` keyword is encountered THEN parser SHALL parse actor declarations with state, `on` message handlers, and `spawn` initialization

#### FR-PARSE: Statement Parsing

- **FR-PARSE.26:** WHEN `let` keyword is encountered THEN parser SHALL parse let bindings with optional type annotation and value expression
- **FR-PARSE.27:** WHEN `var` keyword is encountered THEN parser SHALL parse rebindable var bindings
- **FR-PARSE.28:** WHEN `mut` modifier is encountered on a binding THEN parser SHALL mark the binding as mutable
- **FR-PARSE.29:** WHEN `return` keyword is encountered THEN parser SHALL parse return statements with optional value expression
- **FR-PARSE.30:** WHEN `defer` keyword is encountered THEN parser SHALL parse defer statements `defer expr`
- **FR-PARSE.31:** WHEN `for` keyword is encountered THEN parser SHALL parse for loops `for binding in iter: body`
- **FR-PARSE.32:** WHEN `fanout` keyword is encountered THEN parser SHALL parse fanout loops `fanout binding in iter: body`
- **FR-PARSE.33:** WHEN `while` keyword is encountered THEN parser SHALL parse while loops `while cond: body`
- **FR-PARSE.34:** WHEN `loop` keyword is encountered THEN parser SHALL parse infinite loops `loop: body`
- **FR-PARSE.35:** WHEN `break` keyword is encountered THEN parser SHALL parse break statements with optional value
- **FR-PARSE.36:** WHEN `continue` keyword is encountered THEN parser SHALL parse continue statements

#### FR-PARSE: Pratt Expression Parser

- **FR-PARSE.37:** WHEN the parser enters expression context THEN it SHALL use a Pratt-style operator-precedence parser implementing all 16 precedence levels
- **FR-PARSE.38:** WHEN binary operators are parsed THEN the Pratt loop SHALL use `parse_binary(min_prec)` with left associativity for all operators except `**` which SHALL be right-associative
- **FR-PARSE.39:** WHEN `&&` / `and` operators are parsed THEN parser SHALL assign precedence level 2 (lowest logical)
- **FR-PARSE.40:** WHEN `||` / `or` operators are parsed THEN parser SHALL assign precedence level 1 (lowest overall)
- **FR-PARSE.41:** WHEN comparison operators (`==`, `!=`, `<`, `>`, `<=`, `>=`) are parsed THEN parser SHALL assign precedence level 7
- **FR-PARSE.42:** WHEN arithmetic operators (`+`, `-`, `*`, `/`, `%`, `**`) are parsed THEN parser SHALL assign precedence levels 9, 10, 11 respectively
- **FR-PARSE.43:** WHEN bitwise operators (`|`, `^`, `&`, `<<`, `>>`) are parsed THEN parser SHALL assign precedence levels 3–5, 8

#### FR-PARSE: Unary Expressions

- **FR-PARSE.44:** WHEN unary operators (`-`, `!`, `~`, `*`, `&`, `++`, `--`) are encountered at expression start THEN parser SHALL produce the corresponding `Expr::Unary` or `Expr::Deref`/`Expr::Ref` nodes
- **FR-PARSE.45:** WHEN `await` keyword is encountered at expression start THEN parser SHALL produce `Expr::Await`
- **FR-PARSE.46:** WHEN `spawn` keyword is encountered at expression start THEN parser SHALL produce `Expr::Spawn`
- **FR-PARSE.47:** WHEN `send` keyword is encountered THEN parser SHALL produce `Expr::SendMsg`
- **FR-PARSE.48:** WHEN `emit` keyword is encountered THEN parser SHALL produce `Expr::Emit`
- **FR-PARSE.49:** WHEN `collapse`, `observe`, `decay`, `share` keywords are encountered THEN parser SHALL produce the corresponding ownership expression nodes
- **FR-PARSE.50:** WHEN contextual keyword `teleport` is encountered at expression start THEN parser SHALL parse teleport expressions `teleport value from WorldA to WorldB via Channel`

#### FR-PARSE: Postfix Expressions

- **FR-PARSE.51:** WHEN `(` follows an expression THEN parser SHALL produce `Expr::Call` with parsed arguments
- **FR-PARSE.52:** WHEN `.` follows an expression THEN parser SHALL produce `Expr::Field` or `Expr::MethodCall` if followed by `(`
- **FR-PARSE.53:** WHEN `[` follows an expression THEN parser SHALL produce `Expr::Index`
- **FR-PARSE.54:** WHEN `++` or `--` follows an expression THEN parser SHALL produce desugared post-increment/decrement
- **FR-PARSE.55:** WHEN `?` follows an expression THEN parser SHALL produce `Expr::Try`
- **FR-PARSE.56:** WHEN `?.` follows an expression THEN parser SHALL desugar to null-check + field access
- **FR-PARSE.57:** WHEN `as` follows an expression THEN parser SHALL produce `Expr::Cast` with the target type

#### FR-PARSE: Assignment and Special Forms

- **FR-PARSE.58:** WHEN `=` is parsed in expression context THEN parser SHALL produce `Expr::Assign` with right-associative chaining `a = b = c`
- **FR-PARSE.59:** WHEN compound assignment (`+=`, `-=`, `*=`, etc.) is parsed THEN parser SHALL desugar to `Expr::Assign(target, Expr::Binary(target, op, value))`
- **FR-PARSE.60:** WHEN `a ? b : c` is parsed THEN parser SHALL desugar to `Expr::Match { scrutinee: a, arms: [true => b, false => c] }`
- **FR-PARSE.61:** WHEN `a ?? b` is parsed THEN parser SHALL desugar to null-coalescing match expression
- **FR-PARSE.62:** WHEN `a..b`, `a..=b`, `..b`, `..=b`, `a..` are parsed THEN parser SHALL produce `Expr::Range` with appropriate start/end/inclusive fields

#### FR-PARSE: Generics and Effects

- **FR-PARSE.63:** WHEN `<` follows a function name THEN parser SHALL parse generic parameters `<T: Bound1 + Bound2, U>` handling the `>>` injection problem (splitting `>>` into `> >` for nested generics)
- **FR-PARSE.64:** WHEN `where` contextual keyword is encountered after function signature THEN parser SHALL parse where clause `where T: TraitBound, U: OtherBound`
- **FR-PARSE.65:** WHEN `with` keyword is encountered THEN parser SHALL parse effect annotations `with Pure, IO, Unsafe`
- **FR-PARSE.66:** WHEN `where` and `with` clauses appear THEN parser SHALL accept them in any order relative to each other

#### FR-PARSE: JSX Parsing

- **FR-PARSE.67:** WHEN `<` followed by an identifier is encountered in JSX context THEN parser SHALL parse JSX elements including attributes, children, and closing tags
- **FR-PARSE.68:** WHEN `{expr}` is encountered inside JSX THEN parser SHALL parse the embedded expression
- **FR-PARSE.69:** WHEN `</` is encountered THEN parser SHALL parse JSX closing tags and validate tag name matching
- **FR-PARSE.70:** WHEN JSX is parsed in `render <jsx>` context inside a component THEN parser SHALL treat it as the render body

#### FR-PARSE: Error Recovery

- **FR-PARSE.71:** WHEN a parse error is encountered THEN parser SHALL call `synchronize()` to skip tokens until the next item boundary and continue parsing
- **FR-PARSE.72:** WHEN more than 50 errors are accumulated THEN parser SHALL bail out and return the accumulated error set
- **FR-PARSE.73:** WHEN a reserved keyword is used as an identifier THEN parser SHALL emit `DiagnosticCode::ParseReservedIdentifier`
- **FR-PARSE.74:** WHERE the parser SHALL recognize ~174 reserved keywords that cannot be used as identifiers, including HLSL and C++ reserved words

---

### FR-TYPE: Typechecker Subsystem

> **Research source:** `02-typechecker-types.md` §§1.1–1.7, §§2.1–2.4, §§3.1–3.8, §§4.1–4.5  
> **Target files:** `types.kn`, `effects.kn`, `monomorphize.kn`  
> **Parallelizable with:** FR-PARSE, FR-CODEGEN (shared type definitions)  
> **Depends on:** FR-PARSE for AST type definitions

#### FR-TYPE: Type System

- **FR-TYPE.1:** WHERE the type system SHALL support all 20 `ResolvedType` variants: `Unit`, `Bool`, `Int(IntSize)`, `Float(FloatSize)`, `String`, `Char`, `Array(T, N)`, `Slice(T)`, `Tuple(Vec<T>)`, `Ref { mutable, inner }`, `Ptr { mutable, inner }`, `Option(T)`, `Result(T, E)`, `Future(T)`, `Struct(name, fields)`, `Enum(name, variants)`, `Function { params, ret, effects }`, `Generic(name)`, `Never`, `Unknown`
- **FR-TYPE.2:** WHEN `types_compatible(expected, actual)` is evaluated THEN it SHALL implement the complete pairwise compatibility rules including: numeric promotion (`Int`↔`Float` cross-compatible), nominal struct matching (name equality only), `Unknown` leniency, `Generic` universal compatibility, `Never` bottom-type compatibility, `Ref` auto-deref, and `Option`/`Result` structural matching
- **FR-TYPE.3:** WHEN an integer literal is typed THEN it SHALL resolve to `Int(I64)` by default unless annotated otherwise
- **FR-TYPE.4:** WHEN a float literal is typed THEN it SHALL resolve to `Float(F64)` by default unless annotated otherwise
- **FR-TYPE.5:** WHERE all primitive integer types shall be internally registered at startup: `I8`, `I16`, `I32`, `I64`, `I128`, `Isize`, `U8`, `U16`, `U32`, `U64`, `U128`, `Usize`

#### FR-TYPE: 4-Pass Typecheck Pipeline

- **FR-TYPE.6:** WHEN typechecking begins THEN the system SHALL execute a 4-pass pipeline: Pass 1 (predeclare type names), Pass 2 (register field/method signatures), Pass 3 (re-register for forward references), Pass 4 (full expression typecheck)
- **FR-TYPE.7:** WHEN Pass 1 executes THEN it SHALL predeclare all `struct`, `enum`, `trait`, `world`, `actor`, `component` names as empty shells enabling forward references
- **FR-TYPE.8:** WHEN Pass 2 executes THEN it SHALL resolve field types, variant payload types, and method signatures for all predeclared types
- **FR-TYPE.9:** WHEN Pass 2 fails for an item THEN it SHALL be marked in `skip[2]` and excluded from Pass 3 and Pass 4
- **FR-TYPE.10:** WHEN Pass 3 executes THEN it SHALL reprocess items that passed Pass 2 to resolve types that were registered later in Pass 2 (single re-try, NOT fixpoint iteration)
- **FR-TYPE.11:** WHEN Pass 4 executes THEN it SHALL typecheck all function bodies, expression trees, and pattern matches for items that passed Pass 2 and Pass 3
- **FR-TYPE.12:** WHEN all 4 passes complete with zero errors THEN the system SHALL return `TypedProgram`

#### FR-TYPE: Expression Type Inference

- **FR-TYPE.13:** WHEN an `Expr::Int` is checked THEN its type SHALL resolve to `Int(I64)`
- **FR-TYPE.14:** WHEN an `Expr::Float` is checked THEN its type SHALL resolve to `Float(F64)`
- **FR-TYPE.15:** WHEN `Expr::Bool` is checked THEN its type SHALL resolve to `Bool`
- **FR-TYPE.16:** WHEN `Expr::String` or `Expr::FString` is checked THEN its type SHALL resolve to `String`
- **FR-TYPE.17:** WHEN `Expr::None` is checked THEN its type SHALL resolve to `Option(Unknown)` — `Unknown` to be resolved by context
- **FR-TYPE.18:** WHEN `Expr::Binary` with comparison operators is checked THEN its type SHALL resolve to `Bool`
- **FR-TYPE.19:** WHEN `Expr::Binary` with arithmetic operators is checked THEN its type SHALL be the promoted type of operands
- **FR-TYPE.20:** WHEN `Expr::If` is checked THEN the condition SHALL be `Bool` and the result type SHALL be the unified type of both branches
- **FR-TYPE.21:** WHEN `Expr::Match` is checked THEN all arms SHALL unify to a common type
- **FR-TYPE.22:** WHEN `Expr::Call` is checked THEN argument types SHALL match parameter types, and the call type SHALL match the callee's return type
- **FR-TYPE.23:** WHEN a function return type is not annotated THEN it SHALL be inferred from the body's trailing expression type

#### FR-TYPE: Effect Checking

- **FR-TYPE.24:** WHEN a call site is checked THEN the system SHALL verify that the caller's effects can call the callee's effects using the 4-rule `can_call` lattice
- **FR-TYPE.25:** WHEN a `Pure` function calls a non-`Pure` function THEN the system SHALL emit an effect violation error
- **FR-TYPE.26:** WHEN an `Unsafe` function calls any function THEN the system SHALL allow it (Unsafe is the top of the lattice)
- **FR-TYPE.27:** WHEN a `Pure` function is called from any context THEN the system SHALL allow it (Pure is the bottom of the lattice)
- **FR-TYPE.28:** WHEN `asm(...)`, `bitcast(...)`, `lfence()`, `sfence()`, `mfence()`, `clflush(...)`, `mem_load(...)`, `mem_store(...)`, `ptr_offset(...)`, `alloc(...)`, or `atomic_*` operations appear THEN the system SHALL require the caller to have the `Unsafe` effect
- **FR-TYPE.29:** WHERE the effect lattice SHALL be: Pure (bottom) < IO|GPU|Async|Reactive|Alloc|Panic < Unsafe (top)
- **FR-TYPE.30:** WHEN `pulse` or `resonate` bodies are typechecked THEN the system SHALL auto-emit all 8 effects for the body's semantic context

#### FR-TYPE: Generic Monomorphization

- **FR-TYPE.31:** WHEN a generic function `<T>` is declared THEN the typechecker SHALL resolve `T` as `Generic("T")` and treat it as compatible with all types during typechecking
- **FR-TYPE.32:** WHEN the monomorphizer encounters a call to a generic function with concrete argument types THEN it SHALL execute `unify(param_type, arg_type)` to bind generic names to concrete types
- **FR-TYPE.33:** WHEN a generic name is already bound AND the new binding conflicts THEN the monomorphizer SHALL emit a conflicting generic binding error
- **FR-TYPE.34:** WHEN monomorphization succeeds THEN the monomorphizer SHALL create a monomorphized copy of the function with concrete types substituted via `substitute_type()`
- **FR-TYPE.35:** WHEN `where T: TraitBound` constraints are present THEN the monomorphizer SHALL verify the concrete type satisfies the trait bound at instantiation time

#### FR-TYPE: Stub Strategy for Layers 1–7

- **FR-TYPE.36:** WHEN a `world` item is typechecked THEN the system SHALL predeclare it as a `Struct` with empty fields, register `state` fields as struct fields, and skip surface/entangle validation
- **FR-TYPE.37:** WHEN an `actor` item is typechecked THEN the system SHALL predeclare it as a `Struct`, register `on` handlers as function signatures, and skip message contract validation
- **FR-TYPE.38:** WHEN a `component` item is typechecked THEN the system SHALL predeclare it as a `Struct`, register props as fields, and skip JSX/render validation
- **FR-TYPE.39:** WHEN a `patch` or `law` item is typechecked THEN the system SHALL typecheck it as a plain `fn` (with `Bool` return constraint for law), skipping journaling/epoch validation
- **FR-TYPE.40:** WHEN a `converge` item is typechecked THEN the system SHALL typecheck lanes as plain `fn`, skipping selector/match/verify logic
- **FR-TYPE.41:** WHEN an `orchestrate` item is typechecked THEN the system SHALL typecheck stage bodies as expressions, skipping graph validation
- **FR-TYPE.42:** WHEN a `pulse` or `resonate` item is typechecked THEN the system SHALL typecheck the body as a block expression, skipping duration/dampen parsing beyond syntax
- **FR-TYPE.43:** WHEN an `axiom`, `shatter`, or `teleport` item is typechecked THEN the system SHALL parse and store, skipping all semantic validation

---

### FR-CODEGEN: LLVM Codegen Subsystem

> **Research source:** `03-llvm-codegen-jit.md` §§2.1–2.3, §§3.1–3.4, §§4.1–4.9, §§5.1–5.12, §6  
> **Target files:** `codegen.kn`, `llvm_ffi.kn`  
> **Parallelizable with:** FR-JIT, FR-RUNTIME, FR-ORCH  
> **Depends on:** FR-TYPE for typed program structure definitions

#### FR-CODEGEN: Two-Path Architecture

- **FR-CODEGEN.1:** WHERE the codegen subsystem SHALL support two compilation paths: Path A (textual `.ll` string emission) and Path B (LLVM-C API in-memory module construction)
- **FR-CODEGEN.2:** WHEN Path A is selected THEN the codegen SHALL produce a valid LLVM IR text file by formatting strings — NO LLVM library linkage required
- **FR-CODEGEN.3:** WHEN Path B is selected THEN the codegen SHALL use the LLVM-C API via `include <llvm-c/Core.h> as llvm` to construct LLVM modules, functions, basic blocks, and instructions in-memory

#### FR-CODEGEN: Kain Type → LLVM Type Mapping

- **FR-CODEGEN.4:** WHEN a Kain `Int(I64)` is lowered THEN it SHALL map to LLVM `i64`
- **FR-CODEGEN.5:** WHEN a Kain `Float(F64)` is lowered THEN it SHALL map to LLVM `double`
- **FR-CODEGEN.6:** WHEN a Kain `Bool` is lowered THEN it SHALL map to LLVM `i1` in SSA registers, `i8` at ABI boundaries, `i64` in struct fields
- **FR-CODEGEN.7:** WHEN a Kain `String` is lowered THEN it SHALL map to LLVM `{i8*, i64}` (fat pointer: data pointer + byte length)
- **FR-CODEGEN.8:** WHEN a Kain `ptr<T>` is lowered THEN it SHALL map to LLVM opaque `ptr` (i8* with LLVM opaque pointers)
- **FR-CODEGEN.9:** WHEN a Kain struct is lowered THEN it SHALL map to a named LLVM struct type `%Name = type { T1, T2, ... }`
- **FR-CODEGEN.10:** WHEN a Kain enum is lowered THEN it SHALL map to a tagged union `{i64, [N x i8]}` with tag discriminator and ABI-sized payload
- **FR-CODEGEN.11:** WHEN a Kain `Array(T, N)` is lowered THEN it SHALL map to LLVM `[N x T]` fixed-size array
- **FR-CODEGEN.12:** WHEN a Kain `Option(T)` is lowered THEN it SHALL map to `{i64, T}` (tag 0=Some, 1=None + payload)
- **FR-CODEGEN.13:** WHEN a Kain `Result(T, E)` is lowered THEN it SHALL map to `{i64, T, E}` (tag + ok + err payloads)
- **FR-CODEGEN.14:** WHERE the complete type mapping table from Kain types to LLVM types SHALL be implemented for all 20 ResolvedType variants

#### FR-CODEGEN: Module Structure

- **FR-CODEGEN.15:** WHEN a module is emitted THEN codegen SHALL emit the target triple, data layout string, and module flags header
- **FR-CODEGEN.16:** WHEN a module is emitted THEN codegen SHALL emit all struct type definitions before any function definitions
- **FR-CODEGEN.17:** WHEN a module is emitted THEN codegen SHALL emit `declare` statements for all externally-referenced runtime functions (200+ declare statements)
- **FR-CODEGEN.18:** WHEN a module is emitted THEN codegen SHALL emit comptime global constants (strings and initialized globals)

#### FR-CODEGEN: Function Compilation

- **FR-CODEGEN.19:** WHEN a function is compiled THEN codegen SHALL emit the LLVM function signature with parameter types matched to Kain types
- **FR-CODEGEN.20:** WHEN a function body is compiled THEN codegen SHALL emit an entry basic block with `alloca` for each mutable local variable, `store` of parameter values into allocas, and recursive compilation of body expressions
- **FR-CODEGEN.21:** WHEN a `let` binding is compiled THEN codegen SHALL use SSA registers directly for immutable bindings and alloca+store+load for mutable bindings
- **FR-CODEGEN.22:** WHEN a `return` statement is compiled THEN codegen SHALL emit `ret <ty> <value>` or `ret void`

#### FR-CODEGEN: Expression Compilation

- **FR-CODEGEN.23:** WHEN an integer literal is compiled THEN codegen SHALL emit a virtual register initialized via `add i64 0, <value>` or LLVM `LLVMConstInt`
- **FR-CODEGEN.24:** WHEN a binary arithmetic expression is compiled THEN codegen SHALL compile left and right operands and emit the corresponding LLVM arithmetic instruction (`add`, `sub`, `mul`, `sdiv`, `srem`, `fadd`, etc.)
- **FR-CODEGEN.25:** WHEN a binary comparison expression is compiled THEN codegen SHALL emit `icmp <predicate>` or `fcmp <predicate>` with the appropriate predicate
- **FR-CODEGEN.26:** WHEN an `if`/`else` expression is compiled THEN codegen SHALL emit conditional branch to then/else basic blocks and a merge block with phi node for the result value
- **FR-CODEGEN.27:** WHEN a `while` or `for` loop is compiled THEN codegen SHALL emit header/body/exit basic blocks with a loop stack tracking `(continue_label, break_label)`
- **FR-CODEGEN.28:** WHEN `break` or `continue` is compiled THEN codegen SHALL emit an unconditional branch to the corresponding label from the loop stack
- **FR-CODEGEN.29:** WHEN a `match` expression is compiled THEN codegen SHALL emit tag comparison and conditional branches for each pattern arm
- **FR-CODEGEN.30:** WHEN a function call is compiled THEN codegen SHALL emit `call <ret_ty> @<fn_name>(<args>)` or the LLVM-C equivalent `LLVMBuildCall2`
- **FR-CODEGEN.31:** WHEN a struct literal is compiled THEN codegen SHALL emit field stores into an alloca'd struct using `getelementptr` + `store`
- **FR-CODEGEN.32:** WHEN a field access `obj.field` is compiled THEN codegen SHALL emit `getelementptr` + `load` for in-memory structs
- **FR-CODEGEN.33:** WHEN `asm("...")` is compiled THEN codegen SHALL emit LLVM inline assembly via `call void asm sideeffect`

#### FR-CODEGEN: Runtime Function Declarations

- **FR-CODEGEN.34:** WHEN codegen emits runtime declarations THEN it SHALL include all required `declare` statements from the runtime contract: print functions, allocator (`KAIN_alloc`, `__kain_alloc`), string operations (`string_new`, `str_concat`, `strlen`), Option/Result ABI (`abi_option_*`, `abi_result_*`), actor runtime (`kain_actor_*`), ownership (`__kain_ownership_*`), machine stones (`kain_machine_*`), memory helpers (`__kain_mem_*`, `__kain_atomic_*`), GPU (`abi_gpu_*`), converge/orchestrate (`abi_converge_*`, `abi_orchestrate_*`), and runtime init/shutdown (`abi_runtime_init`, `abi_runtime_shutdown`)
- **FR-CODEGEN.35:** WHEN runtime declarations are emitted THEN codegen SHALL deduplicate them (each symbol declared only once)

#### FR-CODEGEN: C ABI and Target Configuration

- **FR-CODEGEN.36:** WHEN codegen sets up the target THEN it SHALL apply the C ABI policy for the target platform (LP64 for Linux/macOS, LLP64 for Windows)
- **FR-CODEGEN.37:** WHEN a C ABI policy is active THEN codegen SHALL know the size and alignment of every C type (`int` = 4 bytes, `long` = 4 or 8 bytes depending on platform, `void*` = 8 bytes)
- **FR-CODEGEN.38:** WHEN Windows x86-64 MSVC is the target THEN the triple SHALL be `x86_64-pc-windows-msvc`
- **FR-CODEGEN.39:** WHEN Linux x86-64 GNU is the target THEN the triple SHALL be `x86_64-unknown-linux-gnu`

#### FR-CODEGEN: Untagging for @extern Calls

- **FR-CODEGEN.40:** WHEN a call to an `@extern` function is compiled AND arguments include Kain integers THEN codegen SHALL strip the tagged integer representation (`val >> 3`) before the C ABI call
- **FR-CODEGEN.41:** WHEN an `@extern` function returns a value AND the return type is an integer THEN codegen SHALL tag the raw return value (`(val << 3) | 1`) for Kain internal representation
- **FR-CODEGEN.42:** WHEN a Kain `String` is passed to a C function expecting `const char*` THEN codegen SHALL `extractvalue` to pass only the data pointer, not the length
- **FR-CODEGEN.43:** WHEN a C function returns `const char*` and has `@c_string_return` annotation THEN codegen SHALL materialize the raw pointer into an owned Kain String via `string_new` + `strlen`

#### FR-CODEGEN: Metadata and Sidecars

- **FR-CODEGEN.44:** WHEN codegen completes THEN it SHALL emit the runtime contract bundle as a JSON sidecar file
- **FR-CODEGEN.45:** WHEN codegen completes THEN it SHALL emit the realtime app bundle (if applicable)
- **FR-CODEGEN.46:** WHEN shader items are present THEN codegen SHALL emit shader artifact bundles and compute residency sidecars

---

### FR-JIT: Dual JIT Execution Subsystem

> **Research source:** `06-jit-markscript-metal-architecture.md` §§1.1–2.2, §§3.1–3.9, §§4.1–4.6, §§5.1–5.3, §8  
> **Target files:** `jit.kn`, `jit_metal.kn`, `jit_x86.kn`, `jit_orc.kn`, `jit_cache.kn`  
> **Parallelizable with:** FR-CODEGEN, FR-CLI, FR-RUNTIME

#### FR-JIT: Path A — Markscript-Style Direct x86-64 Emission

- **FR-JIT.1:** WHEN Path A (direct x86-64 emission) is invoked THEN the system SHALL emit native x86-64 machine code byte-by-byte into a code array, using the proven pattern from `blades/markscript/src/jit.kn`
- **FR-JIT.2:** WHERE Path A SHALL emit proper prologue (`push rbp; push rbx; mov rbp, rsp`) and epilogue (`mov rsp, rbp; pop rbx; pop rbp; ret`)
- **FR-JIT.3:** WHERE Path A SHALL maintain a software operand stack at RBP-relative offsets rather than using native `push`/`pop` instructions
- **FR-JIT.4:** WHERE Path A SHALL use fixed register allocation: RAX (accumulator), RBX (right operand), RBP (frame pointer)
- **FR-JIT.5:** WHEN Path A encounters forward jumps THEN it SHALL use two-pass fixup: Pass 1 records native offsets and emits jump placeholders; Pass 2 resolves all displacements

#### FR-JIT: Path A — W^X Memory Lifecycle

- **FR-JIT.6:** WHEN Path A compiles code bytes THEN it SHALL execute the W^X lifecycle: `vm_map(RW)` → `mem_store(bytes)` → `vm_protect(RX)` → `cache_flush` every cache line → `full_fence()` → `asm("call rax")`
- **FR-JIT.7:** WHEN allocating JIT pages THEN Path A SHALL compute the page-aligned allocation size from `vm_page_size()`
- **FR-JIT.8:** WHEN writing code bytes THEN Path A SHALL use `collapse` scope to guard the write phase
- **FR-JIT.9:** WHEN transitioning to executable THEN Path A SHALL call `vm_protect_execute_read()` (RX), NOT `vm_protect_execute_read_write()` (RWX), to enforce W^X security
- **FR-JIT.10:** WHEN flushing the instruction cache THEN Path A SHALL iterate every `cpu_cache_line_bytes()` boundary and call `cache_flush(ptr)` at each

#### FR-JIT: Shared asm Trampoline

- **FR-JIT.11:** WHEN either Path A or Path B needs to execute JIT-compiled code THEN they SHALL use the shared assembly trampoline: allocate a 2-element scratch buffer, store the code pointer, execute `asm("mov rax, [rdi]; call rax; mov [rdi+8], rax")`, and load the result
- **FR-JIT.12:** WHERE the trampoline SHALL be annotated with `Unsafe` effect, `memory = true`, and `clobbers = "rax,rcx,rdx"`
- **FR-JIT.13:** WHERE the trampoline contract SHALL specify: Input = code pointer in scratch[0], Output = return value in scratch[1] (captured from RAX), Callee must save/restore RBP and RBX, Callee must return result in RAX and end with RET

#### FR-JIT: Path B — OrcJIT via LLVM-C API

- **FR-JIT.14:** WHEN Path B (OrcJIT) is invoked THEN the system SHALL use `include <llvm-c/Orc.h> as llvm_orc` to access LLVM's OrcJIT API
- **FR-JIT.15:** WHEN initializing OrcJIT THEN the system SHALL call `LLVMInitializeNativeTarget()`, `LLVMInitializeNativeAsmPrinter()`, and create an LLJIT instance via `LLVMOrcCreateLLJIT`
- **FR-JIT.16:** WHEN compiling a module THEN the system SHALL verify the module with `LLVMVerifyModule`, add it to the JIT via `LLVMOrcLLJITAddLLVMIRModule`, and look up the entry symbol via `LLVMOrcLLJITLookup`
- **FR-JIT.17:** WHEN the OrcJIT environment is unavailable (no LLVM DLL) THEN the system SHALL fall back to Path A for supported platforms (x86-64)

#### FR-JIT: JIT Cache

- **FR-JIT.18:** WHEN a code block is JIT-compiled THEN the result SHALL be cached in a `shatter struct` CacheStore with Structure-of-Arrays layout (parallel arrays for hashes, pointers, sizes)
- **FR-JIT.19:** WHEN looking up a cached entry THEN the system SHALL perform a linear scan of the hashes array (optimized by SoA layout for L1 cache residency)
- **FR-JIT.20:** WHERE the cache SHALL track hit count, miss count, total bytes emitted, and compile count for telemetry

#### FR-JIT: W^X Contract

- **FR-JIT.21:** WHERE every primitive used by the JIT SHALL be proven working by the corresponding `metal.kn` benchmark case: `asm()` (Case 0), `cache_flush` with operand binding (Case 1), `lfence`/`sfence`/`mfence` (Case 4), full VM lifecycle (Case 5), `converge` asm lane (Case 10)
- **FR-JIT.22:** WHERE the W^X state machine SHALL follow the exact sequence: UNMAPPED → RW (vm_map) → write bytes (collapse scope) → RX (vm_protect_execute_read) → flush cache → fence → execute → UNMAPPED (decay)

---

### FR-CLI: CLI Driver Subsystem

> **Research source:** `04-cli-driver-selfhost.md` §§3.1–3.3, §§4.1–4.3, §§5.1–5.3, §§7.1–7.4  
> **Target files:** `compiler.kn` (driver), `cli.kn`, `main.kn`  
> **Parallelizable with:** FR-ORCH (shares config/markdown pipeline)  
> **Depends on:** All other subsystems for actual compilation, but CLI can be wired with stubs

#### FR-CLI: Subcommand Tree

- **FR-CLI.1:** WHEN `kainc` is invoked with no arguments THEN it SHALL display help text listing all available subcommands
- **FR-CLI.2:** WHEN `kainc check <input>` is invoked THEN it SHALL lex, parse, and typecheck all `.kn` files under the input path and report diagnostics
- **FR-CLI.3:** WHEN `kainc build <input>` is invoked THEN it SHALL compile the input to a native executable via: lex → parse → typecheck → monomorphize → codegen → link
- **FR-CLI.4:** WHEN `kainc build <input> --target <target>` is invoked THEN it SHALL emit the specified compilation target: `llvm`, `c`, `jit`, `spirv`, `hlsl`, `wasm`
- **FR-CLI.5:** WHEN `kainc run <input>` is invoked THEN it SHALL compile and execute the program, capturing stdout, stderr, and exit code
- **FR-CLI.6:** WHEN `kainc test <input>` is invoked THEN it SHALL discover test cases and execute them, reporting pass/fail/ignored counts
- **FR-CLI.7:** WHEN `kainc selfhost bootstrap` is invoked THEN it SHALL execute the bootstrap pipeline: assemble combined source → compile to LLVM IR → compile runtime → link → produce `kainc` binary
- **FR-CLI.8:** WHEN `kainc selfhost bootstrap --verify-ouroboros` is invoked THEN it SHALL additionally verify the self-compilation roundtrip
- **FR-CLI.9:** WHEN `kainc fmt <input>` is invoked THEN it SHALL format Kain source files to canonical style
- **FR-CLI.10:** WHEN `kainc amalgamate` is invoked THEN it SHALL pack/unpack/inspect amalgamated capsules
- **FR-CLI.11:** WHEN `kainc doctor` is invoked THEN it SHALL report compiler version, build SHA, target triple, Kain HOME, stdlib path, runtime path, and LLVM path
- **FR-CLI.12:** WHEN `kainc config` is invoked THEN it SHALL show/set/init Kain configuration
- **FR-CLI.13:** WHEN `kainc clean` is invoked THEN it SHALL remove build artifacts from `.kain/out`, `.kain/cache`, `.kain/generated`

#### FR-CLI: DriverSession Pipeline

- **FR-CLI.14:** WHEN a compilation is initiated THEN the DriverSession SHALL execute the pipeline: Resolve → Lex → Parse → Comptime → Typecheck → Monomorphize → Codegen, emitting progress events at each phase
- **FR-CLI.15:** WHEN the Resolve phase executes THEN it SHALL resolve all `use` imports (stdlib, local filesystem), `include` headers (C FFI), and aggregate all source text into a single compilation unit with origin tracking
- **FR-CLI.16:** WHEN the pipeline completes THEN the DriverSession SHALL cache the frontend source bundle and checked frontend for incremental recompilation
- **FR-CLI.17:** WHEN a cached checked frontend matches the current source (by content hash + file fingerprints) THEN the pipeline SHALL skip Resolve → Parse → Comptime → Typecheck

#### FR-CLI: Workspace Discovery

- **FR-CLI.18:** WHEN a start path is provided THEN the system SHALL ascend the directory tree looking for workspace anchors: `KAIN.toml`, `kain.toml`, `build.kn`, `platform.kn`, or `.git`
- **FR-CLI.19:** WHEN a workspace root is found THEN the system SHALL load the effective manifest from `KAIN.toml` and `build.kn`
- **FR-CLI.20:** WHEN discovering blades in a workspace THEN the system SHALL glob-expand the default patterns `blades/*`, `apps/*`, `crates/*` and filter to directories containing anchor files

#### FR-CLI: Diagnostics

- **FR-CLI.21:** WHEN formatting diagnostics THEN the system SHALL produce output in the form: `filename:line:col: error: message` with a highlighted source line and caret
- **FR-CLI.22:** WHEN `--json` flag is active THEN the system SHALL produce a JSON array of diagnostic objects with fields: `severity`, `file_path`, `line`, `column`, `message`, `error_kind`, `span_start`, `span_end`
- **FR-CLI.23:** WHEN more than MAX_ERRORS (50) errors are accumulated THEN the system SHALL bail out and report the accumulated errors plus a "too many errors" note

---

### FR-RUNTIME: Runtime Contract & FFI Subsystem

> **Research source:** `05-runtime-contract-ffi.md` §§3.1–3.6, §§4.1–4.5, §§5.1–5.12, §§6.1–6.3, §§7.1–7.5, §8  
> **Target files:** `llvm_ffi.kn`, `runtime.kn`, `builtins.kn`  
> **Parallelizable with:** FR-CODEGEN, FR-JIT, FR-CLI

#### FR-RUNTIME: LLVM-C FFI Binding

- **FR-RUNTIME.1:** WHEN the compiler needs LLVM API access THEN it SHALL use `include <llvm-c/Core.h> as llvm` to import all LLVM-C API functions automatically via the libclang extraction pipeline
- **FR-RUNTIME.2:** WHEN LLVM-C handles are used THEN they SHALL be typed as `ptr<Byte>` (opaque pointers) since all LLVM-C types are opaque
- **FR-RUNTIME.3:** WHERE the following LLVM-C API categories SHALL be accessible: Core (context, module, types, constants, builder instructions), Target (native initialization, target machine), OrcJIT (LLJIT creation, module addition, symbol lookup), Analysis (module/function verification), BitWriter (bitcode serialization), Passes (optimization pipeline)
- **FR-RUNTIME.4:** WHERE every LLVM-C API call SHALL be annotated with `Unsafe` effect

#### FR-RUNTIME: @extern ABI Contract

- **FR-RUNTIME.5:** WHEN a function is annotated `@extern` THEN codegen SHALL emit a `declare` statement for the function with C ABI calling convention
- **FR-RUNTIME.6:** WHEN `@extern @link_name("symbol")` is present THEN codegen SHALL use the specified symbol name as the LLVM symbol, NOT the Kain function name
- **FR-RUNTIME.7:** WHEN `@extern @callconv("win64")` is present THEN codegen SHALL emit `win64cc` calling convention
- **FR-RUNTIME.8:** WHEN `@extern @naked` is present THEN codegen SHALL emit `naked` attribute on the function
- **FR-RUNTIME.9:** WHEN `@extern @c_string_return` is present THEN codegen SHALL materialize the returned `const char*` into an owned Kain String

#### FR-RUNTIME: Three-Layer Stdlib Pattern

- **FR-RUNTIME.10:** WHERE every public stdlib function SHALL follow the three-layer pattern: `@extern fn abi_X(...)` (raw ABI declaration) → `pub fn native_X(...)` (interpreter-interceptable wrapper) → `pub fn X(...)` (documented public API)
- **FR-RUNTIME.11:** WHEN the interpreter (interpret target) encounters a call to `native_X` THEN it SHALL provide a Rust-native implementation; the LLVM codegen SHALL inline through to the raw `abi_X` call

#### FR-RUNTIME: Runtime Function Table

- **FR-RUNTIME.12:** WHEN codegen emits the runtime function table THEN it SHALL include all 200+ runtime functions organized by category: core (print, string, alloc), stdlib ABI (Option, Result, Future, Patch, Resonance, Entangle), actor runtime, memory helpers, ownership state, machine stones, GPU/compute, orchestrate, init/shutdown, filesystem, process, Python interop, JSON/array/map utilities, LLVM math intrinsics
- **FR-RUNTIME.13:** WHEN runtime functions are emitted THEN codegen SHALL use the correct LLVM type signatures matching the C runtime's `runtime/native/include/stdlib_abi.h`

#### FR-RUNTIME: KainType ↔ CType Mapping

- **FR-RUNTIME.14:** WHERE the compiler SHALL know the complete `KainType → LLVM IR → C Type` mapping table including: `Int`↔`int64_t`, `Float`↔`double`, `Bool`↔`int` (ABI boundary), `String`↔`KainString { char* data; int64_t len; }`, `ptr<T>`↔`void*`, `Unit`↔`void`, `Option<T>`↔`KainOption { int64_t tag; T payload; }`, `Result<T,E>`↔`KainResult`

#### FR-RUNTIME: C Header Import Pipeline

- **FR-RUNTIME.15:** WHEN an `include <header.h> as alias` directive is encountered THEN the compiler SHALL support the same three-tier extraction pipeline: libclang (primary), lang-c AST (fallback), regex (last resort)
- **FR-RUNTIME.16:** WHEN libclang extracts a C header THEN the compiler SHALL resolve function declarations, struct definitions, typedefs, enum constants, and macro constants
- **FR-RUNTIME.17:** WHEN the libclang pipeline completes THEN the compiler SHALL generate type-safe FFI bindings with C ABI policy applied for the target platform

---

### FR-ORCH: MarkScript Orchestration Subsystem

> **Research source:** `07-markscript-fusion-contract.md` §§1.1–2.3, §§3.1–3.6, §§4.1–4.3, §§5.1–5.4, §§6.1–6.2, §7  
> **Target files:** `orchestrator.kn`  
> **Parallelizable with:** ALL other subsystems (only depends on the MarkScript VM embedding API)

#### FR-ORCH: VM Embedding

- **FR-ORCH.1:** WHEN the compiler initializes THEN it SHALL create a MarkScript VM via `markscript.mks_new_vm()`
- **FR-ORCH.2:** WHEN the VM is created THEN it SHALL register all 9 compiler-specific IVT handlers (IDs 200–208) into the VM via `markscript.mks_register()`

#### FR-ORCH: IVT Handlers

- **FR-ORCH.3:** WHEN intent phrase `compile check` is dispatched THEN the compiler SHALL execute lex + parse + typecheck and return error count (Handler 200)
- **FR-ORCH.4:** WHEN intent phrase `compile codegen` is dispatched THEN the compiler SHALL execute full compilation: lex → parse → typecheck → codegen → write output (Handler 201)
- **FR-ORCH.5:** WHEN intent phrase `compile jit` is dispatched THEN the compiler SHALL execute JIT compilation and return the result (Handler 202)
- **FR-ORCH.6:** WHEN intent phrase `test run` is dispatched THEN the compiler SHALL execute the test specification and return pass count (Handler 203)
- **FR-ORCH.7:** WHEN intent phrase `test report` is dispatched THEN the compiler SHALL generate a test report in the requested format (Handler 204)
- **FR-ORCH.8:** WHEN intent phrase `build link` is dispatched THEN the compiler SHALL link object files into the final binary (Handler 205)
- **FR-ORCH.9:** WHEN intent phrase `build package` is dispatched THEN the compiler SHALL execute the full build pipeline end-to-end (Handler 206)
- **FR-ORCH.10:** WHEN intent phrase `selfhost phase1` is dispatched THEN the compiler SHALL route through the Rust DLL bridge (Handler 207)
- **FR-ORCH.11:** WHEN intent phrase `selfhost phase2` is dispatched THEN the compiler SHALL execute pure-Kain self-compilation (Handler 208)

#### FR-ORCH: Build Config as Markscript Tables

- **FR-ORCH.12:** WHEN the compiler loads build configuration THEN it SHALL read markscript tables from `build.md` using `mks_table_get_string` and `mks_table_get_int`
- **FR-ORCH.13:** WHEN build config is loaded THEN it SHALL populate a `BuildConfig` struct with fields: name, target, profile, optimize, lto, entry, source_root, deps, output, runtime, linker, linker_flags, cc, cc_flags, test_root, doc_root
- **FR-ORCH.14:** WHEN `@schema` directive is present on build tables THEN the markscript VM SHALL validate column types, required fields, and value ranges

#### FR-ORCH: Build Pipeline as Markscript Intents

- **FR-ORCH.15:** WHEN the build pipeline is executed THEN the system SHALL load and execute `buildex.md` as a markscript file containing domains, routines, and intents
- **FR-ORCH.16:** WHERE the pipeline SHALL support the following routines: `BuildAll`, `QuickCheck`, `JitRun`, `TestAll`, `TestQuick`, `CleanAll`, `WatchLoop`, `SelfHostPhase1`, `SelfHostPhase2`, `PackageRelease`, `CIBuild`
- **FR-ORCH.17:** WHEN process orchestration intents (`> spawn`, `> await`, `> kill`, `> pipe`, `> env`, `> cwd`) are encountered THEN they SHALL be dispatched to markscript's GAMMA handlers

#### FR-ORCH: CLI Integration via Markscript

- **FR-ORCH.18:** WHEN `kainc build .` is invoked THEN the orchestrator SHALL load `build.md` (config), load `buildex.md` (pipeline), and dispatch the `BuildAll` routine through markscript
- **FR-ORCH.19:** WHEN `kainc build . --stage <name>` is invoked THEN the orchestrator SHALL dispatch only the specified pipeline routine

---

## Non-Functional Requirements

### Performance

- **NFR-P1:** WHERE `kainc check` on the compiler's own ~13,000-line source tree SHALL complete within 500ms on an x86-64 CPU with 4+ cores at 3GHz+
- **NFR-P2:** WHERE `kainc build --target llvm` on the compiler's own source tree SHALL complete within 5 seconds (excluding clang link time)
- **NFR-P3:** WHERE the lexer SHALL tokenize at a rate of at least 1MB/s of source text
- **NFR-P4:** WHERE the parser SHALL produce a flat AST at a rate of at least 500K tokens/s
- **NFR-P5:** WHERE the JIT Path A SHALL have sub-millisecond startup time (no LLVM initialization)
- **NFR-P6:** WHERE the JIT Path B (OrcJIT) SHALL be ready to compile within 200ms of initialization

### Correctness (Ouroboros)

- **NFR-C1:** WHEN the self-host compiler compiles its own source THEN the resulting stage-2 binary SHALL be byte-identical to the stage-1 binary
- **NFR-C2:** WHEN the self-host compiler compiles any Kain source THEN the output SHALL be functionally identical to the Rust bootstrap compiler's output for the same source
- **NFR-C3:** WHEN the compiler parses all 111 Kain keywords (58 hard, 51 contextual, 2 operator aliases) THEN it SHALL produce a parseable AST for all keyword combinations exercised by `benchmark/cases_v2/keyword_crucible.kn`

### Code Size

- **NFR-S1:** WHERE the total Kain source for the compiler core (lexer, parser, AST, typechecker, codegen, JIT) SHALL NOT exceed 12,500 lines
- **NFR-S2:** WHERE the MarkScript orchestration integration SHALL NOT exceed 500 lines
- **NFR-S3:** WHERE the total self-host compiler SHALL NOT exceed 13,000 lines of Kain

### Memory

- **NFR-M1:** WHERE the compiler SHALL fit within 512MB of RAM when compiling its own 13,000-line source tree
- **NFR-M2:** WHERE the JIT memory overhead SHALL not exceed 10× the size of the emitted code (page alignment + cache buffers)

### Observability

- **NFR-O1:** WHEN a compilation phase change occurs THEN the DriverSession SHALL emit a progress event identifying the phase (Resolve, Parse, Comptime, Typecheck, Monomorphize, Codegen)
- **NFR-O2:** WHEN a build pipeline executes THEN the system SHALL emit progress events for each task (task started, task finished, cache hit/miss)

### Security

- **NFR-SEC1:** WHERE the JIT W^X lifecycle SHALL use `vm_protect_execute_read` (RX), NOT `vm_protect_execute_read_write` (RWX), for executing JIT pages — pages must never be simultaneously writable and executable
- **NFR-SEC2:** WHERE all LLVM-C FFI calls SHALL be in functions annotated with `Unsafe` effect

---

## Edge Cases

| ID | Scenario | Expected Behavior |
|----|----------|-------------------|
| **EC-1** | Source file is empty (0 bytes) | Lexer produces a single `Eof` token; parser produces an empty `Program`; typechecker reports zero errors; codegen produces an empty LLVM module |
| **EC-2** | Integer literal exceeds i64 range | Parser emits a diagnostic "integer literal out of range" and continues with a truncated value |
| **EC-3** | Float literal with extreme precision | Lexer parses the closest representable f64 value; no diagnostic |
| **EC-4** | Source file with only comments and whitespace | Lexer produces only `Eof` token; parser produces empty program |
| **EC-5** | Indentation uses mixed tabs and spaces | Indent processor treats tab = 4 spaces; resulting indent level is additive (defined but undefined-quality) |
| **EC-6** | Indentation drops from 8 to 0 in one newline | Indent processor emits 2 DEDENT tokens (level 8, level 4) |
| **EC-7** | File ends without trailing newline | Indent processor auto-emits DEDENT for remaining indent levels and appends Eof |
| **EC-8** | `>>` encountered in generic context (nested generics like `Vec<Vec<Int>>`) | Parser splits `>>` into `> >` using the injected tokens buffer |
| **EC-9** | Function with 0 parameters | Parser accepts `fn foo() -> T:` as valid |
| **EC-10** | Function with 0 return type annotation and body with no trailing expression | Typechecker infers return type as `Unit` |
| **EC-11** | Struct with 0 fields | Parser and typechecker accept `struct Empty` as valid |
| **EC-12** | Generic function with 0 where constraints | Parser accepts `fn foo<T>(x: T) -> T:` as valid |
| **EC-13** | `where` and `with` clauses in reversed order | Parser accepts both `fn foo() -> T where T: Bound with Pure:` and `fn foo() -> T with Pure where T: Bound:` |
| **EC-14** | Maximum recursion depth for types (`struct A { b: B }`, `struct B { a: A }`) | Pass 3 (re-register) resolves mutually recursive types; Pass 2 might fail for one, Pass 3 resolves it |
| **EC-15** | Self-referencing enum variant (`Expr::Binary(left: Expr, ...)`) | Pass 1 registers `Expr` as empty enum shell; Pass 2 resolves the recursive reference against the shell |
| **EC-16** | `Unknown` type survives to codegen | Codegen emits a "type not resolved" error and fails compilation |
| **EC-17** | Calling a generic function with completely unconstrained types | Monomorphizer cannot instantiate — emits "could not infer type parameter" error |
| **EC-18** | `Option(Unknown)` from `none` without type annotation | Typechecker propagates `Unknown` upward; if not resolved by context, codegen fails |
| **EC-19** | Exceeding MAX_ERRORS (50) in parser or typechecker | System bails out and reports accumulated errors plus "too many errors" note |
| **EC-20** | `include <nonexistent.h>` with header not found | Diagnostic: "C header not found: nonexistent.h"; compilation continues for other modules but marks this import failed |
| **EC-21** | LLVM-C OrcJIT DLL not available at runtime | Path B fails gracefully; system falls back to Path A (x86-64 direct emission) if on x86-64; otherwise reports "JIT unavailable" |
| **EC-22** | JIT code buffer requires more than one page | Page-aligned allocation computed from code size; all pages transitioned through W^X as a unit |
| **EC-23** | `build.kn` or `KAIN.toml` missing in workspace | Workspace discovery fails with diagnostic; CLI falls back to treating the input as a single-file compilation |
| **EC-24** | Compiler compiles itself (recursive self-reference) | Ouroboros: stage-1 binary → compile source → stage-2 binary; stage-2 must be byte-identical to stage-1 |
| **EC-25** | Source file uses only contextual keywords as identifiers (valid in non-keyword positions) | Parser correctly treats them as `Ident` tokens since contextual keywords are only recognized in specific parser positions |
| **EC-26** | Tagged integer internal representation crossing FFI boundary multiple times | Tag is stripped before C call, re-applied after C return; correctness verified by ouroboros |
| **EC-27** | `pulse` or `resonate` body contains functions with effects | Typechecker auto-emits all 8 effects for the body's semantic context; no effect violations |

---

## Error Cases

| ID | Scenario | Expected Behavior |
|----|----------|-------------------|
| **ERR-1** | Unterminated string literal (no closing `"`) | Lexer emits "unterminated string literal" diagnostic; stops tokenizing the string and continues after a synthetic newline |
| **ERR-2** | Unexpected character (e.g., `$` at top level) | Lexer emits "unexpected character" diagnostic with the character and position |
| **ERR-3** | Missing closing bracket `)`, `]`, or `}` | Parser emits "expected closing delimiter" diagnostic; synchronizes to next item boundary |
| **ERR-4** | `fn` keyword without function name | Parser emits "expected function name" diagnostic; skips to next top-level item |
| **ERR-5** | Reserved keyword used as identifier | Parser emits `ParseReservedIdentifier` diagnostic |
| **ERR-6** | Duplicate struct/enum/trait name | Pass 1 typechecker emits `DuplicateTypeDefinition` diagnostic |
| **ERR-7** | Struct field type references nonexistent type | Pass 2 typechecker emits "type not found" diagnostic; field marked as `Unknown` |
| **ERR-8** | Function body returns incompatible type | Pass 4 typechecker emits "type mismatch: expected X, found Y" diagnostic |
| **ERR-9** | Pure function calls IO function | Effect checker emits "Pure function cannot call IO function" diagnostic |
| **ERR-10** | Pure function calls Unsafe function | Effect checker emits "Pure function cannot call Unsafe function" diagnostic |
| **ERR-11** | Unsafe operation (`mem_load`, `asm`, etc.) in non-Unsafe function | Effect checker emits "operation requires Unsafe effect" diagnostic |
| **ERR-12** | Generic type parameter not bound in where clause but used with trait method | Monomorphizer emits "type does not satisfy trait bound" diagnostic at instantiation time |
| **ERR-13** | Conflicting generic bindings (T bound to both Int and String) | Monomorphizer emits "conflicting generic binding: T is both Int and String" diagnostic |
| **ERR-14** | Trait method not found for concrete type | Typechecker emits "no method named X found for type Y" diagnostic |
| **ERR-15** | Variable used before declaration | Typechecker emits "variable not declared" diagnostic (scope resolution fails) |
| **ERR-16** | Mutable variable assigned in immutable binding scope | Typechecker emits "cannot assign to immutable binding" diagnostic |
| **ERR-17** | Match expression with non-exhaustive arms | Typechecker emits "non-exhaustive match" diagnostic |
| **ERR-18** | LLVM module verification failure | Codegen reports the LLVM verification error string from `LLVMVerifyModule` |
| **ERR-19** | JIT `vm_map` fails (out of memory) | JIT returns error code -1 with message "vm_map failed" |
| **ERR-20** | JIT `vm_protect_execute_read` fails | JIT returns error code -2 with message "protection change failed" |
| **ERR-21** | OrcJIT `LLVMOrcCreateLLJIT` fails (LLVM not installed) | System falls back to Path A or returns "OrcJIT initialization failed" |
| **ERR-22** | `LLVMOrcLLJITLookup` fails to find symbol | System reports "Symbol not found: <name>" |
| **ERR-23** | Clang not found during link phase | System reports "linker clang not found in PATH" |
| **ERR-24** | Source file not found during module resolution | System reports "module file not found: <path>" and skips the import |
| **ERR-25** | Build config table missing required field | MarkScript `@schema` validation reports "required field 'entry' missing in Metadata table" |
| **ERR-26** | Workspace discovery finds no anchor files | System reports "no workspace found: no KAIN.toml, build.kn, or .git in parent directories" |
| **ERR-27** | `>>` injection buffer overflow or token stream corruption | Parser falls back to treating `>>` as right-shift operator; typechecker catches the type mismatch downstream |
| **ERR-28** | Ouroboros verification fails (stage-1 != stage-2 binary) | System reports byte offset of first difference and the differing byte values |

---

## Constraints

| ID | Constraint | Rationale |
|----|------------|-----------|
| **C-1** | Compiler core MUST use only Layer 0 Kain constructs: `fn`, `struct`, `enum`, `trait`, `impl`, `let`, `mut`, `const`, `if`/`elif`/`else`, `match`, `for`, `while`, `loop`, `break`, `continue`, `return`, `defer`, `ptr<T>`, `collapse`/`observe`/`decay`, `use`, `mod`, `pub`, `include`, `asm`, `Pure`/`IO`/`Unsafe` effects | Avoids circular dependency: "compiler needs to compile construct X before it can use construct X" |
| **C-2** | Compiler core MUST NOT use Layer 1–7 constructs: `world`, `actor`, `converge`, `orchestrate`, `patch`, `law`, `pulse`, `resonate`, `shatter`, `teleport`, `axiom`, `entangle`, `component`, `shader`, `comptime`, `macro`, `test`, `spawn`, `send`, `share`, `fanout` | Keeps the bootstrap minimal and avoids complexity the compiler cannot yet compile |
| **C-3** | `orchestrator.kn` (markscript integration) MAY use `world` for compiler session state if it simplifies the design | The orchestrator is not part of the bootstrap critical path; it's the orchestration layer that runs AFTER the compiler core compiles |
| **C-4** | The compiler MUST parse all 38 Item kinds even though it only typechecks ~10 of them (fn, struct, enum, trait, impl, const, type, use, mod, include) | The self-host compiler must parse ALL Kain constructs to self-host; the stub strategy handles typechecking for the rest |
| **C-5** | AST SHALL use flat `Array<AstNode>` representation with integer index references, NOT recursive references | Enables cache locality, serializability, and mapping to LLVM types without recursive types |
| **C-6** | The compiler SHALL NOT use `Box<T>`, `Arc<T>`, or any heap-allocated recursive data structures for AST | The flat array model eliminates the need for these entirely |
| **C-7** | The lexer SHALL be a hand-written DFA, NOT a regex-based scanner | The Rust bootstrap uses Logos (Rust-only); Kain must lex itself without Rust dependencies |
| **C-8** | LLVM-C API calls SHALL be made through `include <llvm-c/Core.h> as llvm` and similar for Target, Orc, Analysis headers | This is the proven first-class Kain FFI mechanism that already extracts 605 functions from windows.h and 755 from vulkan.h |
| **C-9** | The compiler SHALL link against the EXISTING `kain_runtime.lib` (47 C files) — the runtime is NOT reimplemented in Kain | The C runtime is a stable ABI; rewriting it provides zero value and would break compatibility |
| **C-10** | The MarkScript VM SHALL be embedded via `use std::markscript` and the 20 public API functions — NO direct internal function calls | Encapsulation boundary ensures the compiler doesn't break when markscript internals change |
| **C-11** | The compiler SHALL be compilable by the Rust bootstrap compiler (for initial bootstrap) | The Rust bootstrap must be able to build kainc initially; this constrains the Kain features used to those the Rust bootstrap already supports |
| **C-12** | Source files SHALL be UTF-8 encoded | Kain strings are UTF-8; non-UTF-8 files are undefined behavior |
| **C-13** | Target platforms SHALL be x86-64 Windows (primary) and x86-64 Linux (secondary) | The Rust bootstrap targets these; ARM macOS is a future extension |
| **C-14** | Codegen Path A (textual .ll emission) SHALL be the default and must always work | Text-based emission is the proven path from the Rust bootstrap (21,289 lines); it requires zero LLVM library linkage |

---

## Out of Scope (Explicitly)

The following items are **NOT** in scope for the self-host compiler and remain the responsibility of the Rust bootstrap, external tooling, or future work:

- **GPU shader compilation** (SPIR-V, HLSL, WGSL, CUDA/PTX emission) — remains in `crates/gpu/` and `crates/gpu-runtime/` Rust crates
- **UE5 code generation** (Unreal Engine plugin code) — remains in the Rust bootstrap
- **WebAssembly target** (WASM, JS/TS/Hybrid backends) — remains in the Rust bootstrap
- **Interpret target** (direct AST evaluation without codegen) — remains in the Rust bootstrap
- **LSP server** — remains in the Rust bootstrap; future Kain implementation
- **Rust-to-Kain auto-translation** (Phase 1/2 selfhost mirroring pipeline) — remains in `crates/cli/src/selfhost*.rs`
- **Rust DLL bridge** (`bridge.kn` routing to Rust for bootstrap phases) — optional Phase 1 accelerant, not required for pure-Kain ouroboros
- **Python import pipeline** — remains in Rust bootstrap
- **TypeScript import pipeline** — remains in Rust bootstrap
- **MarkScript engine implementation** — markscript is already built (7,500 lines, 114 tests); the self-host compiler EMBEDS it, does NOT rewrite it
- **Native C runtime** (`runtime/native/`) — the 47 C files are maintained separately; kainc links against them, does NOT compile them (except during bootstrap linking)
- **Kain standard library** (`stdlib/`) — the 67 stdlib modules (~3,250 public symbols) exist independently; the self-host compiler USES `std::*` imports, does NOT reimplement them
- **Bazel build system integration** — remains separate
- **Crash handling infrastructure** — handled by the native C runtime's `__kain_crash_handler_init`
- **DWARF debug info emission** — future enhancement; not required for ouroboros
- **Full JSX semantic validation** — stubbed (parse only); the compiler itself uses no JSX
- **Actor scheduler optimization** — the C runtime handles this; compiler only emits the correct `declare` statements
- **Cross-compilation to non-x86-64 targets** — the self-host compiler targets the host platform; cross-compilation is future work

---

## Dependencies

| Dependency | Type | Status |
|------------|------|--------|
| **Rust bootstrap compiler** (`kain.exe`) | Hard — must compile kainc for initial bootstrap | Available (exists, 67 crates) |
| **LLVM 14+ development libraries** (llvm-c/Core.h, Orc.h, Target.h, Analysis.h) | Hard — required for Path B codegen and JIT | Available (system installation or bundled) |
| **libclang** (for `include <header.h>` C header import) | Hard — required for LLVM-C FFI and C interop | Available (bundled with LLVM) |
| **clang** (linker and C compiler for runtime) | Hard — required to link kainc's output | Available (system installation or bundled) |
| **Kain native C runtime** (`kain_runtime.lib`) | Hard — the stable ABI that generated code links against | Available (47 C files in `runtime/native/`) |
| **MarkScript VM** (`std::markscript`) | Hard — the embedded orchestration layer | Available (7,500 lines, 114 tests, proven embedding API) |
| **Kain stdlib** (`stdlib/`, 67 modules, ~3,250 symbols) | Hard — kainc uses `std::text`, `std::machine`, `std::fs`, `std::diagnostics`, etc. | Available |
| **Windows SDK** (for Windows builds) | Soft — required only for Windows platform; VM subsystem calls go through the C runtime | Available on developer machines |
| **metal.kn benchmark cases** (Cases 0, 1, 2, 4, 5, 10) | Soft — prove JIT primitives work; not required at compile time | Available (`benchmark/cases_v2/metal.kn`) |
| **Ouroboros verification framework** (byte-identical binary comparison) | Soft — required for acceptance testing; not required for compilation | To be developed as part of selfhost subcommand |

---

## Requirements Traceability

Each functional requirement maps back to the research documents that define the underlying specification:

| FR Group | Research Document | Key Sections |
|----------|------------------|--------------|
| **FR-LEX** (Lexer) | `01-lexer-parser-ast.md` | §§2.1–2.6 (102 token kinds, 58 hard keywords, indent processor) |
| **FR-PARSE** (Parser/AST) | `01-lexer-parser-ast.md` | §§3.1–3.7 (Pratt parser, items, stmts, expressions, JSX), §§4.1–4.8 (flat array AST, 38 items, 64 exprs) |
| **FR-TYPE** (Typechecker) | `02-typechecker-types.md` | §§1.1–1.7 (20 ResolvedType, types_compatible), §§2.1–2.4 (4-pass pipeline), §§3.1–3.8 (effects), §§4.1–4.5 (monomorphize) |
| **FR-CODEGEN** (LLVM Codegen) | `03-llvm-codegen-jit.md` | §§2.1–2.3 (two paths), §§3.1–3.4 (type mapping), §§4.1–4.9 (LLVM IR patterns), §5 (runtime declarations), §6 (LLVM-C API) |
| **FR-JIT** (Dual JIT) | `06-jit-markscript-metal-architecture.md` | §§1.1–2.2 (dual path), §§3.1–3.9 (markscript JIT), §§4.1–4.6 (metal primitives), §§5.1–5.3 (OrcJIT), §8 (W^X contract) |
| **FR-CLI** (CLI Driver) | `04-cli-driver-selfhost.md` | §§3.1–3.3 (subcommand tree), §§4.1–4.3 (DriverSession), §§5.1–5.3 (workspace), §§7.1–7.4 (bootstrap) |
| **FR-RUNTIME** (Runtime/FFI) | `05-runtime-contract-ffi.md` | §§3.1–3.6 (libclang pipeline), §§4.1–4.5 (@extern ABI), §§5.1–5.12 (runtime function table), §§6.1–6.3 (type mapping), §8 (three-layer pattern) |
| **FR-ORCH** (MarkScript) | `07-markscript-fusion-contract.md` | §§1.1–2.3 (fusion thesis), §§3.1–3.6 (embedding API), §§4.1–4.3 (IVT handlers), §§5.1–5.4 (build config), §§6.1–6.2 (build pipeline) |
| **NFR (all)** | `SELFHOST-KN.MD` | §1 (nuclear thesis), §2 (minimal constructs), §3 (architecture), §8 (DLL bridge), §9 (phase plan) |

---

## Parallel Development Streams

The 7 subsystems can be developed in the following parallel configuration:

```
STREAM A ─── FR-LEX (lexer.kn, token.kn) ───┐
STREAM B ─── FR-PARSE (ast.kn, parser.kn) ──┤
STREAM C ─── FR-TYPE (types.kn, effects.kn) ─┤
STREAM D ─── FR-CODEGEN (codegen.kn, llvm_ffi.kn) ─┼─── FR-CLI (driver.kn, cli.kn) ─── FR-ORCH (orchestrator.kn)
STREAM E ─── FR-JIT (jit_metal.kn, jit_x86.kn, jit_orc.kn) ─┤
STREAM F ─── FR-RUNTIME (runtime.kn, builtins.kn) ─┤
STREAM G ─── FR-ORCH (orchestrator.kn) ─────────────┘

WAVE 1: Streams A, B, D, E, F start in parallel (they share only type definitions, which can be agreed upon upfront)
WAVE 2: Stream C starts after Stream B delivers AST type definitions
WAVE 3: FR-CLI integrates all subsystems after they are individually testable
WAVE 4: FR-ORCH integrates after FR-CLI exposes a CLI surface
```

**Minimum shared artifacts (defined upfront for parallel work):**
1. `TokenKind` enum (102 variants) — needed by FR-LEX, FR-PARSE
2. `AstNode` struct format (flat array, kind+data fields) — needed by FR-PARSE, FR-TYPE, FR-CODEGEN
3. `ResolvedType` enum (20 variants) — needed by FR-TYPE, FR-CODEGEN
4. `CompilerError` / `Diagnostic` struct — needed by all streams
5. `BuildConfig` struct — needed by FR-CLI, FR-ORCH
6. IVT handler ID constants (200–208) — needed by FR-ORCH

---

## Validation Checklist

Before handing off to Design Agent (Phase 2):

- [x] All user roles identified (Kain Developer, Compiler Contributor, Bootstrap Compiler, LLVM Toolchain, Kain Runtime, MarkScript VM, Downstream Tooling)
- [x] Every user story has EARS acceptance criteria (6 stories, 30 criteria)
- [x] Normal, edge, and error cases covered (27 edge cases, 28 error cases)
- [x] All requirements are testable and measurable (all FR-* use EARS format)
- [x] No conflicting requirements
- [x] Constraints and dependencies documented (14 constraints, 11 dependencies)
- [x] Out-of-scope items explicitly listed (16 items)
- [x] Requirements traceability to research documents (8 traceability entries)
- [x] 7 parallel development streams identified
- [x] 6 shared artifacts specified for upstream coordination
- [x] ~103 functional requirements across 7 subsystems
- [x] 13 non-functional requirements (6 performance, 3 correctness, 3 code size, 2 memory, 2 observability, 2 security)
