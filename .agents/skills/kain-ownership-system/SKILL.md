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
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`: lowers ownership expressions to checked native runtime calls. The current hot path is split by provenance: helper-owned allocations use `__kain_ownership_*_helper(...)`, while imported or unknown pointers first go through `__kain_ownership_ensure_imported(...)` and then use the safe registry-only ownership calls.
- `runtime/native/include/kain_runtime_ownership.h` and `runtime/native/src/core/kain_runtime_ownership.c`: native ownership ABI and guarded registry.
- `runtime/native/include/kain_runtime_memory.h` and `runtime/native/src/core/kain_runtime_memory.c`: helper allocation header shape, heap allocation/realloc/free bridge, and the packed helper slot-token fast path.

## Native Registry Hot Path

- `kain_runtime_ownership.c` uses a serialized global registry, but pointer lookup is no longer a full linear scan. It has an 8192-entry masked pointer index driven by a SplitMix-style pointer mixer.
- Helper-owned heap allocations now cache `slot + 1` in the low 16 bits of the allocation header's `magic_and_slot` word. That keeps the header at 16 bytes and lets helper-only `observe`/`collapse`/`decay` resolve helper-owned regions directly instead of re-hashing the pointer on every runtime call.
- Imported or stack/FFI pointers must stay on the registry-only path. Do not reintroduce helper-header probing on generic ownership calls: the solver already found a witness where a fake helper-looking prefix can make a generic prepare step succeed without making the later ownership operation safe.
- Free region discovery uses 64-bit occupancy words plus a de Bruijn low-bit decoder. If `KAIN_OWNERSHIP_MAX_REGIONS` changes, revisit the occupancy word count, pointer-index capacity, and experimental SMT proofs together.
- Realloc/update rebuilds the pointer index after changing a region pointer. Keep that rebuild unless a deletion/tombstone scheme is added and proved.
- The current experimental arithmetic proofs live in `runtime/native/src/core/z3/proofs-experimental/ownership-*.smt2`.

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
- `cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_consumes_lowered_alloc_and_realloc_helpers --target-dir target\codex-ownership-check -- --nocapture`
- `clang -fsyntax-only -I runtime/native/include runtime/native/src/core/kain_runtime_memory.c runtime/native/src/core/kain_runtime_ownership.c`
- `clang -I runtime/native/include runtime/native/tests/test_ownership_memory.c runtime/native/src/core/kain_runtime_memory.c runtime/native/src/core/kain_runtime_ownership.c -o target/codex-ownership-check/native_test_ownership_memory.exe; target\codex-ownership-check\native_test_ownership_memory.exe`
- `py -3 tools/bazel/sync_native_runtime_builds.py --check`
- `bazel test //runtime:native_test_ownership_memory`
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\crates\\kain-ownership", lane="full")`
- `mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="ownership")`
