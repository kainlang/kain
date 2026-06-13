# L3 Dispatch — `converge`: Self-Host Implementation Plan
================================================================================
**Status:** Comprehensive spec for `blades/kain/src/L3_dispatch.kn`
**Based on:** Rust bootstrap at `crates/core/src/parser.rs`, `crates/core/src/ast.rs`,
              `crates/core/src/types.rs`, `crates/sys-codegen/src/codegen_llvm/mod.rs`,
              `crates/core/src/runtime_contract.rs`, `runtime/native/include/converge.h`,
              `docs/CONVERGE.md`, `stdlib/intent.kn`, self-host parser/types stubs.

================================================================================
## 1. Architecture Overview
================================================================================

`converge` is a top-level declaration implementing **spec-plus-fast-lanes dispatch**.
It is the L3 (Dispatch) layer in Kain's decision ladder:

```
converge name(params) -> ReturnType:
    spec lane_name:                        [EXACTLY ONE, required]
        body_stmts...
    fast lane_name when selector:          [AT LEAST ONE, required]
        body_stmts...
    [fast lane_name when selector: ...]    [MORE OPTIONAL]
    [verify random(N)]                     [OPTIONAL]
```

### Key Concepts

| Concept | Description |
|---------|-------------|
| **spec lane** | Reference/ground-truth implementation. Exactly one required. |
| **fast lane** | Alternative implementation. At least one required. |
| **selector** | `target("llvm")` or `capability("cpu.x86.avx2")` — gates a fast lane. |
| **when guard** | `when target("...")` or `when capability("...")` — lane eligibility. |
| **verify random(N)** | Generates N random argument sets, calls spec + selected lane, records mismatches. |
| **lane selection** | Fast lanes scanned in declaration order; first matching selector wins. Fallback to spec if none match. |

### What the Compiler Owns

1. **Lane selection** — the programmer doesn't choose which lane runs. The compiler/runtime probes capabilities at startup and caches the selection.
2. **Signature enforcement** — all lanes must have the same signature (params + return type). The compiler verifies this.
3. **Verify contract** — `verify random(N)` generates startup checks that fast lanes agree with the spec.
4. **Telemetry** — `converge_mismatch_count()` records spec-vs-fast divergence.

### Two-Level Architecture

- **Interpreter path** (`runtime.rs`): Linear scan of fast lanes, direct body execution, `select_converge_lane()`.
- **LLVM path** (`codegen_llvm/mod.rs`): Static target resolution, runtime capability probes, cached lane selection in a static global variable.

================================================================================
## 2. AST Representation
================================================================================

### Rust AST (from `crates/core/src/ast.rs:356`)

```rust
pub struct ConvergeDef {
    pub name: String,
    pub params: Vec<Param>,
    pub return_type: Option<Type>,
    pub spec_lane: ConvergeLane,          // Exactly one
    pub fast_lanes: Vec<ConvergeLane>,    // At least one
    pub verify_random_count: Option<u32>, // None or N
    pub visibility: Visibility,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}

pub struct ConvergeLane {
    pub kind: ConvergeLaneKind,       // Spec or Fast
    pub lane_name: String,            // User-defined identifier
    pub selector: Option<ConvergeSelector>,  // None, or Target/Capability
    pub body: Block,                  // Lane function body
    pub span: Span,
}

pub enum ConvergeLaneKind { Spec, Fast }

pub enum ConvergeSelector {
    Target(String),     // target("llvm")
    Capability(String), // capability("cpu.x86.avx2")
}
```

### Proposed Self-Host AST Layout (flat array)

```
AST_ITEM_CONVERGE (kind=15)
  data[0] = name_idx (string table)
  data[1] = return_type_idx (-1 if none)
  data[2] = params_node_idx (AST child node storing param list)
  data[3] = spec_lane_node_idx (ConvergeLane child)
  data[4] = fast_lane_count
  data[5] = fast_lane_0_idx
  data[6] = fast_lane_1_idx
  ...
  data[4+N] = verify_random_count (-1 if none)
```

**ConvergeLane child node layout:**
```
kind = AST_CONVERGE_LANE (new: 220)
  data[0] = lane_name_idx
  data[1] = lane_kind (0=spec, 1=fast)
  data[2] = selector_kind (0=none, 1=target, 2=capability)
  data[3] = selector_value_idx (string table, -1 if none)
  data[4] = body_block_idx
```

