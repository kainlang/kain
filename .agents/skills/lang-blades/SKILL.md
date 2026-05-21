---
name: lang-blades
description: Use when creating, extending, or repairing a runnable Kain blade workspace, including authored `.kn` sources, blade-local `KAIN.toml`, acceptance/demo apps under `blades/`, and project-shaped dogfood surfaces that compile and run without taking ownership of compiler, runtime, or Bazel internals.
---

# Lang Blades

## Overview

This skill owns authored blade workspaces. Use it when the task is to stand up a new blade, reshape an existing blade into a stronger Kain proof surface, or keep a blade's `.kn` and local metadata coherent while staying out of build-system and engine internals.

## Start Here

- Read `references/blade-authoring-patterns.md` before building a new blade shape from scratch.
- Prefer the nearest existing blade as the template, not a generic blank project.
- Use `scripts/compile_kain_blade_to_root.ps1` when you need the existing local compile flow instead of reinventing it.

## Routing

- Stay here for `blades/*` authored code, blade-local `KAIN.toml`, example reshaping, and acceptance/demo app structure.
- Switch to `tool-build-system` when the blocker lives in Bazel sync, blade resolution internals, generated build state, launcher behavior, or the repo-wide run/build pipeline.
- Switch to `bootstrap-core` when the blade uncovers parser, AST, lowering, or compiler semantic bugs.
- Switch to `runtime-core`, `runtime-stdlib`, or `runtime-gpu` when the blade fails because the native substrate is missing or wrong.
- Co-trigger `lang-ui`, `lang-gpu`, `lang-actors`, `lang-stdlib`, or `lang-c-abi-ffi` when the blade is centered on those surfaces.

## Blade Rules

- A blade should prove a capability, not merely exist. Favor acceptance blades, benchmark cases, or demo surfaces that exercise the claimed feature hard enough to matter.
- Keep authored fixes and infrastructure fixes separate. If a blade reveals an engine defect, preserve the authored intent here and route the subsystem repair to the owning sibling skill.
- Reuse blade-local references and scripts instead of rewriting the same setup logic in every session.
