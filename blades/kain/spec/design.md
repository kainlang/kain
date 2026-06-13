# Design: Kain Self-Host Compiler (kainc)

**Phase:** 2 of 3 — Design
**Created:** 2026-06-12
**Status:** Draft
**Based on:** /spec/requirements.md (Phase 1)
**Next:** /spec/tasks.md (Phase 3 — Task Agent)

---

## Overview

The Kain Self-Host Compiler (kainc) is a **pure-Kain, LLVM-native compiler that can compile itself**. It decomposes into 7 independently buildable subsystems connected by 6 shared type definitions that form the contract between parallel development streams. The compiler core uses only Layer 0 Kain constructs (fn, struct, enum, trait, impl, ptr, collapse/observe/decay, Pure/Unsafe/IO effects) with a small Unsafe bridge to the LLVM-C API. The MarkScript VM serves as the embedded orchestration layer — build config, pipeline, test runner, and REPL all delegate to markscript (0 new lines of orchestration code). The ultimate acceptance criterion is **ouroboros** — kainc compiles its own source to produce a byte-identical binary.

**Key architectural numbers:**
- 7 subsystems, ~13,000 total lines of Kain (vs 519K lines of Rust bootstrap)
- 6 shared type definitions (TokenKind, Token, AstNode, ResolvedType, Diagnostic, BuildConfig)
- 2 parallel compilation paths (textual .ll emission + LLVM-C API OrcJIT)
- 4-pass typecheck pipeline (predeclare → register → re-register → check)
- 9 markscript IVT handlers (IDs 200-208) bridging intents to compiler core
- 200+ runtime function declarations emitted as LLVM `declare` statements
- ~103 functional requirements, 13 non-functional requirements, 27 edge cases, 28 error cases

---

## Architecture

### System Context

```
                    ┌─────────────────────────────────────┐
                    │        kainc (Self-Host Compiler)    │
                    │                                     │
  Kain Source ────▶ │  ┌───────────────────────────────┐  │
  (.kn files)      │  │  7-Subsystem Compiler Core     │  │────▶ LLVM IR (.ll)
                    │  │  LEX → PARSE → TYPE → CODEGEN │  │────▶ Native Binary (.exe)
  build.md ──────▶ │  │  JIT (dual path)               │  │────▶ JIT Result (Int)
  buildex.md      │  │  CLI driver                    │  │
                    │  └───────────┬───────────────────┘  │────▶ JSON Diagnostics
  KAIN.toml ─────▶ │              │                       │
                    │  ┌───────────▼───────────────────┐  │
                    │  │  MarkScript Orchestration     │  │
                    │  │  (embedded VM, ~500 L integ.) │  │
                    │  └───────────────────────────────┘  │
                    └──────────────┬──────────────────────┘
                                   │
                    ┌──────────────▼──────────────────────┐
                    │         External Dependencies        │
                    │  LLVM 14+ (libLLVM.dll / libLLVM.so) │
                    │  clang (linker, C compiler)          │
                    │  kain_runtime.lib (47 C files)       │
                    │  libclang (C header import)          │
                    │  Kain stdlib (67 modules)            │
                    └─────────────────────────────────────┘
```

### Component Decomposition

| Subsystem | Prefix | Files (~lines) | Responsibility | Dependencies |
|-----------|--------|-----------------|----------------|--------------|
| **Lexer** | LEX | `lexer.kn` (~500), `token.kn` (~150), `span.kn` (~100) | Tokenize source → `Array<Token>`; indent processing; 102 token kinds | None (self-contained) |
| **Parser + AST** | PARSE | `parser.kn` (~3000), `ast.kn` (~500) | Parse tokens → flat `Array<AstNode>`; Pratt expression engine; 38 item kinds, 64 expr variants | LEX (Token, TokenKind) |
| **Typechecker** | TYPE | `types.kn` (~1500), `effects.kn` (~200), `monomorphize.kn` (~400) | 4-pass pipeline; 20 ResolvedType variants; effect checking; generic monomorphization; stub strategy for L1-7 | PARSE (AstNode) |
| **LLVM Codegen** | CODEGEN | `codegen.kn` (~2000), `llvm_ffi.kn` (~1000) | Two-path emission: textual .ll (Path A) and LLVM-C API (Path B); 200+ runtime declares; type→LLVM mapping | TYPE (TypedProgram) |
| **Dual JIT** | JIT | `jit.kn` (~300), `jit_metal.kn` (~200), `jit_x86.kn` (~500), `jit_orc.kn` (~400), `jit_cache.kn` (~200) | Path A: markscript-style x86-64 direct emission; Path B: OrcJIT; W^X lifecycle; cache | CODEGEN, LLVM-C FFI |
| **CLI Driver** | CLI | `compiler.kn` (~200), `cli.kn` (~300), `main.kn` (~100) | Subcommand tree; DriverSession pipeline; workspace discovery; diagnostics formatting | All subsystems (wired via orchestration) |
| **MarkScript Orchestration** | ORCH | `orchestrator.kn` (~500) | Embed markscript VM; register 9 IVT handlers; load build config/pipeline; dispatch | CLI, MarkScript VM (`std::markscript`) |
| **Runtime Contract** | RUNTIME | `runtime.kn` (~300), `builtins.kn` (~200), `llvm_ffi.kn` (shared) | @extern ABI contract; 3-layer stdlib pattern; KainType↔CType mapping; runtime function table | CODEGEN (shared llvm_ffi) |

### Data Flow

The primary compilation data flow is:

```
Source (.kn) → [LEX] Array<Token> → [PARSE] Array<AstNode> → [TYPE] TypedProgram
    → [MONOMORPHIZE] MonomorphizedProgram → [CODEGEN] LLVM IR / Native Code
    → [JIT] execute OR [LINK] .exe
```

```
CLI invocation: kainc build src/
    → [ORCH] load build.md config, execute buildex.md pipeline
    → [CLI] DriverSession orchestrates:
        → Resolve (workspace discovery, use resolution, source aggregation)
        → [LEX] tokenize → Array<Token>
        → [PARSE] parse → Array<AstNode>
        → [TYPE] 4-pass typecheck → TypedProgram
        → [MONOMORPHIZE] generic instantiation
        → [CODEGEN] Path A (.ll) or Path B (LLVM-C API)
        → [LINK] clang + kain_runtime.lib → .exe
```

---

## Shared Type Definitions (CRITICAL — The Parallel Stream Contract)

These 6 types are the contract that enables 7 parallel development streams. Every subsystem consumes at least one of these types. They must be defined FIRST and agreed upon before any subsystem implementation begins.

### 1. TokenKind Enum (102 variants)

Defined in `token.kn`. A flat enum with integer discriminant values.

```kn
// token.kn — TokenKind enum (102 variants)
// Consumed by: LEX, PARSE
// Hard-lexer keywords (58): recognized in ALL syntactic positions

pub enum TokenKind:
    // ── Hard Keywords: Core Control & Binding (20) ──
    Fn = 0        // "fn"
    Let = 1       // "let"
    Mut = 2       // "mut"
    Var = 3       // "var"
    Const = 4     // "const"
    If = 5        // "if"
    Else = 6      // "else"
    Elif = 7      // "elif"
    Match = 8     // "match"
    For = 9       // "for"
    While = 10    // "while"
    Loop = 11     // "loop"
    Break = 12    // "break"
    Continue = 13 // "continue"
    Defer = 14    // "defer"
    Return = 15   // "return"
    Await = 16    // "await"
    In = 17       // "in"
    With = 18     // "with"
    As = 19       // "as"

    // ── Hard Keywords: Types, Modules & Visibility (10) ──
    TypeKw = 20   // "type"
    Struct = 21   // "struct"
    Enum = 22     // "enum"
    Trait = 23    // "trait"
    Impl = 24     // "impl"
    Pub = 25      // "pub"
    Mod = 26      // "mod"
    Use = 27      // "use"
    SelfLower = 28 // "self"
    SelfUpper = 29 // "Self"

    // ── Hard Keywords: Built-in Literals (3) ──
    True = 30     // "true"
    False = 31    // "false"
    None = 32     // "none"

    // ── Hard Keywords: Effects (7) ──
    Pure = 33     // "Pure"
    Io = 34       // "IO"
    AsyncKw = 35  // "async" (lowercase, for async fn)
    Async = 36    // "Async" (uppercase, for with Async)
    Gpu = 37      // "GPU"
    Reactive = 38 // "Reactive"
    Unsafe = 39   // "Unsafe"

    // ── Hard Keywords: First-Class Citizens (18) ──
    Component = 40   // "component"
    Shader = 41      // "shader"
    Actor = 42       // "actor"
    State = 43       // "state"
    Spawn = 44       // "spawn"
    Send = 45        // "send"
    Receive = 46     // "receive" (reserved — no parser rule)
    Emit = 47        // "emit"
    Comptime = 48    // "comptime"
    Macro = 49       // "macro"
    Vertex = 50      // "vertex"
    Fragment = 51    // "fragment"
    Collapse = 52    // "collapse"
    Observe = 53     // "observe"
    Decay = 54       // "decay"
    Share = 55       // "share"
    Fanout = 56      // "fanout"
    Test = 57        // "test"

    // ── Operator Aliases (2, share variant with symbolic) ──
    // "and" → And (58); "or" → Or (59)
    // Note: These are TokenKind variants AND operator aliases

    // ── Operators (25) ──
    PlusPlus = 60    // "++"
    MinusMinus = 61  // "--"
    Plus = 62        // "+"
    Minus = 63       // "-"
    Star = 64        // "*"
    Slash = 65       // "/"
    Percent = 66     // "%"
    Power = 67       // "**"
    EqEq = 68        // "=="
    NotEq = 69       // "!="
    Lt = 70          // "<"
    Gt = 71          // ">"
    LtEq = 72        // "<="
    GtEq = 73        // ">="
    And = 74         // "&&" (also "and")
    Or = 75          // "||" (also "or")
    Not = 76         // "!"
    Amp = 77         // "&"
    Pipe = 78        // "|"
    Caret = 79       // "^"
    Tilde = 80       // "~"
    Shl = 81         // "<<"
    Shr = 82         // ">>"
    Eq = 83          // "="
    Arrow = 84       // "->"

    // ── Compound Assignment Operators (11) ──
    PlusEq = 85      // "+="
    MinusEq = 86     // "-="
    StarEq = 87      // "*="
    SlashEq = 88     // "/="
    PercentEq = 89   // "%="
    AmpEq = 90       // "&="
    PipeEq = 91      // "|="
    CaretEq = 92     // "^="
    ShlEq = 93       // "<<="
    ShrEq = 94       // ">>="

    // ── Punctuation (15) ──
    LParen = 95      // "("
    RParen = 96      // ")"
    LBracket = 97    // "["
    RBracket = 98    // "]"
    LBrace = 99      // "{"
    RBrace = 100     // "}"
    Comma = 101      // ","
    Dot = 102        // "."
    DotDot = 103     // ".."
    DotDotDot = 104  // "..."
    Colon = 105      // ":"
    ColonColon = 106 // "::"
    Semi = 107       // ";"
    FatArrow = 108   // "=>"
    At = 109         // "@"

    // ── Special (4) ──
    QuestionQuestion = 110  // "??"
    QuestionDot = 111       // "?."
    Question = 112          // "?"
    LtSlash = 113           // "</"

    // ── Non-Keyword Tokens (6) ──
    Ident = 114      // identifier (includes contextual keywords as Ident)
    Int = 115        // integer literal (payload in Token.literal_int)
    Float = 116      // float literal (payload in Token.literal_float)
    String = 117     // string literal (payload in Token.literal_string)
    FString = 118    // format string "f"..."
    Char = 119       // character literal 'c'

    // ── Synthetic (inserted by indent processor, never from lexer) ──
    Newline = 120    // newline + captured whitespace
    Indent = 121     // synthetic: indentation increased
    Dedent = 122     // synthetic: indentation decreased
    Eof = 123        // end of file marker
    Comment = 124    // skipped (never reaches parser)
    HashComment = 125 // skipped (never reaches parser)

    // ── Error ──
    Error = 126      // unrecognized character
```

### 2. Token Struct

Defined in `token.kn`. Every token carries its kind, source position, and optional literal value.

```kn
// token.kn — Token struct
// Consumed by: LEX (producer), PARSE (consumer)

pub struct Token:
    kind:          TokenKind    // discriminant (see TokenKind enum)
    text:          String       // raw source text of this token
    line_no:       Int          // 1-based line number
    col_no:        Int          // 1-based column number
    byte_offset:   Int          // byte offset from start of source
    // ── Optional literal payload ──
    literal_int:   Int          // valid when kind == Int
    literal_float: Float        // valid when kind == Float
    literal_string: String      // valid when kind == String, FString, Char, Newline
```

**Design note:** The `literal_*` fields are zeroed when not applicable. The `kind` field discriminates. This flat struct avoids the overhead of `Option<T>` wrapping and maps cleanly to stack-allocated arrays.

### 3. AstNode Struct — THE MOST CRITICAL DESIGN

Defined in `ast.kn`. The AST uses a **flat `Array<AstNode>` representation** where parent-child relationships are expressed via integer indices into the same array. This is the single most important design decision — it enables cache locality, serializability, and eliminates recursive types that would require heap allocation.

