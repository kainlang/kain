# Stream RED: Typechecker Completion

**Stream ID:** RED
**Role:** Complete all L0 item checking and expression type inference in types.kn. Make the typechecker fully real for Layer 0.
**Effort:** 2-3 weeks
**Depends On:** none
**Requirements Covered:** FR-typecheck, FR-infer, FR-compat, FR-struct-check, FR-enum-check, FR-trait-check, FR-mono
**Design Reference:** types.kn — 4-pass pipeline, 20 ResolvedType variants, TypeEnv

---

## Context

The typechecker is at ~75% real for Layer 0. `check_function_item` is real (parameter binding, body checking, return type unification). `check_const_item` and `check_type_alias_item` are real. `check_struct_item` resolves field types but doesn't detect duplicates or register fields in env. `check_enum_item` is a stub (just returns TypedItem). `check_trait_impl_item` is a stub. `check_block_body` walks statements but several statement kinds are stubbed. `infer_expr_type` handles ~35 of 64 expression kinds.

The 4-pass pipeline architecture is correct but Pass 4 doesn't properly accumulate state (the `TypeEnv` isn't threaded through all items). This stream fixes that.

**CRITICAL**: Every `check_*` function must return a NEW `TypeEnv` (for mutable state threading) or `check_item` must be rewritten to return `(TypeEnv, TypedItem)` so pass4 can thread state through all items.

---

## Files You Own

### Files to Modify

| File | Region/Function | Change Description |
|------|-----------------|--------------------|
| `X:/blades/kain/src/types.kn` | `check_struct_item` (line 1262-1290) | Make real: resolve all field types, detect duplicate field names, register struct type with field map in env |
| `X:/blades/kain/src/types.kn` | `check_enum_item` (line 1292-1303) | Make real: resolve all variant payload types, register enum variants in env |
| `X:/blades/kain/src/types.kn` | `check_trait_impl_item` (line 1375-1399) | Make real: for trait items — register methods. For impl items — verify all required trait methods exist, check each against trait signature. For inherent impls — check each method as fn. |
| `X:/blades/kain/src/types.kn` | `check_item` (line 840-878) | Rewrite return type from TypedItem to `(TypeEnv, TypedItem)` so state threads through pass4 |
| `X:/blades/kain/src/types.kn` | `typecheck` / `pass4_check` (line 733-837) | Thread env through all check_item calls in pass4 loop |
| `X:/blades/kain/src/types.kn` | `infer_expr_type` (line 1613-1873) | Complete for remaining 29 expression kinds |
| `X:/blades/kain/src/types.kn` | `check_block_body` (line 1002-1062) | Add checking for match, loop, break, continue, defer full validation |
| `X:/blades/kain/src/types.kn` | `types_compatible` (line 567-730) | Add remaining cases: generic-to-concrete, function structural, struct-to-trait |
| `X:/blades/kain/src/monomorphize.kn` | `instantiate_generic` | Complete the generic instantiation loop with substitution |

### Files You Must NOT Touch

| File | Reason |
|------|--------|
| `X:/blades/kain/src/codegen.kn` | Owned by Stream BLUE |
| `X:/blades/kain/src/orchestrator.kn` | Owned by Stream GREEN |
| `X:/blades/kain/src/compiler.kn` | Owned by Stream GREEN |
| `X:/blades/kain/src/parser.kn` | Parser is done — do not modify |
| `X:/blades/kain/src/lexer.kn` | Lexer is done — do not modify |

---

## Implementation Tasks

### RED-1: Thread TypeEnv Through check_item

**Effort:** 1 day
**Objective:** Make pass4 properly accumulate typechecker state across all items.

**Current problem:** `check_item(env, node, idx) -> TypedItem` takes env but doesn't return it. Any mutations to `env` inside check_item are lost after the function returns. This means struct field registrations, function signatures registered in all_types, etc. don't persist.

**Implementation Steps:**

1. Change `check_item` signature from:
   ```
   pub fn check_item(env: TypeEnv, node: AstNode, idx: Int) -> TypedItem:
   ```
   to:
   ```
   pub fn check_item(env: TypeEnv, node: AstNode, idx: Int) -> TypedItemAndEnv:
   ```
   where `TypedItemAndEnv` is a new struct:
   ```
   pub struct TypedItemAndEnv:
       env: TypeEnv
       item: TypedItem
   ```

2. Update every `check_*` function to return `TypedItemAndEnv` instead of `TypedItem`:
   - `check_function_item` → returns env with function type registered
   - `check_struct_item` → returns env with struct type + fields registered
   - `check_enum_item` → returns env with enum variants registered
   - etc.

