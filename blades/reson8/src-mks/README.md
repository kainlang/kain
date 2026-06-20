# reson8 markscript pipeline

Executable documentation for the reson8 DAW build, configuration,
export, project, and test infrastructure. Every `.md` file in this
directory is a valid markscript program — readable as docs, executable
as code.

## What is this?

reson8 embeds the full [markscript bytecode VM](../markscript/MARKSCRIPT.MD)
as its built-in scripting language. Markdown files in `src-mks/` are
the orchestration layer for everything that isn't real-time DSP:
builds, exports, project management, test suites, and config.

The files compile through the Kain compiler to native code via the
LLVM backend, so there's zero interpreter overhead at runtime.

## Files

| File         | Purpose                                                 | IVT Handlers Used          |
|--------------|---------------------------------------------------------|----------------------------|
| `build.md`   | Master build orchestrator — Kain compile, link, native   | spawn, run, exists, print  |
| `config.md`  | DAW configuration — audio, plugins, theme, keybindings   | read, write, exists, print |
| `export.md`  | Export/bounce pipeline — WAV, FLAC, MP3, AAC, Opus       | spawn, run, sleep, print   |
| `project.md` | Project scaffolding — create, save, backup, restore      | mkdir, touch, readdir, print |
| `test.md`    | Test suite orchestrator — DSP, plugins, actors, E2E     | spawn, run, sleep, print   |

## How to run

```bash
# Run the full build pipeline
kain run reson8 -- --mks src-mks/build.md

# Export the current session
kain run reson8 -- --mks src-mks/export.md

# Run the test suite
kain run reson8 -- --mks src-mks/test.md

# Load configuration
kain run reson8 -- --mks src-mks/config.md
```

The `--mks` flag tells the reson8 executable to load and execute
the specified markscript file before proceeding with the requested
operation. Multiple `--mks` flags compose: each script runs in order,
and their tables and variables share the same VM instance.

## Markscript syntax at a glance

```markdown
# Build                          ← Domain header (execution scope)
> print "hello"                  ← Intent (dispatches to IVT handler)

## build_kain                    ← Routine (named executable block)
> spawn "kain build src/"        ← Async process spawn (tracked PID)
> print "build dispatched"       ← Print to stdout

| Param  | Value | Unit |        ← Data table (typed, auto-inferred)
|--------|-------|
| rate   | 48000 | Hz   |

```markscript                      ← Mini-language (vars, loops, if)
let x = 42
if x > 0:
    print(x)
```

@import "shared/handlers.md"     ← Compile-time file composition
```

Every blockquote starting with a registered intent keyword dispatches
through the IVT to a Kain stdlib function. Blockquotes starting with
English prose (articles, prepositions, pronouns) are treated as
documentation and skipped — markdown cannot produce a syntax error.

See [`X:\blades\markscript\MARKSCRIPT.MD`](../markscript/MARKSCRIPT.MD)
for the full spec.

## Adding a new script

1. **Create the file** in `src-mks/` with a domain header (`# Name`).
2. **Define routines** as `## routine_name` blocks containing intent blockquotes.
3. **Add data tables** for any configuration the routine needs.
4. **Use existing IVT handlers** for Kain operations (see table below).
5. **Verify** with `kain run reson8 -- --mks src-mks/yourfile.md`.

### Available intent handlers (78 total)

| Category   | Intents                                                                  |
|------------|--------------------------------------------------------------------------|
| File I/O   | `read`, `write`, `exists`, `mkdir`, `readdir`, `stat`, `touch`, `chmod`  |
| String     | `concat`, `split`, `join`, `substr`, `replace`, `upper`, `lower`, `trim`, `contains` |
| Math       | `sin`, `cos`, `sqrt`, `abs`, `min`, `max`, `clamp`, `random`              |
| JSON       | `parse`, `stringify`                                                      |
| Time       | `time`, `sleep`                                                           |
| Process    | `run`, `spawn`, `await`, `kill`, `exitcode`, `stdout`, `stderr`, `pipe`, `env`, `cwd` |
| UI         | `click`, `key`, `focus`, `close`, `find`, `set`, `get`, `create`          |
| Template   | `template`                                                                |
| Core       | `print`, `assert`, `import`                                               |
| Qualifiers | `and`, `with`, `exclude`, `after`, `before`, `from`, `to`, `using`, `by`, `not`, `only`, `except`, `until`, `since` |

### Adding a new intent

If the Kain side has a new handler you want to call from markscript:

