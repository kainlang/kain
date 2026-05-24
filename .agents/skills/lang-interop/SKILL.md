---
name: lang-interop
description: "Use when authoring, explaining, reviewing, or repairing Kain-side interop with native and foreign systems: automatic `use c::...` imports, optional explicit blade/package bridge metadata, `kain import-c`, `kain import platform`, Rust crate FFI, first-class Python `import ...` and `from ... import ...` with local sibling `.py` resolution, host JSON bridges, shared buffers/images/tensors/geometry, OS or vendor DLL contracts, handles, callbacks, strings, buffers, ABI/lifetime/status design, and app-level native package surfaces without changing compiler or runtime ABI internals."
---

# Lang Interop

Use this skill when Kain code crosses out of Kain into a native, OS, driver,
vendor, package, or host-language boundary.

The goal is not "write C from Kain." The goal is not "pretend Python modules are
Kain modules" either. The goal is:

```text
Kain owns semantic policy, state, orchestration, ownership, validation, and app shape.
C/foreign code owns OS contracts, driver ABIs, vendor libraries, hostile headers,
dynamic loaders, syscalls, native handles, embedded Python modules, and unavoidable
ecosystem surfaces.
```

Interop is why Kain does not become a floating VM. Kain can stay alien and
high-level because the C ABI floor gives it a real contract with the machine,
and the host-language bridges let Kain eat ecosystems without surrendering the
authoring model.

## Trigger Shape

Use this skill for:

- Kain source with `use c::...`, `use rust::...`, `use std::platform`, bridge calls, or generated native package modules.
- Kain source with Python imports such as `import numpy as np`, `import trimesh`, `from mypyfile import run as py_run`, or `import tools.mathish as mathish`.
- A task that mentions C ABI, FFI, DLL, dylib, shared library, object file, static library, bitcode, inline C, package bridge, platform SDK, OS API, Vulkan/Win32/CUDA/etc. boundary, callback, raw buffer, native handle, Python module, local `.py` helper, or host bridge.
- Designing the authored Kain facade around a native library or a Python package.
- Deciding whether a boundary should be raw `import ...`, `std::python::*`, `std::dcc::*`, Fabric `runtime = "python"`, `use c::...`, or `use rust::...`.
- Reviewing whether a native or Python boundary has correct status, lifetime, buffer, handle, ownership, string, and teardown shape.
- Explaining why Kain can touch OS, vendor, and full Python ecosystem surfaces without letting those surfaces own the application.

Do not use this skill for compiler/runtime implementation changes. If the
importer, ABI classifier, Python bridge, dynamic loader, native runtime, or
generated bridge is wrong underneath, hand off to the owning bootstrap/runtime
tool skill.

## The Interop Contract

Every boundary needs five answers before code gets clever:

- **Who owns the bytes?** Kain-owned string/buffer, C-owned borrowed pointer, C-owned allocated result, shared host object, raw pointer span, typed image/tensor/mesh container, or opaque handle.
- **Who owns lifetime?** One-call borrow, Kain `collapse`/`observe`/`decay`, explicit `open`/`close`, bridge-owned cache, runtime service, package session, or Python-owner-backed zero-copy view.
- **Who owns failure?** Integer status, null/zero handle, diagnostic string, last-error slot, Python import failure, Python exception, `Result`, test assertion, or benchmark mismatch.
- **Who owns policy?** Kain should own the semantic decision. Native code or Python should do the thing only native code or Python can do.
- **Who owns live runtime state?** Embedded Python scope, Fabric Python step, node host, C global state, explicit session handle, or runtime service.

If those answers are fuzzy, the code will work once and rot.

## Why Kain Can Do This

Kain has several layers that make interop more than a dumb foreign call:

- `use c::<module>` is detected from source and resolved through the C FFI import lane.
- `use rust::<module>` is resolved through the Rust crate FFI lane.
- Top-level Python `import ...` and `from ... import ...` bind into the embedded Python runtime rather than pretending Python is a static Kain module tree.
- Local Python files and packages can resolve relative to the importing `.kn` file before falling back to the active Python environment.
- Imported Python names are registered in the Kain type environment as dynamic/unknown bindings, so authored code can use them without lying about static Kain types.
- Runtime-owned headers under `runtime/native/include` can resolve automatically. Do not require manifest ceremony for those.
- Blade/package-owned bridges can declare explicit metadata when they own headers, sources, objects, static libs, bitcode, or shared libs.
- `kain-foreign-abi` normalizes scalar, pointer, ownership, callback, aggregate, and calling-convention shape.
- `kain-c-ffi` extracts headers, classifies callable/stubbed/unsupported symbols, emits Kain extern modules, reports, manifests, and bridge crates.
- `stdlib/platform.kn` gives Kain a handle-oriented dynamic library surface for explicit OS loader work.
- `stdlib/interop`, `stdlib/c`, `stdlib/python`, and `stdlib/dcc` expose shared-buffer/image/tensor/geometry and host-language bridge vocabulary.
- `runtime/native` keeps ABI/service headers in C so Kain-authored code can bind to stable machine contracts.
- The embedded Python scope is persistent for the Kain execution, which means repeated `import`, `py_exec`, `py_call`, and host-object access share one live Python world instead of cold-starting on every call.

That stack means agents should not treat FFI or Python as one-off glue. The
authored Kain surface should look like a Kain API with the foreign boundary
hidden underneath.

## Boundary Decision Flow

Use this order:

1. **Runtime-owned native ABI:** if the header lives under `runtime/native/include`, start with plain `use c::<name>` or the public `std.*` wrapper.
2. **Public stdlib wrapper:** if `std.fs`, `std.net`, `std.process`, `std.graphics`, `std.ui`, `std.platform`, `std.python`, or `std.dcc` already expresses the need, author against `std.*` first.
3. **First-class Python import lane:** if the task is "I want to use a Python module like a Python module," prefer `import ...` or `from ... import ...` in authored Kain instead of inventing `use python::...`.
4. **Blade/package-owned bridge:** if the app/package owns a small C wrapper, use `use c::<bridge>` plus optional `[c_ffi]` metadata near that blade/package.
5. **Generated import preflight:** if the header shape is unknown, run `kain import-c` to inspect what Kain can represent before hand-authoring the final facade.
6. **Platform package lock:** if the target is a vendor/system SDK, use `kain import platform` to produce target-aware lock and generated thunk artifacts.
7. **Python wrapper/materialization lane:** if the task needs stable authored package helpers, explicit ownership, DCC adapters, or shared-vs-owned data control, move from raw `import ...` to `std::python::*` and `std::dcc::*`.
8. **Whole-file foreign step:** if an entire Fabric stage should just be Python, use Fabric `runtime = "python"` rather than shoving the whole step through inline bridge strings.
9. **Runtime/compiler handoff:** if the authored shape is good but import/lowering/loading/host-object dispatch is broken, stop blaming the Kain file and fix the substrate with the owner skill.

## Fast Discovery

```powershell
rg -n "use c::|use rust::|\[c_ffi\]|kain_dynlib|shared_lib|tier =|import platform|import-c" . agents blades benchmark library_of_kain runtime stdlib crates
rg -n "^import |^from .* import |py_bridge_|py_import|py_call|py_getattr|kain_.*_from_py|runtime = \"python\"" . agents blades benchmark smoketest stdlib crates
rg -n "abi_|platform_library|host_bridge|shared_buffer|shared_image|shared_tensor|shared_geometry" stdlib runtime/native/include runtime/native/src crates/kain-python
rg --files | rg "(ffi|bridge|interop|platform|host_bridge|foreign_abi|c_ffi|crate_ffi|python|dcc)"
```

