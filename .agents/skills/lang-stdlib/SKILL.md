---
name: lang-stdlib
description: >-
  Use when authoring, explaining, reviewing, repairing, extending, benchmarking, or certifying Kain root `std::*` work: choosing public stdlib imports, querying exact symbols, adding or expanding top-level `stdlib/*.kn` modules, keeping atlas and smoketest proof surfaces current, and routing stdlib-backed compiler/runtime issues to the owning bootstrap/runtime lanes. Use for both consuming the existing root stdlib and growing it; do not use this as the primary skill for overlay-only work under non-root helper trees unless that work is explicitly in service of a root stdlib surface.
---

# Lang Stdlib

This skill is the root Kain stdlib operator manual. Use it to write authored Kain against the public `std::*` surface and to grow that surface with live atlas truth plus smoke/evidence wiring.

## Prime Directive

- Prefer public root imports such as `use std::fs`, `use std::math`, `use std::http`, and `use std::ui`.
- Query the atlas before spelunking. Use `query_stdlib.py`, then open exact source files only where needed.
- Treat private `abi_*` symbols as stdlib-internal runtime wiring, not authoring APIs.
- Treat `@extern` root stdlib functions as runtime-backed ABI surfaces. Escalate to the owning runtime/bootstrap lane when behavior is wrong.
- Regenerate `stdlib/STDLIB_MAP.llm.md` and `stdlib/stdlib.map.json` whenever top-level `stdlib/*.kn` changes.
- Mesh real stdlib changes into `smoketest/`, not just blade-local proofs.
- `std::python` is the explicit CPython bridge lane for module checks/imports, raw attribute and call paths, buffer views, async futures, and actor callbacks. Prefer first-class `import ...` for routine package usage; use `std::python` when you need direct bridge control.

## Fast Operator Loop

Use these commands first:

```powershell
python query_stdlib.py --summary
python query_stdlib.py --imports
python query_stdlib.py --module fs --contains path --limit 40
python query_stdlib.py --module math --contains vec3 --limit 40
python query_stdlib.py --module python --contains python_ --limit 60
python query_stdlib.py --search json --limit 40
python query_stdlib.py --search thread --limit 40
rg -n "^use std::" library_of_kain blades benchmark smoketest
rg -n "\b(fs_|tcp_|process_|graphics_|ui_|python_|hash_|text_|json_)\b" stdlib blades benchmark smoketest runtime
kain check <entry.kn> --target llvm
kain run <entry.kn-or-blade> --target llvm
```

Use these commands for root stdlib maintenance:

```powershell
cargo run -q -p kain-stdlib-map --bin kain_stdlib_map_tool -- --write
cargo run -q -p kain-stdlib-map --bin kain_stdlib_map_tool -- --check
```

## Work Modes

Use this skill in three modes:

1. Consume existing root stdlib.
   - Choose the right import and exact symbol.
   - Write a compact authored Kain proof against public APIs.
   - Route failures to the correct bootstrap/runtime owner.
2. Extend an existing root module.
   - Tighten thin surfaces such as `std::fs`, `std::time`, `std::crypto`, `std::process`, or `std::python`.
   - Replace stringly or `Any`-shaped gaps with typed public records/builders where practical.
   - Mesh the new surface into `smoketest/`, blades, and evidence.
3. Add a new root family.
   - Create a top-level `stdlib/*.kn` module when the family is broad and author-facing.
   - Wire it into atlas + smoke + at least one real consumer path when practical.
   - Prove it through the same pipeline as any other root stdlib surface.

## Root Scope Contract

Root stdlib truth is defined by live surface + proof wiring, not by a separate static checklist.

A root capability is considered real when:

- the public import exists in the live atlas (`query_stdlib.py --imports`)
- symbols and signatures appear in the generated map
- smoke wiring exists and is called from the shared album flow
- runtime-backed behavior is proven in the right lane (`runtime/conformance`, `benchmark`, `attrition`, or proof packs) when the claim requires it