3. Rewrite `pass4_check` to thread env through all items:
   ```
   pub fn pass4_check(env: TypeEnv, program: AstProgram, skip: Array<Bool>) -> TypedProgram:
       let mut e: TypeEnv = env
       let mut items: Array<TypedItem> = []
       ...
       while i < n:
           if !skip[i]:
               let result: TypedItemAndEnv = check_item(e, nodes[i], i)
               e = result.env
               items.push(result.item)
           i = i + 1
       ...
   ```

4. Verify: after pass4, `e.all_types` should contain all function types, struct types, etc. (not just primitives).

**Acceptance Criteria:**
- [ ] All check_* functions return TypedItemAndEnv
- [ ] pass4_check threads env through the loop
- [ ] After check_item for a struct, the env's all_types contains the struct type
- [ ] After check_item for a function, the env's all_types contains the function type
- [ ] `kain check src/types.kn` passes

---

### RED-2: Complete check_struct_item

**Effort:** 1 day
**Objective:** Make struct field checking fully real.

**Implementation Steps:**

1. Parse struct AST layout (from parser.kn parse_struct):
   ```
   data[0] = name_idx
   data[1] = generic_count; data[2..2+2*gc] = (gname, gbound) pairs
   fc_pos = 2 + 2*gc
   data[fc_pos] = field_count
   data[fc_pos+1 .. fc_pos+1+2*fc] = (field_name, field_type_ast) pairs
   ```

2. Resolve every field type via `resolve_type_in_env`
3. Detect duplicate field names: track seen field name indices, report ERR_DUPLICATE_FIELD if found
4. Build a field map: `Array<FieldInfo>` where each `FieldInfo = {name_idx, type_idx}`. Register this in env so codegen can use it.
5. Register the struct type in `e.all_types` with `rt_struct_as(name_idx)` and set `struct_field_count` and `struct_field_types` on the ResolvedType.
6. Handle generic structs: if `gc > 0`, register as a generic struct template.

**Acceptance Criteria:**
- [ ] Field types all resolved via resolve_type_in_env
- [ ] Duplicate field names reported as errors
- [ ] Struct type registered in env.all_types with field info
- [ ] Generic structs register with generic parameters
- [ ] `kain check src/token.kn` → LexerState struct typechecks correctly

---

### RED-3: Complete check_enum_item

**Effort:** 1 day
**Objective:** Make enum variant checking real.

**Implementation Steps:**

1. Parse enum AST layout:
   ```
   data[0] = name_idx
   data[1] = generic_count; data[2..2+2*gc] = (gname, gbound) pairs  
   vc_pos = 2 + 2*gc
   data[vc_pos] = variant_count
   For each variant: data[...] = variant_name_idx, data[...+1] = has_payload, if has_payload: data[...+2] = payload_type_ast
   ```

2. Resolve every variant's payload type via `resolve_type_in_env`
3. Register enum variants in env: create variant entries with name + optional payload type
4. Register the enum type in `e.all_types`

**Acceptance Criteria:**
- [ ] All variant types resolved
- [ ] Enum type registered in env
- [ ] `kain check src/parser.kn` → AST_ITEM_* constants typecheck as referred enum

---

### RED-4: Complete check_trait_impl_item

**Effort:** 2 days
**Objective:** Make trait and impl typechecking real.

**Implementation Steps:**

1. Parse trait AST layout:
   ```
   data[0] = name_idx
   data[1] = generic_count
   data[2] = method_count
   then: (method_name, method_body as AST fn node) pairs
   ```

2. Parse impl AST layout:
   ```
   data[0] = type_name_idx (struct being implemented)
   data[1] = trait_name_idx (-1 for inherent impl)
   data[2] = generic_count
   data[3] = method_count
   then: (method_name, method_body as AST fn node) pairs
   ```

3. For trait items (node.kind == AST_ITEM_TRAIT):
   - Register each method with its signature in env
   - Each method body checked as a function

4. For impl items (node.kind == AST_ITEM_IMPL):
   - If trait_name_idx >= 0: look up the trait, verify ALL required methods exist in impl
   - For each impl method: find the corresponding trait method, check signature compatibility (param types compatible, return type compatible)
   - If trait_name_idx == -1 (inherent impl): check each method as a standalone function bound to `Self`
   - Check for duplicate methods

5. Register the trait-to-impl mapping in env so method resolution works

**Acceptance Criteria:**
- [ ] Trait methods registered with signatures
- [ ] Impl verifies all trait methods present
- [ ] Method signatures checked for compatibility
- [ ] Inherent impls check each method
- [ ] `kain check src/types.kn` → SmokePacket impl SmokeFold typechecks

---

### RED-5: Complete infer_expr_type for remaining expression kinds

**Effort:** 3 days
**Objective:** Make expression type inference handle all 64 expression kinds used by the compiler's own source.

