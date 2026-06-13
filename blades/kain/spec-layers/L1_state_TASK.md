# L1: State Authority — Implementation Tasks

**Date:** 2026-06-12
**Phase:** Wave 5-6 (typechecker + codegen realization)
**Files:** `src/types.kn`, `src/codegen.kn`

---

## TASK-1: Real check_world in types.kn

**File:** `src/types.kn` — replace `check_world_stub` (L1648-1656)
**Effort:** 2 days
**Dependencies:** None (pass1 predeclare already works)

### What to Do

Replace `check_world_stub` with `check_world(env, node, idx)` that:

1. Reads world name from `data[0]`
2. Reads state count from `data[1]` (number of state declarations)
3. For each state slot (positioned after `data[1]`):
   - Read `name_idx` (AST_EXPR_IDENT index), `type_idx` (AST type node index), `init_idx` (AST expression index)
   - Resolve type via `resolve_type_in_env(env, type_idx)`
   - Infer init expr type via `infer_expr_type(env, ast_nodes[init_idx])`
   - Call `ensure_type_compatible(env, resolved_type, inferred_init_type, span, "world state initializer")`
   - Register field name + type in env
4. Read surface count and validate at least 1 exists. If count == 0: push diagnostic `"world 'X' must declare at least one surface"`
5. Validate surface kinds are known (native_ui, web, viewport3d, ue5)
6. Return `TypedItemAndEnv` with `kind: AST_ITEM_WORLD, resolved_type: rt_struct_as(name_idx), effects: EFF_PURE`

### Required TypeEnv Additions

```kain
// In TypeEnv struct:
world_names:        Array<String>     // declared world names (parallel arrays)
world_field_names:  Array<Array<String>>  // per-world field name lists
world_field_types:  Array<Array<ResolvedType>>  // per-world field type lists
```

### Acceptance Criteria

- [ ] World with valid state slots + surface passes
- [ ] World with zero surfaces: error `"must declare at least one surface"`
- [ ] World with state initializer type mismatch: error
- [ ] World state field accessible via `WorldName.field` in expressions

---

## TASK-2: Real check_entangle in types.kn

**File:** `src/types.kn` — replace `check_entangle_stub` (L1658-1666)
**Effort:** 1.5 days
**Dependencies:** TASK-1 (must have world info available in env)

### What to Do

Replace `check_entangle_stub` with `check_entangle(env, node, idx)` that:

1. Read left endpoint: `left_world_idx` from `data[1]`, `left_field_idx` from `data[2]`
2. Read right endpoint: `right_world_idx` from `data[3]`, `right_field_idx` from `data[4]`
3. Validate both worlds exist in `env.world_names`
4. Validate both fields exist in their respective world's `world_field_names`
5. Type-check: the left field's type must be compatible with the right field's type via `types_compatible()`. If not: diagnostic `"entangle endpoint type mismatch: left has T, right has U"`
6. Deduplicate: check that neither endpoint is already registered in `entangle_endpoint_set`. If duplicate: diagnostic `"entangle endpoint 'X.field' is already coupled"`
7. Register in `entangle_left_auth`, `entangle_right_mirror`, `entangle_policies`
8. Mark `right_field` as a mirror for single-writer enforcement
9. Return `TypedItemAndEnv` with `kind: AST_ITEM_ENTANGLE, resolved_type: rt_unit(), effects: EFF_PURE`

### Required TypeEnv Additions

```kain
// In TypeEnv struct:
entangle_left_auth:     Array<String>  // "WorldName.field"
entangle_right_mirror:  Array<String>  // "WorldName.field"
entangle_policies:      Array<Int>     // 0 = single_writer
entangle_endpoint_set:  Array<String>  // all endpoints for dedup
mirror_fields:          Array<(String, String)>  // (world_name, field_name) for write guard
```

### Mirror Write Guard

In `check_item` → `AST_EXPR_ASSIGN` branch (or in expression inference for assign), add:

```kain
// After resolving LHS as a field access:
let is_mirror: Bool = is_mirror_field(env, assigned_world_name, assigned_field_name)
if is_mirror:
    push_diagnostic(env, "cannot write to entangled mirror field 'X.Y'; "
                     + "write to authority 'X.Z' instead", span)
```

### Acceptance Criteria

- [ ] Valid entangle with matching types passes
- [ ] Entangle where endpoint world doesn't exist: error
- [ ] Entangle where endpoint field doesn't exist: error
- [ ] Entangle with type mismatch: error
- [ ] Duplicate endpoint (field in two entangles): error
- [ ] Assignment to mirror field: error

---

## TASK-3: compile_world_globals in codegen.kn

**File:** `src/codegen.kn` — new functions
**Effort:** 2 days
**Dependencies:** None (uses TypedItem data)

### What to Do

Add these new functions:

**(A) `emit_world_type_defs(gen, program)`**
- Iterate `program.items` for `AST_ITEM_WORLD`
- Emit `%world_X = type { i64, i64, ... }` with one `i64` per state field

