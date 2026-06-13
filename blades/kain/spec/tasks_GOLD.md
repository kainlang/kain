# Stream GOLD: L1-L7 Stub→Real (DEFERRED — Wave 3)

**Stream ID:** GOLD
**Role:** Replace all L1-L7 stub functions in types.kn and codegen.kn with real implementations: world, entangle, patch, law, converge, orchestrate, pulse, resonate, axiom, shatter, teleport, actor, component, shader.
**Effort:** 4-6 weeks
**Depends On:** Stream RED (real L0 typechecker) + Stream BLUE (real L0 codegen)
**Requirements Covered:** FR-l1-l7-tc, FR-l1-l7-cg
**Design Reference:** types.kn (L1-L7 stubs at lines 1401-1456), codegen.kn (needs L1-L7 sections added)

---

## Context

This is the FINAL phase — the semantic stack. After RED makes the typechecker fully real for Layer 0 and BLUE makes codegen fully real for Layer 0, GOLD extends both upward through Layers 1-7 of the decision ladder:

```
LAYER 7: SYSTEMS     actor · collapse/observe/decay
LAYER 6: MACHINE     axiom · shatter · teleport
  STONES
LAYER 5: TEMPORAL    pulse · resonate
LAYER 4: STAGE       orchestrate
  GRAPH
LAYER 3: DISPATCH    converge
LAYER 2: STATE       patch · law
  INTEGRITY
LAYER 1: STATE       world · entangle
  AUTHORITY
LAYER 0: PLAIN       fn · struct · let · enum · trait · impl  ← RED + BLUE
  CODE
```

GOLD is fully deferred — it doesn't block ouroboros because the compiler's own source uses very few L1-L7 constructs. The self-host compiler is written almost entirely in Layer 0 (fn, struct, let, if, while, for). GOLD makes the compiler able to compile other Kain programs that use the semantic stack.

**This stream should NOT be started until RED and BLUE are complete and ouroboros Phase 2 passes.**

---

## Files You Own

### Files to Modify

| File | Region/Function | Change Description |
|------|-----------------|--------------------|
| `X:/blades/kain/src/types.kn` | `check_patch_law_stub` (line 1403) | Make real: patch journal tracking, law predicate validation |
| `X:/blades/kain/src/types.kn` | `check_converge_stub` (line 1411) | Make real: spec/fast lane verification, capability selection |
| `X:/blades/kain/src/types.kn` | `check_orchestrate_stub` (line 1418) | Make real: stage dependency validation, residency/transfer checking |
| `X:/blades/kain/src/types.kn` | `check_world_stub` (line 1440) | Make real: world state field tracking, surface validation, entangle propagation |
| `X:/blades/kain/src/types.kn` | `check_shader_stub` (line 1454) | Make real: uniform binding validation, workgroup size, compute metadata |
| `X:/blades/kain/src/types.kn` | NEW: `check_actor_item` | Actor message contract validation, state slot typing, handler signatures |
| `X:/blades/kain/src/types.kn` | NEW: `check_component_item` | Component prop types, state types, JSX validation |
| `X:/blades/kain/src/types.kn` | NEW: `check_entangle_item` | Entangle field pair validation, single_writer policy |
| `X:/blades/kain/src/types.kn` | NEW: `check_axiom_item` | Axiom capability predicate validation |
| `X:/blades/kain/src/types.kn` | NEW: `check_shatter_item` | Shatter struct SoA layout validation |
| `X:/blades/kain/src/types.kn` | `infer_expr_type` | Add inference for L1-L7 expression kinds: spawn, send, teleport, collapse, observe, decay, share, fanout, dispatch, asm, atomics |
| `X:/blades/kain/src/codegen.kn` | NEW: `emit_world_globals` | World state global variable emission |
| `X:/blades/kain/src/codegen.kn` | NEW: `emit_entangle_sync` | Entangle propagation codegen |
| `X:/blades/kain/src/codegen.kn` | NEW: `emit_actor_dispatch` | Actor message dispatch table emission |
| `X:/blades/kain/src/codegen.kn` | NEW: `compile_spawn_textual` | `spawn Actor(...)` codegen |
| `X:/blades/kain/src/codegen.kn` | NEW: `compile_send_textual` | `send actor.Msg(...)` codegen |
| `X:/blades/kain/src/codegen.kn` | NEW: `compile_patch_textual` | Patch function codegen with journal calls |
| `X:/blades/kain/src/codegen.kn` | NEW: `compile_converge_textual` | Converge lane dispatch codegen |
| `X:/blades/kain/src/codegen.kn` | NEW: `compile_orchestrate_textual` | Orchestrate stage graph codegen |
| `X:/blades/kain/src/codegen.kn` | NEW: `compile_pulse_textual` | Pulse timer registration codegen |
| `X:/blades/kain/src/codegen.kn` | NEW: `compile_teleport_textual` | Teleport cross-world handoff codegen |
| `X:/blades/kain/src/codegen.kn` | NEW: `compile_collapse_textual` | Collapse scope codegen (ownership) |
| `X:/blades/kain/src/codegen.kn` | NEW: `emit_gpu_kernel` | GPU shader emission (SPIR-V/PTX/HLSL) |
| `X:/blades/kain/src/codegen.kn` | NEW: `compile_component_textual` | Component JSX lowering |

