---
name: bootstrap-ownership
description: >-
  Use when changing compiler, frontend, or selfhost truth for `collapse`,
  `observe`, and `decay` in `crates/kain-ownership`, `crates/kain-core`,
  `crates/kain-sys-codegen`, or the ownership proof packs: state-lattice
  rules, parser and typechecker behavior, runtime-contract policy, or lowering
  metadata. Do not use for raw `runtime/native` ownership helpers or for
  authored ownership demos.
---

# Bootstrap Ownership

Use this skill when the ownership lattice or compiler-owned ownership policy is the thing changing.

## Trigger Surface

- `crates/kain-ownership/**` for region kinds, legal transitions, policy tables, lowering hints, and the portable ownership kernel.
- `crates/kain-core/src/{ast.rs,parser.rs,types.rs,runtime.rs,runtime_contract.rs}` for surface syntax, type rules, interpreter guards, and reflected ownership policy.
- `crates/kain-sys-codegen/**` for LLVM or direct-C lowering that consumes typed ownership descriptors.

## Boundaries

- Co-trigger `runtime-core` when `runtime/native/src/core/ownership.c` or other native ownership helpers must change to preserve compiler promises.
- Co-trigger `lang-ownership` when the primary work is authored Kain using `collapse`, `observe`, or `decay`.
- Co-trigger `test-bench` when the real evidence surface is a benchmark row such as `ownership_memory`.
- Co-trigger `tool-build-system` when manifests, generated BUILD state, or sync scripts must move with ownership compiler work.

## Workflow

1. Change the lattice and policy table in `crates/kain-ownership` first. That crate remains the semantic center.
2. Add or update a durable proof in `crates/kain-ownership/z3` before trusting tests.
3. Thread the new policy through parser, typechecker, runtime-contract emission, and interpreter behavior.
4. Only then touch lowering metadata or ABI-facing consequences.

## Validation Loop

```powershell
cargo test -p kain-ownership --target-dir target\codex-bootstrap-ownership -- --nocapture
cargo test -p kain-core --test ownership_keywords_test --target-dir target\codex-bootstrap-ownership-core -- --nocapture
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-ownership --lane full
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-core --lane keywords
```

If lowering changed, also run:

```powershell
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-sys-codegen --lane llvm
```

## Guardrails

- `OWNERSHIP_POLICY_TABLE` stays canonical; do not duplicate policy truth across frontend files.
- Imported pointers stay conservative until an external ownership contract proves otherwise.
- Native helpers are consequences of the lattice, not the place where the lattice gets invented.
