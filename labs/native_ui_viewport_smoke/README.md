# Native UI Viewport Smoke

This lab is the fastest visual validation path for the `kain-3D` + `kain-ui-native` WGPU viewport lane.
It is meant to look like a real native tool shell now, not a raw debug dashboard.

What to verify:

- the app launches as a native Kain UI window
- the viewport dominates the layout and is immediately readable
- the overlay says `renderer: wgpu` unless the backend falls back
- clicking geometry updates the `selection` line in the overlay
- a gizmo tripod appears on the selected object
- `T`, `R`, and `Y` switch the gizmo mode label
- roaming still feels stable while the viewport keeps rendering

Useful commands:

```powershell
./build.ps1
./run.ps1
./run.ps1 -Software
./run.ps1 -Inspector
./run.ps1 -Trace
```

If the stable root exe is still open during rebuild, `build.ps1` writes the fresh build to `native_ui_viewport_smoke.next.exe` and `run.ps1` will prefer that newer artifact automatically.
