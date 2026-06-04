---
name: lang-c-abi
description: >-
  Use when authoring, explaining, reviewing, or repairing Kain-side native and
  foreign ABI boundaries: canonical natural `include ... as ...` C header imports,
  angle-bracket system-header imports such as `include <stdio.h> as cstdio`,
  companion `.c` source discovery, legacy/explicit `use c::...` imports,
  optional explicit blade/package bridge
  metadata, `use rust::...`, Rust crate FFI, host JSON bridges, shared
  buffers/images/tensors/geometry, OS or vendor DLL contracts, handles,
  callbacks, strings, buffers, ABI/lifetime/status design, and app-level
  native package surfaces without changing compiler or runtime ABI internals.
  If the task is mainly first-class Python `import ...` or `std::python`, use
  `lang-python` instead; load both when Python and native ABI work are fused.
---

# Lang C ABI

Use this skill when Kain code crosses out of Kain into a native, OS, driver,
vendor, package, or general foreign-ABI boundary.

Canonical authored C interop starts with `include path/to/header.h as alias`.
That is the future-facing Kain shape for local headers and natural C file
imports. `use c::...` remains valid for runtime-owned ABI modules and explicit
bridge metadata, but do not present it as the default for new local C examples.
Treat old `usec::` wording as deprecated legacy vocabulary: mention it only
when repairing or migrating old code.

The goal is not "write C from Kain." The goal is:

```text
Kain owns semantic policy, state, orchestration, ownership, validation, and app shape.
C/foreign code owns OS contracts, driver ABIs, vendor libraries, hostile headers,
dynamic loaders, syscalls, native handles, bridge crates, and unavoidable machine-facing surfaces.
```

If the boundary also includes first-class Python imports, local `.py` helpers,
or `std::python` materialization, co-read `lang-python`.

## Trigger Shape

Use this skill for:

- Kain source with canonical `include native/foo.h as f`, companion `.c` sources, `use c::...`, `use rust::...`, `use std::platform`, bridge calls, or generated native package modules.
- A task that mentions C ABI, FFI, DLL, dylib, shared library, object file, static library, bitcode, inline C, package bridge, platform SDK, OS API, Vulkan/Win32/CUDA/etc. boundary, callback, raw buffer, native handle, or host bridge.
- Designing the authored Kain facade around a native library, Rust crate bridge, or host-facing package contract.
- Deciding whether a boundary should be canonical `include ... as ...`, explicit/runtime `use c::...`, `use rust::...`, `use std::platform`, or `use std::interop`.
- Reviewing whether a native boundary has correct status, lifetime, buffer, handle, ownership, string, JSON, and teardown shape.
- Explaining why Kain can touch OS and vendor surfaces without letting those surfaces own the application.

Do not use this skill for compiler/runtime implementation changes. If the
importer, ABI classifier, dynamic loader, native runtime, or generated bridge
is wrong underneath, hand off to the owning bootstrap/runtime skill.

## The Native Boundary Contract

Every native boundary needs five answers before code gets clever:

- **Who owns the bytes?** Kain-owned string/buffer, C-owned borrowed pointer, C-owned allocated result, shared host object, raw pointer span, typed image/tensor/mesh container, or opaque handle.
- **Who owns lifetime?** One-call borrow, Kain `collapse`/`observe`/`decay`, explicit `open`/`close`, bridge-owned cache, runtime service, or package session.
- **Who owns failure?** Integer status, null/zero handle, diagnostic string, last-error slot, `Result`, test assertion, or benchmark mismatch.
- **Who owns policy?** Kain should own the semantic decision. Native code should do the thing only native code can do.
- **Who owns live runtime state?** C global state, explicit session handle, runtime service, host bridge registry, or generated bridge package.

If those answers are fuzzy, the code will work once and rot.

## Why Kain Can Do This

Kain has several layers that make native interop more than a dumb foreign call:

- `include local/header.h as alias` is the canonical authored local-header C import shape. It is detected from source, resolved through the C FFI import lane, discovers nearby companion C sources, and is tracked as alias-aware include provenance in the AST/runtime contract.
- `include <stdio.h> as cstdio`, `include <math.h> as cmath`, `include <windows.h> as win`, `include <sys/mman.h> as posix`, and `include <vulkan/vulkan.h> as vk` are registry-backed system-header forms. Angle-bracket includes resolve through deterministic SDK/env roots plus compiler-owned linker policy declared in `crates/c-ffi/system_headers.toml` instead of requiring a handwritten bridge manifest first.
- `use c::<module>` is detected from source and resolved through the C FFI import lane, but new local-header examples should prefer `include ... as ...` unless they are targeting runtime-owned ABI or explicit bridge metadata.
- `use rust::<module>` is resolved through the Rust crate FFI lane.
- Runtime-owned headers under `runtime/native/include` can resolve automatically. Do not require manifest ceremony for those.
- Blade/package-owned bridges can declare explicit metadata when they own headers, sources, objects, static libs, bitcode, or shared libs.
- `kain-foreign-abi` normalizes scalar, pointer, ownership, callback, aggregate, and calling-convention shape.
- `kain-c-ffi` extracts headers, classifies callable/stubbed/unsupported symbols, emits Kain extern modules, reports, manifests, and bridge crates.
- `stdlib/platform.kn` gives Kain a handle-oriented dynamic library surface for explicit OS loader work.
- `stdlib/interop.kn` and the host bridge substrate expose shared-buffer/image/tensor/geometry vocabulary for cross-language exchange.
- `runtime/native` keeps ABI/service headers in C so Kain-authored code can bind to stable machine contracts.

That stack means agents should not treat FFI as one-off glue. The authored Kain
surface should look like a Kain API with the foreign boundary hidden
underneath.

## Boundary Decision Flow

Use this order:

1. **Local header/source pair:** if the Kain file owns a nearby C wrapper, use `include native/foo.h as f`; this is the canonical path. The import lane resolves the local header, requires the sibling `.c` source for native linking, and emits alias externs such as `f_call` via `@link_name`.
2. **Known system header family:** if the header is in the registry-backed system lane such as `stdio.h`, `math.h`, `windows.h`, `sys/mman.h`, `pthread.h`, or `vulkan/vulkan.h`, use `include <...> as alias` first. This resolves through deterministic env/SDK roots and registry-declared link policy instead of forcing a bridge manifest.
3. **Runtime-owned native ABI:** if the header lives under `runtime/native/include`, start with plain `use c::<name>` or the public `std.*` wrapper.
4. **Public stdlib wrapper:** if `std.fs`, `std.net`, `std.process`, `std.graphics`, `std.ui`, `std.platform`, or `std.dcc` already expresses the need, author against `std.*` first.
5. **Blade/package-owned bridge with explicit metadata:** if the wrapper needs non-sibling sources, objects, bitcode, static libs, link libs, defines, or vendor include paths, use `use c::<bridge>` plus `[c_ffi]` metadata near that blade/package. This is the explicit bridge path, not the default for simple local C files.
6. **Rust crate or host bridge lane:** if the boundary is already owned by a Rust crate or a host bridge entrypoint, use `use rust::...`, `kain import-crate`, or `kain bridge serve` before inventing a C detour.
7. **Mixed Python boundary:** if a Python package is also part of the public surface, co-trigger `lang-python`.
8. **Runtime/compiler handoff:** if the authored shape is good but import/lowering/loading/dispatch is broken, stop blaming the Kain file and fix the substrate with the owner skill.

## Fast Discovery

```powershell
rg -n "include .* as |use c::|use rust::|\\[c_ffi\\]|kain_dynlib|shared_lib|tier =" . agents blades benchmark library_of_kain runtime stdlib crates
rg -n "abi_|platform_library|host_bridge|shared_buffer|shared_image|shared_tensor|shared_geometry" stdlib runtime/native/include runtime/native/src crates
rg --files | rg "(ffi|bridge|interop|platform|host_bridge|foreign_abi|c_ffi|crate_ffi|dcc)"
```

If the same task also includes first-class Python imports or local `.py`
resolution, open `lang-python` too instead of making this skill explain the
embedded Python lane.

## Command Loops

Authored Kain validation:

