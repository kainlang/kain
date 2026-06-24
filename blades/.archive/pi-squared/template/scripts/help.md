# Help 〰 MarkScript CLI Reference

All available commands for the my-kain-app project.
Run with: mks run scripts/help.md

## Banner

```markscript
print("=== my-kain-app <--> HELP ===")
print("MarkScript + Kain project template")
print("")
```

## Quickstart

```markscript
print("QUICK START:")
print("")
print("1. Typecheck:  kain check src/")
print("2. Build:      kain build src/ --target llvm")
print("3. Run:        kain run src/main.kn --target llvm")
print("")
print("Or use MarkScript:")
print("  mks run scripts/build.md")
print("")
```

## Commands

| Command | Description |
|---------|-------------|
| mks run Mksfile.md | Full all-in-one pipeline |
| mks run scripts/build.md | Build pipeline (check + compile + verify + run) |
| mks run scripts/dev.md | Interactive dev loop |
| mks run scripts/test.md | Test runner |
| mks run scripts/clean.md | Clean build artifacts |
| mks run scripts/help.md | This help display |
| mks check <file.md> | Validate markscript (compile only) |
| mks disasm <file.md> | Show bytecode disassembly |
| mks watch <file.md> | Live reload on file change |
| mks handlers | List registered IVT handlers |

## DirectKain

```markscript
print("DIRECT KAIN CLI:")
print("")
print("  kain check src/                  Typecheck")
print("  kain build src/ --target llvm    Compile to native .exe")
print("  kain run src/main.kn             Compile + run")
print("  kain test tests/ --json          Run tests")
print("  kain fmt src/ --check            Format validation")
print("  kain doctor                      Environment check")
```

## ProjectStructure

```markscript
print("PROJECT STRUCTURE:")
print("")
print("  Mksfile.md          Root orchestrator")
print("  config.md           Project configuration")
print("  src/main.kn         Application entry point")
print("  scripts/build.md    Build pipeline")
print("  scripts/dev.md      Dev loop")
print("  scripts/test.md     Test runner")
print("  scripts/clean.md    Artifact cleanup")
print("  scripts/help.md     This file")
print("  tests/              Test fixtures")
print("  docs/guide.md       User guide")
print("  schemas/            Config schema definitions")
```

## HowItWorks

```markscript
print("HOW IT WORKS:")
print("")
print("MarkScript compiles markdown to bytecode at mks check/run time.")
print("Each ## heading is a routine. Each > blockquote is an intent.")
print("The IVT maps intent phrases to handler functions in the Kain stdlib:")
print("  run        -> FN_PROCESS_OUTPUT (shell commands)")
print("  print      -> FN_PRINTLN         (console output)")
print("  file exists -> FN_FS_EXISTS      (filesystem check)")
print("  read file  -> FN_FS_READ_TEXT    (file reading)")
print("  write file -> FN_FS_WRITE_TEXT   (file writing)")
print("  spawn      -> FN_PROCESS_SPAWN   (process spawn)")
print("  import kain -> FN_IMPORT_KAIN    (module import)")
print("  assert     -> FN_ASSERT          (value assertion)")
print("")
print("Tables are typed data matrices embedded in bytecode.")
print("Markscript blocks run mini-language code (let, while, if/else).")
print("")
print("The build pipeline, README, and documentation are one artifact.")
```
