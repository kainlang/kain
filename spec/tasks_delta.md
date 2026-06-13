# Stream DELTA: Parser + AST Implementation

**Stream ID:** DELTA
**Role:** Define the flat-array AstNode struct with constructors and implement the complete recursive-descent + Pratt parser that converts `Array<Token>` into `Array<AstNode>`
**Effort:** ~12 hours
**Depends On:** Stream ALPHA (token.kn TokenKind+Token, error.kn Diagnostic, span.kn Span, ast.kn AST_* constants)
**Requirements Covered:** FR-PARSE.1–74
**Design Reference:** Research 01 §§3.1–3.7, §§4.1–4.8; Design §§PARSE

---

## Context

You implement the parser — the largest single subsystem (~3000 lines of parser.kn + ~500 lines of ast.kn implementation). The parser converts indent-processed `Array<Token>` into a flat `Array<AstNode>` using integer-index child references. The Pratt expression parser handles 16 precedence levels with left/right associativity. The recursive-descent item parser dispatches to 38 specialized `parse_*()` functions. Error recovery uses `synchronize()` to skip to the next item boundary.

**Critical dependency:** You MUST read ALPHA's completed `token.kn` (TokenKind enum, Token struct), `error.kn` (Diagnostic), `span.kn` (Span), and `ast.kn` (AST_*, BINOP_*, UNOP_* constants). These are your imports.

**IMPORTANT:** The `ast.kn` file already exists from ALPHA with the constants section ending at `// ═══════════════════ END STREAM ALPHA SECTION ═══════════════════`. You APPEND your AstNode struct and helper functions BELOW that marker. Do NOT modify anything above the marker.

---

## Files You Own

### Files to Create

| File | Purpose | After This Stream |
|------|---------|-------------------|
| `X:\blades\kain\src\parser.kn` | Complete recursive-descent + Pratt parser (~3000 lines) | FOXTROT reads |
| `X:\blades\kain\spec\parser_spec.md` | Parser test specification (~200 lines) | GOLF reads for integration tests |

### Files to Modify

| File | Region/Function | Change Description | After This Stream |
|------|-----------------|--------------------|--------------------|
| `X:\blades\kain\src\ast.kn` | Append AFTER the "END STREAM ALPHA SECTION" marker | Add `AstNode` struct, `ast_new_node()`, `ast_push_child()`, `ast_get_child()`, `ast_data_len()`, `ast_kind_name()`, `ast_dump_node()`, string table helpers, `AstProgram` wrapper | FOXTROT consumes AstNode; GOLF consumes AstNode |

### Files You Must NOT Touch

| File | Reason |
|------|--------|
| `X:\blades\kain\src\token.kn` | Owned by ALPHA (read-only) |
| `X:\blades\kain\src\error.kn` | Owned by ALPHA (read-only) |
| `X:\blades\kain\src\lexer.kn` | Owned by ALPHA (read-only) |
| `X:\blades\kain\src\types.kn` | Owned by FOXTROT |
| `X:\blades\kain\src\codegen.kn` | Owned by GOLF |
| `X:\blades\kain\src\ast.kn` ABOVE the END STREAM ALPHA marker | Owned by ALPHA — DO NOT MODIFY |

---

## Implementation Tasks

---

### DELTA-01: AstNode Struct + Node Constructors (append to `ast.kn`)

**Effort:** 1.5h
**Objective:** Append the flat AstNode struct, constructor functions, child access helpers, string table, and AST dump utilities BELOW ALPHA's section in `ast.kn`.

**Implementation:**

Open `X:\blades\kain\src\ast.kn` and append AFTER the line:
```
// ═══════════════════════════════════════════════════════════════════════
// END STREAM ALPHA SECTION — DELTA appends AstNode struct below this line
// ═══════════════════════════════════════════════════════════════════════
```

Add:

