# AGENTS.md


FOR new agents - start with /home/ephemara/Dev/Kain/guides

## Engineering Principles

- Optimize for LLM and AI readability. Prefer explicit structure, self-explanatory naming, and code that a strong model can understand and continue without guesswork.
- Prefer data-driven systems whenever they fit. If a design might otherwise hardcode paths, routes, versions, mappings, toggles, endpoints, or behaviors, first consider configuration, schemas, manifests, lookup tables, or structured metadata.
- Name files, functions, types, modules, and variables so clearly that another agent can inspect the codebase for a few seconds and understand what each piece is for.
- Apply senior-level engineering judgment. Default to clean architecture, strong boundaries, and implementations that will hold up as the system expands.
- Assume every codebase in this workspace is private and unreleased. That makes aggressive cleanup, meaningful refactors, and bold architectural corrections acceptable when they materially improve the system.
- Prefer complete implementations over partial scaffolding when feasible. If a placeholder is the honest next step, label it clearly.

## Git

- Always commit your changes to git.
- Always push your changes to git.
- Always use a descriptive commit message.
- Always use a descriptive tag name.
- Always use a descriptive commit message.

## Execution Style

- Prefer aggressive, complete coding passes over timid micro-edits when the direction is clear.
- In greenfield or performance-sensitive areas, push for strong performance, modern techniques, and GPU usage when it is justified by the system.
- Do not chase broad off-path refactors just because they are tempting. Large refactors should either materially support the requested task or be surfaced clearly as follow-up recommendations.

## Parallel Work And Subagents

- For large tasks, major refactors, or multi-part features, often propose a two- or three-lane split that can run in parallel.
- Use this default lane framing:
- `Alpha` for core, backend, compiler, and runtime logic
- `Delta` for integration, UI, tooling, and documentation wiring
- `Charlie` for validation, memory updates, cleanup, and support work
- Use subagents when the work splits cleanly across files, modules, or subsystems.
- Do not use subagents for tiny fixes, tightly coupled edits, or work where coordination cost is higher than execution cost.

## Memory And Continuity

- When entering a project or codebase, check the project root for `memory.md` or `MEMORY.md`.
- When entering a project for the first time, starting a new conversation, or resuming after a context switch, handoff, or loss of context, check the project root for `ARCHITECTURE.md` and read it before making changes.
- Treat `ARCHITECTURE.md` as the durable project overview for future agents. It should explain what the project does, the major subsystems, the most important folders, the main entrypoints, the key data flows, important external integrations, the languages and stacks in use, the common CLI, build, run, and validation commands, and any critical architectural constraints or conventions.
- If `ARCHITECTURE.md` does not exist, create it once you have enough context to write a useful version. Do not leave behind an empty or low-value placeholder unless the user explicitly asks for scaffolding only.
- Include a high-signal `Common Errors` or `Lessons Learned` section in `ARCHITECTURE.md` when it would help future agents avoid recurring setup traps, build failures, environment gotchas, or debugging dead ends.
- Update `ARCHITECTURE.md` whenever the architecture materially changes or when new features, subsystems, folders, entrypoints, integration patterns, common commands, or recurring errors become important enough that future agents should know them.
- Keep `ARCHITECTURE.md` structural and high signal. It should explain the system, not read like a task log or session transcript.
- If no memory file exists and the task is complex, create one.
- Treat a task as complex if it touches three or more files, changes architecture, introduces a new subsystem, performs a meaningful refactor, or is likely to take more than 30 minutes.
- For complex tasks, update the memory file with durable context for future LLMs. Capture what changed, why it changed, important design decisions, current risks, and the next recommended step.
- Treat `ARCHITECTURE.md` and `memory.md` as complementary. `ARCHITECTURE.md` explains what the project is and how it is organized; `memory.md` captures durable task history, decisions, risks, and recommended next steps.
- For small or isolated tasks, memory updates are optional.

## Workflow Improvement

- Do more than complete the immediate task. When it helps, propose workflows that improve speed, quality, maintainability, repeatability, or multi-agent execution.
- Recommend relevant MCP tools when they can materially improve research, coding, debugging, validation, refactors, file operations, or automation.
- Recommend automation workflows when you notice repeated work, recurring checks, scheduled reports, maintenance loops, or repetitive validation tasks. Codex supports built-in automations, so surface them proactively when they would save time or reduce manual effort.
- If a better workflow, tool path, or automation pattern exists, surface it clearly instead of silently preserving a weaker manual loop.
