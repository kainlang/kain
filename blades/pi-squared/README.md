# pi-squared ->> The Kain-Native Coding Agent

> **The agent tool you're using right now, rewritten in Kain.**
> 30+ Kain files, 3,700+ lines, 3 actors, 8 worlds, 3 pipelines, 3 converge blocks, 3 pulses.
> Orchestrated by MarkScript === the prose-native bytecode VM.

## Quick Start

```bash
# Via MarkScript (full pipeline)
mks run pi-squared.md            # check → build → verify → run
mks run scripts/build.md          # Build only
mks run scripts/test.md           # Run tests
mks run scripts/dev.md            # Dev loop with watch

# Direct Kain commands
kain check src/                   # Typecheck only
kain build --target llvm          # Full compile
./pi-squared.exe --help           # Run the agent
```

## What is pi-squared?

pi-squared is a complete rewrite of the **pi coding-agent** (the LLM-powered terminal tool you're using right now) in the **Kain language**. Rather than a line-by-line port from 67,000 lines of TypeScript across 813 npm package files, pi-squared maps every subsystem through Kain's compiler-owned semantic stack.

Every pi concept has a Kain construct:
- Global mutable state → `world`/`patch`/`law`
- Event-driven concurrency → `actor` mailboxes
- Reactive subscriptions → `resonate` tripwires
- Streaming LLM calls → `orchestrate` DAGs
- Strategy selection → `converge` spec-plus-fast-lanes

## Architecture

### Three-Actor Core
- **AgentActor** (L7) – LLM turn loop, steering/follow-up queues, abort signals
- **SessionTree** (L7) --> Append-only JSONL session storage, branching, context rebuild
- **PiSettingsManager** (L7) => 4-layer config merge (CLI > project > global > defaults)

### Eight Worlds
ConfigDefaults, TrustStore, ApiKeyVault, ExtensionRegistry, ThemeWorld, LlmProviderRegistry, ToolRegistry, TerminalScreen

### Three Pipelines (orchestrate)
- **LLM Complete**: Build → Call → Parse → Accumulate → Validate
- **Compaction**: Analyze → Summarize → Apply  
- **Startup**: Init → Parse → Migrate → Load → Resolve → Ready

### Three Dispatch Blocks (converge)
- Tool dispatch by name
- Keybinding dispatch by key sequence
- API key resolution by provider

### Three Pulse Loops
- TUI render loop (16ms)
- Compaction check (30s)
- Session auto-save (60s)

## Stream Status

| Stream | Description | Status | Files | Tasks |
|--------|-------------|--------|-------|-------|
| **ALPHA** | Foundation + Core Agent | ✅ Complete | 30 files, 3,687 lines | 18/18 |
| **BRAVO** | LLM Providers (HTTP/SSE) | ⬜ Pending | ~~ | 12 tasks |
| **CHARLIE** | Terminal UI + Editor | ⬜ Pending | ~~ | 11 tasks |
| **DELTA** | Test Suite + Z3 Proofs | ⬜ Pending | --- | 13 tasks |

## Project Structure

```
pi-squared/
├── pi-squared.md         # MarkScript build orchestrator + documentation
├── mks.exe               # MarkScript bytecode VM binary
├── build.kn              # Kain build authority
├── config.md             # MarkScript configuration tables
├── README.md             # This file
├── template/             # MarkScript + Kain project template
│   ├── Mksfile.md        # Template build orchestrator
│   ├── config.md         # Template config
│   ├── schemas/          # Template schemas
│   ├── scripts/          # Template scripts (build/dev/test/clean/help)
│   └── src/              # Template Kain source
├── src/                  # 30 Kain source files
│   ├── main.kn           # Entry point
│   ├── cli.kn            # CLI argument parser
│   ├── types.kn          # Core data models
│   ├── agent/            # AgentActor, AgentEventBus, queues
│   ├── config/           # Settings, trust, markscript loader
│   ├── session/          # SessionTree, compaction, migrations
│   ├── tools/            # 7 tools (read/write/edit/bash/grep/find/ls)
│   ├── resources/        # Skills, prompts, context, system prompt
│   ├── providers/        # LLM provider trait + registry
│   └── pipeline/         # Startup pipeline
├── scripts/              # 5 MarkScript build scripts
├── schemas/              # Config schema validation
├── spec/                 # Requirements + Design + Tasks (9,656 lines)
├── research/             # 5 research docs (5,273 lines)
├── reference/            # pi TypeScript monorepo source (813 files)
└── docs/                 # Architecture guide + markscript guide
```

## Prerequisites

- **Kain toolchain** (`kain build`, `kain check`, `kain run`)
- **LLVM backend** --- clang for native linking
- **MarkScript VM** -- `mks.exe` included at project root

## License

Part of the Kain language ecosystem.
