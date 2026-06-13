# Stream BLUE: Codegen Completion

**Stream ID:** BLUE
**Role:** Complete all expression lowering, control flow codegen, runtime function declares, and string ABI marshaling. Make the codegen produce real LLVM IR for all Layer 0 constructs.
**Effort:** 2-3 weeks
**Depends On:** Stream RED (typechecker must produce correct TypedItems with field maps and variant info)
**Requirements Covered:** FR-codegen-expr, FR-codegen-cf, FR-codegen-fn, FR-codegen-struct, FR-codegen-runtime, FR-codegen-string
**Design Reference:** codegen.kn — Path A (textual .ll), Path B stubs (LLVM-C API)

---

## Context

The codegen is at ~70% real for Layer 0. Expression lowering handles 17 of 30+ expression kinds: literals (Int, Float, Bool, None, String), ident, binary, unary, if, block, call, struct lit, field access, assign, ref, deref, cast, paren, while loop, return. The `compile_function_textual` properly walks the AST, locates body_idx, and delegates to `compile_block_textual`.

What's MISSING:
- Match expression codegen (tag dispatch + switch/br + phi merge)
- For-range loop codegen
- Loop/break/continue codegen
- Array literal codegen
- Enum variant construction codegen
- Lambda/closure codegen
- Method call codegen (vtable dispatch or direct)
- String ABI marshaling (fat pointer `{i8*, i64}` lowering)
- Runtime function declares (200+ `declare` statements)
- Struct definition emission (currently `type opaque` for everything)
- Const global value emission (currently `zeroinitializer` everywhere)
- Spawn/send expression codegen (actor integration)
- And/or logical short-circuit codegen
- Index expression codegen (`arr[i]`)
- Tuple expression codegen

This stream completes ALL of these for Layer 0. L1-L7 codegen (world, actor, converge, orchestrate, GPU, etc.) is deferred to GOLD.

---

## Files You Own

### Files to Modify

| File | Region/Function | Change Description |
|------|-----------------|--------------------|
| `X:/blades/kain/src/codegen.kn` | `emit_struct_defs_from_program` (line 586-612) | Emit REAL struct types with field types (not `type opaque`) |
| `X:/blades/kain/src/codegen.kn` | `compile_expr_textual` (line 856-990) | Add dispatch for remaining 13+ expression kinds |
| `X:/blades/kain/src/codegen.kn` | NEW: `compile_match_textual` | Match expression: tag dispatch + phi merge |
| `X:/blades/kain/src/codegen.kn` | NEW: `compile_for_textual` | For-range loop codegen |
| `X:/blades/kain/src/codegen.kn` | NEW: `compile_array_lit_textual` | Array literal: alloca + gep + store chain |
| `X:/blades/kain/src/codegen.kn` | NEW: `compile_enum_variant_textual` | Enum variant construction |
| `X:/blades/kain/src/codegen.kn` | NEW: `compile_lambda_textual` | Lambda: closure struct + function pointer stub |
| `X:/blades/kain/src/codegen.kn` | NEW: `compile_method_call_textual` | Method calls |
| `X:/blades/kain/src/codegen.kn` | NEW: `compile_and_or_textual` | Short-circuit and/or |
| `X:/blades/kain/src/codegen.kn` | NEW: `compile_index_textual` | Array/pointer indexing |
| `X:/blades/kain/src/codegen.kn` | NEW: `compile_spawn_textual` | Actor spawn stub (deferred to GOLD for real impl) |
| `X:/blades/kain/src/codegen.kn` | `compile_field_access_textual` (line 1315-1341) | Use real field name → index mapping from typechecker |
| `X:/blades/kain/src/codegen.kn` | `compile_call_textual` (line 1197-1212) | Use real function signatures (param types from TypedItem) |
| `X:/blades/kain/src/codegen.kn` | `emit_runtime_declares` (line 44-48) | Populate RuntimeTable with real declares |
| `X:/blades/kain/src/codegen.kn` | NEW: `emit_string_constants` | String literal emission: global string constants |
| `X:/blades/kain/src/codegen.kn` | `codegen_textual` (line 515-582) | Wire new sections: string constants, runtime declares, real struct defs |
| `X:/blades/kain/src/codegen.kn` | `LlvmGenerator` struct | Add string_constant_counter, runtime_function_table fields |
| `X:/blades/kain/src/runtime.kn` | `runtime_table_init` | Return populated RuntimeTable instead of empty |

### Files You Must NOT Touch