### Files You Must NOT Touch

| File | Reason |
|------|--------|
| `X:/blades/kain/src/parser.kn` | Parser is done — all L1-L7 parse rules already exist |
| `X:/blades/kain/src/lexer.kn` | Lexer is done |
| `X:/blades/kain/src/orchestrator.kn` | Orchestrator is done (GREEN) |
| `X:/blades/kain/src/compiler.kn` | Compiler is done (GREEN) |

---

## Implementation Tasks

### GOLD-TC-1: World + Entangle Typechecking

**Effort:** 2 days
**Objective:** Make `check_world_stub` real for world state field validation and entangle propagation.

**World AST layout:**
```
data[0] = name_idx
data[1] = surface_count; then surface entries (platform, component_expr)
data[N] = state_count; then state entries (field_name, field_type, initial_value)
```

**Implementation Steps:**

1. Parse world AST layout. Register world type in env.
2. For each state field: resolve type, check initial value compatibility.
3. For each surface: validate component reference exists, check platform compatibility.
4. For entangle items: validate both fields exist on their respective worlds, enforce single_writer policy.
5. Register world fields in env so `world.field` access resolves correctly.

---

### GOLD-TC-2: Patch + Law Typechecking

**Effort:** 1 day
**Objective:** Make `check_patch_law_stub` real.

**Implementation Steps:**

1. Patch items: check that the world parameter is a valid world type, check body mutates only that world's state fields, track patch journal calls.
2. Law items: check that the predicate returns Bool, check parameter types, validate invariant surface.

---

### GOLD-TC-3: Converge Typechecking

**Effort:** 2 days
**Objective:** Make `check_converge_stub` real for spec/fast lane verification.

**Implementation Steps:**

1. Spec lane: check as normal function body with expected return type.
2. Fast lanes: check body against spec's return type. Validate `when target()` and `when capability()` conditions.
3. Verify clause: validate `random(N)` — N must be positive integer literal.

---

### GOLD-TC-4: Orchestrate Typechecking

**Effort:** 2 days
**Objective:** Make `check_orchestrate_stub` real for stage graph validation.

**Implementation Steps:**

1. Each stage: validate runtime type (cpu/gpu/kain/converge/law/patch/world/dispatch/etc.).
2. Check `deps` and `after` references — all named stages must exist.
3. Validate `residency` (host/device/shared) and `transfer` (none/host_to_device/device_to_host/shared_view) compatibility.
4. Check `guarded by` axiom references — axiom must exist.
5. Check `requires` law references — law must exist.
6. Validate pipeline return type — must match declared return type.

---

### GOLD-TC-5: Actor Typechecking

