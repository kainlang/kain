# pi-squared — The Kain-Native Coding Agent

A complete rewrite of the pi coding-agent in Kain.
30+ source files, 3 actors, 8 worlds, 3 pipelines, 3 converge blocks, 3 pulses.
Powered by MarkScript for build orchestration, configuration, and documentation.

## Metadata

| Property | Value |
|----------|-------|
| Name | pi-squared |
| Version | 0.1.0 |
| Language | Kain |
| Entry | src/main.kn |
| Target | llvm |
| Profile | debug |

---

# ProjectMeta

## ProjectIdent

| Property | Value |
|----------|-------|
| Name | pi-squared |
| Version | 0.1.0 |
| Kind | kain_executable |
| Entry | src/main.kn |
| Target | llvm |
| Profile | debug |

---

# Architecture

| File | Layer | Purpose | Stream |
|------|-------|---------|--------|
| src/types.kn | L0 | Core data models (AgentMessage, SessionEntry, Settings, Model, CliArgs) | Alpha |
| src/cli.kn | L0 | CLI argument parser (15 flags, 5 subcommands) | Alpha |
| src/config/defaults.kn | L0 | Config defaults struct + 5 law predicates | Alpha |
| src/config/settings.kn | L7 | PiSettingsManager actor (5 message handlers, 4-layer merge) | Alpha |
| src/config/trust.kn | L1 | TrustStore world + trust resolution | Alpha |
| src/config/markscript_loader.kn | L0 | Load config from markscript tables | Alpha |
| src/session/tree.kn | L7 | SessionTree actor (append, branch, context, persistence) | Alpha |
| src/session/compaction.kn | L4+L5 | Compaction orchestrate pipeline + pulse | Alpha |
| src/session/migrations.kn | L0 | Session migration functions (v1→v2→v3) | Alpha |
| src/agent/agent.kn | L7 | AgentActor (Prompt, Continue, Steer, Abort, SetModel, GetState) | Alpha |
| src/agent/events.kn | L7 | AgentEventBus actor (9 event types, subscribe/emit) | Alpha |
| src/agent/queues.kn | L0 | QueueMode enum + drain/enqueue/clear helpers | Alpha |
| src/resources/loader.kn | L7 | ResourceLoader actor (7 handlers for skills/prompts/context) | Alpha |
| src/resources/skills.kn | L0 | Skill loading, YAML frontmatter parsing | Alpha |
| src/resources/prompts.kn | L0 | Prompt template loading, {{var}} expansion | Alpha |
| src/resources/context.kn | L0 | Context file collection (AGENTS.md, CLAUDE.md, etc.) | Alpha |
| src/resources/system_prompt.kn | L0 | 4-layer system prompt assembly | Alpha |
| src/tools/trait.kn | L0 | Tool trait + ToolParameter/ToolSchema/ToolInfo/ToolExecutionResult | Alpha |
| src/tools/registry.kn | L1+L3 | ToolRegistry world + converge dispatch_tool | Alpha |
| src/tools/read.kn | L0 | Read tool with offset/limit truncation | Alpha |
| src/tools/write.kn | L0 | Write tool with parent dir creation | Alpha |
| src/tools/edit.kn | L0 | Edit tool with multi-occurrence safety | Alpha |
| src/tools/bash.kn | L0 | Bash tool via process_output_text with 100KB cap | Alpha |
| src/tools/grep.kn | L0 | Grep tool via rg with flag building | Alpha |
| src/tools/find.kn | L0 | Find tool via fs_walk with substring matching | Alpha |
| src/tools/ls.kn | L0 | Ls tool via fs_read_dir with limit | Alpha |
| src/tools/utils.kn | L0 | Format size, truncation, diff utils | Alpha |
| src/providers/trait.kn | L0 | LlmProvider trait + streaming interface | Bravo |
| src/providers/registry.kn | L0 | Provider registry world | Bravo |
| src/providers/sse.kn | L0 | SSE streaming parser | Bravo |
| src/providers/models.kn | L0 | Model definitions | Bravo |
| src/main.kn | L0 | Entry point, startup pipeline, mode dispatch | Alpha |
| src/pipeline/startup.kn | L0+L4 | Startup pipeline (init→parse→migrate→load→resolve→ready) | Alpha |

---

# Components

## **Stream Alpha** — Framework Core

> run