## Completion Pipeline

Follow this pipeline for real root stdlib work:

1. Query the atlas to confirm what the public root surface already exposes.
2. Search `blades/`, `benchmark/`, `attrition/`, `smoketest/`, and `library_of_kain/` for nearby authoring patterns.
3. Decide whether the capability belongs in an existing root module or a new top-level `stdlib/*.kn` file.
4. Implement the public Kain-facing API first. Keep `abi_*` helpers private unless the module is explicitly stdlib-maintenance-only.
5. Land backing work in `crates/*`, `runtime/native`, or other owning subsystems if the surface is runtime-backed.
6. Mesh the new surface into `smoketest/`, and make at least one non-stdlib consumer call it when practical.
7. Add deeper evidence in `benchmark/`, `attrition/`, `runtime/conformance/`, and Z3 proof packs when required by the claim.
8. Regenerate and check the stdlib atlas.
9. Update `MEMORY.md`, owning skills, or lane docs when operator truth changed.

## Compiler To Runtime Flow

Root stdlib work normally flows like this:

```text
authored Kain
-> public root import such as std::fs or std::python
-> top-level stdlib/*.kn module
-> private abi_* declarations and pure Kain wrappers/helpers
-> stdlib loader and import truth in crates/core
-> portable Rust contract crates when present
-> runtime/native C ABI substrate and service tables
-> smoketest, blades, benchmark, attrition, runtime conformance, and Z3 evidence
```

Primary source anchors:

- Live root atlas: `stdlib/STDLIB_MAP.llm.md`, `stdlib/stdlib.map.json`
- Atlas query helper: `query_stdlib.py`
- Atlas generator: `crates/stdlib-map`
- Root stdlib source: `stdlib/*.kn`
- Stdlib loading and target profile ordering: `crates/core/src/stdlib.rs`
- Parser/typechecker import consumers: `crates/core/src`
- Portable owners: `crates/fs`, `crates/input`, `crates/net`, `crates/process`, `crates/actor`
- Native runtime owners: `runtime/native/include`, `runtime/native/src/core`, `runtime/native/src/ui`, `runtime/native/src/graphics`
- Smoketest album: `smoketest/src/main.kn`, `smoketest/build.kn`, `smoketest/src/stdlib`
- Proof blades: `blades/stdlib-foundations`, `blades/stdlib-domains`, `blades/network-domains`, `blades/hash-domains`, `blades/math-domains`
- Donor baseline for gap pressure: `reference/langs/zig/lib/std`

## Live Root Profile

Do not trust hardcoded module counts in docs. Pull live truth from the atlas:

```powershell
python query_stdlib.py --summary
python query_stdlib.py --imports
python query_stdlib.py --module python --contains python_ --limit 60
```

## Choose By Need

Use this family map before opening large files:

| Need | Use | First Symbols / Patterns | Co-trigger |
| --- | --- | --- | --- |
| Start or stop native services, heap checks, machine counters | `std::runtime` | `runtime_init`, `runtime_shutdown`, `runtime_heap_validate`, `runtime_converge_*`, `runtime_machine_*` | `runtime-core` |
| Observe semantic runtime behavior | `std::intent` | `entangle_*`, `patch_journal_count`, `law_status`, `converge_mismatch_count`, `orchestrate_stage_count` | `lang-semantics` |
| Actors, registry, supervision, scheduler telemetry | `std::actor` | `actor_spawn`, `actor_send`, `actor_monitor`, `actor_scheduler_queue_depth` | `lang-systems`, `runtime-core` |
| Tiny OTP-shaped services | `std::gen_server` | `gen_server_start`, `gen_server_call`, `gen_server_cast` | `lang-systems` |
| Build/proof/bench/attrition/certify DAG work | `std::build`, `std::proof`, `std::bench`, `std::attrition`, `std::certify`, `std::test` | `build_graph`, `proof_obligation`, `bench_case`, `attrition_case`, `certify_gate`, `test_bool` | `lang-projects`, `test-harness`, `test-bench`, `test-attrition` |
| Explicit CPython interop and bridge control | `std::python` | `python_import`, `python_module_available`, `python_region_*`, `python_buffer_view_*`, `python_call_async` | `lang-interop`, `runtime-stdlib` |
| Typed maps, queues, slot maps, clamps | `std::collections` | `typed_map_*`, `queue_*`, `priority_queue_*`, `slot_map_*`, `int_clamp` | `lang-systems` |
| Arena, bump, and pool allocation | `std::alloc` | `arena_create`, `arena_alloc`, `bump_alloc`, `pool_alloc` | `lang-systems` |
| String views and zero-copy text slicing | `std::text` | `text_slice`, `text_trim`, `text_find`, `text_materialize`, `string_view_*` | `lang-systems` |
| Digests and random bytes | `std::crypto` | `sha256`, `hmac_sha256`, `blake3`, `random_bytes_hex` | `runtime-stdlib`, `lang-interop` |
| Low-level memory fences and int atomics | `std::memory` | `volatile_*`, `atomic_load_*`, `atomic_store_*`, `atomic_compare_exchange_seqcst`, `atomic_fence_*` | `lang-systems`, `runtime-core` |
| Files, temp, metadata, path helpers | `std::fs` | `fs_read_text`, `fs_write_text`, `fs_temp_file`, `fs_hash_file`, `fs_path_join` | `runtime-stdlib`, `bootstrap-fs` |
| Child processes and PTYs | `std::process` | `process_spec_create`, `process_spawn`, `process_wait`, `process_stdout_capture_text` | `runtime-stdlib` |
| Platform identity and dynamic libraries | `std::platform` | `platform_current_name`, `platform_library_open`, `platform_library_resolve` | `lang-interop`, `runtime-stdlib` |
| TCP and network state | `std::net` | `net_reset`, `tcp_connect`, `tcp_listen`, `tcp_read_text`, `tcp_write_text` | `runtime-stdlib` |
| HTTP request/response/server work | `std::http` | `request_create`, `server_create_localhost`, `respond_text`, `route_actor` | `lang-interop`, `runtime-stdlib` |
| GPU resource contracts and shared buffers/images | `std::gpu` | `gpu_resource_policy`, `gpu_shared_buffer_zeroed`, descriptor constants | `lang-gpu`, `runtime-gpu` |
| Native graphics sessions and draw calls | `std::graphics`, `std::graphics::shared` | `graphics_session_create`, `graphics_shader_spirv_from_hex`, `graphics_draw_mesh`, `graphics_shared_vertex_buffer` | `lang-gpu`, `runtime-gpu` |
| Native UI sessions, nodes, styles, events, dialogs | `std::ui` | `ui_session_create`, `ui_node_create`, `ui_node_set_rect`, `ui_push_event`, `ui_draw_command_count` | `lang-ui`, `package-kaintana` |

For exact signatures and full symbol lists, query the atlas instead of loading everything into context.

## Smoketest Mesh Contract

Treat `smoketest/` as the default mixed-surface proof lane for root stdlib work.

When the public root stdlib changes:

- add or extend `smoketest/src/stdlib/*_lane.kn`
- import and call the lane from `smoketest/src/main.kn`
- update `smoketest/build.kn` inputs
- bump `total_tracks` in `smoketest/src/main.kn` when a new track is added
- make another track, blade, or package consume the surface when practical

Style anchors:

- `smoketest/src/stdlib/alloc_lane.kn`
- `smoketest/src/stdlib/collections_lane.kn`
- `smoketest/src/stdlib/crypto_lane.kn`
- `smoketest/src/stdlib/diagnostics_lane.kn`
- `smoketest/src/stdlib/fs_lane.kn`
- `smoketest/src/stdlib/math_lane.kn`
- `smoketest/src/stdlib/platform_lane.kn`
- `smoketest/src/stdlib/text_lane.kn`
- `smoketest/src/stdlib/time_lane.kn`

## Evidence Ladder

