# Handoff Cleanup Checklist

Use this when you inherit unfinished work from another agent.

## Intake

- What was the original goal?
- What artifacts exist?
- Which files are the source of truth?
- Which files look provisional?
- Is there a real diff, or only generated output and notes?
- Are there hardcoded paths, hardcoded values, or embedded assumptions that need to be generalized?

## Assessment

- Is the implementation correct?
- Does it fit repo architecture and conventions?
- Is it compatible with the existing codebase, build graph, data flow, and runtime expectations?
- Are tests, config, wiring, or docs missing?
- Is the current shape an asset or a liability?
- Is the work close enough to refine, or should the structure be replaced?

## Cleanup

- Remove scaffolding, dead code, and one-off glue.
- Rename placeholders to domain terms.
- Collapse duplicate logic.
- Move useful code into proper modules.
- Add new files, adapters, or support systems when that is the cleanest way to make the result fit.
- Keep temporary workspace layout out of the final shape unless it is intentionally part of the product.

## Integration

- Connect exports, routing, CLI, feature flags, build graph, or UI flow as needed.
- Make compatibility explicit when bridging from handoff artifacts into existing code.
- Update docs and comments only where they help future maintenance.
- Make the result feel native to the codebase.

## Validation

- Run the smallest real checks first.
- Expand to broader tests for shared or risky changes.
- Verify the same behavior the prior agent was trying to produce.
- Confirm the final shape still works with surrounding code, not just in isolation.

## Report

- What was kept?
- What was changed?
- What was removed?
- What remains uncertain?
