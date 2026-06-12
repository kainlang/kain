# KainSelfHost — Build Pipeline

View blades/markscript/readme.md for more information on markscript and how it works

> This file IS the build system for the self-hosted Kain compiler.
> Currently Phase 0 (CLI shell). Expands as modules land.
> See: research/SELFHOST-KN.MD for the full phase plan.

## Metadata
| Property | Value |
|----------|-------|
| Project | kainc |
| Phase | 0 — CLI Shell |
| Entry | src/main.kn |
| Bootstrap | crates/cli/kain.exe (Rust) |
| Target | llvm |

## PhaseMap
| Phase | Status | Description |
|-------|--------|-------------|
| 0 — CLI Shell | active | --help, --version, subcommand stubs |
| 1 — DLL Bridge | planned | Rust DLL FFI, OrcJIT |
| 2 — Parser | planned | Lexer + parser in Kain |
| 3 — Typechecker | planned | 4-pass type system |
| 4 — Codegen | planned | LLVM-C FFI emission |
| 5 — Ouroboros | planned | Self-compiles itself |

---

## Build

Build the current phase with the bootstrap compiler.

> run "kain build src/ --target llvm"

---

## Check

Typecheck all sources.

> run "kain check src/"

---

## Smoke

Verify kainc responds to basic commands.

> run "kain build src/ --target llvm"

> file exists ".kain/out/kainc.exe"
> assert 1 "binary must exist after build"

> run ".kain/out/kainc.exe --help"
> assert 0 "--help must exit clean"

> run ".kain/out/kainc.exe --version"
> assert 0 "--version must exit clean"

---

## Clean

> run "rm -rf .kain/out"
