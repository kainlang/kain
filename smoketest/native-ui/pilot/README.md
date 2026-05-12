# Native UI Pilot Smoke

First raw native UI smoke for the LLVM lane.

This smoke proves one Kain file can author a UI system over `stdlib/native/ui.kn`, compile to LLVM, link a native executable, and verify the authored frame through the raw host metadata. It deliberately does not use the older `/smoketest` pipelines.

Run:

```powershell
.\run.ps1
```

Outputs are generated under `outputs/` and ignored:

- `pilot.ll`
- `pilot.exe`
- `pilot.runtime_contract.json`
- `pilot.realtime_app.json`

The current raw host is headless and validates draw/resource/frame metadata. When a real pixel backend is attached to `kain_native_ui_host_present`, this smoke is the place to add window capture.
