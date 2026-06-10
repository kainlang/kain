# Amalgamate - The Katamari Protocol

Kain's **amalgamate capsule pipeline** solves the **permanent problem of code distribution**. Pack any number of modules -> 6 or 2,594 -> into a single portable `.kn` capsule file. Drop it into any project. `use` whatever you need. The compiler resolves everything directly from the capsule. No unpack. No install. No network. No lockfile. No version solver.

**One file. All the code. Forever.**

## What's Proven

| Property | Status | Scale |
|----------|--------|-------|
|  **Mass-scale capsule** packs 2,594 modules into one file | `kain amalgamate ml/raw/-o raw.kn` (first run the lasso.kn script to gather all repo files || kain run ml/lasso.kn --target llvm)| 300k+ lines, 15+ MB, **3 seconds** |
|  **Multi-module import from capsule** -> 8 modules, 938 items typechecked | `use stdlib_math`, `stdlib_crypto`, `stdlib_zip`, ... | Zero errors, passes `kain check` |
|  **Functions resolve across capsule boundary** -> vec3, sha256, json, collections | `vec3_add(a,b)`, `sha256("text")`, `json_parse(...)` | All pub fns callable directly |
|  **Capsule preserves all symbols** -> functions, types, structs, traits, actors, worlds | 3,211+ public symbols in raw.kn | Full semantic surface intact |
|  **Import resolves directly from capsule** -> no unpack needed | `use module_name` | Module = filename minus `.kn` |
|  **Calls against capsule imports work at runtime** | `run_cause_test_by_tag(...)` | Verified in debug_amalgamation |
|  **Chained imports from capsule** -> cause imports effect & spookymagic | All 7 tests pass | Transitive deps preserved |
|  **build.kn capsule_set auto-emission** | `capsule_set("amalgamate").after(cert)` | Build-graph native |
|  **Portable across repos** -> drop capsule + build.kn, `kain run` | Works from any location | Zero env dependencies |


Amalgamate turns Kain code into a **durable digital asset**. Every `pub fn` ever written is one `kain amalgamate` away from being importable by every other Kain project. Forever.


Each phase amalgamates the previous. The file grows. The import surface grows. Nothing is ever lost. Every library you've ever written lives in the katamari.

├── build.kn                ← Standalone test project
└── src/
    ├── main.kn                 ←  Imports 8 modules from 2,594-file capsule
    ├── debug_amalgamation.kn   ← Small capsule (6 modules, 40 KB)
    └── raw.kn                  ←  MEGA CAPSULE (2,594 modules, 316K lines, 15 MB)
```

## The Cannonical Amalgamation Flow

### Step 1: Pack a capsule from the CLI

```powershell
cd X:\blades\edge_cases\amalgamate
kain amalgamate src\ -o katamari.kn 

### Step 2: Import it in another project

```kain
// testimport/src/main.kn
use std::io
use cause          //  resolves from debug_amalgamation.kn
use diagnostics    //  resolves from debug_amalgamation.kn

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

### Step 4: Auto-emit via build.kn (alternatively just use almagamate command from cli)


```kain
// In build.kn:
let capsule = capsule_set("amalgamate")
    .after(cert)
    .source("$root/src")    // the module root to pack
    .tag("portable")
    .telemetry("amalgamate.capsule")
```

When `kain run` hits the `certify` task, the capsule auto-emits alongside the binary.

## The Nuclear Test: 2,594 Modules, 15 MB, 8 Imports, Zero Errors

2026-06-10, amalgamated the entire `ml/` directory -> 2,594 files spanning the full Kain stdlib surface, every benchmark case, actor probes, GPU kernels, C bridges, Python interop, Z3 proofs, SQLite, ZIP/TAR/WASM parsers -> into one file (`raw.kn`):

```
316,135 lines. 15 MB. 3 seconds to amalgamate. 2,594 modules. 3,211+ public symbols.
```

Then imported 8 modules from it in a fresh project:

```kain
// /src/main.kn -> imports 8 modules from a 2,594-file capsule
use std::io
use stdlib_math         // vec3, quat, noise, ray casting
use stdlib_crypto       // sha256
use stdlib_hash         // hash_u32
use stdlib_ascii        // ascii_is_digit_byte
use stdlib_json         // json_parse, json_get_int
use stdlib_collections  // queue_create, queue_push, queue_pop
use stdlib_zip          // zip_write_local_header, zip_write_eocd

fn main() -> Int:
    let a = vec3(1.0, 2.0, 3.0)
    let b = vec3(4.0, 5.0, 6.0)
    let c = vec3_add(a, b)
    let hash = sha256("hello from raw.kn capsule")
    let json = json_parse("{\"key\": 42}")
    let q = queue_create(16)
    // ... all 8 modules, all functions resolve
    return 0
```

```powershell
kain check   # → PASSED. 938 items typechecked. 0 errors. 
```
(also confirmed LLVM and build passes too)

**This is the most important result in the amalgamate proof surface.** It demonstrates that capsule imports scale linearly -> 6 modules or 2,594, the compiler doesn't care. The module resolution is the same. The typechecking is the same. The import syntax is the same.

## Key Insight: Capsules Are First-Class Import Targets

The Kain module resolution system reads capsule `.kn` files natively -> they look like
normal source files to the typechecker. You don't need to unpack before importing.
Just drop the amalgamated capsule into any project's `module_root` and `use` your modules.

However if you want those 2600 files in raw.kn back to their normal state - just run kain almagamate unpack and it unpacks those 2600 files back into their origninal form and layout -- works with artifacts and any other file format too 

The capsule format supports three content modes:
- **source** -> embeddable code
- **artifacts** -> compiled objects, SPIR-V binaries, native runtimes (base64-encoded)
- **evidence** -> telemetry, benchmarks, proof results, attestation data

Source capsules are importable as modules. Artifact and evidence capsules are companion
sidecars that carry the compiled output without polluting the module namespace.
