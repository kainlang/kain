---
name: lang-python
description: >-
  Use when authoring, explaining, reviewing, or repairing Kain-side Python
  interop: first-class `import ...` and `from ... import ...`, local sibling
  `.py` and package resolution, natural Python host-object member/call syntax
  with named Kain args lowered to Python kwargs, `std::python` and
  `std::interop` bridge helpers, package facades for NumPy/Torch/Pandas/PIL/Qt/
  Flet/FastMCP-style ecosystems, Python dependency/deployment truth, region
  caches, async futures, actor callbacks, shared or owned buffer/image/tensor/
  geometry materialization, GPU tensor/storage-buffer contracts, and Kain-owned
  app architecture over Python libraries. If the task is mainly C/Rust/DLL/
  platform-package ABI metadata, use `lang-c-abi`; if parser/runtime bridge
  semantics are broken, use the owning bootstrap/runtime skill.
---

# Lang Python

Use this skill when Kain touches live Python packages, local `.py` helpers,
Python-owned objects, package facades, shared/owned materialization, async/event
flows, or GPU/data bridges.

The doctrine is simple:

```text
Kain owns app semantics, state, policy, ownership, validation, and speed.
Python owns package ecosystems and dynamic host objects.
```

Do not make Python feel like a foreign runtime unless the boundary truly needs
foreign-runtime ceremony. Prefer the natural lane first:

```kn
import json as py_json
import numpy as np

fn main() -> Any:
    let packed = py_json.dumps([1, 2], separators = [",", ":"])
    return np.linspace(start = 0.0, stop = 1.0, num = 5)
```

Named Kain call args on Python host objects are real Python kwargs. That is
current compiler/runtime truth, not facade wishful thinking.

## Start Here

Use this decision order:

1. **Raw Python import:** use `import numpy as np`, `import fastmcp as fastmcp`, or `from PIL import Image as py_image` when the Python package surface is already the API you want.
2. **Natural host-object calls:** call imported modules, classes, bound methods, and keyword-only callables with normal Kain `.` and call syntax, including named args.
3. **Kain facade:** wrap repeated Python vocabulary in a small Kain module/function so app code speaks Kain policy, not bridge plumbing.
4. **`std::python` / `std::interop`:** drop to explicit bridge helpers when you need raw-vs-materialized returns, attr/cache control, async futures, actor callbacks, or ownership inspection.
5. **Shared/owned adoption:** use `kain_*_from_py`, `python_tensor_*`, `python_image_*`, and `interop_shared_*` helpers when bytes, pixels, tensors, or geometry must enter Kain's ownership model.
6. **Runtime/compiler handoff:** if the authored shape is right but import, local resolution, kwargs, host-object dispatch, or diagnostics are wrong, stop patching Kain source and use `bootstrap-core`, `runtime-core`, or `runtime-stdlib`.

Load deeper references only when needed:

- [references/capability-map.md](references/capability-map.md): runtime seams, helper families, imports, kwargs, region/async/buffer/GPU surfaces.
- [references/facades-and-patterns.md](references/facades-and-patterns.md): direct imports, sibling `.py`, tiny bootstraps, package facades, dependency truth, event loops.
- [references/validation-and-anchors.md](references/validation-and-anchors.md): focused tests, benchmark lanes, source anchors, handoff rules, anti-patterns.

## Boundary Contract

Before clever code, answer these:

- **Bytes:** Kain-owned copy, Python-owned object, shared zero-copy view, typed image/tensor/geometry, or opaque host object?
- **Lifetime:** one-call borrow, explicit shared handle, host-object retention, Kain `collapse`/`observe`/`decay`, or Python owner-backed view?
- **Failure:** import error, missing attr, Python exception, dtype/shape mismatch, non-contiguous buffer, ownership mismatch, event-loop mistake, or explicit status object?
- **Policy:** what decision belongs to Kain, and what capability belongs to Python?
- **State:** persistent embedded Python scope, imported module cache, local helper module, facade/session object, or Kain world/actor state?

If those are fuzzy, the interop will work once and rot.

## Modern Pipeline Truth

Python is first-class authored interop:

- `import ...` and `from ... import ...` route into CPython, not Kain's static module tree.
- Local `.py` files and packages resolve relative to the importing `.kn` file before ambient environment resolution.
- Imported Python names bind as dynamic host objects in Kain so normal member access and calls work.
- Named Kain args on Python host-object calls lower to Python kwargs through the bridge.
- Python exceptions now need Kain-shaped diagnostics naming package, symbol, call target, and suggested fix where the substrate has context.
- The embedded Python scope persists across `import`, `py_exec`, `py_eval`, raw bridge calls, and host-object access for the execution.

Use `import` for Python packages. Do not invent `use python::...`.

Supported import shapes:

- `import numpy`
- `import numpy as np`
- `import pkg.subpkg.module as mod`
- `from torch.utils import data as torch_data`
- `from mypyfile import run as py_run`

Unsupported:

- `from pkg import *`

For deep modules, alias by default:

```kn
import tools.mathish as mathish

fn main() -> Int:
    return mathish.bump(41)
```

## Facade Rule

Facades are not a workaround for bad interop. They are how Kain owns policy
while Python supplies library capability.

Use a facade when:

- multiple files would otherwise repeat raw bridge calls
- Python package naming leaks into app policy
- constructor/kwargs setup is noisy
- ownership/materialization needs one canonical contract
- package dependency checks should fail early with a clear Kain diagnostic

Facade shape:

```kn
import pandas as pd

fn read_sales_csv(path: String) -> Any:
    return pd.read_csv(path, dtype = {"sku": "string"})
```

Keep the facade thin. Do not fake static typing for all Python. Give Kain a
stable, semantic package-facing surface and let rich Python objects stay rich
until Kain needs ownership or materialization.

Package families that deserve canonical facade patterns:

- NumPy, Torch, Pandas, PIL/Pillow
- Qt/PySide, Tkinter, Flet, Pygame, Pyglet
- FastMCP and decorator-heavy server/tooling packages
- sounddevice, pyqtgraph, OpenCV, DCC/scientific ecosystems

If a future facade package manager appears, this skill should treat it as a
distribution lane for these Kain-authored facade modules, not as a replacement
for natural `import ...`.

## Dependency Truth

Do not rely on ambient Python luck for serious projects. When a task is about
"works every time" package imports, look for or add project-owned dependency
truth in the appropriate Kain project/package surface.

Until the repo has one blessed facade/dependency package manager, record:

- exact Python import names, which may differ from pip package names
- package version expectations when behavior is version-sensitive
- local helper module roots and whether they shadow installed packages
- runtime environment expectations for GPU, Qt, Torch, FastMCP, or native wheels
- suggested install/fix text in diagnostics or project docs

If the task is implementing the dependency manager itself, this skill is only
the language-side doctrine; use the owning tool/build/package skills too.

## Authoring Taste

Good Kain Python code reads like:

```text
Kain policy is obvious.
Python is a library reservoir, not the app brain.
The boundary shape is explicit: direct import, facade, sibling .py, tiny bootstrap, region cache, async callback, or owned/shared adoption.
Host-object retention versus Kain-owned materialization is named.
Validation proves the boundary with a real file, package, benchmark, or visible/tool surface.
```

Prefer:

- direct imported host objects for simple package APIs
- Kain facades for repeated package vocabulary
- sibling `.py` helper modules for substantial Python syntax, decorators, or closures
- tiny `python_exec` only for narrow bootstrap glue
- region caches for hot module/attr/call loops
- explicit shared/owned helpers for bytes, images, tensors, geometry, and GPU storage

Avoid:

- giant string-built Python apps hidden inside `python_exec`
- raw bridge calls sprayed through app code
- pretending Python modules are statically typed Kain modules
- importing packages at top level and then ignoring them while a bootstrap string owns the feature
- using Python to hide compiler/runtime bugs
- using JSON glue when typed shared buffer/image/tensor lanes exist

## Quick Validation

For authored import/facade work, start small:

```powershell
kain check smoke.kn --target llvm
kain run smoke.kn --target llvm
```

For bridge/compiler behavior, use focused Rust tests before broad suites:

```powershell
cargo test -p kain-core parses_python_import_items -- --nocapture
cargo test -p kain-python python_import_supports_local_sibling_from_imports -- --nocapture
cargo test -p kain-python python_imported_host_object_calls_accept_named_kwargs -- --nocapture
cargo test -p kain-python python_callable_host_objects_accept_keyword_only_args -- --nocapture
cargo test -p kain-python python_host_object_call_errors_name_the_symbol -- --nocapture
```

For current benchmark truth:

```powershell
python benchmark/run.py --case python_interop --languages kain
python benchmark/run.py --case python_stdlib_fused --languages kain
```

On this Windows checkout, use a roomy `--target-dir` such as `X:\target\<lane>`
if `Z:\_b` is full; if a linker race appears, rerun focused Cargo tests
sequentially.

## Hand Off

- Use `bootstrap-core` for parser, AST, import binding, type environment, runtime interpreter dispatch, and natural kwargs lowering bugs.
- Use `runtime-core` or `runtime-stdlib` for embedded runtime, host bridge, handles, startup/shutdown, or public stdlib helper bugs.
- Use `lang-c-abi` for C/Rust/DLL/platform-package/native wheel boundaries.
- Use `lang-semantics` when Python is fused with worlds, actors, ownership, patches, converge, shaders, or app semantics.
- Use `lang-stdlib` when the right answer is public root `std.*` vocabulary.
- Use `test-bench` for Python boundary cost and materialization speed.
- Use `test-crash-forensics` if a native Kain executable crashes or hangs.

If the result still feels like "Kain with a Python escape hatch," tighten the
facade or fix the substrate. The ceiling is Kain-native ergonomics with Python
ecosystem reach.