```kn
// ═══════════════════════════════════════════════════════════════════════
// SECTION: STREAM DELTA — AstNode struct + helpers (appended by DELTA)
// ═══════════════════════════════════════════════════════════════════════
// Consumed by: FOXTROT (typechecker), GOLF (codegen)

// ── Core AstNode struct ──
// Flat representation: parent-child via integer index into Array<AstNode>
// INVARIANT: kind disrciminates how data[] is interpreted
pub struct AstNode:
    kind:       Int
    span_start: Int
    span_end:   Int
    data:       Array<Int>

// ── Constructor ──
pub fn ast_new_node(kind: Int, span_start: Int, span_end: Int, data: Array<Int>) -> AstNode:
    return AstNode {
        kind: kind,
        span_start: span_start,
        span_end: span_end,
        data: data,
    }

// ── Simple node constructors for common patterns ──
pub fn ast_new_leaf(kind: Int, span_start: Int, span_end: Int, value: Int) -> AstNode:
    let mut data: Array<Int> = empty_array()
    data.push(value)
    return ast_new_node(kind, span_start, span_end, data)

pub fn ast_new_empty(kind: Int, span_start: Int, span_end: Int) -> AstNode:
    return ast_new_node(kind, span_start, span_end, empty_array())

pub fn ast_new_child(kind: Int, span_start: Int, span_end: Int, child: Int) -> AstNode:
    let mut data: Array<Int> = empty_array()
    data.push(child)
    return ast_new_node(kind, span_start, span_end, data)

pub fn ast_new_two(kind: Int, span_start: Int, span_end: Int, a: Int, b: Int) -> AstNode:
    let mut data: Array<Int> = empty_array()
    data.push(a)
    data.push(b)
    return ast_new_node(kind, span_start, span_end, data)

pub fn ast_new_three(kind: Int, span_start: Int, span_end: Int, a: Int, b: Int, c: Int) -> AstNode:
    let mut data: Array<Int> = empty_array()
    data.push(a)
    data.push(b)
    data.push(c)
    return ast_new_node(kind, span_start, span_end, data)

// ── Child access ──
pub fn ast_data_len(node: AstNode) -> Int:
    return len(node.data)

pub fn ast_data_get(node: AstNode, index: Int) -> Int:
    if index < 0 or index >= len(node.data):
        return -1
    return node.data[index]

// ── AstProgram wrapper: the root node + all nodes ──
pub struct AstProgram:
    root:     Int             // index of AST_ITEM_PROGRAM node
    nodes:    Array<AstNode>   // flat array of all AST nodes

// ── String table (for identifier interning) ──
pub struct StringTable:
    strings:   Array<String>
    index:     HashMap<String, Int>

pub fn strtab_new() -> StringTable:
    return StringTable {
        strings: empty_array(),
        index: empty_map(),
    }

pub fn strtab_intern(table: *mut StringTable, s: String) -> Int:
    // Check if already interned
    if table.index.has(s):
        return table.index.get(s)
    let idx: Int = len(table.strings)
    table.strings.push(s)
    table.index.insert(s, idx)
    return idx

pub fn strtab_get(table: StringTable, idx: Int) -> String:
    if idx < 0 or idx >= len(table.strings):
        return ""
    return table.strings[idx]

// ── AST dump for debugging ──
pub fn ast_kind_name(kind: Int) -> String:
    if kind == AST_ITEM_FUNCTION: return "Fn"
    if kind == AST_ITEM_STRUCT:   return "Struct"
    if kind == AST_ITEM_ENUM:     return "Enum"
    if kind == AST_ITEM_TRAIT:    return "Trait"
    if kind == AST_ITEM_IMPL:     return "Impl"
    if kind == AST_ITEM_CONST:    return "Const"
    if kind == AST_ITEM_USE:      return "Use"
    if kind == AST_ITEM_MOD:      return "Mod"
    if kind == AST_EXPR_INT:      return "Int"
    if kind == AST_EXPR_FLOAT:    return "Float"
    if kind == AST_EXPR_STRING:   return "String"
    if kind == AST_EXPR_BOOL:     return "Bool"
    if kind == AST_EXPR_NONE:     return "None"
    if kind == AST_EXPR_IDENT:    return "Ident"
    if kind == AST_EXPR_BINARY:   return "Binary"
    if kind == AST_EXPR_UNARY:    return "Unary"
    if kind == AST_EXPR_CALL:     return "Call"
    if kind == AST_EXPR_FIELD:    return "Field"
    if kind == AST_EXPR_INDEX:    return "Index"
    if kind == AST_EXPR_ASSIGN:   return "Assign"
    if kind == AST_EXPR_IF:       return "If"
    if kind == AST_EXPR_MATCH:    return "Match"
    if kind == AST_EXPR_BLOCK:    return "Block"
    if kind == AST_EXPR_REF:      return "Ref"
    if kind == AST_EXPR_DEREF:    return "Deref"
    if kind == AST_EXPR_CAST:     return "Cast"
    if kind == AST_EXPR_TRY:      return "Try"
    if kind == AST_EXPR_AWAIT:    return "Await"
    if kind == AST_EXPR_LAMBDA:   return "Lambda"
    if kind == AST_EXPR_RANGE:    return "Range"
    if kind == AST_EXPR_STRUCT_LIT: return "StructLit"
    if kind == AST_EXPR_ARRAY:    return "Array"
    if kind == AST_EXPR_TUPLE:    return "Tuple"
    if kind == AST_EXPR_COLLAPSE: return "Collapse"
    if kind == AST_EXPR_OBSERVE:  return "Observe"
    if kind == AST_EXPR_DECAY:    return "Decay"
    if kind == AST_EXPR_JSX:      return "JSX"
    if kind == AST_STMT_LET:      return "Let"
    if kind == AST_STMT_RETURN:   return "Return"
    if kind == AST_STMT_FOR:      return "For"
    if kind == AST_STMT_WHILE:    return "While"
    if kind == AST_STMT_LOOP:     return "Loop"
    if kind == AST_STMT_BREAK:    return "Break"
    if kind == AST_STMT_CONTINUE: return "Continue"
    if kind == AST_STMT_DEFER:    return "Defer"
    if kind == AST_STMT_EXPR:     return "ExprStmt"
    if kind == AST_ITEM_PROGRAM:  return "Program"
    return "Unknown(" + str(kind) + ")"

// ═══════════════════════════════════════════════════════════════════════
// END STREAM DELTA SECTION
// ═══════════════════════════════════════════════════════════════════════
```

