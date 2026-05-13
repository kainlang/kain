# Blades Workspace Smoke

This lab is a full local blades workspace fixture. It is intentionally shaped like a small repo, not a toy single file.

It covers:

- root `KAIN.toml` workspace discovery with `apps/*`, `blades/*`, and `crates/*`
- an app blade resolved by `kain equip` and Fabric `blade = "..."`
- a Kain library blade that contributes module roots and graph edges
- a C ABI blade whose platform shared library is built by `blade build`
- a Rust crate blade with Kain glue and Cargo metadata
- a synthetic Cargo-only blade discovered from `crates/*`
- a GPU compute blade discovered through blade GPU metadata
- CPU Fabric execution through Python -> Kain -> C ABI -> Rust crate -> Node
- GPU shader artifact generation, plus a GPU Fabric manifest that validates blade-backed `gpu_compute`
- a real Cargo-built executable, `blade_singularity_atlas`, that inspects the Blade graph plus emitted SPIR-V/reflection artifacts and renders `outputs/singularity-atlas/index.html`

Run the smoke from the repo root after building the CLI package:

```powershell
python labs\blades_workspace_smoke\scripts\run_blades_smoke.py
```

The runner invokes `blade build . --json` inside the lab. The build system materializes the C shared library, builds Rust crate blades through Cargo, emits GPU artifacts under `.kain/out`, validates blade paths, validates Fabric manifests, and runs the CPU Fabric pipeline. The runner then executes the Cargo-built `blade_singularity_atlas` binary, which renders an HTML/SVG/PPM atlas from the SPIR-V outputs, and drives the repo-local `kain` binary through list, graph, check, and equip assertions.

Use `--clean-cache` when you specifically want to force the lab-local `.kain` build/cache roots and Fabric reports to rebuild.

Use the optional Vulkan dispatch pass only on machines with a working Vulkan compute runtime:

```powershell
python labs\blades_workspace_smoke\scripts\run_blades_smoke.py --include-vulkan
```

Generated outputs live under `outputs/`, `.kain/`, and local build folders. They are disposable.
