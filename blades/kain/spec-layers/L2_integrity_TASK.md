# L2: State Integrity — Implementation Tasks

**Date:** 2026-06-12
**Phase:** Wave 5-6 (typechecker + codegen realization)
**Files:** `src/types.kn`, `src/codegen.kn`

---

## TASK-1: Real check_patch in types.kn

**File:** `src/types.kn` — replace `check_patch_law_stub` branch for `AST_ITEM_PATCH`
**Effort:** 2 days
**Dependencies:** `check_function_item` must exist (or the function-view wrapper)

### What to Do

Add `check_patch(env, node, idx) → TypedItemAndEnv`:

1. Set `env.in_patch = true` before typechecking
2. Convert the patch AST node into a function-compatible view (or call `check_function_item` with the existing node passing `kind = AST_ITEM_PATCH` for it to handle)
3. The function view treats patch params, return type, and body block identically to `fn`
4. **Return type**: patches can have an optional return type. If absent, the return type is `rt_unit()`. This is already handled by the function typechecker.
5. **Mutation path collection** (Phase 1 minimal): scan the body block's direct statements for `AST_EXPR_ASSIGN` whose LHS is `AST_EXPR_FIELD`. Record each unique `"WorldName.field"` string in a simple array.
6. **Undo mode** (Phase 1): always set to `"reversible"`. No body scanning needed.
7. Return `TypedItemAndEnv` with `kind: AST_ITEM_PATCH` and the checked return type.

### Key Implementation Detail

The body block index is encoded in the patch's AST node. Use the same layout as function nodes (see `compile_function_textual` data parsing at codegen.kn:918-940 — `cg_func_skip_attrs`, `cg_func_skip_params`, then locate body_idx at the end).

### Acceptance Criteria

- [ ] Patch with `-> Int` return type typechecks and returns correct type
- [ ] Patch without return type typechecks as `rt_unit()`
- [ ] Patch body with world field writes passes typecheck
- [ ] Patch body with type errors produces diagnostics
- [ ] Patch mutation paths are collected in the TypedItem metadata

---

## TASK-2: Real check_law in types.kn (enforce Bool return)

**File:** `src/types.kn` — replace `check_patch_law_stub` branch for `AST_ITEM_LAW`
**Effort:** 1 day
**Dependencies:** `check_function_item` must exist

### What to Do

Add `check_law(env, node, idx) → TypedItemAndEnv`:

1. Read `ret_type_idx` from the law's AST node (same layout as function — find the return type node index using `cg_func_skip_*` or by walking the data manually)
2. If `ret_type_idx < 0` or the resolved type is not `rt_bool()`: push diagnostic `"law 'X' must return Bool"`
3. Set `env.in_patch = true` (laws can read world state)  
4. Call `check_function_item` with the law node (or function view), same as patch
5. After typechecking, verify the body's inferred return type is compatible with `Bool`. If not: push diagnostic `"law 'X' body must return Bool, found T"`
6. Return `TypedItemAndEnv` with `kind: AST_ITEM_LAW, resolved_type: rt_bool()`

### Edge Cases

