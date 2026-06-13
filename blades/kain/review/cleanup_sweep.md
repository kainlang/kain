# CLEANUP SWEEP REPORT — RED + GREEN

**Date:** 2026-06-12
**Sweep by:** Kain God Cleanup Sweep
**Streams audited:** RED (types.kn, monomorphize.kn) + GREEN (llvm_ffi.kn, compiler.kn, KAIN.toml)

---

## 1. Files Audited

| File | Lines | Status |
|------|-------|--------|
| `src/types.kn` | 2,201 | Modified (4 issues fixed) |
| `src/monomorphize.kn` | 460 | Clean (no fixes needed) |
| `src/llvm_ffi.kn` | ~650 | Modified (1 issue fixed) |
| `src/compiler.kn` | ~450 | Clean (no fixes needed) |
| `KAIN.toml` | 37 | Clean (no fixes needed) |

---

## 2. Issues Found and Fixed

### FIX 1 — llvm_ffi.kn: Duplicate constant check (Bug)
**Location:** `llvm_const()` function
**Problem:** Two identical `if name == "LLVM_ATTR_INDEX_FUNCTION"` checks — second was unreachable dead code.
**Fix:** Removed duplicate line.
**Risk:** Zero. Pure cleanup.

### FIX 2 — types.kn: `infer_block_type` ignored env (Gap)
**Location:** `infer_block_type()` → called from `infer_expr_type()`
**Problem:** Function signature was `(node: AstNode)` with no `env` parameter. Always returned `rt_i64()` for the block's trailing expression — couldn't actually infer the type.
**Fix:** Changed signature to `(env: TypeEnv, node: AstNode)`. Now looks up the last child in the block node, checks it's an expression (not a statement kind), and returns `infer_expr_type(env, last_child)`. Returns `rt_unit()` for blocks ending in statements.
**Impact:** Block expressions now return the type of their trailing expression instead of always `Int`. Critical for correct type inference in expression-position blocks.

### FIX 3 — types.kn: `check_block_body` didn't validate trailing expr (Gap)
**Location:** `check_block_body()` 
**Problem:** The `else` branch caught non-statement children and inferred their types, but never checked the LAST expression against `expected_ret`. Blocks used as function bodies had no return type validation.
**Fix:** Added `last_expr_type` / `has_last_expr` tracking. Resets `has_last_expr` at the top of each loop iteration (only `else` branch sets it). After the loop, validates `last_expr_type` against `expected_ret` using `types_compatible`. Emits `ERR_TYPE_MISMATCH` on mismatch.
**Impact:** Function bodies now validate that the trailing expression type matches the declared return type.

### FIX 4 — types.kn: Effect mask mapping (Bug)
**Location:** `check_function_item()` effect loop
**Problem:** The parser stores effect indices as sequential integers (0=Pure, 1=IO, 2=Async, 3=GPU, 4=Reactive, 5=Unsafe). The code was ORing these directly into the EFF_* bitmask: `eff_mask = eff_mask or ek`. This only worked by accident for indices 0 and 1 (EFF_PURE=0x00, EFF_IO=0x01). For indices 2-5, the OR produced wrong bitmask values:
- Index 2 (Async) gave 0x02 instead of EFF_ASYNC=0x04
- Index 3 (GPU) gave 0x03 instead of EFF_GPU=0x02
- etc.
**Fix:** Added `eff_index_to_mask(idx: Int) -> Int` mapping function. Now calls `eff_index_to_mask(ek)` before ORing into the effect mask.
**Impact:** Effect violation detection (via `can_call()`) now works correctly for all effect combinations. Previously, only Pure and IO were checked correctly.

---

## 3. Issues NOT Fixed (Documented Limitations)

