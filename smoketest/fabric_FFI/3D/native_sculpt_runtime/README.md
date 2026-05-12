# Native Sculpt Runtime Prototype

This prototype pushes the next 3D lane forward:

- Kain owns sculpt-session semantics, tool state, contract/report generation, and the authored shell.
- A local Rust crate owns sculpt backend forecasting and brush math.
- A local C library owns a real Win32 window, message pump, native frame loop, and viewport capture.

Artifacts:

- `outputs/native_sculpt_runtime_capture.bmp`
- `outputs/native_sculpt_runtime_dashboard.html`
- `outputs/native_sculpt_runtime_contract.json`
- `outputs/native_sculpt_runtime_report.txt`
- `outputs/generated/native_sculpt_backend.kn`

Run:

```powershell
run_all.bat
run_build_native.bat
run_import_crate.bat
run_test.bat
run_interpret.bat
```

Notes:

- This is still the host-backed lane for Kain execution.
- The native viewport itself is real Win32 code, not an HTML demo.
- The window auto-closes after a short runtime so the smoke stays repeatable.
