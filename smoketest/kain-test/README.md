# Kain Test Harness Smoke

This folder is the small source-level proof suite for `kain test`.

Run it with:

```powershell
target\codex-check-test\debug\kain.exe test smoketest\kain-test
```

The fixtures cover Rust-inspired directive modes, automatic `test` item
execution, nested module test discovery, and ignored cases.