| File | Reason |
|------|--------|
| `X:/blades/kain/src/types.kn` | Owned by Stream RED |
| `X:/blades/kain/src/monomorphize.kn` | Owned by Stream RED |
| `X:/blades/kain/src/orchestrator.kn` | Owned by Stream GREEN |
| `X:/blades/kain/src/compiler.kn` | Owned by Stream GREEN |
| `X:/blades/kain/src/parser.kn` | Parser is done |
| `X:/blades/kain/src/lexer.kn` | Lexer is done |

---

## Implementation Tasks

### BLUE-1: Real Struct Type Definitions

**Effort:** 1 day
**Objective:** Replace `type opaque` with real struct types containing named, typed fields.

**Current state:** `emit_struct_defs_from_program` emits:
```llvm
%struct_0 = type opaque
%struct_1 = type opaque
```

**Implementation Steps:**

1. After RED finishes, TypedItem will carry a field map: for each struct, `item.resolved_type.struct_field_count` and `item.resolved_type.struct_field_types` (array of type indices into `all_types`).

2. For each struct in MonomorphizedProgram:
   - Map each field type to LLVM type via `map_type_to_llvm`
   - Emit: `%struct_<N> = type { i64, double, i64, ... }`
   - Use the field map (from TypedItem) to order fields correctly

3. Handle zero-field structs: emit `type {}` (valid LLVM IR).

4. Handle nested structs: if a field type is itself a struct, use `%struct_<M>`.

**Acceptance Criteria:**
- [ ] Struct definitions emitted with real field types
- [ ] Zero-field structs emit `type {}`
- [ ] Nested struct types referenced correctly
- [ ] Field order matches Kain struct declaration order
- [ ] `kainc build token.kn --target llvm` → LexerState struct has correct fields in .ll output

---

### BLUE-2: Complete Expression Lowering — Match

**Effort:** 2 days
**Objective:** Implement `compile_match_textual` with tag dispatch, pattern binding, and phi merge.

**AST_EXPR_MATCH layout:**
```
data[0] = matched_expr_idx (expression being matched)
data[1] = arm_count
Then for each arm: data[...] = pattern_node_idx, data[...+1] = body_node_idx
```

**Implementation Steps:**

1. Compile the matched expression to a register `match_val`
2. For each arm:
   - Extract the tag from `match_val` (for enum matches) or value (for integer/string matches)
   - Create a basic block for the arm body
   - Create a basic block for the "next arm" check
   - Compare `match_val` tag against the arm pattern's expected tag
   - Branch: if match → arm body block, else → next arm check
3. In arm body: bind pattern variables (if any — e.g., `Some(x)` binds `x` to payload), compile body expression
4. Merge block: phi node combining all arm result registers
5. If no arm matches (exhaustive match guaranteed by typechecker), emit `unreachable`

**Pattern handling:**
- Literal pattern (`42`): compare value directly
- Enum variant pattern (`Some(x)`): compare tag, extract payload to pattern variable
- Wildcard pattern (`_`): always matches
- Struct pattern (`Point { x, y }`): extract fields to pattern variables

**Acceptance Criteria:**
- [ ] Match on enum variants emits correct switch/br + phi
- [ ] Pattern variables bound correctly
- [ ] Exhaustive match with 2+ arms works
- [ ] Nested patterns (e.g., `Some(Ok(x))`) handled
- [ ] `kainc build parser.kn --target llvm` → match expressions emit real IR

---

### BLUE-3: Complete Expression Lowering — Array Literals

**Effort:** 1 day
**Objective:** Implement `compile_array_lit_textual` for `[a, b, c]` expressions.

**AST_EXPR_ARRAY_LIT layout:**
```
data[0] = element_count
data[1..N] = element expression node indices
```

**Implementation Steps:**

1. Allocate array: `alloca [N x element_type], align 8`
2. For each element:
   - Compile element expression
   - GEP to element slot: `getelementptr [N x T], ptr array_alloca, i64 0, i32 idx`
   - Store element value
3. Return pointer to array (arrays are passed by pointer in Kain)

**Acceptance Criteria:**
- [ ] Array of Ints emits correct alloca+GEP+store chain
- [ ] Array of Strings handled (struct type for String fat pointer)
- [ ] Empty array `[]` handled correctly

---

### BLUE-4: Complete Expression Lowering — For-Range Loop

**Effort:** 1 day
**Objective:** Implement `compile_for_textual` for `for x in iterable:`.

**AST_STMT_FOR layout:**
```
data[0] = loop_var_name_idx
data[1] = iterable_expr_idx
data[2] = body_expr_idx
```

**For-range lowering (desugar to while):**
```
// for x in 0..N: body
// desugars to:
let __i = 0
while __i < N:
    let x = __i
    body
    __i = __i + 1
```