### Self-Host Current Status

Current `AST_ITEM_CONVERGE` data layout in `ast.kn:293`:
```
data[0] = name_idx
data[1] = param_count
data[2..2+N] = param_names, param_types (interleaved)
data[N+3] = ret_idx
data[N+4] = body_idx
```
This is WRONG — it stores a single body, not structured lanes. The spec lane, fast lanes, selectors, and verify count are all missing from the AST.

**Critical gap:** The parser treats converge's body as a single flat block, not as structured lane declarations. The self-host compiler cannot distinguish spec from fast lanes, cannot parse selectors, and cannot parse verify random clauses.

================================================================================
## 3. Rust Bootstrap Reference
================================================================================

### 3.1 Parser (`crates/core/src/parser.rs`)

| Function | Lines | Description |
|----------|-------|-------------|
| `parse_converge()` | 1954-2034 | Top-level converge parser. Expects `converge name(params) -> Type:\n indent`. Inside the indent block: one `spec` lane parsed, then `fast` lanes (one or more), then optional `verify random(N)`. Returns `Item::Converge(ConvergeDef)`. |
| `parse_converge_lane()` | 2034-2055 | Parses a single lane. Expects `spec <name>:` or `fast <name> when selector:` or `fast <name>:`. Returns `ConvergeLane { kind, lane_name, selector, body }`. |
| `parse_converge_selector()` | 2055-2079 | Parses `target("...")` or `capability("...")`. Returns `ConvergeSelector`. |
| `parse_converge_verify_random_count()` | 2079-2096 | Parses `verify random(N)`. Returns N as `u32`. |

The Rust parser is **complete** — it fully parses every converge clause into structured AST nodes.

### 3.2 Typechecker (`crates/core/src/types.rs`)

| Function | Lines | Description |
|----------|-------|-------------|
| `check_converge()` | 6120-6160 | Main converge typechecker. Sets `env.in_converge=true`. Resolves dispatcher signature, checks `verify_random_count` compatibility, type-checks spec lane body, type-checks each fast lane body, verifies each lane signature matches the dispatcher signature. In interpret mode, signature mismatches are warnings. Returns `TypedConverge`. |
| `converge_dispatcher_view()` | 5871-5888 | Creates a `Function` view of the converge (for signature derivation). Copies name, params, return_type from the ConvergeDef. Body is empty (just a span placeholder). |
| `converge_lane_function_view()` | 5889-5903 | Creates a `Function` view for a specific lane. Name is `__kain_converge__{name}__{lane_name}`. Copies params, return_type from ConvergeDef. Body is the lane's body. Used for type-checking each lane as a standalone function. |
| `ensure_converge_verify_types_supported()` | 5912-5944 | If `verify random(N)` is present, ensures the return type supports equality comparison (Int, Bool, etc.). |

**TypedConverge** (`types.rs:198`):
```rust
pub struct TypedConverge {
    pub ast: ConvergeDef,
    pub resolved_type: ResolvedType,
}
```

### 3.3 Runtime Contract (`crates/core/src/runtime_contract.rs`)

| Struct | Lines | Description |
|--------|-------|-------------|
| `RuntimeConvergeContract` | 202-215 | `{ name, dispatcher_symbol, spec_lane: RuntimeConvergeLaneContract, fast_lanes: Vec<RuntimeConvergeLaneContract>, verify_random_count: Option<u32> }` |
| `collect_converge_contracts()` | 1411-1461 | Iterates all converges in the module, emits `RuntimeConvergeContract` entries and the `converge.dispatch` capability. |

### 3.4 Interpreter (`crates/core/src/runtime.rs`)

| Function | Lines | Description |
|----------|-------|-------------|
| `register_converge_value()` | 3967, 4175, 4297 | Registers the converge name in the value table as `Value::Converge(name)`. |
| `select_converge_lane()` | 7256-7280 | Linear scan: for each fast lane, evaluates `target("...")` or checks capability bitmask. Returns first matching lane. Falls back to spec. |
| `verify_converge_selected_against_spec()` | 7280-7435 | Calls spec and selected lane with random args, compares results, increments `converge_mismatch_count` on divergence. |

### 3.5 LLVM Codegen (`crates/sys-codegen/src/codegen_llvm/mod.rs`)