Use examples as vocabulary, not molds. For fresh authored code, prefer a small
bridge with its own Kain facade over copying a large package shape.

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
kain check smoke.kn --target interpret
kain run smoke.kn --target interpret
kain check smoke.kn --target llvm
kain build smoke.kn --target llvm -o .kain/run/smoke.exe
python benchmark/run.py --case ffi_shared_call_stress --languages kain --runs 1 --warmups 0 --timeout 900
```

Python import and bridge validation:

```powershell
cargo test -p kain-core parses_python_import_items -- --nocapture
cargo test -p kain-python python_import_supports_local_sibling_from_imports -- --nocapture
cargo test -p kain-python python_import_supports_local_dotted_module_alias_calls -- --nocapture
cargo test -p kain-python python_bridge_exec_scope_persists_between_calls -- --nocapture
```

Use the real runtime target you are authoring for. Start with the smallest
interpret smoke when validating a local `.py` import shape, then graduate to the
larger lane only when the claim requires it.

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

## Python Import Lane

Python is now a first-class interop lane in authored Kain. Use `import` for
Python modules and `use` for Kain/native/static lanes.

Supported authored forms:

- `import numpy`
- `import numpy as np`
- `import pkg.subpkg.module`
- `import pkg.subpkg.module as alias`
- `from torch.utils import data`
- `from torch.utils import data as torch_data`
- `from mypyfile import run as py_run`

Not supported in this lane:

- `from pkg import *`

Important authored rule:

- Kain passes the exact module string to Python. There is no blessed allowlist.
- If CPython can import the module from the local search roots or active Python environment, Kain can bind it.
- Real package names matter. If the environment exposes `torch`, use `import torch`. If it exposes some other module name, import that exact name.

Binding semantics match Python closely:

- `import numpy` binds `numpy`
- `import numpy as np` binds `np`
- `import pkg.tools.mathish` binds `pkg` unless you alias it
- `import pkg.tools.mathish as mathish` binds `mathish`
- `from torch.utils import data as torch_data` binds `torch_data`

That means the authored default for deep module paths should usually be an alias:

```kn
import tools.mathish as mathish

fn main() -> Int:
    return mathish.bump(41)
```

Do not fight this by inventing `use python::...`. The point of `import` is to
model a live Python module/object lane honestly.

## Python Member Access And Return Semantics

Imported Python names are registered as dynamic/unknown bindings in the Kain
type environment. That is intentional. Kain should not pretend it statically
understands arbitrary Python package surfaces.

Runtime behavior:

- `np.linspace(...)`, `trimesh.creation.box(...)`, `mesh.export(...)`, and `runner(...)` use normal Kain `.` and call syntax.
- If the value is a Python-backed host object, field access routes through the Python bridge rather than normal Kain struct lookup.
- If the runtime is missing the Python bridge, host-object field access fails loudly instead of silently lying.

Return conversion is best-effort and deliberate:

- `None` becomes Kain `none`
- `bool`, `int`, `float`, and `str` become Kain scalar values
- `bytes` and `bytearray` become Kain arrays of byte-like ints
- Python `list` and `tuple` become Kain arrays/tuples
- Python `dict` with string keys becomes a Kain struct-like value
- NumPy-like array objects with `tolist()` and shape/array metadata can materialize into Kain values
- non-scalar `torch.Tensor`, module objects, classes, scenes, meshes, and other rich foreign objects stay as Python host objects unless you explicitly materialize them

That split is the whole point:

- stay in host-object form when you want to keep calling ecosystem methods
- materialize into Kain-native forms when you need explicit ownership, indexing, mutation, or export policy

One sharp edge to document every time:

- if a Python-exported symbol name collides with a Kain-reserved or awkward identifier, alias it on import

Example:

```kn
from mypyfile import double as py_double

fn main() -> Int:
    return py_double(21)
```

## Python Local Resolution Pattern

Local Python resolution is importer-aware. This is the lane that lets a `.kn`
file sit beside a `.py` file and call it like a normal Python helper.

Search shape:

1. Start from the importing `.kn` file's directory.
2. Walk upward through each ancestor directory.
3. For each directory, also consider its `src/` child.
4. After importer-relative roots, also consider the current working directory and `cwd/src`.
5. For every root, try `module/path.py`.
6. If that misses, try `module/path/__init__.py`.
7. If no local hit is found, fall back to normal Python environment resolution.

When a local hit is found, the runtime does real Python hygiene before import:

- prepend the resolved root to `sys.path`
- evict conflicting `sys.modules` entries for the root package if they came from some other location
- then let CPython import the module normally

That matters because it keeps local sibling modules from being shadowed by some
unrelated installed package with the same name.

Sibling file example:

```text
src/
  mypyfile.py
  myknfile.kn
