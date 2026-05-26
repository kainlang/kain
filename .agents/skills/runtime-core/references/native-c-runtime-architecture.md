# Native C Runtime Architecture

Read this when a runtime-core task needs the full map: what the C runtime is, what each file does, how manifests and service tables decide what gets linked, and where platform/package/stdlib boundaries sit.

## Big Shape

Kain's native runtime is a lean C ABI floor consumed by emitted LLVM/direct-C code. It is not the language meaning layer and it is not a package policy layer.

```text
Authored .kn
-> parser/typechecker/runtime-contract truth in crates/core
-> LLVM/direct-C lowering in crates/sys-codegen
-> runtime/native/include declares ABI structs/functions/constants
-> runtime/native/src/core implements Kain-owned native services
-> runtime/native_core_runtime.toml selects sources/services/link flags
-> generated executable links the runtime bundle
-> fixtures/conformance/Z3/benchmark/attrition certify the path
```

The runtime exists so Kain can own systems semantics with metal behind them: actors, async, ownership, raw memory, world/entangle, patch/law counters, converge CPU selection, shatter/pulse/teleport, reflection, services, diagnostics, process/net/input/UI/graphics substrate, and startup validation.

## Canonical Build Truth

- `runtime/native_core_runtime.toml` is the canonical production manifest. It must stay vendor-free and Kain-owned.
- `runtime/native_runtime.toml` is a compatibility mirror for older discovery paths. Keep it synced with the canonical manifest.
- `runtime/native_runtime_metadata.json` is tooling-facing reflection of the lean contract.
- `runtime/BUILD.bazel` and `runtime/runtime_manifest_data.bzl` mirror the manifest for Bazel. Regenerate with `py -3 tools/bazel/sync_native_runtime_builds.py`.
- `//runtime:native_runtime` aliases the lean core runtime target. `//runtime:native_full_runtime` is legacy-named compatibility over the same lean source set, not permission to revive a vendor lane.

Manifest sections matter:

- `sources`: generic C runtime files linked on every active platform.
- `windows_sources`, `linux_sources`, `macos_sources`: platform adapter files only.
- `include_dirs`: ABI include roots.
- `windows_defines`, `linux_defines`: platform compile contract.
- `[metadata]`: runtime lane identity and compatibility class.
- `[[services]]`: service descriptors consumed by startup validation, registry, and agent reasoning.
- `[link]`: native OS libraries.

## Service Table Model

The service table is the runtime's capability nervous system. Prefer it over scattered strings or ad hoc checks.

- `runtime/native/include/services.h` declares stable service keys, provider lanes, status, requirement, descriptor shapes, and registry APIs.
- `runtime/native/src/core/services.c` implements aliases, magic-prefix key metadata, descriptor copies, native service registration, availability checks, required-service validation, and formatting.
- `runtime/native/include/contract.h` and `runtime/native/src/core/contract.c` map runtime contract sidecars to required/optional services and startup validation.
- `runtime/native/SERVICE_TABLE_MAPPING.md` explains the migration from legacy masks to canonical service families.

Service provider lanes:

- `native-core`: generic Kain-owned C runtime substrate.
- `platform-win32`, `platform-linux`, `platform-macos`: OS adapter providers.
- `host-rust`, `host-python`, `host-node`, `external`: foreign/host/plugin providers when explicitly registered.

Rules:

- Add new capability as a descriptor, not a pile of string checks.
- Keep alias/canonicalization data in one place.
- Make required services produce structured diagnostics when absent.
- A degraded optional service is better than pretending a platform has semantics it does not have.
- Package adapters may satisfy service seams, but the public semantic contract stays Kain-owned.

## Header Map

Headers are the ABI. Changing them can affect LLVM layout, direct C output, conformance, and package bridges.

