# L1: State Authority — World + Entangle Integration Guide

**Date:** 2026-06-12
**Canonical sources:** `docs/WORLD.MD`, `docs/ENTANGLE.MD`, `src/types.kn` (stubs at L1596-L1666), `src/codegen.kn` (stubs at L643-L646), `src/compiler.kn` (pipeline wiring), `research/02-typechecker-types.md` (stub strategy), `review/FINAL_GAPS.md` (L1-L7 gaps at P2)

---

## 1. Current State

### Parser: ALREADY WORKS — no changes needed

`parse_world()` and `parse_entangle()` in `parser.kn` already produce full `AstNode` trees with proper data layouts. The parser correctly handles:

- `world <Name>:` with `state <field>: <Type> = <expr>` and `surface <kind> => <Component>`
- `entangle <Left> <-> <Right> with single_writer`
- Both `view`/`viewport3d`/`native_ui`/`web`/`ue5` surface kinds

The AST node layouts are:
```
AST_ITEM_WORLD:  data[0]=name_idx, data[1]=state_count, then pairs of
                 (state_name_idx, state_type_idx, state_init_idx)... then
                 surface_count then pairs of (surface_kind_idx, surface_component_idx)

AST_ITEM_ENTANGLE: data[0]=name_idx, data[1]=left_world_idx, data[2]=left_field_idx,
                   data[3]=right_world_idx, data[4]=right_field_idx, data[5]=policy_idx
```

### Typechecker: STUBS — need real implementations

**`check_world_stub`** at `types.kn:1648-1656`:

```kain
pub fn check_world_stub(env: TypeEnv, node: AstNode, idx: Int) -> TypedItemAndEnv:
    let name_idx: Int = if ast_data_len(node) > 0: ast_data_get(node, 0) else: -1
    return TypedItemAndEnv {
        env: env,
        item: TypedItem {
            kind: AST_ITEM_WORLD, name: "w_" + str(name_idx), name_idx: name_idx,
            resolved_type: rt_struct_as(name_idx), ast_index: idx, effects: EFF_PURE,
        }
    }
```

Problems: (1) Does not resolve state field types. (2) Does not validate initializer expressions. (3) Does not enforce surface requirement. (4) Does not register world in any dedicated origin table.

**`check_entangle_stub`** at `types.kn:1658-1666`:

```kain
pub fn check_entangle_stub(env: TypeEnv, node: AstNode, idx: Int) -> TypedItemAndEnv:
    let name_idx: Int = if ast_data_len(node) > 0: ast_data_get(node, 0) else: -1
    return TypedItemAndEnv {
        env: env,
        item: TypedItem {
            kind: AST_ITEM_ENTANGLE, name: "ent_" + str(name_idx), name_idx: name_idx,
            resolved_type: rt_unit(), ast_index: idx, effects: EFF_PURE,
        }
    }
```

Problems: (1) Does not validate endpoint paths exist as worlds. (2) Does not type-check the two endpoints match. (3) Does not deduplicate endpoints. (4) Does not register in any entangle tracking structure.

### Codegen: MISSING — world/entangle items are silently dropped

At `codegen.kn:641-646`, only `AST_ITEM_FUNCTION` is compiled:

```kain
var i: Int = 0
while i < len(program.items):
    let item: TypedItem = program.items[i]
    if item.kind == AST_ITEM_FUNCTION:
        gen = compile_function_textual(gen, item, program.ast_nodes)
    i = i + 1
```

World, entangle, patch, and law items are iterated but produce no code. The struct definition emitter (`emit_struct_defs_from_program`) at line 667 handles `AST_ITEM_STRUCT` and `AST_ITEM_ENUM` but NOT `AST_ITEM_WORLD`.

The world struct type IS registered in `pass1_predeclare` (types.kn:797-799) as a `rt_struct_as(name_idx)` with key `"w_" + str(name_idx)`, but this is a bare struct — no init guard, no surface metadata, no entangle propagation.

### Compiler.kn routing: STUB

`driver_session_compile` at `compiler.kn` calls `typecheck` → `monomorphize` → `codegen_textual`. The pipeline passes TypedItems through correctly, but since both typecheck and codegen are stubs, no L1 semantics are emitted.

