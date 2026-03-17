# DCC Stdlib Wrappers

This folder is the Kain-native semantic layer over Python-backed DCC payloads.

`std::python::*` gets data and objects into the runtime.

`std::dcc::*` is where the data starts feeling like Kain instead of foreign Python state.

## Modules

- `image.kn`: image materialization, info, pixel reads and writes, export to NumPy or Torch
- `tensor.kn`: tensor materialization, shape metadata, indexed reads and writes, export to NumPy or Torch
- `mesh.kn`: geometry materialization, vertex/face access, mutation, export to Trimesh or dict

## Ownership Contract

Every adapter follows the same explicit contract:

- `*_from_python_shared(...)`: require zero-copy shared backing
- `*_from_python_owned(...)`: force a detached native copy
- `*_from_python_auto(...)`: prefer shared, fall back to owned

The info APIs report the resulting ownership so code can make deliberate decisions:

- `dcc_image_info(image).ownership`
- `dcc_tensor_info(tensor).ownership`
- `dcc_mesh_info(mesh).vertex_ownership`

## Why This Layer Exists

Without this layer, Kain would still read like "Python strings in a trench coat."

With this layer:

- images, tensors, and meshes have explicit ownership
- mutations are expressed in Kain
- backend sync behavior is inspectable
- Python becomes the ecosystem substrate instead of the authoring model

## Example

```kain
use std::python::trimesh
use std::dcc::mesh

let sphere = py_trimesh_icosphere(3, 1.0)
let geo = dcc_mesh_from_python_shared(sphere)
let v0 = dcc_mesh_vertex_at(geo, 0)
dcc_mesh_set_vertex(geo, 0, [v0[0], v0[1] + 0.25, v0[2]])
let exported = dcc_mesh_to_trimesh(geo)
```

This is the layer that should grow into real `image`, `mesh`, `tensor`, `scene`, and later host-facing DCC modules.