| Header | Role | High-risk edits |
| --- | --- | --- |
| `base.h` | Portability shim, Windows/POSIX compatibility helpers, shared C types. | Platform macro changes, string copy semantics, sleep/file/env wrappers. |
| `version.h` | Runtime/ABI version descriptors. | Compatibility checks, startup validation. |
| `diagnostics.h` | Diagnostic subsystem, codes, collectors, startup validation result. | Error code stability, message bounds. |
| `services.h` | Service registry ABI and service descriptors. | Service key/status/provider changes, descriptor layout. |
| `contract.h` | Runtime contract env/sidecar loading and service masks. | Required service mask, startup strictness. |
| `actor.h` | Actor ABI v3: ids, refs, spawn config, mailbox, reply ports, scheduler, supervision, registry. | Struct layout, generation semantics, message ownership, scheduler snapshots. |
| `async.h` | Task/future/timer ABI. | State machine, result ownership, timer/wake behavior. |
| `memory.h` | Low-level helper ABI: allocation headers, pointer helpers, loads/stores, alloc/realloc. | Header size, slot token, pointer arithmetic, payload ownership. |
| `ownership.h` | Native guard API for ownership regions. | State constants, error codes, helper fast paths. |
| `entangle.h` | Native entangle registry. | Endpoint text bounds and registration capacity. |
| `machine_stones.h` | Axiom/pulse/shatter/teleport ABI. | Capability bits, pulse timing, shatter layout, teleport token semantics. |
| `cpu.h`, `converge.h`, `simd.h` | CPU feature detection, converge selector/cache, SIMD helper ABI. | Capability gates, lane equivalence, scalar fallbacks. |
| `stdlib_abi.h` | Native stdlib facade used by root stdlib wrappers. | Public helper signatures, result/option/future handles, stdlib map drift. |
| `input_system.h`, `net_system.h`, `process_system.h` | Runtime-backed stdlib domains. | Usually `runtime-stdlib` ownership unless the change is generic service/ABI substrate. |
| `graphics_system.h`, `renderer_backend.h`, `renderer_session.h`, `scene.h` | Raw native graphics and scene substrate. | Use `runtime-gpu` when execution/backend behavior is primary. |
| `reflection.h` | Reflection payload and type/item metadata ABI. | Schema version, item/type kind, buffer sizes. |
| `compatibility.h` | Hot reload and bundle compatibility substrate. | Migration state machine, ABI/runtime validation. |
| `host_bridge.h` | Foreign/host service registry. | Module/service lifetime and lane mapping. |
| `realtime.h` | Realtime bundle sidecar loading and fixed-size summary data. | Buffer capacities, env/sidecar contract. |
| `attrition.h` | Runtime long-run certification counters/events. | Snapshot schema, event ring capacity, teardown accounting. |
| `bitfield.h`, `union.h`, `wire.h`, `json.h` | Small primitive ABI helpers. | Packed layout, parser bounds, encoded data contracts. |
| `platform.h`, `platform_library.h`, `win32.h` | Platform adapter and dynamic library substrate. | Keep generic loading typed and package-facing, not generic public ABI magic. |
| `ui_*` | Raw native UI and hot reload ABI. | Use `runtime-stdlib`, `runtime-gpu`, or `package-kaintana` depending on the change. |

## Core Source Map

These files are linked by the canonical lean runtime manifest unless noted.

| Source | What It Does | How It Affects Kain |
| --- | --- | --- |
| `core.c` | Base runtime primitives: retain/release style helpers, destructor table, simple spawn/sleep support, process-local runtime glue. | If it breaks, generated programs can fail before any higher semantic lane starts. |
| `version.c` | Runtime/ABI version descriptors. | Feeds compatibility and startup checks. |
| `diagnostics.c` | Creates/formats/collects diagnostics and startup validation results. | All runtime failures should surface through this instead of silent null/print-only paths. |
| `services.c` | Data-driven service registry, canonical keys, aliases, required validation, native service catalog. | Determines what a native program believes the runtime can provide. |
| `contract.c` | Loads runtime contract JSON/sidecars/env, validates service availability and strictness. | Connects compiler-emitted requirements to runtime startup truth. |
| `entangle.c` | Registers entanglement endpoint metadata and counters. | Runtime support for `world`/`entangle` telemetry and native registration. |
| `cpu.c` | CPU feature detection. | Feeds `converge`, SIMD, and machine capability gates. |
| `converge.c` | Native converge lane selector/telemetry. | Lets compiled fast lanes remain checked against runtime capability truth. |
| `simd.c` | Scalar and target-specialized SIMD helper kernels. | Hot benchmark substrate; every intrinsic lane needs scalar fallback and proof. |
| `wire.c` | Small wire/encoded data helpers. | Protects ABI encoding boundaries and packed transport assumptions. |
| `json.c` | Native JSON parse/render/object/array helpers. | Used by runtime contracts, sidecars, benchmarks, and stdlib-ish helpers. |
| `json_benchmark.c` | Focused JSON benchmark helper. | Benchmark support, not language semantics. |
| `ray_sphere_benchmark.c` | Small native benchmark kernel. | Performance proof surface for math/runtime overhead. |
| `machine_stones.c` | Native `axiom`, `pulse`, `shatter`, `teleport` services and telemetry. | Backs Kain-only machine semantics without moving meaning out of compiler truth. |
| `stdlib_abi.c` | Native facade for root stdlib wrappers: runtime init/shutdown, option/result/future handles, fs/status/intent and other helper surfaces. | Public stdlib changes often touch this plus `stdlib/*.kn` and `crates/core/src/stdlib.rs`. |
| `input_system.c` | Native input sessions/actions/axes/text/trace substrate. | Usually runtime-stdlib ownership; stays in manifest as native provider. |
| `net_system.c` | Native TCP/HTTP/TLS/HTTP2-ish substrate. | Runtime-backed stdlib domain; keep portable semantics aligned with `kain-net`. |
| `process_system.c` | Native process, argv/env/cwd, pipes, PTY, wait/capture substrate. | Runtime-backed stdlib domain; platform behavior must be honest. |
| `graphics_system.c` | Raw graphics sessions, buffers, shaders, pipelines, draw commands. | Runtime-gpu adjacent; keep authored engines in Kain/packages. |
| `renderer_backend.c`, `renderer_session.c` | Backend/session identity and handles. | Avoid turning these into vendor policy. |
| `scene.c` | Stable scene handles, mutation/query data, status names. | Generic scene substrate for authored higher layers. |
| `reflection.c` | Loads/query/formats reflection payloads and schemas. | Lets runtime/compatibility/tools inspect compiler-emitted metadata. |
| `realtime.c` | Loads realtime sidecars and fixed-size bundle summaries. | Runtime support for generated native app sidecars. |
| `attrition.c` | Long-run counters, event ring, checkpoint/progress, heap/process/actor/async notes. | Certification substrate for teardown and lifecycle truth. |
| `async.c` | Task/future/timer runtime, polling, await, wake, cancellation, attrition hooks. | Backs native `Future`, `async`, timers, and scheduler integration. |
| `compatibility.c` | Bundle compatibility, activate/deactivate/update/uninstall, state snapshots, validation formatting. | Hot reload and runtime version behavior. |
| `host_bridge.c` | Host/foreign module and service registry. | Keeps plugin/foreign services explicit and service-gated. |
| `memory.c` | `__kain_*` helpers, allocation header accounting, helper allocation cache, pointer load/store/realloc. | Critical for raw memory, ownership lowering, and native ABI safety. |
| `ownership.c` | Region registry, pointer index, state transitions for observe/collapse/decay, helper allocation relocation/decay. | Native guard for Kain ownership semantics. |
| `bitfield.c`, `union.c` | Packed field and union helpers. | Low-level layout primitives; use bitvector proofs for changes. |
| `actor.c` | Actor runtime table, scheduler ring, mailbox, node cache, reply-port fast path, registry, monitor/link, supervision, telemetry. | Hot path for `spawn`, `send`, `ask`, actor benchmarks, and runtime stability. |
| Platform `platform.c`, `platform_library.c` | Generic platform and dynamic library substrate. | Keep app/platform details behind adapters and typed package surfaces. |
| Platform `win32/*`, `linux/*` | OS-specific hosts and shared code. | Platform mechanics only; do not define generic Kain semantics here. |
| UI `ui_*` | Raw native UI ABI, component runtime, hot reload, host adapter. | Often owned by `runtime-stdlib`/`package-kaintana` unless the generic core service contract changes. |

