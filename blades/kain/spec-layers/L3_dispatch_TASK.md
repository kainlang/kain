# L3 Dispatch — converge: Implementation Tasks
================================================================================
**Target:** `src/L3_dispatch.kn` — new file (~500 lines)
**Depends on:** parser.kn (indent parsing), ast.kn (AST_CONVERGE_LANE), types.kn (check_converge_stub replacement), codegen.kn + llvm_ffi.kn (LLVM IR emission)
**Required pre-read:** `X:/blades/kain/spec-layers/L3_dispatch.md` (full spec), `X:/blades/kain/src/types.kn:1607` (current stub), `X:/blades/kain/src/parser.kn:2817` (current parse stub)

================================================================================
## Task 1: AST Constants and Helpers (ast.kn) 
================================================================================

**1.1** Add `AST_CONVERGE_LANE = 220` to ast.kn alongside existing item-kind constants (~line 50-100).

**1.2** Add `ast_kind_name(AST_CONVERGE_LANE)` entry mapping to `"ConvergeLane"` in the `ast_kind_name()` dispatch function.

**1.3** Update the comment block above `AST_ITEM_CONVERGE` (currently at `ast.kn:290`) to document the new structured data layout:
```
AST_ITEM_CONVERGE data layout:
  [0] = name_idx (string table)
  [1] = return_type_idx (-1 if none)
  [2] = params_node_idx (child AST node)
  [3] = spec_lane_node_idx (child AST_CONVERGE_LANE)
  [4] = fast_lane_count (N)
  [5..5+N-1] = fast_lane_node_idx for each lane
  [5+N] = verify_random_count (-1 if none)
```

**1.4** Document AST_CONVERGE_LANE child node layout:
```
AST_CONVERGE_LANE data layout:
  [0] = lane_name_idx (string table)
  [1] = lane_kind (0=spec, 1=fast)
  [2] = selector_kind (0=none, 1=target, 2=capability)
  [3] = selector_value_idx (string table, -1 if none)
  [4] = body_block_idx (AST_EXPR_BLOCK child)
```

**Acceptance:** `AST_CONVERGE_LANE` constant exists, `ast_kind_name(220)` returns `"ConvergeLane"`, doc comments are accurate.

================================================================================
## Task 2: Parser Rewrite (parser.kn) 
================================================================================

**2.1** Replace `parse_converge_item()` body (~line 2817) with full structured parse:
- Parse `converge <name>(<params>) [-> <type>]:` then expect indent
- Parse exactly one `spec <lane_name>: <block>` via `parse_converge_lane()`
- Parse at least one `fast <lane_name> [when <selector>]: <block>` (loop, error if zero)
- Parse optional `verify random(<N>)` via `parse_converge_verify_random()`
- Expect dedent
- Emit `AST_ITEM_CONVERGE` with new data layout (Task 1.3)

**2.2** Create `parse_converge_lane(st, lane_kind_int)` helper:
- Parse `<lane_name>:` ident + colon
- If lane_kind is `fast` and next is `when`, call `parse_converge_selector()`
- Parse block body via `parse_block_or_expr()`
- Emit `AST_CONVERGE_LANE` child node (Task 1.4)

**2.3** Create `parse_converge_selector(st)` helper:
- Expect `when target("...")` → `selector_kind=1`, intern the string
- Or `when capability("...")` → `selector_kind=2`, intern the string
- Error if neither target/capability follows `when`
- The token after `target(` or `capability(` must be a string literal

**2.4** Create `parse_converge_verify_random(st)` helper:
- Expect `verify random(<N>)` where N is an integer literal
- Return N (or -1 if verify clause absent)
- Validate N >= 0 at parse time

**2.5** Add error recovery: if a lane lacks a body (immediate dedent or next lane keyword), report an error and skip.

**Acceptance:** `kain check` on a file with `converge mix(v: Int) -> Int: spec ref: return v fast llvm when target("llvm"): return v verify random(4)` produces a structured AST with spec lane, one fast lane with target selector, and verify count=4. Missing spec or zero fast lanes produce a parse error.

================================================================================
## Task 3: Typechecker Implementation (types.kn) 
================================================================================

**3.1** Replace `check_converge_stub()` at `types.kn:1607` with `check_converge()`:
- Extract name_idx, ret_type_idx, params_node, spec_lane_node, fast_count, verify_count from AST data
- Resolve return type: if `ret_type_idx >= 0`, call `resolve_type_in_env()`; else default to `rt_i64()`

**3.2** Type-check the spec lane:
- Extract body block from spec lane child node
- Infer spec lane's return type via `infer_block_type()`
- Assert it matches the dispatcher return type. Error code: `ERR_CONVERGE_SPEC_TYPE_MISMATCH`

**3.3** Type-check each fast lane:
- Loop `i` from 0 to `fast_count - 1`
- For each: extract lane name, selector kind/value, body block
- Validate selector: if `target("x")`, `x` must be non-empty; if `capability("x")`, `x` must be non-empty
- Infer fast lane return type, assert matches dispatcher return type. Lanes may differ in body but must agree in signature.
- Error code on mismatch: `ERR_CONVERGE_FAST_TYPE_MISMATCH` (includes lane name)
- Check no duplicate lane names (case-sensitive)

