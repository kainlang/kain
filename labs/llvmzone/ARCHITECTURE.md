# LLVM Zone Architecture

`labs/llvmzone/` is a compact LLVM validation lane for Kain.
Its job is not to be one giant dogfood app. Its job is to prove that multiple
independent authored Kain utilities can each compile to a native executable
from the current LLVM pipeline.

## Layout

- `apps/` contains five isolated utility apps.
- `generated/` receives emitted `.ll` files, linked executables, and any sidecars.
- `build.ps1` drives the full lane and compiles every app in sequence.

## App Coverage

- `signal_sieve`
  Exercises enum declarations, enum matching, array indexing, `while` loops,
  bitwise operators, and string length handling.
- `budget_balancer`
  Exercises `world`, `patch`, `converge`, `orchestrate`, state mutation, and a
  minimal native UI surface.
- `actor_mailroom`
  Exercises actor declaration, `spawn`, and named `send` message payloads.
- `edge_math_meter`
  Exercises float arithmetic, float comparisons, bitwise integer math, and
  looped aggregation over arrays.
- `scene_console`
  Exercises native UI rendering plus a viewport3d surface in a compact tool.

## Build Flow

`build.ps1` resolves the Kain CLI, then runs:

`kain build <app>/src/main.kn --target llvm --output generated/<app>.ll`

On Windows, the compiler then links a sibling `.exe` next to the `.ll` file.

## Common Errors

- Do not use `println` in these LLVM apps unless the backend has gained exact
  print lowering. The current LLVM lane still rejects that semantics path.
- Run the build from the lab root so the generated output paths stay local to
  `labs/llvmzone/generated/`.
- If `target/debug/kain.exe` is missing, the build harness falls back to a
  repo-local CLI build.
- LLVM linking still depends on `clang` being available on the machine.