```

```python
# mypyfile.py
def run(value):
    return value + 1
```

```kn
# myknfile.kn
from mypyfile import run as py_run

fn main() -> Int:
    return py_run(41)
```

Nested local package example:

```text
src/
  main.kn
  tools/
    __init__.py
    mathish.py
```

```python
# tools/mathish.py
def bump(value):
    return value + 1
```

```kn
import tools.mathish as mathish

fn main() -> Int:
    return mathish.bump(41)
```

`from pkg import name` does one more useful thing:

- it first tries `pkg.name` as an attribute on the imported module
- if that attribute does not exist, it falls back to importing `pkg.name` as a nested module

So `from torch.utils import data as torch_data` can bind either an attribute-like
member or a nested module import cleanly.

## Python Bridge And Materialization Pattern

Think of the Python lane as three levels:

1. **Raw `import ...` and host objects:** best when you want natural module/object calls and minimal ceremony.
2. **`std::python::*` wrappers:** best when you want package-shaped authored helpers like `py_numpy_*`, `py_trimesh_*`, `py_torch_*`, `py_bridge_*`.
3. **`std::dcc::*` adapters:** best when images, tensors, and meshes should become Kain-native carriers with explicit ownership and mutation semantics.

Use raw `import` when the package surface itself is the API you want:

```kn
import numpy as np

fn sample() -> Any:
    return np.linspace(0.0, 1.0, 5)
```

Use `std::python::*` when repeated low-level package calls should get a Kain
wrapper surface:

```kn
use std::python::numpy

fn sample() -> Any:
    return py_numpy_linspace(0.0, 1.0, 5)
```

Use `std::dcc::*` when the real question is ownership and native manipulation:

```kn
import numpy as np
use std::dcc::image

fn banner() -> Any:
    let rgba = np.zeros([128, 256, 4], "uint8")
    let img = dcc_image_from_python_auto(rgba)
    dcc_image_set_pixel(img, 12, 12, [255, 120, 40, 255])
    return dcc_image_to_numpy(img)