```markscript
print("=== Stream Alpha: Core framework ===")
print("Target: 24 files, 3 actors, 8 worlds, 2 pipelines, 3 converge")
print("Dependencies: Kain stdlib, llvm target, debug profile")
print("Status: In flight")
```

### **Stage 1** — Types and CLI

> run

```markscript
print("=== Stage: Types + CLI ===")
print("typecheck: src/types.kn, src/cli.kn")
print("action: verify CliArgs struct has all 15 flags and 5 subcommands")
```

### **Stage 2** — Configuration Subsystem

> run

```markscript
print("=== Stage: Config ===")
print("files: src/config/defaults.kn, src/config/settings.kn, src/config/trust.kn, src/config/markscript_loader.kn")
print("action: spawn PiSettingsManager, init TrustStore world, load markscript config tables")
```

### **Stage 3** — Session Subsystem

> run

```markscript
print("=== Stage: Session ===")
print("files: src/session/tree.kn, src/session/compaction.kn, src/session/migrations.kn")
print("action: spawn SessionTree actor, wire compaction pipeline, register migration functions")
```

### **Stage 4** — Agent Core

> run

```markscript
print("=== Stage: Agent ===")
print("files: src/agent/agent.kn, src/agent/events.kn, src/agent/queues.kn")
print("action: spawn AgentActor, spawn AgentEventBus, wire QueueMode dispatch")
```

### **Stage 5** — Resource Loading

> run

```markscript
print("=== Stage: Resources ===")
print("files: src/resources/loader.kn, src/resources/skills.kn, src/resources/prompts.kn, src/resources/context.kn, src/resources/system_prompt.kn")
print("action: spawn ResourceLoader, register skill/prompt/context providers, assemble system prompt")
```

### **Stage 6** — Tool Registry

> run

```markscript
print("=== Stage: Tools ===")
print("files: src/tools/trait.kn, src/tools/registry.kn, src/tools/read.kn, src/tools/write.kn, src/tools/edit.kn, src/tools/bash.kn, src/tools/grep.kn, src/tools/find.kn, src/tools/ls.kn, src/tools/utils.kn")
print("action: init ToolRegistry world, register all 9 tools, verify converge dispatch_tool")
```

### **Stage 7** — Entry Point

> run

```markscript
print("=== Stage: Main ===")
print("files: src/main.kn, src/pipeline/startup.kn")
print("action: compile and verify startup pipeline (init→parse→migrate→load→resolve→ready)")
print("target: llvm, profile: debug")
```

---

## **Stream Bravo** — Provider Layer

> run

```markscript
print("=== Stream Bravo: LLM Provider integration ===")
print("Target: 4 files, 1 trait, 1 world, SSE parser, model definitions")
print("Dependencies: Stream Alpha completion, HTTP client lib")
print("Status: Planned")
```

### **Stage 1** — Provider Trait and Models

> run

```markscript
print("=== Stage: Provider Trait ===")
print("files: src/providers/trait.kn, src/providers/models.kn")
print("action: define LlmProvider trait with streaming interface, define Model struct with capabilities")
```

### **Stage 2** — Provider Registry

> run

```markscript
print("=== Stage: Provider Registry ===")
print("files: src/providers/registry.kn")
print("action: init provider registry world, register built-in providers")
```

### **Stage 3** — SSE Streaming

> run

```markscript
print("=== Stage: SSE Parser ===")
print("files: src/providers/sse.kn")
print("action: implement SSE streaming parser for provider responses")
```

---

# QuickStart

```markscript
print("=== QuickStart ===")
```

> run

```markscript
print("python scripts/init.py")
print("kain build src/main.kn --target llvm")
print("./pi-squared")
```

## Interactive Mode

> run

```markscript
print("kain run src/main.kn --target llvm -- --interactive")
```

## One-Shot Mode

> run

```markscript
print("kain run src/main.kn --target llvm -- --mode oneshot --prompt 'say hello'")
```

## List Subcommands

> run

```markscript
print("kain run src/main.kn --target llvm -- --help")
```

---

# Install

## Prerequisites

> run

```markscript
print("=== Install Prerequisites ===")
print("Kain compiler (v0.1.0+) — build from root with: bazel build //:kain --config=dev")
print("LLVM toolchain (clang, lld) — for native compilation")
print("Python 3.11+ — for init and migration scripts")
```

## Clone and Build

### **Stage 1** — Repository Setup

> run

```markscript
print("git clone git@github.com:earendil-works/pi-squared.git")
print("cd pi-squared")
print("ls -la")
```

### **Stage 2** — Build Compiler

