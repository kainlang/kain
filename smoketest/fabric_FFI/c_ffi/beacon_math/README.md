# Beacon Math C FFI Smoke

This smoke proves that Kain can consume a local C ABI library through `use c::beacon_math`, build a shared library, and call it at runtime from one `.kn` file.

Primary surface:

- `use c::beacon_math`
- local C header + shared library configured in `KAIN.toml`
- host-backed interpret/test execution through the Kain CLI

Run:

```powershell
run_all.bat
run_build_native.bat
run_test.bat
run_interpret.bat
```

Artifacts:

- `outputs/beacon_signature.txt`
- `outputs/beacon_report.txt`
- `.kain/cache/c_ffi/...`

The local header intentionally includes unsupported declarations so the C FFI extractor has to classify callable and stubbed entries differently instead of pretending everything is supported.
