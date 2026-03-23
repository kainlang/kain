# Selfhost Pipeline Runner

This runner executes manifest-driven selfhost lanes for Ouroboros V2.

Default manifest:

- `M:\Code\OuroborosV2\docs\selfhost\pipeline_manifest.json`

Typical usage:

```powershell
python M:\Code\OuroborosV2\tools\selfhost_pipeline\run_pipeline.py list
python M:\Code\OuroborosV2\tools\selfhost_pipeline\run_pipeline.py run --lane analyze
python M:\Code\OuroborosV2\tools\selfhost_pipeline\run_pipeline.py run --lane phase2-core
python M:\Code\OuroborosV2\tools\selfhost_pipeline\run_pipeline.py run --lane phase2-full
```

Outputs:

- lane summary json under `out\selfhost\pipeline`
- per-run stdout/stderr logs for each step
- blocker bucket counts from the latest repaired-report/core log
- stage2 binary existence status

Companion commands:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File M:\Code\OuroborosV2\scripts\selfhost_workspace_status.ps1
```

This tool is intended to reduce ad hoc command sequences and make phase2 iteration repeatable.