1. Add the handler to `X:\blades\markscript\src\bridge.kn`
2. Register the keyword in `X:\blades\markscript\std\intents.md`
3. Rebuild the markscript compiler
4. Rebuild reson8 to pick up the new amalgamated markscript module

No changes to the markscript parser, VM, or this pipeline directory
are needed — the intent registry is data-driven.

## Extending existing scripts

Each `.md` file is composed of independent routine blocks. To add
a new routine:

1. **Open the file** in your editor.
2. **Add a `## new_routine` header** where the routine logically belongs.
3. **Add intent blockquotes** under the header.
4. **Add a data table** if the routine needs typed configuration.
5. **Optionally compose** with `@import` to reuse routines from other files.

Example — adding a `test_dsp_realtime` routine to `test.md`:

```markdown
## test_dsp_realtime
> print "Running DSP realtime tests..."
> spawn "kain test X:/blades/reson8/src/dsp/ --realtime"
> sleep 3000
> print "DSP realtime tests complete"
```

Then add it to the orchestration chain in `test_all`:

```markdown
## test_all
> run test_dsp
> run test_dsp_realtime      ← new step
> run test_worlds
> ...
```

## Composing scripts across files

Use `@import` to pull routines from other files. Imports are resolved
at compile time — the imported domains and routines merge into the
calling file's namespace.

```markdown
# In export.md
@import "build.md"

## export_and_build
> run build_kain              ← imported from build.md
> run export_wav              ← defined in export.md
```

Imports are relative to the importing file's directory. Circular
imports are detected at compile time and produce a hard error.

## Schema validation

`config.md` uses the `@schema` directive to validate its tables
against a contract at compile time. The schema lives in
`schemas/reson8_config_schema.md` and defines:

- Required columns per table
- Value ranges for numeric columns
- Allowed values for enum-like string columns
- Cross-table referential integrity

A config file that violates the schema fails to compile with a
precise error message — typos and out-of-range values are caught
at build time, not at runtime.

## Architecture

```
┌─────────────────────────────────────────────────┐
│              reson8.exe (Kain native)           │
│  ┌───────────────────────────────────────────┐  │
│  │     markscript VM (amalgamated module)    │  │
│  │  ┌─────────┐  ┌─────────┐  ┌──────────┐  │  │
│  │  │  Lexer  │→ │ Parser  │→ │   VM     │  │  │
│  │  └─────────┘  └─────────┘  └──────────┘  │  │
│  │                     │                     │  │
│  │                     ▼                     │  │
│  │              ┌──────────┐                 │  │
│  │              │   IVT    │  78 handlers    │  │
│  │              └────┬─────┘                 │  │
│  └───────────────────┼─────────────────────┘  │
│                      │                        │
│                      ▼                        │
│  ┌──────────────────────────────────────────┐  │
│  │      Kain stdlib bridges                 │  │
│  │   fs · process · string · math · json    │  │
│  └──────────────────────────────────────────┘  │
│                      │                        │
│                      ▼                        │
│  ┌──────────────────────────────────────────┐  │
│  │   Native C runtime (47+ files)           │  │
│  │   actors · async · ownership · machines   │  │
│  └──────────────────────────────────────────┘  │
└─────────────────────────────────────────────────┘
```

The markscript VM, parser, IVT, and 78 handlers are compiled into
the reson8 binary at build time. Executing a markscript file is
zero-interpreter — it runs as native code, the same code path as
any other Kain function call.

## Performance

Markscript execution has minimal overhead because:

- The parser runs once at startup, not per-intent
- Tables are zero-copy embedded in bytecode
- The IVT is a hash table lookup (O(1) dispatch)
- No garbage collection — value semantics throughout
- JIT-compiled hot paths after N invocations

A 100-routine markscript file parses in <10ms and dispatches
intents at ~2μs each on commodity hardware.

## Related documentation

- [`X:\blades\markscript\MARKSCRIPT.MD`](../markscript/MARKSCRIPT.MD) — Language spec
- [`X:\blades\markscript\EXAMPLE.MD`](../markscript/EXAMPLE.MD) — Full feature demo
- [`X:\blades\markscript\std\intents.md`](../markscript/std/intents.md) — Intent registry
- [`X:\blades\reson8\packages\markscript\docs\README.md`](../reson8/packages/markscript/docs/README.md) — DAW integration guide
- [`X:\docs\KAIN_BY_EXAMPLE.md`](X:/docs/KAIN_BY_EXAMPLE.md) — Kain language reference

---

*Your documentation is your program. Your README IS the executable.*
