# Task: driver.kn — Compilation Pipeline Driver

## Agent: kain-writer
## Wave: 1
## File to write: X:/blades/kain/src/driver.kn
## Target lines: ~300
## Dependencies: None
## Parallel: Yes — can run alongside all other Wave 1 files

---

## What to Build

The compilation driver that orchestrates the full compile pipeline. It takes a source file path and a compilation target, then dispatches through the compiler stages: lexer, parser, typechecker, codegen. Collects diagnostics from each stage and returns a structured result.

This is the central dispatch hub — every CLI subcommand (check, build, run, jit, ast) calls into this file.

## Public API Contract

```kain
use std::fs

/// What kind of compilation output to produce
pub enum CompileTarget:
    Check       // typecheck only, no codegen
    Llvm        // emit LLVM IR
    Native      // emit native executable (LLVM IR + clang link)
    Jit         // JIT compile and execute in-memory
    Ast         // dump parsed AST (debug)
    Tokens      // dump token stream (debug)

/// Result of a compilation
pub struct CompileResult:
    success: Bool
    target: CompileTarget
    output_path: String        // path to output file (for Llvm/Native)
    errors: Array<Diagnostic>
    warnings: Array<Diagnostic>
    token_count: Int
    ast_node_count: Int
    type_count: Int
    elapsed_ms: Int

/// Dispatch: given a file path and target, run the full pipeline
pub fn compile_file(path: String, target: CompileTarget) -> CompileResult with IO

/// Typecheck only — returns diagnostics without codegen
pub fn check_file(path: String) -> CompileResult with IO

/// Compile to LLVM IR file on disk
pub fn build_file(path: String, output: String, profile: String) -> CompileResult with IO

/// JIT compile and run — returns the exit code
pub fn jit_file(path: String, args: Array<String>) -> Int with IO, Unsafe

/// Dump the AST as formatted text
pub fn dump_ast(path: String) -> String with IO
```

## Internal Structure

The driver dispatches through these stages in order:

1. **Load source** — read the .kn file from disk via `std::fs`
2. **Lex** — call lexer.tokenize(source) → Array of Token
3. **Parse** — call parser.parse(tokens, filename) → Program (top-level items)
4. **Typecheck** — call types.check(program) → TypedProgram
5. **Codegen** — (only for Llvm/Native/Jit targets) call codegen.emit(typed_program, target) → LLVM module or .ll text
6. **Link** — (only for Native target) shell out to clang to link .ll + runtime.lib → .exe
7. **JIT Execute** — (only for Jit target) call jit.jit_run(module, args)

At each stage, collect diagnostics into the CompileResult.errors array. If any stage produces errors with severity >= Error, stop the pipeline (unless the target is Check, which always runs all stages for maximum diagnostics).

## Research to Read

- X:/blades/kain/research/04-cli-driver-selfhost.md — CLI/driver architecture sections
- X:/blades/kain/research/SELFHOST-KN.MD — Section 5 (Data Flow Diagram) and Section 6 (Compiler Pipeline Stages)
- X:/blades/kain/research/03-llvm-codegen-jit.md — Section on compilation pipeline stages

## Reference Files to Study

- X:/blades/kain/src/main.kn — existing main.kn for CLI pattern, how subcommands dispatch
- X:/blades/kain/src/cli.kn — existing cli.kn for CliConfig struct pattern
- X:/blades/markscript/src/main.kn — markscript's main.kn for pipeline dispatch pattern
- X:/crates/driver/src/lib.rs — the Rust DriverSession for reference on what a driver does

## Neighboring Files (Wave 1)

These files run in parallel — coordinate on shared types:

| File | Exports | What driver.kn imports from it |
|------|---------|-------------------------------|
| platform.kn | `get_target_triple()`, `is_windows()` | Used for Native target triple |
| context.kn | `create_context()`, `create_module()`, `create_builder()` | Used for codegen setup |
| jit.kn | `jit_run(mod, entry, arg)` | Used for Jit target |
| target.kn | `init_native_target()` | Called before codegen |
| runtime.kn | `runtime_init()`, `runtime_shutdown()` | Called at startup/shutdown |

## Test Expectations

- `kain check src/driver.kn` passes
- `compile_file("test/fixtures/hello.kn", CompileTarget::Check)` exits without panic
- `dump_ast("test/fixtures/hello.kn")` returns non-empty string
- Error case: file not found triggers a Diagnostic in errors array, not a panic

## Code Patterns to Follow

- Use the `use std::fs` for file reading (NOT include C headers for this)
- Match on CompileTarget enum for dispatch
- Use early returns with `return CompileResult { ... }` on error
- Progress: print stage name if verbose flag is set
- All errors go into the errors array, never panic
