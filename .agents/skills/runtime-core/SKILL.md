---
name: runtime-core
description: >-
  Use when adding, changing, debugging, validating, optimizing, or reviewing Kain's native runtime core substrate: `runtime/native/src/core`, `runtime/native/include`, `runtime/native_core_runtime.toml`, runtime service tables, ABI/startup/shutdown contracts, native actor/async/ownership/memory/entangle/machine-stones/realtime/reflection/compatibility/host-bridge behavior, native-core conformance, native-core Z3 proof packs, or proof-backed C hot-path performance work. Pair with `formal-verification` or `tool-z3-black-magic` for solver-heavy work, with `test-bench` or `test-attrition` for performance/teardown certification, and with `runtime-stdlib`, `runtime-gpu`, or `bootstrap-*` when the work crosses those ownership boundaries. Not for authored Kain application code, package-local bridges, generic Bazel plumbing, GPU executor internals, or parser/typechecker/lowering truth.
---

# Runtime Core

This is the native C runtime field manual. Use it when Kain's compiled/native lane touches the ABI floor, startup and shutdown, service discovery, actor turns, async tasks, ownership guards, helper allocation, machine-stones semantics, reflection, compatibility, host bridges, or proof-backed runtime hot paths.

## Prime Directive

- Treat `runtime/native` as Kain's ABI floor, not as a dumping ground for app policy, platform lore, or package experiments.
- Keep generic runtime semantics data-driven through manifests, service descriptors, ABI headers, runtime contracts, reflection payloads, and diagnostics.
- Preserve the layer split: `crates/core` owns language meaning, `crates/sys-codegen` owns lowering, and `runtime/native` owns the concrete C substrate that emitted code calls.
- If a runtime invariant is arithmetic, layout, capacity, pointer, state-machine, or ABI compatibility shaped, prove it with Z3 or explain exactly why it cannot be modeled yet.
- If performance is the reason for the change, produce both proof and a measured lane: Z3 for the invariant/equivalence, benchmark or conformance telemetry for the win.
- If a native runtime change becomes public Kain surface, update the matching root `stdlib/*.kn`, native `stdlib/native/*.kn` if present, `crates/core/src/stdlib.rs` declarations where needed, and regenerate `stdlib/STDLIB_MAP.llm.md`.

## Fast Operator Loop

```powershell
rg -n "runtime/native|native_core_runtime|native_runtime|actor ABI|ownership|proof|Z3|conformance|stdlib map" ARCHITECTURE.md MEMORY.md
rg -n "kain_actor_|kain_task_|__kain_ownership|__kain_alloc|kain_machine_|kain_service_|kain_reflection_|kain_bundle_|abi_" runtime/native/include runtime/native/src/core
rg -n "native-(memory|ownership|machine|services|entangle|process|net|stdlib)|actor-" runtime/native/src/core/z3
kain runtime build
kain runtime validate --skip-cli-build
```

When the task is broad or unfamiliar, read only the reference that matches the work:

- [references/native-c-runtime-architecture.md](references/native-c-runtime-architecture.md): full C runtime map, file responsibilities, manifests, service table, ABI flow, platform boundaries.
- [references/proof-and-validation.md](references/proof-and-validation.md): Z3 pack workflow, `proofs-experimental`, MCP commands, claim shapes, validation ladders.
- [references/performance-hunting.md](references/performance-hunting.md): solver-backed optimization loop, current hot surfaces, benchmark/attrition expectations, unsafe fast-path rules.

## Owns

- Generic native runtime substrate under `runtime/native/src/core/**` and shared ABI headers under `runtime/native/include/**`.
- The lean native runtime manifest path: `runtime/native_core_runtime.toml`, compatibility mirror `runtime/native_runtime.toml`, generated Bazel mirror data, and service metadata when runtime-core source membership or service availability changes.
- Startup/shutdown, service registry, diagnostics, runtime contract loading, reflection payloads, compatibility/hot reload substrate, host bridge registry, memory helpers, ownership guards, actors, async, entangle, machine stones, CPU/converge helpers, wire/bitfield/union primitives, and runtime attrition hooks.
- Native-core proof surfaces: `runtime/native/src/core/z3/**`, especially durable `proofs/*.yaml`, exploratory `proofs-experimental/*.smt2`, local templates, reports, and assumption/cache artifacts.
- Native-core proof inventory truth under `runtime/native/src/core/z3/generated/coverage/proof_inventory.sqlite`; prefer the DB-backed symbol/proof/candidate inventory over stale line-number catalogs when triaging coverage or remaining proof work.
- Native-core conformance and fixture evidence when the touched runtime behavior affects generated LLVM/direct-C programs.

