# Python Smoke Tests

These smokes are the proof surface for Kain's embedded Python and DCC wrapper stack.

Each smoke lives in its own folder so it can act as a reference example instead of a one-off test blob.

## Current Smokes

- `pygame_poster`: raster art generation through `pygame`, then Kain-side image mutation
- `trimesh_glb_forge`: scene and mesh generation through `trimesh`, then Kain-side geometry mutation and GLB export
- `numpy_supernova`: heavy NumPy image/tensor/point-cloud workload with shared and owned Kain-side handling

## Run Model

Each smoke folder contains:

- `smoke.kn`
- `run_test.bat`
- `run_interpret.bat`
- `run_all.bat`
- `outputs/`

`test` proves contract correctness.

`interpret` proves the real user-facing runtime path and regenerates artifacts.

## Layering

These examples are intentionally written against:

- `std::python::*` for ecosystem access
- `std::dcc::*` for Kain-native payload semantics

That is the preferred direction for future pipeline code.
