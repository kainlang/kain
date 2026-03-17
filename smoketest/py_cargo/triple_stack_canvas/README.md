# Triple Stack Canvas Smoke

This smoke proves a real hybrid Kain workflow in one file:

- Python FFI generates the base raster with NumPy and exports BMPs
- Rust crate FFI generates native pattern rows, beacons, and signature data
- Kain performs the composition pass and writes the final runtime report

Run:

```powershell
run_all.bat
run_import_crate.bat
run_test.bat
run_interpret.bat
```

Artifacts:

- `outputs/triple_stack_base.bmp`
- `outputs/triple_stack_final.bmp`
- `outputs/triple_stack_report.txt`
- `outputs/generated/py_cargo_canvas.kn`
- `outputs/generated/py_cargo_canvas_report.txt`

The local Rust crate intentionally includes unsupported public items so the generated report also proves callable, type-only, and stubbed classification.
