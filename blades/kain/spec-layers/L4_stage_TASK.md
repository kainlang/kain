# L4 Stage Graph — orchestrate: Implementation Tasks
================================================================================
**Target:** `src/L4_stage.kn` — new file (~600 lines)
**Depends on:** parser.kn (indent/block parsing, metadata clause parsers), ast.kn (AST_STAGE_DESCRIPTOR + runtime/residency/transfer/fallback/policy constants), types.kn (check_orchestrate_stub replacement), codegen.kn + llvm_ffi.kn (LLVM IR emission + stage ABI)
**Required pre-read:** `X:/blades/kain/spec-layers/L4_stage.md` (full spec), `X:/blades/kain/src/types.kn:1617` (current stub), `X:/blades/kain/src/parser.kn:2936` (current parse stub)
**Design notes:** The Rust bootstrap treats orchestrate's body as a flat `Block` — stage declarations are extracted during typechecking, not parsed as special syntax. The self-host compiler may take a different approach: parse stage declarations as structured child nodes at parse time, keeping the body block for local statements and return expression. The tasks below assume structured child-node approach (similar to how converge lanes are structured).

================================================================================
## Task 1: AST Constants for Stage Descriptors (ast.kn)
================================================================================

**1.1** Add `AST_STAGE_DESCRIPTOR = 221` constant to `ast.kn` (~line 50-100).

**1.2** Add runtime-kind constants (12 runtimes, all silicon-native dispatch through same call path):
```kn
pub const STAGE_RUNTIME_KAIN:     Int = 0
pub const STAGE_RUNTIME_C:        Int = 1
pub const STAGE_RUNTIME_CPU:      Int = 2
pub const STAGE_RUNTIME_GPU:      Int = 3
pub const STAGE_RUNTIME_DISPATCH: Int = 4
pub const STAGE_RUNTIME_CONVERGE: Int = 5
pub const STAGE_RUNTIME_LAW:      Int = 6
pub const STAGE_RUNTIME_PATCH:    Int = 7
pub const STAGE_RUNTIME_WORLD:    Int = 8
pub const STAGE_RUNTIME_PYTHON:   Int = 9
pub const STAGE_RUNTIME_RUST:     Int = 10
pub const STAGE_RUNTIME_NODE:     Int = 11
```

**1.3** Add metadata enum constants:
```kn
pub const RES_HOST:   Int = 1
pub const RES_SHARED: Int = 2
pub const RES_DEVICE: Int = 3

pub const XFER_NONE:            Int = 1
pub const XFER_HOST_TO_DEVICE:  Int = 2
pub const XFER_DEVICE_TO_HOST:  Int = 3
pub const XFER_SHARED_VIEW:     Int = 4

pub const FB_ABORT:   Int = 1
pub const FB_STAGE:   Int = 2
pub const FB_DEGRADE: Int = 3

pub const POLICY_STATIC:      Int = 1
pub const POLICY_PREFER_GPU:  Int = 2
pub const POLICY_PREFER_CPU:  Int = 3
pub const POLICY_BALANCE:     Int = 4
```

**1.4** Add `ast_kind_name()` entry for `AST_STAGE_DESCRIPTOR` → `"StageDescriptor"`.

**1.5** Document the new `AST_ITEM_ORCHESTRATE` data layout (replace current doc at `ast.kn:~300`):
```
AST_ITEM_ORCHESTRATE data layout:
  [0] = name_idx (string table)
  [1] = return_type_idx (-1 if none)
  [2] = params_node_idx (child AST node — reuse or new node kind)
  [3] = body_block_idx (AST_EXPR_BLOCK — local stmts + return expr)
  [4] = stage_descriptor_count (N)
  [5..5+N-1] = stage_descriptor_node_idx for each stage
```

**1.6** Document `AST_STAGE_DESCRIPTOR` child node layout:
```
AST_STAGE_DESCRIPTOR data layout:
  [0] = binding_name_idx
  [1] = runtime_kind (0-11, see STAGE_RUNTIME_* constants)
  [2] = function_name_idx (the function called in this stage)
  [3] = selector_kind (0=none, 1=target, 2=capability)
  [4] = selector_value_idx (-1 if none)
  [5] = dependency_count (D)
  [6..6+D-1] = dependency_name_idx for each dep
  [6+D] = residency_kind (0=unspecified, see RES_*) 
  [7+D] = transfer_kind (0=unspecified, see XFER_*)
  [8+D] = guard_axiom_idx (-1 if none)
  [9+D] = fallback_kind (0=unspecified, see FB_*)
  [10+D] = fallback_target_idx (-1 if none)
  [11+D] = requires_law_idx (-1 if none)
  [12+D] = policy_kind (0=unspecified, see POLICY_*)
```