**3.4** Validate `verify_random_count`:
- If present (`>= 0`), warn if `N > 10000` (startup time concern)
- If present but `N < 0`, error — must be non-negative
- No effect on resolved type — it's a behavioral contract

**3.5** Create helper `converge_dispatcher_signature(env, node, idx) -> ResolvedType`:
- Build a synthetic function signature from the converge name, params, and return type
- Called by the typechecker to establish the baseline signature all lanes must match

**3.6** Wire `check_converge` into the typechecker dispatch at `types.kn:887` (replace the `check_converge_stub` call).

**Acceptance:** A converge with matching lane signatures type-checks OK. A converge with spec returning `Int` and fast lane returning `Float` produces `ERR_CONVERGE_FAST_TYPE_MISMATCH`. An empty capability selector produces a warning.

================================================================================
## Task 4: Codegen — Converge Dispatch (new file or codegen.kn) 
================================================================================

**4.1** Create `src/L3_dispatch.kn` entry point or add to `codegen.kn`:
- `pub fn codegen_converge(env: CodegenEnv, node: AstNode, idx: Int) -> CodegenResult`

**4.2** Emit the spec lane as a separately callable function with symbol `{name}__spec`:
- Call `codegen_named_callable(env, spec_fn_name, params_node, ret_type_idx, spec_body)`
- This produces `define i64 @{name}__spec(i64 %arg) { ... }` in LLVM IR

**4.3** Emit each fast lane as separately callable function with symbol `{name}__fast_{lane_name}`:
- Call `codegen_named_callable()` for each
- This produces `define i64 @{name}__fast_{lane}(i64 %arg) { ... }` in LLVM IR

**4.4** Emit the dispatch function (the converge name itself):
- Declare a cached lane static global: `@__kain_converge_cached_{name}` = internal global i64 -2
- Entry: load cached lane, check if uninitialized (== -2)
- If uninitialized: for each fast lane with a `capability("...")` selector, emit `abi_cpu_capability_mask_for_key()` probe call; for `target("...")`, resolve statically at codegen time (skip non-matching targets entirely)
- First matching fast lane wins → store its index in cached global
- Fallback (spec) = store -1, branch to dispatch
- Dispatch: switch on cached lane index, call the selected lane function

**4.5** Handle target selectors at codegen time:
- If codegen target is `"llvm"` and a fast lane has `when target("llvm")`, it's always eligible (no runtime probe needed)
- If a fast lane has `when target("interpret")` and we're codegenning for LLVM, skip it entirely at codegen (don't emit the function)
- This matches the Rust bootstrap's approach of static target resolution

**4.6** Wire `codegen_converge` into the main codegen dispatch loop.

**Acceptance:** The converged name becomes a callable LLVM function. `kain run` with identical spec/fast bodies returns the correct result. The LLVM IR contains `@__kain_converge_cached_*`, spec function, and fast lane function.

================================================================================
## Task 5: Runtime ABI and Telemetry Wiring 
================================================================================

**5.1** Ensure `src/llvm_ffi.kn` (or equivalent) declares extern functions:
- `abi_cpu_capability_mask_for_key(key: ptr<Byte>) -> Int` (from `kain_machine_capability_mask_for_key`)
- `abi_converge_record_telemetry(key: Int, shape: Int, lane: Int, elapsed: Int, status: Int)` (optional, for verify mismatch tracking)

**5.2** Wire `converge_mismatch_count()` for the runtime counter:
- If the runtime hasn't exposed this yet, add a global atomic counter that `abi_converge_record_telemetry()` increments on mismatch.
- The LLVM codegen emits the counter increment call inside the verify startup block.

**5.3** Add the runtime contract entry for converge:
- When a converge exists, emit `capability "converge.dispatch"` in runtime_contract.json
- Each converge emits `RuntimeConvergeContract { name, spec_lane, fast_lanes, verify_random_count }`

**Acceptance:** `converge_mismatch_count()` returns 0 after running a converge with identical spec and fast lane bodies.

================================================================================
## Task 6: Integration and Tests 
================================================================================

**6.1** Wire the new `src/L3_dispatch.kn` file into the build pipeline:
- Add it to the compiler's file list in `build.kn` or the main import chain
- Ensure it's combined/concatenated before `kain check` runs on the full compiler

**6.2** Create a minimal smoke test:
```
converge smoke_mix(v: Int) -> Int:
    spec ref: return ((v * 31) + 7) % 1000000007
    fast llvm when target("llvm"): return ((v * 31) + 7) % 1000000007
    verify random(4)

pub fn test() -> Int:
    let r = smoke_mix(42)
    if r != ((42 * 31 + 7) % 1000000007): return 1
    if converge_mismatch_count() != 0: return 2
    return 0
```
Save in `smoketest/` and verify `kain check` + `kain run` both pass.

**6.3** Edge case tests (in comments or test file):
- Converge with 3 fast lanes (target, capability, no selector)
- Converge with `verify random(0)` — should not run any verification
- Converge with `capability("cpu.x86.avx2")` — should fallback to spec on non-AVX2 hardware
- Error case: converge with zero fast lanes → parse error
- Error case: converge with no spec lane → parse error

**6.4** Update the compiler's test suite to include `test_converge()`.

**Acceptance:** All acceptance criteria from Tasks 1-5 are met. The compiler passes its own converge smoketest.

================================================================================
