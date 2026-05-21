---
name: lang-interop
description: "Use when authoring, explaining, reviewing, or repairing Kain-side interop with native and foreign systems: automatic `use c::...` imports, optional explicit blade/package bridge metadata, `kain import-c`, `kain import platform`, Rust crate FFI, host JSON bridges, shared buffers/images, OS or vendor DLL contracts, handles, callbacks, strings, buffers, ABI/lifetime/status design, and app-level native package surfaces without changing compiler or runtime ABI internals."
---

# Lang Interop

Use this skill when Kain code crosses out of Kain into a native, OS, driver,
vendor, package, or host-language boundary.

The goal is not "write C from Kain." The goal is:

```text
Kain owns semantic policy, state, orchestration, ownership, validation, and app shape.
C/foreign code owns OS contracts, driver ABIs, vendor libraries, hostile headers,
dynamic loaders, syscalls, native handles, and unavoidable ecosystem surfaces.
```

Interop is why Kain does not become a floating VM. Kain can stay alien and
high-level because the C ABI floor gives it a real contract with the machine.

## Trigger Shape

Use this skill for:

- Kain source with `use c::...`, `use rust::...`, `use std::platform`, bridge calls, or generated native package modules.
- A task that mentions C ABI, FFI, DLL, dylib, shared library, object file, static library, bitcode, inline C, package bridge, platform SDK, OS API, Vulkan/Win32/CUDA/etc. boundary, callback, raw buffer, native handle, or host bridge.
- Designing the authored Kain facade around a native library.
- Reviewing whether a native boundary has correct status, lifetime, buffer, handle, string, and teardown shape.
- Explaining why Kain can touch OS or vendor surfaces without letting C own the application.

Do not use this skill for compiler/runtime implementation changes. If the
importer, ABI classifier, dynamic loader, native runtime, or generated bridge is
wrong underneath, hand off to the owning bootstrap/runtime/tool skill.

## The Interop Contract

Every boundary needs four answers before code gets clever:

- **Who owns the bytes?** Kain-owned string/buffer, C-owned borrowed pointer, C-owned allocated result, shared host object, raw pointer span, or opaque handle.
- **Who owns lifetime?** One-call borrow, Kain `collapse`/`observe`/`decay`, explicit `open`/`close`, bridge-owned cache, runtime service, or package session.
- **Who owns failure?** Integer status, null/zero handle, diagnostic string, last-error slot, `Result`, test assertion, or benchmark mismatch.
- **Who owns policy?** Kain should own the semantic decision. Native code should do the thing only native code can do.

If those answers are fuzzy, the code will work once and rot.

## Why Kain Can Do This

Kain has several layers that make interop more than a dumb foreign call:

- `use c::<module>` is detected from source and resolved through the C FFI import lane.
- Runtime-owned headers under `runtime/native/include` can resolve automatically. Do not require manifest ceremony for those.
- Blade/package-owned bridges can declare explicit metadata when they own headers, sources, objects, static libs, bitcode, or shared libs.
- `kain-foreign-abi` normalizes scalar, pointer, ownership, callback, aggregate, and calling-convention shape.
- `kain-c-ffi` extracts headers, classifies callable/stubbed/unsupported symbols, emits Kain extern modules, reports, manifests, and bridge crates.
- `stdlib/platform.kn` gives Kain a handle-oriented dynamic library surface for explicit OS loader work.
- `stdlib/interop`, `stdlib/c`, `stdlib/python`, and `stdlib/javascript` expose shared-buffer/image and host-language bridge vocabulary.
- `runtime/native` keeps ABI/service headers in C so Kain-authored code can bind to stable machine contracts.

That stack means agents should not treat FFI as one-off glue. The authored Kain
surface should look like a Kain API with a native boundary hidden underneath.

## Boundary Decision Flow

Use this order:

1. **Runtime-owned native ABI:** if the header lives under `runtime/native/include`, start with plain `use c::<name>` or the public `std.*` wrapper.
2. **Public stdlib wrapper:** if `std.fs`, `std.net`, `std.process`, `std.graphics`, `std.ui`, `std.platform`, etc. already exists, author against `std.*` first.
3. **Blade/package-owned bridge:** if the app/package owns a small C wrapper, use `use c::<bridge>` plus optional `[c_ffi]` metadata near that blade/package.
4. **Generated import preflight:** if the header shape is unknown, run `kain import-c` to inspect what Kain can represent before hand-authoring the final facade.
5. **Platform package lock:** if the target is a vendor/system SDK, use `kain import platform` to produce target-aware lock and generated thunk artifacts.
6. **Host-language bridge:** if the boundary is Rust/Python/Node/JSON rather than C ABI, keep payloads explicit and use `lang-translation` only to reshape logic into real Kain.
7. **Runtime/compiler handoff:** if the authored shape is good but import/lowering/loading is broken, stop blaming the Kain file and fix the substrate with the owner skill.

