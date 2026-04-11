# Selfhost Pipeline Runner

This runner executes manifest-driven selfhost lanes for Ouroboros V2.

Default manifest:

- `ouroboros/docs/selfhost/pipeline_manifest.json`

Typical usage:

```bash
python ouroboros/tools/selfhost_pipeline/run_pipeline.py list
python ouroboros/tools/selfhost_pipeline/run_pipeline.py run --lane analyze
python ouroboros/tools/selfhost_pipeline/run_pipeline.py run --lane phase2-core
python ouroboros/tools/selfhost_pipeline/run_pipeline.py run --lane phase2-full
```

Outputs:

- lane summary json under `ouroboros/out/selfhost/pipeline`
- per-run stdout/stderr logs for each step
- blocker bucket counts from the latest repaired-report/core log
- stage2 CLI binary existence status across Linux and Windows naming

Companion commands:

```bash
python ouroboros/scripts/selfhost_workspace_status.py
```

This runner resolves the repo roots from the current checkout by default, so the same manifest can drive Windows and Linux workspaces without rewriting absolute paths.