### DEBT 1 — types.kn: `push_type_slot()` is a no-op (Architecture)
**Problem:** Returns `len(env.all_types)` without pushing. TypeEnv uses Kain value semantics, so `resolve_type_ast` can't mutate env. All compound type `inner_type` indices are phantom — `type_env_get(env, inner_idx)` returns `rt_unknown()`.
**Affected:** Array<T>, Ptr<T>, Ref<T>, Option<T>, Result<T,E> — all resolve inner types as Unknown.
**Severity:** Medium. Mitigated by fallback `rt_i64()` in `infer_index_type`, `infer_expr_type` for mem_load, etc.
**Recommended fix:** Thread `(TypeEnv, ResolvedType)` through entire resolution pipeline. Requires changing `resolve_type_in_env`, `resolve_type_ast`, and all callers. 2-3 hour refactor.

### DEBT 2 — types.kn: `check_struct_item` double-registers struct type
**Problem:** First pushes via `e.all_types.push(sty)`, then re-registers via `register_type(e, struct_name, sty)`. Creates duplicate entries in `all_types` / `all_type_names`.
**Severity:** Low. Second registration overwrites the first in lookup (last-match-wins in `lookup_type`). Harmless but wasteful.

### DEBT 3 — compiler.kn: `compile_workspace` manual TOML parser
**Problem:** Character-by-character string parsing for KAIN.toml `[source_order]`. Only handles single-quoted filenames per line. Doesn't handle `files = [...]` array syntax, multi-line values, or TOML comments after filenames.
**Severity:** Low. Works for current KAIN.toml format. Would break if source_order format changes.
**Recommended fix:** Use proper TOML parsing when std::toml or a parsing library is available.

### DEBT 4 — monomorphize.kn: `unify()` doesn't recurse properly
**Problem:** For Option<T>, Result<T,E>, Array<T>, the recursive calls pass `mono_rt_unknown()` for inner types instead of looking up the actual inner types. Also, `type_has_generic()` only checks top-level kind, doesn't recurse into inner types.
**Severity:** Medium. Generic monomorphization is incomplete. Currently masked because the bootstrap compiler source doesn't have generic functions (uses `Array<Int>`, `Array<String>` which are monomorphic in bootstrap).
**Recommended fix:** Thread type registry into monomorphizer so inner types can be looked up and recursively unified.

---

## 4. Files Modified

| File | Lines Changed | Nature |
|------|-------------|--------|
| `src/llvm_ffi.kn` | -1 line | Removed duplicate check |
| `src/types.kn` | +33 / -5 | Fix 2: infer_block_type + check_block_body trailing expr + effect mapping |

---

## 5. Final Verification

### `kain check src/`
```
24/24 PASSED (0 failed)
```

### `kain build --target llvm`
```
Build succeeded: lane=dev target=project host=x86_64-windows
  ok check-llvm
  ok kainc-source-tests
  ok root-executable → kainc.exe
  ok certify-kainc-local
  ok kain-compile:kainc:llvm → main.exe
```

### Binary produced
- `X:\blades\kain\kainc.exe` — self-host compiler binary
- `X:\blades\kain\.kain\out\x86_64-windows\dev\project\kainc\llvm\main.exe` — LLVM-compiled executable

---

## 6. Summary

| Category | Count |
|----------|-------|
| Bugs fixed | 2 (FIX 1, FIX 4) |
| Gaps closed | 2 (FIX 2, FIX 3) |
| Known debts deferred | 4 |
| Files modified | 2 |
| `kain check` result | 24/24 ✅ |
| `kain build` result | SUCCESS ✅ |

### Verdict

RED's types.kn changes (1,874 → 2,201 lines) are structurally sound. All 13 check_* functions correctly return `TypedItemAndEnv`, TypeEnv is properly threaded through `pass4_check`, and the expression inference handles all 64 expression kinds without fallthrough defaulting to `rt_i64()` for most cases. The architectural limitation (`push_type_slot` no-op) is a known bootstrap constraint, not a RED bug.

GREEN's llvm_ffi.kn stubs are correct — all 70+ wrapper functions return `int_to_ptr(0, "ptr<Byte>")`, HAS_LLVM_HEADERS=0 flag properly gates real headers as comments, and the ECHO→GOLF section markers are preserved. compiler.kn's workspace discovery and multi-file compilation work (using valid std::fs functions). The manual TOML parsing is fragile but functional.

**The self-host compiler pipeline works — build.kn drives the full compilation producing a binary.**
