# Stream ALPHA: Foundation Types + Lexer

**Stream ID:** ALPHA
**Role:** Define shared type definitions (TokenKind enum, Token struct, Diagnostic struct, Span helpers, AST tag constants) and implement the complete Kain lexer with indent processor
**Effort:** ~4 hours
**Depends On:** none (self-contained)
**Requirements Covered:** FR-LEX.1–22, FR-PARSE.3 (AST tag constants), shared type contracts
**Design Reference:** Research 01 §§2.1–2.6, Design §§TokenKind, Token, Diagnostic, Lexer

---

## Context

ALPHA produces the **foundation types** that every other stream imports. You write the 102-variant `TokenKind` enum, the `Token` struct, the `Diagnostic` struct for error reporting, `Span` helpers for source location, AST tag constants (`AST_ITEM_FUNCTION = 0`, etc.), and the complete hand-written DFA lexer with indent processor. The lexer converts UTF-8 source strings into `Array<Token>`, then the indent processor inserts synthetic `Indent`/`Dedent`/`Newline`/`Eof` tokens.

**Critical:** ALPHA tasks 1-3 (token.kn, error.kn, span.kn + AST constants) are the gating deliverables for Wave 2 (DELTA). Finish these FIRST before implementing the lexer (ALPHA-04 through ALPHA-06). The AST constants in ast.kn are integer tags consumed by DELTA's parser, FOXTROT's typechecker, and GOLF's codegen.

---

## Files You Own

### Files to Create

| File | Purpose | After This Stream |
|------|---------|-------------------|
| `X:\blades\kain\src\token.kn` | `TokenKind` enum (102+ variants), `Token` struct | DELTA reads (DO NOT MODIFY after ALPHA) |
| `X:\blades\kain\src\error.kn` | `Diagnostic` struct, error kind constants, `DiagnosticBag` | ALL streams read |
| `X:\blades\kain\src\span.kn` | `Span` struct, `span_line_col()`, `span_from_offsets()` | DELTA reads |
| `X:\blades\kain\src\lexer.kn` | `LexerState`, `lexer_new()`, `lexer_next_token()`, `lexer_tokenize_all()`, indent processor | DELTA reads |
| `X:\blades\kain\src\ast.kn` | ONLY THE CONSTANTS SECTION (~~150 lines of AST_*, BINOP_*, UNOP_* integer tags) | DELTA appends AstNode struct + constructors BELOW your constants |

### Files to Modify

None — all new files.

### Files You Must NOT Touch

| File | Reason |
|------|--------|
| `X:\blades\kain\src\parser.kn` | Owned by Stream DELTA |
| `X:\blades\kain\src\types.kn` | Owned by Stream FOXTROT |
| `X:\blades\kain\src\codegen.kn` | Owned by Stream GOLF |
| `X:\blades\kain\src\jit*.kn` | Owned by Stream BRAVO |
| `X:\blades\kain\src\orchestrator.kn` | Owned by Stream CHARLIE |
| `X:\blades\kain\src\runtime.kn` | Owned by Stream ECHO |

---

## Implementation Tasks

---

### ALPHA-01: TokenKind Enum + Token Struct (`token.kn`)

**Effort:** 1h
**Objective:** Define the complete `TokenKind` enum with 127 variants and the `Token` struct. This is THE single most important type definition in the entire project — every other stream depends on it.

**Implementation Steps:**

1. Create `X:\blades\kain\src\token.kn` with this exact content:

```kn
// token.kn — TokenKind enum and Token struct
// STREAM: ALPHA — sole owner, DO NOT MODIFY outside ALPHA
// Consumed by: ALPHA (lexer), DELTA (parser), FOXTROT (typechecker)

pub enum TokenKind:
    // ═══ Hard Keywords: Core Control & Binding (20) ═══
    Fn = 0
    Let = 1
    Mut = 2
    Var = 3
    Const = 4
    If = 5
    Else = 6
    Elif = 7
    Match = 8
    For = 9
    While = 10
    Loop = 11
    Break = 12
    Continue = 13
    Defer = 14
    Return = 15
    Await = 16
    In = 17
    With = 18
    As = 19

    // ═══ Hard Keywords: Types, Modules & Visibility (10) ═══
    TypeKw = 20
    Struct = 21
    Enum = 22
    Trait = 23
    Impl = 24
    Pub = 25
    Mod = 26
    Use = 27
    SelfLower = 28
    SelfUpper = 29

    // ═══ Hard Keywords: Built-in Literals (3) ═══
    True = 30
    False = 31
    None = 32

    // ═══ Hard Keywords: Effects (7) ═══
    Pure = 33
    Io = 34
    AsyncKw = 35
    Async = 36
    Gpu = 37
    Reactive = 38
    Unsafe = 39

    // ═══ Hard Keywords: First-Class Citizens (18) ═══
    Component = 40
    Shader = 41
    Actor = 42
    State = 43
    Spawn = 44
    Send = 45
    Receive = 46
    Emit = 47
    Comptime = 48
    Macro = 49
    Vertex = 50
    Fragment = 51
    Collapse = 52
    Observe = 53
    Decay = 54
    Share = 55
    Fanout = 56
    Test = 57

    // ═══ Operators (25) ═══
    PlusPlus = 60
    MinusMinus = 61
    Plus = 62
    Minus = 63
    Star = 64
    Slash = 65
    Percent = 66
    Power = 67
    EqEq = 68
    NotEq = 69
    Lt = 70
    Gt = 71
    LtEq = 72
    GtEq = 73
    And = 74
    Or = 75
    Not = 76
    Amp = 77
    Pipe = 78
    Caret = 79
    Tilde = 80
    Shl = 81
    Shr = 82
    Eq = 83
    Arrow = 84

    // ═══ Compound Assignment Operators (11) ═══
    PlusEq = 85
    MinusEq = 86
    StarEq = 87
    SlashEq = 88
    PercentEq = 89
    AmpEq = 90
    PipeEq = 91
    CaretEq = 92
    ShlEq = 93
    ShrEq = 94

    // ═══ Punctuation (16) ═══
    LParen = 95
    RParen = 96
    LBracket = 97
    RBracket = 98
    LBrace = 99
    RBrace = 100
    Comma = 101
    Dot = 102
    DotDot = 103
    DotDotDot = 104
    Colon = 105
    ColonColon = 106
    Semi = 107
    FatArrow = 108
    At = 109

    // ═══ Special (4) ═══
    QuestionQuestion = 110
    QuestionDot = 111
    Question = 112
    LtSlash = 113

    // ═══ Non-Keyword Tokens (6) ═══
    Ident = 114
    Int = 115
    Float = 116
    String = 117
    FString = 118
    Char = 119

    // ═══ Synthetic (inserted by indent processor) ═══
    Newline = 120
    Indent = 121
    Dedent = 122
    Eof = 123
    Comment = 124
    HashComment = 125

    // ═══ Error ═══
    Error = 126

pub struct Token:
    kind:          TokenKind
    text:          String
    line_no:       Int
    col_no:        Int
    byte_offset:   Int
    literal_int:   Int
    literal_float: Float
    literal_string: String

pub fn token_new(kind: TokenKind, text: String, line: Int, col: Int, offset: Int) -> Token:
    return Token {
        kind: kind,
        text: text,
        line_no: line,
        col_no: col,
        byte_offset: offset,
        literal_int: 0,
        literal_float: 0.0,
        literal_string: "",
    }

pub fn token_to_string(tok: Token) -> String:
    return "Token(" + str(tok.byte_offset) + ":" + str(tok.line_no) + ":" + str(tok.col_no) + " " + tok.text + ")"
```

2. Verify the file compiles: `kain check X:\blades\kain\src\token.kn`

**Acceptance Criteria:**
- [ ] `token.kn` exists with exactly 127 TokenKind variants (0–126)
- [ ] `Token` struct has all 8 fields with correct types
- [ ] `token_new()` constructor works correctly
- [ ] `kain check token.kn` passes with zero errors

**Notes:**
- The `TokenKind` enum discriminant values MUST match exactly: 58 hard keywords 0–57, operators 60–94, punctuation 95–113, non-keyword 114–119, synthetic 120–125, Error 126
- `literal_int`, `literal_float`, `literal_string` are zeroed when not applicable
- The `kind` field disciminates which literal field is valid

---

### ALPHA-02: Diagnostic + Error Constants (`error.kn`)

