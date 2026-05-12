# Theme Authoring Shell Smoke

This smoke proves the compiler-owned theme authoring path for UI:

- `<theme>`
- `<scope>`
- `<token>`
- `<variant>`
- `<widget>`
- `<textvariant>`
- `<text role="...">`

Run:

```powershell
run_all.bat
build_native_exe.bat
launch_native_exe.bat
cargo run -q -p cli -- smoketest/UI/theme_authoring_shell/smoke.kn -t test
cargo run -q -p cli -- smoketest/UI/theme_authoring_shell/smoke.kn -t interpret
```
