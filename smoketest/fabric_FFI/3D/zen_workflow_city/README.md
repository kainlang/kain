# Zen Workflow City Smoke

This smoke proves Kain can author a real 3D pipeline while using all four current FFI/runtime lanes underneath it.

What it does:
- mirrors the ZenDCC workflow groups from `/M:/ZenDCC/src/config/appConfig.ts`
- uses a local Rust crate to compute district layout, module placement, and signatures
- uses Python + `trimesh` to generate a real workflow city scene and export GLBs
- uses Kain to mutate the shared mesh in place before final export
- uses C + `cgltf` to inspect the exported GLB and validate scene stats
- uses Node to package a self-contained interactive viewer document
- includes a native viewport launcher so the generated GLB can be opened in the raw native world lab

Artifacts:
- `outputs/workflow_city_base.glb`
- `outputs/workflow_city_mutated.glb`
- `outputs/workflow_city.html`
- `outputs/workflow_city_layout.json`
- `outputs/workflow_city_report.txt`

Run it:

```bat
run_all.bat
```

Open the native viewport with the generated GLB:

```powershell
.\run_native_viewport.ps1
```
