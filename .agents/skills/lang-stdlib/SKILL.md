---
name: lang-stdlib
description: >-
  Use when authoring, explaining, reviewing, repairing, extending, benchmarking, or certifying Kain root `std::*` work: choosing public stdlib imports, querying exact symbols, adding or expanding top-level `stdlib/*.kn` modules, closing capability gaps tracked in `stdlib/requirements.md`, meshing new surfaces into `smoketest/`, and routing stdlib-backed compiler/runtime issues to the owning bootstrap/runtime lanes. Use for both consuming the existing root stdlib and building it toward v1 completeness; do not use this as the primary skill for overlay-only work under `stdlib/python`, `stdlib/javascript`, `stdlib/ue5`, `stdlib/c`, or `stdlib/interop` unless that work is explicitly in service of a root stdlib surface.
---

# Lang Stdlib

This skill is the root Kain stdlib operator manual. Use it to write authored Kain against the public root `std::*` surface and to grow that surface without losing the delivery contract around requirements, smoketest, atlas freshness, and evidence.

## Prime Directive

- Prefer public root imports such as `use std::fs`, `use std::math`, `use std::http`, and `use std::ui`.
- Read `stdlib/requirements.md` immediately when the task involves adding, extending, or certifying root stdlib capability.
- Query the atlas before spelunking. Use `query_stdlib.py` and only open exact source when needed.
- Treat private `abi_*` symbols as stdlib-internal runtime wiring, not authoring APIs.
- Treat `@extern` root stdlib functions as runtime-backed ABI surfaces. Escalate to the owning runtime/bootstrap lane if behavior is wrong.
- Regenerate `stdlib/STDLIB_MAP.llm.md` and `stdlib/stdlib.map.json` whenever top-level `stdlib/*.kn` changes.
- Mesh real stdlib changes into `smoketest/`, not just a blade-local proof.
- Build Kain-shaped surfaces, not donor-file cosplay. Translate Zig/Rust/Go capabilities into Kain-native modules and semantics.

## Fast Operator Loop

Use these commands first:

```powershell
python query_stdlib.py --summary
python query_stdlib.py --imports
python query_stdlib.py --module fs --contains path --limit 40
python query_stdlib.py --module math --contains vec3 --limit 40
python query_stdlib.py --search json --limit 40
python query_stdlib.py --search base64 --limit 40
python query_stdlib.py --search thread --limit 40
rg -n "^use std::" library_of_kain blades benchmark smoketest
rg -n "\b(fs_|tcp_|process_|graphics_|ui_|hash_|text_|json_)\b" stdlib blades benchmark smoketest runtime
rg -n "TODO|PARTIAL|BLOCKED|DONE|WAIVED|json|base64|unicode|ArrayList|HashMap|Uri|atomic|thread|compress|tar|zip|elf|dwarf|pdb" stdlib/requirements.md
rg -n "json|base64|unicode|ArrayList|HashMap|Uri|atomic|Thread|compress|tar|zip|elf|dwarf|pdb" reference\langs\zig\lib\std
kain check <entry.kn> --target llvm
kain run <entry.kn-or-blade> --target llvm
```

Use these commands for root stdlib maintenance:

```powershell
cargo run -q -p kain-stdlib-map --bin kain_stdlib_map_tool -- --write
cargo run -q -p kain-stdlib-map --bin kain_stdlib_map_tool -- --check
```

## Work Modes

Use this skill in three different modes:

1. Consume the existing root stdlib.
   - Choose the right import and exact symbol.
   - Write a compact authored Kain proof against public APIs.
   - Route failures to the correct bootstrap/runtime owner.
2. Extend an existing root module.
   - Tighten a thin surface such as `std::fs`, `std::time`, `std::crypto`, or `std::process`.
   - Replace stringly or `Any`-shaped gaps with typed public records or builders.
   - Mesh the new surface into `smoketest/`, blades, and evidence.
3. Add a missing root family.
   - Create a new top-level `stdlib/*.kn` module when the family is broad and author-facing, such as `std::json`, `std::fmt`, `std::io`, or `std::random`.
   - Add the family to the root completion contract in `stdlib/requirements.md`.
   - Prove it through the same pipeline as any other root stdlib surface.

