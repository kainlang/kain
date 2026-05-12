# Quad Prism Halo Smoke

This smoke proves the full quadrinity from one `.kn` file on top of the shared interop contract.

Roles:

- Python: generates the base NumPy RGBA prism raster
- Kain: paints a visible overlay directly into the shared bytes and writes the report
- Cargo FFI: computes checksums, bands, and the final signature
- C FFI: mutates the same shared image in place and exposes an opaque workspace handle
- Node: emits compare-view HTML plus base/final PPM artifacts

Run:

```powershell
run_all.bat
run_build_native.bat
run_import_crate.bat
run_test.bat
run_interpret.bat
```

Artifacts:

- `outputs/quad_prism_halo.html`
- `outputs/quad_prism_halo_base.ppm`
- `outputs/quad_prism_halo_final.ppm`
- `outputs/quad_prism_halo_report.txt`
- `outputs/generated/quad_prism_lab.kn`