```powershell
kain check smoke.kn --target llvm
kain build smoke.kn --target llvm -o .kain/run/smoke.exe
kain run smoke.kn --target llvm
python benchmark/run.py --case ffi_shared_call_stress --languages kain --runs 1 --warmups 0 --timeout 900
```

Rust crate and host bridge loops:

```powershell
kain import-crate my_crate --manifest-path Cargo.toml --mode both --output .kain/generated/my_crate
kain bridge serve --entry bridge_entry.kn --dispatch-function kain_bridge_dispatch
```

Use the real runtime target you are authoring for. Start with the smallest
smoke, then graduate to the larger lane only when the claim requires it.

## Natural Header Include Pattern

Use this when a Kain example or package owns a nearby C wrapper and should feel
like importing a sibling source file, not writing a manifest first. This is the
default pattern future examples should teach:

```kn
include native/native_math.h as nm

fn native_probe() -> Int:
    return nm_mix(7, 11)
```

Literal companion-file layout:

```text
my_blade/
  src/main.kn
  native/native_math.h
  native/native_math.c
```

```c
// native/native_math.h
#pragma once

int native_math_mix(int a, int b);
const char *native_math_label(void);
```

```c
// native/native_math.c
#include "native_math.h"

int native_math_mix(int a, int b) {
    return (a * 31) ^ (b * 17);
}

const char *native_math_label(void) {
    return "native-math";
}
```

```kn
// src/main.kn
include ../native/native_math.h as nm

fn main() -> Int:
    let score = nm_mix(7, 11)
    println("C says " + nm_label() + " score=" + score)
    return 0
```

The sibling `.c` file is not decoration. It is the implementation translation
unit that gives the native linker real symbols. For local C interop examples,
show both files unless the code is intentionally consuming an already-built
library through explicit bridge metadata.

The include alias is first-class provenance. The current C-FFI lane maps the
header to an import name (`native_math`), finds `native/native_math.c`, emits the
canonical `c::native_math` extern module, and also emits `nm_*` alias externs
linked to the real C symbols through `@link_name`. True dot namespaces can grow
on this AST shape later without flattening the provenance graph.

When writing a Kain facade over generated alias functions, do not give the Kain
wrapper the exact same name as the raw C symbol. For example, if the header
exports `editor_presenter_open` and the include alias emits `ui_open`, name the
authored Kain wrapper `presenter_open` or `editor_ui_open`, not
`editor_presenter_open`. Reusing the raw symbol name can make the alias thunk
call the Kain wrapper instead of the extern and recurse until the native lane
crashes.

## System Header Include Pattern

Use this when the header already belongs to a known system family and the real
pain is ceremony, not package discovery:

```kn
include <stdio.h> as cstdio
include <math.h> as cmath
include <sys/mman.h> as posix_mman
include <vulkan/vulkan.h> as vk
```

Current repo truth:

- Angle-bracket includes resolve through deterministic roots such as `KAIN_C_FFI_SYSTEM_INCLUDE_ROOTS`, `INCLUDE`, Windows SDK roots, POSIX include roots on Unix hosts, and Vulkan SDK roots instead of scanning the whole machine blindly.
- The system lane is data-driven through `crates/c-ffi/system_headers.toml` plus `crates/c-ffi/src/system_registry.rs`; add families, headers, SDK env vars, library names, and platform policies there before adding new resolver branches.
- The registry currently covers portable C runtime headers, a compiler-owned C runtime math subset for `math.h`, POSIX headers on Linux/macOS, Windows SDK headers on Windows, and Vulkan SDK headers through the Vulkan loader subset.
- These imports are currently native-link oriented. They are ideal for LLVM/native packaging; live interpreter/test bridge loading still needs a real dynamic-library ownership story for the imported family.
- Compiler-owned subset headers are intentional when hostile platform headers are too macro-heavy for the current extractor. `math.h` currently routes through `runtime/native/include/c_runtime_math_subset.h`; Vulkan routes through `runtime/native/include/vulkan_loader_subset.h` so loader handles and proc-address returns stay scalar-safe on LLVM (`vk_GetInstanceProcAddr(0, "...") -> Int`).
- If the system family is unknown or the link policy is ambiguous, use explicit `[c_ffi]` metadata or a local C wrapper header instead of pretending the header is self-describing.

