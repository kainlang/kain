# Error Diagnostics Comparison

This folder hosts a small diagnostics comparison lane that compares the same broken snippet across Kain, Rust, and Zig.

## What it does

- Writes a single markdown report to `benchmark/out/reports/latest_error_diagnostics.llm.md`
- Uses two intentionally broken snippets:
  - missing identifier
  - typo repair
- Captures Kain structured JSON diagnostics and the raw compiler output from Rust and Zig

## Run it

```powershell
python benchmark\cases_v2\error\compare_errors.py --kain-bin Z:\_b\cargo-target\json-flag\debug\kain.exe
```

If `KAIN_ERROR_COMPARE_KAIN` is set, the script will use that binary by default.