## Does Not Own

- Authored Kain behavior, examples, blades, or app-level systems design. Use `lang-*` skills.
- Parser, AST, typechecker, runtime contract emission, interpreter meaning, or generic LLVM lowering. Use `bootstrap-core`, `bootstrap-actors`, `bootstrap-ownership`, or adjacent bootstrap skills.
- Domain stdlib contract crates and native stdlib domain bridges such as fs/input/net/process/ui when the work is primarily public stdlib behavior. Use `runtime-stdlib`.
- GPU executor, shader-bundle consumption, graphics backend execution, or Vulkain package bridges. Use `runtime-gpu` or `package-vulkain`.
- Bazel/rules/generated BUILD drift not coupled to runtime semantics. Use `tool-build-system`.
- Package-local native libraries, platform package lock/import behavior, or authored C ABI usage. Use `lang-c-abi` or the relevant package skill.

## Core Runtime Flow

```text
.kn source
-> crates/core parses/types/emits runtime contracts
-> crates/sys-codegen lowers LLVM/direct-C calls and ABI layouts
-> runtime/native_core_runtime.toml selects the lean native C source set
-> runtime/native/include declares stable C ABI shapes
-> runtime/native/src/core implements Kain-owned runtime substrate
-> generated executable calls native services through the ABI floor
-> conformance/fixtures/benchmark/attrition/Z3 prove behavior, speed, and teardown
```

Native runtime code should make this flow easier to inspect. If future agents cannot answer "which header declares this, which source implements it, which manifest links it, which stdlib wrapper exposes it, which proof protects it, and which conformance lane executes it", the change is under-documented.

## First Files

- `runtime/native_core_runtime.toml`: canonical lean runtime manifest. Keep vendor-free and service-descriptor driven.
- `runtime/native_runtime.toml`: compatibility mirror. Keep synced with the canonical lean manifest.
- `runtime/BUILD.bazel`, `runtime/runtime_manifest_data.bzl`, `tools/bazel/sync_native_runtime_builds.py`: Bazel mirror of the manifest truth. Co-trigger `tool-build-system` for build-rule edits.
- `runtime/native/include/base.h`: portability shim and shared C compatibility helpers.
- `runtime/native/include/services.h` plus `runtime/native/src/core/services.c`: canonical service registry, aliases, descriptors, required-service validation, capability discovery.
- `runtime/native/include/contract.h` plus `runtime/native/src/core/contract.c`: runtime contract sidecars, strict mode, service masks, startup validation.
- `runtime/native/include/actor.h` plus `runtime/native/src/core/actor.c`: actor ABI v3, mailbox, scheduler, reply ports, registry, supervision, monitor/link, telemetry.
- `runtime/native/include/async.h` plus `runtime/native/src/core/async.c`: task/future/timer runtime.
- `runtime/native/include/memory.h` plus `runtime/native/src/core/memory.c`: `__kain_*` low-level memory helpers and helper allocation cache.
- `runtime/native/include/ownership.h` plus `runtime/native/src/core/ownership.c`: native guards for `collapse`, `observe`, `decay`, helper-owned heap regions, and imported pointers.
- `runtime/native/include/machine_stones.h` plus `runtime/native/src/core/machine_stones.c`: native `axiom`, `pulse`, `teleport`, and `shatter` substrate.
- `runtime/native/src/core/z3`: native-core proof pack. This is not optional wallpaper; it is the solver-backed memory of the runtime.

## Working Rules