**(B) `emit_world_globals(gen, program)`**
- For each world: emit `@__kain_world_X`, `@__kain_world_init_flag_X`, `define void @__kain_init_world_X()`

**(C) Track world globals for field access.** Add to `LlvmGenerator`:

```kain
// In LlvmGenerator:
world_init_fns: Array<String>  // "world_X" -> "__kain_init_world_X"
world_globals:  Array<String>  // "world_X" -> "@__kain_world_X"
```

### Prototype LLVM IR Output

```llvm
%world_RatTelemetry = type { i64, i64, i64, i64, i64, i64 }
@__kain_world_RatTelemetry = global %world_RatTelemetry zeroinitializer
@__kain_world_init_flag_RatTelemetry = global i1 0
define void @__kain_init_world_RatTelemetry() {
entry:
  %0 = load i1, ptr @__kain_world_init_flag_RatTelemetry
  br i1 %0, label %already_init, label %init
init:
  %1 = getelementptr inbounds %world_RatTelemetry, ptr @__kain_world_RatTelemetry, i32 0, i32 0
  store i64 0, ptr %1
  store i1 1, ptr @__kain_world_init_flag_RatTelemetry
  br label %already_init
already_init:
  ret void
}
```

### Wire into `codegen_textual`

After `emit_const_globals` and before the function loop:

```kain
gen = emit_world_type_defs(gen, program)
gen = emit_world_globals(gen, program)
```

### Acceptance Criteria

- [ ] World with 3 state fields produces LLVM struct type with 3 i64 fields
- [ ] World produces `@__kain_world_X` global with `zeroinitializer`
- [ ] World produces `@__kain_world_init_flag_X` guard
- [ ] World produces init function with GEP + store for each field
- [ ] World metadata is registered for later field access lookup

---

## TASK-4: compile_entangle in codegen.kn

**File:** `src/codegen.kn` — new function
**Effort:** 1.5 days
**Dependencies:** TASK-2 (for entangle metadata), TASK-3 (for world globals)

### What to Do

Add `emit_entangle_registration(gen, program)`:

1. For each `AST_ITEM_ENTANGLE`, emit global string constants for the endpoint paths:
   ```
   @".ent_0.left" = private unnamed_addr constant [...] c"Authority.signal\00"
   @".ent_0.right" = private unnamed_addr constant [...] c"Mirror.signal_copy\00"
   @".ent_0.policy" = private unnamed_addr constant [...] c"single_writer\00"
   @".ent_0.type" = private unnamed_addr constant [...] c"Int\00"
   ```

2. Emit `abi_entangle_register` call (inside a module-level constructor or in `@__kain_init_world_X`):
   ```
   %ent_0_status = call i64 @abi_entangle_register(
     ptr @".ent_0.left", ptr @".ent_0.right",
     ptr @".ent_0.policy", ptr @".ent_0.type")
   ```

3. Add entangle registration to the `core_runtime_declares_fallback()` or `RuntimeTable`:
   ```
   declare i64 @abi_entangle_register(ptr, ptr, ptr, ptr)
   declare i64 @abi_entangle_record_i64(ptr, ptr, i64)
   ```

### Wire into `codegen_textual`

After `emit_world_globals`:

```kain
gen = emit_entangle_registration(gen, program)
```

### Acceptance Criteria

- [ ] Entangle declaration produces string constants for endpoint paths
- [ ] Entangle declaration emits `abi_entangle_register` call
- [ ] Entangle ABI functions are declared in the LLVM module
- [ ] Multiple entangles produce sequentially numbered string constants

---

## TASK-5: Wire routing in codegen_textual

**File:** `src/codegen.kn` — modify `codegen_textual` (L596-649)
**Effort:** 0.5 days
**Dependencies:** TASK-3, TASK-4

### What to Do

Update the item iteration loop to also compile world/entangle items:

```kain
// Before the function loop, emit world infrastructure:
gen = emit_world_type_defs(gen, program)
gen = emit_world_globals(gen, program)
gen = emit_entangle_registration(gen, program)

// In the function loop, add world field access + world init calls:
var i: Int = 0
while i < len(program.items):
    let item: TypedItem = program.items[i]
    if item.kind == AST_ITEM_FUNCTION:
        gen = compile_function_textual(gen, item, program.ast_nodes)
    elif item.kind == AST_ITEM_PATCH or item.kind == AST_ITEM_LAW:
        gen = compile_patch_law_textual(gen, item, program.ast_nodes)
    // AST_ITEM_WORLD and AST_ITEM_ENTANGLE are handled
    // by the emit_* functions above — no per-item codegen needed
    i = i + 1
```

**Important:** World and entangle items generate their LLVM code in the section-level `emit_*` functions (before function defs), not in the item loop. The item loop is only for constructs that produce LLVM functions (fn, patch, law).

### Acceptance Criteria

- [ ] Full pipeline: source → lex → parse → typecheck → monomorphize → codegen produces valid LLVM for world + entangle declarations
- [ ] LLVM module contains struct types, globals, and init functions for each world
- [ ] LLVM module contains abi_entangle_register calls for each entangle
- [ ] No items are silently dropped (all world/entangle items produce output)