```kn
// ast.kn — Flat Array AST
// Consumed by: PARSE (producer), TYPE (consumer), CODEGEN (consumer)
//
// INVARIANT: Every AstNode has kind, span_start, span_end, and data[].
// The interpretation of data[] depends on kind.
// Children are referenced by INDEX into the AstNode array, not by pointer.

pub struct AstNode:
    kind:       Int    // tag discriminant (see AstNodeKind constants below)
    span_start: Int    // byte offset of first token
    span_end:   Int    // byte offset of last token
    data:       Array<Int>  // variable-length payload — interpretation depends on kind

// =============================================================================
// AstNodeKind Tag Constants (for the `kind` field of AstNode)
// =============================================================================

// ── Item Kinds (38) — top-level declarations ──
pub const AST_ITEM_FUNCTION:        Int = 0
pub const AST_ITEM_STRUCT:          Int = 1
pub const AST_ITEM_ENUM:            Int = 2
pub const AST_ITEM_TRAIT:           Int = 3
pub const AST_ITEM_IMPL:            Int = 4
pub const AST_ITEM_TYPE_ALIAS:      Int = 5
pub const AST_ITEM_USE:             Int = 6
pub const AST_ITEM_MOD:             Int = 7
pub const AST_ITEM_CONST:           Int = 8
pub const AST_ITEM_COMPTIME:        Int = 9
pub const AST_ITEM_MACRO:           Int = 10
pub const AST_ITEM_TEST:            Int = 11
pub const AST_ITEM_PATCH:           Int = 12
pub const AST_ITEM_LAW:             Int = 13
pub const AST_ITEM_AXIOM:           Int = 14
pub const AST_ITEM_CONVERGE:        Int = 15
pub const AST_ITEM_WORLD:           Int = 16
pub const AST_ITEM_ENTANGLE:        Int = 17
pub const AST_ITEM_ORCHESTRATE:     Int = 18
pub const AST_ITEM_PULSE:           Int = 19
pub const AST_ITEM_RESONATE:        Int = 20
pub const AST_ITEM_COMPONENT:       Int = 21
pub const AST_ITEM_SHADER:          Int = 22
pub const AST_ITEM_ACTOR:           Int = 23
pub const AST_ITEM_IMPORT:          Int = 24  // include/import/from
// UE5/gameplay items (25-36) — parsed but stubbed
pub const AST_ITEM_MATERIAL_GRAPH:  Int = 25
pub const AST_ITEM_GRAPH_EDITOR:    Int = 26
// ... (remaining UE5 items: 26-36)
pub const AST_ITEM_PROGRAM:         Int = 37  // root node

// ── Statement Kinds (12) ──
pub const AST_STMT_LET:             Int = 50
pub const AST_STMT_RETURN:          Int = 51
pub const AST_STMT_DEFER:           Int = 52
pub const AST_STMT_FOR:             Int = 53
pub const AST_STMT_FANOUT:          Int = 54
pub const AST_STMT_WHILE:           Int = 55
pub const AST_STMT_LOOP:            Int = 56
pub const AST_STMT_BREAK:           Int = 57
pub const AST_STMT_CONTINUE:        Int = 58
pub const AST_STMT_DISPATCH:        Int = 59
pub const AST_STMT_EXPR:            Int = 60
pub const AST_STMT_ITEM:            Int = 61  // nested item

// ── Expression Kinds (64) ──
pub const AST_EXPR_INT:             Int = 100
pub const AST_EXPR_FLOAT:           Int = 101
pub const AST_EXPR_STRING:          Int = 102
pub const AST_EXPR_FSTRING:         Int = 103
pub const AST_EXPR_BOOL:            Int = 104
pub const AST_EXPR_NONE:            Int = 105
pub const AST_EXPR_IDENT:           Int = 106
pub const AST_EXPR_BINARY:          Int = 107
pub const AST_EXPR_UNARY:           Int = 108
pub const AST_EXPR_CALL:            Int = 109
pub const AST_EXPR_METHOD_CALL:     Int = 110
pub const AST_EXPR_FIELD:           Int = 111
pub const AST_EXPR_INDEX:           Int = 112
pub const AST_EXPR_ASSIGN:          Int = 113
pub const AST_EXPR_IF:              Int = 114
pub const AST_EXPR_MATCH:           Int = 115
pub const AST_EXPR_BLOCK:           Int = 116
pub const AST_EXPR_RANGE:           Int = 117
pub const AST_EXPR_STRUCT_LIT:      Int = 118
pub const AST_EXPR_ENUM_VARIANT:    Int = 119
pub const AST_EXPR_ARRAY:           Int = 120
pub const AST_EXPR_TUPLE:           Int = 121
pub const AST_EXPR_REF:             Int = 122
pub const AST_EXPR_DEREF:           Int = 123
pub const AST_EXPR_CAST:            Int = 124
pub const AST_EXPR_TRY:             Int = 125
pub const AST_EXPR_AWAIT:           Int = 126
pub const AST_EXPR_SPAWN:           Int = 127
pub const AST_EXPR_SEND:            Int = 128
pub const AST_EXPR_EMIT:            Int = 129
pub const AST_EXPR_COLLAPSE:        Int = 130
pub const AST_EXPR_OBSERVE:         Int = 131
pub const AST_EXPR_DECAY:           Int = 132
pub const AST_EXPR_SHARE:           Int = 133
pub const AST_EXPR_TELEPORT:        Int = 134
pub const AST_EXPR_LAMBDA:          Int = 135
pub const AST_EXPR_ASM:             Int = 136
pub const AST_EXPR_ALLOC:           Int = 137
pub const AST_EXPR_PTR_OFFSET:      Int = 138
pub const AST_EXPR_MEM_LOAD:        Int = 139
pub const AST_EXPR_MEM_STORE:       Int = 140
pub const AST_EXPR_ATOMIC_LOAD:     Int = 141
pub const AST_EXPR_ATOMIC_STORE:    Int = 142
pub const AST_EXPR_ATOMIC_ADD:      Int = 143
pub const AST_EXPR_ATOMIC_CMPXCHG:  Int = 144
pub const AST_EXPR_ATOMIC_FENCE:    Int = 145
pub const AST_EXPR_CPU_FENCE:       Int = 146
pub const AST_EXPR_CPU_CACHE_FLUSH: Int = 147
pub const AST_EXPR_SIZEOF:          Int = 148
pub const AST_EXPR_ALIGNOF:         Int = 149
pub const AST_EXPR_BITCAST:         Int = 150
pub const AST_EXPR_JSX:             Int = 151
pub const AST_EXPR_MACRO_CALL:      Int = 152
pub const AST_EXPR_COMPTIME:        Int = 153
pub const AST_EXPR_UNINIT:          Int = 154
pub const AST_EXPR_ALLOCA:          Int = 155
pub const AST_EXPR_PAREN:           Int = 156
// ... remaining 8 expression kinds

// ── Pattern Kinds (9) ──
pub const AST_PAT_WILDCARD:         Int = 200
pub const AST_PAT_LITERAL:          Int = 201
pub const AST_PAT_BINDING:          Int = 202
pub const AST_PAT_STRUCT:           Int = 203
pub const AST_PAT_TUPLE:            Int = 204
pub const AST_PAT_VARIANT:          Int = 205
pub const AST_PAT_SLICE:            Int = 206
pub const AST_PAT_OR:               Int = 207
pub const AST_PAT_RANGE:            Int = 208

// ── Type AST Kinds (14) ──
pub const AST_TYPE_NAMED:           Int = 300
pub const AST_TYPE_TUPLE:           Int = 301
pub const AST_TYPE_ARRAY:           Int = 302
pub const AST_TYPE_SLICE:           Int = 303
pub const AST_TYPE_REF:             Int = 304
pub const AST_TYPE_PTR:             Int = 305
pub const AST_TYPE_FUNCTION:        Int = 306
pub const AST_TYPE_OPTION:          Int = 307
pub const AST_TYPE_RESULT:          Int = 308
pub const AST_TYPE_INFER:           Int = 309
pub const AST_TYPE_NEVER:           Int = 310
pub const AST_TYPE_UNIT:            Int = 311
pub const AST_TYPE_IMPL_TRAIT:      Int = 312
pub const AST_TYPE_GENERIC:         Int = 313

// ── BinaryOp Kinds (21) ──
pub const BINOP_ADD:     Int = 0
pub const BINOP_SUB:     Int = 1
pub const BINOP_MUL:     Int = 2
pub const BINOP_DIV:     Int = 3
pub const BINOP_MOD:     Int = 4
pub const BINOP_POW:     Int = 5
pub const BINOP_EQ:      Int = 6
pub const BINOP_NE:      Int = 7
pub const BINOP_LT:      Int = 8
pub const BINOP_GT:      Int = 9
pub const BINOP_LE:      Int = 10
pub const BINOP_GE:      Int = 11
pub const BINOP_AND:     Int = 12
pub const BINOP_OR:      Int = 13
pub const BINOP_BIT_AND: Int = 14
pub const BINOP_BIT_OR:  Int = 15
pub const BINOP_BIT_XOR: Int = 16
pub const BINOP_SHL:     Int = 17
pub const BINOP_SHR:     Int = 18
pub const BINOP_RANGE:       Int = 19
pub const BINOP_RANGE_INCL:  Int = 20

// ── UnaryOp Kinds (6) ──
pub const UNOP_NEG:     Int = 0
pub const UNOP_NOT:     Int = 1
pub const UNOP_BIT_NOT: Int = 2
pub const UNOP_REF:     Int = 3
pub const UNOP_REF_MUT: Int = 4
pub const UNOP_DEREF:   Int = 5
```

#### AstNode data[] Payload Encoding Scheme

The `data: Array<Int>` field is a variable-length integer array. Its interpretation depends on `kind`. The encoding rules:

1. **Child indices** are stored first in `data[]`, followed by literal values and flags.
2. **Literal values (Int/Float)** use repeated `data` slots: Int → 1 slot, Float → 2 slots (as bits), String → reference to string table index.
3. **Ident names** are stored as an index into the global string interning table (built during parsing).
4. **Negative numbers** are represented as signed Int values directly in data slots.

**Example encodings:**

```
AST_EXPR_BINARY:
    data[0] = left child AstNode index
    data[1] = right child AstNode index
    data[2] = BinaryOp kind (BINOP_ADD, etc.)

AST_EXPR_INT:
    data[0] = the integer value

AST_EXPR_IDENT:
    data[0] = string_table_index for the identifier name

AST_EXPR_CALL:
    data[0] = callee child AstNode index
    data[1] = argument count (N)
    data[2..2+N-1] = argument child indices

AST_EXPR_IF:
    data[0] = condition child index
    data[1] = then_branch child index
    data[2] = else_branch child index (or -1 if absent)

AST_ITEM_FUNCTION:
    data[0] = name string_table_index
    data[1] = attributes child index (or -1)
    data[2] = generic count (G)
    data[3..3+2*G-1] = generic param pairs (name_idx, bound_type_idx, ...)
    data[3+2*G] = parameter count (P)
    data[...] = parameter child indices (P entries)
    data[...] = return_type child index (or -1 for inferred)
    data[...] = where_clause child index (or -1)
    data[...] = effect count (E) + effect kind values
    data[...] = body child index (block expression)
```

### 4. ResolvedType Enum (20 variants)

Defined in `types.kn`. The central type representation — every expression, variable, and function has a ResolvedType. Represented as a flat struct with a `kind` discriminant tag and union-like fields. **NOT a recursive enum** — uses integer indices for compound type references.

```kn
// types.kn — ResolvedType
// Consumed by: TYPE (producer), CODEGEN (consumer), JIT (consumer)

pub struct ResolvedType:
    kind: Int            // discriminant (see RT_* constants below)
    // ── Fields used depending on kind ──
    int_size: Int        // for RT_INT: 8=I64, 4=I32, etc. Sign encoded in sign bit.
    float_size: Int      // for RT_FLOAT: 4=F32, 8=F64
    name: Int            // for RT_STRUCT, RT_ENUM: string table index
    inner_type: Int      // for RT_ARRAY, RT_SLICE, RT_OPTION, RT_FUTURE, RT_REF, RT_PTR: index into TypeEnv type array
    array_len: Int       // for RT_ARRAY: compile-time constant length
    tuple_types: Int     // for RT_TUPLE: index into TypeEnv type array (start of tuple element types)
    tuple_len: Int       // for RT_TUPLE: number of elements
    result_ok: Int       // for RT_RESULT: index of ok type
    result_err: Int      // for RT_RESULT: index of err type
    fn_params: Int       // for RT_FUNCTION: index into TypeEnv (start of param types)
    fn_ret: Int          // for RT_FUNCTION: index of return type
    fn_effects: Int      // for RT_FUNCTION: effect bitmask
    ref_mutable: Bool    // for RT_REF, RT_PTR: mutability flag

// ── ResolvedType kind constants (20 variants) ──
pub const RT_UNIT:     Int = 0   // void / empty tuple
pub const RT_BOOL:     Int = 1   // boolean (i1)
pub const RT_INT:      Int = 2   // integer (use int_size for width+sign)
pub const RT_FLOAT:    Int = 3   // float (use float_size for width)
pub const RT_STRING:   Int = 4   // UTF-8 string {ptr, len}
pub const RT_CHAR:     Int = 5   // Unicode scalar (i32)
pub const RT_ARRAY:    Int = 6   // [T; N] — fixed-size array
pub const RT_SLICE:    Int = 7   // [T] — fat pointer {ptr, len}
pub const RT_TUPLE:    Int = 8   // (T1, T2, ...)
pub const RT_REF:      Int = 9   // &T or &mut T
pub const RT_PTR:      Int = 10  // ptr<T> raw pointer
pub const RT_OPTION:   Int = 11  // Option<T>
pub const RT_RESULT:   Int = 12  // Result<T, E>
pub const RT_FUTURE:   Int = 13  // Future<T> (async return)
pub const RT_STRUCT:   Int = 14  // nominal struct (use name field)
pub const RT_ENUM:     Int = 15  // nominal enum (use name field)
pub const RT_FUNCTION: Int = 16  // fn(args) -> ret with effects
pub const RT_GENERIC:  Int = 17  // generic type parameter (use name field)
pub const RT_NEVER:    Int = 18  // bottom type (noreturn)
pub const RT_UNKNOWN:  Int = 19  // unresolved type (leniency valve)

// ── IntSize encoding ──
// Positive values = signed, negative values = unsigned
// 1=I8, 2=I16, 4=I32, 8=I64, 16=I128
// -1=U8, -2=U16, -4=U32, -8=U64, -16=U128
// 0=Isize (pointer-width signed), -100=Usize (pointer-width unsigned)

// ── Effect bitmask ──
pub const EFF_PURE:     Int = 0x00
pub const EFF_IO:       Int = 0x01
pub const EFF_GPU:      Int = 0x02
pub const EFF_ASYNC:    Int = 0x04
pub const EFF_REACTIVE: Int = 0x08
pub const EFF_UNSAFE:   Int = 0x10
pub const EFF_ALLOC:    Int = 0x20
pub const EFF_PANIC:    Int = 0x40
```

### 5. Diagnostic Struct

Defined in `error.kn`. All error and warning information flows through this struct.