When a natural include exposes a raw C string result such as `const char *`,
the generated extern surface should carry `@c_string_return` on both the
canonical import and the include-alias externs. The LLVM lane materializes that
raw `i8*` into owned Kain string storage before normal string semantics touch
it, so authored Kain can treat `nm_label()` or `sql_*` string returns like real
Kain strings instead of borrowed native pointers.

If the C library already uses the alias as its own prefix, keep that spelling.
For example, `include nuklear.h as nk` exposes `nk_strlen` rather than
`nk_nk_strlen`, while `include sqlite3.h as sql` exposes `sql_libversion_number`.
Header-only C libraries still need one sibling implementation translation unit,
such as `nuklear.c` with `#define NK_IMPLEMENTATION`, so the native link has real
symbols to bind.

## Default `use c::...` Pattern

Runtime-owned imports should be boring. This is still supported, but it is not
the canonical teaching pattern for local C headers:

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

Do not write new docs or examples with `usec::...`; that spelling is deprecated
legacy vocabulary. When encountered, migrate the design toward
`include header.h as alias` for local C or `use c::module` for runtime/explicit
bridge imports.

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

This metadata is not the universal default. It is for non-runtime-owned
bridges.

## Modern Multi-Library Bridge Pattern

Real blades often wrap more than one native library. The FFmpeg editor gauntlet
is the current repo truth for a complex multi-library C ABI boundary:

```toml
[c_ffi]
tier = "inline"
include_paths = [
  "native",
  "${env:KAIN_PLATFORM_FFMPEG_SDK}/include",
]

[[c_ffi.libraries]]
name = "ffmpeg_bridge"
header = "native/ffmpeg_bridge.h"
sources = ["native/ffmpeg_bridge.c"]
include_paths = [
  "native",
  "${env:KAIN_PLATFORM_FFMPEG_SDK}/include",
]
static_libs = [
  "${env:KAIN_PLATFORM_FFMPEG_SDK}/lib/avformat.lib",
  "${env:KAIN_PLATFORM_FFMPEG_SDK}/lib/avcodec.lib",
  "${env:KAIN_PLATFORM_FFMPEG_SDK}/lib/avutil.lib",
  "${env:KAIN_PLATFORM_FFMPEG_SDK}/lib/swscale.lib",
]

[[c_ffi.libraries]]
name = "editor_presenter"
header = "native/editor_presenter.h"
sources = ["native/editor_presenter.c"]
include_paths = ["native"]
link_libs = ["user32", "gdi32"]
```

```kn
include "../native/ffmpeg_bridge.h" as ff
include "../native/editor_presenter.h" as ui
include <libavutil/avutil.h> as avu
include <libavcodec/avcodec.h> as avc
include <libavformat/avformat.h> as avf
include <libswscale/swscale.h> as sws

pub fn ffmpeg_open_media(path: String) -> Int:
    return ff_open_media(path)

pub fn ffmpeg_close_media(media: Int) -> Int:
    return ff_close_media(media)

pub fn presenter_open(title: String, width: Int, height: Int) -> Int:
    return ui_open(title, width, height)
```

Key takeaways from `blades/c/ffmpeg`:

- Multiple `[[c_ffi.libraries]]` entries are normal when the blade owns more than one native surface.
- `tier = "inline"` compiles the bridge-owned C sources into the native link.
- `include_paths` can reference env vars for vendor SDK roots.
- `static_libs` links vendor import libraries; `link_libs` links OS system libraries.
- The Kain facade (`ffmpeg_abi.kn`) stays thin: it maps `ff_*` and `ui_*` aliases into semantically named Kain functions and never lets raw C vocabulary leak into the rest of the app.
- Handles are plain integers at the boundary; the C bridge owns the lookup tables and validation.

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
write a small local C wrapper first.

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
use std::interop

fn replace_shared_payload(target: Any, bytes: Any):
    interop_shared_buffer_replace_bytes(target, bytes)