- A law that calls another law (nesting) must typecheck both return types as Bool
- A law that reads `WorldName.field` is allowed (it's a predicate, not a mutator)
- A law containing `return true` at the end is the simplest valid form

### Acceptance Criteria

- [ ] Law `law valid(v: Int) -> Bool: return v >= 0` passes
- [ ] Law without `-> Bool`: diagnostic `"law must return Bool"`
- [ ] Law returning non-Bool expression: diagnostic `"law body must return Bool"`
- [ ] Law with world state reads passes

---

## TASK-3: compile_patch in codegen.kn (emit abi_patch_begin/record/commit)

**File:** `src/codegen.kn` — new function `compile_patch_textual`
**Effort:** 2 days
**Dependencies:** `compile_function_textual` must work for basic functions

### What to Do

Add `compile_patch_textual(gen, item, ast_nodes) → LlvmGenerator`:

1. **Function signature**: same as `compile_function_textual` but add `#1` attribute (nounwind, not readnone):
   ```
   define i64 @patch_<name>(i64 %param_0) #1 {
   ```

2. **Emit global string constants** for the patch name and mutation paths. Add to `emit_global_strings` or emit inline:
   ```
   @".p_<name_idx>" = private unnamed_addr constant [N x i8] c"<patch_name>\00", align 1
   @".p_<name_idx>_path_0" = private unnamed_addr constant [N x i8] c"World.field\00", align 1
   ```

3. **Entry: `abi_patch_begin`**. After prologue (parameter allocas), emit:
   ```
   %patch_begin = call i64 @abi_patch_begin(ptr @".p_<name_idx>")
   ```

4. **Body compilation**: call `compile_block_textual` for the body block (same as function). Track the `current_patch_name` in a mutable generator field so that `compile_assign_textual` knows to emit `abi_patch_record_i64` after field stores.

5. **Exit: `abi_patch_commit`**. Before each `ret` instruction, emit:
   ```
   %patch_commit = call i64 @abi_patch_commit(ptr @".p_<name_idx>")
   ```

### Required LlvmGenerator Changes

```kain
// In LlvmGenerator struct:
current_patch_name: String  // "" if not in a patch body
mut_path_strings:    Array<String>  // string constants for mutation paths
```

### ABI Declares to Add

Add to `core_runtime_declares_fallback()`:

```llvm
declare i64 @abi_patch_begin(ptr)
declare i64 @abi_patch_record_i64(ptr, ptr, i64, i64)
declare i64 @abi_patch_commit(ptr)
declare i64 @abi_patch_journal_count()
```

### Acceptance Criteria

- [ ] Patch function LLVM has `define i64 @patch_*` signature
- [ ] Entry: `call i64 @abi_patch_begin(ptr @".p_*")`
- [ ] Body: normal function body compilation works  
- [ ] Exit: `call i64 @abi_patch_commit(ptr @".p_*")` before each return
- [ ] String constants for patch name and mutation paths exist in module
- [ ] ABI declares are present

---

## TASK-4: compile_law in codegen.kn (emit law_status check)

**File:** `src/codegen.kn` — new function `compile_law_textual`
**Effort:** 1 day
**Dependencies:** `compile_function_textual` must work for basic functions

### What to Do

Add `compile_law_textual(gen, item, ast_nodes) → LlvmGenerator`:

1. **Function signature**: `define i1 @law_<name>(i64 %param_0) #0`
   - Return type is `i1` (Bool)
   - Attribute `#0` = `{ nounwind readnone }` — laws are pure predicates

2. **Body**: call `compile_block_textual` for the body block. The body's trailing expression must produce an `i1`.

3. **No special instrumentation.** Law is a plain `i1`-returning function. No `abi_law_*` calls.

4. **Pure attribute**: if the body has no effects, use `#0`. This enables LLVM optimization.

### Example LLVM Output

```llvm
define i1 @law_signal_in_bounds(i64 %param_0) #0 {
entry:
  %v_0 = alloca i64, align 8
  store i64 %param_0, ptr %v_0, align 8
  %0 = load i64, ptr %v_0, align 8
  %1 = icmp sge i64 %0, 0
  %2 = load i64, ptr %v_0, align 8
  %3 = icmp slt i64 %2, 1000000007
  %4 = and i1 %1, %3
  ret i1 %4
}
```

### Acceptance Criteria

- [ ] Law function LLVM has `define i1 @law_*` signature with `#0` attribute
- [ ] Law body compiles using the normal block/expression compilers
- [ ] Law returns `i1` (Bool)
- [ ] No ABI-specific calls in law output

---

## TASK-5: Wire routing in codegen.kn

**File:** `src/codegen.kn` — modify `codegen_textual` (L596-649)
**Effort:** 0.5 days
**Dependencies:** TASK-3, TASK-4

### What to Do

Modify the item iteration loop in `codegen_textual`:

```kain
// Before function compilation, emit patch string constants
gen = emit_patch_strings(gen, program)

// In the function loop, dispatch by item kind:
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

### Acceptance Criteria

- [ ] Patch items compile to LLVM through the pipeline
- [ ] Law items compile to LLVM through the pipeline
- [ ] Patch and law functions are properly ordered in the LLVM module
- [ ] No regression in function-only compilation
