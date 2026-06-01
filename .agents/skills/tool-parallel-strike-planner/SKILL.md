---
name: tool-parallel-strike-planner
description: Create repo-truth multi-agent strike plans under `plans/` for work that should be split into 3-4 parallel local lanes in one checkout. Use when the user wants a plan shaped like `cross-platform-runtime-strike`, `cuda-pipeline-strike`, `python-superstrate-strike`, or `rage-runtime-strike`, especially when they want agents to start immediately in parallel with no worktrees and no "wait for Alpha to finish first" dependency chain.
---

# Parallel Strike Planner

## Overview

Draft same-checkout strike plans that let multiple agents work at once without stomping each other. Keep the plan grounded in live repo seams, explicit file ownership, cold-merge reservations, lane-local validation, and merge order that does not become execution order.

## Workflow

1. Read the live subsystem truth first.
2. Split the work into immediately-startable lanes.
3. Reserve shared or conflict-heavy files to one finisher lane or a consolidation step.
4. Scaffold the plan folder, then replace placeholders with repo truth.
5. Leave behind a README plus lane briefs that another agent can execute without asking "who goes first?"

## Quick Start

Run the scaffolder:

```powershell
py -3 X:\.agents\skills\tool-parallel-strike-planner\scripts\scaffold_parallel_strike_plan.py `
  --root X:\plans `
  --slug my-strike `
  --title "My Strike" `
  --lane ALPHA:compiler-and-target-truth `
  --lane CHARLIE:artifact-and-cli-materialization `
  --lane DELTA:runtime-and-compatibility-truth
```

Then replace the placeholders in `README.md` and each lane brief with exact repo paths, exact owned files, exact proof/benchmark/smoke lanes, and exact conflict reservations.

## Planning Rules

- Start from live seams, not abstract architecture fantasies. Read the owning skill, the relevant code, the manifests, and the validation lanes first.
- Every lane must be startable on turn one in the same checkout.
- If a lane must wait for another lane before it can even begin, the plan is wrong. Either:
  - add a short prepass that lands the seam split first, or
  - redesign the lane boundaries so each lane owns a different seam from the start.
- Merge order is allowed. Execution order dependencies are not.
- Do not "solve" overlap with worktrees. Solve it with seam selection, file ownership, and cold-merge reservations.
- Put the thinnest possible cross-lane glue in one known file if shared registration is unavoidable.
- Reserve manifests, Bazel mirrors, generated metadata, or other single-conflict files to one finisher lane when possible.

## Required Plan Shape

Every strike plan should leave behind:

- `README.md` with:
  - mission
  - current repo truth
  - frozen boundaries
  - shared public surfaces
  - parallel ownership map
  - global definition of done
  - merge order
  - shared validation floor
  - coordination contract
- One lane brief per agent with:
  - lane name
  - mission
  - owns
  - do not own
  - deliverables
  - design direction
  - proof obligations
  - validation duties
  - smoke target
  - exit criteria

The lane brief should make it obvious what the agent can edit without reading another lane's patch first.

## Lane Design Heuristics

- Use `ALPHA`, `CHARLIE`, `DELTA`, and `THETA` unless there is a strong reason to mirror an existing naming scheme.
- Three lanes usually fit compiler/runtime/matrix or substrate/materialization/runtime splits.
- Add a fourth lane only when the fourth seam is truly orthogonal enough to start immediately.
- Prefer seam ownership such as compiler truth vs runtime truth vs published matrix truth, not broad layers like "backend" and "misc."
- Put hot shared files behind one thin dispatcher or registration seam instead of letting every lane edit the same core file.

## Validation Contract

- During lane execution, validation should stay lane-local and focused.
- Heavy repo-wide builds, broad tests, attrition sweeps, or benchmark refreshes belong in the consolidation block unless the user explicitly wants them during lane execution.
- The README should name both:
  - focused lane checks that each agent may run independently
  - consolidation checks that happen after the parallel implementation pass

## Red Flags

- "Delta waits for Alpha to do the initial refactor."
- "Everyone may touch `runtime/native_runtime.toml` if needed."
- "We will figure out ownership while coding."
- "Run repo-wide validation in every lane."
- "Use worktrees so overlap does not matter."
- "Alpha owns the shared file for now, but Charlie might patch it too."

If any of those show up, stop and fix the plan before calling it parallel.

## Resources

- Use `references/parallel_strike_pattern.md` for the detailed checklist and section-by-section template guidance.
- Use `scripts/scaffold_parallel_strike_plan.py` to generate the folder and markdown skeletons instead of rebuilding them from memory.

## Closeout

Before finishing:

1. Re-read the README and each lane brief as if four agents were about to start at the same time.
2. Verify that each lane can identify its files and begin work immediately.
3. Verify that merge order does not secretly hide execution dependencies.
4. Verify that the plan names cold-merge files and assigns them to one finisher lane or consolidation.