**Effort:** 2 days
**Objective:** Implement full actor typechecking.

**Implementation Steps:**

1. Actor state fields: resolve types, check initial values.
2. Message handlers (`on Msg(params):`): check parameter types, check body with access to `self`.
3. Validate reply port type `P` — must match message handler's reply_to parameter.
4. Check spawn expressions: `spawn Actor(args)` — verify all state fields are provided, types match.
5. Check send expressions: `send handle.Msg(args)` — verify Msg exists on actor type, args match handler params.

---

### GOLD-TC-6: Shatter + Teleport + Axiom + Pulse + Resonate Typechecking

**Effort:** 2 days
**Objective:** Complete remaining L1-L7 typecheck stubs.

**Implementation Steps:**

1. Shatter struct: validate SoA layout, register as shatter type.
2. Teleport: validate source and target worlds exist, check channel name.
3. Axiom: validate capability predicates, register guarantee.
4. Pulse: validate interval units (ns/us/ms/s/tick), check body effects.
5. Resonate: validate trigger field is a valid world state field, check dampen interval, enforce anti-self-feedback.

---

### GOLD-TC-7: Component + Shader Typechecking

**Effort:** 2 days
**Objective:** Complete UI component and GPU shader typechecking.

**Implementation Steps:**

1. Component: validate prop types, state types, check render body JSX.
2. Shader vertex: validate input attributes, uniform bindings, return type.
3. Shader fragment: validate interpolated inputs, uniform bindings, return type.
4. Shader compute: validate workgroup size, storage buffer bindings, comptime metadata.

---

### GOLD-CG-1: World + Entangle Codegen

**Effort:** 2 days
**Objective:** Emit world state global variables and entangle propagation code.

**Implementation Steps:**

1. For each world: emit a global variable struct containing all state fields.
2. Emit world init function: initialize state fields to declared defaults.
3. For entangle: emit propagation function that copies field from authority to mirror.

---

### GOLD-CG-2: Actor Codegen

**Effort:** 3 days
**Objective:** Emit actor message dispatch tables and spawn/send codegen.

**Implementation Steps:**

1. For each actor: emit state struct, message handler function table.
2. `spawn Actor(...)`: allocate actor state, register with scheduler, return handle.
3. `send actor.Msg(...)`: enqueue message in actor's mailbox.
4. `ask(actor, "Msg", payload)`: send + await reply pattern.

---

### GOLD-CG-3: Ownership Codegen (Collapse/Observe/Decay)

**Effort:** 2 days
**Objective:** Lower ownership expressions to allocator calls.

**Implementation Steps:**

1. `collapse ptr:`: enter exclusive mutation scope, emit nothing (runtime tracks via state machine).
2. `observe ptr:`: enter read-only scope.
3. `decay ptr`: call `__kain_free` on the pointer, mark dead.
4. `share ptr: fanout ...`: allocate workspace per fanout lane, emit parallel region.

---

### GOLD-CG-4: Converge + Orchestrate Codegen

**Effort:** 3 days
**Objective:** Emit converge lane dispatch and orchestrate stage graph.

**Implementation Steps:**

1. Converge: emit spec function, fast lane functions, runtime lane selection at startup.
2. Orchestrate: emit stage functions, dependency tracking, residency transfers, fallback dispatch.

---

### GOLD-CG-5: Pulse + Resonate + Teleport Codegen

**Effort:** 2 days
**Objective:** Emit temporal and machine-stone runtime calls.

**Implementation Steps:**

1. Pulse: register timer with runtime, emit timer callback function.
2. Resonate: register state-change watcher, emit trigger handler.
3. Teleport: emit cross-world memory handoff (`__kain_machine_teleport_ptr`).

---

### GOLD-CG-6: GPU Codegen

**Effort:** 4 days
**Objective:** Emit SPIR-V / PTX / HLSL / WGSL for shader items.

**Implementation Steps:**