**Acceptance:** All constants exist and are documented. `ast_kind_name(221)` returns `"StageDescriptor"`.

================================================================================
## Task 2: Parser — Orchestrate Item + Stage Declarations (parser.kn)
================================================================================

**2.1** Replace `parse_orchestrate_item()` body at `parser.kn:2936`:
- Parse `orchestrate <name>(<params>) [-> <type>]:` then expect indent/newline
- Parse zero or more `stage <binding>: <runtime> <function>(<args>)` declarations (call `parse_orchestrate_stage()` for each)
- Parse optional local statements (if-check, let-bindings on stage results)
- Parse trailing return expression
- Expect dedent
- Emit `AST_ITEM_ORCHESTRATE` with new data layout (Task 1.5)

**2.2** Create `parse_orchestrate_stage(st) -> ParseResult`:
- Expect `stage` keyword (contextual ident)
- Parse `binding_name:` ident + colon
- Parse `runtime_keyword` — call `parse_orchestrate_runtime_keyword()` to map keyword to `STAGE_RUNTIME_*` int
- Parse `function_name(` and args (standard function-call expression)
- Parse optional metadata clauses in any order:
  - `when target(...)` / `when capability(...)` → `parse_orchestrate_selector()`
  - `after <name>` / `deps [name, ...]` → `parse_orchestrate_deps()`
  - `residency host|shared|device` → `parse_orchestrate_residency()`
  - `transfer none|host_to_device|device_to_host|shared_view` → `parse_orchestrate_transfer()`
  - `guarded by <name>` → `parse_orchestrate_guard()`
  - `fallback abort|<name>|degrade <name>` → `parse_orchestrate_fallback()`
  - `requires <name>` → `parse_orchestrate_requires()`
  - `policy static|telemetry_prefer_gpu|...` → `parse_orchestrate_policy()`
- Emit `AST_STAGE_DESCRIPTOR` child node (Task 1.6)
- If no metadata clauses after `:`, the stage declaration may end with an implicit newline (the body of the stage is the function call itself, not a block)

**2.3** Create helper `parse_orchestrate_runtime_keyword(st) -> Int`:
- Map contextual idents: `"kain"`→0, `"c"`→1, `"cpu"`→2, `"gpu"`→3, `"dispatch"`→4, `"converge"`→5, `"law"`→6, `"patch"`→7, `"world"`→8, `"python"`→9, `"rust"`→10, `"node"`→11
- Error on unknown runtime keyword

**2.4** Create metadata clause parsers (each callable as optional clauses in any order):

| Helper | Parses | Returns |
|--------|--------|---------|
| `parse_orchestrate_selector(st)` | `when target("str")` / `when capability("str")` | `(kind, value_idx)` |
| `parse_orchestrate_deps(st)` | `after X` (single) or `deps [A, B, C]` (list) | `Array<Int>` of name indices |
| `parse_orchestrate_residency(st)` | `residency host` | `RES_*` int |
| `parse_orchestrate_transfer(st)` | `transfer host_to_device` | `XFER_*` int |
| `parse_orchestrate_guard(st)` | `guarded by axiom_name` | name index |
| `parse_orchestrate_fallback(st)` | `fallback abort` or `fallback <stage>` or `fallback degrade <stage>` | `(kind, target_idx)` |
| `parse_orchestrate_requires(st)` | `requires law_stage_name` | name index |
| `parse_orchestrate_policy(st)` | `policy static` or `policy telemetry_prefer_gpu` | `POLICY_*` int |

**2.5** Wire `parse_orchestrate_item` into the main parser dispatch at `parser.kn:524`. It should already be wired since the stub exists — just ensure the new structured version is called.

**Acceptance:** `kain check` on a file with `orchestrate pipe(v: Int) -> Int: stage base: cpu mix(v) when target("llvm") residency host transfer none policy static return base` produces structured AST with one stage descriptor containing runtime=2 (cpu), residency=1 (host), transfer=1 (none), policy=1 (static). Parsing a valid orchestrate with all metadata clauses produces the correct AST.

================================================================================
## Task 3: Typechecker — Orchestrate Validation (types.kn)
================================================================================

**3.1** Replace `check_orchestrate_stub()` at `types.kn:1617` with `check_orchestrate()`:
- Extract name_idx, ret_type_idx, params_node, body_block, stage_count, stage_nodes from AST data
- Resolve return type: if `ret_type_idx >= 0`, `resolve_type_in_env()`; else `rt_unit()`

