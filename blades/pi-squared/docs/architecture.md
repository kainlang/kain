# pi-squared Architecture Guide

> **Mapping the pi coding-agent onto Kain's compiler-owned semantic stack.**
> 8 layers, 30+ files, 3,687 lines — every pi subsystem in its correct ladder rung.

---

## 1. Decision Ladder Mapping

The defining design decision of pi-squared is that **every subsystem sits at the correct decision-ladder rung**. Plain `fn` code is the exception, not the default.

| Layer | Construct | pi Concept | Kain Equivalent |
|-------|-----------|------------|-----------------|
| L7 | `actor` | LLM turn loop | `AgentActor` — mailbox, steering/follow-up, abort |
| L7 | `actor` | Session persistence | `SessionTree` — append-only JSONL, branching, rebuild |
| L7 | `actor` | Settings manager | `PiSettingsManager` — 4-layer merge, watch, apply |
| L7 | `collapse/observe/decay` | File buffer lifecycle | Raw ptr<T> for editor buffers, clipboard, pipe IO |
| L6 | `axiom` | Platform capabilities | `axiom platform_truth` — OS, arch, terminal features |
| L6 | `shatter struct` | Hot-path dispatch tables | Tool registry, keybinding maps, provider index |
| L6 | `teleport` | Cross-world state handoff | Session export, extension bridge, provider handover |
| L5 | `pulse` | TUI render loop | `pulse tui_render every 16ms` — frame clock |
| L5 | `pulse` | Compaction check | `pulse compaction_check every 30s` |
| L5 | `pulse` | Session auto-save | `pulse session_save every 60s` |
| L5 | `resonate` | Config watcher | Rebuild settings tree on world field change |
| L4 | `orchestrate` | LLM complete pipeline | Build → Call → Parse → Accumulate → Validate |
| L4 | `orchestrate` | Compaction pipeline | Analyze → Summarize → Apply |
| L4 | `orchestrate` | Startup pipeline | Init → Parse → Migrate → Load → Resolve → Ready |
| L3 | `converge` | Tool dispatch | `spec reference + fast lanes by tool name` |
| L3 | `converge` | Keybinding dispatch | `spec reference + fast by key sequence` |
| L3 | `converge` | API key resolution | `spec default + fast by provider name` |
| L2 | `patch` | All world mutations | Journaled state changes with epoch bumps |
| L2 | `law` | Config validation invariants | `law valid_provider`, `law valid_tool_name` |
| L1 | `world` | Global state (8 worlds) | ConfigDefaults, TrustStore, ApiKeyVault, ... |
| L1 | `entangle` | Cross-world sync | ApiKeyVault ↔ ProviderRegistry, ToolRegistry ↔ Dispatch |
| L0 | `fn` | Pure computation | String formatting, JSON parsing, math, validation |
| L0 | `struct`/`enum` | Data models | ToolResult, SessionFrame, ConfigNode, SkillDesc |

---

## 2. File Layout

```
src/
├── main.kn                  # Entry: init worlds, spawn actors, start pulse
├── cli.kn                   # CLI parser — converge dispatch on subcommands
├── types.kn                 # Core struct/enum data models (L0)

src/agent/
├── agent_actor.kn           # AgentActor — mailbox-driven LLM turn loop (L7)
├── agent_events.kn          # Event bus structs and converge dispatch (L3)
├── turn_queue.kn            # Steering/follow-up message queue (L7 state)
├── abort_signal.kn          # Abort signal world + entangle (L1)

src/config/
├── settings.kn              # PiSettingsManager actor (L7)
├── trust_store.kn           # TrustStore world (L1)
├── api_key_vault.kn         # ApiKeyVault world + converge resolution (L3)
├── markscript_loader.kn     # MarkScript → bytecode loader (fn)

src/session/
├── session_tree.kn          # SessionTree actor — JSONL append-only (L7)
├── session_compactor.kn     # Compaction orchestrate pipeline (L4)
├── migrations.kn            # Schema migration helpers (fn)

src/tools/
├── tool_registry.kn         # ToolRegistry world + converge dispatch (L3)
├── tools/read.kn            # read tool (fn → collapse ptr)
├── tools/write.kn           # write tool (fn)
├── tools/edit.kn            # edit tool (fn)
├── tools/bash.kn            # bash tool (fn)
├── tools/grep.kn            # grep tool (fn)
├── tools/find.kn            # find tool (fn)
├── tools/ls.kn              # ls tool (fn)

src/resources/
├── skill_loader.kn          # Skill loading (fn)
├── prompt_builder.kn        # System prompt assembly (fn)
├── context_assembler.kn     # Context window assembly (fn)

src/providers/
├── llm_provider_trait.kn    # LLM provider trait/impl (L0)
├── provider_registry.kn     # ProviderRegistry world (L1)
├── provider_converge.kn     # Provider selection converge (L3)
├── providers/               # Per-provider implementations

src/pipeline/
├── startup_pipeline.kn      # Startup orchestrate (L4)
├── llm_complete.kn          # LLM complete orchestrate (L4)
├── compaction.kn            # Compaction orchestrate (L4)
```