**Effort:** 0.5h
**Objective:** Define the `Diagnostic` struct and all error kind string constants. Every subsystem uses these for error reporting.

**Implementation Steps:**

1. Create `X:\blades\kain\src\error.kn`:

```kn
// error.kn — Diagnostic struct and error constants
// STREAM: ALPHA — sole owner
// Consumed by: ALL streams

pub struct Diagnostic:
    severity:    Int     // 0=error, 1=warning, 2=note, 3=help
    file_path:   String
    line:        Int
    column:      Int
    message:     String
    error_kind:  String
    span_start:  Int
    span_end:    Int
    source_line: String

pub const SEV_ERROR:   Int = 0
pub const SEV_WARNING: Int = 1
pub const SEV_NOTE:    Int = 2
pub const SEV_HELP:    Int = 3

pub const MAX_ERRORS: Int = 50

// ── Error Kind Constants ──
pub const ERR_LEX_UNTERMINATED_STRING: String = "E0001"
pub const ERR_LEX_UNEXPECTED_CHAR:     String = "E0002"
pub const ERR_LEX_INT_OVERFLOW:        String = "E0003"
pub const ERR_LEX_UNTERMINATED_CHAR:   String = "E0004"
pub const ERR_PARSE_EXPECTED_TOKEN:    String = "E0100"
pub const ERR_PARSE_RESERVED_ID:       String = "E0101"
pub const ERR_PARSE_EXPECTED_ITEM:     String = "E0102"
pub const ERR_PARSE_EXPECTED_EXPR:     String = "E0103"
pub const ERR_PARSE_JSX_TAG_MISMATCH:  String = "E0104"
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

pub fn diagnostic_new(severity: Int, path: String, line: Int, col: Int,
                       message: String, kind: String,
                       span_start: Int, span_end: Int, source_line: String) -> Diagnostic:
    return Diagnostic {
        severity: severity,
        file_path: path,
        line: line,
        column: col,
        message: message,
        error_kind: kind,
        span_start: span_start,
        span_end: span_end,
        source_line: source_line,
    }

// DiagnosticBag — accumulator for errors/warnings
pub struct DiagnosticBag:
    errors:   Array<Diagnostic>
    warnings: Array<Diagnostic>
    notes:    Array<Diagnostic>

pub fn diag_bag_new() -> DiagnosticBag:
    return DiagnosticBag {
        errors: empty_array(),
        warnings: empty_array(),
        notes: empty_array(),
    }

pub fn diag_bag_add_error(bag: *mut DiagnosticBag, d: Diagnostic):
    bag.errors.push(d)

pub fn diag_bag_add_warning(bag: *mut DiagnosticBag, d: Diagnostic):
    bag.warnings.push(d)

pub fn diag_bag_has_errors(bag: DiagnosticBag) -> Bool:
    return len(bag.errors) > 0

pub fn diag_bag_too_many(bag: DiagnosticBag) -> Bool:
    return len(bag.errors) >= MAX_ERRORS
```

**Acceptance Criteria:**
- [ ] `error.kn` exists with `Diagnostic` struct (10 fields)
- [ ] All 28 error kind constants defined
- [ ] `DiagnosticBag` with `add_error`, `add_warning`, `has_errors`, `too_many` functions
- [ ] `kain check error.kn` passes

---

### ALPHA-03: Span Helpers + AST Tag Constants (`span.kn` + `ast.kn` constants section)

**Effort:** 0.5h
**Objective:** Define `Span` struct, `span_line_col()` for converting byte offsets to line/column, and write the `ast.kn` constants section (the integer tags for all 38 Item kinds, 12 Stmt kinds, 64 Expr kinds, 21 BinaryOp, 6 UnaryOp, 14 Type AST, 9 Pattern kinds).

**Implementation Steps:**

1. Create `X:\blades\kain\src\span.kn`:

```kn
// span.kn — Source location helpers
// STREAM: ALPHA
// Consumed by: DELTA (parser), FOXTROT (typechecker)

pub struct Span:
    line_start: Int
    col_start:  Int
    line_end:   Int
    col_end:    Int
    byte_start: Int
    byte_end:   Int

pub fn span_new(line_s: Int, col_s: Int, line_e: Int, col_e: Int, byte_s: Int, byte_e: Int) -> Span:
    return Span {
        line_start: line_s,
        col_start: col_s,
        line_end: line_e,
        col_end: col_e,
        byte_start: byte_s,
        byte_end: byte_e,
    }

// Convert byte offset to (line_no, col_no) — both 1-based
pub fn span_line_col(source: String, byte_offset: Int) -> Span:
    var line: Int = 1
    var col: Int = 1
    var i: Int = 0
    while i < byte_offset and i < len(source):
        let c: String = source[i]
        if c == "\n":
            line = line + 1
            col = 1
        else:
            col = col + 1
        i = i + 1
    return Span {
        line_start: line,
        col_start: col,
        line_end: line,
        col_end: col,
        byte_start: byte_offset,
        byte_end: byte_offset,
    }
```

2. Create the constants-only section in `X:\blades\kain\src\ast.kn` (write ONLY this section, delimited by markers):

```kn
// ast.kn — AST tag constants and AstNode struct
// ═══════════════════════════════════════════════════════════════════════
// SECTION: STREAM ALPHA — AST tag constants (DO NOT MODIFY outside ALPHA)
// ═══════════════════════════════════════════════════════════════════════
// Consumed by: DELTA (parser), FOXTROT (typechecker), GOLF (codegen)
//
// NOTE: The AstNode struct and helper functions are in the DELTA section
//       at the bottom of this file. See `// ═══ SECTION: STREAM DELTA ═══`.

// ── Item Kinds (38) ──
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
pub const AST_ITEM_IMPORT:          Int = 24
pub const AST_ITEM_MATERIAL_GRAPH:  Int = 25
pub const AST_ITEM_GRAPH_EDITOR:    Int = 26
pub const AST_ITEM_PROGRAM:         Int = 37

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
pub const AST_STMT_ITEM:            Int = 61

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

// ═══════════════════════════════════════════════════════════════════════
// END STREAM ALPHA SECTION — DELTA appends AstNode struct below this line
// ═══════════════════════════════════════════════════════════════════════
```

**Acceptance Criteria:**
- [ ] `span.kn` exists with `Span` struct and `span_line_col()` function
- [ ] `ast.kn` exists with ALL AST tag constants (38 Item, 12 Stmt, 64 Expr, 14 Type, 9 Pattern, 21 BinOp, 6 UnaryOp)
- [ ] The `ast.kn` file ends with the "END STREAM ALPHA SECTION" marker exactly as shown
- [ ] `kain check span.kn` and `kain check ast.kn` pass

**Notes:**
- The marker comment `// ═══════════════════ END STREAM ALPHA SECTION ═══════════════════` is CRITICAL. DELTA's agent will look for this marker and append BELOW it. Do not change the format.
- The AstNode struct lives in DELTA, not here. ALPHA only provides the integer tag constants.

---

### ALPHA-04: Lexer DFA — Core Tokenizer (`lexer.kn`, part 1)

**Effort:** 1.5h
**Objective:** Implement the hand-written DFA lexer that converts source text into raw tokens (before indent processing). This includes keyword recognition, operator longest-match, string/char/number lexing, and comment skipping.

**Implementation Steps:**

1. Create `X:\blades\kain\src\lexer.kn` with imports and the `LexerState` struct:

```kn
// lexer.kn — Hand-written DFA Lexer + Indent Processor
// STREAM: ALPHA
// Consumed by: DELTA (parser)
use std::text
use std::collections

// Import shared types
use src::token::{TokenKind, Token, token_new}
use src::error::{Diagnostic, DiagnosticBag, diag_bag_new, diag_bag_add_error,
                  SEV_ERROR, ERR_LEX_UNTERMINATED_STRING, ERR_LEX_UNEXPECTED_CHAR,
                  ERR_LEX_INT_OVERFLOW, ERR_LEX_UNTERMINATED_CHAR}
use src::span::{Span, span_new, span_line_col}

pub struct LexerState:
    source:      String
    file_path:   String
    pos:         Int          // current byte position
    line:        Int          // current 1-based line
    col:         Int          // current 1-based column
    tokens:      Array<Token>
    errors:      DiagnosticBag

pub fn lexer_new(source: String, file_path: String) -> LexerState:
    return LexerState {
        source: source,
        file_path: file_path,
        pos: 0,
        line: 1,
        col: 1,
        tokens: empty_array(),
        errors: diag_bag_new(),
    }

// ── Helper: peek current char ──
pub fn lexer_current(state: LexerState) -> String:
    if state.pos >= len(state.source):
        return ""
    return state.source[state.pos]

// ── Helper: peek ahead by N bytes ──
pub fn lexer_peek(state: LexerState, ahead: Int) -> String:
    let idx: Int = state.pos + ahead
    if idx >= len(state.source):
        return ""
    return state.source[idx]

// ── Helper: advance by N bytes ──
pub fn lexer_advance(state: *mut LexerState, n: Int):
    var i: Int = 0
    while i < n and state.pos < len(state.source):
        if state.source[state.pos] == "\n":
            state.line = state.line + 1
            state.col = 1
        else:
            state.col = state.col + 1
        state.pos = state.pos + 1
        i = i + 1

// ── Helper: push a token ──
pub fn lexer_push(state: *mut LexerState, kind: TokenKind, text: String, start_line: Int, start_col: Int, start_pos: Int):
    let tok: Token = token_new(kind, text, start_line, start_col, start_pos)
    state.tokens.push(tok)

// ── Helper: push an error diagnostic ──
pub fn lexer_error(state: *mut LexerState, message: String, kind_str: String, start_pos: Int):
    let span: Span = span_line_col(state.source, start_pos)
    let src_line: String = ""  // source line for caret display (simplified)
    let diag: Diagnostic = diagnostic_new(SEV_ERROR, state.file_path, span.line_start,
        span.col_start, message, kind_str, start_pos, state.pos, src_line)
    diag_bag_add_error(state.errors, diag)
```

2. Continue with the keyword hash table and `lex_ident_or_keyword()`:

```kn
// ── Keyword Map: string → TokenKind for all 58 hard keywords ──
pub fn lexer_keyword_map(name: String) -> TokenKind:
    if name == "fn":         return TokenKind::Fn
    if name == "let":        return TokenKind::Let
    if name == "mut":        return TokenKind::Mut
    if name == "var":        return TokenKind::Var
    if name == "const":      return TokenKind::Const
    if name == "if":         return TokenKind::If
    if name == "else":       return TokenKind::Else
    if name == "elif":       return TokenKind::Elif
    if name == "match":      return TokenKind::Match
    if name == "for":        return TokenKind::For
    if name == "while":      return TokenKind::While
    if name == "loop":       return TokenKind::Loop
    if name == "break":      return TokenKind::Break
    if name == "continue":   return TokenKind::Continue
    if name == "defer":      return TokenKind::Defer
    if name == "return":     return TokenKind::Return
    if name == "await":      return TokenKind::Await
    if name == "in":         return TokenKind::In
    if name == "with":       return TokenKind::With
    if name == "as":         return TokenKind::As
    if name == "type":       return TokenKind::TypeKw
    if name == "struct":     return TokenKind::Struct
    if name == "enum":       return TokenKind::Enum
    if name == "trait":      return TokenKind::Trait
    if name == "impl":       return TokenKind::Impl
    if name == "pub":        return TokenKind::Pub
    if name == "mod":        return TokenKind::Mod
    if name == "use":        return TokenKind::Use
    if name == "self":       return TokenKind::SelfLower
    if name == "Self":       return TokenKind::SelfUpper
    if name == "true":       return TokenKind::True
    if name == "false":      return TokenKind::False
    if name == "none":       return TokenKind::None
    if name == "Pure":       return TokenKind::Pure
    if name == "IO":         return TokenKind::Io
    if name == "async":      return TokenKind::AsyncKw
    if name == "Async":      return TokenKind::Async
    if name == "GPU":        return TokenKind::Gpu
    if name == "Reactive":   return TokenKind::Reactive
    if name == "Unsafe":     return TokenKind::Unsafe
    if name == "component":  return TokenKind::Component
    if name == "shader":     return TokenKind::Shader
    if name == "actor":      return TokenKind::Actor
    if name == "state":      return TokenKind::State
    if name == "spawn":      return TokenKind::Spawn
    if name == "send":       return TokenKind::Send
    if name == "receive":    return TokenKind::Receive
    if name == "emit":       return TokenKind::Emit
    if name == "comptime":   return TokenKind::Comptime
    if name == "macro":      return TokenKind::Macro
    if name == "vertex":     return TokenKind::Vertex
    if name == "fragment":   return TokenKind::Fragment
    if name == "collapse":   return TokenKind::Collapse
    if name == "observe":    return TokenKind::Observe
    if name == "decay":      return TokenKind::Decay
    if name == "share":      return TokenKind::Share
    if name == "fanout":     return TokenKind::Fanout
    if name == "test":       return TokenKind::Test
    if name == "and":        return TokenKind::And
    if name == "or":         return TokenKind::Or
    // Not a keyword → Ident
    return TokenKind::Error   // sentinel; caller checks
```