## Fast Discovery

```powershell
rg -n "use c::|use rust::|\[c_ffi\]|kain_dynlib|shared_lib|tier =|import platform|import-c" . agents blades benchmark library_of_kain runtime stdlib crates
rg -n "abi_|platform_library|host_bridge|shared_buffer|shared_image" stdlib runtime/native/include runtime/native/src
rg --files | rg "(ffi|bridge|interop|platform|host_bridge|foreign_abi|c_ffi|crate_ffi)"
```

Use the examples as vocabulary, not molds. For fresh authored code, prefer a
small bridge with its own Kain facade over copying a large package shape.

## Command Loops

Header/import inspection:

```powershell
kain import-c native/my_bridge.h -I native -D MY_FLAG=1 -o .kain/generated/my_bridge.kn --report-json .kain/generated/my_bridge.report.json
kain import-c vendor/tiny_c_lib -o .kain/generated/tiny_c_lib.kn --include public --exclude tests --report-json .kain/generated/tiny_c_lib.report.json
```

Platform package discovery:

```powershell
kain import platform vendor/tiny_math --package-name tiny_math --header vendor/tiny_math/tiny_math.h --output .kain/platform/tiny_math --dry-run
kain import platform vendor/tiny_math --package-name tiny_math --header vendor/tiny_math/tiny_math.h --output .kain/platform/tiny_math
```

Rust crate/host bridge loops:

```powershell
kain import-crate my_crate --manifest-path Cargo.toml --mode both --output .kain/generated/my_crate
kain bridge serve --entry bridge_entry.kn --dispatch-function kain_bridge_dispatch
```

Authored Kain validation:

```powershell
kain check smoke.kn --target llvm
kain run smoke.kn
kain build smoke.kn --target llvm -o .kain/run/smoke.exe
python benchmark/run.py --case ffi_shared_call_stress --languages kain --runs 1 --warmups 0 --timeout 900
```

## Default `use c::...` Pattern

Runtime-owned imports should be boring:

```kn
use c::version

fn runtime_abi_ok() -> Int:
    if version_check_abi_compatibility(256):
        return 0
    return 1
```

No TOML is required for the runtime-owned lane. If an agent adds a manifest for
`runtime/native/include/version.h` just to make `use c::version` work, it is
probably fighting the current pipeline.

## Blade/Package Shared Bridge Pattern

Use this when the blade/package owns the native wrapper:

```kn
use c::beacon_math

fn beacon_score(a: Int, b: Int) -> Int:
    let sum = beacon_add(a, b)
    if beacon_is_even(sum):
        return sum + len(beacon_label(sum))
    return sum

test beacon_bridge_shape:
    assert(beacon_add(12, 30) == 42, "C bridge should add scalars")
    assert(beacon_label(9) == "beacon-9", "C string return should become Kain String")
```

The Kain side should expose intention (`beacon_score`) rather than scattering
raw native calls across the app.

Optional explicit metadata belongs beside the owner:

```toml
[c_ffi]

[[c_ffi.libraries]]
name = "beacon_math"
header = "native/beacon_math.h"
shared_lib = "native/${kain_dynlib:beacon_math}"
```

This metadata is not the universal default. It is for non-runtime-owned bridges.

## Inline/Object/Static/Bitcode Tiers

Explicit bridges can choose a tier when source/linkage matters:

```toml
[c_ffi]
tier = "inline"

[[c_ffi.libraries]]
name = "tiny_audio_bridge"
header = "native/tiny_audio_bridge.h"
sources = ["native/tiny_audio_bridge.c"]
include_paths = ["native", "vendor/tiny_audio"]
defines = ["TINY_AUDIO_NO_DEMO=1"]
link_libs = ["winmm"]
```

Tier meaning for authored decisions:

