# Crates Pipeline

This document defines the maintenance pipeline for `crates/` so the workspace stays navigable as crates grow.

## Source Of Truth

1. `Cargo.toml` workspace members
2. `crates/repomap.md` for the full tree view
3. `crates/README.md` for the human-friendly index
4. `docs/crates/README.md` for docs-layer notes

## Update Flow

When a crate is added, renamed, or retired:

1. Update `Cargo.toml` workspace members first.
2. Refresh `crates/repomap.md` so the tree reflects the live filesystem.
3. Update `crates/README.md` so humans can find the new crate fast.
4. If the change affects docs, add a short note in `docs/crates/README.md`.

## Output Hygiene

- Do not store build artifacts inside `crates/`.
- Use crate-local `README.md` files for living intent, not audit dumps.
- Avoid standalone audit markdown files unless they are actively maintained.
