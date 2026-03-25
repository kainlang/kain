# The Ultimate Agent Workflow: "The Neural Link" Protocol

## 1. The Core Philosophy
**"The Agent is the Driver, The User is the Engineer."**

You are a solo developer with a fleet of AI agents (Antigravity, Kiro, Trae, Windsurf). The bottleneck is **Context Synchronization**. Agents are "dumb" because they lack **shared short-term memory**. They have your rules (Long-Term Memory), but they don't know *what just happened* or *what needs to happen next* across IDE sessions.

This workflow introduces a **Filesystem-Based Neural Link**—a simple, standardized directory structure that acts as the "hive mind" for all your agents.

---

## 2. The "Neural Link" Structure (`.agent/active/`)

Create a directory `.agent/active/` in your monorepo root. This is the **RAM** for your swarm. All agents must be instructed to read this **first**.

```
.agent/
├── rules/                  # [READ-ONLY] Long-Term Memory (Your existing .md files)
└── active/                 # [READ-WRITE] Short-Term Memory (The Hive Mind)
    ├── MISSION.md          # The "North Star" - current high-level objective
    ├── SITREP.md           # Situation Report - what was just done, what's broken
    ├── ARCHITECTURE.md     # Current architectural constraints for the ACTIVE task
    └── MEMORY_LOG.md       # Append-only journal of decisions
```

### 📄 `MISSION.md` (The "What")
*Current high-level goal. Updated only by YOU or the PLANNER agent.*

> **Example:**
> **Objective:** Implement `Compose()` method for `SlateWidget`.
> **Constraints:** Must use new `gen_slate_expr` function.
> **Definition of Done:** 3 tests pass in `ue5editor.rs`.

### 📄 `SITREP.md` (The "Now")
*The "Talking Stick". The active agent MUST update this before signing off.*

> **Example:**
> **Status:** IN_PROGRESS
> **Last Action:** Kiro refactored `gen_construct_body`.
> **Current Blocker:** `Ty::Named` matching logic is failing in `handlers.rs`.
> **Next Step:** Debug `map_type` function to handle `U*` prefixes correctly.

### 📄 `ARCHITECTURE_SNAPSHOT.md` (The "How")
*Temporary architectural context for the specific mission. Prevents hallucination.*

> **Example:**
> - We are using `kain::compiler::ue5` module.
> - Do NOT use `kain_generic` traits here.
> - **Pattern:** UE5 Reflection uses `FProperty` wrappers.

---

## 3. The "Crew" Roles (Multi-Agent Specialization)

Stop treating all agents as equals. Assign them **Roles** based on their underlying strengths.

| Role | Agent / IDE | Best For | Prompt Strategy |
|:---|:---|:---|:---|
| **The Architect** | **Antigravity** | Planning, Large Refactors, Complex Thinking | "Read `.agent/active/MISSION.md`. Update the plan. Do not write code yet." |
| **The Builder** | **Kiro / Trae** | Fast execution, "Grunt work", Writing tests | "Read `.agent/active/SITREP.md`. Implement the 'Next Step'. Update SITREP when done." |
| **The Navigator** | **Windsurf** | Search, Code Understanding, "Where is X?" | "Search for usage of X. Update `.agent/active/ARCHITECTURE_SNAPSHOT.md` with findings." |
| **The Director** | **YOU** | Review, Approval, unlocking blockers | "Review SITREP. Approve Plan. Poke the Builder." |

---

## 4. The Workflow Loop

### Step 1: Initialization (You + Architect)
1. You define the goal in `.agent/active/MISSION.md`.
2. **Antigravity** reads it, scans the codebase, and generates a detailed plan in `task.md`.
3. **Antigravity** populates `.agent/active/ARCHITECTURE_SNAPSHOT.md` with relevant patterns.

### Step 2: Handoff (Architect -> Builder)
1. **Antigravity** updates `.agent/active/SITREP.md`: "Plan created. Ready for phase 1."
2. You open **Kiro/Trae**.
3. **Kiro** reads `SITREP.md` and sees: "Ready for phase 1."
4. **Kiro** executes the code.

### Step 3: Synchronization (Builder -> Log)
1. **Kiro** finishes the task (or hits a wall).
2. **Kiro** writes to `SITREP.md`: "Implemented X. Tests failing on Y."
3. **Kiro** appends technical details to `MEMORY_LOG.md`.

### Step 4: Recovery (Log -> Architect)
1. You open **Antigravity**.
2. **Antigravity** reads `SITREP.md`: "Tests failing on Y."
3. **Antigravity** analyzes the error (using its superior reasoning), fixes the architecture, updates `ARCHITECTURE_SNAPSHOT.md`.
4. Loop continues.

---

## 5. MCP Tooling Strategy

For a solo dev, you need tools that **bridge the gap** between agents.

### Recommended MCP Servers:
1.  **Filesystem (Crucial):** All agents MUST have this. It's the only way to read/write the "Neural Link".
2.  **Git:** Essential for "Safe Points". Agents should commit after every successful sub-task.
3.  **Command Line / Terminal:** Agents *must* be able to run `cargo check` or `kain build`. The feedback loop (Edit -> Run -> Error -> Fix) is where agents thrive.
4.  **Fetch (Web):** For looking up docs, but less critical if you have local context.

### "Semantic Memory" Hack
Since you can't easily spin up a vector DB, use **grep-friendly** memory.
- Ask agents to write "ADR" (Architectural Decision Records) in a flat folder: `.agent/memory/adr_001_ue5_patterns.md`.
- When an agent asks "How do we handle X?", you prompt: "Search `.agent/memory` for X".

---

## 6. Prompting Best Practices (The "Context Loader")

Start **EVERY** session with this system prompt (or saved custom instruction):

> "You are part of a multi-agent swarm. Your first action must be to read `.agent/active/MISSION.md` and `.agent/active/SITREP.md` to understand the current state.
>
> 1. **Do not** start from scratch.
> 2. **Do not** hallucinate patterns; check `.agent/rules/` and `.agent/active/ARCHITECTURE_SNAPSHOT.md`.
> 3. Before you finish, you **MUST** update `SITREP.md` with your progress and `MEMORY_LOG.md` with any key decisions.
>
> Your specific role right now is: [ARCHITECT | BUILDER | NAVIGATOR]."

---

## Summary
The "Ultimate Workflow" is not a tool, but a **Protocol**. By treating the filesystem as a shared brain (`.agent/active/`), you decouple the *state* from the *session*, allowing you to orchestrate multiple "dumb" agents into a brilliant swarm.
