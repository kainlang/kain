# Ouroboros V2 Folder Guide

This folder is a nested repo that holds the Ouroboros V2 research and tooling lane.
Treat it as a self-contained workspace inside the Kain monorepo.

## What Lives Here

- `docs/` is the Ouroboros V2 documentation surface.
- `automation/` contains automation notes and scripts specific to this lane.
- `probes/` stores experimental validation probes and small smoke runs.
- `tools/` and `scripts/` are local utilities for Ouroboros workflows.
- `legacy/` holds earlier or deprecated efforts that are kept for reference.

## Output Hygiene

- `out/` is disposable output. Clear it after each validation run.
- Keep any important results in `docs/` or upstream under `M:\Code\Kain\docs\recent\`.

## Rule

When `ouroborosV2` generates outputs that matter to the wider repo, promote them into the main Kain `docs/` tree and update the root `repomap.md` if new anchors appear.
