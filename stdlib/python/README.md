# Python Interop

Kain now has two real Python lanes:

- first-class `import ...` / `from ... import ...` for the Python ecosystem
- `use std::python` for the generic embedded-Python bridge and materialization helpers

This folder keeps optional package-specific convenience wrappers for a few
popular libraries, but those files are not the core reason Python works in
Kain. The actual substrate lives in `kain-python` plus the root `std::python`
surface.

## Preferred Shape

Reach for the layers in this order:

1. `import numpy as np`, `import trimesh`, `import mypyfile`, or `from torch import nn`
2. `use std::python` when you need explicit bridge calls, module checks, or generic Python-to-Kain materialization
3. `use std::python::{numpy,torch,trimesh,pygame}` only when the package-specific helper names are genuinely useful
4. `use std::python::bridge` only for older code that has not been migrated yet

## What Lives Where

- `stdlib/python.kn`: the primary generic surface
- `stdlib/python/bridge.kn`: legacy compatibility surface around the older `py_bridge_*` names
- `stdlib/python/numpy.kn`: optional NumPy convenience helpers
- `stdlib/python/torch.kn`: optional torch convenience helpers
- `stdlib/python/trimesh.kn`: optional trimesh convenience helpers
- `stdlib/python/pygame.kn`: optional pygame convenience helpers

## Root Surface Example

```kain
use std::python
use std::dcc::image

python_exec("import numpy as np\n\ndef build_rgba(width, height):\n    xs = np.linspace(0, 255, width, dtype=np.uint8)\n    ys = np.linspace(24, 216, height, dtype=np.uint8)\n    image = np.zeros((height, width, 4), dtype=np.uint8)\n    image[:, :, 0] = xs[np.newaxis, :]\n    image[:, :, 1] = ys[:, np.newaxis]\n    image[:, :, 2] = ((xs[np.newaxis, :] // 2) + (ys[:, np.newaxis] // 3)).astype(np.uint8)\n    image[:, :, 3] = 255\n    return image\n")

let rgba = python_call_raw("build_rgba", [512, 320])
let img = dcc_image_from_python_shared(rgba)
let info = dcc_image_info(img)
dcc_image_set_pixel(img, info.width / 2, info.height / 2, [255, 120, 40, 255])
let exported = python_image_to(img, "numpy")
```

## Ecosystem Import Example

```kain
import numpy as np
import trimesh
import mypyfile

fn main():
    let xs = np.linspace(-1.0, 1.0, 8)
    let mesh = trimesh.creation.box(extents = [1.0, 2.0, 3.0])
    let score = mypyfile.evaluate(xs, mesh)
    return score
```

Local sibling and package imports resolve relative to the importing `.kn` file
before falling back to the active Python environment, so `mypyfile.py` next to
`main.kn` is a valid import target.

## Ownership Model

The generic `std::python` materializers are paired with `std::dcc::*`.

- `python_image` / `python_tensor` / `python_geometry`: prefer shared when possible, otherwise fall back to owned
- `python_image_shared` / `python_tensor_shared` / `python_geometry_shared`: require live shared backing
- `python_image_owned` / `python_tensor_owned` / `python_geometry_owned`: force a detached Kain-owned copy

Use `shared` when you want in-place sync with Python owners such as
`numpy.ndarray`, CPU `torch.Tensor`, or `trimesh` mesh arrays.

Use `owned` when you want deterministic detached mutation on the Kain side.

## Why The Package Files Exist

The hardcoded package wrappers came from the pre-`import` era when the bridge
needed a few obvious examples and convenience entry points. They still have
value for:

- fast availability checks such as `py_numpy_available()`
- common constructors such as `py_numpy_zeros()` or `py_trimesh_box()`
- obvious backend-specific conversions like `py_torch_from_tensor()`

But they are convenience layers, not the architecture. If a package is not
listed here, that does not mean Kain cannot use it. It usually means the raw
Python import lane is the right interface already.

## Current Rule

If you are writing lots of inline `python_exec("...")` in application code, you
are still too low in the stack.

The healthy direction is:

1. use first-class `import ...` for real Python modules
2. materialize into `std::dcc::*` containers or views when Kain should own the next stage
3. keep most orchestration and app semantics in Kain
