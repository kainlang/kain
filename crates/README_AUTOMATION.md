# Crates Automation Maintenance Notes

This file defines the repeatable maintenance loop for `crates/` documentation hygiene and stale artifact enforcement.

## Core Commands

Run stale-artifact validation:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File ./scripts/check-stale-artifacts.ps1
```

Install repository-managed git hooks:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File ./scripts/install-git-hooks.ps1
```

## Expected Outcomes

- No tracked stale artifacts that match:
  - `*.ilk`
  - `runtime/conformance/**/bin/test_*`
  - `.zig-cache/**`
  - `*.pyc`
  - `graphics_runtime_smoke_env_bundle.realtime_app.json`
- Crate documentation uses repo-relative paths (no machine-local absolute paths).
- `crates/README.md` and `crates/repomap.md` stay aligned with workspace members.

## High-Value Drift Checks

- Verify `docs/automation/CRATE_WORK_INDEX.md` points to repo-relative paths.
- Verify `docs/pipeline/C_RUNTIME_PIPELINE.md` keeps runtime references repo-relative.
- Keep this file small and operational; avoid audit-style long-form dumps.