**Acceptance Criteria:**
- [ ] `AstNode` struct appended to `ast.kn` with kind, span_start, span_end, data fields
- [ ] All constructor functions (ast_new_node, ast_new_leaf, ast_new_empty, ast_new_child, ast_new_two, ast_new_three)
- [ ] `StringTable` with interning support
- [ ] `ast_kind_name()` debug function for at least 30+ kinds
- [ ] `AstProgram` wrapper struct
- [ ] `kain check ast.kn` passes with ALPHA section + DELTA section combined

---

### DELTA-02: ParserState + Core Helpers (`parser.kn`, part 1)

**Effort:** 0.5h
**Objective:** Define `ParserState` struct, token cursor helpers, and identifier interning.

**Implementation:**

Create `X:\blades\kain\src\parser.kn`:

```kn
// parser.kn — Recursive-descent + Pratt parser
// STREAM: DELTA
// Consumed by: FOXTROT (typechecker reads parsed AST)

use src::token::{TokenKind, Token, token_new}
use src::error::{Diagnostic, DiagnosticBag, diag_bag_new, diag_bag_add_error,
                  SEV_ERROR, SEV_WARNING, SEV_NOTE, MAX_ERRORS,
                  ERR_PARSE_EXPECTED_TOKEN, ERR_PARSE_RESERVED_ID,
                  ERR_PARSE_EXPECTED_ITEM, ERR_PARSE_EXPECTED_EXPR,
                  ERR_PARSE_JSX_TAG_MISMATCH}
use src::span::{Span, span_new}
use src::ast::{AstNode, AstProgram, StringTable, strtab_new, strtab_intern, strtab_get,
                ast_new_node, ast_new_leaf, ast_new_empty, ast_new_child,
                ast_new_two, ast_new_three, ast_data_len, ast_data_get,
                AST_ITEM_FUNCTION, AST_ITEM_PROGRAM,
                AST_STMT_LET, AST_STMT_RETURN, AST_STMT_EXPR,
                AST_EXPR_INT, AST_EXPR_FLOAT, AST_EXPR_STRING,
                AST_EXPR_BOOL, AST_EXPR_NONE, AST_EXPR_IDENT,
                AST_EXPR_BINARY, AST_EXPR_UNARY, AST_EXPR_CALL,
                AST_EXPR_FIELD, AST_EXPR_INDEX, AST_EXPR_ASSIGN,
                AST_EXPR_IF, AST_EXPR_MATCH, AST_EXPR_BLOCK,
                AST_EXPR_REF, AST_EXPR_DEREF, AST_EXPR_CAST,
                AST_EXPR_STRUCT_LIT, AST_EXPR_ARRAY, AST_EXPR_TUPLE,
                BINOP_ADD, BINOP_SUB, BINOP_MUL, BINOP_DIV, BINOP_MOD,
                BINOP_EQ, BINOP_NE, BINOP_LT, BINOP_GT, BINOP_LE, BINOP_GE,
                BINOP_AND, BINOP_OR, UNOP_NEG, UNOP_NOT, UNOP_DEREF, UNOP_REF}

// (Import remaining AST_* constants as needed)

pub struct ParserState:
    tokens:         Array<Token>
    pos:            Int
    program:        Array<AstNode>
    errors:         DiagnosticBag
    string_table:   StringTable
    loop_stack:     Array<LoopLabel>
    injected:       Array<Token>
    synth_counter:  Int

pub struct LoopLabel:
    continue_idx: Int
    break_idx:    Int

// ── Parser constructor ──
pub fn parser_new(tokens: Array<Token>, file_path: String) -> ParserState:
    return ParserState {
        tokens: tokens,
        pos: 0,
        program: empty_array(),
        errors: diag_bag_new(),
        string_table: strtab_new(),
        loop_stack: empty_array(),
        injected: empty_array(),
        synth_counter: 0,
    }

// ── Token cursor ──
pub fn parser_current(state: ParserState) -> Token:
    if state.pos >= len(state.tokens):
        return token_new(TokenKind::Eof, "", 0, 0, 0)
    return state.tokens[state.pos]

pub fn parser_peek(state: ParserState, ahead: Int) -> Token:
    let idx: Int = state.pos + ahead
    if idx >= len(state.tokens):
        return token_new(TokenKind::Eof, "", 0, 0, 0)
    return state.tokens[idx]

pub fn parser_advance(state: *mut ParserState):
    if state.pos < len(state.tokens):
        state.pos = state.pos + 1

pub fn parser_check(state: ParserState, kind: TokenKind) -> Bool:
    return parser_current(state).kind == kind

pub fn parser_expect(state: *mut ParserState, kind: TokenKind) -> Token:
    let tok: Token = parser_current(state)
    if tok.kind == kind:
        parser_advance(state)
        return tok
    // Error: expected different token
    let msg: String = "expected " + token_kind_name(kind) + ", found " + tok.text
    let diag: Diagnostic = diagnostic_new(SEV_ERROR, "", tok.line_no, tok.col_no,
        msg, ERR_PARSE_EXPECTED_TOKEN, tok.byte_offset, tok.byte_offset + len(tok.text), "")
    diag_bag_add_error(state.errors, diag)
    return tok

// ── Intern identifier string ──
pub fn parser_intern(state: *mut ParserState, name: String) -> Int:
    return strtab_intern(state.string_table, name)

// ── Push a node into the flat array, return its index ──
pub fn parser_push_node(state: *mut ParserState, node: AstNode) -> Int:
    let idx: Int = len(state.program)
    state.program.push(node)
    return idx

// ── TokenKind → human-readable name (for error messages) ──
pub fn token_kind_name(kind: TokenKind) -> String:
    if kind == TokenKind::Fn:       return "'fn'"
    if kind == TokenKind::Let:      return "'let'"
    if kind == TokenKind::If:       return "'if'"
    if kind == TokenKind::Ident:    return "identifier"
    if kind == TokenKind::Int:      return "integer literal"
    if kind == TokenKind::String:   return "string literal"
    if kind == TokenKind::Eof:      return "end of file"
    if kind == TokenKind::LParen:   return "'('"
    if kind == TokenKind::RParen:   return "')'"
    if kind == TokenKind::LBrace:   return "'{'"
    if kind == TokenKind::RBrace:   return "'}'"
    if kind == TokenKind::Colon:    return "':'"
    if kind == TokenKind::Eq:       return "'='"
    if kind == TokenKind::Arrow:    return "'->'"
    if kind == TokenKind::Semi:     return "';'"
    if kind == TokenKind::Newline:  return "newline"
    if kind == TokenKind::Indent:   return "indent"
    if kind == TokenKind::Dedent:   return "dedent"
    return "'" + token_kind_to_str(kind) + "'"

pub fn token_kind_to_str(kind: TokenKind) -> String:
    // Simple: return the discriminant as string
    // This is called by error messages
    return "<kind=" + str(kind as Int) + ">"
```

