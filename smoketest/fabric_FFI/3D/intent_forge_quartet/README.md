# Intent Forge Quartet

`intent_forge_quartet` is the native 3D smoke for the compiler-owned intent quartet.

What it proves:

- `world` declares one shared authored root with all four required surfaces.
- `patch` owns editor-like state mutation for the preview session.
- `converge` owns multi-lane preview math under one semantic contract.
- `orchestrate` owns the staged preview pipeline while the same file still materializes a native executable.
- `build native-ui` can package the authored file into a real native app with a viewport lane.

Key files:

- `smoke.kn`: authored Kain source for the executable smoke app
- `run_smoke.py`: executes `kain run`, builds the native app, and validates the generated artifacts
- `launch_native_app.sh`: Linux launcher that builds and opens the packaged executable

Linux flow:

```bash
source generated/kain-env.sh
python3 smoketest/3D/intent_forge_quartet/run_smoke.py
./smoketest/3D/intent_forge_quartet/launch_native_app.sh
```