```kn
// error.kn — Diagnostic
// Consumed by: ALL subsystems (producer), CLI (consumer for display)

pub struct Diagnostic:
    severity:    Int     // 0=error, 1=warning, 2=note, 3=help
    file_path:   String  // source file path (or "<stdin>")
    line:        Int     // 1-based line number
    column:      Int     // 1-based column number
    message:     String  // human-readable error message
    error_kind:  String  // machine-readable code (e.g. "E0001", "E_TYPE_MISMATCH")
    span_start:  Int     // byte offset of error start
    span_end:    Int     // byte offset of error end
    source_line: String  // the source line text (for caret display)

// ── Error Kind Constants ──
pub const ERR_LEX_UNTERMINATED_STRING: String = "E0001"
pub const ERR_LEX_UNEXPECTED_CHAR:     String = "E0002"
pub const ERR_PARSE_EXPECTED_TOKEN:    String = "E0100"
pub const ERR_PARSE_RESERVED_ID:       String = "E0101"
pub const ERR_PARSE_EXPECTED_ITEM:     String = "E0102"
pub const ERR_TYPE_MISMATCH:           String = "E0200"
pub const ERR_TYPE_DUPLICATE:          String = "E0201"
pub const ERR_TYPE_NOT_FOUND:          String = "E0202"
pub const ERR_TYPE_EFFECT_VIOLATION:   String = "E0203"
pub const ERR_TYPE_NON_EXHAUSTIVE:     String = "E0204"
pub const ERR_TYPE_CANNOT_ASSIGN_IMM:  String = "E0205"
pub const ERR_MONO_CONFLICT:           String = "E0300"
pub const ERR_MONO_TRAIT_BOUND:        String = "E0301"
pub const ERR_MONO_CANNOT_INFER:       String = "E0302"
pub const ERR_CODEGEN_VERIFY_FAILED:   String = "E0400"
pub const ERR_CODEGEN_TYPE_UNRESOLVED: String = "E0401"
pub const ERR_JIT_VM_MAP_FAILED:       String = "E0500"
pub const ERR_JIT_PROTECT_FAILED:      String = "E0501"
pub const ERR_JIT_ORC_INIT_FAILED:     String = "E0502"
pub const ERR_JIT_LOOKUP_FAILED:       String = "E0503"
pub const ERR_CLI_FILE_NOT_FOUND:      String = "E0600"
pub const ERR_CLI_WORKSPACE_NOT_FOUND: String = "E0601"
pub const ERR_CLI_LINKER_NOT_FOUND:    String = "E0602"
pub const ERR_RUNTIME_HEADER_NOT_FOUND: String = "E0700"
```

### 6. BuildConfig Struct

Defined in `build_config.kn` (auto-generated from markscript schema via `mks gen --target kain`).

```kn
// build_config.kn — BuildConfig struct (auto-generated from markscript schema)
// Consumed by: CLI, ORCH

pub struct BuildConfig:
    name:         String   // project name (e.g. "kainc")
    target:       String   // compilation target ("llvm", "c", "jit", etc.)
    profile:      String   // "debug" or "release"
    optimize:     Bool     // enable optimizations
    lto:          String   // LTO mode ("none", "thin", "full")
    entry:        String   // entry point file path
    source_root:  String   // root directory for source files
    deps:         String   // comma-separated dependencies
    output:       String   // output binary name
    runtime:      String   // runtime library name
    linker:       String   // linker binary (e.g. "clang")
    linker_flags: String   // flags passed to linker
    cc:           String   // C compiler (e.g. "clang")
    cc_flags:     String   // flags passed to C compiler
    test_root:    String   // test specification root
    doc_root:     String   // documentation root
```

---

## Component Specifications

### Subsystem: LEX — Lexer

**Files:** `src/lexer.kn` (~500 lines), `src/token.kn` (~150 lines), `src/span.kn` (~100 lines)

**Responsibility:** Convert UTF-8 source string into `Array<Token>`, perform indent processing to insert synthetic INDENT/DEDENT/NEWLINE/EOF tokens.

**Public Interface:**
```kn
// lexer.kn — main lexer module
pub fn lexer_new(source: String, file_path: String) -> LexerState
pub fn lexer_next_token(state: *mut LexerState) -> Token
pub fn lexer_tokenize_all(source: String, file_path: String) -> Array<Token>
    // Convenience: tokenizes entire source in one call

// indent.kn — indent processor (post-lexer pass)
pub fn indent_process(raw_tokens: Array<Token>) -> Array<Token>
    // Inserts synthetic INDENT, DEDENT, NEWLINE tokens
    // Suppresses newlines inside bracket groups
    // Discards blank lines
    // Appends EOF token

// token.kn — type definitions (see Shared Types above)
// span.kn — source location helpers
pub fn span_line_col(source: String, byte_offset: Int) -> Span
    // Converts byte offset to (line_no, col_no)
```

**Internal Data Structures:**
```kn
struct LexerState:
    source:      String        // original source text
    file_path:   String        // source file path for diagnostics
    pos:         Int           // current byte position in source
    line:        Int           // current 1-based line number
    col:         Int           // current 1-based column number
    tokens:      Array<Token>  // accumulated tokens
    errors:      Array<Diagnostic>  // accumulated lexer errors

struct IndentState:
    indent_stack: Array<Int>   // stack of indent levels (starts with [0])
    paren_depth:  Int          // depth of ()
    bracket_depth:Int          // depth of []
    brace_depth:  Int          // depth of {}
```

**Algorithm — Lexer DFA:**

The lexer is a hand-written DFA (Constraint C-7). It uses a single `advance()` method that examines the current character and dispatches:

```
lexer_next_token(state):
    skip_whitespace_except_newlines(state)
    c = current_char(state)

    if c == '\0' (EOF):                      return Token(Eof)
    if c == '\n':                             return lex_newline(state)
    if c == '/' && peek == '/':               skip_line_comment(state); continue
    if c == '#' && at_start_of_line:          skip_hash_comment(state); continue

    if c == '"':                              return lex_string(state)
    if c == 'f' && peek == '"':               return lex_fstring(state)
    if c == '\'':                             return lex_char(state)
    if is_digit(c) || (c == '0' && peek in 'xob'): return lex_number(state)
    if is_alpha(c) || c == '_':               return lex_ident_or_keyword(state)

    // Operators: try longest match first
    if c == '+' && peek == '+':               advance(2); return Token(PlusPlus)
    if c == '+' && peek == '=':               advance(2); return Token(PlusEq)
    if c == '+' :                              advance(1); return Token(Plus)
    // ... (similar for all 25 operators, 11 compound assignments, 15 punctuation)

    else:                                      return Token(Error, "unexpected character")
```

**Keyword Recognition:**

After identifying an identifier (`[a-zA-Z_][a-zA-Z0-9_]*`), the lexer performs a hash-table lookup against the 58 hard-keyword strings. If found, the corresponding `TokenKind` variant is used. If not found, the token is `TokenKind::Ident(name)`. Contextual keywords (51 of them) are NEVER recognized by the lexer — they always arrive as `TokenKind::Ident`.

**Indent Processor Algorithm (Pseudocode):**
```
indent_process(raw_tokens):
    result = []
    indent_stack = [0]
    paren_depth = bracket_depth = brace_depth = 0

    for each token in raw_tokens:
        if token is Newline:
            if paren_depth > 0 or bracket_depth > 0 or brace_depth > 0:
                continue  // suppress inside brackets
            if next_token is Newline:
                continue  // suppress blank lines

            indent = compute_indent(token.literal_string)  // tab→4 spaces
            current = indent_stack.top()

            if indent > current:
                indent_stack.push(indent)
                result.push(Newline)
                result.push(Indent)
            elif indent < current:
                result.push(Newline)
                while indent_stack.len() > 1 and indent_stack.top() > indent:
                    indent_stack.pop()
                    result.push(Dedent)
            else:
                result.push(Newline)

        else:
            update_bracket_depths(token)
            result.push(token)

    // EOF cleanup
    while indent_stack.len() > 1:
        indent_stack.pop()
        result.push(Dedent)
    result.push(Eof)
    return result
```

**Error Handling:**
- Unterminated string literal: Emit `ERR_LEX_UNTERMINATED_STRING`, treat newline as string terminator, continue
- Unexpected character: Emit `ERR_LEX_UNEXPECTED_CHAR` with character and position, advance 1 byte, continue
- Errors are accumulated; lexer never panics on malformed input

**Dependencies:** None (self-contained)

**Performance Target (NFR-P3):** 1MB/s tokenization rate on x86-64

---

### Subsystem: PARSE — Parser & AST

**Files:** `src/parser.kn` (~3000 lines), `src/ast.kn` (~500 lines)

**Responsibility:** Parse `Array<Token>` into a flat `Array<AstNode>`. Implement Pratt expression parser (16 precedence levels), recursive-descent item/statement parsing, JSX parsing, and error recovery via synchronization.

**Public Interface:**
```kn
// parser.kn
pub fn parser_new(tokens: Array<Token>, file_path: String) -> ParserState
pub fn parse(state: *mut ParserState) -> Array<AstNode>
    // Top-level entry point. Returns flat array where root is AST_ITEM_PROGRAM node.

// Internal dispatch:
pub fn parse_item(state: *mut ParserState, vis: Int, attrs: Array<Int>) -> Int
    // Returns AstNode index. Dispatches on token kind to 38 specialized parsers.
pub fn parse_stmt(state: *mut ParserState) -> Int
    // Returns AstNode index. Dispatches on token kind to statement parsers.
pub fn parse_expr(state: *mut ParserState) -> Int
    // Pratt entry: parse_assignment → parse_conditional → parse_range → parse_coalesce → parse_binary(0)
pub fn parse_binary(state: *mut ParserState, min_prec: Int) -> Int
    // Pratt core loop
pub fn parse_unary(state: *mut ParserState) -> Int
    // Prefix operators and ownership expressions
pub fn parse_postfix(state: *mut ParserState, base: Int) -> Int
    // Call, index, field access, method call, post-increment, try, cast, null-coalesce
pub fn parse_primary(state: *mut ParserState) -> Int
    // Literals, idents, parens, blocks, JSX, if-expr, match, struct lit, array, lambda

// ast.kn — type definitions and node constructors
pub fn ast_new_node(kind: Int, span_start: Int, span_end: Int, data: Array<Int>) -> AstNode
pub fn ast_push_child(program: *mut Array<AstNode>, parent: Int, child: Int)
pub fn ast_get_child(program: Array<AstNode>, parent: Int, slot: Int) -> AstNode
pub fn ast_data_len(program: Array<AstNode>, index: Int) -> Int
```

**Internal Data Structures:**
```kn
struct ParserState:
    tokens:         Array<Token>     // input token stream (after indent processing)
    pos:            Int              // current token index
    program:        Array<AstNode>   // flat AST under construction
    errors:         Array<Diagnostic> // accumulated errors (max 50)
    string_table:   Array<String>    // identifier/string interning table
    string_to_idx:  HashMap<String, Int>  // reverse lookup for deduplication
    loop_stack:     Array<LoopLabel> // (continue_label_idx, break_label_idx) for break/continue
    injected:       Array<Token>     // buffer for synthetic tokens (>> → > >)
    synth_counter:  Int              // counter for synthetic variable names
```

**Precedence Table (16 levels, Pratt Core):**
```
Level  1:  ||, or        (BinaryOp::Or)              LEFT
Level  2:  &&, and       (BinaryOp::And)             LEFT
Level  3:  |             (BinaryOp::BitOr)           LEFT
Level  4:  ^             (BinaryOp::BitXor)          LEFT
Level  5:  &             (BinaryOp::BitAnd)          LEFT
Level  6:  ==, !=        (BinaryOp::Eq, Ne)          LEFT
Level  7:  <, >, <=, >=  (BinaryOp::Lt, Gt, Le, Ge)  LEFT
Level  8:  <<, >>        (BinaryOp::Shl, Shr)        LEFT
Level  9:  +, -          (BinaryOp::Add, Sub)        LEFT
Level 10:  *, /, %       (BinaryOp::Mul, Div, Mod)   LEFT
Level 11:  **            (BinaryOp::Pow)              RIGHT
```

**Item Parsing Dispatch Table:**
```
TokenKind          → Item                    → parse_*() function
Fn                 → AST_ITEM_FUNCTION       → parse_function()
AsyncKw            → AST_ITEM_FUNCTION       → parse_async_function()
Struct             → AST_ITEM_STRUCT         → parse_struct()
Enum               → AST_ITEM_ENUM           → parse_enum()
Trait              → AST_ITEM_TRAIT          → parse_trait()
Impl               → AST_ITEM_IMPL           → parse_impl()
TypeKw             → AST_ITEM_TYPE_ALIAS     → parse_type_alias()
Use                → AST_ITEM_USE            → parse_use()
Mod                → AST_ITEM_MOD            → parse_mod()
Const              → AST_ITEM_CONST          → parse_const()
Comptime           → AST_ITEM_COMPTIME       → parse_comptime()
Macro              → AST_ITEM_MACRO          → parse_macro()
Test               → AST_ITEM_TEST           → parse_test()
Component          → AST_ITEM_COMPONENT      → parse_component()
Shader             → AST_ITEM_SHADER         → parse_shader()
Actor              → AST_ITEM_ACTOR          → parse_actor()
Ident("patch")     → AST_ITEM_PATCH          → parse_patch()
Ident("law")       → AST_ITEM_LAW            → parse_law()
Ident("axiom")     → AST_ITEM_AXIOM          → parse_axiom()
Ident("converge")  → AST_ITEM_CONVERGE       → parse_converge()
Ident("world")     → AST_ITEM_WORLD          → parse_world()
Ident("entangle")  → AST_ITEM_ENTANGLE       → parse_entangle()
Ident("orchestrate") → AST_ITEM_ORCHESTRATE  → parse_orchestrate()
Ident("pulse")     → AST_ITEM_PULSE          → parse_pulse()
Ident("resonate")  → AST_ITEM_RESONATE       → parse_resonate()
Ident("shatter")   → AST_ITEM_STRUCT         → parse_shatter_struct() (struct + attr)
Ident("include")   → AST_ITEM_IMPORT         → parse_include()
Ident("import")    → AST_ITEM_IMPORT         → parse_import()
Ident("from")      → AST_ITEM_IMPORT         → parse_from_import()
```

**Error Recovery Strategy:**
```
synchronize(state):
    // Skip tokens until we find an item boundary:
    // (1) a token at indent depth 0, or
    // (2) a keyword that starts an item (fn, struct, enum, pub, etc.), or
    // (3) EOF
    while state.pos < len(state.tokens):
        if is_at_indent_depth_zero or is_item_start_keyword:
            break
        state.pos += 1
```

**Performance Target (NFR-P4):** 500K tokens/s parsing rate

**Dependencies:** LEX (Token, TokenKind structs), shared ast.kn type definitions

---

### Subsystem: TYPE — Typechecker

**Files:** `src/types.kn` (~1500 lines), `src/effects.kn` (~200 lines), `src/monomorphize.kn` (~400 lines)

**Responsibility:** 4-pass typecheck pipeline (predeclare → register → re-register → check), effect checking via `can_call` lattice, generic monomorphization via `unify`/`substitute_type`, stub strategy for Layers 1–7.

