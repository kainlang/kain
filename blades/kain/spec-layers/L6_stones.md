# L6 — Machine Stones: axiom + shatter + teleport

**Spec Document for `src/L6_stones.kn`**
**Date:** 2026-06-12
**Self-Host Kainc Compiler**

---

## Table of Contents

1. [Architecture Overview](#1-architecture-overview)
2. [AST Representation](#2-ast-representation)
3. [Rust Bootstrap Reference](#3-rust-bootstrap-reference)
4. [Parser Status](#4-parser-status)
5. [Typechecker Plan](#5-typechecker-plan)
6. [Codegen Plan](#6-codegen-plan)
7. [Runtime Contract](#7-runtime-contract)
8. [Implementation Tasks](#8-implementation-tasks)
9. [Dependencies](#9-dependencies)
10. [Test Plan](#10-test-plan)

---

## 1. Architecture Overview

### 1.1 Three Stones, One Layer

L6 Machine Stones consists of three distinct constructs that together form the **compiler-owned machine abstraction layer**:

| Construct | Nature | What It Does |
|-----------|--------|-------------|
| **axiom** | Top-level declaration | Declares capability assumptions (target, arch, capability) with a fallback function for when assumptions fail |
| **shatter struct** | Struct modifier | Instructs the compiler to use Structure-of-Arrays (SoA) layout instead of Array-of-Structs (AoS) |
| **teleport** | Expression | Performs zero-copy cross-world data handoff — moves a value from one world to another without copying |

### 1.2 The Machine Stones Relationship

```
axiom ──declares──▶ capability assumptions (machine profile)
  │                      │
  │                      ▼
  │              shatter struct ──uses──▶ SoA memory layout
  │                      │                (cache-friendly, SIMD-friendly)
  │                      │
  │                      ▼
  └──gates──▶      teleport ──moves──▶ cross-world handoff
                 (zero-copy, provenance-tracked)
```

Axiom gates the entire machine-dependent path. Shatter provides the data layout. Teleport provides the cross-world handoff.

### 1.3 Decision Ladder Context

| Question | Construct |
|----------|-----------|
| "Capability assumption?" | `axiom` |
| "Hot-data layout (SoA)?" | `shatter struct` |
| "Cross-world zero-copy?" | `teleport` |
| "Just need a struct?" | `struct` (no shatter) |
| "Just need a copy?" | plain assignment |
| "Continuous coupling?" | `entangle` |

---

## 2. AST Representation

### 2.1 Axiom Node Layout

Rust bootstrap (`ast.rs:269-308`):

```rust
pub struct AxiomDef {
    pub name: String,
    pub predicates: Vec<AxiomPredicate>,
    pub guarantees: Vec<String>,
    pub fallback: Option<String>,
    pub visibility: Visibility,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

pub enum AxiomPredicate {
    Target(String),    // when target("llvm")
    Arch(String),      // when arch("x86_64")
    Capability(String), // when capability("memory.shatter")
}
```

Self-host layout for `AST_ITEM_AXIOM` (constant 14):

```
data[0] = name_idx                      (string-table index of axiom name)
data[1] = predicate_count               (total predicates)
for each predicate:
  data[2 + i*2] = predicate_kind        (0=target, 1=arch, 2=capability)
  data[3 + i*2] = predicate_value_idx   (string-table index of the value)
data[N] = guarantee_count               (number of guarantee strings)
for each guarantee:
  data[N+1 + j] = guarantee_string_idx  (string-table index)
data[M] = has_fallback                  (0 or 1)
data[M+1] = fallback_name_idx          (string-table index, only if has_fallback)
data[M+2] = body_idx                    (AST index of the body block, if any)
```

**Current parser state** (parser.kn:2791-2810):
```
parse_axiom_item:
  - parses name token
  - parses colon + body block
  - stores: [name_idx, body_idx]
  
MISSING:
  - when predicate parsing (target/arch/capability)
  - guarantee string parsing
  - fallback function name parsing
  - deduplication of predicates
```

### 2.2 Shatter Struct Node Layout

Shatter struct is **not a separate AST item kind**. It reuses `AST_ITEM_STRUCT` (constant 1) with a `#[shatter]` attribute marker.

Rust bootstrap:
```rust
pub const SHATTER_ATTRIBUTE_NAME: &str = "shatter";  // ast.rs:38-39

impl Struct {
    pub fn is_shattered(&self) -> bool {
        self.attributes.iter().any(|attr| attr.name == SHATTER_ATTRIBUTE_NAME)
    }
}
```

The self-host already follows this pattern. In `parse_shatter_struct` (parser.kn:2990-2995):
```kn
pub fn parse_shatter_struct(st: ParserState, vis_val: Int, attrs: Array<Int>) -> ParseResult:
    let mut cur: ParserState = parser_advance(st)    # advances past 'shatter'
    if parser_check(cur, TOKEN_STRUCT):
        cur = parser_advance(cur)                     # advances past 'struct'
    return parse_struct(cur, vis_val, attrs)           # delegates to normal struct parser
```

The shatter attribute must be pushed into the `attrs` array before delegating. The current parser does NOT push the shatter attribute:

**Current parser state** (needs fix):
```kn
pub fn parse_shatter_struct(st: ParserState, vis_val: Int, attrs: Array<Int>) -> ParseResult:
    let mut cur: ParserState = parser_advance(st)
    # BUG: attrs.push(AST_ATTR_SHATTER) is missing
    
    # FIX: push the attribute before delegating
    # var extended_attrs: Array<Int> = []
    # push_all(extended_attrs, attrs)
    # extended_attrs.push(AST_ATTR_SHATTER)
    # return parse_struct(cur, vis_val, extended_attrs)
    
    if parser_check(cur, TOKEN_STRUCT):
        cur = parser_advance(cur)
    return parse_struct(cur, vis_val, attrs)
```

Without the attribute push, the typechecker/codegen cannot distinguish `shatter struct` from `struct`.

### 2.3 Teleport Expression Layout

Rust bootstrap (`ast.rs:1866`):

```rust
Expr::Teleport {
    value: Box<Expr>,
    source_world: String,
    target_world: String,
    channel: Option<String>,
    span: Span,
}
```

Self-host layout for `AST_EXPR_TELEPORT` (constant 134):

```
data[0] = value_ast_idx            (AST index of the value expression)
data[1] = source_world_name_idx    (string-table index)
data[2] = target_world_name_idx    (string-table index)
data[3] = has_channel              (0 or 1)
data[4] = channel_name_idx         (string-table index, only if has_channel == 1)
```

**Current parser state** — teleport expression is NOT yet parsed in the self-host parser. The `AST_EXPR_TELEPORT` constant exists but there is no `parse_teleport_expr()` function. The expression router (around line 1070 in parser.kn) needs to handle the "teleport" keyword as an expression prefix.

Required additions:
```
parse_teleport_expr(st: ParserState) -> ParseResult:
  1. Advance past "teleport"
  2. Parse value expression (parse_unary)
  3. Expect "from" contextual keyword
  4. Parse source world name (string-like argument)
  5. Expect "to" contextual keyword
  6. Parse target world name (string-like argument)
  7. If "via" keyword follows:
     - Parse channel name (string-like argument)
  8. Build Expr::Teleport with data array
```

---

## 3. Rust Bootstrap Reference

### 3.1 Axiom in the Bootstrap

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/ast.rs` | 269-308 | `AxiomDef`, `AxiomPredicate` (Target, Arch, Capability) |
| `crates/core/src/parser.rs` | 1015 | Axiom dispatch in parse_item |
| `crates/core/src/parser.rs` | 1776-1856 | `parse_axiom()` — parses name, when predicates, guarantee strings, fallback |
| `crates/core/src/types.rs` | 5760-5800 | `check_axiom()` — validates predicates non-empty, guarantees non-empty, fallback present, deduplication |
| `crates/core/src/runtime_contract.rs` | 147-159 | `RuntimeAxiomPredicateContract`, `RuntimeAxiomContract` |
| `crates/core/src/runtime_contract.rs` | 1250-1286 | `collect_axiom_contracts()`, `runtime_axiom_contract()` |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 15101-15160 | `compile_axiom()` — full implementation |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 1641-1659 | `machine_axiom_symbol()`, `machine_axiom_capability_bit()` |
| `runtime/native/include/machine_stones.h` | `kain_machine_axiom_accept()` declaration |
| `runtime/native/src/core/machine_stones.c` | `kain_machine_target_matches()`, `kain_machine_arch_matches()`, `kain_machine_current_capabilities()`, `kain_machine_axiom_accept()` |

### 3.2 Shatter in the Bootstrap

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/parser.rs` | 76 | `"shatter"` in contextual keyword list |
| `crates/core/src/parser.rs` | 707 | `"shatter"` match arm in parse_item |
| `crates/core/src/parser.rs` | 2724-2738 | `parse_shatter_struct()` — pushes `SHATTER_ATTRIBUTE_NAME`, delegates to `parse_struct_with_attrs()` |
| `crates/core/src/ast.rs` | 38-39 | `pub const SHATTER_ATTRIBUTE_NAME: &str = "shatter"` |
| `crates/core/src/ast.rs` | 1180-1186 | `Struct::is_shattered()` |
| `crates/core/src/runtime_contract.rs` | 184-188 | `RuntimeShatterContract` struct |
| `crates/core/src/runtime_contract.rs` | 618-620 | `memory.shatter` capability emission |
| `crates/core/src/runtime_contract.rs` | 1355-1377 | `collect_shatter_contracts()` |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 118-128 | `ShatteredArrayBacking`, `ShatteredArrayLocal` |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 11217-11228 | `emit_shatter_lane_bases()` — runtime lane base |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 11230-11253 | `emit_stack_shatter_lane_bases()` — stack-private alloca per lane |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 11255-11310 | `populate_shattered_array_literal_lanes()` — fill lane buffers |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 11345-11450 | `compile_shattered_field_ptr()` — core field access lowering |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 11552-11580 | `compile_shattered_array_literal()` — core array literal lowering |
| `runtime/native/include/machine_stones.h` | 4 shatter functions declarations |
| `runtime/native/src/core/machine_stones.c` | ~580-650 | Shatter buffer: struct, alloc, lane_ptr, lane_base, free |
| `runtime/native/src/core/z3/proofs/native-machine-shatter-lane-offset-stays-in-payload.yaml` | Z3: lane ptr in bounds |
| `runtime/native/src/core/z3/proofs/native-machine-shatter-lane-base-shift-offset-stays-in-payload.yaml` | Z3: base+shift in bounds |

### 3.3 Teleport in the Bootstrap

| File | Lines | Content |
|------|-------|---------|
| `crates/core/src/ast.rs` | 77 | `"teleport"` keyword constant |
| `crates/core/src/ast.rs` | 1866 | `Expr::Teleport { value, source_world, target_world, channel, span }` |
| `crates/core/src/parser.rs` | 4790 | `parse_teleport_expr()` dispatch |
| `crates/core/src/parser.rs` | 4977-4995 | `parse_teleport_expr()` implementation |
| `crates/core/src/types.rs` | 8943-8961 | `infer_expr_type` for `Expr::Teleport` — world resolution, distinct check, channel check, move marking |
| `crates/core/src/types.rs` | 9076-9092 | `ensure_teleport_world_reference()` |
| `crates/core/src/runtime_contract.rs` | 625-636 | `world.teleport` + `interop.zero-copy-handoff` capability emission |
| `crates/core/src/runtime_contract.rs` | 2341-2356 | `block_contains_teleport_expr` detection |
| `crates/sys-codegen/src/codegen_llvm/mod.rs` | 11574-11633 | `compile_teleport_expr()` |
| `runtime/native/include/machine_stones.h` | 40-49 | `kain_machine_teleport_ptr`, `kain_machine_teleport_note` |
| `runtime/native/src/core/machine_stones.c` | 487-544 | `kain_machine_teleport_ptr()` implementation |
| `runtime/native/src/core/z3/proofs/native-machine-teleport-token-handoff-is-exclusive.yaml` | Z3: post-handoff exclusive |

---

## 4. Parser Status

### 4.1 Axiom Parser Status

**Current** (`parse_axiom_item`, parser.kn:2791-2810):
```kn
pub fn parse_axiom_item(st: ParserState, vis_val: Int, attrs: Array<Int>) -> ParseResult:
    let sp: SpanPair = parser_current_span(st)
    let mut cur: ParserState = parser_advance(st)
    let name_tok: Token = parser_current(cur)
    if name_tok.kind != TOKEN_IDENT: return ParseResult { state: cur, node: -1 }
    cur = parser_advance(cur)
    let name_ir: InternResult = parser_intern(cur, name_tok.text)
    cur = name_ir.state
    let name_idx: Int = name_ir.index
    var body_idx: Int = -1
    if parser_check(cur, TOKEN_COLON):
        cur = parser_advance(cur)
        let br: ParseResult = parse_block_or_expr(cur)
        cur = br.state
        body_idx = br.node
    let end_off: Int = parser_current(cur).byte_offset
    let mut data: Array<Int> = []
    data.push(name_idx)
    data.push(body_idx)
    let node: AstNode = ast_new_node(AST_ITEM_AXIOM, sp.start, end_off, data)
    return parser_push_result(cur, node)
```

**MISSING:**
- No `when` predicate parsing (target, arch, capability)
- No `guarantee` string parsing
- No `fallback` function name parsing
- No predicate deduplication
- Data contains only name + body_idx — no predicate/guarantee/fallback info

**Required additions for `parse_axiom_item`:**

```
1. Parse name (existing)
2. Expect colon (existing)
3. Enter indented block
4. Loop until dedent or EOF:
   a. If token == "when":
      - Advance
      - Parse predicate kind (target/arch/capability) via ident
      - Expect lparen
      - Parse string literal → predicate_value_idx
      - Expect rparen
      - Push (predicate_kind: 0/1/2, value_idx) to data
   b. If token == "guarantee":
      - Advance
      - Parse string literal → guarantee_string_idx
      - Push to data
   c. If token == "fallback":
      - Advance
      - If fallback already set: error "only one fallback allowed"
      - Parse string literal → fallback_name_idx
      - Store fallback
5. Validate: at least one predicate, one guarantee, one fallback
6. Predicate deduplication: store unique (kind, value) pairs only
```

Predicate encoding for data array:
```kn
# predicate_kind encoding
const PRED_TARGET:     Int = 0
const PRED_ARCH:       Int = 1
const PRED_CAPABILITY: Int = 2
```

### 4.2 Shatter Parser Status

**Current** (`parse_shatter_struct`, parser.kn:2990-2995):
```kn
pub fn parse_shatter_struct(st: ParserState, vis_val: Int, attrs: Array<Int>) -> ParseResult:
    let mut cur: ParserState = parser_advance(st)
    if parser_check(cur, TOKEN_STRUCT):
        cur = parser_advance(cur)
    return parse_struct(cur, vis_val, attrs)
```

**BUG: The shatter attribute is NOT pushed into the attrs array.**

The Rust bootstrap does this correctly:
```rust
fn parse_shatter_struct(&mut self, vis: Visibility, mut attrs: Vec<Attribute>) -> KainResult<Item> {
    self.expect_contextual_ident("shatter")?;
    attrs.push(Attribute { name: SHATTER_ATTRIBUTE_NAME.to_string(), ... });
    self.parse_struct_with_attrs(vis, attrs)
}
```

**Fix required:**
```kn
pub fn parse_shatter_struct(st: ParserState, vis_val: Int, attrs: Array<Int>) -> ParseResult:
    let mut cur: ParserState = parser_advance(st)
    # Push shatter attribute marker
    var extended_attrs: Array<Int> = []
    push_all(extended_attrs, attrs)
    extended_attrs.push(AST_ATTR_SHATTER)  # a new constant: AST_ATTR_SHATTER = some unique neg id
    
    if parser_check(cur, TOKEN_STRUCT):
        cur = parser_advance(cur)
    return parse_struct(cur, vis_val, extended_attrs)
```

**Required:** Define the shatter attribute constant in ast.kn:
```kn
pub const AST_ATTR_SHATTER: Int = -1000  # negative to avoid collision with AST node kinds
```

Or alternatively, store it as a string-indexed attribute (more in line with the Rust bootstrap):
```kn
# In parse_shatter_struct:
# let shatter_name_idx: Int = ... ("shatter" in string table)
# extended_attrs.push(shatter_name_idx)
```

### 4.3 Teleport Parser Status

**Current:** No teleport expression parser exists.

The teleport expression keyword must be recognized in the expression parsing prefix. In the Rust bootstrap, `parse_teleport_expr` is dispatched from the postfix expression parser (parser.rs:4790) because `teleport` starts a unary-like expression, not a prefix.

**Required additions:**

1. In the expression dispatch table (where prefix/postfix expressions are routed), add a match for the "teleport" keyword
2. Implement `parse_teleport_expr(st: ParserState) -> ParseResult`:

```kn
pub fn parse_teleport_expr(st: ParserState) -> ParseResult:
    let sp: SpanPair = parser_current_span(st)
    let mut cur: ParserState = parser_advance(st)  # advance past "teleport"
    
    # Step 1: Parse the value expression (the thing being teleported)
    let val_ir: ParseResult = parse_unary(cur)
    cur = val_ir.state
    let value_idx: Int = val_ir.node
    if value_idx < 0:
        return ParseResult { state: cur, node: -1 }
    
    # Step 2: Expect "from"
    if parser_contextual_keyword(cur, "from") == false:
        return ParseResult { state: cur, node: -1 }
    cur = parser_advance(cur)
    
    # Step 3: Parse source world name (identifier, not string literal in self-host)
    let src_tok: Token = parser_current(cur)
    if src_tok.kind != TOKEN_IDENT:
        return ParseResult { state: cur, node: -1 }
    let src_ir: InternResult = parser_intern(cur, src_tok.text)
    cur = src_ir.state
    let src_idx: Int = src_ir.index
    
    # Step 4: Expect "to"
    if parser_contextual_keyword(cur, "to") == false:
        return ParseResult { state: cur, node: -1 }
    cur = parser_advance(cur)
    
    # Step 5: Parse target world name
    let tgt_tok: Token = parser_current(cur)
    if tgt_tok.kind != TOKEN_IDENT:
        return ParseResult { state: cur, node: -1 }
    let tgt_ir: InternResult = parser_intern(cur, tgt_tok.text)
    cur = tgt_ir.state
    let tgt_idx: Int = tgt_ir.index
    
    # Step 6: Optional "via" channel
    var has_channel: Int = 0
    var channel_idx: Int = -1
    if parser_contextual_keyword(cur, "via"):
        cur = parser_advance(cur)
        let ch_tok: Token = parser_current(cur)
        if ch_tok.kind == TOKEN_IDENT:
            let ch_ir: InternResult = parser_intern(cur, ch_tok.text)
            cur = ch_ir.state
            channel_idx = ch_ir.index
            has_channel = 1
    
    # Step 7: Build data and node
    let end_off: Int = parser_current(cur).byte_offset
    var data: Array<Int> = []
    data.push(value_idx)
    data.push(src_idx)
    data.push(tgt_idx)
    data.push(has_channel)
    if has_channel == 1:
        data.push(channel_idx)
    let node: AstNode = ast_new_node(AST_EXPR_TELEPORT, sp.start, end_off, data)
    return parser_push_result(cur, node)
```

### 4.4 Contextual Keyword Detection

Both axiom and teleport use contextual keywords. The parser needs a helper:

```kn
fn parser_contextual_keyword(st: ParserState, keyword: String) -> Bool:
    let tok: Token = parser_current(st)
    return tok.kind == TOKEN_IDENT and tok.text == keyword
```

This is already used elsewhere in the parser. The specific keywords needed:

| Construct | Keywords to Recognize |
|-----------|----------------------|
| axiom | `when`, `target`, `arch`, `capability`, `guarantee`, `fallback` |
| teleport | `from`, `to`, `via` |
| pulse | `every`, `jitter` |
| resonate | `dampen` |

---

## 5. Typechecker Plan

### 5.1 Axiom Typechecker

**Current stub** (`check_axiom_stub`, types.kn:1638-1645):
```kn
pub fn check_axiom_stub(env: TypeEnv, node: AstNode, idx: Int) -> TypedItemAndEnv:
    let name_idx: Int = if ast_data_len(node) > 0: ast_data_get(node, 0) else: -1
    return TypedItemAndEnv {
        env: env,
        item: TypedItem {
            kind: AST_ITEM_AXIOM, name: "ax_" + str(name_idx), name_idx: name_idx,
            resolved_type: rt_unit(), ast_index: idx, effects: EFF_PURE,
        }
    }
```

**Required validations:**

1. **Predicates non-empty** — must have at least one `when` predicate
2. **Guarantees non-empty** — must have at least one guarantee string
3. **Fallback present** — must have a non-empty fallback function name
4. **Predicate deduplication** — no duplicate (kind, value) pairs
5. **Predicate kind validation** — kind must be 0 (target), 1 (arch), or 2 (capability)
6. **Fallback function existence** — optional: validate the fallback function name resolves to a declared function (at minimum, store the name for codegen)

Diagnostic messages (matching the Rust bootstrap):
```kn
"axiom '{}' must declare at least one machine predicate"
"axiom '{}' must declare at least one guarantee"
"axiom '{}' must declare a portable fallback so unsupported machines stay sound"
"axiom '{}' repeats predicate {} ({})"
```

### 5.2 Shatter Struct Typechecker

Shatter structs should be typechecked as regular structs. The key addition is that the typechecker must preserve the `#[shatter]` attribute through the `TypedItem` so the codegen can identify which structs are shattered.

**Current:** `parse_shatter_struct` delegates to `parse_struct`, which produces a normal `AST_ITEM_STRUCT`. The typechecker processes it through the normal struct path.

**Required:**
1. After fixing the parser to push the shatter attribute, the typechecker reads the attribute and preserves it in the output `TypedItem`
2. No special shatter-specific validation is needed at typecheck time — the codegen handles all layout decisions
3. The `TypedItem.name` should include a marker (e.g., prefix "shatter_") so downstream consumers can identify shattered structs

### 5.3 Teleport Typechecker

**Current stub** (types.kn:1975-1976):
```kn
elif k == AST_EXPR_SHARE or k == AST_EXPR_TELEPORT:
    return rt_unit()
```

**Required validations:**

1. **Value expression typecheck** — infer the type of the value being teleported
2. **Source world exists** — validate that `source_world` resolves to a declared world
3. **Target world exists** — validate that `target_world` resolves to a declared world
4. **Distinct worlds** — source and target must be different worlds
5. **Channel non-empty** — if `via` is provided, channel name must be non-empty
6. **Move semantic marking** — if the value is a simple identifier, mark it as moved in the scope (prevent post-teleport use)
7. **Return type** — the type of the teleport expression is the same as the value expression's type

```kn
fn infer_teleport_type(env: TypeEnv, data: Array<Int>, ctx: TypeCheckCtx) -> ResolvedType:
    let value_idx: Int = data[0]
    let src_name_idx: Int = data[1]
    let tgt_name_idx: Int = data[2]
    
    # Infer value type
    let value_type: ResolvedType = infer_expr_from_node(env, value_idx, copy(ctx))
    
    # Validate worlds exist (simplified — needs world table in env)
    # env.world_exists(src_name_idx) must be true
    # env.world_exists(tgt_name_idx) must be true
    
    # Validate distinct worlds
    # src_name_idx != tgt_name_idx
    
    # Validate channel if present
    if data[3] == 1:
        let ch_idx: Int = data[4]
        # ch_idx must be non-empty
    
    # Mark source identifier as moved if value is an identifier
    # env.mark_moved(value_idx)
    
    return value_type  # teleport preserves the type
```

The world existence check requires the typechecker environment to maintain a table of declared world names. This is already partially done by the world typechecking pass — worlds are registered by name when `check_world_stub` processes them.

### 5.4 Move Semantics Implementation

When a teleport expression moves an identifier, subsequent uses must produce errors:

```kn
fn check_teleport_move(env: TypeEnv, value_node: AstNode):
    let kind: Int = value_node.kind
    if kind == AST_EXPR_IDENT:
        let name_idx: Int = value_node.data[0] if ast_data_len(value_node) > 0 else -1
        if name_idx >= 0:
            env.mark_moved(name_idx)
```

The `mark_moved` function adds the name to a `moved` set in the environment. `resolve_ident` checks the `moved` set before returning a type, producing:
```kn
"Identifier '{}' was moved by teleport and cannot be used again"
```

---

## 6. Codegen Plan

### 6.1 Axiom Codegen

Each axiom compiles to one LLVM function:

```
define i64 @__kain_axiom_accept_<sanitized_name>() {
entry:
  %0 = call i64 @kain_machine_axiom_accept(
    i8* @".static.string.<target>",
    i8* @".static.string.<arch>",
    i64 <capability_bitmask>
  )
  ret i64 %0
}
```

**Self-host implementation:**

```kn
fn compile_axiom_textual(axiom_node: AstNode) -> String:
    var output: String = ""
    let name_idx: Int = ast_data_get(axiom_node, 0)
    let name: String = resolve_string(name_idx)
    
    # Collect predicates
    var target_str: String = ""
    var arch_str: String = ""
    var cap_mask: Int = 0
    
    let pred_count: Int = ast_data_get(axiom_node, 1)
    var pi: Int = 0
    var data_pos: Int = 2
    while pi < pred_count:
        let kind: Int = ast_data_get(axiom_node, data_pos)
        let val_idx: Int = ast_data_get(axiom_node, data_pos + 1)
        let val_str: String = resolve_string(val_idx)
        
        if kind == 0:  # target
            target_str = val_str
        elif kind == 1:  # arch
            arch_str = val_str
        elif kind == 2:  # capability
            cap_mask = cap_mask | capability_bit(val_str)
        
        data_pos = data_pos + 2
        pi = pi + 1
    
    # Emit LLVM function
    let sym: String = "kain_axiom_accept_" + sanitize(name)
    output = output + "define i64 @" + sym + "() {\n"
    output = output + "entry:\n"
    output = output + "  %0 = call i64 @kain_machine_axiom_accept(\n"
    output = output + "    i8* @\".str." + target_str + "\",\n"
    output = output + "    i8* @\".str." + arch_str + "\",\n"
    output = output + "    i64 " + str(cap_mask) + "\n"
    output = output + "  )\n"
    output = output + "  ret i64 %0\n"
    output = output + "}\n\n"
    return output
```

The `capability_bit` mapping:
```kn
fn capability_bit(name: String) -> Int:
    if name == "atomic.bitmask":       return 0x00000001
    elif name == "time.pulse":         return 0x00000002
    elif name == "memory.shatter":     return 0x00000004
    elif name == "world.teleport":     return 0x00000008
    elif name == "cpu.x86.sse2":       return 0x00000010
    elif name == "cpu.x86.avx":        return 0x00000020
    elif name == "cpu.x86.avx2":       return 0x00000040
    elif name == "cpu.x86.avx512f":    return 0x00000080
    else:                              return 0  # unknown capability
```

### 6.2 Shatter Codegen

Shatter struct codegen involves several interrelated pieces:

**Step 1: Track shattered structs**

During codegen initialization, scan all typed items for shattered structs:
```kn
var shattered_structs: Array<String> = []
# For each typed item:
# if item.kind == AST_ITEM_STRUCT and has_shatter_attr(item):
#     shattered_structs.push(item.name)
```

**Step 2: Array literal allocation**

When an array literal of a shattered struct is encountered:

1. Compute lane_count = number of struct fields
2. Compute element_count = number of array elements
3. Emit `kain_machine_shatter_alloc(lane_count, element_count)`
4. For each lane, call `kain_machine_shatter_lane_base(handle, lane_index)`
5. Populate each lane by storing field values at the correct element offsets

**Step 3: Field access lowering**

When `array[index].field` is accessed and the array is shattered:

1. Check if the variable is a `shattered_array_local`
2. Look up the field index in the struct's field list
3. Get the lane base pointer for that field index
4. If index is a compile-time constant (proven in-bounds):
   - Compute `byte_offset = index * 8`
   - Use `getelementptr inbounds` on lane base
   - Bitcast to field type
5. Otherwise (runtime index):
   - Call `kain_machine_shatter_lane_ptr(handle, field_index, element_index)`

**Step 4: Scope cleanup**

For heap-backed shatter allocations, emit `kain_machine_shatter_free(handle)` when the variable goes out of scope.

**jit_cache.kn Reference Pattern**

The existing `blades/kain/src/jit_cache.kn` uses `shatter struct CacheStore` with functional style (value-in, value-out). This pattern gives us confidence the SoA layout works for practical use:

```kn
shatter struct CacheStore:
    hashes:   Array<Int>
    ptrs:     Array<ptr<Byte>>
    sizes:    Array<Int>
    count:    Int
    hits:     Int
    misses:   Int
    bytes:    Int
    compiles: Int
```

This struct has 8 fields — a mix of `Array<Int>`, `Array<ptr<Byte>>`, and scalar `Int`. The SoA layout separates each field into its own contiguous lane, which makes linear scan loops cache-friendly:

```kn
pub fn cache_store_lookup(cache: CacheStore, hash: Int) -> ptr<Byte>:
    var i: Int = 0
    while i < cache.count:
        if cache.hashes[i] == hash:
            return cache.ptrs[i]
        i = i + 1
    return int_to_ptr(0, "ptr<Byte>")
```

Only the `hashes` lane and `ptrs` lane are accessed during lookup — the other 6 lanes stay cold in cache. This is the canonical SoA advantage.

### 6.3 Teleport Codegen

Teleport expressions must compile to LLVM IR that calls the runtime teleport functions.

**For pointer types** (structs, boxed values, heap-allocated):

```llvm
; teleport shard from Authority to Mirror via shard_bus
%raw_ptr = bitcast %ShardStruct* %shard to i8*
%handed_off = call i8* @kain_machine_teleport_ptr(
    i8* %raw_ptr,
    i8* @".str.Authority",
    i8* @".str.Mirror",
    i8* @".str.shard_bus"
)
%result = bitcast i8* %handed_off to %ShardStruct*
```

**For scalar types** (Int, Bool, etc.):

```llvm
; teleport 42 from Authority to Mirror
call void @kain_machine_teleport_note(
    i8* @".str.Authority",
    i8* @".str.Mirror",
    i8* @".str."
)
; Value is unchanged (teleport for scalars is a semantic no-op)
```

**Self-host implementation outline:**

```kn
fn compile_teleport_expr_textual(node: AstNode, ctx: CodegenCtx) -> String:
    let data: Array<Int> = node.data
    let value_idx: Int = data[0]
    let src_idx: Int = data[1]
    let tgt_idx: Int = data[2]
    let src_name: String = resolve_string(src_idx)
    let tgt_name: String = resolve_string(tgt_idx)
    
    # Compile the value expression
    let value_ir: String = compile_expr_textual(value_idx, ctx)
    let value_reg: String = ctx.alloc_register()
    
    var output: String = ""
    output = output + value_ir
    output = output + "  " + value_reg + " = ...\n"  # value is in a register
    
    # Determine if value is pointer type
    let is_ptr: Bool = is_pointer_type(ctx.get_type(value_idx))
    
    if is_ptr:
        output = output + "  %raw_ptr = bitcast " + value_ir_type + " " + value_reg + " to i8*\n"
        output = output + "  %handoff = call i8* @kain_machine_teleport_ptr(\n"
        output = output + "    i8* %raw_ptr,\n"
        output = output + "    i8* @\".str." + src_name + "\",\n"
        output = output + "    i8* @\".str." + tgt_name + "\",\n"
        output = output + "    i8* @\".str." + channel_str + "\"\n"
        output = output + "  )\n"
        output = output + "  %result = bitcast i8* %handoff to " + value_ir_type + "\n"
    else:
        output = output + "  call void @kain_machine_teleport_note(\n"
        output = output + "    i8* @\".str." + src_name + "\",\n"
        output = output + "    i8* @\".str." + tgt_name + "\",\n"
        output = output + "    i8* @\".str." + channel_str + "\"\n"
        output = output + "  )\n"
        output = output + "  %result = " + value_reg + "\n"  # value unchanged
    
    return output
```

---

## 7. Runtime Contract

### 7.1 Axiom Runtime Functions

Declared in `runtime.kn` machine stones section (line 215-216):

```kn
push(funcs, rtf("kain_machine_axiom_accept", "i64", ["i8*", "i8*", "i64"], RT_MACHINE))
push(funcs, rtf("kain_machine_axiom_check", "i1", ["i8*"], RT_MACHINE))
```

| Function | Signature | Purpose |
|----------|-----------|---------|
| `kain_machine_axiom_accept` | `i64(i8* target, i8* arch, i64 capability_mask)` | Check if machine matches all predicates; returns 1=accept, 0=reject |
| `kain_machine_axiom_check` | `i1(i8* name)` | Check a named axiom (alt path) |

### 7.2 Shatter Runtime Functions

Declared in `runtime.kn` machine stones section (line 225-228):

```kn
push(funcs, rtf("kain_machine_shatter_alloc", "i8*", ["i64", "i64"], RT_MACHINE))
push(funcs, rtf("kain_machine_shatter_lane_ptr", "i8*", ["i8*", "i64", "i64"], RT_MACHINE))
push(funcs, rtf("kain_machine_shatter_lane_base", "i8*", ["i8*", "i64"], RT_MACHINE))
push(funcs, rtf("kain_machine_shatter_free", "void", ["i8*"], RT_MACHINE))
```

| Function | Signature | Purpose |
|----------|-----------|---------|
| `kain_machine_shatter_alloc` | `i8*(i64 lane_count, i64 element_count)` | Allocate contiguous SoA buffer |
| `kain_machine_shatter_lane_ptr` | `i8*(i8* handle, i64 lane_index, i64 element_index)` | Get pointer to specific element in specific lane |
| `kain_machine_shatter_lane_base` | `i8*(i8* handle, i64 lane_index)` | Get pointer to start of a lane (element 0) |
| `kain_machine_shatter_free` | `void(i8* handle)` | Free SoA buffer |

Native C `KainMachineShatterBuffer` layout:
```c
typedef struct KainMachineShatterBuffer {
    uint64_t lane_count;       // number of fields (lanes)
    uint64_t element_count;    // number of elements per lane
    uint64_t payload_bytes;    // total data bytes: lane_count * element_count * 8
    unsigned char data[];      // flexible array member
} KainMachineShatterBuffer;
```

Memory layout per element: `data: [lane0_elem0..elemN] [lane1_elem0..elemN] ...`
Each slot is 8 bytes (i64 width). All fields occupy 1 slot.

### 7.3 Teleport Runtime Functions

Declared in `runtime.kn` machine stones section (line 222-224):

```kn
push(funcs, rtf("kain_machine_teleport_ptr", "i8*", ["i8*", "i8*", "i8*", "i8*"], RT_MACHINE))
push(funcs, rtf("kain_machine_teleport_note", "void", ["i8*", "i8*", "i8*"], RT_MACHINE))
push(funcs, rtf("kain_machine_teleport_count", "i64", [], RT_MACHINE))
```

| Function | Signature | Purpose |
|----------|-----------|---------|
| `kain_machine_teleport_ptr` | `i8*(i8* ptr, i8* src, i8* tgt, i8* channel)` | Zero-copy pointer handoff |
| `kain_machine_teleport_note` | `void(i8* src, i8* tgt, i8* channel)` | Bookkeeping-only handoff (scalars) |
| `kain_machine_teleport_count` | `i64()` | Total teleports since runtime start |

### 7.4 Capabilities Emitted

| Condition | Capability | Description |
|-----------|------------|-------------|
| Any axiom exists | `machine.axiom` | "Program declares compiler-owned machine truth axioms." |
| Any shatter struct exists | `memory.shatter` | "Program declares shattered SoA structs." |
| Any teleport expression exists | `world.teleport` | "Program uses destructive zero-copy ownership handoff across worlds." |
| Any teleport expression exists | `interop.zero-copy-handoff` | "Teleport expressions require no-copy destination ownership materialization." |

### 7.5 Contract Metadata Structures

**Axiom contract:**
```kn
struct RuntimeAxiomContract:
    name: String
    predicates: Array<RuntimeAxiomPredicateContract>
    guarantees: Array<String>
    fallback: String

struct RuntimeAxiomPredicateContract:
    kind: String   # "target", "arch", "capability"
    value: String  # e.g. "llvm", "x86_64", "memory.shatter"
```

**Shatter contract:**
```kn
struct RuntimeShatterContract:
    name: String
    layout: String          # always "structure-of-arrays"
    field_lanes: Array<String>  # ordered list of field names
```

---

## 8. Implementation Tasks

### 8.1 Parser Tasks

| # | Task | File | Priority |
|---|------|------|----------|
| P1 | Add `AST_ATTR_SHATTER` constant to ast.kn | ast.kn | HIGH |
| P2 | Fix `parse_shatter_struct` to push the shatter attribute into attrs array before delegating to parse_struct | parser.kn | CRITICAL |
| P3 | Extend `parse_axiom_item` with when target/arch/capability parsing | parser.kn | HIGH |
| P4 | Extend `parse_axiom_item` with guarantee string + fallback name parsing | parser.kn | HIGH |
| P5 | Implement `parse_teleport_expr` with from/to/via parsing | parser.kn | HIGH |
| P6 | Add teleport keyword dispatch in expression parser router | parser.kn | HIGH |
| P7 | Add `parser_contextual_keyword(st, keyword) -> Bool` helper if not already present | parser.kn | MEDIUM |
| P8 | Predicate deduplication during axiom parsing | parser.kn | MEDIUM |

### 8.2 Typechecker Tasks

| # | Task | File | Priority |
|---|------|------|----------|
| T1 | Replace `check_axiom_stub` with full axiom validation: predicates, guarantees, fallback | types.kn | HIGH |
| T2 | Add predicate non-empty check: "must declare at least one machine predicate" | types.kn | HIGH |
| T3 | Add guarantee non-empty check: "must declare at least one guarantee" | types.kn | HIGH |
| T4 | Add fallback presence check: "must declare a portable fallback" | types.kn | HIGH |
| T5 | Shatter attribute preservation: ensure shattered structs carry marker through to TypedItem | types.kn | MEDIUM |
| T6 | Replace teleport `rt_unit()` stub with full type inference | types.kn | HIGH |
| T7 | World existence check for teleport source/target | types.kn | HIGH |
| T8 | Distinct worlds check for teleport | types.kn | HIGH |
| T9 | Channel non-empty validation for teleport | types.kn | MEDIUM |
| T10 | Move semantic marking for teleported identifiers | types.kn | HIGH |
| T11 | World table maintenance in typechecker environment | types.kn | MEDIUM |

### 8.3 Codegen Tasks

| # | Task | File | Priority |
|---|------|------|----------|
| C1 | Emit `__kain_axiom_accept_<name>()` LLVM function for each axiom | codegen.kn | HIGH |
| C2 | Implement `capability_bit(name: String) -> Int` mapping | codegen.kn | HIGH |
| C3 | Track shattered structs during codegen initialization | codegen.kn | HIGH |
| C4 | Emit `kain_machine_shatter_alloc` + lane bases for shattered array literals | codegen.kn | HIGH |
| C5 | Emit shattered field access: compile-time proven index → lane_base + offset | codegen.kn | HIGH |
| C6 | Emit `kain_machine_shatter_lane_ptr` call for runtime-indexed access | codegen.kn | HIGH |
| C7 | Emit `kain_machine_shatter_free` on scope exit for heap-backed allocations | codegen.kn | MEDIUM |
| C8 | Stack-shatter safety analysis: `expr_is_safe_stack_shatter_use` | codegen.kn | MEDIUM |
| C9 | Emit `kain_machine_teleport_ptr` for pointer-type teleport values | codegen.kn | HIGH |
| C10 | Emit `kain_machine_teleport_note` for scalar teleport values | codegen.kn | HIGH |
| C11 | Emit static string constants for world names and channels | codegen.kn | MEDIUM |

---

## 9. Dependencies

### 9.1 Axiom → No Direct Dependencies

Axiom is a standalone machine truth declaration. It does not depend on worlds, entangle, or any other L1-L7 construct. However, `orchestrate` (L4) depends on axiom for its `guarded by` clause, so axiom must be functional before orchestrate codegen can use it.

### 9.2 Shatter Struct → L0 (struct)

Shatter struct is a modifier on the standard struct declaration. It depends on:
- Working struct parsing and typechecking (already implemented)
- The AST_ITEM_STRUCT codegen path (already implemented)
- The attribute system (needs the `AST_ATTR_SHATTER` marker)

Shatter does not depend on worlds or any L1+ construct. The SoA allocation is a pure memory layout decision that applies to any array of struct values.

### 9.3 Teleport → L1 (world)

Teleport depends on worlds for source and target:
- World name resolution (worlds must be declared before use)
- World field access (teleported values may interact with world state)
- Entangle compatibility (teleport is an alternative to entangle for cross-world data movement)

Teleport does NOT depend on axiom (it has its own `world.teleport` capability) or shatter (it works on any type).

### 9.4 Cross-Dependencies Summary

| Construct | Depends On | Required For |
|-----------|-----------|--------------|
| axiom | Nothing standalone | Orchestrate `guarded by` clause |
| shatter struct | L0 `struct`, attribute system | Array literals with SoA layout |
| teleport | L1 `world` resolution | Cross-world handoff |
| shatter + teleport | Both independent | Joint usage in pulse bodies (jit_cache.kn pattern) |

### 9.5 W^X and Memory Safety for Teleport

From `SYSTEMS_PROGRAMMING.MD` and `metal.kn`:

The systems programming surface provides virtual memory primitives that are relevant to teleport safety:

| Primitive | Function | Relevance to Teleport |
|-----------|----------|----------------------|
| `vm_protect_none` | Remove access | Safety: can protect pages after teleport |
| `vm_protect_read_write` | RW access | Teleport destination preparation |
| `vm_protect_execute_read_write` | RWX access | JIT code teleport (jit_cache.kn) |
| `vm_lock` | Lock pages in RAM | Pin cross-world buffers |
| `os_mprotect` | Change page protections | Portable page permission control |
| `os_make_rwx / os_make_rx` | JIT page permissions | jit_cache.kn uses shatter struct for JIT code cache |
| `sfence / mfence` | Store/memory barriers | Ensure teleport writes are visible after handoff |

The `jit_cache.kn` file already uses `shatter struct CacheStore` to store JIT-compiled function pointers (hashes, ptrs, sizes). This is a real-world pattern where teleport could be used to hand off JIT-compiled code between worlds (e.g., from a compiler world to an execution world), requiring W^X memory management via `vm_protect_execute_read_write`.

---

## 10. Test Plan

### 10.1 Unit Tests (kain check)

| Test | Description | Expect |
|------|-------------|--------|
| `axiom_minimal` | `axiom x: when target("llvm") guarantee "t" fallback f` | Types valid |
| `axiom_all_predicates` | target + arch + capability combined | Types valid |
| `axiom_no_predicates` | `axiom x: guarantee "t" fallback f` (no when) | Type error |
| `axiom_no_guarantees` | `axiom x: when target("llvm") fallback f` | Type error |
| `axiom_no_fallback` | `axiom x: when target("llvm") guarantee "t"` | Type error |
| `axiom_duplicate_predicate` | Two `when target("llvm")` | Type error |
| `shatter_struct_minimal` | `shatter struct X: a: Int` | Types valid |
| `shatter_struct_fields` | Multi-field shatter struct | Types valid |
| `shatter_struct_bool_field` | `shatter struct X: alive: Bool` | Types valid |
| `shatter_struct_array` | Array literal of shattered struct | Types valid |
| `teleport_minimal` | `teleport x from A to B` | Types valid (A, B are worlds) |
| `teleport_with_channel` | `teleport x from A to B via c` | Types valid |
| `teleport_bad_source` | `teleport x from Nonexistent to B` | Type error (no such world) |
| `teleport_same_world` | `teleport x from A to A` | Type error (must be distinct) |
| `teleport_empty_channel` | `teleport x from A to B via ""` | Type error (empty channel) |
| `teleport_moved_use` | `let v = ...; teleport v from A to B; let y = v` | Type error (moved) |

### 10.2 Compilation Tests (kain build --target llvm)

| Test | Description |
|------|-------------|
| `axiom_compile` | Single axiom emits `__kain_axiom_accept_<name>()` function |
| `axiom_multi` | Multiple axioms all emit accept functions |
| `axiom_capability_bitmask` | Verify capability mask ORs correctly |
| `shatter_compile` | Shatter array literal allocates and populates lanes |
| `shatter_field_access` | Field access on shattered array compiles to lane_base + offset |
| `shatter_multi_field` | Multiple field accesses in same function |
| `teleport_compile_ptr` | Teleport of struct emits `kain_machine_teleport_ptr` call |
| `teleport_compile_scalar` | Teleport of Int emits `kain_machine_teleport_note` call |
| `shatter_teleport_fusion` | Shattered struct teleported cross-world |

### 10.3 Runtime Tests (kain run --target llvm)

| Test | Description | Verification |
|------|-------------|-------------|
| `axiom_accepts` | Axiom on matching target returns 1 | `__kain_axiom_accept_<name>()` returns 1 |
| `axiom_rejects` | Axiom with impossible capability returns 0 | Returns 0 |
| `shatter_values` | Shatter struct array retains field values after SoA layout | Values match struct literals |
| `shatter_mutate` | Write to shattered field, read back | Written value is read correctly |
| `shatter_loop` | Loop over shattered array, accumulate field values | Correct checksum |
| `teleport_count` | Execute teleport N times | `runtime_machine_teleport_count()` increments by N |
| `teleport_integrity` | Teleport struct with known values | Moved value fields match originals |
| `teleport_moved_compile_error` | Post-teleport read of source identifier | Compile error (not runtime) |

### 10.4 Integration Tests

| File | Constructs | What It Tests |
|------|-----------|---------------|
| `blades/kain/src/jit_cache.kn` | `shatter struct CacheStore` | SoA cache with linear scan over `hashes` lane, 8 fields, functional pattern |
| `blades/test/machine-stones/src/main.kn` | axiom + pulse + shatter + teleport + world | Full 4-stone fusion with runtime telemetry checks |
| `benchmark/cases_v2/fusion_chain.kn` | world + entangle + resonate + patch + law + converge + orchestrate + actor + shatter + teleport + pulse + collapse/observe/decay | Full 7-layer causal chain: teleport in actor handler, shatter struct as teleport payload, pulse driving writes |
| `benchmark/cases_v2/keyword_crucible.kn` | axiom (~line 263), shatter struct (~line 68), teleport (~line 395) among 108 keywords | Keyword stress test with all constructs in one file |
| `smoketest/src/semantics/axiom.kn` | axiom + fallback function | Minimal axiom proof |
| `smoketest/src/semantics/shatter.kn` | shatter struct + cross-file scoring | Minimal SoA struct with cross-file function calls |
| `smoketest/src/semantics/teleport.kn` | teleport + telemetry check | Minimal teleport with integrity verification |
| `benchmark/cases/machine_stones_shatter_loop/main.kn` | shatter struct + 500K iteration loop | Shatter SoA performance under hot loop |
| `benchmark/cases/pulse_teleport_decay_mesh/main.kn` | pulse + shatter + teleport + world + entangle + ownership | 54K iterations, 13+ semantics, teleport every iteration |

### 10.5 Reference: fusion_chain.kn Teleport + Shatter

From `benchmark/cases_v2/fusion_chain.kn` — the canonical L6 usage:

```kn
// Shatter struct — zero-copy payload shape (line 88-94)
shatter struct FusionShard:
    bias: Int
    phase: Int
    tick: Int
    checksum: Int
    alive: Bool

// Teleport inside actor handler (line 305-306)
// ZERO-COPY LAYER: teleport from inside actor context
let moved = teleport shard from FusionAuthority to FusionMirror via fusion_shard_bus

// Teleport integrity proof (line 308-311)
let score_after = fusion_shard_score(moved)
if score_before != score_after:
    send reply_to.Reply(value = fusion_pack(-99, -99))
    return
```

This is the canonical pattern: build shattered struct → teleport cross-world → verify integrity on the other side.

### 10.6 Reference: jit_cache.kn Shatter Usage

From `blades/kain/src/jit_cache.kn` — this file already uses `shatter struct` for the self-host compiler's JIT compilation cache:

```kn
shatter struct CacheStore:
    hashes:   Array<Int>
    ptrs:     Array<ptr<Byte>>
    sizes:    Array<Int>
    count:    Int
    hits:     Int
    misses:   Int
    bytes:    Int
    compiles: Int
```

This demonstrates the SoA layout for a real compiler internal. The linear lookup over `cache.hashes[i]` only touches the `hashes` lane, leaving `ptrs`, `sizes`, and the scalar fields cold in cache. This is the canonical SoA advantage.

This file also proves that the self-host compiler already depends on shatter struct working correctly — so the L6 typechecker/codegen implementation for shatter has real, immediate users.
