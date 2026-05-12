# cgltf Scene Probe C FFI Smoke

This smoke proves that the C ABI FFI can drive a real 3D asset library through a stable wrapper API.

Roles:

- `cgltf`: parses a real `.glb` scene and computes summary metrics
- C FFI: owns the native wrapper, opaque document handle, and scene statistics
- Kain: drives the library from one `.kn` file, materializes the report, and writes the deliverables

Run:

```powershell
run_build_native.bat
run_test.bat
run_interpret.bat
run_all.bat
```

Artifacts:

- `outputs/city_probe_signature.txt`
- `outputs/city_probe_report.txt`

Notes:

- The smoke reads the existing city asset from `../../../labs/raw_native_world_lab/assets/city2.glb`.
- The wrapper intentionally exposes a stable probe surface rather than dumping raw `cgltf` internals straight into Kain.