**Public Interface:**
```kn
// types.kn — main typechecker
pub fn type_env_new() -> TypeEnv
    // Create a fresh type environment with primitive types pre-registered
pub fn typecheck(env: *mut TypeEnv, program: Array<AstNode>) -> TypedProgram
    // Run the full 4-pass pipeline. Returns TypedProgram with all errors accumulated.

// ── 4-Pass Pipeline ──
pub fn pass1_predeclare(env: *mut TypeEnv, program: Array<AstNode>)
pub fn pass2_register(env: *mut TypeEnv, program: Array<AstNode>) -> Array<Bool>  // skip[2]
pub fn pass3_re_register(env: *mut TypeEnv, program: Array<AstNode>, skip2: Array<Bool>) -> Array<Bool>
pub fn pass4_check(env: *mut TypeEnv, program: Array<AstNode>, skip2: Array<Bool>, skip3: Array<Bool>) -> TypedProgram

// ── Type Resolution ──
pub fn resolve_type(env: *mut TypeEnv, type_ast_node: Int) -> ResolvedType
pub fn types_compatible(expected: ResolvedType, actual: ResolvedType) -> Bool
pub fn unify(param_type: ResolvedType, arg_type: ResolvedType, bindings: *mut HashMap<String, ResolvedType>) -> Bool
pub fn substitute_type(ty: ResolvedType, bindings: HashMap<String, ResolvedType>) -> ResolvedType

// ── Effect Checking ──
pub fn can_call(caller_effects: Int, callee_effects: Int) -> Bool
    // Implements the 4-rule lattice:
    //   Rule 1: Pure callee → anyone can call
    //   Rule 2: Pure caller → can only call Pure
    //   Rule 3: Unsafe caller → can call anything
    //   Rule 4: callee ⊆ caller (subset check)
pub fn check_effect_call(caller: Int, callee: Int, caller_name: String, callee_name: String, span: Int) -> Bool

// effects.kn — effect definitions
pub fn effect_from_str(name: String) -> Int  // "Pure"→0, "IO"→1, ...

// monomorphize.kn — generic instantiation
pub fn monomorphize(env: *mut TypeEnv, typed: TypedProgram) -> MonomorphizedProgram
pub fn instantiate_generic(env: *mut TypeEnv, fn_node: Int, concrete_types: Array<ResolvedType>) -> Int
```

**Internal Data Structures:**
```kn
struct TypeEnv:
    types:           HashMap<String, ResolvedType>    // named types (struct, enum, trait)
    values:          HashMap<String, ResolvedType>    // variable bindings
    scopes:          Array<Scope>                     // scope stack for let bindings
    enum_variants:   HashMap<String, HashMap<String, ResolvedType>>  // enum_name → variant_name → payload_type
    trait_methods:   HashMap<String, HashMap<String, ResolvedType>>  // trait_name → method_name → type
    trait_origins:   HashMap<String, SymbolOrigin>    // trait → definition location
    errors:          Array<Diagnostic>                // accumulated errors (max 50)
    skip_2:          Array<Bool>                      // items that failed Pass 2
    skip_3:          Array<Bool>                      // items that failed Pass 3

struct TypedProgram:
    items:     Array<TypedItem>    // fully-typechecked items
    env:       TypeEnv             // final type environment
    errors:    Array<Diagnostic>   // accumulated errors

struct TypedItem:
    kind:      Int                 // AST item kind tag
    name:      String              // item name
    resolved_type: ResolvedType    // the type of this item
    ast_index: Int                 // back-reference to AstNode
    effects:   Int                 // effect bitmask
    // For structs: field_types: Array<(String, ResolvedType)>
    // For enums: variant_types: Array<(String, ResolvedType)>
    // For fns: params: Array<(String, ResolvedType)>, body_typed: TypedExpr

struct MonomorphizedProgram:
    items: Array<TypedItem>    // all generic functions instantiated with concrete types
    // Generic functions replaced by monomorphized copies (mangled names)
```

**4-Pass Pipeline Algorithm:**
```
typecheck(env, program):
    errors = []

    // PASS 1: Predeclare type names
    for each item in program:
        if item is Struct:   env.types[item.name] = Struct(item.name, {})
        if item is Enum:     env.types[item.name] = Enum(item.name, [])
        if item is World:    env.types[item.name] = Struct(item.name, {})   // stub
        if item is Actor:    env.types[item.name] = Struct(item.name, {})   // stub
        if item is Component: env.types[item.name] = Struct(item.name, {})  // stub
        if item is Trait:    env.trait_origins[item.name] = ...

    // PASS 2: Register field/variant/method types
    for each item in program:
        result = register_item_types(env, item)
        if result.is_err(): skip_2[i] = false; errors.extend(result.errors)

    // PASS 3: Re-register (single retry for forward references)
    for each item in program where skip_2[i]:
        result = register_item_types(env, item)
        if result.is_err(): skip_3[i] = false; errors.extend(result.errors)

    // PASS 4: Full expression typecheck
    for each item in program where skip_2[i] and skip_3[i]:
        typed = check_item(env, item)
        typed_items.push(typed)

    return TypedProgram { items: typed_items, env, errors }
```

**types_compatible() Decision Tree (Complete):**
```
types_compatible(expected, actual):
    // Escape valves
    (Unknown, _) or (_, Unknown) → true
    (Never, _) or (_, Never) → true
    (Generic(_), _) or (_, Generic(_)) → true

    // Primitives
    (Unit, Unit) → true
    (Bool, Bool) → true
    (String, String) → true
    (Char, Char) → true
    (Int(_), Int(_)) → true                    // any integer sizes cross-compatible
    (Float(_), Float(_)) → true                // any float sizes cross-compatible
    (Int(_), Float(_)) or (Float(_), Int(_)) → true  // numeric promotion

    // Collections
    (Array(e1, n1), Array(e2, n2)) → (n1==n2 or n1==0 or n2==0) and compatible(e1,e2)
    (Slice(e1), Slice(e2)) → compatible(e1,e2)
    (Slice(e1), Array(e2,_)) → compatible(e1,e2)

    // Tuples (structural)
    (Tuple(ts1), Tuple(ts2)) → len==len and all(compatible)

    // Stdlib
    (Option(a), Option(b)) → compatible(a,b)
    (Result(ok1,err1), Result(ok2,err2)) → compatible(ok1,ok2) and compatible(err1,err2)
    (Future(a), Future(b)) → compatible(a,b)

    // References (auto-deref for immutable refs)
    (Ref{mut:false, inner:i}, other) → compatible(i, other)
    (other, Ref{mut:false, inner:i}) → compatible(other, i)

    // Functions (structural)
    (Function{ps1, r1, _}, Function{ps2, r2, _}) → compatible(r1,r2) and all(compatible(ps1,ps2))

    // Named types (nominal)
    (Struct(n1,_), Struct(n2,_)) → n1 == n2
    (Enum(n1,_), Enum(n2,_)) → n1 == n2

    // Fallthrough
    _ → false
```

**Stub Strategy for Layers 1–7 (FR-TYPE.36–43):**
```
check_item for non-L0 items:
    world        → Treat as Struct(name, state_fields)
    actor        → Treat as Struct(name, state_fields); on handlers → fn signatures
    component    → Treat as Struct(name, prop_fields); skip JSX/render validation
    patch        → Typecheck body as fn; return type can be anything
    law          → Typecheck body as fn; enforce return type Bool
    converge     → Typecheck lanes as fn; skip selector/match/verify logic
    orchestrate  → Typecheck stage bodies as expressions; skip graph validation
    pulse/resonate → Typecheck body as block expr; skip duration/dampen beyond syntax
    axiom/shatter/teleport → Parse and store; skip all semantic validation
```

**Dependencies:** PARSE (AstNode and AST tag constants), shared types.kn type definitions

---

### Subsystem: CODEGEN — LLVM Codegen

**Files:** `src/codegen.kn` (~2000 lines), `src/llvm_ffi.kn` (~1000 lines, shared with RUNTIME)

**Responsibility:** Two-path compilation: Path A (textual `.ll` string emission, zero LLVM dependency) and Path B (LLVM-C API in-memory IR construction). Emit 200+ runtime function `declare` statements. Map all 20 Kain `ResolvedType` variants to LLVM IR types.

**Public Interface:**
```kn
// codegen.kn — LLVM codegen
pub fn codegen_textual(program: MonomorphizedProgram, target: String, debug: Bool) -> String
    // Path A: emits complete .ll text file as a String
    // Consumed by clang for compilation and linking

pub fn codegen_llvm_c(program: MonomorphizedProgram, ctx: ptr<Byte>, mod: ptr<Byte>) -> ptr<Byte>
    // Path B: builds LLVM IR in-memory via LLVM-C API
    // Returns the LLVM module handle

// ── Type Mapping ──
pub fn kain_type_to_llvm_ir(ty: ResolvedType) -> String
    // Path A: returns LLVM IR type string (e.g. "i64", "%Point", "{i8*, i64}")
pub fn kain_type_to_llvm_c_type(ctx: ptr<Byte>, ty: ResolvedType) -> ptr<Byte>
    // Path B: returns LLVMTypeRef

// ── Module Setup ──
pub fn emit_module_header(target_triple: String, data_layout: String) -> String
pub fn emit_runtime_declares() -> String
    // Emits 200+ declare statements. Deduplicated.
    // See Runtime Function Table below.

// ── Function Compilation ──
pub fn compile_function(fn: TypedItem, ctx: ptr<Byte>, mod: ptr<Byte>, builder: ptr<Byte>) -> ptr<Byte>
pub fn compile_expr(expr: TypedExpr, ctx: ptr<Byte>, builder: ptr<Byte>, locals: *mut HashMap<String, ptr<Byte>>) -> ptr<Byte>
pub fn compile_stmt(stmt: TypedStmt, ctx: ptr<Byte>, builder: ptr<Byte>, locals: *mut HashMap<String, ptr<Byte>>)
pub fn compile_if_expr(cond: TypedExpr, then: TypedExpr, else_: TypedExpr, ...) -> (ptr<Byte>, ptr<Byte>)
pub fn compile_match(scrutinee: TypedExpr, arms: Array<TypedMatchArm>, ...) -> ptr<Byte>
pub fn compile_call(callee: String, args: Array<TypedExpr>, ...) -> ptr<Byte>
pub fn compile_struct_literal(struct_name: String, fields: Array<(String, TypedExpr)>, ...) -> ptr<Byte>
pub fn compile_field_access(object: TypedExpr, field: String, ...) -> ptr<Byte>

// llvm_ffi.kn — LLVM-C FFI layer
// All functions annotated with Unsafe effect
// Uses include <llvm-c/Core.h> as llvm
pub fn llvm_context_create() -> ptr<Byte>
pub fn llvm_module_create(name: String, ctx: ptr<Byte>) -> ptr<Byte>
pub fn llvm_builder_create(ctx: ptr<Byte>) -> ptr<Byte>
pub fn llvm_build_add(builder: ptr<Byte>, lhs: ptr<Byte>, rhs: ptr<Byte>, name: String) -> ptr<Byte>
pub fn llvm_build_sub(builder: ptr<Byte>, lhs: ptr<Byte>, rhs: ptr<Byte>, name: String) -> ptr<Byte>
pub fn llvm_build_mul(builder: ptr<Byte>, lhs: ptr<Byte>, rhs: ptr<Byte>, name: String) -> ptr<Byte>
pub fn llvm_build_call(builder: ptr<Byte>, fn: ptr<Byte>, args: Array<ptr<Byte>>, name: String) -> ptr<Byte>
pub fn llvm_build_ret(builder: ptr<Byte>, val: ptr<Byte>)
pub fn llvm_build_br(builder: ptr<Byte>, dest: ptr<Byte>)
pub fn llvm_build_cond_br(builder: ptr<Byte>, cond: ptr<Byte>, then_bb: ptr<Byte>, else_bb: ptr<Byte>)
pub fn llvm_build_alloca(builder: ptr<Byte>, ty: ptr<Byte>, name: String) -> ptr<Byte>
pub fn llvm_build_store(builder: ptr<Byte>, val: ptr<Byte>, ptr: ptr<Byte>)
pub fn llvm_build_load(builder: ptr<Byte>, ty: ptr<Byte>, ptr: ptr<Byte>, name: String) -> ptr<Byte>
pub fn llvm_build_gep(builder: ptr<Byte>, ptr: ptr<Byte>, indices: Array<ptr<Byte>>, name: String) -> ptr<Byte>
pub fn llvm_build_icmp(builder: ptr<Byte>, pred: Int, lhs: ptr<Byte>, rhs: ptr<Byte>, name: String) -> ptr<Byte>
pub fn llvm_build_phi(builder: ptr<Byte>, ty: ptr<Byte>, name: String) -> ptr<Byte>
pub fn llvm_verify_module(mod: ptr<Byte>, action: Int) -> Bool
pub fn llvm_write_bitcode(mod: ptr<Byte>, path: String)
// ... (full LLVM-C API surface as documented in 03-llvm-codegen-jit.md §6)
```

**Kain Type → LLVM Type Mapping (Complete):**
```
Kain Type              LLVM IR String (Path A)     LLVM-C API (Path B)
Unit / void            "void"                      LLVMVoidTypeInContext(ctx)
Bool                   "i1" (SSA) / "i8" (mem)     LLVMInt1TypeInContext(ctx)
Int(I64)               "i64"                       LLVMInt64TypeInContext(ctx)
Int(I32)               "i32"                       LLVMInt32TypeInContext(ctx)
Float(F64)             "double"                    LLVMDoubleTypeInContext(ctx)
Float(F32)             "float"                     LLVMFloatTypeInContext(ctx)
String                 "{i8*, i64}"                LLVMStructType (2 fields)
Char                   "i32"                       LLVMInt32TypeInContext(ctx)
ptr<T>                 "ptr"                       LLVMPointerType(ctx, 0)
Array(T, N)            "[N x T_llvm]"             LLVMArrayType(T_llvm, N)
Slice(T)               "{ptr, i64}"               LLVMStructType (ptr + len)
Tuple(T1, T2, ...)     "{T1_llvm, T2_llvm, ...}"  LLVMStructType (anonymous)
Ref { mut, T }         "ptr"                       LLVMPointerType(ctx, 0)
Option(T)              "{i64, T_llvm}"             LLVMStructType (tag + payload)
Result(T, E)           "{i64, T_llvm, E_llvm}"    LLVMStructType (tag + ok + err)
Future(T)              ptr (opaque)                LLVMPointerType(ctx, 0)
Struct("Name", {})     "%Name = type { ... }"      LLVMStructCreateNamed + LLVMStructSetBody
Enum("Name", [])       "{i64, [N x i8]}"           LLVMStructType (tag + ABI payload)
Function(args, ret)    "fn_ptr_type"               LLVMFunctionType(ret, args)
Generic(_)             (substituted before codegen) (substituted before codegen)
Never                  "void" + noreturn            LLVMVoidTypeInContext(ctx)
Unknown                (error — must be resolved)   (error)
```

