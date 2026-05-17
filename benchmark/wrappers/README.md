# Benchmark Wrappers

Wrapper configs in this folder are the fire-and-forget orchestration layer for `benchmark/run.py`.

The rule is simple:

- `benchmark/run.py` stays the stable core runner.
- `benchmark/wrappers/*.json` are data-driven wrapper plugins.
- New categories should usually land as new wrapper JSON files instead of new branches inside `run.py`.

Run a wrapper directly:

```powershell
python benchmark/run_wrapper.py fast
python benchmark/run_wrapper.py sim --runs 3 --warmups 1
python benchmark/run_wrapper.py --list
```

Compatibility shims still exist:

```powershell
python benchmark/run_fast.py
python benchmark/run_sim.py
```

Wrapper schema:

```json
{
  "description": "Human-readable purpose",
  "runner": "run.py",
  "before_args": ["--manifest", "benchmarks.json"],
  "after_args": ["--languages", "kain,rust,cpp"]
}
```

Notes:

- `before_args` are inserted before any user-supplied CLI args.
- `after_args` are inserted after user-supplied CLI args, so wrapper defaults can intentionally override ad hoc flags.
- Wrapper files are relative to `benchmark/`.
- Use wrapper-owned `--minimal-name` and `--latest-stem` when a category should keep its own root snapshot and report files.