**For-array lowering:**
```
// for x in arr: body
// desugars to:
let __i = 0
let __len = len(arr)
while __i < __len:
    let x = arr[__i]
    body
    __i = __i + 1
```

**Implementation:** Use the existing `compile_while_textual` internally. Desugar the for loop into a while loop with an index variable.

**Acceptance Criteria:**
- [ ] `for i in 0..10:` emits correct while loop IR
- [ ] `for item in array:` works with array index
- [ ] Loop variable shadows correctly

---

### BLUE-5: Complete Expression Lowering — Loop/Break/Continue

**Effort:** 1 day
**Objective:** Implement `compile_loop_textual`, break with value, and continue.

**AST_STMT_LOOP layout:**
```
data[0] = body_expr_idx (block node)
```

**AST_STMT_BREAK layout:**
```
data[0] = value_expr_idx (-1 if no value)
```

**Implementation Steps:**

1. The `loop_push`/`loop_pop` infrastructure already exists with `header_lbl` and `exit_lbl`.
2. `compile_loop_textual`:
   - Create header, body, exit labels
   - Push loop context
   - Branch to header → body
   - Body: compile, then branch to header (infinite loop unless break)
   - Exit: phi node for break value
3. `compile_break_textual`:
   - Check if in loop context (loop stack non-empty)
   - If break value: compile value expression
   - Branch to exit label with the value
4. `compile_continue_textual`:
   - Check if in loop context
   - Branch to header label

**Acceptance Criteria:**
- [ ] `loop: ... break 42` returns `42` from the loop
- [ ] `continue` jumps to loop header
- [ ] `break` outside loop reports error (or emits warning)
- [ ] Nested loops: break/continue targets innermost loop

---

### BLUE-6: Complete Expression Lowering — And/Or Logical Short-Circuit

**Effort:** 0.5 day
**Objective:** Implement `compile_and_or_textual` for `a and b` / `a or b`.

**Implementation Steps:**

1. For `a and b`:
   - Compile `a` to a register
   - Convert to i1: `icmp ne <ty> a, 0`
   - Create left_block (evaluate b), right_block (skip to phi), merge_block
   - Branch: if a==true → left_block, else → right_block
   - left_block: compile b → phi_incoming
   - right_block: phi_incoming = 0 (false)
   - merge_block: phi [b_val, %left], [0, %right]

2. For `a or b`:
   - Same structure but inverted: if a==true → skip to phi with true, else → evaluate b

**Acceptance Criteria:**
- [ ] `a and b` short-circuits: b not evaluated if a is false
- [ ] `a or b` short-circuits: b not evaluated if a is true
- [ ] Result type is i1

---

### BLUE-7: Complete Expression Lowering — Index, Enum Variant, Lambda

**Effort:** 2 days
**Objective:** Implement remaining expression kinds.

**a) Index expression (`arr[i]`):**
- For arrays: GEP to element, load element value
- For pointers: `getelementptr T, ptr base, i64 idx` → load
- Use `map_type_to_llvm` for the element type

**b) Enum variant construction (`MyEnum::Variant(payload)`):**
- Allocate enum struct: `{ i32 tag, %payload_type payload }`
- Set tag field to variant index
- Store payload in payload field
- Return pointer to enum struct

**c) Lambda/closure:**
- Phase 1: stub — emit a function pointer placeholder with captured variables as a struct
- Create closure struct with captured variable slots
- Emit trampoline function that loads captures and calls body
- Return closure struct pointer

**Acceptance Criteria:**
- [ ] `arr[i]` emits GEP + load for arrays
- [ ] `ptr[i]` emits GEP + load for pointers
- [ ] Enum variant construction emits tag + payload store
- [ ] Lambda stub emits valid IR (will improve in GOLD)

---

### BLUE-8: Method Call Codegen

**Effort:** 1 day
**Objective:** Implement `compile_method_call_textual` for `obj.method(args)`.

**AST_EXPR_METHOD_CALL layout:**
```
data[0] = method_name_idx
data[1] = object_expr_idx
data[2..N] = argument expr indices
```

**Implementation Steps:**

1. Compile object expression
2. Look up method on object's type (from TypedItem's resolved_type)
3. For struct methods: direct call to mangled name `struct_N_method_name`
4. For trait methods: vtable dispatch (stub for now — direct call)
5. Compile arguments, emit call instruction

**Acceptance Criteria:**
- [ ] `lexer_state.tokens.push(tok)` emits call to `struct_N_push`
- [ ] Method name mangling is deterministic
- [ ] Multiple arguments handled correctly

---

### BLUE-9: Runtime Function Declares

