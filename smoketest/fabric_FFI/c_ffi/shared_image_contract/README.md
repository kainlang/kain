# Shared Image Contract C FFI Smoke

This smoke proves that the C ABI FFI can consume the same neutral shared image contract already used by Python and Node.

Roles:

- Python: generates a NumPy uint8 RGBA raster
- C FFI: computes a checksum, mutates the raster in place, and round-trips an opaque workspace handle
- Kain: materializes the shared contract, drives the mutation flow, and writes the runtime report

Run:

```powershell
run_build_native.bat
run_all.bat
run_test.bat
run_interpret.bat
```

Notes:

- `run_test.bat` executes the focused `kain-c-ffi` crate test for shared image mutation and opaque-handle roundtrip.
- `run_interpret.bat` runs the one-file Kain smoke through `kain.exe`.

Artifacts:

- `outputs/shared_image_contract_report.txt`
- `.kain/cache/c_ffi/...`
