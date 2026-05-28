---
name: lang-python
description: >-
  Use when authoring, explaining, reviewing, or repairing Kain-side Python
  interop: first-class `import ...` and `from ... import ...`, local sibling
  `.py` or package resolution, `use std::python`, Python host objects, shared
  or owned materialization into Kain images/tensors/buffers/geometry, CPython
  runtime behavior, and Python-package facades without changing compiler or
  runtime bridge internals. If the task is mainly `use c::...`, DLLs, platform
  packages, or native bridge metadata, use `lang-c-abi` instead; load both
  when Python and native ABI work are fused.
---

# Lang Python

Use this skill when Kain code crosses into live Python modules, local `.py`
helpers, or Python-owned objects.

The goal is not "pretend Python modules are Kain modules." The goal is:

```text
Kain owns semantic policy, state, orchestration, ownership, validation, and app shape.
Python owns package ecosystems, dynamic objects, scientific/image/tooling runtimes,
embedded interpreters, and unavoidable host-language surfaces.
```

If the boundary also includes `use c::...`, `[c_ffi]`, DLL loaders, or platform
packages, co-read `lang-c-abi` and let the center of gravity decide which skill
to open first.

## Trigger Shape

Use this skill for:

- Kain source with Python imports such as `import numpy as np`, `import fastmcp as fastmcp`, `from python_lab.bridge import tensor_signature as py_tensor_signature`, or `import python_lab.bridge as py_lab`.
- Kain source with `use std::python`, `use std::interop`, `python_*`, `py_*`, or `kain_*_from_py` helpers.
- A task that mentions Python packages, local `.py` helpers, CPython behavior, host objects, materialization, NumPy, Torch, FastMCP, DCC libraries, or Python-owned buffers/images/tensors/geometry.
- Designing the authored Kain facade around a Python package.
- Deciding whether a boundary should be raw `import ...`, `use std::python`, `use std::interop`, or explicit `kain_*_from_py` helpers.
- Reviewing whether a Python boundary has correct status, lifetime, host-object, materialization, aliasing, ownership, and teardown shape.
- Explaining why Kain can touch the Python ecosystem without letting Python own the application.

Do not use this skill for compiler/runtime implementation changes. If Python
parsing, scope registration, local import resolution, host-object dispatch, or
the embedded runtime is broken underneath, hand off to the owning
bootstrap/runtime skill.

## The Python Boundary Contract

Every Python boundary needs five answers before code gets clever:

- **Who owns the bytes?** Kain-owned copy, Python-owned object, shared zero-copy view, typed image/tensor/geometry container, or opaque host object.
- **Who owns lifetime?** One-call borrow, explicit shared handle, Kain `collapse`/`observe`/`decay`, Python-owner-backed view, or runtime-held module/object cache.
- **Who owns failure?** Import failure, Python exception, missing attribute, null-ish host object, explicit `Result`, assertion failure, or benchmark mismatch.
- **Who owns policy?** Kain should own the semantic decision. Python should do the thing only Python or its package ecosystem can do.
- **Who owns live runtime state?** The embedded Python scope, imported module cache, host object graph, or a Kain-side session/facade.

If those answers are fuzzy, the code will work once and rot.

## Why Kain Can Do This

Kain has several layers that make Python interop more than a dumb foreign call:

- Top-level Python `import ...` and `from ... import ...` bind into the embedded Python runtime rather than pretending Python is a static Kain module tree.
- Local Python files and packages can resolve relative to the importing `.kn` file before falling back to the active Python environment.
- Imported Python names are registered in the Kain type environment as dynamic/unknown bindings, so authored code can use them without lying about static Kain types.
- `stdlib/python.kn` and `stdlib/interop.kn` expose bridge vocabulary instead of forcing every project into one-off host shims.
- The registered `kain_*_from_py` and `kain_*_to_py` builtins expose shared-buffer/image/tensor/geometry lanes with explicit ownership policy.
- The embedded Python scope is persistent for the Kain execution, which means repeated `import`, `py_exec`, `py_call`, and host-object access share one live Python world instead of cold-starting on every call.

That stack means agents should not treat Python as one-off glue. The authored
Kain surface should look like a Kain API with the Python boundary hidden
underneath.

## Boundary Decision Flow

Use this order:

