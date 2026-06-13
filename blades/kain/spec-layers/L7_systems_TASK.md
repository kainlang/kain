# L7 Systems — Actor + Ownership (TASKS)

**Target file:** `src/L7_systems.kn`
**Date:** 2026-06-12
**Wave:** After FOXTROT (needs TypedItem, ResolvedType, effects.kn)
**Parallel with:** GPU tasks (no cross-dependency)

---

## T1: Ownership State Machine Core (src/L7_systems.kn, ~120 lines)

**Files:** `src/L7_systems.kn`

Implement the ownership state machine enum, transition table, and region policy table.

**Acceptance criteria:**
- [ ] `OwnershipState` enum with 5 variants: `Idle`, `Observed(Int)`, `Collapsed`, `Shared`, `Decayed`
- [ ] `OwnershipTransition` enum with 8 variants: `BeginObserve`, `EndObserve`, `BeginCollapse`, `EndCollapse`, `BeginShare`, `EndShare`, `Decay`
- [ ] `ownership_apply(state: OwnershipState, transition: OwnershipTransition) -> Result<OwnershipState, Int>` returns new state or error code
- [ ] `OwnershipRegionKind` enum with 7 variants matching 0..6
- [ ] `RegionPolicy` struct with booleans for can_observe/collapse/share/decay + mode ints
- [ ] `OWNERSHIP_POLICY_TABLE: Array<RegionPolicy>` — 7 entries matching table in spec §3.2
- [ ] `ownership_region_policy(kind: OwnershipRegionKind) -> RegionPolicy` lookup
- [ ] File checks standalone: `kain check src/L7_systems.kn` passes

---

## T2: Actor Contract Core (src/L7_systems.kn, ~100 lines)

**Files:** `src/L7_systems.kn`

Implement the actor contract types for typechecking.

**Acceptance criteria:**
- [ ] `ActorStateSlot` struct: `name: String, type_name: String, has_default: Bool`
- [ ] `ActorHandlerParam` struct: `name: String, type_name: String`
- [ ] `ActorHandlerSignature` struct: `name: String, params: Array<ActorHandlerParam>, body_effects: Int`
- [ ] `ActorContract` struct: `name: String, state: Array<ActorStateSlot>, handlers: Array<ActorHandlerSignature>`
- [ ] `actor_contract_new(name: String) -> ActorContract`
- [ ] `actor_contract_add_state(contract, name, typ, has_default)`
- [ ] `actor_contract_add_handler(contract, name, params, effects)`
- [ ] `actor_contract_validate(contract) -> Result<(), String>` returning errors for: zero handlers, duplicate handler names, duplicate state names

---

## T3: Actor Check Functions (src/L7_systems.kn, ~200 lines)

**Files:** `src/L7_systems.kn`, `src/types.kn` (extend check_item)

Implement `check_actor()`, `check_spawn()`, `check_send()`, `check_ask()`.

**Acceptance criteria:**
- [ ] `check_actor(env, ast_node, program) -> Result<TypedItem, String>`:
  - Extracts state fields from flat AST and validates initializers
  - Extracts handler signatures and validates body effects
  - Registers `ActorContract` in `env.actor_contracts` map
  - Returns `TypedItem { kind: AST_ITEM_ACTOR, name, ... }`
- [ ] `check_spawn(env, ast_node, program) -> Result<ResolvedType, String>`:
  - Finds actor contract by name; errors if not found
  - Validates init fields match state slots (names + types)
  - Errors on missing required fields, unknown fields, type mismatches
  - Returns `Struct(actor_name)`
- [ ] `check_send(env, ast_node, program) -> Result<ResolvedType, String>`:
  - Verifies target is actor handle, message exists on contract
  - Validates field names and types against handler params
  - Returns `Unit`
- [ ] `check_ask(env, ast_node, program) -> Result<ResolvedType, String>`:
  - Verifies 3 args: actor handle, string literal, Int payload
  - Returns `Int`
- [ ] All error constants match spec edge case table (§5)
- [ ] Wire `check_item()` in `src/types.kn` to dispatch `AST_ITEM_ACTOR` to `check_actor()`

---

## T4: Ownership Check Functions (src/L7_systems.kn, ~180 lines)

**Files:** `src/L7_systems.kn`, `src/types.kn` (extend check_expr + check_stmt)

Implement `check_collapse()`, `check_observe()`, `check_decay()`, `check_share_fanout()`.

**Acceptance criteria:**
- [ ] `check_collapse(env, ast_node, program) -> Result<ResolvedType, String>`:
  - Validates pointer target type, not in share scope, structured exit
  - Region policy check for Collapsed support
  - Returns body result type, emits EFF_UNSAFE
- [ ] `check_observe(env, ast_node, program) -> Result<ResolvedType, String>`:
  - Same pointer validation as collapse
  - Supports nesting (observer count tracking)
  - Returns body result type, emits EFF_UNSAFE