Use the shallowest honest proof that matches the claim, then escalate:

1. Atlas and root source integrity

```powershell
cargo run -q -p kain-stdlib-map --bin kain_stdlib_map_tool -- --write
cargo run -q -p kain-stdlib-map --bin kain_stdlib_map_tool -- --check
```

2. Root domain blades

```powershell
kain check blades/stdlib-foundations/src/main.kn --target llvm
kain check blades/stdlib-domains/src/main.kn --target llvm
kain check blades/network-domains/src/main.kn --target llvm
kain check blades/hash-domains/src/main.kn --target llvm
kain check blades/math-domains/src/main.kn --target llvm
```

3. Smoketest album

```powershell
kain check smoketest/src/main.kn --target llvm
```

4. Benchmark for performance-sensitive claims

```powershell
rg -n "stdlib|json|fs|hash|process|net|alloc|text|python" benchmark/benchmarks.json benchmark/cases
python benchmark/run.py --case <case> --languages kain --runs 5 --warmups 1 --timeout 240
```

5. Attrition for resource/teardown-sensitive surfaces

```powershell
rg -n "stdlib|json|fs|process|net|ui|graphics|reload|python" attrition/attritions.json attrition/cases
python attrition/run.py --case <case> --scale small --timeout 120
```

6. Runtime conformance for ABI-backed surfaces

```powershell
Get-ChildItem -Name runtime\conformance
```

7. Z3 when the surface relies on unsafe math, pointer/index bounds, packed layouts, branchless selectors, or queue/index invariants

- Leave a code comment that points to the proof path when the proof backs a dirty fast path.
- Prefer existing proof-pack homes in `crates/core/z3/proofs`, `crates/gpu/z3/proofs`, or `runtime/native/src/core/z3/proofs`.

## Handoff Boundaries

- Use `lang-semantics` when stdlib work is really semantic fusion (`world`, `entangle`, `patch`, `law`, `converge`, `orchestrate`, `axiom`, `pulse`, `teleport`, `shatter`).
- Use `lang-systems` when stdlib work is really ownership, raw memory, pointer math, atomics, or low-level throughput.
- Use `lang-gpu` for authored Kain work centered on `std::gpu`, `std::graphics`, or `std::graphics::shared`.
- Use `lang-interop` for `std::python`, `std::platform`, host bridges, DLL seams, and native package surfaces.
- Use `lang-projects` when the stdlib change must be surfaced through `build.kn`, blades, or evidence DAG authoring.
- Use `bootstrap-core` when stdlib loading, import resolution, parser/typechecker truth, or lowering behavior is wrong.
- Use `bootstrap-fs`, `bootstrap-actors`, `bootstrap-gpu`, or `bootstrap-ownership` when compiler/frontend ownership changes for those domains.
- Use `runtime-stdlib` when native bridge behavior behind a root stdlib API is wrong or missing.
- Use `runtime-core` for init/shutdown, actor substrate, ownership, heap, service tables, timers, or machine substrate failures.
- Use `runtime-gpu` when GPU executor/graphics runtime behavior is wrong below authored stdlib surfaces.
- Use `test-bench`, `test-attrition`, and `test-harness` when the task is primarily evidence-lane work.
- Use `formal-verification` or `tool-z3-black-magic` when solver-driven proof is the center of gravity.

## Anti-Patterns

- Do not paste the full generated atlas into context to find one symbol.
- Do not author examples against private `abi_*` helpers.
- Do not invent a parallel `std::native.*` tree for user-facing work.
- Do not count overlay-only work under non-root helper trees as root completion by itself.
- Do not ship new root APIs as raw string blobs when a typed public record or enum is clearly warranted.
- Do not stop at a blade-local proof when the capability belongs in root stdlib.
- Do not skip `smoketest/` meshing or atlas regeneration after changing top-level `stdlib/*.kn`.
- Do not claim completion because a symbol exists somewhere. Claim completion only when public surface, smoke wiring, and required evidence are all real.
