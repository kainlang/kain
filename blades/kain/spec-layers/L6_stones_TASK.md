# L6 Machine Stones — axiom + shatter + teleport Implementation Tasks

**Date:** 2026-06-12
**Source spec:** L6_stones.md
**Target files:** `src/parser.kn`, `src/types.kn`, `src/codegen.kn`, `src/runtime.kn`, `src/ast.kn`

---

## Summary

Implement axiom (machine truth with fallback), shatter (SoA layout intent), and teleport (zero-copy cross-world handoff) in the self-host compiler. Axiom is standalone; shatter depends on L0 struct; teleport depends on L1 world resolution.

Current state: axiom and shatter have parser dispatch and AST nodes. Teleport has parser expression parsing (`parse_teleport_expr` via `parse_expr` router). Typechecker stubs return hardcoded types (axiom → `rt_unit()`, teleport → `rt_unit()`). Codegen has no emission for any of the three.

---

## Phase 1: Parser Completion

### P-AXM-01 — Complete axiom predicate parsing (HIGH)
**File:** `parser.kn`
**Details:** In `parse_axiom_item`, add predicate parsing loop. For each `when` keyword: parse predicate kind (`target`, `arch`, `capability`), expect `(` then string literal then `)`. Store as `(kind_encoding, value_string_idx)` in data. Kind encoding: 0=target, 1=arch, 2=capability. Reject unknown predicate names. Reject empty predicate values.

### P-AXM-02 — Complete axiom guarantee + fallback parsing (HIGH)
**File:** `parser.kn`
**Details:** After predicates, parse `guarantee` string literals (at least one). Parse `fallback` function name (exactly one). Reject duplicate fallback. Reject fallback with empty name. AST_ITEM_AXIOM data layout: `[name_idx, pred_count, (kind, val_idx)*, gtee_count, (gtee_idx)*, has_fallback, fallback_idx]`.

### P-AXM-03 — Predicate deduplication during parsing (MEDIUM)
**File:** `parser.kn`
**Details:** Track seen `(kind, value)` pairs during predicate parsing. If a duplicate is encountered, skip or emit a warning. This is a mild validation — the typechecker will catch duplicates as errors, but early dedup is cleaner.

### P-SHT-01 — Fix shatter attribute propagation (CRITICAL)
**File:** `parser.kn`, `ast.kn`
**Details:** In `parse_shatter_struct`, the contextual keyword `shatter` is consumed, but the `#[shatter]` attribute must be pushed into `attrs` before delegating to `parse_struct_with_attrs`. Add `AST_ATTR_SHATTER` constant to `ast.kn` (value = some high constant like 9000, outside existing range). Push `Attribute { name_idx: AST_ATTR_SHATTER, args: [] }` into the attrs array. Verify downstream consumers can detect the attribute on the emitted AST_ITEM_STRUCT node.

### P-TEL-01 — Verify teleport expression parsing (HIGH)
**File:** `parser.kn`
**Details:** Review existing `parse_teleport_expr` at line ~1327. Verify it correctly parses: teleport keyword → value expression → `from` → source world string → `to` → target world string → optional `via` channel string. Verify AST_EXPR_TELEPORT data layout is: `[value_idx, src_name_idx, tgt_name_idx, has_via_flag, via_idx]`. Ensure source/target are parsed as string-like arguments (quoted strings or bare identifiers that get string-table entries).

### P-TEL-02 — Empty channel validation (MEDIUM)
**File:** `parser.kn`
**Details:** If `via` is parsed and the channel string is empty, emit a parse error: "teleport channel cannot be empty".

---

## Phase 2: Typechecker Implementation

### T-AXM-01 — Complete axiom validation (HIGH)
**File:** `types.kn`
**Details:** Replace `check_axiom_stub` with `check_axiom(env, node, idx)`. Extract predicates, guarantees, fallback from AST data. Validate: at least 1 predicate (error `"axiom '{}' must declare at least one machine predicate"`), at least 1 guarantee (error `"axiom '{}' must declare at least one guarantee"`), fallback present and non-empty (error `"axiom '{}' must declare a portable fallback"`). Reject duplicate `(kind, value)` predicate pairs. Store axiom metadata in `TypedItem` for codegen consumption.

### T-AXM-02 — Predicate kind validation (HIGH)
**File:** `types.kn`
**Details:** Validate predicate kind integer is in {0, 1, 2} representing {target, arch, capability}. Any other integer is invalid. This guards against parser bugs producing malformed AST data.

