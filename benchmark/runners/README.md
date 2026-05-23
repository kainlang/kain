# Benchmark Runners

`bench.py` orchestrates runner entrypoints.

Current runner ownership:

- Main suite runner: `benchmark/run.py`
- GPU lane runner: `benchmark/lanes/gpu/run_gpu.py`
- WASM lane runner: `benchmark/lanes/wasm/run.py`
- FFI boundary runner: `benchmark/lanes/ffi_boundary/run.py`

This folder is the stable namespace for future extractions from the legacy `run.py` monolith while preserving the command contract introduced by `bench.py`.
