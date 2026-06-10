# MarkScript — The Prose-Native Scripting Runtime for Kain

> **Your documentation is your program. Your README IS the executable.**

MarkScript is a **markdown-native bytecode VM** that serves as Kain's companion language for configuration, orchestration, and executable documentation. It compiles through Kain's LLVM backend to native code. No interpreter, no runtime dependency — a standalone `.exe`.

**Core property:** Markdown has no syntax errors. Every `#`, `>`, `|`, and `` ``` `` is valid. The only errors are *runtime* errors — name not found, arity mismatch, bounds violation, import failure.

```
mks run README.md                  → 625 bytecode ops, 21 data tables → EXECUTED
mks run game_engine.md             → 142 bytecode ops, 6 data tables  → EXECUTED
mks run servo_controller.md        → 161 bytecode ops, 6 data tables  → EXECUTED
mks run fizzbuzz.md                → 21 VM opcodes, full FizzBuzz     → EXECUTED
```

**This README is a valid MarkScript program.** Running `mks run README.md` compiles itself — the headings are domains, sections are routines, tables are data matrices, and code blocks are extracted.

---

## Quick Start

```bash
# Build the MarkScript runtime (requires Kain toolchain)
cd blades/markscript
kain build

# Run a markdown script
mks run examples/game_engine.md

# Validate a script without executing
mks check examples/data_pipeline.md

# Disassemble bytecode
mks disasm examples/servo_controller.md
```

### Prerequisites

- **Kain toolchain** (`kain build`, `kain check`)
- **Native runtime** — auto-linked during `kain build`
- **Windows, Linux, or WSL** — targets x86_64

---

## CLI Reference

### Subcommands

| Subcommand | Usage | Description |
|-----------|-------|-------------|
| `run` | `mks run <file.md>` | Compile and execute (default subcommand) |
| `check` | `mks check <file.md>` | Compile-only validation, no VM execution |
| `disasm` | `mks disasm <file.md>` | Dump bytecode opcodes and exit |
| `repl` | `mks repl` | Interactive REPL — type intents, see results |
| `eval` | `mks eval '<intent>'` | One-shot intent compilation and dispatch |
| `init` | `mks init <name>` | Scaffold a new markscript project directory |
| `handlers` | `mks handlers` | List all registered IVT handlers |
| `doc` | `mks doc <file.md>` | Render clean documentation (strip VM output) |

### Flags

| Flag | Description |
|------|-------------|
| `-h, --help` | Show help message and exit |
| `-v, --version` | Show version and exit |
| `-q, --quiet` | Suppress runtime logging and telemetry |
| `--json` | Output structured JSON (where supported) |

### Exit Codes

| Code | Meaning |
|------|---------|
| `0` | Success |
| `1` | Help or version shown (no error) |
| `2` | Error — file not found, parse failure, runtime |
| `3` | Unknown subcommand or flag |

### Examples

```bash
# Default mode (no subcommand = run)
mks examples/game_engine.md
mks game_engine.md

# Validate bytecode without executing
mks check examples/data_pipeline.md

# Debug bytecode
mks disasm examples/servo_controller.md

# Interactive session
mks repl

# One-shot intent
mks eval '> print "hello world"'

# Scaffold a new project
mks init my-pipeline

# List registered IVT handlers
mks handlers

# Suppress runtime telemetry
mks -q run examples/fizzbuzz.md
```

---

## Writing MarkScript — Language Reference

### Domains — `# Title`

A top-level heading creates a **domain** — a named scope for routines and data.

```markdown
# PhysicsSim
# DataPipeline
# GameConfig
```

Compiles to: `OP_ENTER_DOMAIN hash("PhysicsSim")`

### Routines — `## Subtitle`

A second-level heading creates a **routine** — a named executable block.

```markdown
## ComputeForces
## physics_tick
## render_frame
```

Compiles to: `OP_ROUTINE_HEADER hash("ComputeForces")`

### Intents — `> phrase`

A blockquote is an **intent** — a natural language command dispatched through the Intent Vector Table (IVT).

```markdown
> apply gravity
> compute pathfinding
> present swapchain
```

Compiles to:
```
OP_PUSH_PARAM hash("apply gravity")
OP_EXECUTE_CALL
```

The IVT maps phrase hashes to handler IDs at runtime. When a handler is registered for `"apply gravity"`, the VM dispatches to it. Built-in handlers include `print`, `assert`, `read file`, `write file`, `run`, and `import kain`.

### Data Tables — `| col1 | col2 |`

A markdown table is a **matrix** — parsed into a contiguous `Array<Int>` stored in the VM's data table.

```markdown
| Object | Mass | Velocity_X | Velocity_Y |
|--------|------|------------|------------|
| Player | 80   | 0          | -9         |
| Crate  | 200  | 12         | 0          |
```

Compiles to:
```
OP_PUSH_MATRIX handle=0 cols=4 rows=2 data_count=8
[80, 0, -9, 200, 12, 0]
```

**Properties:**
- Separator rows (`|---|`) are detected and skipped
- The row before the separator is the header (column names)
- Values are stored contiguous in bytecode — zero copy, zero indirection
- Tables are accessible at runtime by handle ID