## C Runtime Mechanics Worth Remembering

Startup:

- Generated programs call runtime init helpers from the stdlib/native facade.
- Contract sidecars and service descriptors tell the runtime what must exist.
- Required services should fail with structured diagnostics.
- Optional/degraded services should be explicit and queryable.

Memory:

- `KainAllocHeader` is 16 bytes by static assertion.
- Helper allocations carry a magic tag plus slot token so ownership helpers can find the registry entry.
- Allocation/reallocation math must prove header+payload and payload multiplication cannot wrap.
- The allocation cache is a performance lane, not an ownership bypass.

Actors:

- `KainActorRef` carries actor id, generation, execution class, and locality class.
- Reply ports use generation-tagged synthetic refs so late stale replies are rejected.
- The scheduler queue is mask-indexed and requires power-of-two capacity.
- Mailbox/message ownership transfers must follow `ACTOR_RUNTIME_OWNERSHIP.md`.

Ownership:

- Portable language truth lives in `crates/ownership`; C owns checked transitions for helper-owned heap regions and imported pointers.
- States are idle, observed, collapsed, decayed.
- Decay/free is legal only for idle heap regions unless a proof-backed helper path says otherwise.

Machine stones:

- `axiom` checks target/arch/capability truth through native capability bits.
- `pulse` owns runtime timing snapshots and fire count telemetry.
- `teleport` records cross-world handoff tokens.
- `shatter` allocates SoA-style lane payloads and lane pointers.

## Boundary Rules

- Platform-specific mechanics can live in platform adapters. Platform-specific language meaning cannot.
- Vendor code can strengthen a provider lane. Vendor code cannot become the public Kain contract.
- Package-local bridges belong in packages/blades, not generic runtime core.
- Native app/demo sidecars belong in manifest/sidecar data, not hardcoded C branches.
- If the runtime API is public to authored Kain, root `stdlib` and `STDLIB_MAP.llm.md` must be updated.
- If the runtime API is called by generated LLVM, check codegen declarations and ABI layout.

## Source Search Patterns

```powershell
rg -n "kain_actor_|KainActorRef|reply_port|generation|mailbox|scheduler" runtime/native/include/actor.h runtime/native/src/core/actor.c
rg -n "__kain_alloc|KainAllocHeader|payload_size|slot_token|pointer_with" runtime/native/include/memory.h runtime/native/src/core/memory.c
rg -n "__kain_ownership|KAIN_OWNERSHIP_|observe|collapse|decay" runtime/native/include/ownership.h runtime/native/src/core/ownership.c
rg -n "KAIN_SERVICE_KEY|kain_service_registry|canonicalize|descriptor" runtime/native/include/services.h runtime/native/src/core/services.c runtime/native_core_runtime.toml
rg -n "kain_machine_|pulse|teleport|shatter|axiom" runtime/native/include/machine_stones.h runtime/native/src/core/machine_stones.c
```