```

## Rust Crate And Host Bridge Pattern

Not every foreign boundary should become a C wrapper first.

Use `use rust::...` and crate-FFI generation when:

- a Rust crate already owns the low-level contract cleanly
- the real task is exposing a crate API into Kain, not translating it yet
- bridge generation is cheaper and safer than manual C shim work

Use host bridges when:

- the boundary is tool-facing, control-plane-ish, or integration-shaped
- a dynamic dispatch entrypoint is a better fit than raw exported symbols
- performance-critical payloads stay in typed/shared lanes rather than JSON blobs

Rules:

- Treat JSON as acceptable for diagnostics, config, or offline tool glue, not hot-lane transport.
- Do not invent a C shim just because it feels familiar if `use rust::...` or a host bridge owns the contract better.
- If the task is actually "call Python modules naturally," leave this skill and use `lang-python`.

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

If the real boundary is a Python package, use `lang-python`.

## Kain Facade Pattern

Always wrap the foreign vocabulary in Kain vocabulary:

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

## Source Anchors

Use these when you need implementation truth:

- `crates/c-ffi/system_headers.toml` and `crates/c-ffi/src/system_registry.rs`: registry-backed angle-bracket system header families, SDK/env roots, shim headers, per-target link policy, and package discovery metadata.
- `crates/c-ffi/src/lib.rs`: canonical `include ... as ...` detection, `use c::` detection, resolution order, automatic runtime-owned headers, registry-backed system headers, cache, bridge loading, native link inputs.
- `crates/c-ffi/src/config.rs`: `[c_ffi]`, library metadata, and interop tiers.
- `crates/c-ffi/src/extract.rs`: header parsing, regex fallback, callable/stubbed report entries, callback and named-type treatment.
- `crates/c-ffi/src/generate.rs`: generated `.kn` module, bridge crate, binding reports, packaged bridge manifests, string and byte-buffer marshaling.
- `crates/c-ffi/src/platform.rs`: target locks, generated platform modules, and package discovery metadata.
- `crates/foreign-abi/src/lib.rs`: normalized ABI type graph, scalar table, pointer direction/ownership policy, callback metadata.
- `crates/crate-ffi/src/lib.rs`: `use rust::...` and Rust crate FFI bridge generation.
- `crates/cli/src/import_crate.rs`, `crates/cli/src/bridge.rs`: Rust crate and host bridge command behavior.
- `runtime/native/include/host_bridge.h` and `runtime/native/src/core/host_bridge.c`: host bridge registry, foreign runtime lanes, service descriptors, bridge contracts.
- `runtime/native/include/platform_library.h` and `runtime/native/src/platform/platform_library.c`: dynamic library handle/status surface.
- `stdlib/platform.kn`, `stdlib/js.kn`, `stdlib/interop.kn`: authored stdlib bridge vocabulary.
- `crates/import/tests/abi_corpus/manifest.json`: C ABI layout cases for pragma pack, explicit alignment, named pack stacks, bitfields, and unions.

Use examples as probes:

- `blades/c/ffmpeg`: multi-library C ABI gauntlet with system headers, static libs, link libs, env-var include paths, and deterministic handle lifecycle.
- `blades/c/sqlite`: zero-manifest natural include of a real C amalgamation (`include sqlite3.h as sql`).
- `blades/c/include-natural`: canonical `include ... as ...` with sibling `.c` source discovery.
- `blades/c/nuklear`: natural include where the alias matches the library prefix and a companion `.c` provides `NK_IMPLEMENTATION`.
- `library_of_kain/ffi_shared_call.kn`: compact shared-library call pressure.
- `benchmark/cases/ffi_shared_call_stress/main.kn`: benchmark version of the shared-call lane.
- `blades/test/fabric_FFI/c_ffi/beacon_math`: scalar, bool, float, string, and intentionally unsupported pointer declarations.
- `blades/test/fabric_FFI/c_ffi/miniaudio_tone_lab`: native audio file generation/probe through a small C bridge.
- `blades/test/fabric_FFI/c_ffi/cgltf_scene_probe`: handle lifecycle over a third-party parser.
- `smoketest/src/stdlib/interop_lane.kn`: shared-contract vocabulary and stdlib integration pressure.

Do not make Vulkain the default example for generic interop. Use
`package-vulkain` only when the task is specifically the Vulkain package or a
graphics/compute package boundary.

Use `lang-python` when the implementation truth you need lives in
`crates/python/src/lib.rs`, first-class Python import parsing, or Python
materialization helpers.

## Validation Ladder

For authored native interop:

1. `rg` the import and bridge metadata.
2. Read the C header directly; if it is too macro-heavy or complex, write a smaller C wrapper header rather than importing the full vendor dump.
3. Inspect any existing generated report (`.kain/reports/*.json`) for callable/stubbed/unsupported entries.
4. Run `kain check` on the Kain entry.
5. Run the smallest smoke blade or file.
6. Run the package/benchmark/attrition lane only when the change claims package health, performance, or long-horizon runtime cleanliness.

For low-level memory or handle math:

- Use Z3 when pointer span, generation bits, capacity, packing, or ring arithmetic is part of the safety claim.
- Use `test-crash-forensics` if the native executable crashes or hangs.
- Use `test-bench` if the claim is boundary cost or native speed.

If the claim also includes Python-side host objects or local `.py` resolution,
use `lang-python` for that half instead of jamming both models into one skill.

## Hand Off When

- Use `lang-python` when the task starts talking about first-class Python imports, local `.py` modules, `std::python`, or Python-owned host objects.
- Use `lang-systems` when the interop code is mostly raw memory, ownership, actor pressure, effects, zero-copy, async, or unsafe authored Kain.
- Use `lang-semantics` when interop is being fused with worlds, laws, patches, converge, pulse, teleport, components, or shader/compute semantics.
- Use `lang-stdlib` when the right answer is to consume or extend an existing public `std.*` domain rather than bind a native library directly.
- Use `lang-translation` when the task starts as C/C++/Rust/JS logic and should become idiomatic Kain.
- Use `bootstrap-core` when parser/type/lowering/import semantics are wrong.
- Use `runtime-core` when host bridge, dynamic loading, ABI services, handles, memory helpers, scheduler interaction, or native runtime startup/shutdown is wrong.
- Use `runtime-stdlib` when the public stdlib wrapper or runtime-backed domain is wrong.
- Use `runtime-gpu` or `bootstrap-gpu` when the interop boundary is GPU executor/codegen internals, not package use.
- Use `package-kaintana` or `package-vulkain` only for those package-owned surfaces.
- Use `tool-build-system` when Bazel, link inputs, generated `BUILD.bazel`, launcher shims, or build provenance are the blocker.

## Anti-Patterns

- Requiring `KAIN.toml` for runtime-owned `use c::...`.
- Teaching `use c::...` or deprecated `usec::...` as the default path for new local C wrappers when `include header.h as alias` is the canonical shape.
- Showing a local header import without the companion `.c` source or an explicit already-built library path.
- Copying a large package bridge as the default shape for a new native package.
- Calling native functions everywhere instead of writing one Kain facade.
- Naming a Kain facade wrapper exactly the same as the raw C symbol behind an include alias.
- Returning raw pointers without length, ownership, and destroy policy.
- Passing a buffer without count/capacity/element-size agreement.
- Keeping C-owned borrowed strings alive as if they are Kain-owned.
- Letting native callbacks mutate Kain world/actor state from arbitrary threads.
- Swallowing native status codes because the happy path worked once.
- Using interop to hide a compiler/runtime bug.
- Routing performance-sensitive data through JSON host glue when a typed binary or shared-memory lane is clearly the right shape.
- Forcing Python package work through `use c::...` when `lang-python` is the honest boundary.

## Final Taste Check

A good Kain native-interop result should read like:

```text
Kain policy and semantics are obvious.
Native code is a tight, boring contract.
Ownership and teardown are named.
The smallest smoke proves the boundary.
The larger benchmark/package/attrition lane is available when the claim needs it.
```

If an agent reads this skill and still writes "just check Vulkain," it missed
the point. Native interop is a design lane, not a scavenger hunt.
