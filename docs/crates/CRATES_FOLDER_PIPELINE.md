# Crates Folder Pipeline

This document describes how `crates/` stays structurally correct as the Kain workspace evolves.

## Source Of Truth

1. `Cargo.toml` workspace members
2. `crates/repomap.md` for the current crate tree
3. `crates/README.md` for a human summary
4. `docs/automation/CRATE_WORK_INDEX.md` for crate-specific maintenance notes

## Maintenance Loop

1. Add/rename/remove crate entries in `Cargo.toml`.
2. Regenerate or refresh `crates/repomap.md`.
3. Update `crates/README.md` to keep crate lane descriptions accurate.
4. Add/update `AGENT_NOTES.md` in touched crates when behavior or ownership changes.
5. Run stale artifact guard:

```powershell
pwsh -NoProfile -ExecutionPolicy Bypass -File ./scripts/check-stale-artifacts.ps1
```

## Guardrails

- Do not commit generated binaries or linker intermediates in crate folders.
- Use repo-relative paths in docs.
- Keep crate notes living and short; avoid one-off phase docs without owners.
