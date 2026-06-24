# my-kain-app Template Changelog

Version history for the Kain + MarkScript project template.

## v0.1.1 (2026-06-13) ~ Working Build

### Summary

Fixed the template to work with the June 10, 2026 build of mks.exe. The key
insight: strings inside ```markscript blocks are hashed (not preserved as text),
so `run("kain check src/")` passes a numeric hash instead of the command string.
Blockquotes dispatch by exact hash match ->> only bare registered phrases like
`> run` and `> print` work (no args).

### Architecture

- **Blockquote intents** use EXACT registered phrases: `> run`, `> print`,
  `> file exists`, `> spawn`, `> read file`, `> write file`, `> import kain`,
  `> assert`
- **Markscript blocks** use `print()` for logging (prints hash values in this
  binary), and can do integer arithmetic, loops, and conditionals
- **Tables** store structured data embedded in bytecode
- **Kain app** at `src/main.kn` compiles cleanly with `kain check`
- **Handler dispatch** works correctly ~ `> run` dispatches to handler 4, etc.
  Handler errors about missing args are expected (non-fatal)

### Files

| File | LOC | Role | mks check | kain check |
|------|-----|------|-----------|------------|
| `Mksfile.md` | ~140 | Root orchestrator | PASS | N/A |
| `config.md` | ~65 | Project config as markscript tables | PASS | N/A |
| `schemas/project_schema.md` | ~70 | Column type/constraint schema | PASS | N/A |
| `scripts/build.md` | ~80 | Build pipeline (4 stages) | PASS | N/A |
| `scripts/dev.md` | ~75 | Dev loop | PASS | N/A |
| `scripts/test.md` | ~55 | Test runner | PASS | N/A |
| `scripts/clean.md` | ~50 | Artifact cleanup | PASS | N/A |
| `scripts/help.md` | ~140 | CLI reference + how-it-works | PASS | N/A |
| `src/main.kn` | ~80 | Kain application entry point | N/A | PASS |
| `docs/guide.md` | ~180 | Architecture guide | PASS | N/A |
| **Total** | **~935** | | **9/9 PASS** | **1/1 PASS** |

### Verified

- All 9 markdown files pass `mks check` with zero errors
- `mks run scripts/build.md` executes cleanly (27 dispatches, terminates safely)
- `mks run Mksfile.md` executes cleanly (45 dispatches, terminates safely)
- `main.kn` compiles with `kain check` (zero type errors)
- No "unknown intent phrase" errors during execution
- All IVT dispatches hit registered handlers (handlers 3, 4, 8)

### Known Limitations (June 10 Binary)

- String arguments in markscript blocks are hashed → integer hash values
- Blockquote phrases match by exact hash of full text → no argument passing
- Only 8 primary handlers registered (BETA/GAMMA/DELTA are stubs)
- Shell commands cannot be executed through markscript; run `kain ...` directly

### Usage

```bash
# Validate markscript files
mks check Mksfile.md
mks check scripts/build.md

# Run markscript pipelines
mks run Mksfile.md              # Full pipeline
mks run scripts/build.md        # Build orchestration
mks run scripts/help.md         # CLI reference

# Direct Kain commands
kain check src/                 # Typecheck
kain build src/ --target llvm   # Compile
kain run src/main.kn            # Execute
```
