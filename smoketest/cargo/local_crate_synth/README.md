# Local Crate Synth Smoke

This smoke test proves that Kain can consume a local Rust crate folder through crate FFI, generate binding artifacts, call the live bridge at runtime, and turn the result into user-visible outputs.

Primary surface:

- `use rust::cargo_smoke_lab`
- local path crate configured in `KAIN.toml`
- `kain import-crate` for generated artifacts

Run:

```powershell
run_all.bat
run_import_crate.bat
run_test.bat
run_interpret.bat
```

Artifacts:

- `outputs/cargo_wave.txt`
- `outputs/cargo_bridge_report.txt`
- `outputs/generated/cargo_smoke_lab.kn`
- `outputs/generated/cargo_smoke_lab_prelude.kn`
- `outputs/generated/cargo_smoke_lab_report.json`
- `outputs/generated/cargo_smoke_lab_report.txt`

The local Rust crate also intentionally exposes unsupported public items so the generated binding report shows callable, type-only, and stubbed entries in one place.