- Keep platform-specific mechanics behind provider lanes, platform adapters, manifests, and service status. Do not bake Win32, Linux, macOS, Vulkan, D3D12, or demo assumptions into generic `core` semantics.
- Do not add hardcoded pipelines in core when a runtime contract, manifest, service descriptor, reflection payload, or sidecar can carry the data.
- Make ABI additions in lockstep: header declaration, source implementation, manifest service/source membership if relevant, codegen declarations if generated LLVM calls it, stdlib wrappers if public Kain calls it, proof/conformance coverage for the dangerous part.
- Prefer existing service-table, diagnostics, attrition, reflection, and ABI helper surfaces over side channels.
- Keep native wrappers thin when the portable truth belongs in Rust crates such as `kain-actor`, `kain-ownership`, `kain-fs`, `kain-input`, `kain-net`, or `kain-process`.
- When C runtime code needs a fast path, guard it with a simple scalar fallback, exact capability check, and proof of equivalence or bounds.
- If a proof comment references a proof file, verify the file exists or repair the breadcrumb as part of the change.
- Generated outputs, reports, `.kain`, `target`, and runtime sidecars are disposable unless intentionally archived and documented.

## Proof Standard

Use Z3 for:

- size addition/multiplication and allocation header accounting
- pointer offset and signed/unsigned cast boundaries
- table, registry, queue, mailbox, ring, and handle capacity invariants
- actor state transitions, reply-port generation, scheduler accounting, and supervision restart windows
- ownership state lattice transitions and observer/collapse/decay exclusivity
- service key canonicalization, token/magic collision freedom, branchless selectors, de Bruijn tables, packed layouts, and SIMD/converge equivalence

Target result: `unsat` for violation queries. `sat` is a counterexample, not a failure of the workflow. Save durable claims in `runtime/native/src/core/z3/proofs/*.yaml`; save exploratory solver hunts in `runtime/native/src/core/z3/proofs-experimental/*.smt2` until the candidate is worth promoting.

For runtime-core coverage inventory, treat `runtime/native/src/core/z3/generated/coverage/proof_inventory.sqlite` as the pack-local source of truth. The coverage lane in `z3-mcp` now syncs stable `symbol_id` rows for current symbols, durable proofs, auto-results, ready targets, and generated proof candidates so we do not have to rebuild ad hoc line-based catalogs to answer "what remains?".

Use the one-command candidate automation loop when shrinking coverage backlog:

```powershell
uv run --directory X:\mcp\polytools\z3-mcp python -m z3_mcp.workflow_tools --pack-path runtime\native\src\core\z3 --automate-candidates --max-symbols 50 --max-cases 10 --timeout-ms 10000 --report-name runtime-core-candidate-automation
```

That pass materializes missing generated candidates, runs candidate proofs, promotes clean outcomes into durable `z3/proofs/coverage/**`, quarantines counterexamples under `z3/generated/coverage/runs/<run-id>/`, and resyncs the SQLite inventory.

Useful MCP calls:

- `mcp__z3_local__.analyze_source_file(path="runtime/native/src/core/actor.c", symbol="...")`
- `mcp__z3_local__.suggest_proof_targets(path="runtime/native/src/core/memory.c")`
- `mcp__z3_local__.extract_source_proof_cases(path="...", save=true)`
- `mcp__z3_local__.run_proof_pack(path="runtime/native/src/core/z3", lane="memory")`
- `mcp__z3_local__.check_smt2(smt2="...", include_model=true)`
- `mcp__z3_local__.bitvec_equiv(...)`, `range_check(...)`, `size_add_ok(...)`, `size_mul_ok(...)`, `ptr_offset_ok(...)`, `buffer_growth_ok(...)`, `state_machine_check(...)`

For solver-discovered tables, constants, branchless rewrites, or weird hot-path replacements, pair this skill with `tool-z3-black-magic` and land both proof and benchmark evidence.

## Implementation Playbooks

Actor runtime:

- Read `runtime/native/include/actor.h`, `runtime/native/include/ACTOR_RUNTIME_OWNERSHIP.md`, `runtime/native/src/core/actor.c`, `crates/actor/src/native.rs`, and the LLVM actor lowering seam if ABI layout changes.
- Prove queue capacity, generation, reply-port stale rejection, mailbox underflow, scheduler accounting, and restart-window math.
- Validate with `runtime/conformance/actor_runtime/run_tests.sh --verbose`, Bazel `//runtime:native_runtime_tests`, and a Kain actor fixture/benchmark when generated code behavior changes.

