---
name: bootstrap-core
description: >-
  Use when changing compiler, frontend, or selfhost truth for generic Kain
  semantics in `crates/core`, `crates/sys-codegen`, `src/core`,
  `crates/reflect`, or adjacent proof packs: parser or AST behavior,
  typechecking, diagnostics, runtime-contract reflection, module resolution,
  compiler-owned keywords, or generic lowering that is not primarily actors,
  ownership, filesystem, or GPU. Do not use for authored `.kn` work or raw
  `runtime/native` substrate changes.
---

# Bootstrap Core

Use this skill when the primary job is changing what Kain means, how the frontend proves it, or how compiler-owned lowering carries that meaning forward.

## Trigger Surface

- `crates/core/**` for parser, AST, typechecker, diagnostics, runtime execution, module resolution, stdlib loading, and runtime-contract emission.
- `crates/sys-codegen/**` when the lowering change is generic compiler truth rather than a domain-specific actor, ownership, filesystem, or GPU lane.
- `src/core/**` and nearby selfhost compiler-owned Kain sources when they define language behavior instead of serving as app-style examples.
- `crates/reflect/**` and generic reflection or contract metadata that the frontend emits and other subsystems consume.

## Boundaries

- Use `bootstrap-actors`, `bootstrap-ownership`, `bootstrap-fs`, or `bootstrap-gpu` when one of those domains is the dominant owner.
- Co-trigger `runtime-core`, `runtime-stdlib`, or `runtime-gpu` when the change requires `runtime/native` ABI or host substrate work to stay truthful.
- Co-trigger `lang-semantics`, `lang-authoring`, or another `lang-*` skill when the main deliverable is authored Kain source, specimens, or blade code.
- Co-trigger `tool-build-system` when `BUILD.bazel`, `MODULE.bazel`, runtime manifests, generated BUILD files, or sync scripts must move with the compiler change.

## Workflow

1. Read `ARCHITECTURE.md` and the relevant `MEMORY.md` notes before editing semantics.
2. Change the semantic owner first, then thread that truth through parser, types, runtime-contract emission, and lowering. Do not patch lower layers around a wrong frontend model.
3. For new item clauses such as generic `where`, add the real AST surface first, then normalize and validate later. Keep test fixtures honest by updating manual AST constructors instead of making semantic fields optional by accident.
4. Keep proof ownership local: `crates/core/z3` for parser, keyword, diagnostics, and layout claims; `crates/sys-codegen/z3` for lowering arithmetic and CFG claims.
5. Dogfood the smallest real proof surface that exercises the feature instead of relying on generic green test sweeps.

## Validation Loop

```powershell
cargo check -p kain-core -p kain-sys-codegen
cargo test -p kain-core --test semantic_typecheck_test --target-dir target\codex-bootstrap-core -- --nocapture
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path crates\core --lane parser
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path crates\core --lane keywords
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path crates\core --lane full
```

If lowering math or CFG shape changed, also run:

```powershell
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path crates\sys-codegen --lane llvm
```

## Guardrails

- Do not let `runtime/native` become the source of language meaning.
- Do not hide new semantics only in CLI glue, host wrappers, or generated artifacts.
- If the change is really a build-system contract problem, move that work under `tool-build-system` instead of bloating this skill.
