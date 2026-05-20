---
name: kain-translation-engineer
description: Translate Rust, C++, TypeScript, JavaScript, or existing MCP/tooling code into idiomatic Kain rather than mechanical ports. Use when Codex is asked to convert foreign-language files or folders to .kn, replace Rust/C++/TypeScript services with Kain blades/modules, migrate an MCP/server/tool implementation to Kain, consult benchmark/cases as translation exemplars, or choose root stdlib APIs while authoring translated Kain.
---

# Kain Translation Engineer

## Overview

Translate into Kain by preserving the source system's intent, invariants, and public contract, then re-expressing them with Kain's own semantics: root stdlib domains, actors, worlds/entangle, ownership scopes, converge lanes, laws, patches, and proof-backed low-level memory where appropriate.

Treat importer output as a donor sketch, not the final architecture. The goal is Kain-native behavior, not Rust/C++/TypeScript wearing `.kn`.

## Required First Pass

1. Search `ARCHITECTURE.md` and `MEMORY.md` for the subsystem, source root, error strings, and target blade. Do not wander outside the requested source root unless those files point to an owned dependency.
2. Inspect `stdlib/STDLIB_MAP.llm.md` for matching root domains. Use `rg "Cook By Need|std::fs|std::net|std::process|std::text|std::collections" stdlib/STDLIB_MAP.llm.md` instead of guessing symbol names.
3. Rank current benchmark exemplars before copying old instincts:

```powershell
py .agents\skills\kain-translation-engineer\scripts\select_translation_examples.py --repo . --top 10
```

4. Read only the benchmark cases relevant to the translation shape. Start with `references/benchmark-translation-compass.md`, then open the listed `.kn`, `.rs`, and `.cpp` files when the task touches performance-sensitive structure.
5. For app/server/UI/ABI/network translations, read `references/example-atlas.md` before broader searching. It points at the compact Kain blades worth using as local RAG seeds.
6. For Rust, C, C++, or TypeScript importer work, also use the `kain-engineer` skill. For new runnable blades, use the relevant blade skill.

## Translation Workflow

1. Inventory the donor code: entrypoints, public API, data models, IO surfaces, concurrency model, unsafe/raw-memory zones, config files, generated files, tests, and external dependencies.
2. Extract the semantic graph: commands, state transitions, message flow, invariants, hot loops, byte layouts, and failure modes. Name what the source is trying to prove, not only what syntax it uses.
3. Choose the Kain ownership surface:
   - `blades/<name>` for runnable apps, MCP servers, demos, and acceptance shells.
   - `stdlib/` for reusable public Kain capabilities.
   - `runtime/native` or C FFI only for OS, driver, ABI, filesystem watcher, socket, GPU, or platform substrate.
   - `benchmark/cases` when performance is part of the claim.
   - `attrition/` when long-run cleanup, sabotage, or teardown closure matters.
4. Draft the Kain version around Kain constructs:
   - Use `use std::<domain>` root imports instead of local helper clones.
   - Replace lock/channel/service boilerplate with `actor`, `ask`, `send`, `world`, `entangle`, `patch`, and `pulse` when they model the real system.
   - Replace RAII or lifetime ceremony with `collapse`, `observe`, and `decay` when the problem is ownership of a bounded resource.
   - Use `converge` for "clear reference spec plus fast lane" translations. Keep the scalar/spec version readable; make fast lanes target-gated and proof-backed.
   - Use `law` and `Result`/`Option` for boundary validation rather than stringly error drift.
   - Use `shatter struct`, packed fields, `ptr_offset`, `mem_load`, and `mem_store` for layout-critical code only with Z3 bounds/equivalence proof.
5. Validate in layers: compile the smallest `.kn` proof first, run the blade or benchmark case, add Z3 for index/layout/state-machine claims, then broaden to integration.

## MCP Translation Rule

For a source tree such as `mcp/reference`, treat Rust files as a donor/oracle and `blades/kain-mcp` as the canonical Kain-owned MCP direction. Prefer data-driven tool catalogs such as `config/tools.json`, root stdlib domains, and Kain actors/worlds over recreating the Rust module graph.

Typical mapping:

- `executor.rs` -> `std::process` plus a Kain command actor or blade service.
- `file_tree.rs`, `transaction.rs`, `memory.rs` -> `std::fs`, `std::text`, `std::collections`, and explicit laws for path/resource bounds.
- `web.rs` -> `std::net`, `std::http`, `std::tls`, `std::http2`; use native runtime substrate only where the stdlib map lacks a needed primitive.
- `telemetry.rs` -> Kain diagnostics/status plus data-driven counters.
- `path_index*.rs`, `ast_query.rs`, screenshots, and OS-specific lanes -> keep a spec lane in Kain and isolate native/FFI substrate if Kain does not yet expose the primitive.

Use `blades/network-domains/src/main.kn` as the first net/http donor before inventing server, request, route, TLS, or HTTP/2 shapes.

## Do Not

- Do not transliterate classes, traits, modules, mutexes, callbacks, promises, or template layers one-for-one.
- Do not recreate stdlib functions locally before checking `stdlib/STDLIB_MAP.llm.md`.
- Do not use benchmark wins as marketing copy without preserving their maturity and fairness notes.
- Do not hide compiler/runtime blockers inside workarounds. Patch the owning Kain subsystem or leave a precise blocker.
- Do not claim a performance migration is done without a benchmark row or focused report.
- Do not overbuild this skill into a static RAG dump. Keep it as a pointer map; let the Kain MCP/indexer own semantic search over examples.

## References

- `references/translation-patterns.md`: crosswalk from Rust/C++/TypeScript shapes into Kain constructs and stdlib domains.
- `references/benchmark-translation-compass.md`: current top benchmark exemplars and how to use them as style donors.
- `references/example-atlas.md`: compact blade atlas for net/http, stdlib, UI, actors, hash, Pong, and `use c`/C ABI examples.
- `scripts/select_translation_examples.py`: reranks current benchmark reports so future agents do not freeze this skill to one timestamp.
