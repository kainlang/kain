# Cargo Smoke Tests

These smokes are the proof surface for Kain's Rust crate FFI lane.

Each smoke lives in its own folder so it can act as a reference example instead of a one-off bridge probe.

## Current Smokes

- `local_crate_synth`: local path-crate import through `KAIN.toml`, generated binding artifacts, live bridge execution, and runtime-authored output artifacts

## Run Model

Each smoke folder contains:

- `KAIN.toml`
- `smoke.kn`
- `run_import_crate.bat`
- `run_test.bat`
- `run_interpret.bat`
- `run_all.bat`
- `outputs/`

`run_import_crate.bat` proves the generated `.kn`, prelude, and binding report path.

`test` proves runtime correctness.

`interpret` proves the real user-facing execution path and regenerates output artifacts.
