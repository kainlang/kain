# Signalcraft Landing Smoke

This smoke pushes the native UI lane toward a website-style landing page:

- top navigation
- hero section
- stacked landing sections
- horizontal and vertical scrolling regions
- mount-time entrance motion in the native host

Run:

```powershell
run_all.bat
build_native_exe.bat
launch_native_exe.bat
cargo run -q -p cli -- smoketest/UI/website_clone_signalcraft/smoke.kn -t test
cargo run -q -p cli -- smoketest/UI/website_clone_signalcraft/smoke.kn -t interpret
```

## Output Hygiene

- `native-app/` executables are disposable; do not keep them checked in.