---

### DELTA-03 through DELTA-12: Item Parsers, Statement Parsers, Expression Parser

**Effort:** 8h total (DELTA-03 through DELTA-12)
**Objective:** Implement all parser functions as specified in the design document.

These are the CRITICAL tasks. The parser is the largest single file. Here is the complete specification for each function:

**DELTA-03: Top-Level Parsing (0.5h)**
- `pub fn parse(state: *mut ParserState) -> AstProgram` — main entry point
- Loop at indent depth 0, dispatch `parse_item()` or `parse_stmt()`
- Wrap in AST_ITEM_PROGRAM root node
- Call `synchronize()` on error

**DELTA-04: Function Parser (1h)**
- `parse_function(state, vis, attrs, is_async) -> Int` — parse fn declarations
- Handles: generic params `<T: Bound>`, params `(name: Type)`, return `-> Type`, effect `with Effect`, where `where T: Bound`, body (indented block)
- AST_ITEM_FUNCTION data encoding: name_idx, attr_idx, generic_count, generic_pairs, param_count, param_indices, return_type_idx, where_clause_idx, effect_count, effect_values, body_idx

**DELTA-05: Struct/Enum/Trait/Impl Parsers (1h)**
- `parse_struct()`, `parse_enum()`, `parse_trait()`, `parse_impl()`
- `parse_type_alias()`, `parse_const()`, `parse_use()`, `parse_mod()`