## Root Completion Contract

Treat `stdlib/requirements.md` as the authoritative backlog and delivery contract for finishing the root `std::*` surface.

Use it to:

- find the relevant `P0`, `P1`, `P2`, or `KX` row before implementing
- decide whether a task is `TODO`, `PARTIAL`, or `BLOCKED`
- avoid duplicate or blade-only solutions that do not close a root gap
- decide whether a capability belongs in an existing module or a new root family
- update status to `DONE` during the same landing change when the capability is fully completed

If a new missing family is discovered while comparing against donor stdlibs or against real authored Kain pressure, add it to `stdlib/requirements.md` instead of keeping the knowledge in your head or in a transient conversation.

## Completion Pipeline

Follow this pipeline when doing real root stdlib work:

1. Read the relevant row in `stdlib/requirements.md`.
2. Query the live atlas to confirm what the current public root surface already exposes.
3. Search `blades/`, `benchmark/`, `attrition/`, `smoketest/`, and `library_of_kain/` for nearby authoring patterns.
4. Decide whether the capability belongs in an existing root module or a new top-level `stdlib/*.kn` file.
5. Implement the public Kain-facing API first. Keep `abi_*` helpers private unless the module is explicitly stdlib-maintenance-only.
6. Land the owning backing work in `crates/*`, `runtime/native`, or other subsystem owners if the surface is runtime-backed.
7. Mesh the new surface into `smoketest/`, and make at least one non-stdlib consumer call it when practical.
8. Add deeper evidence in `benchmark/`, `attrition/`, `runtime/conformance/`, and Z3 proof packs when the claim requires it.
9. Regenerate and check the stdlib atlas.
10. Update `stdlib/requirements.md` row status and `MEMORY.md` when operator truth changed.

## Compiler To Runtime Flow

Root stdlib work normally flows like this:

```text
authored Kain
-> public root import such as std::fs or std::json
-> top-level stdlib/*.kn module
-> private abi_* declarations and pure Kain wrappers/helpers
-> stdlib loader and import truth in crates/kain-core
-> portable Rust contract crates when present
-> runtime/native C ABI substrate and service tables
-> smoketest, blades, benchmark, attrition, runtime conformance, and Z3 evidence
```

Primary source anchors:

- Root completion backlog: `stdlib/requirements.md`
- Live root atlas: `stdlib/STDLIB_MAP.llm.md`, `stdlib/stdlib.map.json`
- Atlas query helper: `query_stdlib.py`
- Atlas generator: `crates/kain-stdlib-map`
- Root stdlib source: `stdlib/*.kn`
- Stdlib loading and target profile ordering: `crates/kain-core/src/stdlib.rs`
- Parser/typechecker import consumers: `crates/kain-core/src`
- Portable owners: `crates/kain-fs`, `crates/kain-input`, `crates/kain-net`, `crates/kain-process`, `crates/kain-actor`
- Native runtime owners: `runtime/native/include`, `runtime/native/src/core`, `runtime/native/src/ui`, `runtime/native/src/graphics`
- Smoketest album: `smoketest/src/main.kn`, `smoketest/build.kn`, `smoketest/src/stdlib`
- Proof blades: `blades/stdlib-foundations`, `blades/stdlib-domains`, `blades/network-domains`, `blades/hash-domains`, `blades/math-domains`
- Donor baseline: `reference/langs/zig/lib/std`

## Live Root Profile

The current native root profile reports:

- `modules=35`
- `public_symbols=1655`
- `total_symbols=2172`
- `rust_builtins=233`
- `native_services=42`

Current root imports:

```kn
use std::actor
use std::alloc
use std::attrition
use std::bench
use std::bits
use std::build
use std::certify
use std::collections
use std::crypto
use std::diagnostics
use std::fs
use std::gen_server
use std::gpu
use std::graphics
use std::graphics::shared
use std::hash
use std::http
use std::http2
use std::input
use std::intent
use std::machine
use std::math
use std::memory
use std::net
use std::platform
use std::process
use std::proof
use std::reload
use std::result
use std::runtime
use std::test
use std::text
use std::time
use std::tls
use std::ui
```

Remember that a large symbol count does not mean the root stdlib is done. `stdlib/requirements.md` is the completion truth, not the atlas count.