3. Implement `lexer_next_token()` — the main DFA dispatch:

```kn
pub fn lexer_next_token(state: *mut LexerState) -> Token:
    // Skip whitespace except newlines
    while state.pos < len(state.source):
        let c: String = state.source[state.pos]
        if c == " " or c == "\t" or c == "\r":
            lexer_advance(state, 1)
            continue
        break

    // Check EOF
    if state.pos >= len(state.source):
        return token_new(TokenKind::Eof, "", state.line, state.col, state.pos)

    let c: String = state.source[state.pos]
    let start_line: Int = state.line
    let start_col: Int = state.col
    let start_pos: Int = state.pos

    // Newline — captured with following whitespace
    if c == "\n":
        lexer_advance(state, 1)
        var ws: String = ""
        while state.pos < len(state.source):
            let nc: String = state.source[state.pos]
            if nc == " " or nc == "\t" or nc == "\r":
                ws = ws + nc
                lexer_advance(state, 1)
            else:
                break
        let tok: Token = token_new(TokenKind::Newline, ws, start_line, start_col, start_pos)
        tok.literal_string = "\n" + ws
        return tok

    // Line comment //
    if c == "/" and lexer_peek(state, 1) == "/":
        lexer_advance(state, 2)
        while state.pos < len(state.source) and state.source[state.pos] != "\n":
            lexer_advance(state, 1)
        return lexer_next_token(state)  // tail-recursive skip

    // Hash comment #
    if c == "#" and (start_col == 1 or (start_pos > 0 and state.source[start_pos - 1] == "\n")):
        lexer_advance(state, 1)
        while state.pos < len(state.source) and state.source[state.pos] != "\n":
            lexer_advance(state, 1)
        return lexer_next_token(state)

    // String literal "..."
    if c == "\"":
        return lexer_lex_string(state, start_line, start_col, start_pos)

    // Format string f"..."
    if c == "f" and lexer_peek(state, 1) == "\"":
        lexer_advance(state, 1)  // skip 'f'
        return lexer_lex_string(state, start_line, start_col, start_pos, true)

    // Char literal 'c'
    if c == "'":
        return lexer_lex_char(state, start_line, start_col, start_pos)

    // Number literal (decimal, hex 0x, octal 0o, binary 0b)
    if lexer_is_digit(c) or (c == "0" and (lexer_peek(state, 1) == "x" or lexer_peek(state, 1) == "o" or lexer_peek(state, 1) == "b")):
        return lexer_lex_number(state, start_line, start_col, start_pos)

    // Identifier or keyword
    if lexer_is_alpha(c) or c == "_":
        return lexer_lex_ident(state, start_line, start_col, start_pos)

    // Operators and punctuation — longest match first
    return lexer_lex_operator(state, start_line, start_col, start_pos)
```

Now write the `lexer_lex_ident`, `lexer_lex_string`, `lexer_lex_number`, `lexer_lex_char`, and `lexer_lex_operator` functions. These should follow the exact algorithm from the design doc (research 01 §2.3). Include all 25 operators, 11 compound assignments, and 16 punctuation tokens with longest-match dispatch.

Key sub-functions to implement:
- `lexer_is_digit(c: String) -> Bool`
- `lexer_is_alpha(c: String) -> Bool`
- `lexer_is_alnum(c: String) -> Bool`
- `lexer_lex_ident(state, line, col, pos) -> Token` — reads [a-zA-Z_][a-zA-Z0-9_]* then checks keyword map
- `lexer_lex_string(state, line, col, pos, is_fstring=false) -> Token` — handles \" \" \n \\t \\\\ escape sequences
- `lexer_lex_number(state, line, col, pos) -> Token` — handles decimal, hex(0x), octal(0o), binary(0b), underscores, float (with .)
- `lexer_lex_char(state, line, col, pos) -> Token` — single char with escape support
- `lexer_lex_operator(state, line, col, pos) -> Token` — longest-match dispatch for all operators and punctuation

