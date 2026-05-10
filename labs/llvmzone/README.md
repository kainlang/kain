# LLVM Zone

`labs/llvmzone/` is a five-app LLVM smoke lane for Kain.
Each app is an isolated utility source tree that compiles to its own executable.

Layout:

- `apps/`
  Separate Kain apps, each with its own `src/main.kn`.
- `generated/`
  LLVM IR, native executables, and any emitted sidecars.
- `build.ps1`
  Compiles every app in the lane to an executable.

Apps:

- `signal_sieve`
  Enum matching, arrays, bitwise math, and looped aggregation.
- `budget_balancer`
  `world`, `patch`, `converge`, `orchestrate`, plus a minimal UI surface.
- `actor_mailroom`
  Actor spawn/send mailbox lowering.
- `edge_math_meter`
  Float arithmetic, comparisons, bitwise operators, and array scans.
- `scene_console`
  Native UI and viewport wiring with a compact telemetry surface.

Run:

```powershell
.\build.ps1
```

The lane intentionally avoids `println` in the LLVM-only paths because the
current backend still rejects runtime print semantics.