**Codegen Pipeline (Path A — Textual .ll):**
```
codegen_textual(program, target, debug):
    output = ""
    // 1. Module header
    output += "target triple = \"" + target_triple + "\"\n"
    output += "target datalayout = \"" + data_layout + "\"\n"

    // 2. Runtime function declarations (200+ declare statements)
    output += emit_runtime_declares()

    // 3. Comptime global constants (@.str.0, @.str.1, ...)
    output += emit_global_constants(program)

    // 4. Struct type definitions
    for each struct in program:
        output += emit_struct_type_definition(struct)

    // 5. Enum type definitions
    for each enum in program:
        output += emit_enum_type_definition(enum)

    // 6. Functions
    for each fn in program:
        output += compile_function_textual(fn)

    return output
```

**Runtime Function Table (Key Declarations):**

The `emit_runtime_declares()` function emits ~200 declarations. Key categories:
- Core: `print_i64`, `print_str`, `KAIN_alloc`, `string_new`, `str_concat`, `strlen`
- Stdlib ABI: `abi_option_none`, `abi_option_some`, `abi_result_ok`, `abi_result_err`, `abi_patch_begin`, `abi_resonate_exit`, `abi_entangle_record_i64`
- Actor: `kain_actor_spawn`, `kain_actor_send`, `kain_actor_reply_port_new`, `kain_actor_reply_port_wait`
- Memory: `__kain_alloc`, `__kain_realloc`, `__kain_mem_load`, `__kain_mem_store`, `__kain_ptr_offset`
- Atomics: `__kain_atomic_load_seqcst`, `__kain_atomic_store_seqcst`, `__kain_atomic_add_seqcst`, `__kain_atomic_compare_exchange_seqcst`
- Ownership: `__kain_ownership_begin_collapse`, `__kain_ownership_end_collapse`, `__kain_ownership_decay`
- Machine Stones: `kain_machine_pulse_start`, `kain_machine_teleport_ptr`, `kain_machine_shatter_alloc`
- GPU/Opt: `abi_gpu_dispatch`, `abi_converge_select_lane_for_key`, `abi_orchestrate_stage_begin`
- Init/Shutdown: `abi_runtime_init`, `abi_runtime_shutdown`, `__kain_crash_handler_init`
- LLVM Intrinsics: `llvm.floor.f64`, `llvm.fptosi.sat.i64.f64`

**Untagging for @extern Calls:**
```
compile_extern_call(fn, args):
    for each arg:
        if arg.type is Int(_) and fn is @extern:
            // Strip tagged integer representation: (v << 3) | 1 → raw int64_t
            emit "  %untagged = ashr i64 %tagged, 3\n"
            pass untagged as C ABI argument
        elif arg.type is String and fn expects const char*:
            emit "  %ptr = extractvalue {i8*, i64} %str, 0\n"
            pass ptr as C ABI argument

    result = emit call instruction

    if fn.return is @c_string_return:
        emit materialize result into owned Kain String {i8*, i64}
```

**Dependencies:** TYPE (TypedProgram, ResolvedType), shared codegen type definitions

---

### Subsystem: JIT — Dual JIT Execution

**Files:** `src/jit.kn` (~300), `src/jit_metal.kn` (~200), `src/jit_x86.kn` (~500), `src/jit_orc.kn` (~400), `src/jit_cache.kn` (~200)

**Responsibility:** Path A: markscript-style x86-64 direct emission (instant startup, zero LLVM dependency). Path B: OrcJIT via `include <llvm-c/Orc.h>` (full optimization). Shared W^X memory lifecycle and asm trampoline. JIT code cache.

**Public Interface:**
```kn
// jit.kn — JIT dispatcher
pub fn jit_execute(ast: Array<AstNode>, entry: String) -> Int with Unsafe
    // Main entry. Selects Path A or B based on capability.

// jit_metal.kn — W^X lifecycle + trampoline
pub fn jit_compile_and_run(code_bytes: Array<Int>, code_size: Int) -> Int with Unsafe
    // W^X: vm_map(RW) → mem_store → vm_protect(RX) → cache_flush → full_fence → asm call
pub fn call_jit_trampoline(code_ptr: ptr<Byte>) -> Int with Unsafe
    // Shared asm trampoline: scratch[0]=code_ptr → asm("mov rax,[rdi];call rax;mov [rdi+8],rax") → result

// jit_x86.kn — Path A: x86-64 direct emission
pub fn jit_emit_x86_block(bytecode: Array<Int>) -> (Array<Int>, Array<FixupEntry>)
    // Emit native x86-64 machine code for bytecode block
pub fn jit_apply_fixups(code: Array<Int>, fixups: Array<FixupEntry>, offsets: Array<Int>) -> Array<Int>

// jit_orc.kn — Path B: OrcJIT compilation
pub fn jit_orc_init() -> ptr<Byte>  // returns LLVMOrcLLJITRef
pub fn jit_orc_compile(jit: ptr<Byte>, ctx: ptr<Byte>, mod: ptr<Byte>) -> Bool
pub fn jit_orc_lookup(jit: ptr<Byte>, symbol: String) -> ptr<Byte>

// jit_cache.kn — code cache
pub fn jit_cache_lookup(cache: *mut CacheStore, hash: Int) -> LookupResult
pub fn jit_cache_register(cache: *mut CacheStore, hash: Int, ptr: ptr<Byte>, size: Int)
pub fn jit_cache_promote(cache: *mut CacheStore, hash: Int, orcjit_ptr: ptr<Byte>)
```

**W^X Lifecycle (Exact Sequence, jit_metal.kn):**
```
jit_compile_and_run(code_bytes, code_size):
    // Step 1: Allocate RW pages
    page_size = vm_page_size()
    alloc_size = align_to_page(code_size, page_size)
    pages = vm_map(alloc_size)     // RW (reserve + commit)
    if ptr_to_int(pages) == 0: return -1

    // Step 2: Write JIT code into RW pages (collapse scope)
    collapse pages:
        for i in 0..code_size:
            bp = ptr_offset(pages, i, "Byte")
            mem_store(bp, code_bytes[i], "Byte")
        0

    // Step 3: Transition RW → RX (W^X enforcement)
    prot = vm_protect_execute_read(pages, alloc_size)
    if prot != 0: decay pages; return -2

    // Step 4: Flush instruction cache (clflush every cache line)
    cls = cpu_cache_line_bytes()
    for ci in (0..alloc_size) step cls:
        bp = ptr_offset(pages, ci, "Byte")
        fp = int_to_ptr(ptr_to_int(bp), "ptr<Int>")
        cache_flush(fp)

    // Step 5: Full memory fence (mfence)
    full_fence()

    // Step 6: Execute via asm trampoline
    result = call_jit_trampoline(pages)

    // Step 7: Release pages
    decay pages
    return result
```

**Path A: x86-64 Direct Emission (Key Patterns):**

Fixed register allocation: RAX (accumulator), RBX (right operand), RBP (frame pointer). Software operand stack at RBP-relative offsets (no native push/pop).

```
Prologue:  push rbp; push rbx; mov rbp, rsp
Epilogue:  mov rsp, rbp; pop rbx; pop rbp; ret

RBP-relative access:  [rbp + disp32]  (ModRM: mod=10, rm=101)
    Load:  0x48 0x8B ModRM disp32     (mov reg, [rbp+disp])
    Store: 0x48 0x89 ModRM disp32     (mov [rbp+disp], reg)

Jump fixup (two-pass):
    Pass 1: emit jmp 0xE9 + placeholder 4-byte displacement; record FixupEntry
    Pass 2: compute rel32 = target_native - (patch_at + 4); patch displacement bytes
```

**Path B: OrcJIT API Integration:**
```
jit_orc_compile_and_call(module, entry):
    // Initialize
    LLVMInitializeNativeTarget()
    LLVMInitializeNativeAsmPrinter()
    jit = LLVMOrcCreateLLJIT()

    // Verify + add module
    LLVMVerifyModule(module, LLVMReturnStatusAction, ...)
    tracker = LLVMOrcLLJITAddLLVMIRModule(jit, module)

    // Lookup entry symbol
    LLVMOrcLLJITLookup(jit, &addr, entry_name)
    code_ptr = int_to_ptr(addr, "ptr<Byte>")

    // Execute via shared trampoline
    result = call_jit_trampoline(code_ptr)

    // Cleanup
    LLVMOrcDisposeLLJIT(jit)
    return result
```

**Cache Store (shatter struct):**
```kn
shatter struct CacheStore:
    hashes:   Array<Int>        // SoA: function hashes (contiguous for L1 scan)
    ptrs:     Array<ptr<Byte>>  // SoA: JIT code pointers
    sizes:    Array<Int>        // SoA: compiled sizes
    count:    Int
    hits:     Int
    misses:   Int
    bytes:    Int
    compiles: Int
```

**Dependencies:** CODEGEN (LLVM-C API for Path B), `std::machine` (vm_*, fences, cache_flush), metal.kn primitives (proven in cases 0, 1, 4, 5, 10)

---

### Subsystem: CLI — CLI Driver

**Files:** `src/compiler.kn` (~200), `src/cli.kn` (~300), `src/main.kn` (~100)

**Responsibility:** Subcommand tree (check, build, run, test, selfhost, fmt, amalgamate, doctor, config, clean), DriverSession pipeline (Resolve→Lex→Parse→Comptime→Typecheck→Monomorphize→Codegen), workspace discovery, diagnostics formatting.

**Public Interface:**
```kn
// compiler.kn — DriverSession
pub fn driver_session_new() -> DriverSession
pub fn driver_session_compile(session: *mut DriverSession, source: String, source_path: String, target: String) -> CompileResult
    // Full pipeline: resolve → lex → parse → typecheck → monomorphize → codegen
pub fn driver_session_check(session: *mut DriverSession, source: String, source_path: String) -> CheckResult
    // Simplified pipeline: lex → parse → typecheck only (no codegen)

// cli.kn — CLI argument parsing
pub fn parse_args(args: Array<String>) -> CliConfig
pub fn run_subcommand(config: CliConfig) -> Int

// main.kn — entry point
pub fn main() -> Int with IO
```

**Internal Data Structures:**
```kn
struct DriverSession:
    source:         String      // aggregated source text
    tokens:         Array<Token>      // lexer output
    ast:            Array<AstNode>    // parser output
    typed:          TypedProgram      // typechecker output
    mono:           MonomorphizedProgram  // monomorphizer output
    diagnostics:    Array<Diagnostic>     // accumulated diagnostics
    config:         BuildConfig           // loaded from build.md
    progress_phase: Int           // current phase for progress events
    cache_frontend: Option<CachedFrontend>  // caching for incremental
    cache_checked:  Option<CachedChecked>

struct CliConfig:
    subcommand:     Int       // SUBCMD_CHECK, SUBCMD_BUILD, SUBCMD_RUN, etc.
    input_path:     String    // path to source file or directory
    target:         String    // "llvm", "c", "jit", etc.
    profile:        String    // "debug", "release"
    json_output:    Bool      // --json flag
    json_out_path:  String    // --json-out <path>
    verbose:        Bool      // -v flag
    debug_info:     Bool      // --debug flag
    verify_ouroboros: Bool    // --verify-ouroboros flag

// Subcommand constants
pub const SUBCMD_CHECK:    Int = 0
pub const SUBCMD_BUILD:    Int = 1
pub const SUBCMD_RUN:      Int = 2
pub const SUBCMD_TEST:     Int = 3
pub const SUBCMD_SELFHOST: Int = 4
pub const SUBCMD_FMT:      Int = 5
pub const SUBCMD_AMALGAMATE: Int = 6
pub const SUBCMD_DOCTOR:   Int = 7
pub const SUBCMD_CONFIG:   Int = 8
pub const SUBCMD_CLEAN:    Int = 9
pub const SUBCMD_HELP:     Int = 10
```

**DriverSession Pipeline Algorithm:**
```
driver_session_compile(session, source, source_path, target):
    // Phase 0: Resolve (workspace discovery, use imports, source aggregation)
    emit_progress("Resolve")
    resolved = resolve_imports(source, source_path)
    if resolved.errors > 0: return CompileResult { errors: resolved.errors }

    // Phase 1: Lex
    emit_progress("Lex")
    session.tokens = indent_process(lexer_tokenize_all(resolved.source, source_path))
    if any lexer errors: return early

    // Phase 2: Parse
    emit_progress("Parse")
    session.ast = parse(parser_new(session.tokens, source_path))
    if len(parser.errors) >= MAX_ERRORS: bail

    // Phase 3: Comptime (stub for initial implementation)
    emit_progress("Comptime")
    // Expand comptime blocks, resolve const values

    // Phase 4: Typecheck
    emit_progress("Typecheck")
    session.typed = typecheck(type_env_new(), session.ast)
    if len(session.typed.errors) > 0: return CompileResult { errors: session.typed.errors }

    // Phase 5: Monomorphize
    emit_progress("Monomorphize")
    session.mono = monomorphize(env, session.typed)

    // Phase 6: Codegen
    emit_progress("Codegen")
    if target == "llvm":
        llvm_text = codegen_textual(session.mono, target, session.config.debug_info)
        write_file(output_path + ".ll", llvm_text)
        // Invoke clang: clang -c output.ll → output.o → link with runtime → .exe
    elif target == "jit":
        result = jit_execute(session.ast, "main")
        return CompileResult { exit_code: result }

    return CompileResult { errors: [], output_path: output_path }
```

**Workspace Discovery Algorithm:**
```
discover_workspace(start_path):
    current = canonicalize(start_path)
    while current is not filesystem root:
        if exists(current / "KAIN.toml"): return current
        if exists(current / "kain.toml"): return current
        if exists(current / "build.kn"):   return current
        if exists(current / "platform.kn"): return current
        if exists(current / "Cargo.toml"):  return current
        if exists(current / ".git"):        return current
        current = parent(current)
    return None  // no workspace found
```

**Diagnostics Formatting:**
```
format_diagnostic(diag, source):
    // Classic format: filename:line:col: error: message
    line = source.lines[diag.line - 1]
    caret = " " * diag.column + "^"
    return "{diag.file_path}:{diag.line}:{diag.column}: {severity_str}: {diag.message}\n{line}\n{caret}"

format_diagnostic_json(diag):
    // JSON format for --json flag
    return {
        "severity": diag.severity,
        "file_path": diag.file_path,
        "line": diag.line,
        "column": diag.column,
        "message": diag.message,
        "error_kind": diag.error_kind,
        "span_start": diag.span_start,
        "span_end": diag.span_end
    }
```

