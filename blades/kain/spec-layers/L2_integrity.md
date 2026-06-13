# L2: State Integrity — Patch + Law Integration Guide

**Date:** 2026-06-12
**Canonical sources:** `docs/PATCH.MD`, `docs/LAW.MD`, `src/types.kn` (stubs at L1596-L1608), `src/codegen.kn` (stubs at L643-L646), `src/compiler.kn` (pipeline wiring), `runtime/native/include/stdlib_abi.h` (abi_patch_*), `review/FINAL_GAPS.md`

---

## 1. Current State

### Parser: ALREADY WORKS — no changes needed

`parse_patch()` and `parse_law()` in `parser.kn` already produce full `AstNode` trees:

**Patch** — `patch <name>(<params>) [-> <ReturnType>]: <body>`
```
AST_ITEM_PATCH: data[0]=name_idx, data[1]=param_count, then
                (param_name_idx, param_type_idx) pairs, then
                ret_type_idx (or -1 if none), then body_idx
```

**Law** — `law <name>(<params>) -> Bool: <body>`
```
AST_ITEM_LAW: data[0]=name_idx, data[1]=param_count, then
              (param_name_idx, param_type_idx) pairs, then
              ret_type_idx (always points to Bool type node), then body_idx
```

Both parse successfully and pass through the 4-pass typecheck pipeline.

### Typechecker: STUBS — need real implementations

**`check_patch_law_stub`** at `types.kn:1596-1608`:

```kain
pub fn check_patch_law_stub(env: TypeEnv, node: AstNode, idx: Int, kind: Int) -> TypedItemAndEnv:
    let name_idx: Int = if ast_data_len(node) > 0: ast_data_get(node, 0) else: -1
    let ret: ResolvedType = if kind == AST_ITEM_LAW: rt_bool() else: rt_i64()
    return TypedItemAndEnv {
        env: env,
        item: TypedItem {
            kind: kind, name: "pl_" + str(name_idx), name_idx: name_idx,
            resolved_type: ret, ast_index: idx, effects: EFF_PURE,
        }
    }
```

Problems: (1) Does not type-check the function body at all. (2) For law, does not enforce `-> Bool` return type. (3) For patch, does not collect mutation paths. (4) Does not infer undo mode.

### Codegen: MISSING — patch/law items silently dropped

At `codegen.kn:641-646`, only `AST_ITEM_FUNCTION` is in the compile loop. Patch and law items produce no LLVM IR. There is no `abi_patch_begin/record/commit` emission.

### Runtime ABI: EXISTS — C functions ready

The native C runtime already provides:
- `abi_patch_begin(patch_name)` — start a patch transaction
- `abi_patch_record_i64(patch_name, path, old_value, new_value)` — record a mutation
- `abi_patch_commit(patch_name)` — commit the transaction
- `abi_patch_journal_count()` — get total journal entries
- `abi_patch_undo_last()` — undo last committed entry

Declares are in `core_runtime_declares_fallback()` already. The codegen just needs to call them.

The law runtime ABI is even simpler: **there is none.** Law compiles as a plain function with a `Bool` return type. The `law_status()` wrapper in `stdlib/intent.kn` converts `Bool` to `Int` (0 = valid, -1 = invalid). No C-level law registry exists.

---

## 2. What Needs to Happen

### 2.1 Real `check_patch` in types.kn

Replace the stub branch for `AST_ITEM_PATCH` with a real typecheck that:

**(A) Reuses the function typecheck path.** Patch is structurally identical to `fn` — parameters, return type, body block. The optimal implementation is to wrap the patch AST in a function view and call `check_function_item` (or `check_named_callable`):

```kain
// In check_patch:
env.in_patch = true          // world state access enabled
let result = check_function_item(env, patch_as_function_view(node), idx)
env.in_patch = false
```

This gives us: parameter type checking, body expression inference, effect tracking, return type unification — all for free.

**(B) Collect mutation paths.** After typechecking, walk the body block's AST nodes to find all `AST_EXPR_ASSIGN` nodes where the LHS is a field access (`AST_EXPR_FIELD`). Record `"WorldName.field"` for each. These are the journaled mutation paths.

