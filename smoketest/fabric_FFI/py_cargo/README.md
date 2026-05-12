# Python + Cargo Hybrid Smoke Tests

These smokes are the proof surface for Kain running Python FFI and Rust crate FFI together in the same Kain file.

Each smoke lives in its own folder so it can act as a reusable reference example instead of a one-off experiment.

## Current Smokes

- `triple_stack_canvas`: Python generates the base image, Rust crate FFI generates native overlay data, and Kain performs the composition pass and report generation

## Run Model

Each smoke folder contains:

- `KAIN.toml`
- `smoke.kn`
- `run_import_crate.bat`
- `run_test.bat`
- `run_interpret.bat`
- `run_all.bat`
- `outputs/`

`run_import_crate.bat` proves the generated crate FFI binding/report path.

`test` proves the combined runtime contract.

`interpret` proves the real hybrid execution path and regenerates visible artifacts.