> run

```markscript
print("cd /repo/root")
print("bazel build //:kain --config=dev")
print("kain_sync_binary")
```

### **Stage 3** — Build Project

> run

```markscript
print("cd blades/pi-squared")
print("kain check src/main.kn")
print("kain build src/main.kn --target llvm --debug")
```

---

# Build

## Profiles

| Profile | Config | When |
|---------|--------|------|
| debug | --profile debug | Daily dev, stepping through agent logic |
| release | --profile release | Benchmarks, CI |
| speed | --profile speed --opt speed | Max performance (thin LTO) |

### **Debug Build**

> run

```markscript
print("kain build src/main.kn --target llvm --debug")
```

### **Release Build**

> run

```markscript
print("kain build src/main.kn --target llvm --config release")
```

### **Speed Build**

> run

```markscript
print("kain build src/main.kn --target llvm --opt speed --lto thin")
```

## Verification

> run

```markscript
print("=== Build Verification ===")
print("step 1: kain check src/main.kn --json")
print("step 2: oracle scan --dir target/llvm/debug/")
print("step 3: oracle launch target/llvm/debug/pi-squared.exe --wait 2000")
print("step 4: oracle debug --pid <pid>")
print("step 5: oracle find --pid <pid> --timeout 5000")
print("step 6: oracle matrix --handle <handle> --rows 15 --cols 30 --text")
print("step 7: oracle delta --handle <handle> --interval 200")
```

---

# Config

## Config Layers (bottom to top)

| Layer | Source | Format | Merge Rule |
|-------|--------|--------|------------|
| Defaults | src/config/defaults.kn | Kain struct | Base values |
| Markscript | pi-squared.md tables | Markdown table | Override defaults |
| Home Config | ~/.pi-squared/config.md | Markdown file | Override markscript |
| Project Config | .pi-squared/config.md | Markdown file | Highest priority |

### **Load Chain**

> run

```markscript
print("=== Config Load Chain ===")
print("1. Load defaults from src/config/defaults.kn")
print("2. Parse markscript tables from pi-squared.md")
print("3. Check ~/.pi-squared/config.md for overrides")
print("4. Check .pi-squared/config.md in project root for overrides")
print("5. Merge via PiSettingsManager actor (4-layer strategy)")
```

## Trust Store

The TrustStore world maintains two tables:

| Table | Key | Value |
|-------|-----|-------|
| allowed | path string | bool |
| denied | path string | bool |

> run

```markscript
print("Trust resolution is a converge block:")
print("  spec: check static allowlist")
print("  fast: check TrustStore world when target(llvm)")
print("  verify random(4)")
```

---

# Test

## Test Suite Layout

| Test | Source | Type |
|------|--------|------|
| types_test.kn | src/types.kn | Unit — struct constructors, defaults, serialization |
| cli_test.kn | src/cli.kn | Unit — all 15 flags, 5 subcommands, error cases |
| settings_test.kn | src/config/settings.kn | Actor — PiSettingsManager handler coverage |
| trust_test.kn | src/config/trust.kn | World — TrustStore add/check/clear |
| session_test.kn | src/session/tree.kn | Actor — SessionTree append/branch/context |
| compaction_test.kn | src/session/compaction.kn | Pipeline — full orchestrate run with pulse |
| agent_test.kn | src/agent/agent.kn | Actor — Prompt→Continue→Steer→Abort cycle |
| events_test.kn | src/agent/events.kn | Actor — EventBus subscribe/emit/unsubscribe |
| tools_test.kn | src/tools/registry.kn | Converge — dispatch_tool across all 9 tools |
| migration_test.kn | src/session/migrations.kn | Unit — v1→v2→v3 round-trip |

### **Run All Tests**

> run

```markscript
print("kain test src/ --json")
```

### **Run Single Test**

> run

```markscript
print("kain run src/tests/cli_test.kn --target llvm")
```

### **Run With Coverage**

> run

```markscript
print("kain test src/ --json --coverage")
```

---

# Scripts

## Init Script

```markscript
print("=== Init: Scaffold project structure ===")
```

> run

```markscript
print("python scripts/init.py --name pi-squared --dir src/")
```

## Migration Script

```markscript
print("=== Migration: Migrate session files ===")
```

> run

```markscript
print("python scripts/migrate.py ~/.pi/sessions/ ~/.pi-squared/sessions/ --from v1 --to v3")
```

## Sync Script

```markscript
print("=== Sync: Sync binary to toolchain path ===")
```

