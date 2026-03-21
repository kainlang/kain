# Kain Repository Map

```text
📂 Code/Kain
    ├── README.md                 # top-level operational brief
    ├── repomap.md                # top-level folder guide
    ├── MEMORY.md                 # curated memory for architectural lessons
    ├── CHANGELOG.md              # rolling human-visible change log
    ├── Cargo.toml                # workspace manifest
    ├── crates/                   # workspace crates and crate-level maps
    ├── docs/                     # doctrine, blueprints, and implementation plans
    │   └── kainplan/kain-fabric/ # active Fabric design/spec/task docs
    ├── generated/                # generated artifacts and large proof outputs
    ├── labs/                     # focused validation labs and smoke apps
    ├── runtime/                  # native runtime contracts, headers, and C runtime
    │   └── native/               # raw-native execution lane and viewport host
    ├── smoketest/                # proof matrix for bridges, UI, 3D, and mixed runtimes
    ├── stdlib/                   # standard library data and runtime support
    ├── toolchain/                # LLVM and related toolchain assets
    ├── third_party/              # vendored or external dependencies
    ├── unreal/                   # Unreal-facing support crates and asset tooling
    ├── guides/                   # longer-form repo guides and reference material
    ├── scripts/                  # workspace scripts and helpers
    ├── tools/                    # standalone utilities
    ├── apps/                     # application sources
    ├── bootstrap/                # bootstrap and selfhost support
    ├── python/                   # Python-side helpers and scripts
    └── testing/                  # test infrastructure and fixtures
```

Notes:

- `generated/` and `target/` are build outputs and should stay disposable.
- `docs/kainplan/` is where active design docs live before they become stable reference material.
- `runtime/native/` is the current raw-native C runtime lane, including the compute/viewer bridge.
- `crates/repomap.md` remains the crate-level detail map for workspace internals.
