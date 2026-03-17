# Dock Layout Workbench Smoke

This smoke focuses on semantic layout lowering for UI authoring:

- `layout="dock"`
- `dock`
- `split_ratio`
- `width`
- `min_width` and `max_width`
- `min_height` and `max_height`
- `resizable`
- `overflow`

Run:

```powershell
run_all.bat
build_native_exe.bat
launch_native_exe.bat
cargo run -q -p cli -- smoketest/UI/dock_layout_workbench/smoke.kn -t test
cargo run -q -p cli -- smoketest/UI/dock_layout_workbench/smoke.kn -t interpret
```
