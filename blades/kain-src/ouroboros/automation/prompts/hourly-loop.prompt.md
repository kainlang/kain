You are Codex running the Ouroboros V2 hourly self-host loop for this repo.

Start by reading:

- `automation/config/pipeline.config.json`
- `automation/README.md`
- `automation/BACKLOG.md`
- `automation/CHANGELOG.md`
- `automation/docs/SELFHOST_LOGIC_MAP.md`
- `automation/docs/PIPELINE_BLUEPRINT.md`

Determine the next turn number by counting existing markdown files in `automation/reports/` and adding one, or just run:

- `node automation/scripts/next-turn.mjs`

Then execute exactly one lane-appropriate improvement for the active turn.

Core expectations:

- Treat `M:/Code/OuroborosV2` as the self-host control plane.
- Treat `M:/Code/Kain` as the live implementation repo.
- Protect the currently used bootstrap corridor unless the current turn has exact evidence for a safe targeted fix.
- Prioritize importer hardening until strict Rust self-host import is much cleaner.
- Prefer data-driven manifests, inventories, typed contracts, and repair rules over ad hoc hardcoded command sequences.
- Use the lane definitions in `automation/config/pipeline.config.json` instead of inventing a new workflow mid-run.
- The retirement target is a real self-hosted `kain.exe` plus a near-1:1 Rust -> KAIN -> compile path across the intended crate surface under `M:/Code/Kain/crates`.
- The long-range target is to import as much Rust as possible into KAIN and make the self-hosted path strong enough to carry very large systems.

Validation expectations:

- Run the lane commands from the pipeline config.
- If a command is too expensive, flaky, or times out, use a listed fallback command and explain why in the report.

Required output:

- Make one concrete improvement in code or docs for the active lane.
- Write a report in `automation/reports/` named `TURN-XXX-<lane>.md`.
- Base it on `automation/templates/session-report.md`.
- Update `automation/CHANGELOG.md` with one concise entry for the turn.
- Include what changed, which Kain and Ouroboros files were used, which commands ran, what passed or failed, and the best handoff for the next agent.
- If phase 2 appears materially complete, create a concrete phase-3 plan instead of stopping.