> run

```markscript
print("python scripts/sync.py --bin target/llvm/debug/pi-squared.exe --dest ~/.local/bin/")
```

## Doctor Script

```markscript
print("=== Doctor: Check environment ===")
```

> run

```markscript
print("python scripts/doctor.py")
print("checks: kain version, llvm toolchain, python version, session dirs, config files")
```

---

# Structure

## Directory Layout

```
blades/pi-squared/
├── pi-squared.md          # MarkScript orchestrator (this file)
├── src/
│   ├── main.kn            # Entry point
│   ├── types.kn           # Core data models
│   ├── cli.kn             # CLI argument parser
│   ├── config/
│   │   ├── defaults.kn    # Config defaults + laws
│   │   ├── settings.kn    # PiSettingsManager actor
│   │   ├── trust.kn       # TrustStore world
│   │   └── markscript_loader.kn  # Markscript table loader
│   ├── session/
│   │   ├── tree.kn        # SessionTree actor
│   │   ├── compaction.kn  # Compaction pipeline
│   │   └── migrations.kn  # Session migration functions
│   ├── agent/
│   │   ├── agent.kn       # AgentActor
│   │   ├── events.kn      # AgentEventBus actor
│   │   └── queues.kn      # QueueMode enum + helpers
│   ├── resources/
│   │   ├── loader.kn      # ResourceLoader actor
│   │   ├── skills.kn      # Skill loading
│   │   ├── prompts.kn     # Prompt template expansion
│   │   ├── context.kn     # Context file collection
│   │   └── system_prompt.kn      # System prompt assembly
│   ├── tools/
│   │   ├── trait.kn       # Tool trait + types
│   │   ├── registry.kn    # ToolRegistry world + converge
│   │   ├── read.kn        # Read tool
│   │   ├── write.kn       # Write tool
│   │   ├── edit.kn        # Edit tool
│   │   ├── bash.kn        # Bash tool
│   │   ├── grep.kn        # Grep tool
│   │   ├── find.kn        # Find tool
│   │   ├── ls.kn          # Ls tool
│   │   └── utils.kn       # Format/truncation/diff
│   ├── providers/
│   │   ├── trait.kn       # LlmProvider trait
│   │   ├── registry.kn    # Provider registry world
│   │   ├── sse.kn         # SSE streaming parser
│   │   └── models.kn      # Model definitions
│   └── pipeline/
│       └── startup.kn     # Startup orchestrate pipeline
├── scripts/
│   ├── build.md           # Project scaffolding
│   ├── clean.md           # Session migration
│   ├── markscript.md      # Binary sync
│   └── markscript.md      # Environment check
├── build.kn               # Build entry (project authority)
├── template/
│   └── Mksfile.md         # MarkScript template
└── README.md              # Project readme
```

---

# Invariants

## Structural Laws

### **Law 1** — Tool Names Are Unique

> run

```markscript
print("let name_table = ...")
print("assert(len(unique_names) == len(all_tools))")
```

### **Law 2** — Session Has Active Pane

> run

```markscript
print("let session = ...")
print("assert(session.active_pane != none)")
```

### **Law 3** — Compaction Never Destroys Context

> run

```markscript
print("let before = session.context_size()")
print("compaction_pipeline(session)")
print("let after = session.context_size()")
print("assert(before == 0 or after > 0)")
```

### **Law 4** — Config Merge Preserves All Keys

> run

```markscript
print("let merged = merge_layers(defaults, markscript, home, project)")
print("let key_count = len(merged.keys())")
print("let max_keys = len(defaults.keys()) + len(markscript.keys())")
print("assert(key_count <= max_keys)")
print("assert(key_count >= len(defaults.keys()))")
```

### **Law 5** — Provider Capability Subset

> run

```markscript
print("let model = find_model(provider_id, model_name)")
print("assert(model.provider_id == provider_id)")
print("assert(is_subset(model.capabilities, provider_capabilities(provider_id)))")
```

---

# QuickRef

## Commands

| Command | Action |
|---------|--------|
| `kain check src/main.kn` | Typecheck the project |
| `kain build src/main.kn --target llvm` | Build native binary |
| `kain run src/main.kn --target llvm` | Run in one-shot mode |
| `kain test src/ --json` | Run all tests |
| `kain build src/main.kn --target llvm --debug` | Debug build |
| `kain run src/main.kn --target llvm -- --interactive` | Interactive mode |
| `kain run src/main.kn --target llvm -- --help` | Show help |