**Operator longest-match dispatch table (in `lexer_lex_operator`):**
```
// + family: ++  +=  +
// - family: --  -=  ->  -
// * family: **  *=  *
// / family: /=  /
// % family: %=  %
// & family: &&  &=  &
// | family: ||  |=  |
// ^ family: ^=  ^
// ~ family: ~
// = family: ==  =>
// ! family: !=
// < family: <<=  <<  <=  </
// > family: >>=  >>  >=
// . family: ...  ..  .
// : family: ::
// ? family: ??  ?.  ?
// other: (  )  [  ]  {  }  ,  ;  @
```

**Acceptance Criteria:**
- [ ] `lexer_next_token()` correctly produces tokens for all 127 TokenKind variants
- [ ] Keyword recognition: 58 hard keywords produce dedicated TokenKind variants; all other identifiers produce `TokenKind::Ident`
- [ ] Contextual keywords (patch, law, world, converge, etc.) are NEVER recognized — they produce `TokenKind::Ident`
- [ ] Comment skipping works: `//` and `#` comments are discarded
- [ ] String escapes: `\n`, `\t`, `\\`, `\"`, `\0` correctly resolved
- [ ] Number literals: decimal, 0x hex, 0o octal, 0b binary, underscore separators, float with `.`
- [ ] Operator longest-match: `++` not `+`+`+`, `<=` not `<`+`=`
- [ ] Error handling: unterminated string → E0001 diagnostic; unexpected char → E0002

---

### ALPHA-05: Lexer Convenience Function (`lexer_tokenize_all`)

**Effort:** 0.25h
**Objective:** Wrap the token-at-a-time lexer with a convenience function that tokenizes an entire source string in one call.

**Implementation:**

```kn
pub fn lexer_tokenize_all(source: String, file_path: String) -> Array<Token>:
    let mut state: LexerState = lexer_new(source, file_path)
    loop:
        let tok: Token = lexer_next_token(state)
        let is_eof: Bool = tok.kind == TokenKind::Eof
        state.tokens.push(tok)
        if is_eof:
            break
        if state.pos >= len(source):
            break
    return state.tokens
```

**Acceptance Criteria:**
- [ ] `lexer_tokenize_all("fn main(): return 42", "test")` produces the correct token sequence
- [ ] Empty source produces single `Eof` token

---

### ALPHA-06: Indent Processor (`indent_process`)

**Effort:** 1h
**Objective:** Implement the post-lexer pass that inserts synthetic `Indent`, `Dedent`, `Newline`, and `Eof` tokens based on indentation level changes.

**Implementation in `lexer.kn`:**

```kn
pub fn indent_process(raw_tokens: Array<Token>) -> Array<Token>:
    let mut result: Array<Token> = empty_array()
    let mut indent_stack: Array<Int> = array_init(1)
    indent_stack[0] = 0
    var paren_depth: Int = 0
    var bracket_depth: Int = 0
    var brace_depth: Int = 0

    var i: Int = 0
    let n: Int = len(raw_tokens)
    while i < n:
        let tok: Token = raw_tokens[i]

        // Track bracket depths
        if tok.kind == TokenKind::LParen or tok.kind == TokenKind::LBracket or tok.kind == TokenKind::LBrace:
            // ... increment appropriate depth counter
            // ...

        if tok.kind == TokenKind::RParen or tok.kind == TokenKind::RBracket or tok.kind == TokenKind::RBrace:
            // ... decrement appropriate depth counter
            // ...

        // Handle newlines
        if tok.kind == TokenKind::Newline:
            // Suppress inside brackets
            if paren_depth > 0 or bracket_depth > 0 or brace_depth > 0:
                i = i + 1
                continue

            // Suppress blank lines (consecutive Newlines)
            if i + 1 < n and raw_tokens[i + 1].kind == TokenKind::Newline:
                i = i + 1
                continue

            // Compute indent from whitespace string (tab = 4 spaces)
            let ws: String = tok.literal_string
            let indent: Int = compute_indent(ws)

            let current: Int = indent_stack[len(indent_stack) - 1]
            if indent > current:
                indent_stack.push(indent)
                result.push(tok)  // keep the Newline
                // ... push synthetic Indent token
                // ...
            elif indent < current:
                result.push(tok)  // keep the Newline
                while len(indent_stack) > 1 and indent_stack[len(indent_stack) - 1] > indent:
                    indent_stack.pop()
                    // ... push synthetic Dedent token
                    // ...
            else:
                result.push(tok)  // same indent, keep Newline
        else:
            result.push(tok)

        i = i + 1

    // EOF cleanup: pop all remaining indent levels
    while len(indent_stack) > 1:
        indent_stack.pop()
        // ... push Dedent token
        // ...

    // Push Eof
    let eof_tok: Token = token_new(TokenKind::Eof, "", 0, 0, 0)
    result.push(eof_tok)

    return result

pub fn compute_indent(ws: String) -> Int:
    var total: Int = 0
    var i: Int = 0
    while i < len(ws):
        if ws[i] == " ":
            total = total + 1
        elif ws[i] == "\t":
            total = total + 4  // tab = 4 spaces
        i = i + 1
    return total
```

