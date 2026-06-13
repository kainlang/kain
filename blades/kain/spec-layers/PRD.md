# PRD: Kainc Self-Host Compiler — Decision Layer Implementation (L1-L7 + GPU)

## Summary

Implement all decision ladder layers (L1 through L7) plus GPU in the Kain self-host compiler (`blades/kain/src/`). Each layer has a detailed spec doc and task file in `X:\blades\kain\spec-layers\`. The compiler currently has parser stubs and typechecker/codegen stubs for most constructs — the goal is to replace every stub with real implementations that produce correct LLVM IR.

## Current State

- **Parser**: Mostly works for all layers. Some gaps in structured parsing (converge lanes, orchestrate stages, pulse durations, resonate endpoints, axiom predicates).
- **Typechecker**: All layer constructs have `check_*_stub` functions that return hardcoded types with no validation.
- **Codegen**: Only `AST_ITEM_FUNCTION` is compiled. World, entangle, patch, law, converge, orchestrate, pulse, resonate, axiom, shatter, teleport, actor, spawn, send, ask, collapse, observe, decay, share, fanout, shader, dispatch — ALL are silently dropped.

## Layer Dependencies

```
L1 State Authority  ─┐
                     ├── L3 Dispatch (converge) ─── L4 Stage (orchestrate)
L2 State Integrity  ─┘                             L5 Temporal (pulse, resonate)
                                                      L6 Machine Stones (axiom, shatter, teleport)
                                                        L7 Systems (actor, ownership)
                                                          GPU (shader, dispatch)
```

- **L1 and L2 are independent** of each other — can be done in parallel.
- **L3 depends on L0 only** (function signatures, type inference).
- **L4 depends on L3** (orchestrate stages can reference converge lanes).
- **L5 depends on L1** (pulse/resonate bodies access world fields).
- **L6 depends on L1** (teleport needs world names; axiom is standalone).
- **L7 depends on L0** (actor/ownership are standalone systems).
- **GPU depends on L0** (shader/dispatch are standalone).

## Layer Specifications

Each layer has a spec doc (architecture + edge cases) and a task file (implementation steps):

| Layer | Spec | Tasks | Target Files | Constructs |
|-------|------|-------|-------------|------------|
| L1 | `L1_state.md` | `L1_state_TASK.md` | `src/types.kn`, `src/codegen.kn` | world, entangle |
| L2 | `L2_integrity.md` | `L2_integrity_TASK.md` | `src/types.kn`, `src/codegen.kn` | patch, law |
| L3 | `L3_dispatch.md` | `L3_dispatch_TASK.md` | `src/L3_dispatch.kn` (new), `src/types.kn`, `src/codegen.kn` | converge |
| L4 | `L4_stage.md` | `L4_stage_TASK.md` | `src/L4_stage.kn` (new), `src/types.kn`, `src/codegen.kn` | orchestrate |
| L5 | `L5_temporal.md` | `L5_temporal_TASK.md` | `src/types.kn`, `src/codegen.kn`, `src/parser.kn` | pulse, resonate |
| L6 | `L6_stones.md` | `L6_stones_TASK.md` | `src/types.kn`, `src/codegen.kn`, `src/parser.kn`, `src/ast.kn` | axiom, shatter, teleport |
| L7 | `L7_systems.md` | `L7_systems_TASK.md` | `src/L7_systems.kn` (new), `src/types.kn`, `src/codegen.kn` | actor, spawn, send, ask, collapse, observe, decay, share, fanout |
| GPU | `GPU.md` | `GPU_TASK.md` | `src/GPU.kn` (new), `src/types.kn`, `src/codegen.kn` | shader, dispatch |

## Key Files

| File | Role |
|------|------|
| `X:\blades\kain\KN.MD` | Compiler state dashboard — MUST be updated after each layer |
| `X:\blades\kain\src\types.kn` | Typechecker — all `check_*_stub` replacements go here |
| `X:\blades\kain\src\codegen.kn` | Codegen — all LLVM IR emission goes here |
| `X:\blades\kain\src\parser.kn` | Parser — structured parsing improvements |
| `X:\blades\kain\src\ast.kn` | AST constants — new node kinds |
| `X:\blades\kain\src\compiler.kn` | Pipeline routing — may need updates |
| `X:\blades\kain\src\runtime.kn` | Runtime ABI declares |

## Acceptance Criteria

For each layer:
1. `kain check` on a file using the layer's constructs passes with no diagnostics
2. `kain build --target llvm` produces valid LLVM IR with correct ABI calls
3. Error cases produce meaningful diagnostics (not silent failures or crashes)
4. `blades/kain/KN.MD` is updated with the layer's completion status

## Worker Skills

Each worker should load:
- `lang-semantics` for understanding Kain constructs
- `wildcard-justwritebro` for fast Kain code writing
- `formal-verification` if Z3 proof work is needed

## Constraints

- All code is Kain (`.kn` files) — no Rust, no C
- Workers must validate with `kain_lang check` after each change
- Workers must update `KN.MD` when done with their layer
- Multiple layers touching `types.kn` or `codegen.kn` need file reservation coordination
