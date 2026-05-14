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
- `observe ptr:` and `collapse ptr:` are scoped block expressions in `kain-core`; v1 rejects `return`, `break`, or `continue` inside those scopes so LLVM/native begin/end guards stay balanced.
- `decay ptr` is a unary expression returning `Unit`; native heap allocations free through the ownership runtime, while imported pointers are marked terminal without claiming heap ownership.
- World and entangle-backed regions use snapshot observation in v1. Do not claim direct readonly aliasing over live entangle propagation until epoch/freeze semantics exist.
- Entangled mirrors do not support collapse or decay in v1.
- Imported pointers support borrowed observe/collapse/lifetime-end semantics. They do not support Kain-owned heap free.

## Main Files

- `crates/kain-ownership/src/lib.rs`: ownership states, transitions, policy table, lowering hints, and descriptor type.
- `crates/kain-ownership/z3`: durable proof pack for ownership-state and policy invariants.
- `docs/syntax-and-semantics/low-level-memory.md`: memory expression documentation.
- `crates/kain-core/src/ast.rs`: owns `Expr::Observe`, `Expr::Collapse`, and `Expr::Decay`.
- `crates/kain-core/src/parser.rs`: reserves and parses `observe`, `collapse`, and `decay`.
- `crates/kain-core/src/types.rs`: validates pointer-like targets and rejects unbalanced scoped exits.
- `crates/kain-core/src/runtime.rs`: implements interpreter-visible ownership guards.
- `crates/kain-core/src/runtime_contract.rs`: emits the `memory.ownership` capability when functions use ownership expressions.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`: lowers ownership expressions to checked native runtime calls and lazily imports untracked pointers as imported regions.
- `runtime/native/include/kain_runtime_ownership.h` and `runtime/native/src/core/kain_runtime_ownership.c`: native ownership ABI and guarded registry.
- `runtime/native/src/core/kain_runtime_memory.c`: heap allocation/realloc/free registration bridge.

## Workflow

1. Read `ARCHITECTURE.md`, `MEMORY.md`, and `crates/kain-ownership/src/lib.rs`.
2. Change the semantic lattice in `kain-ownership` before changing parser or backend behavior.
3. Add or update a Z3 proof under `crates/kain-ownership/z3/proofs` for every new transition or policy mode.
4. Keep region policy data centralized in `OWNERSHIP_POLICY_TABLE`.
5. Treat LLVM `noalias`, `readonly`, and `llvm.lifetime.*` as generated consequences of proven policy, not as user promises.

## Validation

- `cargo fmt -p kain-core -p kain-sys-codegen -p kain-ownership`
- `cargo check -p kain-core -p kain-sys-codegen --target-dir target\codex-ownership-check`
- `cargo test -p kain-ownership --target-dir target\codex-ownership-check -- --nocapture`
- `cargo test -p kain-core --test ownership_keywords_test --target-dir target\codex-ownership-check -- --nocapture`
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_ownership_keywords_to_runtime_guards --target-dir target\codex-ownership-check -- --nocapture`
- `py -3 tools/bazel/sync_native_runtime_builds.py --check`
- `bazel test //runtime:native_test_ownership_memory`
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\crates\\kain-ownership", lane="full")`
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="ownership")`