**Acceptance Criteria:**
- [ ] `indent_process()` correctly handles all 6 indent processor rules (FR-LEX.17–22):
  - [ ] Newlines inside `()`, `[]`, `{}` suppressed
  - [ ] Blank lines (consecutive newlines) discarded
  - [ ] Indent increase → push Indent
  - [ ] Indent decrease → pop + emit Dedent(s)
  - [ ] Tabs treated as 4 spaces
  - [ ] EOF → pop remaining levels + append Eof

---

## Stream Conventions

- **Language:** Pure Kain (Layer 0 constructs only: fn, struct, enum, let, while, if, return, const)
- **Naming:** snake_case for functions and variables; PascalCase for structs and enums; SCREAMING_SNAKE_CASE for constants
- **Imports:** `use src::module` for sibling modules; `use std::module` for stdlib
- **Error handling:** Accumulate errors in `DiagnosticBag`; never panic on malformed input
- **Comments:** Document every function with a brief comment describing what it does and its edge cases
- **Testing:** Each module should have corresponding test functions at the bottom of the file using compiletest directives or inline `test fn` blocks

---

## Stream Boundary — What You Do NOT Do

- ❌ Do NOT implement the parser (that's DELTA's job)
- ❌ Do NOT implement the typechecker (FOXTROT's job)
- ❌ Do NOT implement codegen (GOLF's job)
- ❌ Do NOT modify `ast.kn` below the "END STREAM ALPHA SECTION" marker — that space is reserved for DELTA
- ❌ Do NOT use Layer 1–7 constructs (world, actor, converge, etc.) — this is constraint C-1
- ❌ Do NOT use `Box<T>`, `Arc<T>`, or heap-allocated recursive types — constraint C-6

---

## Verification (After This Stream)

```bash
# Check individual files
kain check X:\blades\kain\src\token.kn
kain check X:\blades\kain\src\error.kn
kain check X:\blades\kain\src\span.kn
kain check X:\blades\kain\src\ast.kn
kain check X:\blades\kain\src\lexer.kn

# Typecheck everything together
kain check X:\blades\kain\src\token.kn X:\blades\kain\src\error.kn X:\blades\kain\src\span.kn X:\blades\kain\src\ast.kn X:\blades\kain\src\lexer.kn

# Run inline tests (if any)
kain test X:\blades\kain\src\lexer.kn
```

**Self-check:**
- [ ] All 5 files created with correct content
- [ ] TokenKind enum has exactly 127 variants (0–126)
- [ ] Lexer produces correct tokens for a known test file
- [ ] Indent processor correctly inserts INDENT/DEDENT tokens
- [ ] Error cases produce diagnostics (not panics)
- [ ] `ast.kn` ends with the "END STREAM ALPHA SECTION" marker

---

## Completion Report

When done, report:
- Files created: token.kn, error.kn, span.kn, ast.kn (constants section), lexer.kn — with line counts
- TokenKind variants: 127
- Lexer DFA: complete with keyword map, operator dispatch, string/number/char lexing
- Indent processor: complete with all 6 rules
- Any issues encountered
- Whether DELTA's agent can safely start reading token.kn + error.kn