**DELTA-06: Layer 1-7 Item Parsers (1h)**
- `parse_world()`, `parse_actor()`, `parse_component()`, `parse_shader()`
- `parse_patch()`, `parse_law()`, `parse_converge()`, `parse_orchestrate()`
- `parse_pulse()`, `parse_resonate()`, `parse_axiom()`, `parse_entangle()`
- `parse_shatter_struct()`, `parse_include()`, `parse_import()`, `parse_from_import()`
- These parse the syntax but the typechecker will stub the semantics

**DELTA-07: Statement Parsers (1h)**
- `parse_let()`, `parse_var()`, `parse_return()`, `parse_defer()`
- `parse_for()`, `parse_fanout()`, `parse_while()`, `parse_loop()`
- `parse_break()`, `parse_continue()`

**DELTA-08: Pratt Expression Parser Core (1.5h)**
- `parse_expr(state) -> Int` — entry: parse_assignment → parse_binary(0)
- `parse_binary(state, min_prec) -> Int` — Pratt core loop
- Precedence table mapping TokenKind → (precedence, BinaryOp, associativity)
- 16 levels, left-assoc except ** (right)

**DELTA-09: Unary + Primary Expressions (1h)**
- `parse_unary(state) -> Int` — prefix operators, ownership exprs
- `parse_primary(state) -> Int` — literals, idents, parens, blocks, if-expr, match, struct lit, array, lambda

**DELTA-10: Postfix Expressions (0.5h)**
- `parse_postfix(state, base) -> Int` — call, field, index, ++, --, ?, ?., as

**DELTA-11: Assignment + Special Forms (0.5h)**
- `parse_assignment(state) -> Int` — right-assoc = chaining
- Compound assignment desugaring (a += b → a = a + b)
- Ternary a ? b : c desugaring
- Null coalesce a ?? b desugaring
- Range a..b, a..=b

