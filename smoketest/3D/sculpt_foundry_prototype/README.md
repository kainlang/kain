# Sculpt Foundry Prototype

This is a Kain-authored sculpt-suite prototype aimed at the future 3D pipeline shape:

- Kain owns the sculpt frontend, workspace composition, and authored tool semantics
- a local Rust crate owns stroke math, mesh forecasting, and backend signatures
- the smoke emits a real visual artifact plus a contract/report for the backend data

Artifacts:

- `outputs/sculpt_foundry_prototype.html`
- `outputs/sculpt_foundry_contract.json`
- `outputs/sculpt_foundry_report.txt`
- `outputs/generated/sculpt_foundry_backend.kn`

Run:

```powershell
run_all.bat
run_import_crate.bat
run_test.bat
run_interpret.bat
```
