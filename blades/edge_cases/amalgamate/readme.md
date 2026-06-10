# Amalgamate — Portable Capsule Import Testing

Tests Kain's **amalgamate capsule pipeline**: pack a multi-module project into a single portable `.kn` capsule file, then import from it naturally in another project with `use module`.

## What's Proven

| Property | Status |
|----------|--------|
| ✅ **Multi-module capsule** packs 6 modules into one file | `kain amalgamate src/ -o capsule.kn` |
| ✅ **Capsule preserves all symbols** — functions, types, structs | 21 exported functions, 2 structs |
| ✅ **Import resolves directly from capsule** — no unpack needed | `use cause` / `use diagnostics` |
| ✅ **Calls against capsule imports work at runtime** | `run_cause_test_by_tag(...)` |
| ✅ **Chained imports from capsule** — cause imports effect & spookymagic | All 7 tests pass |
| ✅ **build.kn capsule_set auto-emission** | `capsule_set("amalgamate").after(cert)` |
| ✅ **Portable across repos** — drop capsule + build.kn, `kain run` | Works from any location |

## File Structure

```
amalgamate/
├── build.kn           ← Build authority with capsule_set auto-emission
├── readme.md          ← This file
├── spawn.kn           ← Self-replicating template cloner
├── debug-template.exe ← Pre-built portable binary (from capsule import)
└── src/
    ├── main.kn         ← CLI entry with diagnostics
    ├── diagnostics.kn   ← Orchestrator — imports all modules, runs tests
    ├── cause.kn         ← Root cause tests
    ├── effect.kn        ← Downstream effect modeling
    ├── spookymagic.kn   ← Black-box / spooky-magic behaviors
    └── vm.kn            ← Isolated process wrapper

testimport/
├── build.kn                ← Standalone test project
└── src/
    ├── main.kn                 ← Imports from the capsule below
    └── debug_amalgamation.kn   ← 🔥 AMALGAMATED CAPSULE (6 modules in 1 file)
```

## The Cannonical Amalgamation Flow

### Step 1: Pack a capsule from the CLI

```powershell
cd X:\blades\edge_cases\amalgamate
kain amalgamate src\ -o testimport\src\debug_amalgamation.kn `
  --name amalgamate `
  --tag portable --tag amalgamation-test `
  --contents source
```

### Step 2: Import it in another project

```kain
// testimport/src/main.kn
use std::io
use cause          // ✅ resolves from debug_amalgamation.kn
use diagnostics    // ✅ resolves from debug_amalgamation.kn

fn main() -> Int:
    // Call functions from the capsule directly
    let r1 = run_cause_test_by_tag("cause_sanity")
    let dr = run_diagnostics("all", true)
    return dr
```

### Step 3: Build & run

```powershell
cd X:\blades\edge_cases\amalgamate\testimport
kain run          # → exit=0, all 7 tests pass
```

### Step 4: Auto-emit via build.kn

```kain
// In build.kn:
let capsule = capsule_set("amalgamate")
    .after(cert)
    .source("$root/src")    // the module root to pack
    .tag("portable")
    .telemetry("amalgamate.capsule")
```

When `kain run` hits the `certify` task, the capsule auto-emits alongside the binary.

## Key Insight: Capsules Are First-Class Import Targets

The Kain module resolution system reads capsule `.kn` files natively — they look like
normal source files to the typechecker. You don't need to unpack before importing.
Just drop the amalgamated capsule into any project's `module_root` and `use` your modules:
