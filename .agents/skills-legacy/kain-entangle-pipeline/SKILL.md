---
name: kain-entangle-pipeline
description: Use when adding, changing, debugging, validating, or reviewing Kain's first-class entangle Topological State Coupling pipeline, including crates/kain-entangle, Kain core AST/parser/type/runtime wiring, runtime contract and realtime bundle entanglements, state.entangle capability metadata, editor symbols, and single-writer propagation semantics.
---

# Kain Entangle Pipeline

## V1 Semantics

- Syntax: `entangle Authority.path <-> Mirror.path with single_writer`
- The left endpoint is authoritative. The right endpoint is a mirror.
- Endpoints must be stable dotted storage paths with at least two segments.
- Both endpoints must resolve through Kain's value/type environment and have the same resolved storage type after shared-reference peeling.
- `single_writer` means authority writes propagate to the mirror and direct mirror writes are rejected.
- The feature is compiler-owned metadata plus interpreter behavior, with native target support now split by lane: LLVM registers entanglements through the native C runtime hook, while direct C output preserves a static metadata table.

## Main Files

- `crates/kain-entangle/src/lib.rs`: shared endpoint ids, policy names, binding descriptors, capability string, and `EntangleGraph`.
- `crates/kain-core/src/ast.rs`: `Item::Entangle`, endpoint/policy AST types.
- `crates/kain-core/src/parser.rs`: contextual top-level `entangle` parsing.
- `crates/kain-core/src/types.rs`: endpoint resolution, duplicate/self endpoint rejection, same-type checks, `TypedItem::Entangle`.
- `crates/kain-core/src/runtime.rs`: registration, mirror-write guard, authority-to-mirror propagation.
- `crates/kain-core/src/runtime_contract.rs`: `RuntimeContractBundle.entanglements`, `state.entangle` requirement, reflection summaries.
- `crates/kain-core/src/realtime_app_bundle.rs`: `RealtimeAppBundle.entanglements` and tool requirements.
- `crates/kain-core/src/formatter.rs`: source formatting for entangle declarations.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`: LLVM entangle registration function and runtime hook calls.
- `crates/kain-sys-codegen/src/codegen_c.rs`: direct C entangle metadata table and world/intent callable lowering.
- `runtime/native/include/kain_runtime_entangle.h`: native C entangle registry ABI.
- `runtime/native/src/core/kain_runtime_entangle.c`: fixed-capacity runtime registry implementation.
- `runtime/native_runtime.toml`: manifest source inclusion for the native runtime registry implementation.
- `crates/cli/src/lsp.rs`: document symbol and semantic keyword support.
- `crates/ue5/src/codegen_ue5.rs`: metadata-only exhaustive match handling.
- `docs/runtime/compiler-owned-intents.md`: user-facing semantics and limits.

## Workflow

1. Start by reading `ARCHITECTURE.md`, `MEMORY.md`, and the files above that match the task.
2. Keep entangle as an explicit AST/type/runtime variant. Do not hide it behind wildcard matches in core feature surfaces.
3. If adding a policy, update `kain-entangle`, parser policy parsing, AST formatter, typechecker, interpreter policy enforcement, contracts, bundles, docs, and tests together.
4. If adding backend lowering, prefer `RuntimeContractBundle.entanglements` or `RealtimeAppBundle.entanglements` for adapter work. If you are inside typed codegen, consume `TypedItem::Entangle` directly and keep the emitted shape aligned with the contract/bundle fields.
5. Preserve `state.entangle` as the capability/service-binding name unless doing a deliberate migration.

## Sharp Edges

- Interpreter propagation currently keys off the authored assignment path. Alias writes such as `let p = Physics; p.player_health -= 10` do not canonicalize back to `Physics.player_health` yet.
- Do not use loose assignment compatibility for endpoint matching. Entanglement requires identical storage reality, so keep strict resolved type equality.
- Entanglements inside modules must still be collected recursively by contract and bundle collectors.
- Realtime/native UI staging must not build UI from the flattened frontend import bundle. Feed `build_ui_output_from_source(...)` target-prepared source instead: flattened source still contains the original `use`, so imported `world` / `entangle` declarations will be registered once as inlined items and again through module loading.
- LLVM codegen currently collects typed entangles recursively for native registration; keep that aligned if module codegen expands.
- Direct C output preserves metadata but does not call `kain_runtime_entangle_register` by default, so do not claim runtime-linked propagation from C output until a registration strategy is added.
- Direct writes to mirror endpoints should fail even if the value is identical.
- If `TypedItem` or `Item` gains new exhaustive consumers, add `Entangle` deliberately as metadata, a symbol, or a backend lowering input depending on the surface.
- The native runtime registry is intentionally fixed-capacity and string-copy bounded. Keep `kain_runtime_copy_entangle_text(...)` and binding-count guards aligned with the repo-local proof pack when changing native entangle storage.

## Validation

- `cargo test -p kain-entangle --target-dir target\codex-entangle`
- `cargo test -p kain-core entangle --target-dir target\codex-entangle -- --nocapture`
- `cargo test -p kain-core --test compiler_owned_intent_test --target-dir target\codex-entangle -- --nocapture`
- `cargo test -p kain-sys-codegen --target-dir target\codex-llvm-refresh`
- `cargo test -p cli --lib stage_llvm_native_artifacts_materializes_entangle_metadata --target-dir target\codex-llvm-refresh -- --nocapture`
- `toolchain\llvm\bin\clang.exe -c runtime\native\src\core\kain_runtime_entangle.c -Iruntime\native\include -o target\codex-llvm-refresh\kain_runtime_entangle.obj`
- `uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\runtime\native\src\core --lane entangle`
- `$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\codex-entangle`
- `git diff --check`

If you touch shared proof templates or the native proof manifest, also rerun workspace orchestration:

- `uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --workspace --project-root D:\Kain-Lang --lane smoke`