| Function | Lines | Description |
|----------|-------|-------------|
| `compile_converge()` | 15722-15822+ | Full converge codegen. Emits: (1) a static cached lane global (`i64 -2` = uninitialized), (2) spec function as `{name}__spec`, (3) each fast lane as `{name}__fast_{fragment}`, (4) the dispatch function itself. The dispatch function checks the cached lane global; if uninitialized, probes capabilities, selects lane, caches it; then jumps via switch to the selected lane's code. |

**Key LLVM IR pattern:**
```llvm
; Static cached lane (i64, -2 = uninitialized)
@__kain_converge_cached_lane_mix = internal global i64 -2

; Spec function
define i64 @mix__spec(i64 %arg) { ... }

; Fast lane function
define i64 @mix__fast_avx2(i64 %arg) { ... }

; Dispatch function (the converge name)
define i64 @mix(i64 %arg) {
  %cached = load i64, i64* @__kain_converge_cached_lane_mix
  %uninit = icmp eq i64 %cached, -2
  ; if uninit: probe, select, store
  ; select via switch over lane index
}
```

### 3.6 Native C Runtime (`runtime/native/include/converge.h`)

| Function | Description |
|----------|-------------|
| `abi_converge_select_lane_for_key(key, shape_key, mask, fallback)` | Probes autotune cache (64-slot open-addressed hash table). |
| `abi_converge_commit_winner(key, shape_key, lane_index, mask)` | Commits a lane selection to the cache. |
| `abi_converge_record_telemetry(key, shape_key, lane_index, elapsed, status)` | Records timing/lane telemetry in a 64-slot circular buffer. |

================================================================================
## 4. Parser Status
================================================================================

### Current: STUB

Current `parse_converge_item()` at `parser.kn:2817`:
```kn
pub fn parse_converge_item(st, vis_val, attrs) -> ParseResult:
    # Skips 'converge' keyword
    # Parses name ident
    # Parses (param_name: type, ...) — correct shape
    # Parses -> return_type — correct shape
    # Parses : block_or_expr — WRONG: treats everything as a single flat block
    # Data layout: [name_idx, param_count, params..., ret_idx, body_idx]
    # NO spec lane, NO fast lanes, NO selectors, NO verify random
```

The parser does NOT distinguish between the `converge` keyword body structure and a regular block. It stores a single body node, losing all semantic structure.

### Required: COMPLETE REWRITE

The parser must:
1. After `converge name(params) -> Type:`, expect indent
2. Parse `spec <lane_name>:` + block body
3. Parse `fast <lane_name> [when target("...") | capability("...")]:` + block body (one or more)
4. Parse optional `verify random(N)`
5. Expect dedent
6. Emit structured AST with lane nodes and selector nodes

**New data layout for AST_ITEM_CONVERGE:**
```
data[0] = name_idx
data[1] = return_type_idx (-1 if none)
data[2] = params_node_idx (child AST node — reuse AST_EXPR_TUPLE or new node kind)
data[3] = spec_lane_node_idx (child AST_CONVERGE_LANE node)
data[4] = fast_lane_count (N)
data[5..5+N] = fast_lane_node_idx for each lane
data[5+N] = verify_random_count (-1 if none)
```

**New node kind: AST_CONVERGE_LANE (220)**
```
data[0] = lane_name_idx
data[1] = lane_kind (0=spec, 1=fast)
data[2] = selector_kind (0=none, 1=target, 2=capability)
data[3] = selector_value_idx (string table, -1 if none)
data[4] = body_block_idx
```

================================================================================
## 5. Typechecker Plan
================================================================================

### Current: TRUE STUB

`check_converge_stub()` at `types.kn:1607`:
```kn
pub fn check_converge_stub(env: TypeEnv, node: AstNode, idx: Int) -> TypedItemAndEnv:
    let name_idx = if ast_data_len(node) > 0: ast_data_get(node, 0) else: -1
    return TypedItemAndEnv {
        env: env,
        item: TypedItem {
            kind: AST_ITEM_CONVERGE, name: "cvg_" + str(name_idx), name_idx: name_idx,
            resolved_type: rt_i64(), ast_index: idx, effects: EFF_PURE,
        }
    }
```

Returns `rt_i64()` always, no checking of spec lane, no checking of fast lane signatures, no verify analysis. This is a complete no-op.