```

Raw bridge helpers still matter for sharp work:

- `py_import`, `py_import_with_context`
- `py_import_from_with_context`
- `py_call`, `py_call_raw`
- `py_getattr`, `py_getattr_raw`
- `py_setattr`, `py_hasattr`
- `py_eval`, `py_exec`

Use the `*_raw` variants when you explicitly want the result to stay a foreign
host object instead of auto-materializing into a Kain scalar/array/struct-ish
value.

Materialization helpers:

- `kain_image_from_py`, `kain_tensor_from_py`, `kain_geometry_from_py`
- `kain_image_from_py_shared`, `kain_tensor_from_py_shared`, `kain_geometry_from_py_shared`
- `kain_image_from_py_owned`, `kain_tensor_from_py_owned`, `kain_geometry_from_py_owned`
- neutral shared-contract helpers such as `kain_shared_buffer_from_py` and `kain_shared_image_from_py`

Ownership modes mean:

- `shared`: require live zero-copy/shared backing and fail if it cannot be established
- `owned`: force a detached Kain-owned copy
- `auto`: prefer shared when possible, otherwise fall back to owned

Use `shared` when you want mutation to sync back into the Python owner such as a
NumPy array, CPU Torch tensor, or Trimesh mesh arrays.

Use `owned` when you want deterministic detached mutation on the Kain side.

Use `auto` when authored ergonomics matter more than forcing one policy up
front, but still inspect the resulting ownership info if downstream behavior
depends on it.

The info APIs exist so authored code can prove the contract instead of guessing:

- `dcc_image_info(image).ownership`
- `dcc_tensor_info(tensor).ownership`
- `dcc_mesh_info(mesh).vertex_ownership`

If the task is "call a Python function and keep using Python objects," stay in
raw `import` or `py_bridge_*`.

If the task is "bring bytes, pixels, tensor values, or geometry under Kain's
ownership model," graduate into `std::dcc::*`.

## Fabric Python Runtime Pattern

Not every Python interaction should be an embedded import. If an entire Fabric
step is naturally a Python program, use the Python runtime lane directly.

Manifest shape:

```toml
runtime = "python"
entry = "scripts/python_step.py"
```

Expected authored shape:

- the host loads the target `.py` file
- executes it in the Python runtime
- then looks for a `run(fabric_inputs)` entrypoint

Use this pattern when:

- the whole step is already Python-shaped
- the Kain layer only needs to orchestrate inputs/outputs between stages
- the script should live as a real `.py` file instead of a large bridge string

Do not use this when:

- the task is just "I want to call a couple ecosystem APIs from authored Kain"
- a narrow `import ...` plus DCC materialization would keep the flow more Kain-owned

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
- Python packages that already own a domain Kain wants to eat, such as numeric, DCC, simulation, image, or tooling ecosystems.
- Benchmarks that measure boundary cost honestly, such as shared-library call pressure or Python materialization pressure.

Interop is not the right answer for:

- Reimplementing Kain semantics in C or Python.
- Turning a Kain app into a bag of extern or host-object calls.
- Avoiding a real compiler/runtime fix.
- Copying a package bridge shape just because it already runs.

## Kain Facade Pattern

Always wrap the native or foreign vocabulary in Kain vocabulary:

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

Same rule for Python:

```kn
import trimesh
use std::dcc::mesh

fn make_probe_mesh() -> Any:
    let sphere = trimesh.creation.icosphere(2, 1.0)
    return dcc_mesh_from_python_shared(sphere)