- `dynamic`: consume an existing shared library or generated live bridge.
- `static`: link object/static/bitcode inputs into the native artifact.
- `bitcode`: preserve LLVM bitcode as a native-link input.
- `inline`: compile bridge-owned C source into the native link.
- `fused`: runtime-owned/future compiler-runtime contract, not a generic app shortcut.

If an agent cannot explain why a tier is chosen, use `dynamic`/shared bridge or
preflight with `kain import-c` first.

## Handle Lifecycle Pattern

Native libraries often want handles. Kain should make handle lifetime visible:

```kn
use c::cgltf_scene_probe

fn scene_probe_score(path: String) -> Int:
    let probe = cgltf_probe_open(path)
    if probe <= 0:
        return -1

    let score =
        cgltf_probe_node_count(probe) +
        cgltf_probe_mesh_count(probe) +
        cgltf_probe_primitive_count(probe)

    cgltf_probe_close(probe)
    return score
```

Rules:

- Handles are integers only at the boundary. Inside Kain, name them as handles and close them deterministically.
- Prefer `open/create` plus `close/destroy` pairs over hidden global state.
- A stale handle should be rejected by the native side and tested in the bridge lane.
- If handle validity is a runtime substrate property, use `runtime-core` for the implementation.

## Dynamic Library Pattern

For direct OS loader work, prefer `std::platform` instead of hand-rolling loader
calls in every bridge:

```kn
use std::platform

fn library_symbol_probe(path: String, symbol: String) -> Int:
    let handle = platform_library_open(path)
    if handle <= 0:
        return platform_library_last_status()

    let address = platform_library_resolve(handle, symbol)
    let close_status = platform_library_close(handle)
    if close_status != 0:
        return close_status
    if address == 0:
        return platform_library_last_status()
    return 0
```

Use this for explicit dynamic loading and diagnostics. Use `use c::<bridge>` when
you want typed generated calls.

## String Boundary Pattern

C strings are not Kain strings. The bridge may copy `const char*` returns into a
Kain `String`, but agents must still design lifetime intentionally.

Good:

```kn
use c::beacon_math

fn label_report(id: Int) -> String:
    let label = beacon_label(id)
    return "native label=" + label
```

Risky:

```text
native returns borrowed char* -> Kain stores it as if the C allocation stays valid forever
```

Rules:

- Prefer C APIs that return copied text, status, or write into caller-owned buffers.
- If C returns a borrowed static string, treat it as immediate-use text.
- If C allocates a string, expose a matching free/destroy call or wrap it in a handle.
- Reject C strings containing embedded NULs unless the bridge explicitly owns a byte-buffer path.

## Buffer Boundary Pattern

For buffers, the shape must include pointer, length, capacity, element size, and
write direction. Do not pass "a pointer" and hope.

Kain-side raw memory pattern:

```kn
fn fill_native_words(words: ptr<Int>, count: Int) -> Int with Unsafe:
    if count <= 0:
        return 0

    collapse words:
        let status = native_fill_words(words, count)
        if status != 0:
            return status
        return observe words:
            mem_load(words, "Int")
```

Rules:

- Use `with Unsafe` when raw pointer traffic is explicit.
- Use `collapse` when native code mutates Kain-owned memory.
- Use `observe` when Kain reads after mutation.
- Use `decay` when Kain owns the allocation and the lifetime ends.
- Prove pointer/length math with `tool-z3-black-magic` or the owning Z3 proof lane when it is nontrivial.

For host objects, prefer shared-buffer/image helpers instead of raw pointers:

```kn
use std::interop::bridge

fn replace_shared_payload(target: Any, bytes: Any):
    interop_shared_buffer_replace_bytes(target, bytes)
```

## Callback And Event Pump Pattern

Callbacks are where interop goes feral. Keep the native side boring:

```text
native library/event source
    -> C bridge polls or receives callbacks
    -> bridge stores compact event records/status
    -> Kain actor/world consumes events and owns policy
```

Prefer:

- `poll_events(session) -> Int`
- `event_count(session) -> Int`
- `event_kind(session, index) -> Int`
- `event_payload_*` accessors
- `close/session_destroy`

Avoid:

- C calling arbitrary Kain closures from unknown threads.
- Native callbacks mutating Kain world state directly.
- Event loops that block actor turns without a clear async/actor boundary.
- Borrowed callback context pointers without generation or owner checks.

