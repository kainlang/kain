# Blades Workspace Smoke

This lab is a full local blades workspace fixture. It is intentionally shaped like a small repo, not a toy single file.

It covers:

- root `KAIN.toml` workspace discovery with `apps/*`, `blades/*`, and `crates/*`
- an app blade resolved by `kain equip` and Fabric `blade = "..."`
- a Kain library blade that contributes module roots and graph edges
- a C ABI blade with a generated platform shared library
- a Rust crate blade with Kain glue and Cargo metadata
- a synthetic Cargo-only blade discovered from `crates/*`
- a GPU compute blade discovered through blade GPU metadata
- CPU Fabric execution through Python -> Kain -> C ABI -> Rust crate -> Node
- GPU shader artifact generation, plus a GPU Fabric manifest that validates blade-backed `gpu_compute`

Run the smoke from the repo root:

```powershell
python labs\blades_workspace_smoke\scripts\run_blades_smoke.py
```

The runner builds `blades/native_filter/native/blade_filter.dll` or the platform equivalent before `kain blades check`, then drives the repo-local `kain` binary through list, graph, check, equip, Fabric validate, Fabric run, and GPU artifact generation.

Use `--clean-cache` when you specifically want to force the lab-local C/Rust FFI bridge crates to rebuild.

Use the optional Vulkan dispatch pass only on machines with a working Vulkan compute runtime:

```powershell
python labs\blades_workspace_smoke\scripts\run_blades_smoke.py --include-vulkan
```

Generated outputs live under `outputs/`, `.kain/`, and local build folders. They are disposable.