```

This keeps agents from sprinkling low-level bridge or host-object calls
throughout a feature. One native package or Python package should feel like one
Kain capability.

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

Python does not use the C FFI report lane, but the same doctrine applies:

- keep the first authored probe tiny
- prove local resolution with a real on-disk `.kn` + `.py` pair
- move to wrappers/materializers only after the import shape itself is solid

## Source Anchors

Use these when you need implementation truth:

- `crates/kain-c-ffi/src/lib.rs`: `use c::` detection, resolution order, automatic runtime-owned headers, cache, bridge loading, native link inputs.
- `crates/kain-c-ffi/src/config.rs`: `[c_ffi]`, library metadata, and interop tiers.
- `crates/kain-c-ffi/src/extract.rs`: header parsing, regex fallback, callable/stubbed report entries, callback and named-type treatment.
- `crates/kain-c-ffi/src/generate.rs`: generated `.kn` module, bridge crate, binding reports, packaged bridge manifests, string and byte-buffer marshaling.
- `crates/kain-c-ffi/src/platform.rs`: `kain import platform`, target locks, generated platform modules, package discovery.
- `crates/kain-foreign-abi/src/lib.rs`: normalized ABI type graph, scalar table, pointer direction/ownership policy, callback metadata.
- `crates/kain-crate-ffi/src/lib.rs`: `use rust::...` and Rust crate FFI bridge generation.
- `crates/kain-core/src/ast.rs`, `crates/kain-core/src/parser.rs`, `crates/kain-core/src/runtime.rs`, `crates/kain-core/src/types.rs`: authored Python `import` syntax, scope registration, and runtime loading behavior.
- `crates/kain-python/src/lib.rs`: embedded Python bridge registration, local import resolution, host-object conversion, shared/owned materializers, and Python runtime helpers.
- `crates/cli/src/import_c.rs`, `crates/cli/src/import_platform.rs`, `crates/cli/src/import_crate.rs`, `crates/cli/src/bridge.rs`: command behavior.
- `runtime/native/include/host_bridge.h` and `runtime/native/src/core/host_bridge.c`: host bridge registry, foreign runtime lanes, service descriptors, bridge contracts.
- `runtime/native/include/platform_library.h` and `runtime/native/src/platform/platform_library.c`: dynamic library handle/status surface.
- `stdlib/platform.kn`, `stdlib/c/bridge.kn`, `stdlib/interop/bridge.kn`, `stdlib/python/bridge.kn`, `stdlib/python/*.kn`, `stdlib/dcc/*.kn`: authored stdlib bridge vocabulary.
- `crates/kain-import/tests/abi_corpus/manifest.json`: C ABI layout cases for pragma pack, explicit alignment, named pack stacks, bitfields, and unions.

Use examples as probes:

- `library_of_kain/ffi_shared_call.kn`: compact shared-library call pressure.
- `benchmark/cases/ffi_shared_call_stress/main.kn`: benchmark version of the shared-call lane.
- `blades/kain-test/fabric_FFI/c_ffi/beacon_math`: scalar, bool, float, string, and intentionally unsupported pointer declarations.
- `blades/kain-test/fabric_FFI/c_ffi/miniaudio_tone_lab`: native audio file generation/probe through a small C bridge.
- `blades/kain-test/fabric_FFI/c_ffi/cgltf_scene_probe`: handle lifecycle over a third-party parser.
- `blades/kain-test/fabric_FFI/python/numpy_supernova`: NumPy image/tensor/point-cloud bridge and export lane.
- `blades/kain-test/fabric_FFI/python/trimesh_glb`: Trimesh scene/mesh/export lane.
- `blades/kain-test/fabric_FFI/python/pygame_poster`: Pygame-to-image bridge lane.
- `blades/kain-test/fabric_FFI/fabric/polyglot_local`: Fabric whole-file Python runtime lane.

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

For authored Python import work:

1. Decide whether the right shape is raw `import ...`, `std::python::*`, `std::dcc::*`, or Fabric `runtime = "python"`.
2. Prove the import shape first with the smallest real on-disk case.
3. If local resolution matters, use a real sibling `.kn` + `.py` or package directory with `__init__.py`.
4. If the result is supposed to stay foreign, keep it as a host object and prove the member/call path.
5. If the result is supposed to become Kain-owned, materialize it and inspect ownership metadata.
6. If the claim includes zero-copy sync, mutate one side and prove visibility on the other side.

For low-level memory or handle math:

- Use Z3 when pointer span, generation bits, capacity, packing, or ring arithmetic is part of the safety claim.
- Use `test-crash-forensics` if the native executable crashes or hangs.
- Use `test-bench` if the claim is boundary cost or native speed.

## Hand Off When

- Use `lang-systems` when the interop code is mostly raw memory, ownership, actor pressure, effects, zero-copy, async, or unsafe authored Kain.
- Use `lang-semantics` when interop is being fused with worlds, laws, patches, converge, pulse, teleport, components, or shader/compute semantics.
- Use `lang-stdlib` when the right answer is to consume or extend an existing public `std.*` domain rather than bind a native library or host package directly.
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
- Documenting Python support as "Kain has Python imports now" without explaining runtime scope, local resolution, ownership, and host-object behavior.
- Forcing Python ecosystem access through invented `use python::...` syntax instead of the real `import ...` lane.
- Pretending Python modules are static Kain modules with static types.
- Assuming sibling `.py` support is real without proving it with a real file-on-disk smoke.
- Expecting `from pkg import *` to work in this lane.
- Forgetting aliases when imported Python names collide with Kain syntax or read badly in authored code.
- Using raw host objects forever when the actual requirement is explicit Kain-owned image/tensor/geometry state.

## Final Taste Check

A good Kain interop result should read like:

```text
Kain policy and semantics are obvious.
Native code is a tight, boring contract.
Python is a real ecosystem lane, not a fake static module tree.
Ownership and teardown are named.
The smallest smoke proves the boundary.
The larger benchmark/package/attrition lane is available when the claim needs it.
```

If an agent reads this skill and still writes "just check Vulkain" or "yup Kain
has Python imports lol," it missed the point. Interop is a design lane, not a
scavenger hunt.
