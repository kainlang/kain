# my-kain-app — Kain + MarkScript Template

A Kain application built and orchestrated by MarkScript.
Running `mks run Mksfile.md` compiles this file and executes it.

**No Makefile. No build.kn. No KAIN.toml. The README IS the build.**

## Metadata

| Property | Value |
|----------|-------|
| Project | my-kain-app |
| Version | 0.1.0 |
| Language | Kain (orchestrated by MarkScript) |
| Entry | src/main.kn |
| Target | llvm |
| Profile | debug |

## Install

```markscript
print("========================================")
print("  my-kain-app — MarkScript Orchestrated")
print("========================================")
print("")
print("Prerequisites: Kain toolchain in PATH")
```

> run

```markscript
print("Prerequisite check dispatched")
```

## Build

```markscript
print("")
print("--- Stage 1: Typecheck ---")
```

> run

```markscript
print("Typecheck dispatched")
```

```markscript
print("")
print("--- Stage 2: Compile to native ---")
```

> run

```markscript
print("Build dispatched")
```

```markscript
print("")
print("--- Stage 3: Verify artifacts ---")
```

> file exists

```markscript
print("Verify dispatched")
```

```markscript
print("")
print("--- Stage 4: Run ---")
```

> run

```markscript
print("Run dispatched")
```

```markscript
print("")
print("--- Build pipeline complete ---")
```

## Config

```markscript
print("")
print("--- Config validation ---")
```

> file exists

```markscript
print("Config check dispatched")
```

## Test

```markscript
print("")
print("--- Running tests ---")
```

> run

```markscript
print("Tests dispatched")
```

## Scripts

| Script | Path | Description |
|--------|------|-------------|
| Build | scripts/build.md | Full build pipeline |
| Dev | scripts/dev.md | Interactive dev loop |
| Test | scripts/test.md | Test runner |
| Clean | scripts/clean.md | Clean artifacts |
| Help | scripts/help.md | CLI reference |

## Structure

| Path | Role | Type |
|------|------|------|
| Mksfile.md | Build orchestrator | MarkScript |
| config.md | Project configuration | MarkScript |
| scripts/build.md | Build pipeline | MarkScript |
| scripts/dev.md | Dev loop | MarkScript |
| scripts/test.md | Test runner | MarkScript |
| scripts/clean.md | Artifact cleanup | MarkScript |
| scripts/help.md | Help reference | MarkScript |
| src/main.kn | Application entry | Kain |
| tests/test_math.kn | Test fixtures | Kain |
| docs/guide.md | User guide | Markdown |
| schemas/project_schema.md | Config schema | MarkScript |

## Invariants

| # | Invariant |
|---|-----------|
| 1 | Every pipeline stage is a MarkScript heading/domain |
| 2 | Each `> intent` phrase matches a registered IVT entry exactly |
| 3 | No build.kn, KAIN.toml, or Makefile required |
| 4 | All configuration is expressed as markscript data tables |
| 5 | The README, the build script, and the documentation are one artifact |

## QuickRef

```markscript
print("QUICK REFERENCE:")
print("")
print("  mks run Mksfile.md            All-in-one pipeline")
print("  mks run scripts/build.md      Build")
print("  mks run scripts/dev.md        Dev loop")
print("  mks run scripts/test.md       Tests")
print("  mks run scripts/clean.md      Clean")
print("  mks run scripts/help.md       Help")
print("")
print("  kain check src/               Typecheck")
print("  kain build src/ --target llvm Compile")
print("  kain run src/main.kn          Run directly")
```