### Pass1 predeclare: CORRECT for world name registration

At `types.kn:797-799`:

```kain
elif kind == AST_ITEM_WORLD or kind == AST_ITEM_ACTOR or kind == AST_ITEM_COMPONENT:
    let name_idx: Int = ast_data_get(node, 0)
    e = declare_named_type(e, "w_" + str(name_idx), rt_struct_as(name_idx))
```

This is correct — world names are registered as struct-like types so that `WorldName.field` dotted access resolves through struct field lookup. The issue is that no fields are registered in pass2.

---

## 2. What Needs to Happen

### 2.1 Real `check_world` in types.kn

Replace `check_world_stub` with a real function that:

**(A) Resolve state slots.** Walk the world AST node data. For each `state <name>: <Type> = <init_expr>`:
- Resolve `<Type>` via `resolve_type_in_env()`
- Infer `<init_expr>` type via `infer_expr_type()`
- Call `ensure_type_compatible()` to check the initializer matches the declared type
- Register the field name + type in the type environment so `WorldName.field` dotted access resolves

**(B) Validate surface requirement.** Count the surface declarations in the AST node. If zero, push a diagnostic: `"world 'X' must declare at least one surface"`. Surface kind must be one of `native_ui`, `viewport3d`, `web`, `ue5`.

**(C) Register world as a typed entity.** The world should be:
- A `rt_struct_as(name_idx)` in the type registry (already done in pass1)
- A global origin entry so `WorldName.field` path resolution works during expression inference
- Registered in the env's `val_names`/`val_types` as an immutable binding (so field read `WorldName.field` resolves via `lookup_var("w_" + name_idx)` then struct field access)

**(D) Track world metadata.** Add to `TypeEnv`:
- `world_names: Array<String>` — parallel list of declared world names
- `world_field_counts: Array<Int>` — field count per world
- `world_surface_kinds: Array<Array<Int>>` — surface kinds per world

Or simpler: add `in_patch: Bool` (already exists), `in_world: Int` (-1 = not in world context), and a `world_fields: Array<Array<(String, ResolvedType)>>` list.

**Simplest approach for Phase 1:** Add `world_flags: Array<(String, Array<ResolvedType>, Array<String>)>` — (name, field_types, field_names). This is enough to validate field access and entangle endpoint types.

**(E) Effects.** World typechecking is `with Pure` — no effects are consumed during world declaration. The `check_world` function itself should return a `TypedItem` with `effects: EFF_PURE`.

### 2.2 Real `check_entangle` in types.kn

Replace `check_entangle_stub` with a function that:

**(A) Validate endpoint paths.** Both `left` and `right` endpoints must be dotted paths with exactly 2 segments: `WorldName.field`. The world must be registered (from pass1 or from an already-checked world item). The field must exist in that world's state slots, and its type must be resolved.

**(B) Type-match endpoints.** Both endpoint types must match. Use `types_compatible()` or a simpler structural check. For Phase 1, restrict to `Int` ↔ `Int` (the dominant pattern) with a note that other types can be added.

**(C) Deduplicate endpoints.** No endpoint can participate in more than one entangle. Add tracking (e.g., `entangle_endpoint_set: Array<String>` in TypeEnv, or simpler: check against already-registered entangle items).

**(D) Register in entangle metadata.** Add to `TypeEnv`:
```
entangle_left_auth:   Array<String>  — "WorldName.field" per entangle
entangle_right_mirror: Array<String> — "WorldName.field" per entangle
entangle_policies:     Array<Int>    — 0 = single_writer
```
These are consumed by codegen to emit the LLVM propagation calls.

**(E)** Return `TypedItem` with `resolved_type: rt_unit()` and `effects: EFF_PURE`. Entangle is a declaration, not a callable.

### 2.3 `compile_world_globals` in codegen.kn

New function `compile_world_globals(gen, program) → LlvmGenerator` called after const globals. For each `AST_ITEM_WORLD`:

**(A) Emit LLVM struct type.** The world becomes a named LLVM struct type containing all state fields. Unlike regular structs which use `%struct_N`, worlds use `%world_Name`:

