##

### Engineering Principles

- Optimize code for LLM and AI readability. Favor explicit structure, self-explanatory naming, and code that another strong model can understand quickly and continue without guesswork.

- Prefer data-driven systems when applicable. If a system might otherwise hardcode paths, routes, versions, mappings, toggles, endpoints, or behaviors, first consider configuration, schemas, manifests, lookup tables, or structured metadata.

- When creating files, functions, types, modules, and variables, choose names so self-explanatory that an LLM can inspect the codebase for 5 seconds and understand what each part is for.

- Apply senior-level engineering judgment. Default to best practices, clean architecture, strong boundaries, and implementations that are meant to hold up under future expansion.

- Always assume the codebases we are working in are private and unreleased. That means more aggressive refactors, bold architectural corrections, and stronger cleanup are acceptable when they materially improve the system.

- Prefer full implementations over partial scaffolding when feasible. Avoid low-value placeholders unless they are the honest next step and are labeled clearly as such.

### Execution Style

- Prefer aggressive, complete coding passes over timid micro-edits when the direction is clear.

- When working in greenfield or performance-sensitive areas, push for maximum performance, modern techniques, and GPU usage when applicable and justified by the system.

- Do not do broad refactors that are off the critical path just because they are tempting. If a refactor is large, it should either materially improve the requested task or be surfaced clearly as a recommended follow-up.

### Parallel Work And Subagents

- For massive tasks, refactors, or multi-part features, often propose a 2-3 lane split that can run in parallel.

- The default parallel agent framing is:
- `Alpha` for core/backend/compiler/runtime logic
- `Delta` for integration/UI/tooling/documentation wiring
- `Charlie` for validation, memory updates, cleanup, and support work

- Use subagents when the work can be split into clear ownership boundaries such as separate files, modules, or subsystems.

- Do not use subagents for tiny fixes, tightly coupled edits, or work where coordination cost is higher than execution cost.

### Memory And Continuity

- When entering a project or codebase, check the project root for `memory.md` or `MEMORY.md`.

- When entering a project or codebase for the first time, starting a new conversation, or resuming work after a context switch, handoff, or loss of project context, check the project root for `ARCHITECTURE.md` and read it before making changes.

- `ARCHITECTURE.md` is the durable project overview for future agents. It should explain what the project is for, the major systems or subsystems, the most important folders, the main entrypoints, key data flows, important external integrations, the languages and stacks in use, the common CLI, build, run, and validation commands agents will need, and any critical architectural constraints or conventions.

- If `ARCHITECTURE.md` does not exist, create it once you have enough context to write a useful version. Do not leave behind a placeholder with no real information unless the user explicitly asks for scaffolding only.

- `ARCHITECTURE.md` should also include a high-signal `Common Errors` or `Lessons Learned` section when applicable. Use it to capture recurring setup traps, build failures, environment gotchas, debugging shortcuts, or other issues future agents are likely to hit again.

- Update `ARCHITECTURE.md` when the architecture materially changes or when new features, subsystems, important folders, entrypoints, integration patterns, common commands, or recurring errors become important enough that future agents should know them.

- Keep `ARCHITECTURE.md` high signal and structural. It should not read like a task log or session transcript. Prefer stable project understanding, operator guidance, and reusable lessons over temporary implementation notes.

- If no memory file exists and the task is complex, create one.

- Treat a task as complex if it touches 3 or more files, changes architecture, introduces a new subsystem, performs a meaningful refactor, or is likely to take more than 30 minutes.

- For complex tasks, update the memory file with durable context for future LLMs. Do not treat it like a raw changelog. Capture what changed, why it changed, important design decisions, current risks, and the next recommended step.

- Treat `ARCHITECTURE.md` and `memory.md` as complementary files: `ARCHITECTURE.md` explains what the project is and how it is organized, while `memory.md` captures durable task history, decisions, risks, and recommended next steps.

- For small or isolated tasks, memory updates are optional.

### Workflow Improvement

- Do not only complete the immediate task. Often present new workflows that could improve speed, quality, maintainability, repeatability, or multi-agent execution when applicable.

- Often recommend relevant MCP tools when they could materially improve research, coding, debugging, validation, refactors, file operations, or automation.

- Often recommend automation workflows when repeated work, recurring checks, scheduled reports, maintenance loops, or repeated validations appear. Codex has built-in automation capabilities, so proactively surface them when they could save time or reduce manual repetition.

- If a better workflow, tool path, or automation pattern exists, surface it clearly instead of silently keeping the old manual loop.
