# Benchmark Wrappers

Wrapper configs are legacy-compatible launcher presets for `benchmark/run_wrapper.py`.

Primary orchestration now lives in `benchmark/bench.py`, but wrappers remain useful for compatibility and quick ad-hoc presets.

## Wrapper Contract

- Files: `benchmark/wrappers/*.json`
- Launcher: `python benchmark/run_wrapper.py <wrapper>`
- Schema:

```json
{
  "description": "Human-readable purpose",
  "runner": "run.py",
  "before_args": ["--manifest", "benchmark/benchmarks.json"],
  "after_args": ["--languages", "kain,rust,cpp"]
}
```

## Current Wrappers

- `fast`: reduced language sweep with `latest_fast` report stem.
- `sim`: simulation suite manifest preset.
- `gpu`: dedicated GPU lane runner.

## Examples

```powershell
python benchmark/run_wrapper.py --list
python benchmark/run_wrapper.py fast
python benchmark/run_wrapper.py sim --runs 3 --warmups 1
python benchmark/run_wrapper.py gpu --case semantic_ping_pong --languages kain,cpp
```
