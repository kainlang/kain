# All-In-One Pipeline Smoke

This folder is the broad regression harness for Kain's major pipeline surfaces.

It is meant to give one place where we can re-run the important codegen and bridge lanes and inspect each lane's artifacts in a dedicated output folder.

## What It Covers

- direct `kain import-ts`
- direct `kain import-asm`
- standalone C ABI FFI bridge smoke through `use c::...`
- standalone Rust crate FFI smoke through `use rust::...` plus explicit `kain import-crate`
- direct `kain gpu-artifacts`
- `kain omni build` with staged TypeScript, C, and assembly imports plus TypeScript, KainScript, shader, GPU, and UE5 targets
- `kain fabric validate` and `kain fabric run`
- Fabric runtime adapters for `python`, `kain`, `gpu_compute`, `c_abi`, `rust_crate`, and `node`
- `kain build --ue5` through a local minimal plugin packager lane

## Folder Layout

- `fixtures/`: self-contained source inputs for each pipeline
- `outputs/`: per-lane generated artifacts and command logs
- `results/`: summary reports written by the orchestrator
- `pipeline_manifest.json`: data-driven command registry for the runner
- `run_all.ps1`: main orchestrator
- `run_all.bat`: Windows wrapper

## Run

```powershell
powershell -ExecutionPolicy Bypass -File .\smoketest\allinone\run_all.ps1
```

Subset run:

```powershell
powershell -ExecutionPolicy Bypass -File .\smoketest\allinone\run_all.ps1 -Pipelines import_ts,import_asm,omni_build
```

Stop on the first failure:

```powershell
powershell -ExecutionPolicy Bypass -File .\smoketest\allinone\run_all.ps1 -StopOnError
```

## Expected Output Roots

- `outputs/import_ts`
- `outputs/import_asm`
- `outputs/c_ffi`
- `outputs/crate_ffi`
- `outputs/gpu_artifacts`
- `outputs/omni`
- `outputs/fabric`
- `outputs/ue5`
- `outputs/logs`
- `results`

## Notes

- The runner prefers repo-local binaries first, then falls back to PATH.
- The runner deletes lane-specific generated outputs before each command so stale files do not produce false green regressions.
- The Fabric lane builds its local C sidecar with `clang` before `fabric run`.
- The Omni lane deliberately stages TypeScript, C, and assembly imports even though the direct CLI lanes already validate those import commands on their own. That gives both standalone importer coverage and manifest-driven importer coverage.
- The standalone `c_ffi` and `crate_ffi` fixtures mirror the durable repo-local smoke patterns, but keep their outputs under this folder so broad codegen regressions are easier to inspect in one place.
- Generated artifacts in `outputs/` and `results/` are disposable.