1. **Public stdlib wrapper:** if `std::python`, `std::interop`, or another root `std.*` surface already expresses the need, author against `std.*` first.
2. **First-class Python import lane:** if the task is "I want to use a Python module like a Python module," prefer `import ...` or `from ... import ...` in authored Kain instead of inventing `use python::...`.
3. **Python bridge/materialization lane:** if the task needs explicit bridge calls, attribute dispatch, or shared-vs-owned data control, move from raw `import ...` to `use std::python`, `use std::interop`, and the `kain_*_from_py` helpers.
4. **Kain facade:** once the import shape is proven, wrap the package vocabulary in a small Kain function/module so the rest of the app does not speak raw host-object dialect.
5. **Mixed native boundary:** if the Python package also hides DLL, C ABI, platform SDK, or bridge-metadata work, co-trigger `lang-c-abi`.
6. **Runtime/compiler handoff:** if the authored shape is good but import/lowering/loading/host-object dispatch is broken, stop blaming the Kain file and fix the substrate with the owner skill.

## Fast Discovery

```powershell
rg -n "^import |^from .* import |py_import|py_call|py_getattr|kain_(image|tensor|geometry|shared_(buffer|image))_from_py|python_(module_available|require_module)" . agents blades benchmark smoketest stdlib crates
rg -n "numpy|fastmcp|torch|python_lab|pyglet|mypyfile" blades smoketest stdlib crates .agents
rg -n "shared_buffer|shared_image|shared_tensor|shared_geometry|python_region_|python_buffer_view_" stdlib runtime/native/include runtime/native/src crates/python
rg --files | rg "(python|interop|host_bridge|dcc)"
```

If the task also includes `use c::...`, `[c_ffi]`, `kain import-c`, or platform
locks, open `lang-c-abi` too instead of forcing one skill to explain both
halves.

## Command Loops

Authored Kain validation:

```powershell
kain check smoke.kn --target llvm
kain build smoke.kn --target llvm -o .kain/run/smoke.exe
kain run smoke.kn --target llvm
```

Python import and bridge validation:

```powershell
cargo test -p kain-core parses_python_import_items -- --nocapture
cargo test -p kain-python python_import_supports_local_sibling_from_imports -- --nocapture
cargo test -p kain-python python_import_supports_local_dotted_module_alias_calls -- --nocapture
cargo test -p kain-python python_bridge_exec_scope_persists_between_calls -- --nocapture
```

Use the smallest real on-disk `.kn` + `.py` proof first, then graduate to a
blade, benchmark, or attrition lane only when the claim requires it.

## Python Import Lane

Python is a first-class interop lane in authored Kain. Use `import` for Python
modules and `use` for Kain/native/static lanes.

Supported authored forms:

- `import numpy`
- `import numpy as np`
- `import fastmcp as fastmcp`
- `import pkg.subpkg.module`
- `import pkg.subpkg.module as alias`
- `from torch.utils import data`
- `from torch.utils import data as torch_data`
- `from python_lab.bridge import tensor_signature as py_tensor_signature`
- `from mypyfile import run as py_run`

Not supported in this lane:

- `from pkg import *`

Important authored rule:

- Kain passes the exact module string to Python. There is no blessed allowlist.
- If CPython can import the module from the local search roots or active Python environment, Kain can bind it.
- Real package names matter. If the environment exposes `torch`, use `import torch`. If it exposes `fastmcp`, use `import fastmcp as fastmcp`. If it exposes some other module name, import that exact name.

Binding semantics match Python closely:

- `import numpy` binds `numpy`
- `import numpy as np` binds `np`
- `import fastmcp as fastmcp` binds `fastmcp`
- `import pkg.tools.mathish` binds `pkg` unless you alias it
- `import pkg.tools.mathish as mathish` binds `mathish`
- `from python_lab.bridge import tensor_signature as py_tensor_signature` binds `py_tensor_signature`
- `from torch.utils import data as torch_data` binds `torch_data`

That means the authored default for deep module paths should usually be an
alias:

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

- `np.linspace(...)`, `fastmcp.FastMCP(...)`, `py_lab.make_numpy_grid(...)`, and `runner(...)` use normal Kain `.` and call syntax.
- If the value is a Python-backed host object, field access routes through the Python bridge rather than normal Kain struct lookup.
- If the runtime is missing the Python bridge, host-object field access fails loudly instead of silently lying.

Return conversion is best-effort and deliberate:

- `None` becomes Kain `none`
- `bool`, `int`, `float`, and `str` become Kain scalar values
- `bytes` and `bytearray` become Kain arrays of byte-like ints
- Python `list` and `tuple` become Kain arrays/tuples
- Python `dict` with string keys becomes a Kain struct-like value
- NumPy-like array objects with `tolist()` and shape/array metadata can materialize into Kain values
- non-scalar `torch.Tensor`, module objects, classes, `FastMCP` apps, and other rich foreign objects stay as Python host objects unless you explicitly materialize them

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
2. **`use std::python`:** best when you need explicit bridge calls, module checks, attribute dispatch, or deliberate raw-vs-materialized return control.
3. **Ownership helpers:** best when tensors, images, geometry, or shared contracts should become Kain-owned or explicitly shared through `python_*`, `interop_*`, and `kain_*_from_py`.

