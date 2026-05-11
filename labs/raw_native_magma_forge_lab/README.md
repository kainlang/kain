# Raw Native Generic Scene Lab

This folder is the standalone raw-native Kain exe lane for the procedural `geometry_fixture` world.

Layout:

- `src/`
  Kain source files that drive the exe configuration before launch.
- `assets/`
  Reserved for future asset experiments. This lab intentionally launches the fallback procedural world today.
- root outputs
  `raw_native_magma_forge_lab.exe`, `raw_native_magma_forge_lab.ll`, and sidecar artifacts land here after build.

Current entrypoint:

- [main.kn](/M:/Code/Kain/labs/raw_native_magma_forge_lab/src/main.kn)

Commands:

```powershell
./build.ps1
./run.ps1
```

Notes:

- [main.kn](/M:/Code/Kain/labs/raw_native_magma_forge_lab/src/main.kn) configures the raw native runtime through `native_config_*` before opening the viewport.
- `run.ps1` intentionally clears `KAIN_NATIVE_WORLD_ASSET` so the exe proves the procedural native scene path instead of depending on a GLB.
- The native viewport fallback is selected through `KAIN_NATIVE_SCENE_PROFILE=default_viewport`; authored scene data should come from Kain source, runtime bundles, or external assets.
