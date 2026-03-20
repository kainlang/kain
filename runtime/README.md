# Kain Native Runtime

The Kain native runtime is the manifest-driven C runtime used by the LLVM/native executable lane.

The canonical runtime entrypoint is [native_runtime.toml](./native_runtime.toml), not the legacy umbrella file [kain_runtime.c](./kain_runtime.c). If you are working on native Kain executables, raw-native app hosting, or LLVM-linked runtime behavior, this folder is the source of truth.

## Executive Summary

- For Windows-native LLVM executables, the runtime is already materially usable.
- For the broader "full Kain runtime vision", it is still only partially complete.
- A fair engineering estimate today is:
  - `65-75%` of the foundational raw-native runtime substrate is in place.
  - `35-45%` of the broader long-range Kain runtime vision is in place.

Why that split matters:

- The substrate is real: ABI/versioning, runtime contracts, memory helpers, actor bootstrap, async tasks, compiled UI bundles, Win32 app/input/viewport hosting, compatibility APIs, host bridge registration, and conformance harnesses all exist.
- The bigger vision is still incomplete: typed runtime values, deeper actor semantics, stronger async/executor architecture, full material/compute execution, a richer UI/component runtime, stronger cross-platform parity, and tighter backend/runtime truth-keeping.

## What This Runtime Is

This runtime is a raw-native systems layer for compiled Kain programs. It sits between compiler-emitted artifacts and the host platform.

It currently provides:

- ABI versioning and startup validation
- native service registration and capability checks
- low-level memory helper ABI for compiler lowering
- reflection payload loading and lookup
- actor spawn, mailbox, registry, supervision, and scheduler plumbing
- async tasks, wake/poll, timers, cancellation, and async sleep
- compiled UI bundle loading plus runtime-side focus/edit/event groundwork
- Win32 app host, input host, viewport host, and OpenGL lane support
- compatibility, bundle lifecycle, migration, snapshot, and restore primitives
- host bridge registration for Rust, Python, Node, C, and Zig lanes
- conformance harnesses for ABI, actors, async, reflection, diagnostics, UI, graphics, hot reload, host bridge, and platform parity

## What This Runtime Is Not

It is not yet:

- a BEAM-class actor runtime
- a Go-class integrated scheduler/runtime
- a Zig-class comptime/runtime contract
- a full React/SwiftUI/Flutter-style reconciler
- a Unity/Unreal-class rendering and tool runtime
- a fully cross-platform host runtime
- a fully unified backend/runtime truth model across every Kain lane

## Source Of Truth

Use these files in this order when judging the runtime:

1. [native_runtime.toml](./native_runtime.toml)
2. [native/](./native/)
3. [conformance/README.md](./conformance/README.md)
4. [changelogs/NATIVE_RUNTIME_COMPLETION_TRACKER.md](./changelogs/NATIVE_RUNTIME_COMPLETION_TRACKER.md)
5. [native/C_RUNTIME_CONTRACT_PIPELINE.md](./native/C_RUNTIME_CONTRACT_PIPELINE.md)

Important caveats:

- [native_runtime_metadata.json](./native_runtime_metadata.json) is useful, but it currently lags some of the real compiled surface.
- Older sections of [changelogs/KAIN_NATIVE_RUNTIME_FEATURE_MATRIX.md](./changelogs/KAIN_NATIVE_RUNTIME_FEATURE_MATRIX.md) and lower sections of the completion tracker are historical logs, not always current truth.
- [kain_runtime.c](./kain_runtime.c) is legacy and should not be treated as the active runtime definition.

## How It Works

At a high level, the runtime flow looks like this:

1. `kain-core` and related compiler layers emit native-side artifacts and sidecars.
   - runtime contract metadata
   - reflection payloads
   - UI bundles
   - realtime or graphics bundle metadata

2. The LLVM codegen layer lowers Kain operations into a mix of:
   - direct native code
   - calls into the runtime ABI such as `__kain_*` helpers
   - service-aware startup assumptions

3. The driver/native app materialization path resolves [native_runtime.toml](./native_runtime.toml), compiles the listed C sources, and links them into the native executable or app bundle.

4. At startup, the runtime validates:
   - ABI compatibility
   - runtime version compatibility
   - platform requirements
   - required versus optional services
   - bundle metadata expectations

5. Optional subsystems activate based on the emitted artifacts and available services.
   - reflection can load schema payloads
   - actor and async lanes can boot
   - UI bundles can be validated and projected into runtime state
   - graphics/realtime metadata can be validated against the active platform lane

6. Conformance harnesses in [conformance/](./conformance/) validate the runtime behavior against the documented ABI contract.

## Architecture

### Manifest-driven runtime

The runtime is defined by [native_runtime.toml](./native_runtime.toml).

Today that manifest includes:

