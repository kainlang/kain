# L4 Stage Graph — `orchestrate`: Self-Host Implementation Plan
================================================================================
**Status:** Comprehensive spec for `blades/kain/src/L4_stage.kn`
**Based on:** Rust bootstrap at `crates/core/src/parser.rs`, `crates/core/src/ast.rs`,
              `crates/core/src/types.rs`, `crates/orchestrate/src/` (4 files),
              `crates/sys-codegen/src/codegen_llvm/mod.rs`, `crates/core/src/runtime_contract.rs`,
              `docs/ORCHESTRATE.MD`, `benchmark/cases_v2/orchestrate_god.kn`,
              self-host parser/types stubs.

================================================================================
## 1. Architecture Overview
================================================================================

`orchestrate` is a top-level declaration implementing a **typed multi-stage computation
pipeline** — a graph of named stages that the compiler validates and the runtime
telemetry-tracks.

```
orchestrate name(params) -> ReturnType:
    stage <binding>: <runtime> <function>(<args>)
        [when <selector>]
        [after <dep> | deps [<dep>, ...]]
        [residency <host | shared | device>]
        [transfer <none | host_to_device | device_to_host | shared_view>]
        [guarded by <axiom_name>]
        [fallback <abort | <stage> | degrade <stage>>]
        [requires <law_stage>]
        [policy <static | telemetry_prefer_gpu | telemetry_prefer_cpu | telemetry_balance_latency>]
    ...
    return <expr>
```

### 12 Stage Runtimes

| Kind | Keyword | Group | Silicon Native? |
|------|---------|-------|----------------|
| CPU | `cpu` | Silicon-native | Yes |
| GPU | `gpu` | Silicon-native | Yes |
| Dispatch | `dispatch` | Silicon-native | Yes |
| Converge | `converge` | Silicon-native | Yes |
| Law | `law` | Silicon-native | Yes |
| Patch | `patch` | Silicon-native | Yes |
| World | `world` | Silicon-native | Yes |
| C | `c` | Primary interop | No |
| Python | `python` | Primary interop | No |
| Rust | `rust` | Compat adapter | No |
| Node | `node` | Compat adapter | No |
| Kain | `kain` | Ungrouped | Yes (via call) |

### What the Compiler Owns

1. **Dependency validation** — the compiler detects cycles, unknown stage names, duplicate names.
2. **Transfer/residency compatibility** — `host_to_device` + `residency host` is rejected.
3. **Guard type checking** — `guarded by` must reference an axiom, not a function.
4. **Requires validation** — `requires` must reference a law stage.
5. **Fallback validation** — fallback target must be a valid stage name.
6. **Stage ordering** — stages must be declared before local variable statements.
7. **Telemetry generation** — `abi_orchestrate_stage_begin_graph()` records every stage call.

### Three Authoring Styles (from benchmarks)

| Style | Metadata | Example |
|-------|----------|---------|
| Selector-only | Only `when capability(...)` | `orchestration.kn` — 7 stages, 6 runtime kinds |
| Minimal guard | Stage + when + `if legal == false` | `gpu_cpu_pipeline.kn` — 2 stages |
| God mode | ALL metadata clauses | `orchestrate_god.kn` — 8 stages, 9 runtimes |

================================================================================
## 2. AST Representation
================================================================================

### Rust AST (from `crates/core/src/ast.rs:469`)

```rust
pub struct OrchestrateDef {
    pub name: String,
    pub params: Vec<Param>,
    pub return_type: Option<Type>,
    pub body: Block,              // Stage declarations + return expr
    pub visibility: Visibility,
    pub attributes: Vec<Attribute>,
    pub span: Span,
}
```

**Key design decision:** The orchestrate body is a `Block` containing:
1. Stage declaration statements (parsed as special statements within the block)
2. Local variable assignments (e.g., `if legal == false: return ...`)
3. A trailing return expression

The stage declarations are extracted from the block during typechecking by
`collect_orchestrate_stage_descriptors()` in `types.rs:6996`.

### Rust Stage Kind Types (from `crates/orchestrate/src/stage.rs`)

```rust
pub enum OrchestrateStageKind {
    Kain, C, Cpu, Gpu, Dispatch, Converge, Law, Patch, World, Python, Rust, Node,
}

pub enum OrchestrateSelector {
    Capability(String),
    Target(String),
}

pub enum OrchestrateResidency {
    Host, Shared, Device,
}

pub enum OrchestrateTransfer {
    None, HostToDevice, DeviceToHost, SharedView,
}

pub enum OrchestrateFallback {
    Abort,
    Stage(String),
    Degrade(String),
}

pub enum OrchestratePlannerPolicy {
    Static,
    TelemetryPreferGpu,
    TelemetryPreferCpu,
    TelemetryBalanceLatency,
}
```

### Rust Graph Metadata (from `crates/orchestrate/src/graph.rs`)

```rust
pub struct OrchestrateStageGraphMetadata {
    pub dependencies: Vec<String>,      // after / deps
    pub residency: Option<OrchestrateResidency>,
    pub transfer: Option<OrchestrateTransfer>,
    pub guard: Option<String>,           // axiom name
    pub fallback: Option<OrchestrateFallback>,
    pub requires: Option<String>,        // law stage name
    pub policy: Option<OrchestratePlannerPolicy>,
}
```

### Proposed Self-Host AST Layout