### T-SHT-01 — Shatter attribute preservation (MEDIUM)
**File:** `types.kn`
**Details:** During struct typechecking, check for `AST_ATTR_SHATTER` attribute on the AST_ITEM_STRUCT node. If present, propagate the shatter marker to the `TypedItem`. The simplest approach: prefix the item name with `"shatter_"` so codegen can detect shattered structs by name prefix. Alternatively, add a dedicated field in `TypedItem` like `attributes: Array<Int>`.

### T-TEL-01 — Teleport full type inference (HIGH)
**File:** `types.kn`
**Details:** Replace the `rt_unit()` stub in the teleport case (`AST_EXPR_TELEPORT`) with full inference. Implement `infer_teleport_type(env, data, ctx)`. Steps:
1. Extract value_idx from data[0], src_name_idx from data[1], tgt_name_idx from data[2]
2. Infer value expression type via `infer_expr_from_node`
3. Validate source world exists in type environment
4. Validate target world exists in type environment
5. Validate source != target
6. If `via` present (data[3] == 1), validate channel string at data[4] is non-empty
7. If value expression is an identifier, call `env.mark_moved(name)` to prevent post-teleport access
8. Return value_type (teleport preserves the type)

### T-TEL-02 — World existence table (HIGH)
**File:** `types.kn`
**Details:** The typechecker environment needs a way to look up declared world names. Add a `world_names: Array<String>` or `worlds: HashMap<String, Int>` to the TypeEnv struct (or reuse the existing type-table if worlds register as structs). Worlds are registered by name during the predeclare/register passes. Implement `env.world_exists(name: String) -> Bool` that checks whether a world with the given name has been declared.

### T-TEL-03 — Move semantic implementation (HIGH)
**File:** `types.kn`
**Details:** Implement `env.mark_moved(name: String)` which adds `name` to a `moved: HashMap<String, Bool>` set in the type environment. In `resolve_ident`, before returning the type, check if the identifier is in the moved set. If so, emit error: `"Identifier '{}' was moved by teleport and cannot be used again"`. The moved set must be scoped to the current function body — identifiers outside the teleport expression's scope are unaffected.

---

## Phase 3: Codegen Implementation

### C-AXM-01 — Axiom accept function emission (HIGH)
**File:** `codegen.kn`
**Details:** For each typed axiom, emit `define i64 @__kain_axiom_accept_{sanitized_name}()` function. Collect target string, arch string, and capability bitmask from the axiom's predicates. Call `@kain_machine_axiom_accept(i8* target_str, i8* arch_str, i64 cap_mask)`. Return the result. Emit static string constants for target and arch.

### C-AXM-02 — capability_bit mapping (HIGH)
**File:** `codegen.kn`
**Details:** Implement `fn capability_bit(name: String) -> Int` with this mapping:
- `"atomic.bitmask"` → 0x00000001
- `"time.pulse"` → 0x00000002
- `"memory.shatter"` → 0x00000004
- `"world.teleport"` → 0x00000008
- `"cpu.x86.sse2"` → 0x00000010
- `"cpu.x86.avx"` → 0x00000020
- `"cpu.x86.avx2"` → 0x00000040
- `"cpu.x86.avx512f"` → 0x00000080
- Unknown → 0x00000000 (no capability match)
Capability masks from multiple predicates are OR'd together.

### C-AXM-03 — Static string emission helper (MEDIUM)
**File:** `codegen.kn`
**Details:** Implement or reuse `fn emit_static_string_const(name: String, value: String) -> String` that emits LLVM IR global constant: `@".str.{name}" = private unnamed_addr constant [{len} x i8] c"{value}\00"`. Used for axiom target/arch strings, teleport world names and channels.

### C-SHT-01 — Track shattered structs in codegen init (HIGH)
**File:** `codegen.kn`
**Details:** During codegen initialization (when scanning typed items), check each struct item for the shatter marker. Collect into `shattered_struct_names: Array<String>`. This list drives SoA codegen decisions.

### C-SHT-02 — Shattered array literal allocation (HIGH)
**File:** `codegen.kn`
**Details:** When lowering an array literal of a shattered struct type: compute lane_count = number of struct fields, element_count = number of array elements. Emit calls:
1. `%handle = call i8* @kain_machine_shatter_alloc(i64 lane_count, i64 element_count)`
2. For each lane (field), call `%lane_base = call i8* @kain_machine_shatter_lane_base(i8* %handle, i64 lane_idx)`
3. Populate each lane element-by-element using GEP + store

### C-SHT-03 — Shattered field access lowering (HIGH)
**File:** `codegen.kn`
**Details:** When `array[index].field` is accessed on a shattered array: look up the field index in the struct's field list. If index is a compile-time constant, emit GEP on lane_base: `%ptr = getelementptr inbounds i8, i8* %lane_base, i64 {index * 8}` then bitcast to field type. If index is runtime, emit call: `%ptr = call i8* @kain_machine_shatter_lane_ptr(i8* %handle, i64 field_idx, i64 element_idx)`.