- core runtime sources
- asset loading
- Win32 and OpenGL host pieces
- platform boundary logic
- UI runtime sources

This is the active build surface for native Kain executables.

### Service and contract layer

The runtime has a real contract and service model:

- ABI and runtime version checks are enforced.
- Missing required services can fail startup in strict mode.
- Missing optional services produce downgrade warnings.

Current limitation:

- The compiled runtime surface is broader than the currently auto-populated service registry. The registry still centers mainly on app host, input, viewport, glTF, and compiled UI bundle services. That means the runtime implementation is ahead of the startup discovery model.

See [native/C_RUNTIME_CONTRACT_PIPELINE.md](./native/C_RUNTIME_CONTRACT_PIPELINE.md) for the contract-specific pipeline and maintenance rules.

### Windows-first platform boundary

The runtime is explicitly Windows-first today.

- Win32 is the active host lane.
- Linux and macOS are represented as explicit stubs with capability descriptors and diagnostics.
- This is better than pretending those platforms are supported, but it is not real host parity yet.

## Current Support Matrix

Status legend used here:

- `Strong`: usable and materially implemented
- `Partial`: real and useful, but still constrained or incomplete
- `Scaffold`: groundwork exists, but it is not enough to claim full feature support

| Area | Status | What is present now | Main caveats |
| --- | --- | --- | --- |
| Core substrate | Strong | Allocation helpers, RC primitives, strings, arrays, maps, file I/O, sockets, queues, threads, diagnostics plumbing | Still not a full managed runtime or unified value model |
| ABI/version/runtime contract | Partial | ABI versioning, runtime versioning, startup validation, strict-mode contract checks, service masks | The registry and metadata surface still lag some implemented subsystems |
| Low-level memory ABI | Partial | `__kain_bind_local`, `__kain_addr_of`, pointer helpers, field/index helpers, load/store, alloc/realloc | `__kain_realloc(..., zeroed_new)` is not fully correct because allocation sizes are not tracked |
| Reflection | Partial | JSON payload loading from string/path/env, schema version checks, type and item lookup, summary formatting | Minimal custom parser, fixed-size internals, not a rich runtime-wide type system |
| Actor runtime | Partial | Spawn, mailbox send/receive, bounded mailbox behavior, registry, monitors, links, supervision, pooled scheduler, snapshots | No typed mailboxes, no selective receive, no distributed actors, deeper policy semantics still partial |
| Async runtime | Partial | Task spawn/poll/await/cancel, wake handles, timers, async sleep, task ids, diagnostics | Fixed capacities, timers are thread-backed, not a larger effect-aware executor |
| UI runtime | Partial | Compiled bundle validation/loading, runtime component records, focus routing, event routing, editable groundwork, overlay compatibility | Not yet a full reconciler, widget toolkit, accessibility layer, or broad retained UI framework |
| Realtime and graphics | Partial | Realtime bundle loading, material/compute metadata parsing, binding validation, GL lane readiness, Win32 viewport hosting | Validation is ahead of execution; full material lifecycle and compute execution are still partial |
| Compatibility and hot reload | Partial | Compatibility validation, install/activate/deactivate/update/uninstall flow, migration hooks, snapshot/restore state | Lifecycle primitives exist, but full live subsystem reload policy is still shallow |
| Host bridge | Partial | Module install/activate/unregister, service registration, ABI checks, required service validation, foreign lane contracts | Still an in-process registry model with thin marshalling and no full dynamic plugin loader story |
| Platform support | Partial | Win32 real lane, Linux/macOS capability stubs, platform diagnostics, service masks | Only Windows is truly implemented |
| Conformance coverage | Strong | Ten registered categories with executable harnesses on the active Windows lane | Green lane-level conformance is not the same as full end-to-end parity proof |

## What Native Kain Executables Can Actually Rely On Today

For the LLVM/native lane, the runtime is already sufficient for:

- linking against the new modular runtime instead of the legacy umbrella
- using the canonical low-level memory helper ABI
- running Windows native executables with runtime contract validation
- booting actor and async runtime subsystems in a real native lane
- loading reflection payloads and compiled UI bundles
- driving Win32 app, input, and viewport flows
- validating graphics/realtime bundle metadata
- exercising the runtime through the shared conformance suite

That means native Kain executables are past the "toy runtime" phase.

What they cannot honestly claim yet is full parity with the larger Kain language vision.

## Comparisons To Other Languages And Runtimes

### Versus C or Rust

The current Kain runtime is closest to a C or Rust-style explicit systems substrate.

- Similarity: explicit ABI, native linking, platform-aware host code, low-level memory helpers
- Difference: Kain adds runtime contracts, service discovery, reflection payload loading, actor/async lanes, and compiled UI bundle support on top of that substrate

