# Kain Repository Map

```text
📂 Code/Kain
    ├── README.md                 # top-level operational brief
    ├── repomap.md                # top-level folder guide
    ├── MEMORY.md                 # curated memory for architectural lessons
    ├── CHANGELOG.md              # rolling human-visible change log
    ├── Cargo.toml                # workspace manifest
    ├── crates/                   # workspace crates and crate-level maps
    │   ├── README.md             # human-friendly crate navigation index
    │   ├── repomap.md            # crate-level detail map
    │   └── kain-gpu-runtime/     # runtime-facing Vulkan compute executor for GPU payloads
    ├── docs/                     # doctrine, blueprints, and implementation plans
    │   ├── archive/              # archived docs and older references
    │   ├── automation/           # automation and agent handoff notes
    │   ├── crates/               # crate-level guidance and audit history
    │   ├── guides/               # longer-form repo guides and references
    │   ├── kainplan/             # active design/spec/task docs
    │   │   └── kain-fabric/      # active Fabric design/spec/task docs
    │   ├── kainvsgiants/         # strategic moat and positioning notes
    │   ├── pipeline/             # pipeline docs and operational notes
    │   ├── recent/               # fresh validation logs and recent notes
    │   ├── stdlib/               # stdlib docs and references
    │   └── validation/           # validation logs and reports
    ├── generated/                # generated artifacts and large proof outputs
    ├── labs/                     # focused validation labs and smoke apps
    ├── runtime/                  # native runtime contracts, headers, C runtime, and parallel companion lane
    │   ├── native/               # raw-native execution lane and viewport host
    │   └── parallel/             # Rust/Zig companion lane for runtime completion work
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
- `runtime/parallel/` is the companion Rust/Zig lane for runtime planning and reports.
- `crates/README.md` is the human-friendly index for crate navigation.
- `crates/kain-gpu-runtime/` is the runtime-facing Vulkan compute executor for KAIN GPU payloads.
- `crates/repomap.md` remains the crate-level detail map for workspace internals.
- `docs/kainvsgiants/` is a focused strategy note folder with a single working paper.