### Required Implementation

```kn
pub fn check_converge(env: TypeEnv, node: AstNode, idx: Int) -> TypedItemAndEnv:
    # Step 1: Extract AST fields
    let name_idx = ast_data_get(node, 0)
    let ret_type_idx = ast_data_get(node, 1)
    let params_node = ast_data_get(node, 2)
    let spec_lane_node = ast_data_get(node, 3)
    let fast_count = ast_data_get(node, 4)
    let verify_count = ast_data_get(node, 5 + fast_count)

    # Step 2: Resolve the dispatcher signature
    # Build a synthetic function signature from the converge params + return type
    let return_type: ResolvedType = if ret_type_idx >= 0:
        resolve_type_in_env(env, ret_type_idx)
    else:
        rt_i64()

    # Step 3: Check the spec lane
    # Extract spec lane body, resolve its return type
    # Must match the dispatcher return type
    let spec_body = ...  # get body from spec lane child node
    let spec_ret = infer_block_type(env, spec_body)
    assert_type_match(env, spec_ret, return_type,
        "converge spec lane return type does not match dispatcher")

    # Step 4: Check each fast lane
    var i = 0
    while i < fast_count:
        let fast_lane_node = ast_data_get(node, 5 + i)
        let lane_name = ast_data_get(fast_lane_node, 0)
        let selector_kind = ast_data_get(fast_lane_node, 2)
        let selector_value = ast_data_get(fast_lane_node, 3)

        # Resolve selector string (if present)
        if selector_kind == 1:  # target
            let target_val = string_table_get(env.strings, selector_value)
            # Validate: non-empty string
            if len(target_val) == 0:
                env.report_error("converge target selector value cannot be empty")
        if selector_kind == 2:  # capability
            let cap_val = string_table_get(env.strings, selector_value)
            if len(cap_val) == 0:
                env.report_error("converge capability selector value cannot be empty")

        # Check fast lane body type
        let fast_body = ...  # extract from lane child
        let fast_ret = infer_block_type(env, fast_body)
        assert_type_match(env, fast_ret, return_type,
            "converge fast lane '" + str(table_get(lane_name)) + "' signature does not match")

        i = i + 1

    # Step 5: Validate verify random count (if present)
    if verify_count >= 0 and verify_count > 10000:
        env.report_warning("verify random(N) with N > 10000 may be slow at startup")

    # Step 6: Return typed item
    return TypedItemAndEnv {
        env: env,
        item: TypedItem {
            kind: AST_ITEM_CONVERGE,
            name: "cvg_" + str(name_idx),
            name_idx: name_idx,
            resolved_type: return_type,
            ast_index: idx,
            effects: EFF_PURE,  # converge itself is pure; lanes may have effects
        }
    }
```

### Validation Checklist

| Check | Description | Error |
|-------|-------------|-------|
| Spec lane exists | `ast_data_get(node, 3) >= 0` | "converge requires exactly one spec lane" |
| Fast lanes exist | `fast_count >= 1` | "converge requires at least one fast lane" |
| Spec lane return type | Matches dispatcher signature | "spec lane return type mismatch" |
| Fast lane signatures | Each matches dispatcher | "fast lane 'X' does not match dispatcher signature" |
| Capability string | Non-empty, reasonable format | "capability selector value cannot be empty" |
| Target string | Non-empty | "target selector value cannot be empty" |
| Verify random N | N >= 0 (N=0 means disabled) | "verify random count must be non-negative" |
| No duplicate lanes | No two lanes share a name | "duplicate lane name 'X'" |

================================================================================
## 6. Codegen Plan
================================================================================

### Current: NOTHING

`codegen.kn` has zero references to converge or orchestrate.

### Required Implementation