## Choose By Need

Use this family map before opening large files:

| Need | Use | First Symbols / Patterns | Co-trigger |
| --- | --- | --- | --- |
| Start or stop native services, heap checks, machine counters | `std::runtime` | `runtime_init`, `runtime_shutdown`, `runtime_heap_validate`, `runtime_converge_*`, `runtime_machine_*` | `runtime-core` |
| Observe semantic runtime behavior | `std::intent` | `entangle_*`, `patch_journal_count`, `law_status`, `converge_mismatch_count`, `orchestrate_stage_count` | `lang-semantics` |
| Actors, registry, supervision, scheduler telemetry | `std::actor` | `actor_spawn`, `actor_send`, `actor_monitor`, `actor_scheduler_queue_depth` | `lang-actors`, `runtime-core` |
| Tiny OTP-shaped services | `std::gen_server` | `gen_server_start`, `gen_server_call`, `gen_server_cast` | `lang-actors` |
| Build, proof, bench, attrition, certify DAG work | `std::build`, `std::proof`, `std::bench`, `std::attrition`, `std::certify`, `std::test` | `build_graph`, `proof_obligation`, `bench_case`, `attrition_case`, `certify_gate`, `test_bool` | `lang-projects`, `test-harness`, `test-bench`, `test-attrition` |
| Typed maps, queues, slot maps, clamps | `std::collections` | `typed_map_*`, `queue_*`, `priority_queue_*`, `slot_map_*`, `int_clamp` | `lang-systems` |
| Arena, bump, and pool allocation | `std::alloc` | `arena_create`, `arena_alloc`, `bump_alloc`, `pool_alloc` | `lang-systems` |
| String views and zero-copy text slicing | `std::text` | `text_slice`, `text_trim`, `text_find`, `text_materialize`, `string_view_*` | `lang-systems` |
| Digests and random bytes | `std::crypto` | `sha256`, `hmac_sha256`, `blake3`, `random_bytes_hex` | `runtime-stdlib`, `lang-interop` |
| Status/result composition | `std::diagnostics`, `std::result` | `status_ok`, `bool_to_status`, `first_error`, `result_ok`, `result_is_error` | `test-harness` |
| Deterministic bit twiddling and packed integer math | `std::bits` | `u32`, `wrapping_add_u32`, `rotl32`, `popcount32`, `bswap32` | `lang-systems`, `tool-z3-black-magic` |
| Low-level memory fences and int atomics | `std::memory` | `volatile_*`, `atomic_load_*`, `atomic_store_*`, `atomic_compare_exchange_seqcst`, `atomic_fence_*` | `lang-systems`, `runtime-core` |
| CPU facts, prefetch, `cpuid`, thread affinity, VM pages | `std::machine` | `pause`, `rdtsc`, `cpuid_*`, `current_thread_id`, `set_current_thread_affinity`, `vm_*` | `lang-systems`, `runtime-core` |
| Files, temp, metadata, path-ish helpers | `std::fs` | `fs_read_text`, `fs_write_text`, `fs_temp_file`, `fs_hash_file`, `fs_path_join` | `runtime-stdlib`, `bootstrap-fs` |
| Child processes and PTYs | `std::process` | `process_spec_create`, `process_spawn`, `process_wait`, `process_stdout_capture_text` | `runtime-stdlib` |
| Platform identity and dynamic libraries | `std::platform` | `platform_current_name`, `platform_library_open`, `platform_library_resolve` | `lang-interop`, `runtime-stdlib` |
| TCP and network server/client state | `std::net` | `net_reset`, `tcp_connect`, `tcp_listen`, `tcp_read_text`, `tcp_write_text` | `runtime-stdlib` |
| HTTP request/response/server work | `std::http` | `request_create`, `server_create_localhost`, `respond_text`, `route_actor` | `lang-interop`, `runtime-stdlib` |
| HTTPS and HTTP/2 wrappers | `std::tls`, `std::http2` | `tls_https_request_create`, `tls_https_get_text`, `http2_request_create`, `http2_get_text` | `runtime-stdlib` |
| Input sessions and action maps | `std::input` | `input_session_create`, `input_bind_action`, `input_begin_frame`, `input_trace_json` | `lang-ui`, `runtime-stdlib` |
| Engine math and layout | `std::math` | `vec3_*`, `quat_*`, `mat4_*`, `ray_vs_aabb`, `fbm2`, `std140_*` | `lang-gpu`, `lang-ui`, `lang-systems` |
| GPU resource contracts and shared buffers/images | `std::gpu` | `gpu_resource_policy`, `gpu_shared_buffer_zeroed`, descriptor constants | `lang-gpu`, `runtime-gpu` |
| Native graphics sessions and draw calls | `std::graphics`, `std::graphics::shared` | `graphics_session_create`, `graphics_shader_spirv_from_hex`, `graphics_draw_mesh`, `graphics_shared_vertex_buffer` | `lang-gpu`, `runtime-gpu` |
| Native UI sessions, nodes, styles, events, text, dialogs | `std::ui` | `ui_session_create`, `ui_node_create`, `ui_node_set_rect`, `ui_push_event`, `ui_draw_command_count` | `lang-ui`, `package-kaintana` |
| Hot-reload generations and migration hints | `std::reload` | `reload_begin`, `reload_commit`, `reload_generation`, `reload_snapshot`, `reload_lane_*` | `lang-ui`, `package-kaintana` |