Use raw `import` when the package surface itself is the API you want:

```kn
import numpy as np

fn sample() -> Any:
    return np.linspace(0.0, 1.0, 5)
```

Use `use std::python` when repeated low-level foreign calls should get a small
Kain surface:

```kn
use std::python

fn sample() -> Any:
    let fastmcp = python_require_module("fastmcp")
    let app = python_call_attr_raw(fastmcp, "FastMCP", ["kain-python-lab"])
    return python_getattr(app, "name")
```

Use ownership helpers when the real question is mutation and lifetime:

```kn
import numpy as np
use std::python

fn banner() -> Any:
    let image = python_image_shared(np.zeros([128, 256, 4], "uint8"))
    kain_image_set_pixel(image, 12, 12, [255, 120, 40, 255])
    return python_image_to(image, "numpy")
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
NumPy array or CPU Torch tensor.

Use `owned` when you want deterministic detached mutation on the Kain side.

Use `auto` when authored ergonomics matter more than forcing one policy up
front, but still inspect the resulting ownership info if downstream behavior
depends on it.

The info APIs exist so authored code can prove the contract instead of guessing:

- `kain_image_info(image).ownership`
- `kain_tensor_info(tensor).ownership`
- `interop_shared_buffer_info(handle).ownership`
- `interop_shared_image_info(handle).ownership`

If the task is "call a Python function and keep using Python objects," stay in
raw `import` or `use std::python`.

If the task is "bring bytes, pixels, tensor values, or geometry under Kain's
ownership model," graduate into `use std::python`, `use std::interop`, and the
`kain_*_from_py` helpers.

## Python Callback And Event Pump Pattern

Callbacks are where the Python lane goes feral. Keep the host side boring:

```text
python package event source
    -> Python callback or poll method records compact state
    -> Kain facade reads the result or host object
    -> Kain actor/world owns policy
```

Prefer:

- polling or explicit tick methods over hidden long-lived callbacks
- compact status or event records instead of direct mutation of Kain state
- one Kain facade that hides repeated `python_call_attr_raw` plumbing

Avoid:

- Python callbacks mutating Kain world state directly from foreign control flow
- sprinkling raw host-object calls through unrelated modules
- relying on implicit event-loop ownership without naming who pumps it

If the package also depends on native callbacks or platform event sources,
co-trigger `lang-c-abi`.

## Python Use Cases

Python is the right answer for:

- Numeric, image, simulation, DCC, scientific, MCP, ML, and tooling ecosystems that already exist and are expensive to re-create.
- Local helper modules that are naturally expressed as `.py` siblings during a package or smoketest proof.
- Host-object retention when Kain wants to orchestrate a package rather than copy every value back immediately.
- Shared or owned materialization benchmarks that honestly measure the Kain-to-Python boundary.

Python is not the right answer for:

- Reimplementing Kain semantics in Python.
- Turning a Kain app into a bag of raw host-object calls.
- Avoiding a real compiler/runtime fix.
- Using Python when the real boundary is a native ABI package that `lang-c-abi` should own.

## Kain Facade Pattern

Always wrap Python vocabulary in Kain vocabulary:

```kn
import fastmcp as fastmcp
import python_lab.bridge as py_lab
use std::python

fn launch_probe(plan_text: String) -> String:
    let app = python_call_attr_raw(fastmcp, "FastMCP", ["kain-python-lab"])
    let _grid = py_lab.make_numpy_grid(plan_text, 7)
    return python_getattr(app, "name")