**Effort:** 1 day
**Objective:** Populate RuntimeTable with real function declares and emit them in module output.

**Implementation Steps:**

1. In `runtime.kn`, update `runtime_table_init()` to populate a minimum viable set of runtime functions. At minimum:
   - `__kain_alloc` (i64 size) → ptr
   - `__kain_free` (ptr)
   - `__kain_realloc` (ptr, i64 size) → ptr
   - `__kain_string_new` (ptr data, i64 len) → {i8*, i64}
   - `__kain_string_concat` ({i8*, i64} a, {i8*, i64} b) → {i8*, i64}
   - `__kain_println_str` ({i8*, i64} s) → void
   - `__kain_runtime_init` → i32
   - `__kain_runtime_shutdown` → i32
   - `__kain_strlen` (ptr) → i64
   - `__kain_strcmp` (ptr a, ptr b) → i32
   - `__kain_abort` → void
   - `__kain_assert_fail` (ptr msg, i64 line) → void

2. In `codegen.kn`, update `emit_runtime_declares`:
   - For each function in RuntimeTable:
   - Emit: `declare <return_type> @<name>(<param_types>)`
   - Add appropriate attributes (nounwind, readonly, etc.)

3. Wire into `codegen_textual`: call `emit_runtime_declares` after struct definitions and before functions.

**Acceptance Criteria:**
- [ ] RuntimeTable populated with 12+ runtime functions
- [ ] `declare` statements emitted in module output
- [ ] Function signatures match actual C runtime (`runtime/native/`)
- [ ] Attributes correctly applied (nounwind for alloc, readonly for strlen, etc.)

---

### BLUE-10: String ABI Marshaling

**Effort:** 1.5 days
**Objective:** Implement Kain `String` → LLVM `{i8*, i64}` fat pointer lowering.

**Implementation Steps:**

1. Add `map_type_to_llvm` entry for String: map to `{%struct_String = type {i8*, i64}}` or inline `{i8*, i64}`.

2. For string literals in code:
   - Create global string constant: `@.str.42 = private unnamed_addr constant [5 x i8] c"hello\00"`
   - Emit fat pointer: `{i8*, i64} {i8* getelementptr([5 x i8], ptr @.str.42, i64 0, i64 0), i64 5}`

3. Add `emit_string_constants` function:
   - Walk AST for all string literal nodes
   - Emit one global constant per unique string
   - Return mapping from string_idx → global name

4. In `compile_expr_textual` for AST_EXPR_STRING:
   - Allocate fat pointer struct: `alloca {i8*, i64}`
   - Store data pointer: `getelementptr @.str.N` → gep → store
   - Store length: computed from string literal length → store
   - Load fat pointer struct for use

5. String concatenation: emit call to `__kain_string_concat` with two fat pointers.

**Acceptance Criteria:**
- [ ] `let msg = "hello"` emits correct fat pointer
- [ ] String constants pooled (same string literal = same global)
- [ ] String concatenation `a + b` emits call to `__kain_string_concat`
- [ ] String pass to println emits correct fat pointer

---

### BLUE-11: Improve Struct Field Access Codegen

**Effort:** 1 day
**Objective:** Replace hardcoded `i32 0` field index with real field-name-to-index mapping.

**Current state:** `compile_field_access_textual` uses hardcoded `i32 0` for GEP (only accesses field 0). This needs to resolve field names to indices.

**Implementation Steps:**

1. After RED finishes, each Struct TypedItem carries a field name → index map.
2. In `compile_field_access_textual`:
   - Extract `field_name_idx` from the expression node
   - Look up field name in the AST (from string table or name_idx)
   - Look up field index from the struct's field map
   - Emit GEP with correct index: `getelementptr %struct_N, ptr obj, i32 0, i32 <field_idx>`

3. Handle nested field access: `a.b.c` → two GEPs.

**Acceptance Criteria:**
- [ ] `obj.field_name` emits GEP with correct field index
- [ ] Field index resolves from struct definition
- [ ] Nested field access works

---

### BLUE-12: Wire All New Codegen Into Pipeline

**Effort:** 0.5 day
**Objective:** Ensure all new expression compilers are dispatched from `compile_expr_textual` and `compile_stmt_textual`.

**Implementation Steps:**