Memory and ownership:

- Read `memory.h`, `memory.c`, `ownership.h`, `ownership.c`, and compiler low-level memory lowering before changing helper allocation or pointer provenance.
- Prove header+payload arithmetic, cache bounds, pointer offset rebuild, registry capacity, observer count, decay/free eligibility, and imported-pointer registration.
- Validate with `runtime/native/tests/test_ownership_memory.c`, `runtime/fixtures/validate_all.sh`, and the ownership Z3 lane.

Services, contract, and reflection:

- Keep service keys stable and aliases data-driven. Add new service descriptors in the manifest and registry, not scattered string checks.
- Prove bounded text copies and descriptor capacity. Validate startup and ABI conformance when required-service logic changes.
- If a new service becomes public Kain surface, update the stdlib map and root `stdlib` wrappers.

Machine stones, CPU, converge, SIMD:

- Keep capability checks explicit and scalar fallback behavior honest.
- Prove shatter lane offsets, teleport exclusivity, pulse missed-beat bounds, selector equivalence, and SIMD lane equivalence before trusting target attributes or intrinsics.
- Use benchmark rows for performance claims; use `converge_mismatch_count()` or native telemetry for behavioral drift.

Stdlib-facing native helpers:

- Co-trigger `runtime-stdlib` when the behavior is a domain API rather than generic runtime substrate.
- Update `stdlib/*.kn`, `stdlib/native/*.kn` if relevant, `crates/core/src/stdlib.rs` native declarations, canonical examples, and regenerate `stdlib/STDLIB_MAP.llm.md` when public surface changes.

## Validation Ladders

Fast native loop:

```powershell
kain runtime build
kain runtime validate --skip-cli-build
```

Aggregate runtime proof:

```powershell
./runtime/fixtures/validate_all.sh
./runtime/conformance/run_all.sh
./runtime/validate_native_runtime.sh
```

Windows wrappers:

```powershell
powershell -ExecutionPolicy Bypass -File runtime\fixtures\validate_all.ps1
powershell -ExecutionPolicy Bypass -File runtime\conformance\run_all.ps1
powershell -ExecutionPolicy Bypass -File runtime\validate_native_runtime.ps1
```

Bazel/runtime manifest:

```powershell
py -3 tools/bazel/sync_native_runtime_builds.py --check
bazel build //runtime:all
bazel test //runtime:native_runtime_tests
```

Z3 pack:

```powershell
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path runtime\native\src\core --lane smoke
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path runtime\native\src\core --lane actor
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path runtime\native\src\core --lane memory
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path runtime\native\src\core --lane ownership
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path runtime\native\src\core --lane full
```

Pick the smallest lane that proves the touched surface, then climb if the change affects shared ABI or manifests.

## Anti-Patterns

- Do not add platform semantics to generic runtime core just because one OS is easiest to test today.
- Do not grow public `std.native.*` authoring lanes when root `std.*` wrappers are the canonical surface.
- Do not add direct app/demo logic, vendor-specific pipelines, or package-owned policy into `runtime/native`.
- Do not land unsafe pointer math with only a unit test.
- Do not call a runtime fast path correct because it passed one benchmark input.
- Do not change ABI structs or constants without checking generated LLVM layouts, conformance, and compatibility descriptors.
- Do not leave runtime helpers without diagnostics when a caller can fail.
- Do not let service keys become string-spread across C, Rust, Kain, scripts, and manifests. Centralize or generate.
- Do not silence benchmark regressions by simplifying Kain semantics. If the weird semantic lane is slow, harden runtime/compiler truth until it wins.

## Closeout Contract

When finishing runtime-core work, report:

- files changed and which runtime layer they affect
- ABI/service/stdlib/codegen surfaces touched
- Z3 claims saved or rerun, with lane/result
- conformance, fixture, Bazel, benchmark, or attrition commands run
- any remaining platform-specific limitation or proof gap