**DELTA-12: JSX Parser (1h)**
- `parse_jsx_element(state) -> Int`
- Handles `<Component props={expr}>children</Component>`
- Braced expression interpolation `{expr}`
- Tag name matching validation
- Produces AST_EXPR_JSX nodes

**DELTA-13: Generics + Effects Parsing (0.5h)**
- `parse_generic_params(state) -> Array<Int>` — handles `>>` injection
- `parse_where_clause(state) -> Int`
- `parse_effect_annotations(state) -> Array<Int>`

**DELTA-14: Error Recovery + Reserved Keywords (0.5h)**
- `synchronize(state)` — skip to next item boundary
- `RESERVED_KEYWORDS` array — ~174 identifiers that cannot be used as names
- MAX_ERRORS bail at 50

---

### DELTA-15: Test Specification (`spec/parser_spec.md`)

**Effort:** 1h
**Objective:** Write markscript-format test cases for the parser.

Create `X:\blades\kain\spec\parser_spec.md` with test cases covering:
- Every item kind parsed correctly
- Pratt precedence for all 16 levels
- JSX element parsing
- Error recovery on malformed input
- `>>` injection in generics
- Reserved keyword rejection

Format:
```markdown
# Parser Test Specification

## Item Parsing

| Case | Source | Expected AST Kind | Expected Children |
|------|--------|-------------------|-------------------|
| fn simple | `fn foo(): 42` | AST_ITEM_FUNCTION | name="foo", body=Int(42) |
| fn generic | `fn id<T>(x: T) -> T: return x` | AST_ITEM_FUNCTION | generic=T, param=x, ret=T |
...
```

---

## Stream Conventions

- **Language:** Pure Kain Layer 0 (fn, struct, enum, let, while, if, match, return, const)
- **Naming:** snake_case for functions; `parse_*` prefix for all parser functions; `parser_*` for state helpers
- **Imports:** Import ALL needed AST_*, BINOP_*, UNOP_* constants from `ast.kn`
- **Error handling:** Accumulate errors in `DiagnosticBag` via `parser_error(state, msg, kind, span)`. NEVER panic. After each error, try `synchronize()` and continue.
- **AST encoding:** Follow the data[] encoding scheme from the design document exactly. Every AST node kind has a specific data[] layout.
- **Testing:** Test-driven — write test cases first for each item kind, then implement the parser.

---

## Stream Boundary — What You Do NOT Do

- ❌ Do NOT implement typechecking — that's FOXTROT's job
- ❌ Do NOT implement codegen — that's GOLF's job
- ❌ Do NOT modify ALPHA's section of `ast.kn` (above the END marker)
- ❌ Do NOT use recursive types for AST — use flat Array<AstNode> with integer indices
- ❌ Do NOT use Box<T>, Arc<T>, or heap-allocated recursive data structures (Constraint C-6)

---

## Verification (After This Stream)

```bash
# Check individual files
kain check X:\blades\kain\src\ast.kn
kain check X:\blades\kain\src\parser.kn

# Parse a test file
kain run -- -test-parse X:\blades\kain\src\token.kn
```

**Self-check:**
- [ ] `ast.kn` has AstNode struct + all helpers appended correctly BELOW ALPHA's section
- [ ] `parser.kn` compiles with zero type errors
- [ ] Parser produces correct AST for a simple `fn main(): 42` source
- [ ] All 38 item kinds parseable (even if some produce simplified AST for stubbed items)
- [ ] Pratt precedence: `1 + 2 * 3` parses as `(1 + (2 * 3))` not `((1 + 2) * 3)`
- [ ] Error recovery: malformed input doesn't crash
- [ ] Reserved keyword check: using `fn` as identifier emits error

---

## Completion Report

When done, report:
- Files created/modified: ast.kn (appended section), parser.kn, spec/parser_spec.md — with line counts
- AST node kinds: 38 items, 12 stmts, 64 exprs implemented
- Pratt levels: 16 with correct precedence/associativity
- Test coverage: N test cases in parser_spec.md
- Any issues encountered
- Whether FOXTROT can safely start
