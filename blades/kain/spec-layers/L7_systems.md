# L7 Systems — Actor + Ownership (SPEC)

**Target file:** `src/L7_systems.kn`
**Date:** 2026-06-12
**Budget spec:** Integration guide for the self-host compiler's Layer 7 typechecker + codegen

---

## 1. Architecture Overview

L7 Systems provides two independent but composable subsystems:

- **Actor model** — message-passing concurrency with private state, typed `on` handlers, `spawn`/`send`/`ask`, reply ports.
- **Ownership model** — explicit raw pointer lifecycle via `collapse`/`observe`/`decay` with a 5-state compiler-owned machine, region-aware policies, and deterministic teardown.

Both live in `src/L7_systems.kn` because they share the L7 label, share the `_self: Self_` method convention, and compose (actors use ownership inside handlers).

```
src/L7_systems.kn
  ├── Actor definitions: TypedActorState, TypedActorHandler, TypedActorContract
  ├── check_actor()        — typecheck actor declaration
  ├── check_spawn()        — typecheck spawn expression
  ├── check_send()         — typecheck send expression
  ├── check_ask()          — typecheck ask() call
  ├── Ownership model: OwnershipState enum, transition table
  ├── check_collapse()     — typecheck collapse expression
  ├── check_observe()      — typecheck observe expression
  ├── check_decay()        — typecheck decay statement
  ├── check_share_fanout() — typecheck share + fanout
  ├── codegen_actor_spawn_table()  — emit spawn metadata
  ├── codegen_ownership_calls()    — emit __kain_ownership_* calls
  └── codegen_fanout()     — emit __kain_fanout_i64
```

---

## 2. Actor Typechecking

### 2.1 AST Representation

All L7 expressions use the flat `Array<AstNode>` format from `ast.kn`:

```
AST_ITEM_ACTOR = 23
  ast_data[0] = name_idx (string pool)
  ast_data[1] = state_count (N)
  ast_data[2..2+3N] = state entries: [name_idx, type_idx, init_expr_idx]
  ast_data[2+3N] = handler_count (M)
  ast_data[2+3N+1..] = handler entries:
    [name_idx, param_count, param_name0, param_type0, ..., body_expr_idx]
```

```
AST_EXPR_SPAWN = 127
  ast_data[0] = actor_type_name_idx
  ast_data[1] = init_field_count (P)
  ast_data[2..2+2P] = init field pairs: [field_name_idx, value_expr_idx]
```

```
AST_EXPR_SEND = 128
  ast_data[0] = target_expr_idx
  ast_data[1] = msg_name_idx
  ast_data[2] = field_count (Q)
  ast_data[3..3+2Q] = field pairs: [field_name_idx, value_expr_idx]
```

### 2.2 `check_actor()` — Validate Actor Declaration

```
Input:  AstNode with kind == AST_ITEM_ACTOR
Output: TypedItem with actor_contract attached to env.actor_contracts[name]

Algorithm:
  1. Extract name_idx from ast_data[0]. Look up String from string pool.
  2. Extract state_count N from ast_data[1].
  3. For each state (i = 0..N-1):
     a. name_idx = ast_data[2 + 3*i]; type_idx = ast_data[2 + 3*i + 1]; init_expr_idx = ast_data[2 + 3*i + 2]
     b. Typecheck init_expr: actual_type = check_expr(env, init_expr_idx)
     c. If init_expr is not AST_EXPR_NONE: verify actual_type matches type_idx
     d. Register state field in actor contract
  4. Compute handler_base = 2 + 3*N. Extract handler_count M from ast_data[handler_base].
  5. Walk handlers sequentially starting at handler_base+1:
     a. Read [name_idx, param_count, param_name0, param_type0, ..., body_expr_idx]
     b. Verify param_count >= 1 (first param is always reply_to: P)
     c. Typecheck body_expr, verify return is Unit (send) or Int (reply)
     d. Register handler in actor contract
  6. Validate contract invariants:
     a. Duplicate message names → ERR_ACTOR_DUPLICATE_HANDLER
     b. Empty handlers + empty methods → ERR_ACTOR_NO_HANDLERS
     c. State field without initializer and no init in spawn → ERR_ACTOR_MISSING_INIT
  7. Register actor contract: env.actor_contracts[name] = contract
```

