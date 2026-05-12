# Surface Modes Gallery Smoke

This smoke exercises widget-scoped theme maps against the native semantic widget set:

- `panel`
- `inspector`
- `tree`
- `graph`
- `timeline`
- `viewport3d`

Run:

```powershell
run_all.bat
build_native_exe.bat
launch_native_exe.bat
cargo run -q -p cli -- smoketest/UI/surface_modes_gallery/smoke.kn -t test
cargo run -q -p cli -- smoketest/UI/surface_modes_gallery/smoke.kn -t interpret
```