## Key Constructs

| Construct | Location | Layer | Purpose |
|-----------|----------|-------|---------|
| PiSettingsManager | src/config/settings.kn | L7 | 5-handler config merge actor |
| SessionTree | src/session/tree.kn | L7 | Append/branch/context actor |
| AgentActor | src/agent/agent.kn | L7 | Prompt/Continue/Steer/Abort cycle |
| AgentEventBus | src/agent/events.kn | L7 | 9 event types, publish/subscribe |
| ResourceLoader | src/resources/loader.kn | L7 | 7-handler resource loading actor |
| ToolRegistry | src/tools/registry.kn | L1+L3 | World + converge dispatch_tool |
| TrustStore | src/config/trust.kn | L1 | World + trust resolution |
| CompactionPipeline | src/session/compaction.kn | L4+L5 | Orchestrate + pulse |
| ProviderRegistry | src/providers/registry.kn | L0 | Provider registry world |
| StartupPipeline | src/pipeline/startup.kn | L0+L4 | 6-stage startup orchestrate |

## Layer Count

| Layer | Count |
|-------|-------|
| L0 | 24 files |
| L1 | 3 files |
| L3 | 1 file |
| L4 | 2 files |
| L5 | 1 file |
| L7 | 6 files |

## Actor Handlers Total

| Actor | Handlers |
|-------|----------|
| PiSettingsManager | 5 |
| SessionTree | 4 |
| AgentActor | 6 |
| AgentEventBus | 3 |
| ResourceLoader | 7 |
| Total | 25 |

---

# Pipeline

## Startup Orchestrate

```markscript
print("=== Startup Orchestrate Pipeline ===")
print("stage: init — runtime_init(), init logging, init error handler")
print("stage: parse — parse CliArgs from process args")
print("stage: migrate — run session migrations if detected (v1→v2→v3)")
print("stage: load — spawn config actor, load 4-layer config, init trust store")
print("stage: resolve — resolve provider, resolve model, validate capabilities")
print("stage: ready — dispatch to interactive or oneshot mode")
```

> run

```markscript
print("Pipeline dispatched via orchestrate:")
print("  stage init: kain runtime_init() with Pure")
print("  stage parse: c parse_args() after init")
print("  stage migrate: python migrate_if_needed() after parse guarded by law migration_required")
print("  stage load: converge config_load after migrate")
print("  stage resolve: converge resolve_provider after load")
print("  stage ready: dispatch main_loop or oneshot after resolve requires law all_ready")
```

---

# Pulse

## Compaction Timer

```markscript
print("=== Compaction Pulse ===")
print("Every 30s, jitter 5s — compact active session if over threshold")
```

> run

```markscript
print("pulse compact_timer every 30s jitter 5s:")
print("  let session = SessionTree.get_active()")
print("  let context_size = session.context_size()")
print("  if context_size > COMPACTION_THRESHOLD:")
print("    compaction_pipeline(session)")
```

## Heartbeat

```markscript
print("=== Heartbeat Pulse ===")
print("Every 10s, no jitter — emit Heartbeat event for keepalive")
```

> run

```markscript
print("pulse heartbeat every 10s:")
print("  AgentEventBus.emit(Heartbeat(timestamp = runtime_now()))")
```

## Idle Detection

```markscript
print("=== Idle Detection Pulse ===")
print("Every 60s, jitter 10s — detect inactivity and auto-compact")
```

> run

```markscript
print("pulse idle_detector every 60s jitter 10s:")
print("  let last_active = session_tree.last_active_time()")
print("  let now = runtime_now()")
print("  let idle_ms = now - last_active")
print("  if idle_ms > IDLE_TIMEOUT_MS:")
print("    AgentEventBus.emit(IdleDetected(idle_ms = idle_ms))")
---

# Markscript

This file is a MarkScript document. MarkScript uses markdown headings as domain
identifiers, bold **Stage** headers for stage blocks, `> run` lines for dispatch
intents, and `` ```markscript `` code blocks for executable content. Tables
carry structured data for config loading, architecture maps, and invariant laws.

## Rendering Pipeline

> run

```markscript
print("=== Markscript Rendering ===")
print("1. Parse headings → domain names")
print("2. Parse bold **Stage** blocks → stage steps")
print("3. Parse ```markscript blocks → executable content")
print("4. Parse markdown tables → structured config data")
print("5. Execute via markscript_loader or render to markdown")
```