**3.2** Stage validation loop (for each stage descriptor):
- Extract binding_name, runtime_kind, function_name, selector, deps, residency, transfer, guard, fallback, requires, policy
- **Runtime-kind check:** if `runtime_kind < 0` or `runtime_kind > 11`, error `ERR_ORCHESTRATE_UNKNOWN_RUNTIME`
- **Duplicate names:** track all binding names in a list; if any repeats, error `ERR_ORCHESTRATE_DUPLICATE_STAGE`
- **Dependencies:** each dep must be either declared earlier in the stage list or resolve to a known stage name. Error if unresolved.
- **Guard validation:** if a guard axiom is specified, look it up in the environment's global-origins table; it must be an axiom (not a function). Error `ERR_ORCHESTRATE_GUARD_NOT_AXIOM`.
- **Requires validation:** if a requires law is specified, it must match a stage name in the orchestrate. Error `ERR_ORCHESTRATE_REQUIRES_UNKNOWN`.
- **Fallback validation:** if fallback kind is `FB_STAGE` or `FB_DEGRADE`, the target must resolve to a known stage name. Error `ERR_ORCHESTRATE_FALLBACK_UNKNOWN`.
- **Transfer/residency compatibility** (see spec matrix):
  - `XFER_HOST_TO_DEVICE` + `RES_HOST` → error `ERR_ORCHESTRATE_XFER_INCOMPATIBLE`
  - `XFER_DEVICE_TO_HOST` + `RES_DEVICE` → error `ERR_ORCHESTRATE_XFER_INCOMPATIBLE`

**3.3** Cycle detection via DFS:
- Build adjacency list from dependency maps
- Run DFS with `visiting` (gray set) and `visited` (black set) tracking
- If a node is encountered while in the `visiting` set, report `ERR_ORCHESTRATE_CYCLE_DETECTED`
- Create helper `has_orchestrate_cycle(binding_idx, deps_map, visiting, visited) -> Bool`

**3.4** Type-check the body block:
- Infer the body block's return type via `infer_block_type(env, body_block_idx)`
- Assert it matches the declared return type. Error `ERR_TYPE_MISMATCH` if different.

**3.5** Wire `check_orchestrate` into the typechecker dispatch at `types.kn:889` (replace `check_orchestrate_stub` call).

**Acceptance:** A valid orchestrate with correct stage definitions passes typechecking. Duplicate stage names produce `ERR_ORCHESTRATE_DUPLICATE_STAGE`. A guard referencing a function (not an axiom) produces `ERR_ORCHESTRATE_GUARD_NOT_AXIOM`. A dependency cycle produces `ERR_ORCHESTRATE_CYCLE_DETECTED`. A host-resident stage with `host_to_device` transfer produces `ERR_ORCHESTRATE_XFER_INCOMPATIBLE`.

================================================================================
## Task 4: Codegen — Orchestrate Body + Stage ABI (new file or codegen.kn)
================================================================================

**4.1** Create `src/L4_stage.kn` entry point or add to `codegen.kn`:
- `pub fn codegen_orchestrate(env: CodegenEnv, node: AstNode, idx: Int) -> CodegenResult`

**4.2** Emit the orchestrate body as a named callable:
- The orchestrate name becomes a function with its params and body block
- The body block contains stage declarations (which emit ABI calls) and the return expression
- Call `codegen_named_callable(env, name_idx, params_node, body_block)` — the body codegen handles stage ABI calls inline (Task 4.3)

**4.3** Codegen each stage call inside the body:
When the body codegen encounters a `stage <binding>: <runtime> <fn>(<args>)` statement, emit:

```llvm
; Step A: Stage begin — record all metadata
call void @abi_orchestrate_stage_begin_graph(
    i8* getelementptr("cpu"),              ; runtime_name string
    i8* getelementptr("mix"),              ; function_name string
    i8* getelementptr("capability(\"cpu.scalar\")"), ; selector string (empty if none)
    i8* getelementptr(""),                 ; dependency list (comma-sep or empty)
    i8* getelementptr("host"),             ; residency string
    i8* getelementptr("none"),             ; transfer string
    i8* getelementptr(""),                 ; guard axiom name (empty if none)
    i8* getelementptr(""),                 ; fallback string (empty if none)
    i8* getelementptr(""),                 ; requires law name (empty if none)
    i8* getelementptr("static")            ; policy string
)

; Step B: The actual stage function call
%result = call i64 @mix(i64 %arg)

; Step C: Stage end — pass-through result with telemetry
call void @abi_orchestrate_stage_end_i64(i64 %result)
```