```kn
# In src/L3_dispatch.kn — codegen section
pub fn codegen_converge(env: CodegenEnv, conv_node: AstNode, idx: Int) -> CodegenResult:
    let name_idx = ast_data_get(conv_node, 0)
    let ret_type_idx = ast_data_get(conv_node, 1)
    let spec_lane_node = ast_data_get(conv_node, 3)
    let fast_count = ast_data_get(conv_node, 4)
    let verify_count = ast_data_get(conv_node, 5 + fast_count)

    # Step 1: Emit spec lane as a named callable: {name}__spec
    let spec_body = ast_data_get(spec_lane_node, 4)
    let spec_fn_name = str(name_idx) + "__spec"
    codegen_named_callable(env, spec_fn_name, params_node, ret_type_idx, spec_body)

    # Step 2: Emit each fast lane as a named callable: {name}__fast_{lane}
    let i = 0
    while i < fast_count:
        let fast_lane_node = ast_data_get(conv_node, 5 + i)
        let lane_name_idx = ast_data_get(fast_lane_node, 0)
        let lane_body = ast_data_get(fast_lane_node, 4)
        let fast_fn_name = str(name_idx) + "__fast_" + str(lane_name_idx)
        codegen_named_callable(env, fast_fn_name, params_node, ret_type_idx, lane_body)
        i = i + 1

    # Step 3: Emit the dispatch function itself
    # LLVM IR pattern:
    #   define i64 @name(i64 %arg) {
    #     %cached = load i64, i64* @cached_lane
    #     %uninit = icmp eq i64 %cached, -2
    #     br i1 %uninit, label %select, label %dispatch
    #   select: ... probe capabilities ...
    #   dispatch: switch i64 %cached [ ... ]
    #   }
    emit_dispatch_header(env, name_idx, params_node, ret_type_idx)
    emit_cached_lane_check(env, name_idx, fast_count)

    # Step 4: For each fast lane with a capability selector, emit a capability probe
    let i = 0
    while i < fast_count:
        let fast_lane_node = ast_data_get(conv_node, 5 + i)
        let selector_kind = ast_data_get(fast_lane_node, 2)
        if selector_kind == 2:  # capability
            let cap_idx = ast_data_get(fast_lane_node, 3)
            emit_capability_probe(env, cap_idx, i)
        i = i + 1

    # Step 5: Emit lane selection switch (first matching lane wins)
    emit_lane_selector_switch(env, name_idx, fast_count)

    # Step 6: Emit spec fallback label
    emit_spec_fallback_call(env, spec_fn_name, params_node)

    # Step 7: Optionally emit verify_random startup code
    if verify_count >= 0 and verify_count > 0:
        emit_verify_random_code(env, name_idx, spec_fn_name, fast_count, verify_count)
```

### LLVM IR Target Pattern

```llvm
; Static cached lane global
@__kain_converge_cached_<name> = internal global i64 -2

; Dispatch function
define i64 @<name>(i64 %arg) {
entry:
  %cached = load i64, i64* @__kain_converge_cached_<name>
  %uninit = icmp eq i64 %cached, -2
  br i1 %uninit, label %select_lane, label %use_cached

select_lane:
  ; Probe capabilities for each fast lane
  %caps = call i64 @abi_cpu_capability_mask_for_key(...)
  %has_feature = ...
  br i1 %has_feature, label %lane_0, label %next_probe

lane_0:
  store i64 0, i64* @__kain_converge_cached_<name>
  br label %use_cached

; ... more probes for each lane ...

spec_fallback:
  store i64 -1, i64* @__kain_converge_cached_<name>
  br label %use_cached

use_cached:
  %lane = load i64, i64* @__kain_converge_cached_<name>
  switch i64 %lane, label %spec_lane [-1, label %spec_lane] [i64 0, label %fast_0] [i64 1, label %fast_1] ...

spec_lane:
  %spec = call i64 @<name>__spec(i64 %arg)
  ret i64 %spec

fast_0:
  %f0 = call i64 @<name>__fast_<lane0>(i64 %arg)
  ret i64 %f0
}
```

### Target Selector Handling

`target("llvm")` selectors are resolved STATICALLY at codegen time:
- If `target("llvm")` and we're codegenning for LLVM → always eligible
- If `target("interpret")` and we're codegenning for LLVM → never eligible
- The codegen can skip emitting non-matching target fast lanes entirely, OR keep them and emit a static `true`/`false` check

**Recommendation:** Skip emitting non-matching target fast lanes at codegen time. The dispatch function will never need them. This matches the Rust bootstrap's approach.

================================================================================
## 7. Runtime Contract
================================================================================

### Stdlib Telemetry Functions (from `stdlib/intent.kn`)