```kain
// Simplified mutation path collection:
fn collect_mutation_paths(env: TypeEnv, body_idx: Int, ast_nodes: Array<AstNode>) -> Array<String>:
    var paths: Array<String> = []
    // Walk statements in body block
    let body_node: AstNode = ast_nodes[body_idx]
    var si: Int = 0
    while si < ast_data_len(body_node):
        let stmt_idx: Int = ast_data_get(body_node, si)
        let stmt_node: AstNode = ast_nodes[stmt_idx]
        if stmt_node.kind == AST_STMT_LET:
            let init_idx: Int = ast_data_get(stmt_node, 1)
            // Recurse into init expr if it's a block
            // ... (simplified for Phase 1: just scan for assigns in direct stmts)
        si = si + 1
    return paths
```

**(C) Infer undo mode.** For Phase 1, always return `"reversible"`. Future phases will scan for fanout/GPU/IO effects.

**(D) Return** `TypedItem` with `kind: AST_ITEM_PATCH, resolved_type: <checked_return_type>, effects: <checked_effects>`.

### 2.2 Real `check_law` in types.kn

Replace the stub branch for `AST_ITEM_LAW`:

**(A) Enforce `-> Bool` return.** Law must declare `-> Bool`. Check the return type node in the AST. If it's missing or not Bool, push diagnostic: `"law 'X' must return Bool"`.

**(B) Reuse function typecheck path.** Same as patch — wrap in function view and call `check_function_item` with `env.in_patch = true` (laws can read world state).

**(C) Validate body infers Bool.** The body's inferred type must be compatible with `Bool`. If not: `"law 'X' body must return Bool, found T"`.

**(D) Return** `TypedItem` with `kind: AST_ITEM_LAW, resolved_type: rt_bool(), effects: <checked_effects>`.

### 2.3 `compile_patch` in codegen.kn

Patch codegen wraps the normal function codegen with journaling calls:

**(A) Function signature.** Same as `fn` — `define <ret_type> @patch_<name>(<params>)`. The patch is a callable function in LLVM.

**(B) Entry: `abi_patch_begin`.** After function prologue, emit:

```llvm
%patch_status = call i64 @abi_patch_begin(ptr @".patch_name")
```

Where `@".patch_name"` is a global string constant for the patch's name.

**(C) Per-field assignment: `abi_patch_record_i64`.** After every `store` to a world field, emit:

```llvm
; Before store: backup old value
; (already captured if the store follows a load-modify-write pattern)
%record_status = call i64 @abi_patch_record_i64(
  ptr @".patch_name",       ; patch name string
  ptr @".path.World.field", ; mutation path string
  i64 %old_value,           ; value before mutation
  i64 %new_value)           ; value after mutation
```

For Phase 1, this is emitted as a post-store call. The codegen tracks `current_patch_name` (set by `compile_patch`) and emits `abi_patch_record_i64` after any store instruction where the dest is a world field.

**(D) Exit: `abi_patch_commit`.** Before every return instruction, emit:

```llvm
%commit_status = call i64 @abi_patch_commit(ptr @".patch_name")
```

**(E) String constants.** Emit private global strings for patch name and each mutation path:

```llvm
@".patch_guard_set" = private unnamed_addr constant [11 x i8] c"guard_set\00", align 1
@".path.Authority.signal" = private unnamed_addr constant [19 x i8] c"Authority.signal\00", align 1
```

### 2.4 `compile_law` in codegen.kn

Law codegen is simpler — it's just `compile_function_textual` with a `Bool` return:

**(A) Function signature.** `define i1 @law_<name>(<params>)`. The return type is `i1` (Bool).

**(B) Body.** Compile the body expression. The last expression must produce an `i1`. The compilation reuses `compile_block_textual` and `compile_expr_textual`.

**(C) No special instrumentation.** Law compiles to a plain function. No `abi_law_*` calls exist because no C-level law infrastructure exists. The `law_status()` wrapper is a separate function in `stdlib/intent.kn` that calls the law function and converts `Bool` → `Int`.

```llvm
define i1 @law_signal_in_bounds(i64 %value) #0 {
entry:
  %0 = icmp sge i64 %value, 0
  %1 = icmp slt i64 %value, 1000000007
  %2 = and i1 %0, %1
  ret i1 %2
}
```

**(D) Pure attribute.** Since laws should be pure predicates, emit `#0` (nounwind readnone):

```llvm
define i1 @law_signal_in_bounds(i64) #0 {
```

### 2.5 Routing in codegen.kn

**(A) `emit_struct_defs_from_program`** — no changes needed. Patch/law are functions, not structs.

**(B) `codegen_textual`** — after const globals and before function loop, emit patch string constants. In the function loop, add dispatch:

```kain
var i: Int = 0
while i < len(program.items):
    let item: TypedItem = program.items[i]
    if item.kind == AST_ITEM_FUNCTION:
        gen = compile_function_textual(gen, item, program.ast_nodes)
    elif item.kind == AST_ITEM_PATCH:
        gen = compile_patch_textual(gen, item, program.ast_nodes)
    elif item.kind == AST_ITEM_LAW:
        gen = compile_law_textual(gen, item, program.ast_nodes)
    i = i + 1
```

**(C) Compiler.kn routing** — no changes needed. The `TypedItem`s already flow through the pipeline.

---

## 3. Edge Cases

### 3.1 Epoch Tracking

Every `patch` that mutates world state should bump an epoch counter. The typechecker does NOT enforce this — it's a convention, not a compiler rule. However, the codegen CAN detect patches that mutate world state and emit a warning if no epoch field is written.

For Phase 1: no epoch tracking enforcement. Document as known gap.

### 3.2 patch_journal_count Integration

The `patch_journal_count()` function is already declared in `core_runtime_declares_fallback()`:

```llvm
declare i64 @abi_patch_journal_count()
```

User code can call it like any extern function. The codegen does NOT auto-emit journal count checks — they're the user's responsibility via `use std::intent`.

### 3.3 Law Function View

The law AST node layout has the same parameter/body structure as `fn`. The `check_function_item` function currently expects `AST_ITEM_FUNCTION` items. For law/patch, we need a wrapper that creates a function-shaped view:

```kain
fn patch_law_as_function_view(node: AstNode, kind: Int) -> AstNode:
    // Returns a modified node with kind == AST_ITEM_FUNCTION
    // but the same data layout (name_idx, params, ret_type, body)
    // This allows reusing check_function_item's internals
    let mut fn_node: AstNode = node
    fn_node.kind = AST_ITEM_FUNCTION
    return fn_node
```

Alternatively, modify `check_function_item` to accept a `kind` parameter and handle `AST_ITEM_PATCH`/`AST_ITEM_LAW` directly.

### 3.4 Law Body Must Be Pure by Convention

The compiler allows effects in law bodies, but it should warn. For Phase 1, do NOT enforce — just typecheck the body normally. Add a diagnostic for non-Pure effects in law bodies as an optimization for Phase 2.

### 3.5 Patch in Hot Loops (Journal Capacity)

The native C journal has a capacity of 256 entries. When a patch is called in a hot loop, the journal can fill up. The codegen does NOT handle this — the C runtime returns `-3` from `abi_patch_record_i64` when full. This is documented in the Z3 proofs as expected behavior. The user's code should check `patch_journal_count()` before hot loops.

---

## 4. Files to Modify

| File | Changes | Priority |
|------|---------|----------|
| `src/types.kn` | Replace `check_patch_law_stub` branch with `check_patch` + `check_law` | P1 |
| `src/types.kn` | Add `patch_law_as_function_view` or extend `check_function_item` | P1 |
| `src/types.kn` | Add `collect_mutation_paths` helper function | P2 |
| `src/types.kn` | Add law `-> Bool` enforcement in typecheck | P1 |
| `src/codegen.kn` | Add `compile_patch_textual` — emit abi_patch_begin/record/commit | P1 |
| `src/codegen.kn` | Add `compile_law_textual` — emit as `i1` function with `#0` | P1 |
| `src/codegen.kn` | Add `emit_patch_strings` — global string constants for patch names + paths | P1 |
| `src/codegen.kn` | Wire patch/law dispatch in `codegen_textual` | P1 |

---

## 5. Acceptance Criteria

1. `kain check patch_valid.kn` — patch with world field assignments passes typecheck
2. `kain check law_valid.kn` — law returning Bool passes typecheck
3. `kain check law_no_bool.kn` — law without `-> Bool` produces diagnostic `"law must return Bool"`
4. `kain check law_body_not_bool.kn` — law body that doesn't return Bool produces type mismatch diagnostic
5. `kain build patch.kn --target llvm` — patch function LLVM emits `abi_patch_begin` at entry, `abi_patch_record_i64` after field writes, `abi_patch_commit` before return
6. `kain build law.kn --target llvm` — law function LLVM emits `define i1 @law_*` with `#0` attribute and Bool-returning body
7. Patch string constants (`@".patch_*"`, `@".path.*"`) appear in the LLVM module
8. Both patch and law functions are callable from user code (via the LLVM function symbols)