- [ ] `check_decay(env, ast_node, program) -> Result<ResolvedType, String>`:
  - Validates pointer target, not in share scope
  - Marks pointer as decayed in env.decayed_pointers
  - Returns Unit, emits EFF_UNSAFE
- [ ] `check_share(env, ast_node, program) -> Result<ResolvedType, String>`:
  - Increments shared_region_depth, validates pointer
  - Walks body for fanout statements
  - Decrements depth on exit
- [ ] `check_fanout(env, ast_node, program) -> Result<ResolvedType, String>`:
  - Verifies inside share scope (env.shared_region_depth > 0)
  - Validates range expression, structured exit, no nested fanout
  - Returns Unit
- [ ] `check_expr()` in `src/types.kn` dispatches AST_EXPR_COLLAPSE/OBSERVE to these
- [ ] `check_stmt()` dispatches AST_EXPR_DECAY and AST_STMT_FANOUT
- [ ] Use-after-decay: any Ident in `env.decayed_pointers` used as pointer target → ERR_USE_AFTER_DECAY

---

## T5: Actor Codegen (src/codegen.kn, ~150 lines)

**Files:** `src/codegen.kn` (extend compile_item/compile_expr)

Implement `compile_spawn()`, `compile_send_msg()`, `compile_ask()`.

**Acceptance criteria:**
- [ ] `compile_spawn(gen, node, program) -> GenResult` emits:
  - Alloca for `%KainActorSpawnConfig`
  - Field stores for init values
  - `call i64 @kain_actor_spawn(...)`
- [ ] `compile_send_msg(gen, node, program) -> GenVoidResult` emits:
  - Message struct allocation + field fill
  - `call i32 @kain_actor_send(i64, %KainActorMessage*, i8*)`
- [ ] `compile_ask(gen, node, program) -> GenResult` emits:
  - `call i8* @kain_actor_reply_port_new()`
  - `call i32 @kain_actor_ask_send_ref(i64, i8*, i64, i8*)`
  - `call i32 @kain_actor_reply_port_wait(i8*, i64, i8*, i64, i64*)`
  - Load i64 result from output pointer
- [ ] Each emits the corresponding `declare` in the LLVM preamble
- [ ] Wire `compile_expr()` to dispatch AST_EXPR_SPAWN/SEND, and `compile_call()` to handle "ask"

---

## T6: Ownership Codegen (src/codegen.kn, ~150 lines)

**Files:** `src/codegen.kn` (extend compile_expr/compile_stmt)

Implement `compile_collapse_expr()`, `compile_observe_expr()`, `compile_decay_stmt()`, `compile_share_expr()`, `compile_fanout_stmt()`.

**Acceptance criteria:**
- [ ] `compile_collapse_expr(gen, node, program) -> GenResult` emits:
  - `bitcast ptr to i8*`
  - `call i32 @__kain_ownership_begin_collapse(i8*)` with abort-on-failure branch
  - Compiled body + `call i32 @__kain_ownership_end_collapse(i8*)` with abort branch
  - Ephemeral local shortcut: skip runtime calls entirely when provenance is ephemeral
- [ ] `compile_observe_expr(gen, node, program) -> GenResult`: same pattern with begin/end_observe
- [ ] `compile_decay_stmt(gen, node, program) -> GenVoidResult`: single `call i32 @__kain_ownership_decay(i8*)`
- [ ] `compile_share_expr(gen, node, program) -> GenResult`: same pattern as collapse with begin/end_share
- [ ] `compile_fanout_stmt(gen, node, program) -> GenVoidResult`:
  - Emits `call i32 @__kain_fanout_i64(start, end, ptr, worker_fn_ptr)`
- [ ] All `declare` for ownership functions emitted in LLVM preamble section
- [ ] Wire `compile_expr()` for AST_EXPR_COLLAPSE/OBSERVE/SHARE and `compile_stmt()` for AST_STMT_FANOUT

---

## T7: Edge Case Tests (smoketest/, ~80 lines)

**Files:** `smoketest/src/semantics/L7_actor.kn`, `smoketest/src/semantics/L7_ownership.kn`

Acceptance tests for the actor and ownership typechecker + codegen.

**Acceptance criteria:**
- [ ] `L7_actor.kn` passes `kain check`:
  - `test_actor_declaration`: actor with 2 state fields + 1 on handler + reply_to
  - `test_spawn_init`: spawn with correct field values
  - `test_send_msg`: send to spawned actor, verify no type errors
  - `test_ask_reply`: ask with string + Int, verify return type
- [ ] `L7_ownership.kn` passes `kain check`:
  - `test_collapse_observe_decay_chain`: alloc → collapse → observe → decay
  - `test_nested_observe`: 2-level nested observe
  - `test_share_fanout`: shared region + fanout workers
  - `test_use_after_decay_error`: decay then use → parse error
  - `test_collapse_inside_share_error`: share { collapse ptr: ... } → parse error
- [ ] Each test uses compiletest directives (`//@ check: E02XX`) for error cases