Since orchestrate's body is a `Block` containing stage declaration statements,
the AST structure in the self-host compiler should be:

```
AST_ITEM_ORCHESTRATE (kind=18)
  data[0] = name_idx
  data[1] = return_type_idx (-1 if none)
  data[2] = params_node_idx
  data[3] = body_block_idx       # The full body block
  data[4] = stage_count
  data[5..5+N] = stage_descriptor_node_idx for each stage
```

Each AST_STAGE_DESCRIPTOR (new: 221):
```
  data[0] = binding_name_idx
  data[1] = runtime_kind (0=kain, 1=c, 2=cpu, 3=gpu, 4=dispatch, 5=converge,
                          6=law, 7=patch, 8=world, 9=python, 10=rust, 11=node)
  data[2] = function_name_idx      # the function called in this stage
  data[3] = selector_kind (0=none, 1=target, 2=capability)
  data[4] = selector_value_idx (string table, -1 if none)
  data[5] = dependencies_count
  data[6..6+N] = dependency_name_idx for each dep
  data[6+N] = residency_kind (0=unspecified, 1=host, 2=shared, 3=device)
  data[7+N] = transfer_kind (0=unspecified, 1=none, 2=host_to_device, 3=device_to_host, 4=shared_view)
  data[8+N] = guard_axiom_idx (-1 if none)
  data[9+N] = fallback_kind (0=unspecified, 1=abort, 2=stage, 3=degrade)
  data[10+N] = fallback_target_idx (-1 if none)
  data[11+N] = requires_law_idx (-1 if none)
  data[12+N] = policy_kind (0=unspecified, 1=static, 2=telemetry_prefer_gpu,
                             3=telemetry_prefer_cpu, 4=telemetry_balance_latency)
```

### Current Self-Host Status

Current `AST_ITEM_ORCHESTRATE` data layout in `parser.kn:2936`:
```
data[0] = body_idx   # just a block — no name, no params, no stages
```

**Critical gap:** The parser skips the orchestrate name entirely:
```kn
pub fn parse_orchestrate_item(st, vis_val, attrs) -> ParseResult:
    let sp = parser_current_span(st)
    let cur = parser_advance(st)                     # skip 'orchestrate'
    if parser_check(cur, TOKEN_IDENT):               # skip name
        cur = parser_advance(cur)
    if parser_check(cur, TOKEN_COLON):               # expect ':'
        cur = parser_advance(cur)
    let br = parse_block_or_expr(cur)                # parse whole body as block
    # data[0] = body_idx
    # This discards: name, params, return type, stage structure
```