**Dependencies:** All subsystems (LEX, PARSE, TYPE, CODEGEN, JIT, RUNTIME, ORCH)

---

### Subsystem: RUNTIME — Runtime Contract & FFI

**Files:** `src/runtime.kn` (~300 lines), `src/builtins.kn` (~200 lines)

**Responsibility:** @extern function declarations, 3-layer stdlib pattern, KainType↔CType mapping, runtime function table registry, C header import pipeline support.

**Public Interface:**
```kn
// runtime.kn — runtime contract management
pub fn runtime_table_init() -> RuntimeTable
pub fn runtime_table_lookup(table: *mut RuntimeTable, symbol: String) -> RuntimeFunction
pub fn runtime_table_emit_declares(table: RuntimeTable) -> String
    // Generates 200+ declare statements for LLVM IR output

// builtins.kn — builtin type and function registration
pub fn register_builtin_types(env: *mut TypeEnv)
    // Pre-register I8, I16, I32, I64, I128, Isize, U8, U16, U32, U64, U128, Usize
pub fn register_builtin_functions(env: *mut TypeEnv)
    // Register builtin functions: alloc, alloc_zeroed, realloc_mem, mem_load, mem_store,
    // ptr_offset, ptr_to_int, int_to_ptr, bitcast, lfence, sfence, mfence, clflush,
    // atomic_load, atomic_store, atomic_add, atomic_cmpxchg, asm,
    // sizeof, alignof, vm_page_size, vm_map, vm_protect_execute_read, etc.
```

**Internal Data Structures:**
```kn
struct RuntimeFunction:
    name:         String    // LLVM symbol name (e.g. "kain_machine_shatter_alloc")
    return_type:  String    // LLVM IR return type (e.g. "i8*")
    param_types:  Array<String>  // LLVM IR parameter types (e.g. ["i64", "i64"])
    is_vararg:    Bool      // variadic function
    calling_conv: String    // "ccc" (default), "win64cc", "x86_64_sysvcc"
    attributes:   Array<String>  // "noalias", "allocsize(0)", "naked", etc.

struct RuntimeTable:
    functions:    HashMap<String, RuntimeFunction>  // keyed by symbol name
    categories:   HashMap<String, Array<String>>    // category → list of symbols
```

**Three-Layer Stdlib Pattern (Every Public Function):**
```kn
// Layer 1: Raw ABI declaration
@extern fn abi_runtime_init() -> Int

// Layer 2: Interpreter-interceptable wrapper
pub fn native_runtime_init() -> Int:
    return abi_runtime_init()

// Layer 3: Public documented API
pub fn runtime_init() -> Int:
    return native_runtime_init()
```

**KainType → C Type Mapping (Complete):**
```
Kain Type      LLVM IR Type    C Type in Runtime     Notes
Int / i64      i64             int64_t               Primary integer
I32            i32             int32_t               32-bit signed
UInt / u64     i64             uint64_t              Both use i64
Float / f64    double          double                64-bit IEEE 754
F32            float           float                 32-bit IEEE 754
Bool           i1→i8→i64       int                   SSA/mem/struct variants
String         {i8*, i64}      KainString {char*;int64_t}
ptr<T>         i8* (opaque)    void*
Unit / void    void            void
Option<T>      {i64, T}        KainOption {tag; payload}
Result<T,E>    {i64, T, E}     KainResult {tag; ok; err}
struct Foo     %Foo type       struct Foo {...}
```

**C ABI Policy (Platform-Specific):**
```
                LP64 (Linux)    LLP64 (Windows)
int             32-bit          32-bit
long            64-bit          32-bit
long long       64-bit          64-bit
void*           64-bit          64-bit
size_t          64-bit          64-bit
wchar_t         32-bit          16-bit
```

**Dependencies:** CODEGEN (shared llvm_ffi.kn), shared type definitions

---

### Subsystem: ORCH — MarkScript Orchestration

**Files:** `src/orchestrator.kn` (~500 lines)

**Responsibility:** Embed markscript VM, register 9 compiler-specific IVT handlers (IDs 200-208), load build config from markscript tables, execute build pipeline via markscript intents.

**Public Interface:**
```kn
// orchestrator.kn — MarkScript fusion integration
pub fn orchestrator_init() -> OrchestratorState
    // Create VM, register handlers, load build config
pub fn orchestrator_build(state: *mut OrchestratorState, stage: String) -> Int
    // Execute build pipeline stage (or "BuildAll" by default)
pub fn orchestrator_check(state: *mut OrchestratorState, path: String) -> Int
pub fn orchestrator_test(state: *mut OrchestratorState, spec_path: String) -> Int
pub fn orchestrator_selfhost(state: *mut OrchestratorState, phase: Int) -> Int

// ── IVT Handler Functions (registered into markscript VM) ──
pub fn handler_compile_check(file_path: String) -> Int        // ID 200
pub fn handler_compile_codegen(file_path: String, target: String, profile: String) -> Int  // ID 201
pub fn handler_compile_jit(file_path: String) -> Int          // ID 202
pub fn handler_test_run(spec_path: String) -> Int             // ID 203
pub fn handler_test_report(format: String) -> String          // ID 204
pub fn handler_build_link(target: String) -> Int              // ID 205
pub fn handler_build_package(package_name: String) -> Int     // ID 206
pub fn handler_selfhost_phase1(crate_name: String) -> Int     // ID 207
pub fn handler_selfhost_phase2(crate_name: String) -> Int     // ID 208
```

**IVT Handler ID Registry:**
```
HANDLER_COMPILE_CHECK    = 200   → compile check   → lex + parse + typecheck
HANDLER_COMPILE_CODEGEN  = 201   → compile codegen  → full compilation (lex→parse→typecheck→codegen)
HANDLER_COMPILE_JIT      = 202   → compile jit      → JIT in-memory execution
HANDLER_TEST_RUN         = 203   → test run          → execute test specification
HANDLER_TEST_REPORT      = 204   → test report       → generate formatted report
HANDLER_BUILD_LINK       = 205   → build link        → link objects into binary
HANDLER_BUILD_PACKAGE    = 206   → build package     → full build pipeline end-to-end
HANDLER_SELFHOST_PHASE1  = 207   → selfhost phase1   → Rust DLL bridge
HANDLER_SELFHOST_PHASE2  = 208   → selfhost phase2   → pure Kain self-compilation
```

**Markscript Embedding API Contract:**

The compiler uses ONLY these 20 public functions from `std::markscript`. No direct internal function calls (Constraint C-10):
```kn
use std::markscript
// VM lifecycle: mks_new_vm, mks_run_file, mks_run_string, mks_run_with_vm, mks_register
// Table access: mks_tables, mks_table, mks_table_get_string, mks_table_get_int, mks_table_get_float,
//               mks_find_table, mks_table_rows, mks_table_cols
// Variables:      mks_get_var, mks_to_int, mks_to_string, mks_to_float
// Widgets:        mks_find_widget, mks_create_widget, mks_widget_set, mks_widget_get
```

**Build Config Loading:**
```
load_build_config(config_path):
    config_vm = markscript.mks_run_file(config_path)
    handle = markscript.mks_find_table(config_vm, "Metadata")
    if handle < 0: return defaults

    return BuildConfig {
        name:         mks_table_get_string(config_vm, handle, 0, 1, "kainc"),
        target:       mks_table_get_string(config_vm, handle, 1, 1, "llvm"),
        profile:      mks_table_get_string(config_vm, handle, 2, 1, "debug"),
        optimize:     mks_table_get_int(config_vm, handle, 3, 1, 0) == 1,
        entry:        mks_table_get_string(config_vm, handle, 5, 1, "src/main.kn"),
        source_root:  mks_table_get_string(config_vm, handle, 6, 1, "src/"),
        output:       mks_table_get_string(config_vm, handle, 8, 1, "kainc"),
        linker:       mks_table_get_string(config_vm, handle, 10, 1, "clang"),
    }
```

**Dependencies:** MarkScript VM (`std::markscript`), CLI, All subsystems (via IVT handler dispatch)

---

## Data Flow Diagrams

### Flow 1: Compile Check (`kainc check src/`)

```
CLI: kainc check src/
  │
  ├─► [CLI] parse_args() → CliConfig { subcommand: SUBCMD_CHECK, input_path: "src/" }
  │
  ├─► [ORCH] orchestrator_init()
  │     ├─ mks_new_vm()
  │     ├─ mks_register("compile check", 200)
  │     └─ load_build_config("build.md")
  │
  ├─► [ORCH] orchestrator_check(state, "src/")
  │     │
  │     ├─► [CLI] DriverSession
  │     │     ├─ Resolve: workspace("src/") → Array<source_files>
  │     │     │
  │     │     for each source file:
  │     │       ├─► [LEX] lexer_tokenize_all(source) → Array<Token>
  │     │       │     └─ indent_process(raw_tokens) → Array<Token> (with INDENT/DEDENT/EOF)
  │     │       │
  │     │       ├─► [PARSE] parse(tokens) → Array<AstNode>
  │     │       │     ├─ parse_item() dispatch for each top-level item
  │     │       │     └─ Pratt parse_binary() for expressions
  │     │       │
  │     │       └─► [TYPE] typecheck(ast) → TypedProgram
  │     │             ├─ Pass 1: predeclare type names
  │     │             ├─ Pass 2: register field/method types
  │     │             ├─ Pass 3: re-register (forward refs)
  │     │             └─ Pass 4: check expressions
  │     │                   ├─ types_compatible() at every binding
  │     │                   └─ can_call() at every call site
  │     │
  │     └─ Output: error count + diagnostics
  │
  └─► [STDERR] formatted diagnostics (or JSON if --json)
  └─► [EXIT] code 0 (no errors) or 1 (errors)
```

### Flow 2: Compile Build (`kainc build src/ --target llvm`)

```
CLI: kainc build src/ --target llvm
  │
  ├─► [ORCH] orchestrator_build(state, "BuildAll")
  │     │  loads buildex.md → executes BuildAll routine
  │     │
  │     ├─► > compile check "src/"          → handler_compile_check()
  │     │     └─ Same as Flow 1 above, produces TypedProgram
  │     │
  │     ├─► > compile codegen "src/" --llvm  → handler_compile_codegen()
  │     │     │
  │     │     ├─► [TYPE] monomorphize(typed) → MonomorphizedProgram
  │     │     │     ├─ Find all generic function calls
  │     │     │     ├─ unify(param_type, arg_type) → bindings
  │     │     │     ├─ substitute_type() for each instantiation
  │     │     │     └─ Create monomorphized copies (mangled names)
  │     │     │
  │     │     └─► [CODEGEN] codegen_textual(mono, "llvm", debug)
  │     │           ├─ Emit target triple, data layout
  │     │           ├─ emit_runtime_declares() → 200+ declare statements
  │     │           ├─ Emit struct type definitions (%Token, %AstNode, ...)
  │     │           ├─ Emit global constants (@.str.0, ...)
  │     │           └─ For each function:
  │     │                 ├─ emit function signature
  │     │                 ├─ entry block: alloca locals, store params
  │     │                 ├─ compile_expr() recursive walk
  │     │                 │    ├─ Int literal → add i64 0, val
  │     │                 │    ├─ Binary op → add/sub/mul/icmp/...
  │     │                 │    ├─ If/else → cond br + phi node
  │     │                 │    ├─ Call → call @fn(args)
  │     │                 │    ├─ Struct lit → alloca + gep + stores
  │     │                 │    └─ Field access → gep + load
  │     │                 └─ ret <ty> <val>
  │     │
  │     └─► > build link exe                → handler_build_link()
  │           │
  │           ├─ At this point, out/*.ll files exist on disk
  │           ├─ Shells out via markscript process handlers:
  │           │     > spawn "clang -c -O2 out/*.ll"    → .o files
  │           │     > await 0
  │           │     > spawn "clang out/*.o -lkain_runtime -o kainc.exe"
  │           │     > await 0
  │           │     > assert 0
  │           └─ kainc.exe produced
  │
  └─► [EXIT] code 0 → kainc.exe ready
```

### Flow 3: JIT Execute (`kainc run src/main.kn`)

```
CLI: kainc run src/main.kn
  │
  ├─► [ORCH] > compile jit "src/main.kn"   → handler_compile_jit()
  │     │
  │     ├─► [LEX] → [PARSE] → [TYPE] → TypedProgram (same as Flow 1)
  │     │
  │     ├─► [JIT] jit_execute(ast, "main")
  │     │     │
  │     │     ├─ Capability check:
  │     │     │   if llvm_orc_available(): → Path B (OrcJIT)
  │     │     │   else if target=="x86_64": → Path A (markscript-style)
  │     │     │   else: → interpreter fallback
  │     │     │
  │     │     ├─ Path A (direct x86-64):
  │     │     │   ├─ jit_emit_x86_block(bytecode) → raw machine code bytes
  │     │     │   ├─ jit_apply_fixups() → resolved forward jumps
  │     │     │   └─ jit_compile_and_run(code_bytes, code_size)
  │     │     │         ├─ vm_map(RW)
  │     │     │         ├─ collapse: mem_store(code bytes)
  │     │     │         ├─ vm_protect(RX)          ← W^X
  │     │     │         ├─ cache_flush + full_fence
  │     │     │         ├─ call_jit_trampoline(code_ptr)
  │     │     │         └─ decay pages
  │     │     │
  │     │     └─ Path B (OrcJIT):
  │     │           ├─ codegen_llvm_c(program, ctx, module)
  │     │           ├─ jit_orc_init() → LLJIT
  │     │           ├─ LLVMOrcLLJITAddLLVMIRModule()
  │     │           ├─ LLVMOrcLLJITLookup("main") → fn ptr
  │     │           ├─ call_jit_trampoline(fn_ptr)
  │     │           └─ LLVMOrcDisposeLLJIT()
  │     │
  │     └─ Returns result (exit code of compiled program)
  │
  └─► [STDOUT] program output
  └─► [EXIT] program exit code
```

### Flow 4: Ouroboros Bootstrap (`kainc selfhost --verify-ouroboros`)

