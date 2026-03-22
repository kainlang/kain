# Docs Pipeline Index

This folder is the docs navigation layer for runtime and pipeline work.

It should stay focused on how the pipeline is organized, what the canonical truth source is, and how to find the active execution or validation lane.

## Primary Anchors

- [`../../runtime/native/C_RUNTIME_CONTRACT_PIPELINE.md`](../../runtime/native/C_RUNTIME_CONTRACT_PIPELINE.md) for the canonical native runtime contract pipeline.
- [`./C_RUNTIME_PIPELINE.md`](./C_RUNTIME_PIPELINE.md) for the C runtime execution pipeline notes.
- [`../../runtime/README.md`](../../runtime/README.md) for the broader native runtime overview.
- [`../../runtime/parallel/README.md`](../../runtime/parallel/README.md) for the non-C companion lane.
- [`./CRATES_PIPELINE.md`](./CRATES_PIPELINE.md) for the crate maintenance pipeline.
- [`../README.md`](../README.md) for the docs landing page.

## Current Scope

- native runtime contract and service truth
- runtime-side pipeline validation and operational notes
- companion lane notes for Rust and Zig execution planning
- future pipeline docs that should remain linked from a folder guide

## Output Hygiene

Pipeline runs can emit binaries and caches into `generated/`, `labs/`, and `smoketest/`.

Keep those outputs disposable:

- delete compiled artifacts (`.exe`, `.dll`, `.lib`, `.obj`, `.o`, `.pdb`, `.ilk`) after validation
- remove build caches (`target/`, `.kain`, `.kain-runtime`) once the run is complete
- move any long-lived validation logs into `docs/validation/` or `docs/recent/`

## Rule

Do not let pipeline knowledge drift into disconnected markdown files when a living index can point at the canonical contract or runtime README.
