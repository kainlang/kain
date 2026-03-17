# Python Stdlib Wrappers

This folder is the first-party wrapper layer over Kain's embedded Python bridge.

The design goal is simple:

- `std::python::*` exposes Python ecosystem access in a Kain-shaped surface
- raw bridge calls still exist, but they are the escape hatch rather than the main story
- wrapper names are intentionally prefixed (`py_numpy_*`, `py_trimesh_*`, `py_bridge_*`) because imported stdlib symbols currently flatten into scope

## Modules

- `bridge.kn`: low-level Python access, module availability checks, raw calls, attribute access
- `numpy.kn`: NumPy discovery, basic constructors, and image/tensor/geometry conversion helpers
- `torch.kn`: PyTorch discovery plus tensor-focused helpers
- `trimesh.kn`: Trimesh discovery, primitive builders, geometry conversion
- `pygame.kn`: Pygame discovery plus image conversion helpers

## Ownership Model

These wrappers are paired with the DCC adapters in `std::dcc::*`.

- `shared`: require live shared backing and fail if zero-copy cannot be established
- `owned`: force a detached Kain-owned copy
- `auto`: prefer shared, otherwise fall back to owned

Use `shared` when you want in-place sync with Python owners such as `numpy.ndarray`, CPU `torch.Tensor`, or `trimesh` mesh arrays.

Use `owned` when you want deterministic detached mutation on the Kain side.

## Typical Flow

```kain
use std::python::bridge
use std::python::numpy
use std::dcc::image

let rgba = py_bridge_call_raw("build_rgba", [512, 320])
let img = dcc_image_from_python_shared(rgba)
let info = dcc_image_info(img)
dcc_image_set_pixel(img, info.width / 2, info.height / 2, [255, 120, 40, 255])
let exported = dcc_image_to_numpy(img)
```

## Current Rule

If you are writing lots of raw `py_exec("...")` in application code, you are still too low in the stack.

The preferred direction is:

1. put low-level Python kernels behind a narrow bridge function
2. materialize into `std::dcc::*` containers or views
3. keep most pipeline logic in Kain