---

## 3. Key Design Decisions

### 3.1 Three Actors, Not One Monolith

The original pi uses a single `Agent` class (1,200 lines) that handles everything. pi-squared splits this into three actors:

- **AgentActor** (L7) — owns the LLM conversation loop. Messages drive turns. Steering/follow-up queues are mailbox priorities. Abort is a separate signal world entangled into the actor.
- **SessionTree** (L7) — owns persistence. Append-only JSONL means no locking. Branches are cheap (copy the tree pointer, write new frames). Context rebuild is a `to_vector` of ancestor frames.
- **PiSettingsManager** (L7) — owns the config cascade. CLI > project > global > defaults, each level a world field with `entangle` propagation.

This split means each actor has bounded state and a clear message contract. The runtime schedules them independently.

### 3.2 Worlds for State, Not Singletons

Eight worlds. The original pi had 20+ global variables, module-level singletons, and object instances. Each world in pi-squared is a named state container with fields, patches for journaled mutation, laws for invariants, and entangles for cross-world coupling.

The ApiKeyVault ↔ ProviderRegistry entangle is a concrete example: when the vault patches a new key, the registry automatically learns about it — no manual `notify()` call needed.

### 3.3 Orchestrate Pipelines for Every Multi-Step Flow

Three orchestrate blocks replace what was deeply nested callback chains. The LLM Complete pipeline is the most important:

```
Build    →  Call      →  Parse    →  Accumulate   →  Validate
kain       rust          fn          fn              law
```

Each stage has a distinct runtime, residency, and transfer policy. The `law` stage (`validate`) gates the output: if the law fails, the stage graph falls back to a retry degraded path.

### 3.4 Converge for Dispatch

Three converge blocks handle the three dispatch-heavy subsystems. The tool dispatch converge is the pattern:

```
spec reference:
    linear_scan(tool_name)
fast ffi_tool when capability("tool.ffi"):
    ffi_tool_call(tool_name)
fast hashed when capability("cpu.x86.avx2"):
    hash_table_lookup(tool_name, tool_names_hash)
verify random(16)
```

The `verify random(N)` clause fuzz-tests fast lanes against the spec at first-run, so correctness is proven per-machine.

### 3.5 Pulse Loops Replace setTimeout/setInterval

Three pulse loops at different cadences. Each is a compiler-owned recurrent block with jitter tolerance. The 16ms TUI render pulse is the heartbeat of the UI layer — everything that touches the screen flows through it.

---

## 4. Stream Verdict

| Stream | Layer Coverage | Status |
|--------|---------------|--------|
| **ALPHA** | L0-L7 all layers, 3 actors, 8 worlds, 3 pipelines, 3 converge | Complete |
| **BRAVO** | LLM provider HTTP/SSE implementations | In design |
| **CHARLIE** | Terminal UI framing, editor surfaces | Pending |
| **DELTA** | Kain test suite, Z3 proof packs for ownership safety | Pending |

### 4.1 Known Gaps (ALPHA)

- `build.kn` — the Kain build authority is a placeholder; needs runtime manifest wiring
- `pi-squared.md` — MarkScript orchestrator references mks.exe which is a separate binary
- Provider implementations (BRAVO) are stubbed as pure trait definitions
- TUI substrate (CHARLIE) is not wired; pulse loop exists but frame buffer is internal
- No Z3 proof packs yet for the ownership lattice (collapse/observe/decay in editor buffers)

---

## 5. Build & Verify

```bash
# Typecheck the entire workspace
kain check src/

# Build native binary
kain build --target llvm

# Run with debug diagnostics
./pi-squared.exe --debug

# Core state query
./pi-squared.exe --status
```