### 2.3 `check_spawn()` — Validate Spawn Expression

```
Input:  AstNode with kind == AST_EXPR_SPAWN
Output: ResolvedType (actor handle type — Struct with actor name)

Algorithm:
  1. Look up actor_type_name from ast_data[0] string pool.
  2. Find contract: env.actor_contracts[actor_type_name] — ERR_ACTOR_NOT_FOUND if missing.
  3. Extract init_field_count P from ast_data[1].
  4. For each field pair (i = 0..P-1):
     a. field_name = ast_data[2 + 2*i]; value_expr = ast_data[2 + 2*i + 1]
     b. Verify field_name exists in contract.state (by name). ERR_ACTOR_UNKNOWN_FIELD.
     c. Infer value type: check_expr(env, value_expr)
     d. Verify type matches contract.state[field_name].type. ERR_ACTOR_TYPE_MISMATCH.
  5. Verify all required state fields (those without defaults) appear in init. ERR_ACTOR_MISSING_FIELD.
  6. Return ResolvedType::Struct(actor_name, []).
```

### 2.4 `check_send()` — Validate Send Expression

```
Input:  AstNode with kind == AST_EXPR_SEND
Output: ResolvedType::Unit

Algorithm:
  1. Infer target type from ast_data[0]: check_expr(env, target_expr)
  2. Verify target type is an actor handle (Struct name registered as actor). ERR_ACTOR_TARGET.
  3. Look up msg_name from ast_data[1] string pool.
  4. Find handler in contract.handlers matching msg_name. ERR_ACTOR_UNKNOWN_MSG.
  5. Extract field_count Q from ast_data[2].
  6. For each field pair (i = 0..Q-1):
     a. field_name = ast_data[3 + 2*i]; value_expr = ast_data[3 + 2*i + 1]
     b. Verify field_name exists in handler params. ERR_ACTOR_UNKNOWN_PARAM.
     c. Typecheck value_expr, verify type matches handler param type.
  7. Extract reply_to from target expression if present.
  8. Return Unit.
```

### 2.5 `check_ask()` — Validate Ask Call

```
Input:  Expr::Call where callee name is "ask"
Output: ResolvedType::Int (single-value reply — temporary constraint)

Algorithm:
  1. Verify arg_count == 3.
  2. First arg: infer type, verify it's an actor handle.
  3. Second arg: must be a string literal (AST_EXPR_STRING). Extract message name.
  4. Third arg: must be Int-compatible.
  5. Look up actor contract + handler by name. Verify handler has reply_to: P.
  6. Return Int (single-value packed reply).
```

---

## 3. Ownership Typechecking

### 3.1 State Machine

The ownership state machine has 5 states with 8 transitions:

```
Idle ───BeginCollapse──→ Collapsed ───EndCollapse──→ Idle
Idle ───BeginObserve───→ Observed(N) ───EndObserve──→ Idle (N=1 after decrement)
                        Observed(N) ───BeginObserve──→ Observed(N+1) (nesting)
Idle ───BeginShare────→ Shared ───────EndShare─────→ Idle
Idle ───Decay─────────→ Decayed (terminal)
```

Transition error constants (from bootstrap `crates/ownership/src/ownership_state.rs`):

| Error code | Value | Meaning |
|-----------|-------|---------|
| ERR_CANNOT_OBSERVE_COLLAPSED | -1 | observe on collapsed pointer |
| ERR_CANNOT_OBSERVE_DECAYED | -2 | observe on decayed pointer |
| ERR_CANNOT_COLLAPSE_OBSERVED | -3 | collapse while observers exist |
| ERR_CANNOT_COLLAPSE_DECAYED | -4 | collapse on decayed pointer |
| ERR_CANNOT_DECAY_OBSERVED | -5 | decay while observers exist |
| ERR_CANNOT_DECAY_COLLAPSED | -6 | decay while collapsed |
| ERR_CANNOT_DECAY_DECAYED | -7 | double decay |