### Fenced Code Blocks — `` ```lang ``

Fenced code blocks extract code content for the host runtime.

````markdown
```kain
fn pid_compute(setpoint: Int, measured: Int, kp: Int) -> Int:
    let error = setpoint - measured
    return error * kp / 100
```
````

Compiles to:
```
OP_FENCED_CODE lang_hash=3284219 content_hash=<hash_of_content>
```

Language tag and content hash are stored in the VM's `code_blocks` array for dispatch at runtime.

### Inline Text — Documentation

Plain text between structural tokens is consumed as `TOK_TEXTSTR` and silently skipped by the parser. Write documentation freely between structural elements.

```markdown
# PhysicsSim

The physics engine runs a fixed timestep loop. This text is documentation.
It produces no bytecode.

## ComputeForces
> apply gravity
```

### The Markscript Mini-Language (Inside ` ```markscript ` Blocks)

Within ` ```markscript ` fenced blocks, you can write imperative code:

```markscript
let n = 1
while n <= 100:
    if n % 15 == 0:
        print("FizzBuzz")
    elif n % 3 == 0:
        print("Fizz")
    n = n + 1
```

Supports: variables, `while` loops, `if/elif/else`, arithmetic (`+`, `-`, `*`, `/`, `%`), `print()`, `str()`, `len()`.

---

## Architecture

### Pipeline

```
.md source
    │
    ▼ ┌───────────────────────────┐
    │  LEXER (lexer.kn)           │
    │  22 token types - headings, │
    │  blockquotes, tables, fences│
    │  Pure value semantics       │
    └──────────┬──────────────────┘
               │ tokens
               ▼
    ┌───────────────────────────────┐
    │ PARSER + COMPILER (parser.kn) │
    │ Single-pass: tokens→bytecode  │
    │ • Tables → OP_PUSH_MATRIX     │
    │ • Intents → OP_PUSH_PARAM +   │
    │              OP_EXECUTE_CALL   │
    │ • @import resolution          │
    └──────────┬────────────────────┘
               │ bytecode (Array<Int>)
               ▼
    ┌──────────────────────────────┐
    │ VIRTUAL MACHINE (vm.kn)      │
    │ 20 opcodes, stack-based      │
    │ IVT dispatch for intents     │
    │ Data table, call stack, vars │
    │ JMP/JZ/JN for control flow   │
    └──────────┬───────────────────┘
               │ ExecResult
               ▼
    ┌──────────────────────────────┐
    │ HANDLER DISPATCH (bridge.kn) │
    │ 12 built-in handlers:        │
    │ fs_read, fs_write, fs_exists │
    │ process_run, process_spawn   │
    │ import_kain, assert, println │
    │ str, len, push, pop          │
    └──────────────────────────────┘
```

### Source Map

| File | LOC | Role |
|------|-----|------|
| `src/lexer.kn` | ~350 | Tokenizer — 22 token types, value semantics |
| `src/parser.kn` | ~500 | Parser + Compiler — single-pass, bytecode emission, `@import` |
| `src/vm.kn` | ~620 | Virtual Machine — 20 opcodes, stack, data table, IVT |
| `src/main.kn` | ~510 | CLI driver, subcommand dispatch, REPL, handler loop |
| `src/cli.kn` | ~310 | Argument parser, usage text, MksConfig |
| `src/bridge.kn` | ~480 | IVT handler registry, 12 built-in Kain stdlib bridges |
| `src/types.kn` | ~210 | MarkValue, MatrixRecord, type inference |
| `src/error.kn` | ~150 | MarkError, formatting, did-you-mean |
| `src/import.kn` | ~220 | `@import` resolution, cycle detection |
| **Total** | **~3,400** | |

### The IVT (Intent Vector Table)

The IVT maps hashed natural-language phrases to handler IDs. Twelve built-in handlers bridge to the Kain stdlib:

| Handler | Intent Pattern | Kain Bridge |
|---------|---------------|-------------|
| `FN_FS_READ_TEXT` | `> read file "path"` | `std::fs::fs_read_text()` |
| `FN_FS_WRITE_TEXT` | `> write file "path" "content"` | `std::fs::fs_write_text()` |
| `FN_FS_EXISTS` | `> file exists "path"` | `std::fs::fs_path_exists()` |
| `FN_PROCESS_OUTPUT` | `> run "command"` | `std::process::process_spawn()` |
| `FN_PROCESS_SPAWN` | `> spawn "command"` | Full process API |
| `FN_IMPORT_KAIN` | `> import kain "module"` | Kain module loader |
| `FN_ASSERT` | `> assert value expected` | Equality check on error |
| `FN_PRINTLN` | `> print value` | `println(str(value))` |
| `FN_STR` | Implicit | `str()` conversion |
| `FN_LEN` | Implicit | `len()` container length |
| `FN_PUSH` | Implicit | Push to VM stack |
| `FN_POP` | Implicit | Pop from VM stack |

Add custom handlers by registering Kain functions into the IVT — see `docs/IVT_AND_HANDLERS.md`.

### `@import` — Multi-File Composition

```markdown
@import "path/to/other.md"
@import "../shared/handlers.md"
```

Imports are resolved at compile-time, before bytecode emission. Rules:
- Paths resolve relative to the importing file
- Imported domains and routines merge into the calling namespace
- Duplicate domain names: last import wins (warning emitted)
- Circular imports: hard error at compile time
- Max depth: 16

### Error Model

```
Error: <kind>: <message>
  at line <N>, domain "<domain>", routine "<routine>"
  suggestion: <suggestion>
