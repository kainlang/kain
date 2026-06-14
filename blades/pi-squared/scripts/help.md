# Help — Command Reference

MarkScript IVT command index for the pi-squared project.

## Banner

```markscript
print("=== PI-SQUARED HELP ===")
print("MarkScript Interactive Verification Tasks")
print("Usage: markscript run <script.md>")
```

## Commands

| Script | Path | Description |
|--------|------|-------------|
| build | scripts/build.md | Full compilation pipeline: typecheck, build, verify |
| dev | scripts/dev.md | Interactive dev loop with watch and auto-rebuild |
| test | scripts/test.md | Full test suite: unit, integration, e2e, Z3 proofs |
| clean | scripts/clean.md | Clean cache, build output, and compiled binaries |
| help | scripts/help.md | This reference |

## KainCommands

| Command | Purpose |
|---------|---------|
| `kain check` | Typecheck without codegen |
| `kain build --target llvm` | Compile to native via LLVM |
| `kain build --target llvm --debug` | Debug build with DWARF metadata |
| `kain run` | Compile + link + execute |
| `kain test` | Run compiletest-style test fixtures |
| `kain dev` | Watch + auto-rebuild loop |

## Architecture

- **Project root:** `X:/blades/pi-squared/`
- **Scripts:** `scripts/*.md` — MarkScript IVT orchestration
- **Build target:** LLVM native `.exe`

## QuickRef

```markscript
print("mark run build.md   -- run build pipeline")
print("mark run dev.md     -- start dev loop")
print("mark run test.md    -- run test suite")
print("mark run clean.md   -- clean artifacts")
print("mark run help.md    -- this reference")
```