For exact signatures and full symbol lists, query the atlas instead of reading it all into context.

## Root Gap Index

Use `stdlib/requirements.md` for the authoritative rows, but remember the missing family clusters:

- Text/data basics: `json`, `fmt`, `base64`, `ascii`, `unicode`, `uri`, `semver`
- Container/algo depth: generic vectors/maps/sets, typed `SlotMap<T>`, bitsets, sort/search, richer allocator integration
- Filesystem/path/I/O depth: typed metadata, typed directory entries, path toolkit, file handles, stream abstractions
- Time/random/sync: richer time model, real RNG family, atomic/sync/thread modules
- Systems and OS surface: `os`, `posix`, better target/meta/reflect coverage
- Network/process/crypto depth: stronger typed config, streaming, bytes-based APIs, richer status objects
- Archives and binary tooling: `compress`, `tar`, `zip`, `elf`, `dwarf`, `macho`, `coff`, `pdb`, `wasm`
- Kain-only leverage: typed `std::intent`, stronger `std::actor`, better `std::gen_server`, `std::runtime`, `std::reload`, `std::proof`, and `std::test`

Note the current nuance:

- `std::memory` already exposes a thin int-oriented atomic/fence surface, but that does not by itself close the broader `std::atomic` gap tracked in `stdlib/requirements.md`.
- `std::machine` already exposes `cpuid`, affinity, and VM-page helpers, but that does not by itself close the broader `std::thread`, `std::os`, or `std::target` gaps.

## Smoketest Mesh Contract

Treat `smoketest/` as the default mixed-surface proof lane for root stdlib work.

When the public root stdlib changes:

- add or extend `smoketest/src/stdlib/*_lane.kn`
- import the lane in `smoketest/src/main.kn`
- call the lane from the album flow in `smoketest/src/main.kn`
- update `total_tracks` in `smoketest/src/main.kn` when adding a new track; current value is `36`
- add the new source file to the relevant input lists in `smoketest/build.kn`
- reuse existing stdlib lanes as style anchors:
  - `smoketest/src/stdlib/alloc_lane.kn`
  - `smoketest/src/stdlib/collections_lane.kn`
  - `smoketest/src/stdlib/crypto_lane.kn`
  - `smoketest/src/stdlib/diagnostics_lane.kn`
  - `smoketest/src/stdlib/fs_lane.kn`
  - `smoketest/src/stdlib/math_lane.kn`
  - `smoketest/src/stdlib/platform_lane.kn`
  - `smoketest/src/stdlib/text_lane.kn`
  - `smoketest/src/stdlib/time_lane.kn`

Do not stop at an isolated stdlib lane if the new surface is broadly useful. Make another track, blade, or package consume it when practical so the album proves the composition story.

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

4. Benchmark when the claim is performance-sensitive

```powershell
rg -n "stdlib|json|fs|hash|process|net|alloc|text" benchmark/benchmarks.json benchmark/cases
python benchmark/run.py --case <case> --languages kain --runs 5 --warmups 1 --timeout 240
```