| Function | Signature | What it does |
|----------|-----------|-------------|
| `converge_mismatch_count()` | `() -> Int` | Total number of spec-vs-fast lane mismatches across all converges |
| `converge_choose_int(spec, fast)` | `(Int, Int) -> Int` | Returns spec or fast with telemetry recording |
| `converge_choose_bool(spec, fast)` | `(Bool, Bool) -> Bool` | Bool version of converge_choose |
| `converge_status(spec, fast)` | `(Int, Int) -> Int` | Compares spec vs fast, returns status code |

### Runtime Contract Emissions

When a converge declaration exists, the emitted `runtime_contract.json` contains:
```json
{
  "converges": [
    {
      "name": "xoshiro_scramble",
      "dispatcher_symbol": "xoshiro_scramble",
      "spec_lane": { "name": "builtin", "symbol": "xoshiro_scramble__spec" },
      "fast_lanes": [
        { "name": "llvm_lane", "symbol": "xoshiro_scramble__fast_llvm_lane",
          "selector": "target(\"llvm\")" }
      ],
      "verify_random_count": null
    }
  ],
  "capabilities": ["converge.dispatch"]
}
```

### What the Self-Host Compiler Must Emit

The codegen must emit calls to:
1. `abi_cpu_capability_mask_for_key("key")` for `capability("key")` probes
2. The converge name itself must be a callable symbol (the dispatch function)
3. Each spec/fast lane as a separate callable symbol with `__spec` / `__fast_` suffix

================================================================================
## 8. Implementation Tasks
================================================================================

### Phase 1: AST Extensions (in `ast.kn`)

- [ ] Add `AST_CONVERGE_LANE = 220` constant
- [ ] Add `ast_kind_name` entry for `AST_CONVERGE_LANE` → `"ConvergeLane"`
- [ ] Update `AST_ITEM_CONVERGE` doc comment with new data layout

### Phase 2: Parser Rewrite (in `parser.kn`)

- [ ] Rewrite `parse_converge_item()` to produce structured AST:
  - Parse `converge <name>(<params>) [-> <type>]:`
  - Expect indent
  - Parse `spec <lane_name>: <block>`
  - Loop: parse `fast <lane_name> [when target("val") | capability("val")]: <block>` (at least one)
  - Parse optional `verify random(<N>)`
  - Expect dedent
  - Emit `AST_ITEM_CONVERGE` with new data layout
- [ ] Create `parse_converge_lane()` helper
- [ ] Create `parse_converge_selector()` helper (parses `target("...")` / `capability("...")`)
- [ ] Create `parse_converge_verify_random()` helper
- [ ] Add AST child node constructors for `AST_CONVERGE_LANE`

### Phase 3: Typechecker (in `types.kn`)

- [ ] Replace `check_converge_stub()` with `check_converge()`:
  - Extract spec lane, fast lanes from AST data
  - Resolve dispatcher signature (params + return type)
  - Type-check spec lane body as function with that signature
  - Type-check each fast lane body as function with that signature
  - Verify lane signatures match dispatcher
  - Validate selector strings (non-empty)
  - Validate verify random N >= 0
  - Return typed item with correct resolved_type
- [ ] Add helper functions: `converge_dispatcher_signature()`, `converge_check_lane()`
- [ ] Add `ensure_converge_verify_types_supported()` stub (when return types support equality)

### Phase 4: Codegen (`src/L3_dispatch.kn`)

- [ ] Create `codegen_converge()` function:
  - Emit spec lane as `{name}__spec` callable
  - Emit each fast lane as `{name}__fast_{lane}` callable
  - Emit dispatch function with cached lane selection
  - Emit capability probe code for `capability("...")` selectors
  - Skip non-matching target lanes (`target("llvm")` vs `target("interpret")`)
  - Optionally emit verify_random startup validation
- [ ] Create codegen helpers: `codegen_cached_lane_global()`, `codegen_lane_selector_switch()`, `codegen_capability_probe()`

### Phase 5: Integration

- [ ] Wire `parse_converge_item` into the main parser dispatch
- [ ] Wire `check_converge` into the typechecker dispatch (at line 887)
- [ ] Wire `codegen_converge` into the codegen dispatch
- [ ] Add smoketest: `build.kn` with a converge, verify `kain check` passes
- [ ] Add smoketest: verify lane selection, verify mismatch count == 0

================================================================================
## 9. Dependencies
================================================================================

### Direct Dependencies