The parser:
1. Discards the orchestrate name (reads it but stores nothing)
2. Discards params (doesn't parse `(...)`)
3. Discards return type (doesn't parse `-> Type`)
4. Treats the body as a generic block, losing all stage metadata

================================================================================
## 3. Rust Bootstrap Reference
================================================================================

### 3.1 Parser (`crates/core/src/parser.rs`)

| Function | Lines | Description |
|----------|-------|-------------|
| `parse_orchestrate()` | 1927-1954 | Parses `orchestrate name(params) -> Type:\n body`. Stores body as `Block`. **The stage declarations are NOT parsed as special syntax at the parser level** — they're parsed as regular statement nodes within the block. The typechecker extracts them later. |
| `parse_orchestrate_stage_runtime()` | 224 | Maps stage runtime name string to `OrchestrateStageRuntime` enum. |

**Key Rust design note:** The Rust parser does NOT parse stage declarations specially. They're parsed as regular block statements with a special `stage` prefix. The typechecker's `collect_orchestrate_stage_descriptors()` walks the block statements and extracts stage metadata. This means the self-host parser could potentially do the same — parse `stage <name>: <runtime> <fn>(<args>) <metadata>` as a special statement within the block.

### 3.2 Orchestrate Crate (`crates/orchestrate/src/`)

| File | Key Types | Description |
|------|-----------|-------------|
| `lib.rs` (20 lines) | `OrchestrateError` | Re-exports all types from graph, planner, stage modules. Two error variants: `UnknownStageKind(String)` and `InvalidGraph(String, String)`. |
| `stage.rs` (219 lines) | `OrchestrateStageKind` (12 variants), `OrchestrateSelector`, `OrchestrateResidency`, `OrchestrateTransfer`, `OrchestrateFallback`, `OrchestrateStagePlan` | 12 stage runtime kinds with classification methods (`is_silicon_native()`, `is_primary_interop()`, `is_compat_adapter()`). `OrchestrateStagePlan` holds a binding name, kind, function, selector, and metadata. |
| `graph.rs` (187 lines) | `OrchestrateStageGraphMetadata`, `OrchestrateGraphPlan`, `OrchestrateGraphValidation` | Metadata has 7 optional fields (deps, residency, transfer, guard, fallback, requires, policy). GraphPlan has name + stage list. Validation checks: duplicate names, unknown deps, unknown requires, unknown fallback targets, cycle detection. |
| `planner.rs` (41 lines) | `OrchestratePlannerPolicy` (4 variants) | Static, TelemetryPreferGpu, TelemetryPreferCpu, TelemetryBalanceLatency. `adaptive()` returns true for all non-Static. |

**Graph validation logic** (`graph.rs:94-139`, `from_plan()`):
1. Collect all stage names into a HashSet
2. For each stage: check duplicate name, unknown deps, unknown requires target, unknown fallback target
3. For each stage: run `has_cycle()` DFS cycle detection
4. Return `OrchestrateGraphValidation { valid, diagnostics }`

**Cycle detection** (`graph.rs:139-157`, `has_cycle()`):
Classic DFS with `visiting` (gray set) and `visited` (black set). If we revisit a node in the `visiting` set, there's a cycle.

### 3.3 Typechecker (`crates/core/src/types.rs`)

| Function | Lines | Description |
|----------|-------|-------------|
| `check_orchestrate()` | 6347-6380 | Main entry: type-checks the function body via `orchestrate_function_view()`, then collects stage descriptors via `collect_orchestrate_stage_descriptors()`, then builds + validates the graph plan via `build_orchestrate_graph_plan()`. Returns `TypedOrchestrate { ast, resolved_type, stages, graph }`. |
| `orchestrate_function_view()` | 5904-5918 | Creates a `Function` view of the orchestrate's body for typechecking. The orchestrate is treated as a function with the named body. |
| `collect_orchestrate_stage_descriptors()` | 6996 | Walks the orchestrate's body block statements, finds `stage` declarations, and returns `Vec<OrchestrateStageDescriptor>`. |
| `build_orchestrate_graph_plan()` | 6362-6392 | Creates an `OrchestrateGraphPlan` from stage descriptors. Calls `validate_orchestrate_stage_metadata()` for each stage, then `graph.validate()` for full DAG validation. |
| `validate_orchestrate_stage_metadata()` | 6392-6440+ | Per-stage metadata validation: (1) guard must reference an axiom, (2) transfer/residency compatibility. In interpret mode, violations are warnings. |

**TypedOrchestrate** (`types.rs:218`):
```rust
pub struct TypedOrchestrate {
    pub ast: OrchestrateDef,
    pub resolved_type: ResolvedType,
    pub stages: Vec<OrchestrateStageDescriptor>,
    pub graph: OrchestrateGraphPlan,
}
```

**OrchestrateStageDescriptor** (`types.rs:212`):
```rust
pub struct OrchestrateStageDescriptor {
    pub runtime: OrchestrateStageRuntime,
    pub function: String,
    pub binding_name: String,
    pub selector: Option<OrchestrateSelector>,
    pub metadata: OrchestrateStageGraphMetadata,
}
```

### 3.4 Runtime Contract (`crates/core/src/runtime_contract.rs`)

| Struct | Lines | Description |
|--------|-------|-------------|
| `RuntimeOrchestrationContract` | 269+ | `{ name, return_type, graph_mode, adaptive_policy, stages }` |
| `RuntimeOrchestrationStageContract` | 243-267 | `{ runtime, kind, function, binding_name, selector, dependencies, residency, transfer, guard, fallback, requires, policy, adaptive_policy, silicon_native, compatibility_adapter }` |

### 3.5 LLVM Codegen (`crates/sys-codegen/src/codegen_llvm/mod.rs`)

| Function | Lines | Description |
|----------|-------|-------------|
| `compile_orchestrate()` | 16114-16120 | **Strikingly simple:** treats the orchestrate body as a named callable. The stage metadata recording happens INSIDE the block body by calling `abi_orchestrate_stage_begin_graph()` at each stage call site. The body is compiled as a normal function. |
| `emit_machine_stones_entry_preamble()` | 16120-16140 | Emits startup code for axioms and pulses. |

The key insight from the Rust codegen: orchestrate doesn't generate special pipeline infrastructure in LLVM IR. Instead, the stage calls inside the body emit `abi_orchestrate_stage_begin_graph()` runtime ABI calls that record metadata and increment telemetry counters. The actual computation for each stage is a normal function call.

### 3.6 Native C Runtime (`stdlib_abi.h`, `stdlib_abi.c`)

The C runtime maintains 16 global variables for orchestrate telemetry, populated by `abi_orchestrate_stage_begin_graph()`:

| Variable | Type | Meaning |
|----------|------|---------|
| `stage_count` | `int64_t` | Total stage calls |
| `transfer_count` | `int64_t` | Stages with non-default transfer policy |
| `fallback_count` | `int64_t` | Stages with non-default fallback |
| `adaptive_stage_count` | `int64_t` | Stages with adaptive policy |
| `last_runtime` | `char[128]` | Last stage's runtime kind |
| `last_function` | `char[256]` | Last stage's function name |
| ... 10 more `last_*` fields | string | Last stage's metadata |

ABI signature:
```c
int64_t abi_orchestrate_stage_begin_graph(
    const char* runtime_name,
    const char* function_name,
    const char* selector_name,
    const char* dependency_names,
    const char* residency_name,
    const char* transfer_name,
    const char* guard_name,
    const char* fallback_name,
    const char* requires_name,
    const char* policy_name
);
```

================================================================================
## 4. Parser Status
================================================================================

### Current: SEVERELY INCOMPLETE STUB

Current `parse_orchestrate_item()` at `parser.kn:2936`:
```kn
pub fn parse_orchestrate_item(st, vis_val, attrs) -> ParseResult:
    let sp = parser_current_span(st)
    let cur = parser_advance(st)                      # skip 'orchestrate'
    # NAME IS DISCARDED
    if parser_check(cur, TOKEN_IDENT):
        cur = parser_advance(cur)
    # PARAMS AND RETURN TYPE NOT PARSED
    if parser_check(cur, TOKEN_COLON):
        cur = parser_advance(cur)
        let br = parse_block_or_expr(cur)              # whole body as flat block
        body_idx = br.node
    # data[0] = body_idx — everything else lost
```

### Required Rewrite

```kn
pub fn parse_orchestrate_item(st, vis_val, attrs) -> ParseResult:
    let sp = parser_current_span(st)
    let cur = parser_advance(st)     # skip 'orchestrate'

    # 1. Parse name
    let name_tok = parser_current(cur)
    if name_tok.kind != TOKEN_IDENT: return error
    cur = parser_advance(cur)
    let name_ir = parser_intern(cur, name_tok.text)
    cur = name_ir.state

    # 2. Parse params
    if parser_check(cur, TOKEN_LPAREN):
        cur = parser_advance(cur)
        # ... parse params similar to function params ...
        if parser_check(cur, TOKEN_RPAREN):
            cur = parser_advance(cur)

    # 3. Parse return type
    var ret_idx = -1
    if parser_check(cur, TOKEN_ARROW):
        cur = parser_advance(cur)
        let rr = parse_type(cur)
        cur = rr.state
        ret_idx = rr.node

    # 4. Parse body
    if parser_check(cur, TOKEN_COLON):
        cur = parser_advance(cur)

    # 5. Parse stage declarations — expect indent block
    if parser_check(cur, TOKEN_INDENT) or parser_check(cur, TOKEN_NEWLINE):
        # ... parse stage declarations, then local statements, then return expr ...

    # 6. Collect stage descriptors
    var stage_nodes: Array<Int> = []
    # ... parse each `stage <name>: <runtime> <fn>(args) [metadata...]` ...
    # For each stage, parse:
    #   stage <binding>: <runtime_keyword> <function_ident>(<args>)
    #     [when target("...") | capability("...")]
    #     [after <name> | deps [a, b, ...]]
    #     [residency host|shared|device]
    #     [transfer none|host_to_device|device_to_host|shared_view]
    #     [guarded by <axiom>]
    #     [fallback abort|<name>|degrade <name>]
    #     [requires <name>]
    #     [policy static|telemetry_prefer_gpu|telemetry_prefer_cpu|telemetry_balance_latency]

    # 7. Build AST_ITEM_ORCHESTRATE node
    let data = []
    data.push(name_idx)
    data.push(ret_idx)
    data.push(params_node_idx)
    data.push(body_block_idx)
    data.push(len(stage_nodes))
    var si = 0
    while si < len(stage_nodes):
        data.push(stage_nodes[si])
        si = si + 1
    let node = ast_new_node(AST_ITEM_ORCHESTRATE, sp.start, end_off, data)
    return parser_push_result(cur, node)
```

### Stage Parser Helpers Required

| Helper | What it parses |
|--------|---------------|
| `parse_orchestrate_stage()` | `stage <binding>: <runtime> <fn>(<args>)` + metadata clauses |
| `parse_orchestrate_runtime()` | `cpu`, `gpu`, `kain`, `converge`, `law`, `patch`, `world`, `dispatch`, `c`, `python`, `rust`, `node` — map to runtime_kind int |
| `parse_orchestrate_selector()` | `when target("...")` / `when capability("...")` |
| `parse_orchestrate_after_deps()` | `after <name>` / `deps [<name>, ...]` |
| `parse_orchestrate_residency()` | `residency host|shared|device` |
| `parse_orchestrate_transfer()` | `transfer none|host_to_device|device_to_host|shared_view` |
| `parse_orchestrate_guard()` | `guarded by <axiom_name>` |
| `parse_orchestrate_fallback()` | `fallback abort|<name>|degrade <name>` |
| `parse_orchestrate_requires()` | `requires <law_stage_name>` |
| `parse_orchestrate_policy()` | `policy static|telemetry_prefer_gpu|...` |

================================================================================
## 5. Typechecker Plan
================================================================================

### Current: TRUE STUB

`check_orchestrate_stub()` at `types.kn:1617`:
```kn
pub fn check_orchestrate_stub(env: TypeEnv, node: AstNode, idx: Int) -> TypedItemAndEnv:
    let name_idx = if ast_data_len(node) > 0: ast_data_get(node, 0) else: -1
    return TypedItemAndEnv {
        env: env,
        item: TypedItem {
            kind: AST_ITEM_ORCHESTRATE, name: "orch_" + str(name_idx), name_idx: name_idx,
            resolved_type: rt_i64(), ast_index: idx, effects: EFF_PURE,
        }
    }
```

Always returns `rt_i64()`, no stage validation, no graph validation, no metadata checks.

### Required Implementation

```kn
pub fn check_orchestrate(env: TypeEnv, node: AstNode, idx: Int) -> TypedItemAndEnv:
    # Step 1: Extract AST fields
    let name_idx = ast_data_get(node, 0)
    let ret_type_idx = ast_data_get(node, 1)
    let params_node_idx = ast_data_get(node, 2)
    let body_block_idx = ast_data_get(node, 3)
    let stage_count = ast_data_get(node, 4)

    # Step 2: Resolve return type (default Unit)
    let return_type = if ret_type_idx >= 0:
        resolve_type_in_env(env, ret_type_idx)
    else:
        rt_unit()

    # Step 3: Collect stage names for validation
    var stage_names: Array<Int> = []
    var deps_map: HashMap<Int, Array<Int>> = HashMap:new()

    var i = 0
    while i < stage_count:
        let stage_node = ast_data_get(node, 5 + i)
        let binding_name_idx = ast_data_get(stage_node, 0)
        let runtime_kind = ast_data_get(stage_node, 1)
        let fn_name_idx = ast_data_get(stage_node, 2)

        # Validate runtime kind (0-11)
        if runtime_kind < 0 or runtime_kind > 11:
            env.report_error("orchestrate stage '" + str(binding_name_idx) +
                "' has unknown runtime kind")

        # Validate silicon-native stage kinds
        # (cpu=2, gpu=3, dispatch=4, converge=5, law=6, patch=7, world=8 are silicon-native)

        # Check for duplicate names
        if stage_names_contains(stage_names, binding_name_idx):
            env.report_error("orchestrate has duplicate stage '" +
                str(binding_name_idx) + "'")

        stage_names.push(binding_name_idx)

        # Extract dependencies (from data offset 5)
        let dep_count = ast_data_get(stage_node, 5)
        var deps: Array<Int> = []
        var j = 0
        while j < dep_count:
            let dep_idx = ast_data_get(stage_node, 6 + j)
            deps.push(dep_idx)
            j = j + 1
        deps_map.put(binding_name_idx, deps)

        # Validate guard (must reference an axiom)
        let guard_idx = ast_data_get(stage_node, 8 + dep_count)
        if guard_idx >= 0:
            # Look up in global origins table — must be an axiom
            let guard_name = str(guard_idx)
            if env.global_origin_exists(guard_name) == false
            or env.global_origin_kind(guard_name) != "axiom":
                env.report_error("orchestrate stage '" + str(binding_name_idx) +
                    "' guard '" + guard_name + "' must reference an axiom")

        # Validate requires (must reference a law stage in this orchestrate)
        let requires_idx = ast_data_get(stage_node, 11 + dep_count)
        if requires_idx >= 0:
            if stage_names_contains(stage_names, requires_idx) == false:
                env.report_error("orchestrate stage '" + str(binding_name_idx) +
                    "' requires unknown law stage '" + str(requires_idx) + "'")

        # Validate transfer/residency compatibility
        let residency_kind = ast_data_get(stage_node, 6 + dep_count)
        let transfer_kind = ast_data_get(stage_node, 7 + dep_count)
        if transfer_kind == 2 and residency_kind == 1:   # host_to_device + host
            env.report_error("orchestrate stage '" + str(binding_name_idx) +
                "' transfer 'host_to_device' is incompatible with residency 'host'")
        if transfer_kind == 3 and residency_kind == 3:   # device_to_host + device
            env.report_error("orchestrate stage '" + str(binding_name_idx) +
                "' transfer 'device_to_host' is incompatible with residency 'device'")

        # Validate fallback target
        let fallback_kind = ast_data_get(stage_node, 9 + dep_count)
        let fallback_target = ast_data_get(stage_node, 10 + dep_count)
        if fallback_kind == 2 or fallback_kind == 3:  # stage or degrade
            if stage_names_contains(stage_names, fallback_target) == false:
                env.report_error("orchestrate stage '" + str(binding_name_idx) +
                    "' fallback references unknown stage '" + str(fallback_target) + "'")

        i = i + 1

    # Step 4: Cycle detection
    var visiting: Array<Int> = []
    var visited: Array<Int> = []
    i = 0
    while i < stage_count:
        let binding_idx = ast_data_get(node, 5 + i)
        if has_orchestrate_cycle(binding_idx, deps_map, visiting, visited):
            env.report_error("orchestrate stage '" + str(binding_idx) +
                "' participates in a dependency cycle")
        i = i + 1

    # Step 5: Type-check the body block as function body
    # (stages' function calls and return expr are checked here)
    let body_result = infer_block_type(env, body_block_idx)

    # Step 6: Return typed item
    return TypedItemAndEnv {
        env: env,
        item: TypedItem {
            kind: AST_ITEM_ORCHESTRATE,
            name: "orch_" + str(name_idx),
            name_idx: name_idx,
            resolved_type: return_type,
            ast_index: idx,
            effects: EFF_PURE,  # orchestrate effects depend on stage kinds
        }
    }
```

### Validation Checklist

| Check | Source | Error |
|-------|--------|-------|
| Stage runtime kind valid | `stage_node[1]` | "unknown runtime kind" |
| Duplicate stage names | `stage_names` set | "duplicate stage 'X'" |
| Unknown dependency | `deps_map` lookup | "depends on unknown stage 'X'" |
| Unknown requires target | `stage_names` lookup | "requires unknown law stage 'X'" |
| Unknown fallback target | `stage_names` lookup | "fallback references unknown stage 'X'" |
| Guard is an axiom | `env.global_origin_kind()` | "guard must reference an axiom" |
| Guard resolves | `env.global_origin_exists()` | "guard does not resolve" |
| Transfer/residency compatible | Compatibility matrix | "incompatible" |
| Dependency cycle | DFS on deps_map | "cycle detected" |

### Transfer/Residency Compatibility Matrix

| Transfer \ Residency | Host (1) | Shared (2) | Device (3) |
|---------------------|----------|------------|------------|
| None (0) | OK | OK | OK |
| HostToDevice (2) | **INCOMPATIBLE** | OK | OK |
| DeviceToHost (3) | OK | OK | **INCOMPATIBLE** |
| SharedView (4) | OK | OK | OK |

================================================================================
## 6. Codegen Plan
================================================================================

### Current: NOTHING

`codegen.kn` has zero references to orchestrate.

### Required Implementation

The Rust codegen reveals a key insight: **orchestrate body is compiled as a normal function body**. The magic is in the stage call ABI — each stage function call is wrapped by `abi_orchestrate_stage_begin_graph()` calls that record metadata.

```kn
# In src/L4_stage.kn — codegen section
pub fn codegen_orchestrate(env: CodegenEnv, node: AstNode, idx: Int) -> CodegenResult:
    let name_idx = ast_data_get(node, 0)
    let ret_type_idx = ast_data_get(node, 1)
    let params_node_idx = ast_data_get(node, 2)
    let body_block_idx = ast_data_get(node, 3)
    let stage_count = ast_data_get(node, 4)

    # Step 1: Emit the orchestrate as a named callable
    # The body block contains stage declarations that emit telemetry calls
    # and function calls. This is compiled as a normal function.
    codegen_named_callable(env, name_idx, params_node_idx, body_block_idx)

    # Step 2: For each stage, emit stage begin/end telemetry wrappers
    # These are NOT separate functions — they're inline calls within the body.
    # The actual body compilation handles this during codegen of the body block.

    return CodegenResult { env: env }
```

### Stage Call Codegen Pattern

Inside the orchestrate body, each `stage <binding>: <runtime> <fn>(<args>):` statement should codegen to:

```llvm
; Stage begin telemetry
call void @abi_orchestrate_stage_begin_graph(
    i8* getelementptr("gpu"),              ; runtime_name
    i8* getelementptr("my_function"),      ; function_name
    i8* getelementptr("capability(..."")"), ; selector
    i8* getelementptr("dep1,dep2"),        ; dependencies
    i8* getelementptr("device"),           ; residency
    i8* getelementptr("host_to_device"),   ; transfer
    i8* getelementptr("my_axiom"),         ; guard
    i8* getelementptr("degrade fallback"), ; fallback
    i8* getelementptr("legal_stage"),      ; requires
    i8* getelementptr("telemetry_prefer_gpu") ; policy
)

; Actual stage function call
%result = call i64 @my_function(i64 %arg)

; Stage end telemetry (for tracking return value)
call void @abi_orchestrate_stage_end_i64(i64 %result)
```

### What the Codegen Must Track

For each stage, the codegen needs to emit the correct metadata strings. Since the AST stores everything as integer indices (string table, enum values), the codegen must:

1. Convert `runtime_kind` int to string: `cpu`, `gpu`, `kain`, etc.
2. Look up interned string indices for function name, selector, deps, guard, fallback, requires
3. Format dependency list as comma-separated string
4. Look up residency/transfer/fallback/policy enum values as strings
5. Emit `abi_orchestrate_stage_begin_graph()` call with ALL metadata before the stage function call
6. Emit `abi_orchestrate_stage_end_i64()` after the stage function call

### ABI Calls Required

| ABI Function | Purpose |
|-------------|---------|
| `abi_orchestrate_stage_begin_graph(...)` | Records stage metadata, increments counters |
| `abi_orchestrate_stage_end_i64(i64)` | Records stage end, passes through result |
| `orchestrate_stage_count()` | Telemetry: total stage calls (stdlib function) |
| `orchestrate_transfer_count()` | Telemetry: stages with transfers |
| `orchestrate_fallback_count()` | Telemetry: stages with fallbacks |
| `orchestrate_adaptive_stage_count()` | Telemetry: stages with adaptive policies |

================================================================================
## 7. Runtime Contract
================================================================================

### Stdlib Telemetry Functions (from `stdlib/intent.kn`)

| Function | Signature | What it does |
|----------|-----------|-------------|
| `orchestrate_stage_count()` | `() -> Int` | Total stage executions |
| `orchestrate_transfer_count()` | `() -> Int` | Stages with meaningful transfer |
| `orchestrate_fallback_count()` | `() -> Int` | Stages with meaningful fallback |
| `orchestrate_adaptive_stage_count()` | `() -> Int` | Stages with adaptive policies |
| `orchestrate_last_runtime()` | `() -> String` | Last stage's runtime kind |
| `orchestrate_last_function()` | `() -> String` | Last stage's function name |
| `orchestrate_last_selector()` | `() -> String` | Last selector |
| `orchestrate_last_dependencies()` | `() -> String` | Last dependency list |
| `orchestrate_last_residency()` | `() -> String` | Last residency |
| `orchestrate_last_transfer()` | `() -> String` | Last transfer |
| `orchestrate_last_guard()` | `() -> String` | Last guard axiom |
| `orchestrate_last_fallback()` | `() -> String` | Last fallback |
| `orchestrate_last_requires()` | `() -> String` | Last requires law |
| `orchestrate_last_policy()` | `() -> String` | Last policy |

### Runtime Contract Emissions

```json
{
  "orchestrations": [
    {
      "name": "fusion_signal_pipeline",
      "return_type": "Int",
      "graph_mode": true,
      "adaptive_policy": false,
      "stages": [
        {
          "runtime": "cpu",
          "kind": "cpu",
          "function": "fusion_mix",
          "binding_name": "host_mix",
          "selector": "capability(\"cpu.scalar\")",
          "dependencies": [],
          "residency": "host",
          "transfer": null,
          "guard": null,
          "fallback": null,
          "requires": null,
          "policy": "telemetry_prefer_cpu",
          "adaptive_policy": true,
          "silicon_native": true,
          "compatibility_adapter": false
        }
      ]
    }
  ],
  "capabilities": ["orchestrate.pipeline"]
}
```

================================================================================
## 8. Implementation Tasks
================================================================================

### Phase 1: AST Extensions (in `ast.kn`)

- [ ] Add `AST_STAGE_DESCRIPTOR = 221` constant
- [ ] Add runtime kind constants: `STAGE_RUNTIME_KAIN=0`, `STAGE_RUNTIME_C=1`, `STAGE_RUNTIME_CPU=2`, `STAGE_RUNTIME_GPU=3`, `STAGE_RUNTIME_DISPATCH=4`, `STAGE_RUNTIME_CONVERGE=5`, `STAGE_RUNTIME_LAW=6`, `STAGE_RUNTIME_PATCH=7`, `STAGE_RUNTIME_WORLD=8`, `STAGE_RUNTIME_PYTHON=9`, `STAGE_RUNTIME_RUST=10`, `STAGE_RUNTIME_NODE=11`
- [ ] Add residency constants: `RES_HOST=1`, `RES_SHARED=2`, `RES_DEVICE=3`
- [ ] Add transfer constants: `XFER_NONE=1`, `XFER_HOST_TO_DEVICE=2`, `XFER_DEVICE_TO_HOST=3`, `XFER_SHARED_VIEW=4`
- [ ] Add fallback constants: `FB_ABORT=1`, `FB_STAGE=2`, `FB_DEGRADE=3`
- [ ] Add policy constants: `POLICY_STATIC=1`, `POLICY_PREFER_GPU=2`, `POLICY_PREFER_CPU=3`, `POLICY_BALANCE=4`
- [ ] Update documentation comment for `AST_ITEM_ORCHESTRATE` with new data layout

### Phase 2: Parser (in `parser.kn`)

- [ ] Rewrite `parse_orchestrate_item()` to parse name, params, return type, structured body
- [ ] Create `parse_orchestrate_stage()` — parse a single stage declaration
- [ ] Create `parse_orchestrate_runtime()` — map runtime keyword to enum int
- [ ] Create `parse_orchestrate_selector()` — parse `when target(...)` / `capability(...)`
- [ ] Create `parse_orchestrate_after_deps()` — parse `after X` / `deps [A, B, ...]`
- [ ] Create `parse_orchestrate_residency()` — parse `residency host|shared|device`
- [ ] Create `parse_orchestrate_transfer()` — parse `transfer none|host_to_device|...`
- [ ] Create `parse_orchestrate_guard()` — parse `guarded by <axiom>`
- [ ] Create `parse_orchestrate_fallback()` — parse `fallback abort|name|degrade name`
- [ ] Create `parse_orchestrate_requires()` — parse `requires <law_stage>`
- [ ] Create `parse_orchestrate_policy()` — parse `policy static|telemetry_*`

### Phase 3: Typechecker (in `types.kn`)

- [ ] Replace `check_orchestrate_stub()` with `check_orchestrate()`:
  - Extract stage descriptors from AST data
  - Validate stage runtime kinds (0-11)
  - Check duplicate stage names
  - Validate dependencies resolve to known stages
  - Validate guard references an axiom
  - Validate requires references a law stage
  - Validate fallback targets are known stages
  - Validate transfer/residency compatibility
  - Run cycle detection DFS
  - Type-check the body block
  - Return typed item with correct return type
- [ ] Create `has_orchestrate_cycle()` helper for DFS cycle detection
- [ ] Create `stage_names_contains()` helper for name resolution
- [ ] Create `orchestrate_runtime_name()` helper for diagnostics

### Phase 4: Codegen (`src/L4_stage.kn`)

- [ ] Create `codegen_orchestrate()` entry point
- [ ] Create `codegen_orchestrate_stage_begin()` — emits `abi_orchestrate_stage_begin_graph()` with all metadata
- [ ] Create `codegen_orchestrate_stage_end()` — emits stage end telemetry
- [ ] Wire stage metadata string generation (enum to string, interned lookups)
- [ ] Wire into the body codegen: when encountering a stage call in the body, emit begin/end wrappers

### Phase 5: Integration

- [ ] Wire `parse_orchestrate_item` into the main parser dispatch (line 2936)
- [ ] Wire `check_orchestrate` into the typechecker dispatch (line 889)
- [ ] Wire `codegen_orchestrate` into the codegen dispatch
- [ ] Add smoketest: `build.kn` with a simple orchestrate, verify `kain check` passes
- [ ] Add smoketest: verify telemetry counters
- [ ] Add smoketest: verify graph validation catches cycles

================================================================================
## 9. Dependencies
================================================================================

### Layer Dependencies

| Layer | How L4 depends on it |
|-------|---------------------|
| L0 (Plain code) | Stage function calls are normal function typechecking; return type inference |
| L1 (World) | `stage x: world world_fn(...)` — world stage kind reads world state |
| L2 (Law) | `stage x: law law_fn(...)` and `requires law_stage` — law+patch stages |
| L3 (Converge) | `stage x: converge converge_fn(...)` — converge stage kind |
| L6 (Axiom) | `guarded by axiom_name` — guard references an axiom |

### Direct Dependencies

| Dependency | Why |
|------------|-----|
| Function typechecking | Stage function calls are type-checked as function calls |
| Type resolution | Parameter and return types |
| Block type inference | The orchestrate body is a block |
| String table | Interning stage names, function names, metadata strings |
| Axiom resolution (`env.global_origin_kind()`) | Validating guard references an axiom |

### Non-Dependencies

Orchestrate does NOT require:
- L5 (pulse/resonate) — orchestrate works without temporal semantics
- L7 (actor, ownership) — orchestrate doesn't depend on actor model

================================================================================
## 10. Test Plan
================================================================================

### Unit Tests (compiler level)

| Test | What it verifies |
|------|-----------------|
| Parse minimal orchestrate (2 stages, no metadata) | Structured AST with stage descriptors |
| Parse full orchestrate (all metadata clauses) | All fields populated correctly |
| Parse orchestrate with `cpu` runtime | runtime_kind == 2 |
| Parse orchestrate with `gpu` runtime + guard | runtime_kind == 3, guard resolved |
| Parse orchestrate with `converge` stage | runtime_kind == 5 (converge) |
| Parse orchestrate with `law` stage + `requires` | runtime_kind == 6, requires resolved |
| Parse orchestrate with `patch` stage | runtime_kind == 7 |
| Parse orchestrate with `world` stage | runtime_kind == 8 |
| Parse orchestrate with `after` dep | Single dependency parsed |
| Parse orchestrate with `deps [a, b]` | Multiple dependencies parsed |
| Parse orchestrate with `fallback abort` | fallback_kind == 1 |
| Parse orchestrate with `fallback degrade x` | fallback_kind == 3, target = x |
| Parse orchestrate with `policy telemetry_prefer_gpu` | policy_kind == 2 |
| Typecheck valid orchestrate | passes |
| Typecheck duplicate stage names | Error |
| Typecheck unknown dep | Error |
| Typecheck guard that's not an axiom | Error |
| Typecheck transfer/residency mismatch | Error |
| Typecheck cycle in deps | Error |
| Typecheck unknown requires | Error |
| Codegen orchestrate body as callable | LLVM IR function emitted |
| Codegen stage ABI recording call | `abi_orchestrate_stage_begin_graph` in IR |

### Integration Tests

| Test | What it verifies |
|------|-----------------|
| `kain check` on file with orchestrate | Passes with all stages |
| `kain run` with orchestrate + telemetry checks | `orchestrate_stage_count()` returns correct count |
| `kain run` with law + requires guarding a stage | Stage doesn't run when law fails |
| `kain run` with converge stage in orchestrate | Converge lane selected inside pipeline |
| `kain run` with patch stage in orchestrate | World state mutated |

### Example Test File

```kn
use std::runtime
use std::intent

const OM: Int = 1000000007

fn mix_fn(value: Int) -> Int:
    return (value * 17 + 7) % OM

law om_in_bounds(value: Int) -> Bool:
    return value >= 0 and value < OM

orchestrate test_pipeline(value: Int) -> Int:
    stage base: cpu mix_fn(value) when capability("cpu.scalar")
        residency host transfer none policy static
    stage legal: law om_in_bounds(base) after base
        residency host policy static
    if legal == false:
        return 0
    return base

pub fn test_orchestrate() -> Int:
    let before = orchestrate_stage_count()
    let result = test_pipeline(42)
    let delta = orchestrate_stage_count() - before
    if delta < 2:       # 2 stages should have fired
        return 1
    if result != ((42 * 17 + 7) % OM):
        return 2
    return 0
```

### Edge Cases

| Case | Expected |
|------|----------|
| Orchestrate with 0 stages | Error (or treat as body-only) |
| Orchestrate with 100 stages | OK (no hard limit) |
| Orchestrate with `c` runtime without C bridge registered | Warning |
| Orchestrate with `python` runtime without Python bridge | Warning |
| Orchestrate with nested orchestrates (pipeline calling pipeline) | OK — they're function calls |
| Orchestrate with `return` inside stage body | Error (stage body is a call, not a definition) |

================================================================================
## Appendix: Current vs Required State

| Aspect | Current (Self-Host Stub) | Required |
|--------|-------------------------|----------|
| AST data layout | `[body_idx]` — name and params discarded | `[name, ret, params, body, count, stages...]` — full structure |
| Stage descriptors | Not parsed | Dedicated child nodes with all metadata |
| Runtime kind | Not parsed | 12 runtime keywords recognized |
| Selectors | Not parsed | `target("...")` + `capability("...")` |
| Deps/after | Not parsed | `after X` + `deps [A, B]` |
| Residency/transfer | Not parsed | `host`, `device`, `host_to_device`, etc. |
| Guard/requires/fallback | Not parsed | `guarded by`, `requires`, `fallback` |
| Policy | Not parsed | `static`, `telemetry_prefer_*` |
| Typechecking | `rt_i64()` stub, no validation | Full DAG validation + metadata checks |
| Cycle detection | None | DFS on deps_map |
| Codegen | Nothing | Body as callable + ABI stage begin/end calls |
| Test coverage | None | Unit + integration + edge cases |

================================================================================