```

This keeps agents from sprinkling low-level bridge or host-object calls
throughout a feature. One Python package should feel like one Kain capability.

## Tiny Probe Doctrine

Python does not use the C FFI report lane, so make your own tiny probes:

- keep the first authored probe tiny
- prove local resolution with a real on-disk `.kn` + `.py` pair
- move to wrappers/materializers only after the import shape itself is solid
- if the package also needs native import locks, platform SDK setup, or `use c::...`, switch to `lang-c-abi` for that half

## Source Anchors

Use these when you need implementation truth:

- `crates/core/src/ast.rs`, `crates/core/src/parser.rs`, `crates/core/src/runtime.rs`, `crates/core/src/types.rs`: authored Python `import` syntax, scope registration, and runtime loading behavior.
- `crates/python/src/lib.rs`: embedded Python bridge registration, local import resolution, host-object conversion, shared/owned materializers, and Python runtime helpers.
- `crates/cli/src/bridge.rs`: bridge command behavior.
- `runtime/native/include/host_bridge.h` and `runtime/native/src/core/host_bridge.c`: host bridge registry, foreign runtime lanes, service descriptors, bridge contracts.
- `stdlib/python.kn`, `stdlib/interop.kn`: authored stdlib bridge vocabulary.
- `blades/python`: canonical first-class Python import lab using `import numpy`, `import fastmcp`, local `python_lab.bridge`, and Kain world/teleport/ownership semantics over Python-backed tensors.
- `crates/python/src/lib.rs`: executable source snippets for zero-copy NumPy/Torch materialization, shared-vs-owned mutation, and export back into Python objects.
- `smoketest/src/stdlib/interop_lane.kn`: shared-contract vocabulary and stdlib integration pressure.

Use `lang-c-abi` when the truth you need lives in `crates/c-ffi`,
`crates/foreign-abi`, `runtime/native/include/platform_library.h`, or
`kain import-c` / `kain import platform` flows.

## Validation Ladder

For authored Python import work:

1. Decide whether the right shape is raw `import ...`, `use std::python`, `use std::interop`, or direct `kain_*_from_py` helpers.
2. Prove the import shape first with the smallest real on-disk case.
3. If local resolution matters, use a real sibling `.kn` + `.py` or package directory with `__init__.py`.
4. If the result is supposed to stay foreign, keep it as a host object and prove the member/call path.
5. If the result is supposed to become Kain-owned, materialize it and inspect ownership metadata with `kain_tensor_info`, `kain_image_info`, or `interop_shared_*_info`.
6. If the claim includes zero-copy sync, mutate one side and prove visibility on the other side.
7. Run the package/benchmark/attrition lane only when the change claims package health, performance, or long-horizon runtime cleanliness.

For low-level memory or mixed-boundary math:

- Use Z3 when pointer span, capacity, ownership bits, or queue math is part of the safety claim.
- Use `test-crash-forensics` if the native executable crashes or hangs.
- Use `test-bench` if the claim is Python boundary cost or materialization speed.
- Use `lang-c-abi` if the bug is really in a native bridge or platform loader path.

## Hand Off When

- Use `lang-c-abi` when the task is really `use c::...`, `[c_ffi]`, `kain import-c`, DLLs, platform packages, `use rust::...`, or mixed Python-plus-native package wiring.
- Use `lang-systems` when the code is mostly raw memory, ownership, actor pressure, effects, zero-copy, async, or unsafe authored Kain.
- Use `lang-semantics` when Python work is being fused with worlds, laws, patches, converge, pulse, teleport, components, or shader/compute semantics.
- Use `lang-stdlib` when the right answer is to consume or extend an existing public `std.*` domain rather than bind a Python package directly.
- Use `lang-translation` when the task starts as Python logic and should become idiomatic Kain.
- Use `bootstrap-core` when parser/type/lowering/import semantics are wrong.
- Use `runtime-core` when host bridge, handles, scheduler interaction, or native runtime startup/shutdown is wrong.
- Use `runtime-stdlib` when the public stdlib wrapper or runtime-backed Python domain is wrong.
- Use `runtime-gpu` or `bootstrap-gpu` when the Python boundary is only incidental to GPU executor/codegen internals.
- Use `package-kaintana` or `package-vulkain` only for those package-owned surfaces.
- Use `tool-build-system` when Bazel, launcher shims, or build provenance are the blocker.

## Anti-Patterns

- Documenting Python support as "Kain has Python imports now" without explaining runtime scope, local resolution, ownership, and host-object behavior.
- Forcing Python ecosystem access through invented `use python::...` syntax instead of the real `import ...` lane.
- Pretending Python modules are static Kain modules with static types.
- Assuming sibling `.py` support is real without proving it with a real file-on-disk smoke.
- Expecting `from pkg import *` to work in this lane.
- Forgetting aliases when imported Python names collide with Kain syntax or read badly in authored code.
- Using raw host objects forever when the actual requirement is explicit Kain-owned image/tensor/geometry state.
- Routing first-class Python imports through dead bridge folders or one-off host harnesses when a local `.py` module plus `import ...` already expresses the boundary.
- Using Python to hide a compiler/runtime bug.
- Smuggling performance-critical data through JSON host glue when a typed shared buffer/image/tensor lane exists.

## Final Taste Check

A good Kain Python result should read like:

```text
Kain policy and semantics are obvious.
Python is a real ecosystem lane, not a fake static module tree.
Host-object vs materialized ownership is named.
The smallest on-disk probe proves the boundary.
The larger benchmark/package/attrition lane is available when the claim needs it.
```

If an agent reads this skill and still writes "yup Kain has Python imports lol,"
it missed the point. Python is a design lane, not a scavenger hunt.
