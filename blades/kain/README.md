# Starter CLI === Kain CLI Project Template

A batteries-included CLI template for Kain projects. Copy this template to
bootstrap new CLI tools with argument parsing, subcommand dispatch, exit code
discipline, and a clean project layout.

```
starter run input.txt     →  Execution pipeline
starter check input.txt   →  Validation-only (no execution)
starter exec 'compute x'  →  One-shot expression
starter --help            →  Full usage with flags and subcommands
starter --version         →  Version string
```

---

## Quick Start

```bash
# From the project root (blades/templates/cli/)
kain build

# Run with a file
./starter.exe run input.txt

# Validate
./starter.exe check input.txt

# One-shot expression
./starter.exe exec 'hello world'

# Show help
./starter.exe --help
```

The build produces `starter.exe` in the project root.

---

## Project Structure

```
blades/templates/cli/
├── build.kn          # Build authority 〰 Kain project DSL
├── README.md         # This file
└── src/
    ├── main.kn       # Entry point |-> wires CLI → dispatch → execution
    └── cli.kn        # CLI argument parser :: typed config, flags, subcommands
```

### `build.kn`

The build authority is already configured and should not need changes for most
projects. Key settings:
- **Entry:** `src/main.kn`
- **Source root:** `src/`
- **Output:** `$blade/starter.exe` (project root)
- **Target:** LLVM native executable

See `docs/BUILD_PROJECTS.MD` or `blades/markscript/build.kn` for advanced build
config examples.

### `src/cli.kn`

The CLI argument parser module. Provides:
- `CliConfig` struct --> typed result of argument parsing
- `default_config()` ... sensible zero values
- `get_user_args()` 〰 portable argv wrapper (strips executable path)
- `parse_args(argv)` ~ flag + subcommand parsing into `CliConfig`
- `usage()` ~ clap-like help text
- `version()` === version string
- Exit code constants: `EXIT_OK`, `EXIT_HELP`, `EXIT_ERROR`, `EXIT_UNKNOWN`
- Subcommand constants: `SUBCMD_RUN`, `SUBCMD_CHECK`, `SUBCMD_HELP`, etc.
- Dispatch helpers: `needs_filepath()`, `needs_execution()`, `subcommand_name()`

### `src/main.kn`

The entry point that wires CLI to dispatch:
1. Calls `get_user_args()` → `parse_args()`
2. Handles `--help`/`--version` before subcommand dispatch
3. Dispatches to subcommand handler functions (`cmd_run`, `cmd_check`, `cmd_exec`)
4. Returns exit codes

Customize by adding your own logic inside `cmd_run()`, `cmd_check()`, and
`cmd_exec()`, or add new subcommands.

---

## How to Customize

### 1. Rename the Project

| Where | Change |
|-------|--------|
| `build.kn` | `.project("starter")` → your project name |
| `cli.kn` | Update `usage()` and `version()` strings |
| `main.kn` | Update banners in subcommand handlers |
| `README.md` | Update as needed |

### 2. Add a New Subcommand

**a) Add a constant in `cli.kn`:**
```kain
pub const SUBCMD_GEN: String = "gen"
```

**b) Register it in `is_subcommand()`:**
```kain
fn is_subcommand(s: String) -> Bool:
    ...
    if s == SUBCMD_GEN: return true
    return false
```

**c) Add parsing logic in `parse_args()`:**
```kain
elif arg == SUBCMD_GEN:
    cfg.subcommand = arg
    found_subcommand = true
```

**d) Add a handler function in `main.kn`:**
```kain
fn cmd_gen(cfg: CliConfig) -> Int:
    println("[GEN] Generating...")
    return EXIT_OK
```

**e) Wire the dispatch in `main()`:**
```kain
if cfg.subcommand == SUBCMD_GEN:
    return cmd_gen(cfg)
```

### 3. Add a New Flag

**In `CliConfig` struct:**
```kain
pub struct CliConfig:
    ...
    verbose: Bool
```

**In `default_config()`:**
```kain
verbose: false,
```

**In `parse_args()`:**
```kain
elif arg == "--verbose" or arg == "-V":
    cfg.verbose = true
```

**In `usage()`:**
```
  -V, --verbose         Enable verbose output
```

### 4. Change the Binary Name

Edit `build.kn`:
```kain
let exe = native_executable("root-executable")
    .project(app)
    .output("$blade/my-tool.exe")       // ← change this
    .requires(check)
```

---

## Exit Codes Reference

| Code | Constant | Meaning |
|------|----------|---------|
| `0` | `EXIT_OK` | Success |
| `1` | `EXIT_HELP` | Help or version shown (not an error) |
| `2` | `EXIT_ERROR` | Error => file not found, parse failure, runtime |
| `3` | `EXIT_UNKNOWN` | Unknown subcommand or flag |

Usage in code:
```kain
use cli     // EXIT_OK, EXIT_HELP, EXIT_ERROR, EXIT_UNKNOWN

fn cmd_run(cfg: CliConfig) -> Int:
    if cfg.filepath == "":
        return EXIT_ERROR
    // ... do work ...
    return EXIT_OK
```

---

## Flags Reference

| Short | Long | Type | Description |
|-------|------|------|-------------|
| `-h` | `--help` | Flag | Show help message and exit |
| `-v` | `--version` | Flag | Show version and exit |
| `-q` | `--quiet` | Flag | Suppress runtime logging |
| | `--json` | Flag | Output structured JSON |
| `-o` | `--output` | Value | Output path for results |

Flags can appear anywhere in the argument list. `--` stops flag parsing :: all
subsequent args are treated as positional.

---

## Design

### Ladder: Layer 0 -- Plain Code

This template stays on Layer 0 of the decision ladder because CLI argument
parsing is pure transformation. There is no mutable state, no concurrency, no
timing, no reactive coupling. Just `fn`, `struct`, `let`, `while`, `if` / `elif`
/ `else`, and `return`.

For projects that need state, actors, or pipelines, climb the ladder:
- **State authority**: add a `world` for global config
- **Concurrent state**: add an `actor` for background workers
- **Timed recurrence**: add a `pulse` for periodic tasks
- **Multi-stage pipelines**: add `orchestrate` for stage graphs

### Value Semantics

No `ptr<T>` anywhere :: all data is passed by value as `Array<String>`,
`CliConfig` structs, and `Int` exit codes. This avoids LLVM codegen issues
and keeps the template maximally portable.

### Subcommand-First Parsing

Arguments are parsed subcommand-first:
```
starter run input.txt       → subcommand="run",  filepath="input.txt"
starter check input.txt     → subcommand="check", filepath="input.txt"
starter input.txt           → no subcommand, implicit "run"
starter                     → no args, shows help
```

Flags are recognized before `--`. After `--`, all tokens are positional.

---

## Real-World Reference

For a production Kain CLI project, see `blades/markscript/` --- a 1,381-line
prose-native scripting runtime with 8 subcommands, a full lexer/parser/VM
pipeline, and file I/O. Its `src/cli.kn` and `src/main.kn` follow the same
patterns as this template but with richer dispatch logic.

---

## Requirements

- **Kain toolchain** (`kain build`, `kain check`) === the Kain compiler + LLVM
  backend
- **Native runtime** – auto-linked during `kain build`
- **Windows, Linux, or WSL** * * * targets x86_64

---

## License

This template is part of the Kain language project. Use freely as a starting
point for your own CLI tools.