5. Attrition when the surface owns resources or teardown-sensitive state

```powershell
rg -n "stdlib|json|fs|process|net|ui|graphics|reload" attrition/attritions.json attrition/cases
python attrition/run.py --case <case> --scale small --timeout 120
```

6. Runtime conformance when the surface is ABI-backed

```powershell
Get-ChildItem -Name runtime\conformance
```

7. Z3 when the surface relies on unsafe math, pointer/index bounds, packed layouts, branchless selectors, or queue/index invariants

- Leave a code comment that points to the proof path when the proof backs a dirty fast path.
- Prefer existing proof-pack homes in `crates/kain-core/z3/proofs`, `crates/gpu/z3/proofs`, or `runtime/native/src/core/z3/proofs`.

## Donor Comparison Discipline

Use donor stdlibs, especially Zig, as capability inventories rather than as folder templates.

Use `reference/langs/zig/lib/std` to ask:

- what family exists there that root Kain stdlib still lacks?
- what typed surface should Kain expose for authors?
- can Kain collapse multiple donor modules into one better Kain-native surface?
- does the missing donor family actually belong in root stdlib, or in a different repo subsystem?

Do not do this:

- mirror Zig's file graph one-for-one
- create fake parity by hiding a capability in a blade or overlay tree
- ignore Kain-only leverage just because donor languages do not have it

Use donors for pressure from categories such as:

- text, json, fmt, unicode, base64
- containers, algorithms, allocators
- io, files, paths, streams
- random, hash, crypto
- os, posix, threads, atomics, memory, target metadata
- http, tls, process, platform
- archives, compression, binary and debug formats

## Handoff Boundaries

- Use `lang-semantics` when root stdlib work is really about authored semantic fusion: `world`, `entangle`, `patch`, `law`, `converge`, `orchestrate`, `axiom`, `pulse`, `teleport`, `shatter`, actors, components, or shaders.
- Use `lang-systems` when root stdlib work is really about ownership, raw memory, pointer math, branchless lanes, atomics, or low-level throughput patterns.
- Use `lang-gpu` for authored Kain work centered on `std::gpu`, `std::graphics`, or `std::graphics::shared`.
- Use `lang-ui` for authored UI flows centered on `std::ui`; use `package-kaintana` for Kaintana package work.
- Use `lang-interop` for `std::platform`, host bridges, DLLs, native packages, and OS/vendor seams.
- Use `lang-projects` when the stdlib change must be surfaced through `build.kn`, blades, or the evidence DAG authoring experience.
- Use `bootstrap-core` when stdlib loading, import resolution, parser/typechecker truth, or lowering behavior is wrong.
- Use `bootstrap-fs`, `bootstrap-actors`, `bootstrap-gpu`, or `bootstrap-ownership` when the compiler/frontend ownership for those domains changes.
- Use `runtime-stdlib` when the native bridge behavior behind a root stdlib API is wrong or missing.
- Use `runtime-core` when runtime init/shutdown, actor substrate, ownership, heap, service tables, timers, or machine substrate are wrong.
- Use `runtime-gpu` when GPU executor or graphics runtime behavior is wrong below the authored stdlib surface.
- Use `test-bench`, `test-attrition`, and `test-harness` when the task is primarily evidence-lane work.
- Use `formal-verification` or `tool-z3-black-magic` when the interesting part is proving or solver-driving the implementation.

## Anti-Patterns

- Do not paste the full generated atlas into context to find one symbol.
- Do not author examples against private `abi_*` helpers.
- Do not invent a parallel `std::native.*` tree for user-facing work.
- Do not count overlay-only work under `stdlib/python`, `stdlib/javascript`, `stdlib/ue5`, `stdlib/c`, or `stdlib/interop` as root completion by itself.
- Do not ship new root APIs as raw string blobs when a typed public record or enum is clearly warranted.
- Do not stop at a blade-local proof when the capability belongs in the root stdlib contract.
- Do not skip `smoketest/` meshing, atlas regeneration, or requirement-row updates after changing top-level `stdlib/*.kn`.
- Do not claim completion because a symbol exists somewhere. Claim completion only when the public root surface, smoke wiring, and required evidence are all real.
