---
name: handoff-repair
description: Assess, repair, and integrate work produced by a prior agent or draft implementation. Use when Codex inherits partial code, scaffolding, scratch artifacts, unfinished changes, hardcoded paths or assumptions, or a handoff that needs review, cleanup, compatibility checking, architectural fit, tests, and production wiring.
---

# Handoff Repair

## Overview

Use this skill when inheriting work from another agent, a draft implementation, or a partial handoff. Preserve the useful intent, not the prior shape, and raise the result to repo quality.

## Default Stance

- Treat provisional artifacts as input, not authority.
- Keep verified behavior and good decisions; replace incidental structure freely.
- Do not assume any specific scratch path or staging layout. Discover the real artifacts in the workspace.
- Look for hardcoded paths, hardcoded values, and one-off code that should become configuration, helpers, or proper abstractions.
- Feel free to add new files, modules, adapters, tests, or small support systems when that is the cleanest way to make the work fit.
- Treat compatibility with existing code as a first-class requirement: check APIs, conventions, build graph, data flow, runtime assumptions, and neighboring systems.
- Start from the highest-confidence source of truth: existing code, tests, config, docs, and the actual diff.

## Workflow

### 1. Triage the handoff

- Identify what the prior work was trying to accomplish.
- Separate finished logic, partial scaffolding, generated artifacts, and notes.
- Flag any hardcoded paths, local-only assumptions, or embedded code that will break outside the original handoff context.
- Check whether the work is close to correct or whether the structure itself needs replacement.
- Use [handoff-cleanup.md](references/handoff-cleanup.md) for a compact audit checklist.

### 2. Assess quality and fit

- Verify correctness first.
- Check architectural fit with the surrounding codebase.
- Look for duplicated logic, dead ends, temporary names, leaky abstractions, and missing integration points.
- Decide whether to refine in place or rework the surface. Prefer the smallest change that makes the result solid.

### 3. Clean up and elevate

- Remove throwaway scaffolding and temporary glue that no longer serves a purpose.
- Normalize naming, boundaries, formatting, and error handling to match local conventions.
- Add new files, modules, adapters, or support systems when compatibility or maintainability requires them.
- Wire the work into the real architecture: modules, exports, configs, feature flags, CLI entry points, UI flows, or docs as needed.
- Make compatibility explicit when adapting the work to the existing codebase; do not force the codebase to conform to a temporary handoff shape.
- If the prior agent worked in a temporary area, move only the useful output into the proper repo location; do not preserve the temporary layout just because it exists.

### 4. Validate proportionally

- Run the smallest real verification that proves the integration.
- Expand validation when the touched surface is shared, risky, or user-visible.
- If validation is blocked, say exactly what remains unproven and why.

### 5. Hand back a crisp result

- Summarize what was kept, what changed, what was discarded, and what still needs attention.
- Call out any assumptions made while taking over.

## Useful Heuristics

- Preserve intent, not implementation shape.
- Prefer finishing the work over polishing the scaffold.
- If the prior work is already good, avoid churn.
- If the prior work is conceptually right but technically awkward, improve the structure rather than layering fixes on top.
- Hardcoded paths and hardcoded code are liabilities unless they are truly part of the product contract.
- It is normal to introduce new files or small systems if that makes the result compatible with the real codebase.
- If the handoff is ambiguous, rebuild the context from the artifacts before editing.

## Resources

- [handoff-cleanup.md](references/handoff-cleanup.md): compact audit checklist and decision prompts
