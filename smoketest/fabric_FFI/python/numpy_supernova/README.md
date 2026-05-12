# NumPy Supernova Smoke

This smoke test pushes the Python bridge through a larger NumPy workload: procedural image synthesis, shared tensor access, point-cloud extraction, and Kain-side artifact generation.

Primary wrapper surface:

- `use std::python::bridge`
- `use std::python::numpy`
- `use std::dcc::image`
- `use std::dcc::tensor`
- `use std::dcc::mesh`

Run:

```powershell
run_all.bat
cargo run -q -p cli -- smoketest/python/numpy_supernova/smoke.kn -t test
cargo run -q -p cli -- smoketest/python/numpy_supernova/smoke.kn -t interpret
```

Artifacts:

- `outputs/numpy_supernova.ppm`
- `outputs/numpy_supernova_points.ply`
- `outputs/numpy_supernova_report.txt`
