# MarkScript ~ The Prose-Native Scripting Runtime for Kain
 
> Your documentation is your program. Your README IS the executable.

```
mks.exe README.md              →  625 bytecode ops, 21 data tables, 47 fenced code blocks  →  EXECUTED
mks.exe game_engine.md         →  142 bytecode ops, 6 data tables, 9 intents              →  EXECUTED
mks.exe data_pipeline.md       →  171 bytecode ops, 8 data tables, 12 intents             →  EXECUTED
mks.exe servo_controller.md    →  161 bytecode ops, 6 data tables, 4 fenced code blocks   →  EXECUTED
```

**this README is a valid MarkScript program.** Run `mks.exe README.md` ~ it compiles itself, producing 625 opcodes from 567 lines of documentation. The headings are domains. The sections are routines. The tables are matrices. The code blocks are extracted. This file explains how it works AND IS ITSELF a working proof that it works.

---

## Table of Contents

2. [How It Works](#how-it-works)
3. [Quick Start](#quick-start)
4. [Writing MarkScript — Language Reference](#writing-markscript--language-reference)
5. [Examples](#examples)
6. [Architecture](#architecture)
7. [Build Pipeline](#build-pipeline)
8. [Advanced Features](#advanced-features)
9. [The Future](#the-future)
10. [FAQ](#faq)

**MarkScript removes the ladder entirely.** You write natural prose:
- Headings (`#`) define **domains** (modules, namespaces)
- Subheadings (`##`) define **routines** (functions, behaviors)
- Blockquotes (`>`) define **intents** (natural language commands)  
- Tables (`|`) define **data** (parsed into contiguous memory matrices)
- Fenced code blocks (`` ```kain ``) define **inline computation** (pass-through to the host language)

## How It Works

```
game_engine.md
      │
      ▼ ┌──────────────────────────────────────────────────────┐
      │  LEXER (lexer.kn)                                      │
      │  22 token types — headings, blockquotes, tables,       │
      │  fenced code blocks, list markers, HR, bold, italic    │
      │  Pure value semantics — no ptr<T>, no heap alloc       │
      └──────────────────────┬──────────────────────────────────┘
                             │ tokens
                             ▼
      ┌──────────────────────────────────────────────────────┐
      │  PARSER + COMPILER (ast.kn)                           │
      │  Single-pass: consumes tokens, emits flat bytecode    │
      │  • Tables → OP_PUSH_MATRIX + inline data              │
      │  • Intents → OP_PUSH_PARAM + OP_EXECUTE_CALL          │
      │  • Fenced code → OP_FENCED_CODE                       │
      │  • Handles, not pointers — GC-safe                   │
      └──────────────────────┬──────────────────────────────────┘
                             │ bytecode (Array<Int>)
                             ▼
      ┌──────────────────────────────────────────────────────┐
      │  VIRTUAL MACHINE (vm.kn)                              │
      │  17 opcodes — stack-based, IVT dispatch              │
      │  • Data table: handle → MatrixRecord                 │
      │  • IVT: phrase_hash → handler_id                     │
      │  • Call stack for subroutine dispatch                │
      │  • JMP/JZ for control flow                           │
      │  • ADD/SUB/PRINT for computation                     │
      └──────────────────────┬──────────────────────────────────┘
                             │ ExecResult
                             ▼
      ┌──────────────────────────────────────────────────────┐
      │  NATIVE EXECUTION                                     │
      │  Compiled through Kain's LLVM backend                 │
      │  Linked with native runtime for file I/O, etc.       │
      │  Standalone .exe — no dependencies                    │
      └──────────────────────────────────────────────────────┘
```

**Everything is authored in Kain.** The lexer, parser, compiler, VM — all 1,381 lines of Kain source. The only C code is Kain's native runtime (47 files), shared with every other Kain program.

---

## Quick Start

```bash
# Build MarkScript (requires Kain toolchain)
cd blades/markscript
kain build

# Run a markdown script
mks.exe examples/game_engine.md

# No args = demo mode with built-in test source
mks.exe
```

### Prerequisites

- **Kain toolchain** (`kain build`, `kain check`) — the Kain compiler + LLVM backend
- **Native runtime** — auto-linked during `kain build`
- **Windows, Linux, or WSL** — targets x86_64

### Building from Source

```bash
cd blades/markscript
kain check          # typecheck only (fast)
kain build          # full native build
./mks.exe           # run with demo source
./mks.exe my_script.md  # run a real file
```

---

## Writing MarkScript — Language Reference

MarkScript extends standard Markdown with semantic meaning. Every construct maps to a bytecode operation.

### Domains — `# Title`

A top-level heading creates a **domain** — a named scope for routines and data.

```markdown
# PhysicsSim
# DataPipeline
# ServoController
```

Compiles to: `OP_ENTER_DOMAIN hash("PhysicsSim")`

Domains are the top-level organizational unit. Every file should have at least one domain.

### Routines — `## Subtitle`

A second-level heading creates a **routine** — a named executable block.

```markdown
## ComputeForces
## ai_update
## render_frame
```

Compiles to: `OP_ROUTINE_HEADER hash("ComputeForces")`

Routines contain intents, tables, and fenced code blocks.

### Intents — `> phrase`

A blockquote is an **intent** — a natural language command dispatched through the Intent Vector Table (IVT).

```markdown
> apply gravity
> compute pathfinding
> cull frustum
> present swapchain
```

Compiles to:  
```
OP_PUSH_PARAM hash("apply gravity")
OP_EXECUTE_CALL
```

The IVT maps phrase hashes to handler IDs at runtime. When a handler is registered for `"apply gravity"`, the VM dispatches to it. Otherwise, the hash is accumulated and returned.

### Data Tables — `| col1 | col2 |`

A markdown table is a **matrix** — parsed into a contiguous `Array<Int>` stored in the VM's data table.

```markdown
| Object | Mass | Velocity_X | Velocity_Y |
|--------|------|------------|------------|
| Player | 80   | 0          | -9         |
| Crate  | 200  | 12         | 0          |
| Debris | 5    | 45         | -3         |
```

Compiles to:  
```
OP_PUSH_MATRIX handle=0 cols=4 rows=3 data_count=12
[80, 0, -9, 200, 12, 0, 5, 45, -3]
```

**Key properties:**
- Column header rows (separator rows with `---`) are detected and skipped
- Values are hashed to `Int` for uniform storage in the data table
- The matrix is accessible at runtime by handle ID
- **Zero raw pointers** — all data lives in the VM's `data_table` via handle-based lookup
- The VM stores `MatrixRecord { handle_id, cols, rows, data }` for each table

### Fenced Code Blocks — ` ```lang `

Fenced code blocks extract code content for the host runtime.

````markdown
```kain
fn pid_compute(setpoint: Int, measured: Int, kp: Int, ki: Int, kd: Int) -> Int:
    let error = setpoint - measured
    return error * kp / 100
```
````

Compiles to:  
```
OP_FENCED_CODE lang_hash=3284219 content_hash=<hash_of_content>
```

The language tag (`kain`, `c`, `python`, etc.) and content hash are stored in the VM's `code_blocks` array. Future versions will compile and execute these blocks.

### Inline Text

Plain text between structural tokens is consumed as `TOK_TEXTSTR` and skipped by the parser. This means you can write documentation freely:

```markdown
# PhysicsSim

The physics engine runs a fixed timestep loop.
Gravity is applied as a constant acceleration.

## ComputeForces
> apply gravity
| Mass | Velocity |
| 80   | 0        |
```

The sentence "The physics engine runs a fixed timestep loop." is parsed as text tokens and silently consumed. Only structural elements (headings, blockquotes, tables, fences) produce bytecode.

### All Recognized Markdown Constructs

| Construct | Token | Bytecode | Parsed? |
|-----------|-------|----------|---------|
| `# Title` | `TOK_HEADER1` | `OP_ENTER_DOMAIN` | ✅ |
| `## Title` | `TOK_HEADER2` | `OP_ROUTINE_HEADER` | ✅ |
| `###`–`######` | `TOK_HEADER3`–`6` | *(future)* | Lexed only |
| `> intent` | `TOK_BLOCKQUOTE` | `OP_PUSH_PARAM` + `OP_EXECUTE_CALL` | ✅ |
| `\| col \| col \|` | `TOK_TABLEPIPE` | `OP_PUSH_MATRIX` | ✅ |
| `` ```lang `` | `TOK_FENCE` | `OP_FENCED_CODE` | ✅ |
| `*italic*` | `TOK_ITALIC` | *(future)* | Lexed only |
| `**bold**` | `TOK_BOLD` | *(future)* | Lexed only |
| `` `code` `` | `TOK_CODE_SPAN` | *(future)* | Lexed only |
| `- list` | `TOK_LIST_UNORDERED` | *(future)* | Lexed only |
| `1. list` | `TOK_LIST_ORDERED` | *(future)* | Lexed only |
| `---` | `TOK_HR` | *(future)* | Lexed only |
| `[text](url)` | `TOK_LINK_TEXT` + `TOK_LINK_URL` | *(future)* | Lexed only |

---

## Examples

Every example in `examples/` compiles and executes with `mks.exe`:

### `game_engine.md` — Game Loop with Physics, AI, and Rendering

Three domains: `physics_tick` applies gravity, resolves collisions, updates transforms — with a table of 5 game objects. `ai_update` computes pathfinding and evaluates behavior trees — with a table of 4 agents. `render_frame` runs the full graphics pipeline — with a table of 5 render passes.

```bash
mks.exe examples/game_engine.md
# → 142 bytecode ops, 6 data tables, 9 dispatched intents
```

### `data_pipeline.md` — Streaming ETL Pipeline

Four routines: `ingest_stream` (Kafka consumer, batch poll, protobuf deserialize), `transform_batch` (schema mapping, enrichment, dedup, null filter), `validate_output` (constraint checks, referential integrity, checksum signing), `write_sink` (Parquet flush, catalog update, Prometheus metrics). Each stage has its own data table with realistic row counts and latency metrics.

```bash
mks.exe examples/data_pipeline.md
# → 171 bytecode ops, 8 data tables, 12 dispatched intents
```

### `servo_controller.md` — 6-Axis Servo with PID + C ISR

The most comprehensive example. Contains:
- An inline ` ```kain ` code block implementing PID compute
- A `calibrate` routine with a 6-joint servo calibration table (30 data points)
- A `move_to_position` routine with inverse kinematics, trajectory planning, and a C interrupt handler (```` ```c ````)
- A 6-axis position verification table with target/actual/error for all 6 DOF
- An `emergency_stop` routine with signal status table

```bash
mks.exe examples/servo_controller.md
# → 161 bytecode ops, 6 data tables, 4 fenced code blocks, 9 dispatched intents
```

### `test.md` — Minimal Smoke Test

A compact test file with 2 routines, 2 intents, 1 table, and 2 fenced code blocks.

```bash
mks.exe examples/test.md
# → 43 bytecode ops, 1 data table (3x4 = 12 ints), 4 code blocks
```

---

## Architecture

### Source Map

| File | LOC | Role |
|------|-----|------|
| `src/lexer.kn` | 355 | Tokenizer — 22 token types, value semantics, no heap alloc |
| `src/ast.kn` | 279 | Parser + Compiler — single-pass, emits flat bytecode with inline matrix data |
| `src/vm.kn` | 352 | Virtual Machine — 17 opcodes, stack, data table, IVT, call stack |
| `src/main.kn` | 143 | CLI driver — file reading, bytecode disassembler, VM execution loop |
| `src/test_lexer.kn` | 213 | Lexer test suite — 9 tests, all passing |
| `build.kn` | 39 | Build authority — Kain project DSL, source sets, native executable |
| **Total** | **1,381** | |

### The Lexer (`lexer.kn`)

Pure value-semantics tokenizer. Takes `LexerState` by value, returns `TokenResult { token, state }`. No `ptr<T>` parameters — avoids LLVM codegen issues with pointer field access.

**22 token types:** HEADER1-6, BLOCKQUOTE, TABLEPIPE, TEXTSTR, EOF, FENCE, LANG_TAG, FENCED_CODE, BOLD, ITALIC, CODE_SPAN, LIST_UNORDERED, LIST_ORDERED, LINK_TEXT, LINK_URL, HR, NEWLINE.

Key design decisions:
- **Newline mode**: When a non-structural token is followed by `\n`, a `TOK_NEWLINE` is emitted. Structural tokens (headings, blockquotes, fences) silently skip newlines.
- **Line-start detection**: Tracked via `at_line_start` flag. Enables line-start-only constructs (HR, list markers) without false positives in the middle of text.
- **Fence state machine**: Triple backtick detection is handled at the token level. The parser reads `TOK_LANG_TAG` and `TOK_FENCED_CODE` tokens emitted by the lexer.

### The Parser+Compiler (`ast.kn`)

Single-pass: consumes tokens from `LexerState`, emits flat `Array<Int>` bytecode.

**6 bytecode opcodes emitted by the parser:**

| Opcode | Value | Meaning |
|--------|-------|---------|
| `OP_HALT` | 0 | Terminate execution |
| `OP_ENTER_DOMAIN` | 1 | Open a domain scope |
| `OP_ROUTINE_HEADER` | 2 | Open a routine scope |
| `OP_PUSH_PARAM` | 3 | Push a hashed intent phrase |
| `OP_EXECUTE_CALL` | 4 | Dispatch accumulated params |
| `OP_PUSH_MATRIX` | 5 | Embed inline matrix data |
| `OP_FENCED_CODE` | 6 | Store fenced code block hash |

**Matrix encoding:** When a table is parsed, the raw values are hashed to `Int` and packed into the bytecode stream:
```
OP_PUSH_MATRIX, handle_id, cols, rows, data_count, v0, v1, ..., vN
```
The VM reconstructs the `MatrixRecord` at execution time. No raw pointers cross the bytecode boundary — handles are `Int` values safe from GC and tagged-int encoding.

**Fenced code encoding:**
```
OP_FENCED_CODE, lang_hash, content_hash
```

### The Virtual Machine (`vm.kn`)

Stack-based VM with 17 opcodes. Pure Kain — no hand-written assembly, no C interop in the VM core.

**11 additional VM-only opcodes for advanced use:**

| Opcode | Value | Meaning |
|--------|-------|---------|
| `OP_PUSH_STACK` | 7 | Push operand to stack |
| `OP_POP_STACK` | 8 | Pop stack to accumulator |
| `OP_DUP` | 9 | Duplicate top of stack |
| `OP_CALL` | 10 | IVT lookup + subroutine call |
| `OP_RET` | 11 | Return from subroutine |
| `OP_JMP` | 12 | Unconditional jump |
| `OP_JZ` | 13 | Jump if zero |
| `OP_ADD` | 14 | Stack add |
| `OP_SUB` | 15 | Stack subtract |
| `OP_PRINT` | 16 | Pop and print |

**VM state:**
```kain
pub struct MarkScriptVM:
    ip:             Int                     // instruction pointer
    accumulator:    Int                     // primary return register
    stack:          Array<Int>              // operand stack
    data_table:     Array<MatrixRecord>     // handle → matrix data
    data_table_cnt: Int                     // next free handle
    code_blocks:    Array<CodeBlockRecord>  // fenced code blocks
    call_stack:     Array<Int>              // return addresses
    ivt:            Array<IVTEntry>         // intent vector table
    ivt_count:      Int                     // registered handlers
```

The VM uses **value semantics** — `execute_bytecode(vm, bc)` takes VM by value and returns `ExecResult`. This avoids `ptr<T>` codegen issues while still allowing the data_table and code_blocks to be returned to the caller.

### The CLI Driver (`main.kn`)

Entry point that:
1. Reads args via `process_arg()` with a workaround for a compiler tag-check bug (see tree-kn's `get_user_args()` for details)
2. Reads the source file via `fs_read_text()`
3. Creates a lexer state via `create_lexer(source)`
4. Compiles via `compile_source(lexer_state)` → `Array<Int>` bytecode
5. Disassembles the bytecode to stdout
6. Executes via `init_vm()` + `execute_bytecode(vm, bc)`
7. Reports data table and code block stats
8. Calls a stub handler with the final accumulator

---

## Build Pipeline

MarkScript is a standard Kain blade. Build it like any other Kain project:

```bash
kain build          # typecheck + compile to LLVM IR + link native .exe
kain check          # typecheck only (fast for iteration)
```

The `build.kn` defines:
- `source_set("mks-sources")` with glob `src/**/*.kn`
- `check_task("check-llvm")` for typechecking
- `native_executable("root-executable")` for the final `mks.exe`

Output goes to `$blade/mks.exe` (project root) and `.kain/out/` (build artifacts).

### LLVM Codegen

The Kain compiler emits textual LLVM IR (`.ll`), then clang compiles and links against the native runtime. The current binary is **1.6 MB** for debug mode — it includes the full Kain standard library surface used (text, fs, diagnostics, runtime).

### Runtime Contract

The compiler emits a `runtime_contract.json` with:
- Required capabilities (memory, ownership, raw pointers)
- Service bindings (diagnostics, memory, contract)
- All typed items (constants, types, functions, worlds, actors)

---

## Advanced Features

### Zero-Copy Matrix Injection

This is MarkScript's most novel feature. When the parser encounters a markdown table:

```markdown
| Object | Mass | Velocity |
| Player | 80   | 0        |
| Crate  | 200  | 12       |
```

The values are hashed to `Int`, packed inline in the bytecode stream, and reconstructed into `MatrixRecord { handle_id, cols, rows, data }` at VM execution time. The VM's data table holds the result, accessible by handle.

**What this enables:**
- Game object tables → contiguous arrays for GPU upload
- Pipeline metrics → ready for aggregation queries
- Servo calibration data → matrix math for inverse kinematics
- No JSON serialization, no CSV parsing, no schema declarations

### Intent Vector Table (IVT)

The IVT maps hashed natural language phrases to handler IDs:

```kain
// Registration (in Kain, at init time)
vm::register_handler(vm, hash("apply gravity"), HANDLER_GRAVITY)

// Dispatch (inside VM execution)
let handler_id = lookup_handler(vm, target_hash)
```

This decouples the markdown script from the implementation. Domain experts write intents. Systems engineers register handlers.

### Fenced Code Block Extraction

Code blocks with language tags are extracted and stored in the VM's `code_blocks` array. This enables:

- **Inline Kain**: ` ```kain ` blocks contain executable Kain code for future compilation
- **Inline C/ASM**: ` ```c ` blocks for hardware-level control (servo ISRs, memory barriers)
- **Documentation**: ` ```python ` for Python interop stubs
- The lang tag and content hash are available for dispatch at runtime

---

## The Future

### Near Term (Next Session)

| Feature | Priority |
|---------|----------|
| **REPL mode** — `mks` with no file arg starts a live read-eval-print loop | 🔴 P0 |
| **IVT wiring** — register real Kain function handlers instead of a stub | 🔴 P0 |
| **`--output-kn`** — render bytecode as Kain source for debugging | 🟡 P1 |
| **Error recovery** — graceful handling of malformed markdown | 🟡 P1 |
| **Lexer tests** — the existing `test_lexer.kn` has 9/9 passing, integrate into build | 🟡 P1 |

### Medium Term

| Feature | Impact |
|---------|--------|
| **Fenced code execution** — compile and run ` ```kain ` blocks inline | Game-changing |
| **Kain FFI bridge** — call `std::fs`, `std::gpu`, etc. from intents | Massive |
| **`include` directive** — `@import other.md` for multi-file projects | Major |
| **Table schema inference** — detect column types (Int, Float, String) | Major |
| **World/actor generation** — `# Store X` → `world X: state ...` | Paradigm shift |

### Long Term — The Real Vision

- **LLM-native programming**: Write markdown, get a working binary. Zero syntax errors because markdown has no syntax errors.
- **Literate programming at scale**: Game designers write AI scripts in markdown. Engineers write IVT handlers in Kain with worlds, actors, and formal verification.
- **The bus factor is zero**: The README IS the program. Every new hire reads the docs and sees the entire control flow.
- **PRs on docs ARE PRs on code**: "This table has a bug in row 3" is a valid code review comment.

---

## FAQ

### Why Markdown? Why not YAML/TOML/JSON?

Markdown is the most widely understood structured text format on earth. Every developer, designer, and technical writer knows it. JSON is for machines. YAML is for configs. Markdown is for humans.

### How fast is it?

The VM is authored in Kain and compiled through LLVM to native code. There's no interpreter overhead beyond the opcode dispatch loop. Matrix data is stored as contiguous `Array<Int>` — no indirection, no boxing. The binary is a native .exe with no interpreter dependency.

### Can I call Kain from MarkScript?

Currently, intents are dispatched through the IVT to stub handlers. Future versions will wire the IVT to real Kain function pointers, enabling `> write file` to call `std::fs::fs_write_text()`.

### Can I call MarkScript from Kain?

Yes — `mks.exe` is a regular Kain native executable. It can be spawned as a subprocess, called via FFI, or embedded as a library. The `compile_source()` function is a pure function from `LexerState` to `Array<Int>` that can be called from any Kain code.

### Does it work on Linux?

The Kain toolchain targets x86_64 Windows and Linux. Build the blade under WSL or Linux to get a native Linux binary. The LLVM IR is platform-independent.

### Is this production-ready?

**No.** It's a working prototype built in a single session. The pipeline compiles and runs real files, but there's no error recovery, no REPL, no real IVT handlers, and no fenced code execution. It's a proof of concept that the architecture works.

---

## References

- **Source**: `blades/markscript/src/` — 5 Kain source files (1,381 LOC)
- **Examples**: `blades/markscript/examples/` — 9 markdown scripts (400 LOC total)
- **Build**: `blades/markscript/build.kn` — Kain build authority
- **Kain docs**: `docs/RULEBOOK.md` — The decision ladder for writing Kain
- **Kain GLOSSARY**: `GLOSSARY.MD` — Every term mapped to its location
- **Kain stdlib**: `stdlib/` — 65+ modules, 3,500+ symbols

---

*Built with Kain — the non-Von Neumann systems language with a compiler-owned semantic stack.*

*"Your documentation is your program."*
