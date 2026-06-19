# my-kain-app - User Guide

> A complete guide to building, running, and extending this MarkScript-orchestrated
> Kain project. This document IS executable prose |-> every heading can be a
> script domain, every table is typed data, and every blockquote is an intent.

## Quick Start

### Prerequisites

- Kain compiler toolchain in PATH (run `kain doctor` to verify)
- MarkScript runtime: `X:\blades\markscript\mks.exe`
- Windows x86_64 (primary) or Linux x86_64

### First Build

```bash
# Typecheck the Kain source
kain check src/

# Compile to native executable
kain build src/ --target llvm

# Run the application directly
kain run src/main.kn --target llvm
```

### Using MarkScript

```bash
# Full pipeline (check + build + verify + run)
mks run Mksfile.md

# Individual stages
mks run scripts/build.md      # Build pipeline
mks run scripts/dev.md        # Interactive dev loop
mks run scripts/test.md       # Run tests
mks run scripts/clean.md      # Clean artifacts
mks run scripts/help.md       # Help & reference

# Validate markscript files
mks check Mksfile.md
mks check config.md
mks check scripts/build.md

# Inspect bytecode
mks disasm scripts/build.md

# Watch for changes
mks watch scripts/dev.md
```

## Architecture

### MarkScript Orchestration

This project uses MarkScript as the sole build orchestrator. There is no
Makefile, no build.kn, and no KAIN.toml. Every build action, test run,
and configuration check is encoded as markdown:

- **`# Heading`** = Domain (entry point / namespace)
- **`## Subheading`** = Routine (a named block of operations)
- **`> intent`** = IVT dispatch (maps to handler functions)
- **`| Table |`** = Typed data matrix (embedded in bytecode)
- **````markscript`** = Mini-language (let, while, if/else, arithmetic)

### Intent Phrase Registry (IVT)

The MarkScript VM registers 8 intent phrases in the Intent Vector Table:

| Phrase | Handler | Function |
|--------|---------|----------|
| run | FN_PROCESS_OUTPUT (4) | Execute shell commands |
| print | FN_PRINTLN (8) | Console output |
| file exists | FN_FS_EXISTS (3) | Filesystem existence check |
| read file | FN_FS_READ_TEXT (1) | File reading |
| write file | FN_FS_WRITE_TEXT (2) | File writing |
| spawn | FN_PROCESS_SPAWN (5) | Process spawning |
| import kain | FN_IMPORT_KAIN (6) | Module import |
| assert | FN_ASSERT (7) | Value assertion |

### Kain Application

The Kain application at `src/main.kn` is a standard Kain program with:

- **Types**: `AppInfo` struct
- **Pure computation**: `compute_checksum()` with no side effects
- **Effect annotations**: `fn ... with Pure` for pure functions
- **String conversion**: `text_to_string()` from std::text

## Files

| File | Purpose |
|------|---------|
| `Mksfile.md` | Root orchestrator --> the full pipeline |
| `config.md` | Project configuration as typed tables |
| `schemas/project_schema.md` | Column type definitions |
| `scripts/build.md` | Build pipeline (5 stages) |
| `scripts/dev.md` | Dev loop with quick reference |
| `scripts/test.md` | Test runner |
| `scripts/clean.md` | Artifact cleanup |
| `scripts/help.md` | Complete CLI reference |
| `src/main.kn` | Kain application entry point |
| `tests/test_math.kn` | Math utility tests |
| `docs/guide.md` | This user guide |

## Workflows

### Daily Development

```bash
# Start dev loop (typecheck + build + run)
mks run scripts/dev.md

# Or with live reload
mks watch scripts/dev.md
```

### Before Commit

```bash
kain check src/
kain test tests/ --json
mks check Mksfile.md
mks check config.md
mks check scripts/build.md
mks check scripts/dev.md
mks check scripts/test.md
mks check scripts/clean.md
mks check scripts/help.md
```

### Release Build

```bash
kain build src/ --target llvm
# Binary at .kain/out/<target-triple>/debug/llvm/
```

## Extending

### Adding a new script

1. Create `scripts/my-script.md` with headings for stages
2. Use `> intent` blockquotes for dispatch
3. Use ````markscript` blocks for logging/flow
4. Add a table for metadata
5. Run `mks check scripts/my-script.md` to validate
6. Add entry to the Scripts table in config.md

### Adding a new Kain module

1. Create `src/my_module.kn`
2. Import in `src/main.kn` with `use my_module`
3. Typecheck: `kain check src/`

### Adding tests

1. Create `tests/test_my_feature.kn`
2. Use `test fn` with `assert()` calls
3. Run: `kain test tests/ --json`

## Limitations

### Current Binary (June 10, 2026)

- String arguments in markscript blocks are hashed, not preserved as text
- Blockquote intents match by exact hash of the full phrase (no argument passing)
- Only the 8 primary handlers are registered (BETA/GAMMA/DELTA handlers are
  stubs in the Kain source but not compiled into this binary)
- `> run` dispatches correctly but cannot receive command arguments =>
  shell commands must be run directly via `kain ...` CLI

### Future

- String arguments will be passed as actual strings (not hashes)
- Process lifecycle handlers (spawn tracked, await, kill, exitcode) will
  enable full build orchestration with output capture
- UI scripting handlers will enable graphical MarkScript-driven apps

## See Also

- MarkScript source: `X:\blades\markscript\src\`
- MarkScript examples: `X:\blades\markscript\examples\`
- Kain docs: `X:\docs\KAIN_BY_EXAMPLE.md`
- Kain stdlib: `X:\stdlib\`