### 3.2 Self-Host Representation

```kain
pub enum OwnershipState:
    Idle
    Observed(count: Int)
    Collapsed
    Shared
    Decayed

pub enum OwnershipRegionKind:
    LocalAlloca       // 0 — stack alloca
    HeapAllocation    // 1 — heap alloc/alloc_zeroed
    RcObject          // 2 — RC_OBJECT
    WorldState        // 3 — world field
    EntangledAuthority // 4 — entangle authority
    EntangledMirror   // 5 — entangle mirror
    ImportedPointer   // 6 — C FFI/external

// Region policy: which operations are allowed
pub struct RegionPolicy:
    can_observe: Bool
    can_collapse: Bool
    can_share: Bool
    can_decay: Bool
    observe_mode: Int  // 0=ReadonlyBorrow, 1=Snapshot
    decay_mode: Int     // 0=LifetimeEnd, 1=FreeHeap, 2=ReleaseStrong
```

Policy table (7 regions, each with supported operations):

| Region | observe | collapse | share | decay |
|--------|---------|----------|-------|-------|
| LocalAlloca | ReadonlyBorrow | ScopedNoAlias | no | LifetimeEnd |
| HeapAllocation | ReadonlyBorrow | ScopedNoAlias | AtomicSeqCst | FreeHeap |
| RcObject | ReadonlyBorrow | ExclusiveToken | no | ReleaseStrong |
| WorldState | Snapshot | GraphExclusive | no | no |
| EntangledAuthority | Snapshot | GraphExclusive | no | no |
| EntangledMirror | Snapshot | no | no | no |
| ImportedPointer | ReadonlyBorrow | ScopedNoAlias | AtomicSeqCst | LifetimeEnd |

### 3.3 `check_collapse()` / `check_observe()` — Scoped Ownership

```
Input:  AstNode with kind AST_EXPR_COLLAPSE (130) or AST_EXPR_OBSERVE (131)
Output: Body result type (the trailing expression of the block)

Algorithm:
  1. Extract target_expr_idx from ast_data[0], body_expr_idx from ast_data[1].
  2. Infer target type: target_ty = check_expr(env, target_expr)
  3. Verify target_ty is Ptr{T} or Ref{T}. ERR_OWNERSHIP_NOT_POINTER.
  4. Verify not inside share scope: env.shared_region_depth == 0. ERR_OWNERSHIP_IN_SHARE.
  5. Verify structured exit: walk body AST, reject if return/break/continue found.
  6. Verify region policy supports this operation (via region_kind lookup).
     For collapse on HeapAllocation: ok. For collapse on EntangledMirror: ERR_UNSUPPORTED.
  7. Infer body type: body_ty = check_expr(env, body_expr)
  8. Track state: env.ownership_states[target_ptr_id] -> new state.
  9. Emit effects: EFF_UNSAFE (for the raw pointer access).
  10. Return body_ty as the expression result type.
```

### 3.4 `check_decay()` — Decay Statement

```
Input:  AstNode with kind AST_EXPR_DECAY (132)
Output: ResolvedType::Unit

Algorithm:
  1. Extract target_expr_idx from ast_data[0].
  2. Infer target type, verify Ptr{T} or Ref{T}.
  3. Verify not inside share scope.
  4. Verify region supports decay (e.g., not WorldState which rejects decay).
  5. Mark pointer as decayed: env.ownership_states[target_ptr_id] = Decayed.
     Also: env.decayed_pointers.insert(target_var_name) — for use-after-decay checks.
  6. Emit effects: EFF_UNSAFE.
  7. Return Unit.
```