```
CLI: kainc selfhost --verify-ouroboros
  │
  ├─ 1. Assemble combined source
  │     ├─ Read source_order from build.md or src/KAIN.toml
  │     ├─ Concatenate all compiler .kn files → combined_source.kn
  │     └─ Write to src/.selfhost/bootstrap/combined/kain_core_bootstrap.kn
  │
  ├─ 2. Compile to LLVM IR (Path A — always works, no LLVM DLL needed)
  │     ├─ DriverSession::compile(combined_source, target="llvm")
  │     │     LEX → PARSE → TYPE → MONO → CODEGEN
  │     └─ Write out/*.ll files
  │
  ├─ 3. Build runtime + Link → stage-1 binary
  │     ├─ Compile kain_runtime.lib (if not prebuilt)
  │     ├─ clang -c out/*.ll → out/*.o
  │     ├─ clang out/*.o -lkain_runtime -o stage1_kainc.exe
  │     └─ stage1_kainc.exe ready
  │
  ├─ 4. Stage 2: stage1_kainc.exe compiles combined source
  │     ├─ spawn "./stage1_kainc.exe build combined_source.kn --target llvm"
  │     ├─ await → exit code
  │     └─ stage2 out/*.ll files written
  │
  ├─ 5. Verification: compare stage1 output vs stage2 output
  │     ├─ diff stage1_out.ll stage2_out.ll
  │     ├─ If byte-identical: "OUROBOROS VERIFIED"
  │     └─ If different: report byte offset of first divergence
  │
  └─► [EXIT] code 0 (verified) or 1 (mismatch)
```

---

## Error Handling Strategy

### Error Categories

| Category | Examples | Strategy |
|----------|----------|----------|
| **Lexer Errors** | Unterminated string (ERR-1), unexpected character (ERR-2) | Emit diagnostic; continue tokenizing; never panic |
| **Parser Errors** | Missing closing bracket (ERR-3), expected function name (ERR-4), reserved identifier (ERR-5) | Emit diagnostic; call `synchronize()` to skip to next item boundary; continue |
| **Type Errors** | Type mismatch (ERR-8), effect violation (ERR-9,10,11), duplicate name (ERR-6) | Accumulate in Pass 2/3/4; skip items that fail early passes |
| **Monomorphization Errors** | Conflicting generic binding (ERR-13), trait bound unsatisfied (ERR-12) | Emit diagnostic; skip instantiation for that call site |
| **Codegen Errors** | LLVM verification failure (ERR-18), type unresolved at codegen (EC-16) | Emit diagnostic; fail compilation |
| **JIT Errors** | vm_map failed (ERR-19), protect failed (ERR-20), OrcJIT init failed (ERR-21) | Return error code; fall back Path A→Path B→interpreter |
| **CLI/File Errors** | File not found (ERR-24), workspace not found (ERR-26), linker not found (ERR-23) | Emit diagnostic; exit with code 1 |
| **Ouroboros Error** | Stage-1 != stage-2 binary (ERR-28) | Report byte offset and differing bytes |

### Error Accumulation Policy

- **MAX_ERRORS = 50**: After 50 accumulated errors, bail out and report "too many errors"
- **Not fail-fast**: Errors are accumulated into an `Array<Diagnostic>`. Compilation continues after individual errors.
- **Skip propagation**: Items that fail Pass 2 are excluded from Pass 3 and Pass 4 to prevent cascading errors
- **All errors returned**: CLI displays all accumulated errors at end of pipeline

### Error Response Format

**Text format (default):**
```
src/lexer.kn:42:15: error: type mismatch: expected Int, found String
    let x: Int = "hello"
                 ^~~~~~~
```

**JSON format (--json flag):**
```json
{
    "severity": "error",
    "file_path": "src/lexer.kn",
    "line": 42,
    "column": 15,
    "message": "type mismatch: expected Int, found String",
    "error_kind": "E_TYPE_MISMATCH",
    "span_start": 891,
    "span_end": 898
}
```

---

## Testing Strategy

### Unit Tests

**Scope:** Each subsystem tested in isolation with mock inputs.

| Subsystem | Test File | Key Test Scenarios |
|-----------|-----------|-------------------|
| LEX | `spec/lexer_spec.md` | Every TokenKind produced; comment skipping; string escapes; indent processing (17 rules) |
| PARSE | `spec/parser_spec.md` | Every item kind; Pratt precedence (16 levels); JSX parsing; error recovery; `>>` injection |
| TYPE | `spec/typechecker_spec.md` | types_compatible() pairwise; 4-pass pipeline; effect checking (4 rules); generic unify/substitute |
| CODEGEN | `spec/codegen_spec.md` | Kain type → LLVM type mapping; function compilation; control flow (if/while/match); struct/GEP |
| JIT | `spec/jit_spec.md` | W^X lifecycle; trampoline; x86-64 emitting (20 opcodes); OrcJIT init/compile/lookup |
| ORCH | `spec/orchestrator_spec.md` | VM creation; IVT handler registration; config loading; pipeline execution |

**Tools:** Markscript test specification format (markdown tables with `| Case | Source | Expected |` columns). Each test case compiles the Source and asserts the Expected result.

### Integration Tests

**Scope:** Full pipeline from source to executable.

| Test | Description |
|------|-------------|
| `parse + typecheck` | Parse and typecheck the entire ~13K-line compiler source; zero type errors |
| `compile → LLVM IR` | Compile the full compiler source to LLVM IR; verify output against known-good IR |
| `compile → native` | Compile → clang → link → executable; verify exit code 0 |
| `JIT execute` | Compile a 100-line test program via JIT; verify correct output |
| `Path A vs Path B` | Compile same source via textual .ll and LLVM-C API; verify identical LLVM IR |

### Ouroboros Verification Test

```
kainc selfhost --verify-ouroboros
  ├─ Stage 1: kainc (Rust-bootstrap-built) compiles combined source → stage1_kainc.exe
  ├─ Stage 2: stage1_kainc.exe compiles combined source → stage2 binary output
  ├─ Compare: diff stage1_out.ll stage2_out.ll → must be byte-identical
  └─ Exit: 0 if verified, 1 if mismatch (report offset)
```

### Performance Tests

| NFR | Test | Target |
|-----|------|--------|
| NFR-P1 | `kainc check` on ~13K-line source | < 500ms |
| NFR-P2 | `kainc build --target llvm` on ~13K-line source | < 5s (excluding clang link) |
| NFR-P3 | Lexer throughput | > 1MB/s |
| NFR-P4 | Parser throughput | > 500K tokens/s |
| NFR-P5 | JIT Path A startup | < 1ms |
| NFR-P6 | JIT Path B init | < 200ms |

### Edge Case Coverage

| EC ID | Test Approach |
|-------|--------------|
| EC-1 (empty file) | Lexer test: empty source → single Eof token; parser: empty Program |
| EC-2 (int overflow) | Lexer test: `99999999999999999999` → diagnostic, truncated value |
| EC-4 (only comments) | Lexer test: source with only `// ...` and `# ...` → Eof only |
| EC-5 (mixed tabs/spaces) | Indent processor test: tab=4 spaces, additive indent |
| EC-6 (8→0 indent drop) | Indent processor test: emits 2 DEDENT tokens |
| EC-7 (no trailing newline) | Indent processor test: auto-emit DEDENT + Eof |
| EC-8 (>> injection) | Parser test: `Vec<Vec<Int>>` → tokens split into `> >` |
| EC-14 (mutual recursion) | Typechecker test: `struct A { b: B }` + `struct B { a: A }` → Pass 3 resolves |
| EC-19 (max errors) | Integration test: file with 60 errors → bail at 50 + "too many errors" |
| EC-21 (OrcJIT unavailable) | JIT test: no LLVM DLL → fallback to Path A on x86-64 |
| EC-24 (self-compilation) | Ouroboros test: stage-1 ≠ stage-2 → report byte offset |

---

## Technology Decisions Log

### Decision 1: Flat Array AST vs Recursive Enum AST

**Context:** The Rust bootstrap uses recursive enums with `Box<Expr>` for the AST. Kain has no `Box` — value semantics require a different approach.

**Options Considered:**
1. **Recursive enum with `Option<T>` field indices** — Requires heap allocation for recursive types. Not cache-friendly. Complex to serialize. Kain type system can express this but it adds complexity.
2. **Flat `Array<AstNode>` with integer indices** — No recursion. Cache-local. Directly serializable. Each node is a fixed-size struct with kind tag + variable-length int payload. Parent-child relationships are index-based.

**Decision:** Flat `Array<AstNode>` (Option 2).

**Rationale:** This eliminates the need for recursive types, enables O(1) random access to any AST node, maps cleanly to LLVM structs for codegen, and allows the entire AST to sit in contiguous memory. The Rust bootstrap's recursive AST was a pragmatic choice for Rust, not a semantic requirement. The flat array is a better fit for Kain's value-oriented design.

**Requirements Addressed:** FR-PARSE.1, FR-PARSE.2, C-5, C-6

### Decision 2: Textual .ll Emission (Path A) vs LLVM-C API (Path B) as Primary

**Context:** The Rust bootstrap uses textual .ll emission (21,289 lines of string formatting). The LLVM-C API provides in-memory IR construction. Which should be the primary path?

**Options Considered:**
1. **Textual .ll only** — Proven in Rust bootstrap. Zero LLVM library linkage. Trivially portable. Can examine output. Slower (string I/O). No JIT.
2. **LLVM-C API only** — Faster compilation. JIT-ready. Requires LLVM DLL at compile time. More complex FFI.
3. **Both paths (dual)** — Path A for AOT compilation (always works), Path B for JIT and optimized builds. Fallback when LLVM unavailable.

**Decision:** Both paths (Option 3). Path A is the default — always works, no LLVM DLL required (C-14). Path B is available when LLVM DLL is present for JIT execution (FR-JIT.14-17).

**Rationale:** The dual-path design provides resilience (Path A always works), speed (Path B for hot paths), and JIT capability. The ouroboros verification uses Path A because it's deterministic and doesn't depend on LLVM's evolving API.

**Requirements Addressed:** FR-CODEGEN.1-3, FR-JIT.1-22, C-14, NFR-P5-6

### Decision 3: MarkScript as Orchestration Layer vs Custom Build System

**Context:** The compiler needs build config, pipeline, test runner, and REPL. Options: build custom infrastructure (~3,000 lines) or embed markscript (~500 integration lines).

**Options Considered:**
1. **Custom build system** — Full control. But 3,000+ lines of solve-the-world code. Config parsing, pipeline DSL, test framework, watch mode, process orchestration — all from scratch.
2. **MarkScript embedding** — Markscript already has 78 built-in handlers for all these features. The compiler just registers 9 custom IVT handlers. 500 lines of integration code vs 3,000.

**Decision:** MarkScript embedding (Option 2).

**Rationale:** Markscript IS a build system, config system, test runner, and REPL — all proven with 114 tests and 7,500 lines of working code. The fusion contract (§3 of research doc 07) specifies exactly 20 public API functions the compiler uses. This eliminates ~3,000 lines of infrastructure code, keeping the total compiler under the 13,000-line target (NFR-S3).

**Requirements Addressed:** All FR-ORCH.*, NFR-S2, C-10

### Decision 4: Kain-Only Core vs Rust Bridge

**Context:** The compiler core (lexer, parser, typechecker, codegen) could be pure Kain from the start, or could route through a Rust DLL bridge initially.

**Options Considered:**
1. **Rust DLL bridge first** — Faster initial progress. Compiler calls Rust for typechecking and codegen. But creates a permanent dependency.
2. **Pure Kain from the start** — Harder bootstrap. But produces a truly self-hosted compiler. The Rust bootstrap already compiles Kain, so kainc can be compiled from the start.
3. **Gradual migration** — Start with bridge, migrate subsystems one at a time.

**Decision:** Pure Kain from the start (Option 2), with the Rust bootstrap as the initial compilation toolchain (C-11).

**Rationale:** The Kain language already has all necessary constructs (Layer 0: fn, struct, enum, trait, impl, ptr, collapse/observe/decay, Unsafe effect). The `include <llvm-c/Core.h> as llvm` FFI mechanism already works (605 functions from windows.h, 755 from vulkan.h). The Rust bootstrap can compile kainc from day 1. Adding a DLL bridge adds complexity without changing the fundamental challenge.

**Requirements Addressed:** C-1, C-2, C-11

### Decision 5: Bootstrapping Strategy — Combined Source vs Modular Compilation

**Context:** The self-host compiler must compile itself. Options: compile individual .kn files separately and link, or concatenate all sources into a single combined file.

**Options Considered:**
1. **Modular compilation** — Each .kn file compiled to .ll separately, linked at the LLVM level. More complex module resolution. Requires managing inter-module symbol visibility.
2. **Combined source compilation** — All .kn files concatenated into one combined source (respecting source_order from KAIN.toml). Single compilation unit. Simpler. Proven by the Rust bootstrap's `selfhost_bootstrap.rs`.

**Decision:** Combined source compilation (Option 2).

**Rationale:** The Rust bootstrap already assembles combined sources for self-host testing (`assemble_combined_source()` in `selfhost_bootstrap.rs`). This approach eliminates inter-module resolution complexity, ensures consistent symbol ordering, and produces deterministic LLVM IR — critical for byte-identical ouroboros verification (NFR-C1).

**Requirements Addressed:** FR-CLI.7-8, NFR-C1

---

## Requirements Traceability Matrix

### FR-LEX (Lexer) → Design Elements

| FR | Design Element |
|----|---------------|
| FR-LEX.1 | `Token` struct (token.kn) — kind, text, line_no, col_no, byte_offset |
| FR-LEX.2-3 | `TokenKind` enum — 58 hard keywords → dedicated variants; 51 contextual keywords → `TokenKind::Ident` |
| FR-LEX.4 | Lexer DFA: `skip_line_comment()`, `skip_hash_comment()` in lexer_next_token() |
| FR-LEX.5-6 | `lex_number()` in lexer DFA — hex/octal/binary/decimal/float parsing |
| FR-LEX.7-9 | `lex_string()`, `lex_fstring()`, `lex_char()` — escape sequence resolution |
| FR-LEX.10-12 | Operator/punctuation longest-match dispatch in lexer DFA |
| FR-LEX.13 | `lex_newline()` — captures whitespace string after `\n` |
| FR-LEX.14 | `TokenKind::Error` — unrecognized character diagnostic |
| FR-LEX.15 | `TokenKind` enum: exactly 102 variants (audited in §Shared Types) |
| FR-LEX.16 | `indent_process()` — post-lexer pass inserting INDENT/DEDENT/EOF |
| FR-LEX.17 | Bracket depth tracking (paren_depth, bracket_depth, brace_depth) suppresses inside groups |
| FR-LEX.18-22 | Indent processor algorithm: blank line suppression, tab=4 spaces, multi-DEDENT pop, EOF auto-close |

### FR-PARSE (Parser/AST) → Design Elements

