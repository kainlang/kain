---
name: kain-ownership-system
description: Use when adding, changing, debugging, validating, or reviewing Kain's ownership-state memory model, including crates/kain-ownership, collapse/observe/decay semantics, ownership Z3 proof packs, low-level memory expression integration, world/entangle ownership policy, LLVM noalias/readonly/lifetime lowering, and native runtime ownership helpers.
---

# Kain Ownership System

## Current Contract

- `crates/kain-ownership` is the semantic center for `collapse`, `observe`, and `decay`.
- `kain-core` should own syntax, AST nodes, typechecking, interpreter behavior, and runtime-contract emission.
- `kain-sys-codegen` should consume typed ownership descriptors and lower them into LLVM/C/native runtime behavior.
- `runtime/native` should own concrete C ABI helpers for checked ownership guards, heap free/release, and future debug enforcement.
- `OWNERSHIP_CAPABILITY` is currently `memory.ownership`.

## Semantics

- `Idle` is the only state that may enter `Collapsed` or `Decayed`.
- `Observed(n)` may add or release observers, but it may not collapse or decay.
- `Collapsed` is exclusive mutation and must end before observation or decay.
- `Decayed` is terminal. No transition may make the region live again.
- World and entangle-backed regions use snapshot observation in v1. Do not claim direct readonly aliasing over live entangle propagation until epoch/freeze semantics exist.
- Entangled mirrors do not support collapse or decay in v1.
- Imported pointers do not support observe, collapse, or decay until an external ownership contract exists.

## Main Files

- `crates/kain-ownership/src/lib.rs`: ownership states, transitions, policy table, lowering hints, and descriptor type.
- `crates/kain-ownership/z3`: durable proof pack for ownership-state and policy invariants.
- `docs/syntax-and-semantics/low-level-memory.md`: memory expression documentation.
- `crates/kain-core/src/ast.rs`: add future expression/block AST forms here.
- `crates/kain-core/src/parser.rs`: parse future `observe`, `collapse`, and `decay` surface forms here.
- `crates/kain-core/src/types.rs`: reject illegal transitions and attach region policies here.
- `crates/kain-core/src/runtime.rs`: implement interpreter-visible guards and snapshots here.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`: lower policies into LLVM metadata/intrinsics and runtime calls here.
- `runtime/native/src/core`: add runtime guard/free/release helpers here when syntax lowering lands.

## Workflow

1. Read `ARCHITECTURE.md`, `MEMORY.md`, and `crates/kain-ownership/src/lib.rs`.
2. Change the semantic lattice in `kain-ownership` before changing parser or backend behavior.
3. Add or update a Z3 proof under `crates/kain-ownership/z3/proofs` for every new transition or policy mode.
4. Keep region policy data centralized in `OWNERSHIP_POLICY_TABLE`.
5. Treat LLVM `noalias`, `readonly`, and `llvm.lifetime.*` as generated consequences of proven policy, not as user promises.

## Validation

- `cargo fmt -p kain-ownership`
- `cargo test -p kain-ownership --target-dir target\codex-kain-ownership -- --nocapture`
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\crates\\kain-ownership", lane="full")`
- After parser/typechecker integration, also run focused `kain-core` parser/type tests and a `kain check` smoke on a `.kn` fixture.
- After LLVM/native integration, also run the relevant `crates/kain-sys-codegen/z3` lane and a native fixture build.