### C-SHT-04 — Shattered scope cleanup (MEDIUM)
**File:** `codegen.kn`
**Details:** For heap-allocated shattered arrays (not stack-temporary), emit `call void @kain_machine_shatter_free(i8* %handle)` on scope exit. Track allocated handles in a scope stack during codegen.

### C-TEL-01 — Pointer-type teleport emission (HIGH)
**File:** `codegen.kn`
**Details:** When lowering AST_EXPR_TELEPORT with a pointer-type value (struct, boxed, heap-allocated): compile value to get LLVM register, bitcast to i8*, emit `%handoff = call i8* @kain_machine_teleport_ptr(i8* %raw_ptr, i8* @".str.{src}", i8* @".str.{tgt}", i8* @".str.{channel}")`, bitcast result back to value type. The result is the teleport expression's value.

### C-TEL-02 — Scalar-type teleport emission (HIGH)
**File:** `codegen.kn`
**Details:** For non-pointer types (Int, Bool, etc.): compile value normally, emit `call void @kain_machine_teleport_note(i8* @".str.{src}", i8* @".str.{tgt}", i8* @".str.{channel}")` for bookkeeping. The value is unchanged — teleport is a semantic no-op for scalars (they pass by value).

### C-TEL-03 — Teleport string constants (MEDIUM)
**File:** `codegen.kn`
**Details:** For each unique source_world, target_world, and channel name across all teleport expressions, emit static string constants using the `emit_static_string_const` helper. Reuse existing constants when the same string appears in multiple teleports.

---

## Phase 4: Runtime Contract & Declares

### R-AXM-01 — Axiom runtime declares (MEDIUM)
**File:** `runtime.kn`
**Details:** Verify `kain_machine_axiom_accept(i8*, i8*, i64) -> i64` is declared. Add if missing. The axiom codegen also needs `kain_machine_axiom_check(i8*) -> i1` if used.

### R-SHT-01 — Shatter runtime declares (MEDIUM)
**File:** `runtime.kn`
**Details:** Verify all four shatter runtime functions are declared:
- `kain_machine_shatter_alloc(i64, i64) -> i8*`
- `kain_machine_shatter_lane_ptr(i8*, i64, i64) -> i8*`
- `kain_machine_shatter_lane_base(i8*, i64) -> i8*`
- `kain_machine_shatter_free(i8*) -> void`

### R-TEL-01 — Teleport runtime declares (MEDIUM)
**File:** `runtime.kn`
**Details:** Verify all three teleport runtime functions are declared:
- `kain_machine_teleport_ptr(i8*, i8*, i8*, i8*) -> i8*`
- `kain_machine_teleport_note(i8*, i8*, i8*) -> void`
- `kain_machine_teleport_count() -> i64`

---

## Phase 5: Verification

### V-AXM-01 — Axiom typecheck tests (MEDIUM)
**File:** `tests/`
**Details:** Add test cases: axiom minimal valid, all predicates combined, no predicates (error), no guarantees (error), no fallback (error), duplicate predicate (error), capability bitmask accumulation across multiple `when capability(...)` lines.

### V-AXM-02 — Axiom codegen tests (MEDIUM)
**File:** `codegen.kn` test output matching
**Details:** Verify axiom emits correct accept function, static string constants for target/arch, correct capability bitmask OR logic, and call to `kain_machine_axiom_accept`.

### V-SHT-01 — Shatter struct tests (MEDIUM)
**File:** `tests/`
**Details:** Add tests: shatter struct minimal, multi-field, with Bool field, array literal of shattered struct. Verify shatter attribute is preserved through typechecking to codegen.

### V-SHT-02 — Shatter codegen tests (MEDIUM)
**File:** `codegen.kn` test output matching
**Details:** Verify shatter array literal emits alloc call, lane base calls for each field, field population stores. Verify field access emits GEP for compile-time index or lane_ptr call for runtime index.

### V-TEL-01 — Teleport typecheck tests (MEDIUM)
**File:** `tests/`
**Details:** Add tests: teleport minimal, with channel, bad source world (error), same world (error), empty channel (error), moved-identifier re-use (error), scalar teleport (ok), struct teleport (ok).

### V-TEL-02 — Teleport codegen tests (MEDIUM)
**File:** `codegen.kn` test output matching
**Details:** Verify pointer-type teleport emits `kain_machine_teleport_ptr` with correct bitcast chain. Verify scalar teleport emits `kain_machine_teleport_note`. Verify static string constants for world names and channels.