1. Shader vertex: emit vertex shader in target shading language.
2. Shader fragment: emit fragment shader.
3. Shader compute: emit compute kernel with workgroup size.
4. `dispatch "shader::Kernel::compute" [x, y, z]`: emit GPU dispatch call.
5. Storage buffer bindings: emit descriptor set layouts.

---

### GOLD-CG-7: Component + JSX Codegen

**Effort:** 2 days
**Objective:** Lower component render trees to UI runtime calls.

**Implementation Steps:**

1. Component props → struct type.
2. Component state → local state slot.
3. JSX `<element attr={expr}>` → `ui_element_create("element", attrs)`.
4. JSX children → `ui_element_append_child(parent, child)`.
5. `for item in list: <...>` → loop with element creation.
6. `if cond: <...>` → conditional element creation.

---

### GOLD-INT-1: Wire All L1-L7 Codegen Into Pipeline

**Effort:** 1 day
**Objective:** Wire all new codegen into compile_expr_textual and codegen_textual.

**Implementation Steps:**

1. In `compile_expr_textual`, dispatch for: AST_EXPR_SPAWN, AST_EXPR_SEND, AST_EXPR_TELEPORT, AST_EXPR_COLLAPSE, AST_EXPR_OBSERVE, AST_EXPR_DECAY, AST_EXPR_SHARE, AST_EXPR_FANOUT, AST_EXPR_DISPATCH.
2. In `codegen_textual`, emit: world globals, entangle sync, actor dispatch tables, pulse registrations.
3. Wire GPU emission as a separate codegen path (`codegen_gpu`).

---

## Stream Conventions

- **Language:** Kain (.kn files)
- **Naming:** snake_case for functions, PascalCase for structs
- **Code reuse:** L1-L7 typechecking functions should reuse L0 check functions where possible (e.g., world state fields reuse `check_let_stmt` patterns)
- **Codegen:** L1-L7 codegen functions should follow the same `GenTypeResult` / `emit_raw` pattern as L0 codegen
- **Comments:** Clear `// ── GOLD: L<X> ──` markers for each layer
- **Stubs for rare constructs:** Some constructs (e.g., `dispatch` inside non-GPU contexts) can remain as "emit comment + fallthrough" stubs initially

---

## Stream Boundary — What You Do NOT Do

- ❌ Do NOT modify parser.kn or lexer.kn (all L1-L7 parse rules exist)
- ❌ Do NOT modify orchestrator.kn or compiler.kn
- ❌ Do NOT modify the L0 typechecking functions (RED's work)
- ❌ Do NOT modify the L0 codegen functions (BLUE's work)
- ❌ Do NOT change the 20 ResolvedType variants — add new ones only if absolutely needed
- ❌ Do NOT add new AST_ITEM_* or AST_EXPR_* constants — parser already has all needed

**NOTE:** This stream is fully deferred. Do NOT start until RED and BLUE are complete and ouroboros Phase 2 passes. The current L1-L7 stubs (returning minimal TypedItems) are sufficient for the self-host compiler's own source.

---

## Verification (After This Stream)

```bash
# All smoketest files should now typecheck and compile
kain check X:\smoketest\src\
kain build X:\smoketest\src\ --target llvm

# Specific L1-L7 acid tests
kain check X:\smoketest\src\semantics\world.kn
kain check X:\smoketest\src\semantics\actor.kn
kain check X:\smoketest\src\semantics\converge.kn
kain check X:\smoketest\src\semantics\orchestrate.kn
kain check X:\smoketest\src\semantics\pulse.kn
kain check X:\smoketest\src\gpu\compute.kn

# Full self-compilation with L1-L7 support
kain selfhost bootstrap --manifest src/KAIN.toml --verify-ouroboros
```

---

## Completion Report

When done, report:
- L1-L7 typechecking functions made real: <count>
- L1-L7 codegen functions added: <count>
- Smoketest files passing typecheck: <N>/91
- Smoketest files producing real codegen: <N>/91
- Any L1-L7 constructs still stubbed: <list>
- Any issues encountered: <list or "none">
