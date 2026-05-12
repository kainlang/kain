# miniaudio Tone Lab C FFI Smoke

This smoke proves that the C ABI FFI can drive a real audio library through a stable wrapper API.

Roles:

- `miniaudio`: writes a real `.wav` file and decodes it back for analysis
- C FFI: owns the native tone-generation wrapper and summary functions
- Kain: drives the render/analyze flow, writes the report, and keeps the authored layer compact

Run:

```powershell
run_build_native.bat
run_test.bat
run_interpret.bat
run_all.bat
```

Artifacts:

- `outputs/tone_220hz.wav`
- `outputs/tone_signature.txt`
- `outputs/tone_report.txt`