- Create helper `codegen_orchestrate_stage_begin(env, stage_node) -> CodegenResult` that generates the LLVM IR for step A
- Create helper `codegen_orchestrate_stage_end(env) -> CodegenResult` that generates the LLVM IR for step C
- Wire these into the body statement codegen via a new expression kind or inline rewrite

**4.4** Map enum constants to string metadata:
- Runtime kind (0-11) → `"kain"`, `"c"`, `"cpu"`, `"gpu"`, etc.
- Residency (1-3) → `"host"`, `"shared"`, `"device"`
- Transfer (1-4) → `"none"`, `"host_to_device"`, `"device_to_host"`, `"shared_view"`
- Fallback kind (1-3) → `"abort"`, `"stage"`, `"degrade"`
- Policy (1-4) → `"static"`, `"telemetry_prefer_gpu"`, `"telemetry_prefer_cpu"`, `"telemetry_balance_latency"`

**4.5** Wire `codegen_orchestrate` into the main codegen dispatch loop.

**Acceptance:** The orchestrate name becomes a callable LLVM function. Running an orchestrate with 2 stages produces `orchestrate_stage_count()` returning 2. The LLVM IR contains `call void @abi_orchestrate_stage_begin_graph(...)` before each stage function call.

================================================================================
## Task 5: ABI Declarations and Runtime Wiring
================================================================================

**5.1** Ensure `src/llvm_ffi.kn` declares the required extern ABI functions:
```kn
pub fn abi_orchestrate_stage_begin_graph(
    runtime_name: ptr<Byte>, function_name: ptr<Byte>,
    selector_name: ptr<Byte>, dep_names: ptr<Byte>,
    residency_name: ptr<Byte>, transfer_name: ptr<Byte>,
    guard_name: ptr<Byte>, fallback_name: ptr<Byte>,
    requires_name: ptr<Byte>, policy_name: ptr<Byte>
) -> Void with Unsafe

pub fn abi_orchestrate_stage_end_i64(result: Int) -> Int with Unsafe
```

**5.2** Ensure `orchestrate_stage_count()`, `orchestrate_transfer_count()`, `orchestrate_fallback_count()`, `orchestrate_adaptive_stage_count()` are exposed as extern functions (they live in `runtime/native/src/core/stdlib_abi.c`).

**5.3** Add the runtime contract entry for orchestrate:
- When an orchestrate exists, emit `capability "orchestrate.pipeline"` in `runtime_contract.json`
- Each orchestrate emits `RuntimeOrchestrationContract { name, return_type, stages }`

**Acceptance:** The ABI calls are declared and linkable. Running the compiled orchestrate triggers the C runtime's telemetry counters.

================================================================================
## Task 6: Integration and Tests
================================================================================

**6.1** Create `src/L4_stage.kn` and wire into the compiler's file list in `build.kn`.

**6.2** Create a minimal smoke test:
```kn
use std::runtime
use std::intent

const OM: Int = 1000000007

fn mix_fn(v: Int) -> Int:
    return (v * 17 + 7) % OM

orchestrate test_pipe(v: Int) -> Int:
    stage base: cpu mix_fn(v) when target("llvm")
        residency host transfer none policy static
    if v < 0:
        return 0
    return base

pub fn test() -> Int:
    let before = orchestrate_stage_count()
    let r = test_pipe(42)
    let delta = orchestrate_stage_count() - before
    if delta < 1: return 1    # stage didn't fire
    if r != ((42 * 17 + 7) % OM): return 2
    return 0
```
Save in `smoketest/` and verify `kain check` + `kain run` both pass.

**6.3** Integration test with law guard:
```kn
law val_ok(v: Int) -> Bool:
    return v >= 0

orchestrate guarded_pipe(v: Int) -> Int:
    stage legal: law val_ok(v) when capability("law.invariants")
        residency host policy static
    stage work: cpu mix_fn(v) after legal requires legal
        residency host policy static
    if legal == false: return -1
    return work
```

**6.4** Error-case tests:
- Orchestrate with duplicate stage names → typecheck error
- Orchestrate with guard referencing a fn (not axiom) → typecheck error
- Orchestrate with cyclic deps → typecheck error
- Orchestrate with `residency host transfer host_to_device` → typecheck error

**6.5** Update the compiler's test suite to include `test_orchestrate()`, verifying: stage count > 0, law-based guards work, error cases report correctly.

**Acceptance:** All acceptance criteria from Tasks 1-5 are met. The compiler passes its own orchestrate smoketest. Error cases are caught at typecheck time.

================================================================================