| FR | Design Element |
|----|---------------|
| FR-PARSE.1 | Flat `Array<AstNode>` representation (ast.kn) |
| FR-PARSE.2 | `AstNode` struct: kind, span_start, span_end, data[] |
| FR-PARSE.3 | 38 Item kinds, 12 Stmt kinds, 64 Expr kinds, 21 BinaryOp, 6 UnaryOp (ast.kn constants) |
| FR-PARSE.4-6 | `parse()` top-level dispatch, `parse_item()`, implicit main() wrapping |
| FR-PARSE.7-25 | Item dispatch table and specialized parse_*() functions for all 38 item kinds |
| FR-PARSE.26-36 | Statement dispatch: parse_let(), parse_var(), parse_return(), parse_defer(), parse_for(), parse_while(), parse_loop(), parse_break(), parse_continue(), etc. |
| FR-PARSE.37-43 | Pratt expression parser: `parse_binary(min_prec)` with 16-level precedence table and left/right associativity |
| FR-PARSE.44-50 | `parse_unary()` dispatch: unary operators, await, spawn, send, emit, ownership expressions, teleport |
| FR-PARSE.51-57 | `parse_postfix()` dispatch: call, field, index, ++, --, ?, ?., as |
| FR-PARSE.58-62 | `parse_assignment()`, compound assignment desugaring, ternary/coalesce/range desugaring |
| FR-PARSE.63-66 | `parse_generics()`, `parse_where_clause()`, `parse_effects()`, >> injection |
| FR-PARSE.67-70 | `parse_jsx_element()`, braced expression interpolation, tag matching |
| FR-PARSE.71-73 | `synchronize()` error recovery, MAX_ERRORS=50 bail, RESERVED_KEYWORDS list |
| FR-PARSE.74 | ~174 reserved identifiers in `RESERVED_KEYWORDS` array |

### FR-TYPE (Typechecker) → Design Elements

| FR | Design Element |
|----|---------------|
| FR-TYPE.1-2 | `ResolvedType` struct with 20 kind constants; `types_compatible()` complete decision tree |
| FR-TYPE.3-5 | TypeEnv with pre-registered primitives; Int→I64, Float→F64 literals |
| FR-TYPE.6-12 | 4-pass pipeline: `pass1_predeclare()`, `pass2_register()`, `pass3_re_register()`, `pass4_check()` |
| FR-TYPE.13-23 | Expression inference rules in `check_expr()` for all Expr kinds |
| FR-TYPE.24-30 | `can_call()` 4-rule lattice (Pure bottom, Unsafe top); `check_effect_call()` at every call site |
| FR-TYPE.31-35 | `unify()`, `substitute_type()`, `instantiate_generic()` in monomorphize.kn |
| FR-TYPE.36-43 | Stub strategy: world/actor/component→Struct; patch/law→fn; converge→fn lanes; orchestrate/pulse/resonate→expression body; axiom/shatter/teleport→store |

### FR-CODEGEN (LLVM Codegen) → Design Elements

| FR | Design Element |
|----|---------------|
| FR-CODEGEN.1-3 | `codegen_textual()` (Path A) and `codegen_llvm_c()` (Path B) |
| FR-CODEGEN.4-14 | Type mapping table: KainType → LLVM IR type → LLVMTypeRef for all 20 variants |
| FR-CODEGEN.15-18 | Module structure: target triple, data layout, struct types, global constants, runtime declares |
| FR-CODEGEN.19-33 | `compile_function()`, entry block, let/return, binary/compare, if/else (phi), loop stack, match dispatch, call, struct/GEP, asm |
| FR-CODEGEN.34-35 | `emit_runtime_declares()` — 200+ declare statements, deduplicated |
| FR-CODEGEN.36-39 | C ABI policy (LP64/LLP64), target triple (x86_64-pc-windows-msvc, x86_64-unknown-linux-gnu) |
| FR-CODEGEN.40-43 | Untagging (ashr i64 %tagged, 3) for @extern; String↔C string marshaling |
| FR-CODEGEN.44-46 | JSON sidecar emission (runtime contract, realtime app, shader artifact bundles) |

### FR-JIT (Dual JIT) → Design Elements

| FR | Design Element |
|----|---------------|
| FR-JIT.1-5 | Path A: `jit_emit_x86_block()`, prologue/epilogue, RBP-relative stack, fixed registers, two-pass fixup |
| FR-JIT.6-10 | W^X lifecycle in `jit_compile_and_run()`: vm_map(RW)→mem_store→vm_protect(RX)→cache_flush→full_fence |
| FR-JIT.11-13 | `call_jit_trampoline()`: shared asm trampoline with scratch buffer |
| FR-JIT.14-17 | Path B: `jit_orc_init()`, `jit_orc_compile()`, `jit_orc_lookup()`; fallback to Path A |
| FR-JIT.18-20 | `CacheStore` shatter struct with SoA layout, linear hash scan, telemetry |
| FR-JIT.21-22 | W^X state machine: UNMAPPED→RW→RX→UNMAPPED; metal.kn case verification |

### FR-CLI (CLI Driver) → Design Elements

| FR | Design Element |
|----|---------------|
| FR-CLI.1-13 | `CliConfig` struct, `parse_args()`, 10 subcommand handlers (check, build, run, test, selfhost, fmt, amalgamate, doctor, config, clean) |
| FR-CLI.14-17 | `DriverSession` pipeline: Resolve→Lex→Parse→Comptime→Typecheck→Monomorphize→Codegen; caching |
| FR-CLI.18-20 | `discover_workspace()` ascending anchor search; blade pattern expansion |
| FR-CLI.21-23 | `format_diagnostic()` (classic format) and `format_diagnostic_json()` (JSON format); MAX_ERRORS bail |

### FR-RUNTIME (Runtime Contract & FFI) → Design Elements

| FR | Design Element |
|----|---------------|
| FR-RUNTIME.1-4 | `include <llvm-c/Core.h> as llvm`; all LLVM-C types as `ptr<Byte>`; Unsafe effect on all calls |
| FR-RUNTIME.5-9 | @extern, @link_name, @callconv, @naked, @c_string_return attributes |
| FR-RUNTIME.10-11 | 3-layer stdlib pattern: @extern→native_→public |
| FR-RUNTIME.12-13 | `RuntimeTable` with 200+ functions; correct LLVM type signatures |
| FR-RUNTIME.14 | KainType→LLVM IR→C Type mapping table |
| FR-RUNTIME.15-17 | libclang 3-tier extraction pipeline (libclang→lang-c→regex) |

### FR-ORCH (MarkScript Orchestration) → Design Elements

| FR | Design Element |
|----|---------------|
| FR-ORCH.1-2 | `orchestrator_init()`: mks_new_vm(), mks_register() × 9 |
| FR-ORCH.3-11 | 9 IVT handler functions: handler_compile_check(200), handler_compile_codegen(201), ..., handler_selfhost_phase2(208) |
| FR-ORCH.12-14 | `load_build_config()` via mks_table_get_string/int; @schema validation |
| FR-ORCH.15-17 | `orchestrator_build()` loads buildex.md routines; GAMMA handlers for process orchestration |
| FR-ORCH.18-19 | CLI subcommands route through orchestrator; --stage flag for specific pipeline routines |

### NFR Traceability

| NFR | Design Strategy |
|-----|---------------|
| NFR-P1 (500ms check) | Flat array AST, no heap allocation, 4-pass pipeline with skip vectors |
| NFR-P2 (5s build) | Path A string emission, no LLVM library overhead |
| NFR-P3 (1MB/s lex) | Hand-written DFA, single scan, no regex |
| NFR-P4 (500K tok/s parse) | Flat AST, O(1) child access, no recursion |
| NFR-P5 (<1ms JIT A) | Direct x86-64 emission, no LLVM init |
| NFR-P6 (<200ms JIT B) | OrcJIT lazy init, shared trampoline |
| NFR-C1 (ouroboros) | Deterministic combined source compilation; byte-identical .ll output |
| NFR-C2 (functional parity) | Same type mapping, same runtime ABI, same declare statements |
| NFR-C3 (111 keywords) | Full keyword support in lexer + parser; verified against keyword_crucible.kn |
| NFR-S1-3 (code size) | ~12,500L core + ~500L orchestration = ~13,000L total |
| NFR-M1 (512MB RAM) | Flat arrays, no recursive types, minimal heap |
| NFR-M2 (10× JIT mem) | Page-aligned allocation, shatter struct cache |
| NFR-O1-2 (observability) | DriverSession progress events; markscript pipeline events |
| NFR-SEC1 (W^X) | vm_protect_execute_read (RX), never RWX simultaneously |
| NFR-SEC2 (Unsafe gating) | All LLVM-C FFI in Unsafe-annotated functions |

---

## Risks & Mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|-----------|--------|------------|
| LLVM-C API ABI incompatibility with newer LLVM versions | Medium | High | Path A (textual .ll) always works — C-14 guarantees this. Path B is optional. |
| Flat array AST encoding bugs (wrong data[] indices) | High | Medium | Comprehensive spec-based parser tests (all 38 item kinds, 64 expr kinds). Markscript test tables provide exhaustive coverage. |
| Ouroboros non-determinism (slightly different LLVM IR) | Medium | Critical | Combined source compilation ensures consistent symbol ordering. Deterministic virtual register allocation (monotonically incrementing counter). No hash-based ordering. |
| MarkScript VM API changes break compiler integration | Low | Medium | Stable 20-function public API contract. Compiler uses only documented public functions (C-10). Markscript is already at 1.0 with 114 tests. |
| x86-64 direct emission Path A limited to x86-64 | High | Low | Path A is a fallback for JIT only. AOT compilation uses Path A (.ll text) which is platform-independent. Path B (OrcJIT) supports all LLVM targets. |
| ~13,000 line target too optimistic for full compiler | Medium | Medium | Rust bootstrap is 519K lines but covers 17 targets, GPU, UE5, Python, LSP. Self-host is LLVM-only with stub strategy for L1-7. The core pipeline is ~10× smaller by design. |
| Tagged integer internal representation bugs in FFI boundary | Medium | High | Untagging (ashr by 3) and retagging tested in integration tests. Ouroboros verification proves FFI correctness end-to-end. |

---

## Open Questions

1. **String interning table size:** How large should the pre-allocated string table be for the ~13K-line compiler source? Recommend starting with 4096 entries with exponential growth.
2. **`use` resolution during workspace discovery:** Should `use std::*` imports be resolved at parse time (fused into source) or left as unresolved references for the typechecker? Recommend typechecker-time resolution for incremental compilation.
3. **DWARF debug info priority:** The out-of-scope list defers DWARF. Should basic line-number debug info be included for ouroboros debugging? Recommend adding after ouroboros passes — minimal implementation for `!DILocation` only.
4. **Caching granularity for DriverSession:** Per-file or per-function? Recommend per-file initially (simpler), upgrade to per-function when performance data demands it.
5. **LLVM optimization pass selection:** For Path B (OrcJIT), which passes are essential for acceptable compile times? Recommend O0 for JIT, O2 for AOT — standard LLVM pass pipeline.

---

## Validation Checklist

Before handing off to Task Agent (Phase 3):

- [x] Every FR-* traced to at least one component (see Traceability Matrix above)
- [x] Every NFR-* has a design strategy (see NFR Traceability above)
- [x] All components have specified interfaces (see Component Specifications above)
- [x] All data models have complete schemas (see Shared Type Definitions above)
- [x] Error handling covers all ERR-* (see Error Handling Strategy above)
- [x] Technology decisions documented with rationale (see Technology Decisions Log above)
- [x] Testing strategy covers all EC-* (see Edge Case Coverage above)
- [x] 6 shared types defined as parallel stream contract
- [x] 7 subsystem interfaces specified with public function signatures
- [x] 4 data flow diagrams for key scenarios
- [x] Parallel implementation plan with wave ordering

---

## Parallel Implementation Plan

### Wave 0: Shared Type Definitions (Must Complete First)

**Duration:** 1-2 days
**Deliverables:** `token.kn`, `ast.kn`, `types.kn` (type definitions only, not implementations)

These 6 shared types are the contract that enables all parallel work. They must be finalized before any subsystem implementation begins.

### Wave 1: Parallel Independent Subsystems

These 5 streams can be built simultaneously — they share only the type definitions from Wave 0:

| Stream | Files | Dependencies | Estimated Lines |
|--------|-------|-------------|-----------------|
| **LEX** | `lexer.kn`, `span.kn` | token.kn (types) | ~600 |
| **PARSE** | `parser.kn` | token.kn, ast.kn (types) | ~3000 |
| **JIT** | `jit_metal.kn`, `jit_x86.kn` | `std::machine` | ~700 |
| **ORCH** | `orchestrator.kn` | `std::markscript`, BuildConfig | ~500 |
| **RUNTIME** | `runtime.kn`, `builtins.kn` | token.kn, types.kn | ~500 |

### Wave 2: Typechecker (Depends on Parser)

| Stream | Files | Dependencies | Estimated Lines |
|--------|-------|-------------|-----------------|
| **TYPE** | `types.kn`, `effects.kn`, `monomorphize.kn` | ast.kn, token.kn | ~2100 |

### Wave 3: Codegen + JIT Integration (Depends on Typechecker)

| Stream | Files | Dependencies | Estimated Lines |
|--------|-------|-------------|-----------------|
| **CODEGEN** | `codegen.kn`, `llvm_ffi.kn` | types.kn, runtime.kn | ~3000 |
| **JIT (Path B)** | `jit.kn`, `jit_orc.kn`, `jit_cache.kn` | codegen.kn, jit_metal.kn | ~900 |

### Wave 4: CLI + Integration (Depends on All Subsystems)

| Stream | Files | Dependencies | Estimated Lines |
|--------|-------|-------------|-----------------|
| **CLI** | `compiler.kn`, `cli.kn`, `main.kn` | All subsystems | ~600 |

### Dependency Graph (Visual)

```
Wave 0:  token.kn ── ast.kn ── types.kn (shared definitions)
              │\        │\        │\
              │ \       │ \       │ \
Wave 1:     LEX  ORCH  PARSE    JIT  RUNTIME  (parallel, 5 streams)
              │         │         │      │
              │         │         │      │
Wave 2:       │         │       TYPE     │    (depends on PARSE)
              │         │         │\     │
              │         │         │ \    │
Wave 3:       │         │     CODEGEN────┤  (depends on TYPE + RUNTIME)
              │         │         │   \  │
              │         │         │  JIT_B │ (depends on CODEGEN + JIT_metal)
              │         │         │     \ │
Wave 4:       └─────────┴─────── CLI ─────┘  (integrates all)
                                  │
                              ORCH ←→ CLI    (orchestration bridges commands)
```