```

**Error kinds:** `name error`, `arity error`, `bounds error`, `type error`, `import error`, `circular import`.

When an IVT lookup fails, the error engine searches registered handlers for the closest match (edit distance ≤ 3):

```
Error: name error: unknown intent "apply graviti"
  at line 14, domain "PhysicsSim", routine "physics_tick"
  suggestion: did you mean "apply gravity"?
```

---

## Embedding MarkScript in Kain

Any Kain program can embed the MarkScript VM:

```kain
let content = fs_read_text("config.md")
let lex = create_lexer(content)
let bc = compile_source(lex)
let vm = init_vm_with_builtins()
let result = execute_bytecode(vm, bc)
// result.vm.data_table — parsed tables
// result.value — final accumulator
```

Both MarkScript and Kain compile through the same LLVM backend to the same native binary. Both link against the same C runtime. Both access the same stdlib. The separation is at the language level only.

---

## Project Structure

```
markscript/
├── README.md                  ← this file (self-executing docs)
├── MARKSCRIPT.MD              ← canonical specification & contract
├── build.kn                   ← Kain project authority
├── KAIN.toml                  ← blade metadata
├── docs/                      ← authoring guides
│   ├── GETTING_STARTED.md
│   ├── AUTHORING_GUIDE.md
│   ├── CLI_REFERENCE.md
│   ├── IVT_AND_HANDLERS.md
│   └── POSSIBILITIES.md
├── src/                       ← Kain source (the VM implementation)
│   ├── main.kn                — CLI, subcommand dispatch
│   ├── cli.kn                 — argument parser
│   ├── lexer.kn               — 22-token tokenizer
│   ├── parser.kn              — single-pass bytecode compiler
│   ├── vm.kn                  — 20-opcode stack VM
│   ├── types.kn               — MarkValue, MatrixRecord
│   ├── bridge.kn              — IVT handler registry
│   ├── error.kn               — runtime error formatting
│   ├── import.kn              — @import resolution
│   ├── test_lexer.kn          — tokenizer tests
│   └── test_markscript_parser.kn — parser tests
├── examples/                  ← executable example scripts
│   ├── game_engine.md         — physics/AI/rendering loop
│   ├── data_pipeline.md       — streaming ETL pipeline
│   ├── servo_controller.md    — 6-axis servo with PID
│   ├── fizzbuzz.md            — full FizzBuzz in markscript
│   ├── pong.md                — complete Pong game spec (30+ intents)
│   ├── compute_pipeline.md    — neural compute pipeline
│   ├── render_loop.md         — GPU render loop
│   └── ...
└── projects/
    └── mks-ultra/             — Markscript-embedded Kain physics engine
        ├── build.kn           — references markscript src/
        ├── src/engine/        — Vec3, RigidBody, collision, integrator
        └── scripts/sim.md     — N-body simulation in Markscript
```

---

## Examples

| Example | Domains | Routines | Data | What It Shows |
|---------|---------|----------|------|---------------|
| `game_engine.md` | 3 | 3 | 3 tables | Physics, AI, rendering loop |
| `data_pipeline.md` | 1 | 4 | 4 tables | ETL pipeline with latency metrics |
| `servo_controller.md` | 1 | 4 | 2 tables | PID control + C ISR |
| `fizzbuzz.md` | 1 | 3 | 0 | Full mini-language: loops, if/else, vars |
| `pong.md` | 8 | 24 | 9 tables | Complete game architecture in prose |

---

## What MarkScript Is NOT

| Not This | Because |
|----------|---------|
| A general-purpose language | That's Kain. MarkScript orchestrates Kain |
| A replacement for Kain | No type system, memory model, ownership, or effects |
| A compiler platform | Cannot express generics or code generation |
| A build system | Dispatches TO Kain's build system through intents |
| A package manager | Dependency management belongs to Kain |

---

## Further Reading

- **`MARKSCRIPT.MD`** — Canonical specification, invariants, 1.0 contract
- **`docs/GETTING_STARTED.md`** — Build, run, first script in 2 minutes
- **`docs/AUTHORING_GUIDE.md`** — Complete markdown→semantics mapping
- **`docs/CLI_REFERENCE.md`** — Full subcommand and flag reference
- **`docs/IVT_AND_HANDLERS.md`** — The intent dispatch system explained
- **`docs/POSSIBILITIES.md`** — What you can build with MarkScript

---

*Built with [Kain](https://kain-lang.org) — the non-Von Neumann systems language with a compiler-owned semantic stack.*

*"Your documentation is your program."*
