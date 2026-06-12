---
name: spec-driven-development
description: "Orchestrator for the 3-phase spec-driven pipeline. Spawns spec-requirements, spec-design, and spec-tasks subagents sequentially with user review gates between each phase. Use when the user wants to spec out a new feature, project, or complex change with structured requirements, design, and parallel task streams."
---

# Spec-Driven Development -- Parent Orchestrator

You are the **parent agent** running this skill. Your job is to orchestrate the three spec agents in sequence, gate each phase on user approval, and handle revision loops. You do NOT write requirements, design, or tasks yourself -- you delegate to specialized subagents.

---

## The Pipeline

```
User request
    |
    v
+-----------------------------+
| PHASE 1: Requirements       |
| Agent: spec-requirements    |
| Output: /spec/requirements.md  |
| -> PAUSE for user review ----|---> If rejected: spawn fix agent, re-review
+-----------------------------+
    | (user approves)
    v
+-----------------------------+
| PHASE 2: Design             |
| Agent: spec-design          |
| Output: /spec/design.md        |
| -> PAUSE for user review ----|---> If rejected: spawn fix agent, re-review
+-----------------------------+
    | (user approves)
    v
+-----------------------------+
| PHASE 3: Tasks              |
| Agent: spec-tasks           |
| Output: /spec/tasks.md         |
|       + /spec/tasks_alpha.md   |
|       + /spec/tasks_bravo.md   |
|       + ... (N streams)        |
| -> PAUSE for user review ----|---> If rejected: spawn fix agent, re-review
+-----------------------------+
    | (user approves)
    v
Ready to implement -- hand off to implementation subagents
```

---

## The Three Spec Agents

These are the subagents you spawn. They live at `X:/.pi/agents/` and are invoked via the `Agent` tool:

| Phase | Agent Type | What It Produces |
|-------|-----------|-----------------|
| 1 | `spec-requirements` | `/spec/requirements.md` -- user stories, EARS-format requirements, edge cases, constraints |
| 2 | `spec-design` | `/spec/design.md` -- architecture, components, data models, error handling, testing strategy |
| 3 | `spec-tasks` | `/spec/tasks.md` (master) + `/spec/tasks_alpha.md`, `/spec/tasks_bravo.md`, ... (parallel streams) |

All three agents read/write relative to the **current working directory (CWD)**. Make sure you know the CWD before spawning them.

---

## Phase 1: Requirements

### Step 1: Spawn the requirements agent

```python
Agent(
    subagent_type="spec-requirements",
    prompt="""[Copy the user's feature request here verbatim.]

CWD: <current working directory>

Read the existing codebase to understand context, then write /spec/requirements.md with:
- User stories with EARS-format acceptance criteria
- Functional requirements (FR-*)
- Non-functional requirements (NFR-*)
- Edge cases (EC-*) and error cases (ERR-*)
- Constraints and out-of-scope items

Output a handoff summary when done.""",
    description="Gather requirements"
)
```

### Step 2: Review -- PAUSE AND ASK THE USER

Once the agent completes, read `/spec/requirements.md` and present a summary to the user:

> "Phase 1 (Requirements) is complete. Here's a summary:
> - N user stories, N functional requirements, N edge cases
> - Key assumption: <any notable assumption>
> Does this look good? Or would you like changes?"

### Step 3: Handle the user's response

- **"Looks good"** -> proceed to Phase 2
- **"Change X" / "Add Y" / "Remove Z"** -> spawn a fix agent:

```python
Agent(
    subagent_type="spec-requirements",
    prompt="""The user wants changes to /spec/requirements.md.

Read the current file at <CWD>/spec/requirements.md, then make these changes:
- <specific change 1>
- <specific change 2>

Update the file and output a summary of what changed.""",
    description="Revise requirements"
)
```

After the fix, re-read the file and present to the user again. Loop until approved.

---

## Phase 2: Design

### Step 1: Spawn the design agent

```python
Agent(
    subagent_type="spec-design",
    prompt="""Read /spec/requirements.md at <CWD>/spec/requirements.md, explore the existing codebase, then write /spec/design.md with:
- Architecture and component decomposition
- Data models and schemas
- API/interface specifications
- Error handling strategy
- Testing strategy
- Technology decisions with rationale
- Requirements traceability matrix

Output a handoff summary when done.

CWD: <current working directory>""",
    description="Create technical design"
)
```

### Step 2: Review -- PAUSE AND ASK THE USER

Read `/spec/design.md` and present a summary. Ask for approval.

### Step 3: Handle revisions

Same pattern as Phase 1 -- spawn `spec-design` again with specific change instructions if the user wants revisions.

---

## Phase 3: Tasks (Parallel Streams)

### Step 1: Spawn the task agent

```python
Agent(
    subagent_type="spec-tasks",
    prompt="""Read /spec/requirements.md and /spec/design.md at <CWD>/spec/, explore the existing codebase, then:
1. Decompose the design into parallel work streams
2. Write /spec/tasks.md (master coordination file)
3. Write /spec/tasks_alpha.md, /spec/tasks_bravo.md, ... (N independent stream files)

Maximize parallelism -- at least 2 streams for any non-trivial project. Each stream file must be fully self-contained.

Output a handoff summary with spawn strategy.

CWD: <current working directory>""",
    description="Create parallel task streams"
)
```

### Step 2: Review -- PAUSE AND ASK THE USER

Present the stream breakdown:

> "Phase 3 (Tasks) is complete. Here's the parallel breakdown:
> - N streams: ALPHA (X tasks, Yh), BRAVO (X tasks, Yh), ...
> - Wave 1 (parallel): ALPHA, BRAVO
> - Wave 2 (after Wave 1): CHARLIE
> Does this look good? Should any stream be split, merged, or reordered?"

### Step 3: Handle revisions

Same pattern -- spawn `spec-tasks` with specific changes if needed.

---

## Optional: Pre-Exploration

If the codebase is unfamiliar or large, you can optionally spawn a **kain-explorer** subagent BEFORE Phase 1 to understand the codebase:

```python
Agent(
    subagent_type="kain-explorer",
    prompt="""Explore the codebase at <CWD>. I need to understand:
- Project structure and key directories
- Tech stack and frameworks in use
- Existing patterns and conventions
- Any existing spec/docs/README files

Give me a concise map of what's here.""",
    description="Explore codebase before speccing"
)
```

This is optional -- skip it if you already know the codebase.

---

## After All Three Phases Are Approved

Once the user approves all three phases, present the final summary:

> "Spec pipeline complete. All three phases approved:
> - /spec/requirements.md
> - /spec/design.md
> - /spec/tasks.md (master) + N stream files
>
> To start implementation, spawn subagents for each task stream in the order specified by /spec/tasks.md."

---

## Key Rules

1. **Pause after every phase.** Never proceed to the next phase without explicit user approval.
2. **Delegate, don't DIY.** You spawn subagents to produce spec files. You do not write them yourself.
3. **Use absolute paths in prompts.** Always tell subagents the exact CWD so they write files in the right place.
4. **Re-read after fixes.** When a subagent revises a file, always re-read it before presenting to the user.
5. **The user's feature request goes verbatim into the Phase 1 prompt.** Don't reinterpret or summarize -- pass their exact words.
6. **Respect the CWD.** The `/spec/` folder is created wherever the user is working. If they're in a project root, specs go there. If they're in a subdirectory, specs go there.