If callback support itself is missing or wrong in the ABI classifier, use
`bootstrap-core` or `runtime-core` instead of hacking around it in Kain.

## OS Contract Use Cases

Interop is the right answer for:

- Window handles, swapchains, file dialogs, clipboard, IME, drag/drop, process handles, PTYs, sockets, TLS backends, audio devices, HID/input devices, platform timers, dynamic libraries, and vendor SDKs.
- Parsing or probing third-party formats through stable C libraries, then projecting compact results into Kain.
- Tight C wrappers around platform APIs where Kain should decide policy but not manually speak every OS struct.
- Native packages that expose a small Kain-first facade while hiding large C header churn.
- Benchmarks that measure boundary cost honestly, such as shared-library call pressure.

Interop is not the right answer for:

- Reimplementing Kain semantics in C.
- Turning a Kain app into a bag of extern calls.
- Avoiding a real compiler/runtime fix.
- Copying a package bridge shape just because it already runs.

## Kain Facade Pattern

Always wrap the native vocabulary in Kain vocabulary:

```kn
use c::audio_tone_lab

struct ToneReport:
    frames: Int
    channels: Int
    peak: Float
    signature: String

fn write_probe_tone(path: String, hz: Float) -> ToneReport:
    let frames = audiofx_write_sine_wave(path, 48000, 1, 500, hz, 0.65)
    return ToneReport {
        frames: frames,
        channels: audiofx_wav_channels(path),
        peak: audiofx_wav_peak(path),
        signature: audiofx_wav_signature(path)
    }
```

This keeps agents from sprinkling low-level bridge calls throughout a feature.
One native package should feel like one Kain capability.

## Converge And Native Lanes

Use native interop as a lane, not as the semantic source of truth:

```kn
fn scalar_mix(value: Int) -> Int:
    return ((value * 31) + 7) % 1000000007

converge mix(value: Int) -> Int:
    spec reference:
        return scalar_mix(value)
    fast llvm_lane when target("llvm"):
        return ((value * 31) + 7) % 1000000007
    verify random(8)

use c::native_mix_bridge

fn mixed_with_native_probe(value: Int) -> Int:
    let kain_value = mix(value)
    let native_value = native_mix(kain_value)
    return native_value
```

Current authored `orchestrate` examples may use stages such as `kain`, `rust`,
`python`, or `node`. Do not invent a `c` orchestrate stage unless the compiler
surface actually owns it. Use `use c::...` for C ABI calls.

## Import Reports Matter

Generated C FFI reports are not paperwork. They tell you what is callable,
type-only, opaque, stubbed, or unsupported.

Look for:

- `status: callable` for scalar/string/handle functions you plan to call.
- `status: type_only` for callbacks or declarations captured for metadata but not directly callable.
- `status: opaque_handle` for pointer-heavy APIs that need handle wrappers.
- `status: stubbed` or `unsupported` for signatures that need a smaller C shim.
- `capabilities` such as `shared-buffer` when byte/image payloads are involved.

If the report is noisy, write a smaller C wrapper header. Kain should import the
bridge you wish existed, not a 20,000-line vendor header dump.

## Source Anchors

Use these when you need implementation truth:

- `crates/kain-c-ffi/src/lib.rs`: `use c::` detection, resolution order, automatic runtime-owned headers, cache, bridge loading, native link inputs.
- `crates/kain-c-ffi/src/config.rs`: `[c_ffi]`, library metadata, and interop tiers.
- `crates/kain-c-ffi/src/extract.rs`: header parsing, regex fallback, callable/stubbed report entries, callback and named-type treatment.
- `crates/kain-c-ffi/src/generate.rs`: generated `.kn` module, bridge crate, binding reports, packaged bridge manifests, string and byte-buffer marshaling.
- `crates/kain-c-ffi/src/platform.rs`: `kain import platform`, target locks, generated platform modules, package discovery.
- `crates/kain-foreign-abi/src/lib.rs`: normalized ABI type graph, scalar table, pointer direction/ownership policy, callback metadata.
- `crates/kain-crate-ffi/src/lib.rs`: `use rust::...` and Rust crate FFI bridge generation.
- `crates/cli/src/import_c.rs`, `crates/cli/src/import_platform.rs`, `crates/cli/src/import_crate.rs`, `crates/cli/src/bridge.rs`: command behavior.
- `runtime/native/include/host_bridge.h` and `runtime/native/src/core/host_bridge.c`: host bridge registry, foreign runtime lanes, service descriptors, bridge contracts.
- `runtime/native/include/platform_library.h` and `runtime/native/src/platform/platform_library.c`: dynamic library handle/status surface.
- `stdlib/platform.kn`, `stdlib/c/bridge.kn`, `stdlib/interop/bridge.kn`, `stdlib/python/bridge.kn`, `stdlib/javascript/bridge.kn`: authored stdlib bridge vocabulary.
- `crates/kain-import/tests/abi_corpus/manifest.json`: C ABI layout cases for pragma pack, explicit alignment, named pack stacks, bitfields, and unions.

