# Trimesh GLB Forge Smoke

This smoke test proves that Kain can drive `trimesh`, build scene geometry in Python, inspect and mutate mesh data through Kain semantics, and export GLB artifacts.

Primary wrapper surface:

- `use std::python::bridge`
- `use std::python::trimesh`
- `use std::dcc::mesh`

Run:

```powershell
run_all.bat
cargo run -q -p cli -- smoketest/python/trimesh_glb_forge/smoke.kn -t test
cargo run -q -p cli -- smoketest/python/trimesh_glb_forge/smoke.kn -t interpret
```

Artifacts:

- `outputs/trimesh_scene.glb`
- `outputs/trimesh_scene_mutated.glb`
- `outputs/trimesh_scene_report.txt`