In practice, Kain today feels much closer to "a native systems runtime with higher-level subsystems layered on top" than to a deeply managed runtime.

### Versus Go

Go ships with a much more integrated runtime model.

- Go has a cohesive scheduler, GC, stack management, networking integration, and a mature standard runtime contract.
- Kain does not yet have that level of unified runtime ownership.

Kain is currently more explicit and modular, but also less complete as a total runtime environment.

### Versus Erlang / BEAM

Kain now has real actors, mailboxes, links, monitors, supervision, and scheduler machinery, so the comparison is no longer fake.

But it is still far from BEAM:

- no selective receive
- no distributed actor fabric
- no BEAM-level fault containment story
- no equally mature introspection and production policy surface

So the correct framing is:

- Kain has a real native actor runtime
- Kain does not yet have a BEAM-class actor runtime

### Versus Zig

Kain shares Zig's native-first feel more than most language runtimes do.

- Similarity: explicit memory, explicit ABI boundaries, native executables, strong interest in compile-time and native tooling
- Difference: Zig's comptime contract is much tighter and more fundamental to the language model than Kain's current staged/runtime story

Kain still needs a clearer comptime-to-runtime contract before that comparison becomes stronger.

### Versus React, SwiftUI, or Flutter

The UI runtime has real bundle loading, runtime-side state records, focus routing, and event plumbing.

That is enough to compare it loosely to modern declarative UI systems.

But the current runtime does not yet provide:

- a mature reconciler
- a full widget toolkit
- accessibility parity
- broad platform UI backends

So today it is more accurate to say:

- Kain has a native compiled UI bundle runtime
- Kain does not yet have a full React/SwiftUI/Flutter-class UI runtime

### Versus Unity or Unreal

There is now enough host, viewport, asset, UI, and graphics metadata infrastructure to justify engine/runtime comparisons.

But the gap is still large:

- full renderer/runtime lifecycle is incomplete
- tool runtime and editor module story is incomplete
- material and compute execution are not complete
- broader scene/runtime architecture is still thin

The runtime is best thought of as an engine/tool substrate in progress, not a finished engine runtime.

## Biggest Remaining Gaps

If the goal is "native Kain executables are production-grade and aligned with the broader language vision", the next highest-value work is:

1. Lock the canonical runtime truth model.
   - Align manifest, metadata JSON, service registry population, and docs.
   - Remove drift between what is compiled and what is advertised.

2. Finish low-level memory ABI hardening.
   - Add allocation tracking so `__kain_realloc(..., zeroed_new)` can be correct.
   - Keep parity coverage strong across LLVM, native, and other lanes.

3. Deepen actor semantics.
   - Typed mailboxes
   - clearer crash policy guarantees
   - richer observability
   - explicit decisions on selective receive and distributed scope

4. Deepen async execution.
   - Move beyond small fixed tables and thread-backed timers
   - strengthen task ownership and subsystem interop
   - connect more clearly to any future effects/capability model

5. Turn graphics support from validation into execution.
   - Full material lifecycle
   - resource lifetime management
   - compute execution
   - stronger backend contracts

6. Finish the UI runtime story.
   - richer component lifecycle
   - better retained state semantics
   - text editing and widget depth
   - accessibility and broader event semantics

7. Expand cross-platform host parity.
   - Linux and macOS need real host, input, viewport, and graphics lanes

8. Strengthen final-lane proof.
   - Keep conformance green
   - add more end-to-end parity proof
   - keep docs, metadata, and runtime behavior synchronized

## Practical Bottom Line

The runtime no longer needs "basic existence" work. It needs "truth, hardening, and completion" work.

That is an important shift.

For Windows-native executables, the remaining work is mostly about:

- tightening contracts
- removing drift
- hardening semantics
- finishing the deeper subsystems

For the broader Kain language promise, there is still significant runtime architecture left to build.

## Build And Validation

Useful commands:

```bash
bash runtime/compile_native_runtime.sh
bash runtime/validate_native_runtime.sh
bash runtime/conformance/run_all.sh --verbose
```

What those mean:

- `compile_native_runtime.sh` builds the active manifest-driven runtime bundle
- `validate_native_runtime.sh` runs the canonical package tests plus runtime compilation
- `conformance/run_all.sh --verbose` is the strongest current lane-level proof that the Windows native runtime behaves as expected

## Guidance For Future Runtime Work

When extending this runtime:

- update the manifest first if the compiled surface changes
- keep service names and capability declarations data-driven
- prefer widening the service registry and metadata truth model over adding hidden one-off behavior
- add or extend a conformance harness for every real runtime behavior change
- do not treat the legacy [kain_runtime.c](./kain_runtime.c) file as the canonical runtime lane

If this document and the runtime disagree, trust the manifest, the current source in [native/](./native/), and the top reality-update sections in the runtime changelogs, then update this README.
