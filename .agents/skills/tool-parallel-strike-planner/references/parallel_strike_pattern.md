# Parallel Strike Pattern

Use this reference when the plan needs to look like the repo's existing strike folders but the exact subsystem split is still being designed.

## Non-Negotiables

- One checkout.
- No worktrees.
- All lanes can start immediately.
- Shared files are reserved deliberately, not optimistically.
- Merge order is a landing sequence, not a start sequence.

## README Shape

Use these sections in order:

1. Title
2. Mission
3. Current Repo Truth
4. Frozen Boundaries
5. Shared Public Surfaces Allowed This Pass
6. Parallel Ownership Map
7. Global Definition Of Done
8. Merge Order
9. Shared Validation Floor
10. Coordination Contract

## README Content Rules

### Mission

- Say what the strike is trying to finish, not just what area it touches.
- List the lanes in one numbered list with one-line roles.

### Current Repo Truth

- Anchor the plan in concrete file paths, current limitations, prepass status, known manifests, and existing tests.
- Prefer "this file currently does X" over generic architecture prose.

### Frozen Boundaries

- State the same-checkout rule explicitly.
- State the no-wait rule explicitly.
- State whether heavy validation is deferred or allowed.
- Reserve conflict-heavy files to one lane when possible.

### Shared Public Surfaces

- Keep the list narrow.
- Name only the folders and files that the overall strike may need.
- If one file is too conflict-heavy for everyone, move it out of this list and assign it to a finisher lane instead.

### Parallel Ownership Map

- Give each lane one seam family.
- Good examples:
  - compiler truth
  - runtime substrate
  - published matrix
  - artifact materialization
  - async bridge
  - GPU handoff
- Bad examples:
  - everything else
  - cleanup
  - miscellaneous follow-up

### Global Definition Of Done

- Write outcomes, not intentions.
- Include proof, smoke, or benchmark expectations when the claim requires them.

### Merge Order

- Keep it short.
- Explain why the order helps landing cleanly.
- Do not imply that the later lanes must wait to begin implementation.

### Shared Validation Floor

- Split this into:
  - lane-local checks during parallel execution
  - consolidation checks after lane work lands

### Coordination Contract

Require every lane brief to leave behind:

- exact files touched
- exact public surfaces changed
- exact proof or validation artifacts added or rerun
- one smoke or demo lane
- known compromises
- unresolved seams another lane must consume

## Lane Brief Shape

Each lane file should contain:

1. Lane
2. Mission
3. Owns
4. Do Not Own
5. Deliverables
6. Design Direction
7. Proof Obligations
8. Validation Duties
9. Smoke Target
10. Exit Criteria

## Parallelism Checklist

Before calling the plan done, check:

- Can every lane identify its owned files without waiting for another diff?
- Are cold-merge files assigned to one finisher lane?
- Is there a prepass note if the seam split already happened?
- Does any lane say "only for tiny hook points" on shared glue files?
- Are the validation commands honest about what is deferred?
- Would selective staging be straightforward in a dirty worktree?

## When To Add A Prepass

Add a prepass instead of fake parallelism when:

- the repo still has one giant source file that must be split before lanes can diverge
- all candidate lanes need to edit the same manifest-heavy file repeatedly
- the ownership seam does not exist yet and has to be created first

Call that out in `Current Repo Truth` or `Prepass Status`. Do not bury it.