| Dependency | Why | Layer |
|------------|-----|-------|
| Function typechecking (`check_function`) | Lane bodies are checked as functions | L0 |
| Type resolution (`resolve_type_in_env`) | Parameter and return types | L0 |
| Block type inference (`infer_block_type`) | Lane body return types | L0 |
| String table (`strtab_*`) | Interned selector values | L0 |

### Optional Dependencies

| Dependency | Why | Layer |
|------------|-----|-------|
| Capability probe strings | `capability("gpu.compute")` validated as known capability | L0 |
| World state (via called functions) | Converge lanes can call world functions | L1 |
| Law (via called functions) | Converge lanes can call law functions | L2 |

### Non-Dependencies

Converge does NOT require:
- world/entangle (L1) — converge works without any state authority
- patch/law (L2) — converge works without state integrity
- orchestrate (L4) — converge is independent of stage graphs
- pulse/resonate (L5) — converge doesn't need temporal semantics

**Converge is self-contained.** It depends only on L0 (function signatures, type inference, block bodies). It can call higher-layer functions (world, law, patch) because the lanes are full function bodies, but the converge mechanism itself doesn't require them.

================================================================================
## 10. Test Plan
================================================================================

### Unit Tests (compiler level)

| Test | What it verifies |
|------|-----------------|
| Parse converge with spec + 1 fast lane + verify | Structured AST produced correctly |
| Parse converge with spec + 3 fast lanes (target + capability + no selector) | Multiple lanes, mixed selectors |
| Parse converge without verify | verify_random_count == -1 |
| Parse converge without fast lanes | Error: "requires at least one fast lane" |
| Parse converge with no spec | Error: "requires exactly one spec lane" |
| Parse malformed selector `target  ("x")` | Error: expected `(` |
| Typecheck converge with matching lane signatures | OK |
| Typecheck converge with mismatched return types | Error: fast lane signature doesn't match |
| Typecheck converge with empty capability string | Error or warning |
| Typecheck converge with verify random(0) | OK (zero is legal) |
| Typecheck converge with verify random(-1) | Error: non-negative required |
| Codegen converge produces callable dispatch function | LLVM IR has `define i64 @name(i64)` |
| Codegen converge produces spec lane callable | LLVM IR has `define i64 @name__spec(i64)` |
| Codegen converge produces fast lane callable | LLVM IR has `define i64 @name__fast_lane(i64)` |
| Codegen converge caches lane in static global | LLVM IR has `@__kain_converge_cached_<name>` |
| Codegen converge emits capability probe for avx2 | LLVM IR calls capability function |

### Integration Tests

| Test | What it verifies |
|------|-----------------|
| `kain check` on file with converge | Passes typechecking |
| `kain run` on file with converge (all lanes same impl) | Returns correct result |
| `kain run` on file with converge + verify | Mismatch count == 0 |
| `kain run` on file with `converge_mismatch_count()` | Telemetry call works |
| `kain run` with `capability("cpu.x86.avx2")` on non-AVX2 machine | Falls back to spec lane |

### Example Test File

```kn
use std::runtime
use std::intent

const TEST_MOD: Int = 1000000007

fn mix_scalar(value: Int) -> Int:
    return ((value * 31) + 7) % TEST_MOD

converge mix(value: Int) -> Int:
    spec reference:
        return mix_scalar(value)
    fast llvm_lane when target("llvm"):
        return ((value * 31) + 7) % TEST_MOD
    verify random(4)

pub fn test_converge() -> Int:
    let result = mix(42)
    let expected = mix_scalar(42)
    if result != expected:
        return 1
    if converge_mismatch_count() != 0:
        return 2
    return 0
```

================================================================================
## Appendix: Current vs Required State

| Aspect | Current (Self-Host Stub) | Required |
|--------|-------------------------|----------|
| AST data layout | `[name, params..., ret, body]` — flat, no structure | `[name, ret, params_node, spec_lane, count, lanes..., verify]` — structured |
| Spec lane | Not distinguished from body | Dedicated child node |
| Fast lanes | Not parsed | One or more child nodes |
| Selectors | Not parsed | `target("...")` / `capability("...")` parsed |
| Verify random | Not parsed | `verify random(N)` parsed |
| Typechecking | `rt_i64()` stub, no validation | Full signature matching, selector validation |
| Codegen | Nothing | Spec function + fast lane functions + dispatch function with caching |
| Test coverage | None | Unit + integration + litmus test |

================================================================================
