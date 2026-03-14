# Raw Native World Lab

This folder is the clean iteration sandbox for the raw native Kain viewport lane.

Layout:

- `src/`
  Kain source files for the lab.
- `assets/`
  Place environment assets here, starting with `.glb` files.
- root outputs
  `raw_native_world_lab.exe`, `raw_native_world_lab.ll`, and native debug artifacts land here after build.

Current entrypoint:

- [main.kn](/M:/Code/Kain/labs/raw_native_world_lab/src/main.kn)

Commands:

```powershell
./build.ps1
./run.ps1
./run.ps1 -AssetName city2.glb
```

Notes:

- `run.ps1` uses the local [ui_bundle.json](/M:/Code/Kain/labs/raw_native_world_lab/ui_bundle.json) if present so the raw C runtime keeps the compiled UI metadata path alive.
- `run.ps1` also auto-picks the first `.glb` in [assets](/M:/Code/Kain/labs/raw_native_world_lab/assets) and exports it through `KAIN_NATIVE_WORLD_ASSET`.
- [main.kn](/M:/Code/Kain/labs/raw_native_world_lab/src/main.kn) now configures the native viewport before launch, so edits in Kain visibly change profile, movement tuning, particles, and world scaling in the raw exe.
- This is the right place to drop the next environment asset and iterate on actor movement, camera behavior, and raw native world loading.