```llvm
%world_RatTelemetry = type { i64, i64, i64, i64, i64, i64 }
```

**(B) Emit zero-initialized global.**

```llvm
@__kain_world_RatTelemetry = global %world_RatTelemetry zeroinitializer
```

**(C) Emit init flag + init function.**

```llvm
@__kain_world_init_flag_RatTelemetry = global i1 0

define void @__kain_init_world_RatTelemetry() {
entry:
  %0 = load i1, ptr @__kain_world_init_flag_RatTelemetry
  br i1 %0, label %already_init, label %init
init:
  %1 = getelementptr inbounds %world_RatTelemetry, ptr @__kain_world_RatTelemetry, i32 0, i32 0
  store i64 0, ptr %1
  ; ... GEP + store for each state field with its initial value ...
  store i1 1, ptr @__kain_world_init_flag_RatTelemetry
  br label %already_init
already_init:
  ret void
}
```

**(D) Emit preamble call.** At program start (in `codegen_textual`), emit a call to `__kain_init_*` for each world, guarded by `runtime_init()`.

**(E) Register world globals.** Maintain a `world_globals:` dict-like structure (parallel arrays: `world_names: Array<String>`, `world_init_fns: Array<String>`) so that `WorldName.field` access can emit `call void @__kain_init_world_X()` before GEP + load.

### 2.4 `compile_entangle` in codegen.kn

New function `compile_entangle(gen, program) → LlvmGenerator`. For each `AST_ITEM_ENTANGLE`:

**(A) Emit entangle registration at module level.** After struct defs but before function defs, emit calls to `abi_entangle_register`:

```llvm
%0 = call i64 @abi_entangle_register(
  i8* getelementptr inbounds ([...], ptr @".ent_0.left", i64 0, i64 0),
  i8* getelementptr inbounds ([...], ptr @".ent_0.right", i64 0, i64 0),
  i8* getelementptr inbounds ([...], ptr @".ent_0.policy", i64 0, i64 0),
  i8* getelementptr inbounds ([...], ptr @".ent_0.type", i64 0, i64 0))
```

The string constants (`@".ent_0.left"`, etc.) should be emitted as private global strings.

**(B) Emit propagation after world field writes.** This is the trickiest part. After every `store` to a world field that is an authority endpoint, emit:

```llvm
; load old value (before store) from backup
%ent_status = call i64 @abi_entangle_record_i64(
  i8* getelementptr inbounds ([...], ptr @".ent_0.left", i64 0, i64 0),
  i8* getelementptr inbounds ([...], ptr @".ent_0.right", i64 0, i64 0),
  i64 %new_val)
```

For Phase 1, simplify: emit entangle propagation as a separate function `@__kain_entangle_propagate_N()` that is called after any assignment to an entangled field. The codegen discovers which fields are authority endpoints by scanning the entangle metadata.

**(C) Emit mirror write guard.** The compile-time typechecker prevents mirror writes. The codegen does NOT need a runtime guard — mirrors cannot be written because the typechecker rejects `Mirror.field = value` in function bodies. If the typechecker catches this, the IR is never generated.

### 2.5 Routing in compiler.kn

The routing is already partially wired. The changes needed:

**(A) `codegen_textual`** — add dispatch for world/entangle items:

```kain
// After const globals, before function defs:
gen = emit_world_type_defs(gen, program)
gen = emit_world_globals(gen, program)
gen = emit_entangle_registration(gen, program)

// In the function loop:
while i < len(program.items):
    let item: TypedItem = program.items[i]
    if item.kind == AST_ITEM_FUNCTION:
        gen = compile_function_textual(gen, item, program.ast_nodes)
    elif item.kind == AST_ITEM_PATCH or item.kind == AST_ITEM_LAW:
        gen = compile_patch_law_textual(gen, item, program.ast_nodes)
    i = i + 1
```

**(B) `monomorphize`** — world and entangle items pass through without monomorphization. The `monomorphize.kn` already handles this correctly (non-generic items pass through).

---

## 3. Edge Cases

### 3.1 Single-writer Enforcement

The single_writer policy must be enforced at **compile time** (typechecker), not runtime:

- When `entangle A.x <-> B.y with single_writer` is registered, `B.y` is marked as a mirror
- Any `B.y = value` assignment in a `fn` or `patch` body must be rejected with diagnostic: `"cannot write to entangled mirror field 'B.y'; write to authority 'A.x' instead"`
- Implementation: track mirror fields in a set `mirror_fields: Array<(String, String)>` — (world_name, field_name). During expression typechecking for `AST_EXPR_ASSIGN`, check if the LHS is a mirror field. If so, error.

### 3.2 Surface -> Component Binding

The `surface native_ui => ComponentName` syntax is parsed but not validated. For Phase 1:
- Validate that the surface expression is a valid identifier (component name)
- Do NOT attempt to resolve the component (components are higher-layer constructs)
- Store surface metadata in the type environment for future use
- Error if `surface_count == 0`

### 3.3 Dual-World Authority+Mirror Pattern

The most common pattern pairs two worlds with matching fields:

```kain
world Authority: state signal: Int = 0  surface native_ui => Panel
world Mirror:    state signal_copy: Int = 0  surface web => Panel
entangle Authority.signal <-> Mirror.signal_copy with single_writer
```

The typechecker must ensure:
- `Authority.signal` and `Mirror.signal_copy` have compatible types (both `Int`)
- Neither endpoint is already entangled
- The mirror world is not used as an LHS in any patch or fn body

### 3.4 World Field Dotted Access

`WorldName.field` in expressions must resolve:
1. Look up `WorldName` in declared worlds → find its struct type
2. Look up `field` in that world's field list → get field type
3. Emit `call @__kain_init_world_X()` + `GEP` + `load`

This requires:
- `lookup_var` or `lookup_type` for world names during expression inference
- Struct field access codegen to handle world-specific init function calls

### 3.5 Multiple Worlds + Selectively Entangled Fields

Some world fields should NOT be entangled (e.g., raw `ptr<Int>` buffers). The typechecker should silently allow this — an endpoint that is never referenced by any `entangle` is just a normal world field with no propagation.

---

## 4. Files to Modify

| File | Changes | Priority |
|------|---------|----------|
| `src/types.kn` | Replace `check_world_stub` with `check_world` (resolve slots, validate surfaces) | P1 |
| `src/types.kn` | Replace `check_entangle_stub` with `check_entangle` (validate endpoints, type-match, dedupe) | P1 |
| `src/types.kn` | Add `mirror_fields` tracking to TypeEnv for single-writer enforcement | P1 |
| `src/types.kn` | Add `infer_field_access_type()` for `WorldName.field` dotted path resolution | P1 |
| `src/types.kn` | Add entangle metadata arrays to TypeEnv | P1 |
| `src/codegen.kn` | Add `emit_world_type_defs` — LLVM struct types for worlds | P1 |
| `src/codegen.kn` | Add `emit_world_globals` — LLVM globals + init flags + init fn | P1 |
| `src/codegen.kn` | Add `emit_entangle_registration` — abi_entangle_register calls | P1 |
| `src/codegen.kn` | Add `compile_world_field_access` — init guard + GEP + load for world field reads | P1 |
| `src/codegen.kn` | Add `compile_world_field_write` — store + entangle propagation for world field writes | P1 |
| `src/codegen.kn` | Wire world/entangle dispatch in `codegen_textual` | P1 |
| `src/compiler.kn` | No changes needed — routing already passes TypedItems through | P0 |

---

## 5. Acceptance Criteria

1. `kain check world_decl.kn` where `world_decl.kn` has the minimal dual-world pattern passes with no diagnostics
2. `kain check world_no_surface.kn` where a world has no surfaces produces diagnostic `"world 'X' must declare at least one surface"`
3. `kain check entangle_type_mismatch.kn` where entangled fields have different types produces diagnostic
4. `kain check entangle_duplicate.kn` where the same field appears in two entangles produces diagnostic
5. `kain check entangle_write_mirror.kn` where code assigns to a mirror field produces diagnostic
6. `kain check world_field_access.kn` where `WorldName.field` is read in a function produces correct LLVM: init function call + GEP + load
7. `kain check world_entangle.kn` produces LLVM with `@__kain_world_*` globals and `abi_entangle_register` calls