### 3.5 `check_share_fanout()` — Share + Fanout

```
Input:  AstNode with kind AST_EXPR_SHARE (133) — contains fanout stmts inside body
Output: Body result type

Algorithm (check_share):
  1. Extract target_expr_idx, body_expr_idx. Same pointer type check as collapse.
  2. Increment env.shared_region_depth.
  3. Enter share context: env.in_share_scope = true.
  4. Check body: walk body AST for fanout statements (AST_STMT_FANOUT = 54).
     For each fanout:
     a. Extract [name_idx, range_expr_idx, body_expr_idx_from_fanout]
     b. Verify range_expr is integer range.
     c. Verify fanout body has no return/break/continue.
     d. Verify no nested fanout.
     e. Verify collapse/observe/decay NOT used inside fanout body.
  5. Decrement env.shared_region_depth. Exit share context.
  6. Emit effects: EFF_UNSAFE.
  7. Return body result type.
```

### 3.6 Use-After-Decay Prevention

After any `decay ptr_var`, all subsequent references to `ptr_var` as a pointer target
must produce error `ERR_USE_AFTER_DECAY`. Implementation:

```kain
// In check_expr(), when encountering an Ident:
if ident in env.decayed_pointers:
    // Check if this ident is used as a pointer target
    // (e.g., collapse ptr_var:, observe ptr_var:, decay ptr_var, ptr_offset(ptr_var...))
    if context is PTR_TARGET:
        return type_error("cannot use decayed pointer", span)
```

---

## 4. Codegen Integration

### 4.1 Actor Spawn Table

For each actor item, the codegen emits a spawn configuration structure and the
`kain_actor_spawn()` call:

```llvm
; Actor spawn table entry (one per actor type)
%actor_spawn_config = alloca %KainActorSpawnConfig, align 8
; Set config fields...
%actor_id = call i64 @kain_actor_spawn(%actor_spawn_config, i8* @".actor.name")
```

### 4.2 Actor Send

```llvm
; Preparing a message for send
%msg = alloca %KainActorMessage, align 8
; Fill msg.name, msg.payload, msg.payload_size
%status = call i32 @kain_actor_send(i64 %target_id, %KainActorMessage* %msg, i8* @".msg.name")
```

### 4.3 Ownership Runtime Calls

```kain
// Self-host codegen emits these LLVM declare + call patterns:

// collapse begin: __kain_ownership_begin_collapse(ptr_as_i8*) -> i32
// collapse end:   __kain_ownership_end_collapse(ptr_as_i8*) -> i32
// observe begin:  __kain_ownership_begin_observe(ptr_as_i8*) -> i32
// observe end:    __kain_ownership_end_observe(ptr_as_i8*) -> i32
// decay:          __kain_ownership_decay(ptr_as_i8*) -> i32
// fanout:         __kain_fanout_i64(start, end, ptr, worker_fn_ptr) -> i32
```

LLVM output pattern for collapse:

```llvm
%cast = bitcast T* %ptr to i8*
%begin_ok = call i32 @__kain_ownership_begin_collapse(i8* %cast)
%begin_check = icmp eq i32 %begin_ok, 0
br i1 %begin_check, label %body, label %abort

abort:
  call void @abort()
  unreachable

body:
  ; ... compiled body ...
  %end_ok = call i32 @__kain_ownership_end_collapse(i8* %cast)
  %end_check = icmp eq i32 %end_ok, 0
  br i1 %end_check, label %done, label %abort2

abort2:
  call void @abort()
  unreachable

done:
```

### 4.4 Reply Port Codegen

```llvm
; ask call lowers to:
%port = call i8* @kain_actor_reply_port_new()
%payload = ... ; compile payload expr
%send_ok = call i32 @kain_actor_ask_send_ref(i64 %target_id, i8* @".msg.name", i64 %payload, i8* %port)
%reply_val = alloca i64, align 8
%wait_ok = call i32 @kain_actor_reply_port_wait(i8* %port, i64 30000, i8* null, i64 0, i64* %reply_val)
%result = load i64, i64* %reply_val
```