Use examples as probes:

- `library_of_kain/ffi_shared_call.kn`: compact shared-library call pressure.
- `benchmark/cases/ffi_shared_call_stress/main.kn`: benchmark version of the shared-call lane.
- `blades/kain-test/fabric_FFI/c_ffi/beacon_math`: scalar, bool, float, string, and intentionally unsupported pointer declarations.
- `blades/kain-test/fabric_FFI/c_ffi/miniaudio_tone_lab`: native audio file generation/probe through a small C bridge.
- `blades/kain-test/fabric_FFI/c_ffi/cgltf_scene_probe`: handle lifecycle over a third-party parser.

Do not make Vulkain the default example for generic interop. Use
`package-vulkain` only when the task is specifically the Vulkain package or a
graphics/compute package boundary.

## Validation Ladder

For authored interop:

1. `rg` the import and bridge metadata.
2. Run `kain import-c` or `kain import platform` if the native shape is unclear.
3. Read the generated report for callable/stubbed/unsupported entries.
4. Run `kain check` on the Kain entry.
5. Run the smallest smoke blade or file.
6. Run the package/benchmark/attrition lane only when the change claims package health, performance, or long-horizon runtime cleanliness.

For low-level memory or handle math:

- Use Z3 when pointer span, generation bits, capacity, packing, or ring arithmetic is part of the safety claim.
- Use `test-crash-forensics` if the native executable crashes or hangs.
- Use `test-bench` if the claim is boundary cost or native speed.

## Hand Off When

- Use `lang-systems` when the interop code is mostly raw memory, ownership, actor pressure, effects, zero-copy, async, or unsafe authored Kain.
- Use `lang-semantics` when interop is being fused with worlds, laws, patches, converge, pulse, teleport, components, or shader/compute semantics.
- Use `lang-stdlib` when the right answer is to consume an existing public `std.*` domain rather than bind a native library directly.
- Use `lang-translation` when the task starts as C/C++/Rust/JS/Python logic and should become idiomatic Kain.
- Use `bootstrap-core` when parser/type/lowering/import semantics are wrong.
- Use `runtime-core` when host bridge, dynamic loading, ABI services, handles, memory helpers, scheduler interaction, or native runtime startup/shutdown is wrong.
- Use `runtime-stdlib` when the public stdlib wrapper or runtime-backed domain is wrong.
- Use `runtime-gpu` or `bootstrap-gpu` when the interop boundary is GPU executor/codegen internals, not package use.
- Use `package-kaintana` or `package-vulkain` only for those package-owned surfaces.
- Use `tool-build-system` when Bazel, link inputs, generated `BUILD.bazel`, launcher shims, or build provenance are the blocker.

## Anti-Patterns

- Requiring `KAIN.toml` for runtime-owned `use c::...`.
- Copying a large package bridge as the default shape for a new native package.
- Calling native functions everywhere instead of writing one Kain facade.
- Returning raw pointers without length, ownership, and destroy policy.
- Passing a buffer without count/capacity/element-size agreement.
- Keeping C-owned borrowed strings alive as if they are Kain-owned.
- Letting native callbacks mutate Kain world/actor state from arbitrary threads.
- Swallowing native status codes because the happy path worked once.
- Using interop to hide a compiler/runtime bug.
- Treating `kain import-c` generated output as final application design.

## Final Taste Check

A good Kain interop result should read like:

```text
Kain policy and semantics are obvious.
Native code is a tight, boring contract.
Ownership and teardown are named.
Errors are inspectable.
The smallest smoke proves the boundary.
The larger benchmark/package/attrition lane is available when the claim needs it.
```

If an agent reads this skill and still writes "just check Vulkain," it missed the
point. Interop is a design lane, not a scavenger hunt.