1. In `compile_expr_textual`, add dispatch cases for:
   - `AST_EXPR_MATCH` → `compile_match_textual`
   - `AST_EXPR_WHILE` → `compile_while_textual` (already exists)
   - `AST_EXPR_ARRAY_LIT` → `compile_array_lit_textual`
   - `AST_EXPR_ENUM_VARIANT` → `compile_enum_variant_textual`
   - `AST_EXPR_LAMBDA` / `AST_EXPR_CLOSURE` → `compile_lambda_textual`
   - `AST_EXPR_METHOD_CALL` → `compile_method_call_textual`
   - `AST_EXPR_AND` / `AST_EXPR_OR` → `compile_and_or_textual`
   - `AST_EXPR_INDEX` → `compile_index_textual`
   - `AST_EXPR_LOOP` → `compile_loop_textual`
   - `AST_EXPR_BREAK` → `compile_break_textual`
   - `AST_EXPR_CONTINUE` → `compile_continue_textual`
   - `AST_EXPR_RETURN` → `compile_return_textual` (already exists)
   - `AST_EXPR_FOR` → `compile_for_textual`

2. In `compile_stmt_textual`, add dispatch for:
   - `AST_STMT_FOR` → `compile_for_textual`
   - `AST_STMT_LOOP` → `compile_loop_textual`
   - `AST_STMT_BREAK` → `compile_break_textual`
   - `AST_STMT_CONTINUE` → `compile_continue_textual`

3. In `codegen_textual`, wire:
   - After struct defs: call `emit_string_constants`
   - After string constants: call `emit_runtime_declares`
   - Then functions as before

**Acceptance Criteria:**
- [ ] All 64 expression kinds have a dispatch case in `compile_expr_textual`
- [ ] Unknown expression kinds still use fallback (but emit a diagnostic comment)
- [ ] `kainc build parser.kn --target llvm` → produces .ll with real instructions

---

## Stream Conventions

- **Language:** Kain (.kn files)
- **LLVM IR format:** Textual IR with proper indentation (2-space)
- **Naming:** LLVM registers named `%rN`, labels named descriptive names (`then`, `else`, `merge`, `header`, `body`, `exit`)
- **Register management:** Use `next_reg(gen) -> GenResult` consistently
- **Label management:** Use `next_label(gen) -> GenResult` consistently
- **Type mapping:** Use `map_type_to_llvm(ResolvedType) -> String` for all type lookups
- **Comments:** Mark new functions with `// ── Stream BLUE ──`

---

## Stream Boundary — What You Do NOT Do

- ❌ Do NOT modify typechecker (types.kn) — RED owns that
- ❌ Do NOT modify orchestrator.kn or compiler.kn — GREEN owns those
- ❌ Do NOT implement L1-L7 codegen (world, actor, converge, orchestrate, GPU, etc.) — GOLD owns that
- ❌ Do NOT implement DWARF debug info emission — deferred
- ❌ Do NOT implement Path B (LLVM-C API) as real — keep as stubs for now
- ❌ Do NOT change the textual IR format (must remain valid LLVM IR)
- ❌ Do NOT add external dependencies beyond what's already in stdlib

---

## Verification (After This Stream)

After completing all tasks, verify:

```bash
# Build check — codegen.kn must typecheck
kain check X:\blades\kain\src\codegen.kn

# Compile a simple test file  
echo "fn main() -> Int: return 42" > test_simple.kn
cd X:\blades\kain
.\.kain\out\kainc.exe build test_simple.kn --target llvm
# Verify output.ll contains: define i64 @fn_main() { ... ret i64 42 }

# Compile a more complex file
.\.kain\out\kainc.exe build src/token.kn --target llvm
# Verify struct types are real, const globals exist, function bodies are real

# Compile the parser!
.\.kain\out\kainc.exe build src/parser.kn --target llvm
# This is the acid test — 3345 lines, uses nearly every expression kind
```

**Self-check:**
- [ ] All 64 expression kinds have codegen dispatch
- [ ] Struct types emitted with real fields (not opaque)
- [ ] Match expressions emit switch/br + phi
- [ ] For loops desugar to while loops
- [ ] Loop/break/continue emit correct branches
- [ ] Array literals emit alloca + gep + store chain
- [ ] Method calls emit direct calls
- [ ] String literals emit fat pointers
- [ ] Runtime declares emitted in module header
- [ ] Field access uses field name → index mapping
- [ ] `kainc build parser.kn --target llvm` produces valid LLVM IR
- [ ] No files modified outside codegen.kn and runtime.kn

---

## Completion Report

When done, report:
- Files modified: <list with changes summary>
- New functions added: <count and names>
- Expression kinds now handled: <count> (was 17, target: 30+)
- Runtime functions declared: <count>
- String constants emitted: <mechanism description>
- Any expression kinds still stubbed: <list>
- Any issues encountered: <list or "none">
- What ouroboros Phase 2 needs to know: <notes>
- Test results: `kainc build parser.kn --target llvm` output summary