---

## 5. Edge Cases

| # | Scenario | Handling |
|---|----------|----------|
| 1 | Actor mailbox full (bounded mailbox) | `kain_actor_send` returns error; codegen must abort or propagate |
| 2 | Spawn with missing required state field | Typechecker: `ERR_ACTOR_MISSING_FIELD` |
| 3 | Spawn with unknown state field | Typechecker: `ERR_ACTOR_UNKNOWN_FIELD` |
| 4 | Actor with zero handlers | Typechecker: `ERR_ACTOR_NO_HANDLERS` |
| 5 | send to non-actor target | `ERR_ACTOR_TARGET` |
| 6 | send with unknown message name | `ERR_ACTOR_UNKNOWN_MSG` |
| 7 | Send with wrong field name | `ERR_ACTOR_UNKNOWN_PARAM` |
| 8 | Send with wrong field type | `ERR_ACTOR_TYPE_MISMATCH` |
| 9 | decay on a non-pointer | `ERR_OWNERSHIP_NOT_POINTER` |
| 10 | collapse on decayed pointer | `ERR_OWNERSHIP_DECAYED` (runtime: abort) |
| 11 | decay on world state | `ERR_UNSUPPORTED` (world state cannot be decayed) |
| 12 | collapse inside share scope | `ERR_OWNERSHIP_IN_SHARE` (share uses atomic_store, not collapse) |
| 13 | observe inside share scope | `ERR_OWNERSHIP_IN_SHARE` |
| 14 | Use pointer variable after decay | `ERR_USE_AFTER_DECAY` |
| 15 | return/break/continue inside collapse body | `ERR_OWNERSHIP_STRUCTURED_EXIT` |
| 16 | fanout inside fanout | `ERR_FANOUT_NESTED` |
| 17 | fanout outside share scope | `ERR_FANOUT_NO_SHARE` |
| 18 | Duplicate state field names in actor | `ERR_ACTOR_DUPLICATE_STATE` |
| 19 | Duplicate message handler names in actor | `ERR_ACTOR_DUPLICATE_HANDLER` |
| 20 | ask on actor without reply_to: P handler | `ERR_ACTOR_NO_REPLY_PORT` |

---

## 6. Codegen Stubs Already Present

| File | Lines | Status |
|------|-------|--------|
| `src/codegen.kn` | 72 (ActorInstruction enum) | Stub — needs emission logic |
| `src/types.kn` | AST_EXPR_SPAWN=127, AST_EXPR_SEND=128 | Constants defined |
| `src/types.kn` | AST_EXPR_COLLAPSE=130..133, AST_STMT_FANOUT=54 | Constants defined |

**New codegen functions needed in `src/codegen.kn`:**

```kain
// Actor codegen:
pub fn compile_spawn(gen: LlvmGenerator, node: AstNode, program: MonomorphizedProgram) -> GenResult
pub fn compile_send_msg(gen: LlvmGenerator, node: AstNode, program: MonomorphizedProgram) -> GenVoidResult
pub fn compile_ask(gen: LlvmGenerator, node: AstNode, program: MonomorphizedProgram) -> GenResult

// Ownership codegen:
pub fn compile_collapse_expr(gen: LlvmGenerator, node: AstNode, program: MonomorphizedProgram) -> GenResult
pub fn compile_observe_expr(gen: LlvmGenerator, node: AstNode, program: MonomorphizedProgram) -> GenResult
pub fn compile_decay_stmt(gen: LlvmGenerator, node: AstNode, program: MonomorphizedProgram) -> GenVoidResult
pub fn compile_share_expr(gen: LlvmGenerator, node: AstNode, program: MonomorphizedProgram) -> GenResult
pub fn compile_fanout_stmt(gen: LlvmGenerator, node: AstNode, program: MonomorphizedProgram) -> GenVoidResult
```
