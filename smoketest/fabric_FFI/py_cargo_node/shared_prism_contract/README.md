# Shared Prism Contract Smoke

This smoke proves the neutral shared image/buffer contract can bridge Python, Rust crate FFI, Kain, and Node from one `.kn` file.

Roles:

- Python: generates a NumPy uint8 raster image
- Cargo FFI: computes checksum, band anchors, and a signature from the shared bytes
- Kain: materializes the shared contract, inspects it, builds the legend/report, and coordinates the full flow
- Node: encodes a PPM artifact and writes an HTML viewer

Run:

```powershell
run_import_crate.bat
run_all.bat
run_test.bat
run_interpret.bat
```

Artifacts:

- `outputs/shared_prism_contract.html`
- `outputs/shared_prism_contract.ppm`
- `outputs/shared_prism_contract_report.txt`
- `outputs/generated/shared_prism_lab.kn`