**Currently handled (~35 kinds):** literals (Int, Float, Bool, String, None, Char), ident, binary, unary, call, if, block, struct lit, field, assign, ref, deref, cast, paren, return, let, while, index, array lit, tuple, enum variant, match (partial)

**Remaining kinds to implement (~29 kinds):**

1. **AST_EXPR_MATCH** — pattern matching. For each arm, infer pattern-bound variable types, unify arm body types with phi.

2. **AST_EXPR_LAMBDA** / **AST_EXPR_CLOSURE** — anonymous function type. Build function type from captured vars + param types + return type.

3. **AST_EXPR_METHOD_CALL** — `obj.method(args)`. Look up method on obj's type, check arg compatibility.

4. **AST_EXPR_INDEX** — `arr[i]`. If arr is Array<T>, return T. If arr is ptr<T>, return T.

5. **AST_EXPR_ARRAY_LIT** — `[a, b, c]`. If all elements have same type T, return Array<T>. If mixed, find common supertype.

6. **AST_EXPR_TUPLE_LIT** — `(a, b)`. Return Tuple of element types.

7. **AST_EXPR_ENUM_VARIANT** — `MyEnum::Variant(payload)`. Look up enum, find variant, check payload type.

8. **AST_EXPR_RANGE** — `a..b`. Return Range<Int> always.

9. **AST_EXPR_SPAWN** — `spawn Actor(args)`. Look up actor, verify args match state fields. Return actor handle type.

10. **AST_EXPR_SEND** — `send actor.Msg(args)`. Look up actor type, find message handler, check arg compatibility. Return unit.

11. **AST_EXPR_TELEPORT** — `teleport val from WorldA to WorldB via bus`. Check value type matches bus. Return moved value type.

12. **AST_EXPR_AND** / **AST_EXPR_OR** — logical short-circuit. Both arms must be Bool, result is Bool.

13. **AST_EXPR_LOOP** — type of break value (from break statement in body).

14. **AST_EXPR_BREAK** — return type Never, but with break_value for loop result.

15. **AST_EXPR_CONTINUE** — type Never.

16. **AST_EXPR_TRY** — `expr?`. If expr is Option<T>, unwrap to T. If Result<T,E>, unwrap to T.

17. **AST_EXPR_AWAIT** — `await fut`. If fut is Future<T>, return T.

18. **AST_EXPR_CAST_AS** — `expr as Type`. Return TargetType. Check that cast is valid.

19. **AST_EXPR_COLLAPSE** — `collapse ptr: body`. Body result type.

20. **AST_EXPR_OBSERVE** — `observe ptr: body`. Body result type.

21. **AST_EXPR_DECAY** — `decay ptr`. Return unit.

**Priority order:** Implement in order of usage by compiler source (most-used first):
- MATCH, INDEX, ARRAY_LIT, ENUM_VARIANT, METHOD_CALL (used by token.kn, lexer.kn, parser.kn)
- RANGE, TUPLE_LIT, AND, OR (used by lexer.kn for conditionals)
- LOOP, BREAK, CONTINUE (used by control flow)
- SPAN, SEND, AWAIT, TRY (used by actor/test code)
- COLLAPSE, OBSERVE, DECAY, TELEPORT (ownership code)
- LAMBDA, CLOSURE (rare in compiler source)

**Acceptance Criteria:**
- [ ] All 64 expression kinds handled (no fallthrough to `rt_unknown()`)
- [ ] Match inference correctly unifies arm types
- [ ] Method call resolves to correct method on type
- [ ] Array lit with mixed types finds common supertype
- [ ] `kain check src/parser.kn` → all expression types inferred (no Unknown fallbacks)

---

### RED-6: Complete check_block_body for match, loop, break, continue

**Effort:** 1 day
**Objective:** Make block body checking handle all statement kinds.

**Currently handled:** let, return, expr, while, for, if-expr, defer (partial), loop (stubbed), break/continue (no-op), dispatch (no-op), block.

**Implementation Steps:**

1. Implement `check_match_expr` — for each arm pattern, bind variables, check guard condition, check arm body type.
2. Implement `check_loop_stmt` — mark loop context, check body with break/continue awareness, check break value type.
3. Implement `check_break_stmt` — verify in loop context, check break value type against loop's expected type.
4. Implement `check_continue_stmt` — verify in loop context.
5. Implement `check_defer_stmt` completely — record defer expression, verify it's valid in current context.

**Acceptance Criteria:**
- [ ] Match arms all checked with pattern variable binding
- [ ] Loop context tracked for break/continue validation
- [ ] Break value type checked
- [ ] Continue outside loop reported as error
- [ ] `kain check src/parser.kn` → all control flow typechecks

---

### RED-7: Complete types_compatible for remaining cases

**Effort:** 1 day
**Objective:** Make the pairwise type compatibility function cover all 400 combinations.

**Currently covered:** Primitives, arrays, tuples, nominals, refs, ptrs, options, results, futures, functions (basic). Missing: generic-to-concrete substitution, struct-to-trait (for impl), Never/Unknown edges.

**Implementation Steps:**

1. Add generic substitution case: if expected is Generic(T) and actual is concrete, bind T→actual and return true.
2. Add struct-to-trait case: if expected is a trait and actual is a struct that impls that trait, return true (requires impl registry).
3. Add Never rules: Never is compatible with everything (never type is bottom).
4. Add function structural comparison: if both are Function, check param count, param types pairwise, return types.

**Acceptance Criteria:**
- [ ] Generic-to-concrete substitution works
- [ ] Struct-to-trait compatibility works
- [ ] Never compatible with every type
- [ ] Function types compared structurally

---

### RED-8: Complete Generic Monomorphization

**Effort:** 1 day
**Objective:** Make the generic instantiation loop actually generate concrete instances.

**Current state:** `monomorphize.kn` passes non-generic items through. `unify()` and `substitute_type()` exist. `has_generic_params()` works.

**Implementation Steps:**

1. In the monomorphize loop, after pass-through of non-generic items, scan for generic functions/structs that were instantiated.
2. For each instantiation: call `instantiate_generic()` which:
   - Creates a BindingMap from generic params → concrete types
   - Uses `substitute_type()` to replace all generic references in the function/struct body
   - Generates a mangled name (e.g., `fn_Array_push_Token` → `fn_Array_push_Int`)
   - Pushes the monomorphized copy into the TypedProgram items
3. Handle recursive instantiation: if a monomorphized function calls another generic function with the same concrete types, instantiate that one too.

**Acceptance Criteria:**
- [ ] `Array<Token>` usage in parser.kn → produces `Array_Int` specialization
- [ ] Mangled names are deterministic
- [ ] Recursive instantiation doesn't loop infinitely
- [ ] `kain check src/parser.kn` → no "unresolved generic" errors

---

## Stream Conventions

- **Language:** Kain (.kn files)
- **Naming:** snake_case for functions, PascalCase for structs, SCREAMING_CASE for constants
- **Error reporting:** Use `type_error(env, message, code, start, end) -> TypeEnv` helper
- **Error codes:** Define any new error constants at the top of types.kn
- **Type pattern:** Every check function returns `TypedItemAndEnv { env, item }`
- **Comments:** Mark new functions with `// ── Stream RED ──` so they're identifiable

---

## Stream Boundary — What You Do NOT Do

- ❌ Do NOT modify `codegen.kn`, `orchestrator.kn`, `compiler.kn`, `cli.kn`, `main.kn`
- ❌ Do NOT modify parser.kn or lexer.kn
- ❌ Do NOT touch L1-L7 stub functions (check_patch_law_stub, check_converge_stub, check_orchestrate_stub, check_world_stub, check_shader_stub, etc.) — leave them as stubs for GOLD
- ❌ Do NOT change the 4-pass pipeline architecture
- ❌ Do NOT change the ResolvedType variant set (20 constants)
- ❌ Do NOT add dependencies on external crates/libraries

---

## Verification (After This Stream)

After completing all tasks, verify:

```bash
# Build check — all files must typecheck
kain check X:\blades\kain\src\

# Specific acid tests
kain check X:\blades\kain\src\token.kn       # Const + type alias checking
kain check X:\blades\kain\src\lexer.kn       # Function body checking, struct fields
kain check X:\blades\kain\src\parser.kn      # All 64 expr kinds, match, generics
kain check X:\blades\kain\src\types.kn       # Self-check — typechecker checks itself!
kain check X:\blades\kain\src\codegen.kn     # All expression types checked
kain check X:\blades\kain\src\monomorphize.kn # Generic monomorphization
```

**Self-check:**
- [ ] All files created/modified as listed above
- [ ] All check functions return TypedItemAndEnv (not TypedItem)
- [ ] check_struct_item detects duplicate fields
- [ ] check_enum_item resolves variant payload types
- [ ] check_trait_impl_item verifies method signatures
- [ ] infer_expr_type handles all 64 expression kinds
- [ ] types_compatible handles generic→concrete and struct→trait cases
- [ ] Generic monomorphization produces concrete instances
- [ ] All 23 files pass `kain check` individually
- [ ] No files modified outside types.kn and monomorphize.kn

---

## Completion Report

When done, report:
- Files created: <list with line counts>
- Files modified: <list with changes summary>
- New functions added: <count>
- Functions made real (from stub): <count>
- Tests passing: `kain check src/` → N/23 files pass
- Any issues encountered: <list or "none">
- Anything the BLUE stream needs to know: <notes about TypedItem structure changes, field map format, etc. or "none">
