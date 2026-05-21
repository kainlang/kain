---
name: kain-blades-system
description: Use when changing Kain's blade workspace system, including crates/kain-blades, crates/kain-build, KAIN.toml blade/build metadata, kain blades/equip/build CLI commands, the standalone blade CLI, blade-aware module resolution, Fabric blade steps, Rust crate FFI blade lookup, C ABI FFI blade lookup, or Blade workspace smoke tests.
---

# Kain Blades System

## Overview

Blades are Kain's crate-like local workspace units. The Rust package/import crate is named `blade` so callers write `use blade::...`; the source folder remains `crates/kain-blades`, and workspace folders remain plural (`blades/*`). The `blade` crate owns discovery and resolution so `blades/*`, `apps/*`, and `crates/*` can participate in one graph without each caller inventing path rules. The `kain-build` crate owns the Blade workspace build DAG and artifact/cache/report layout.

Blade, Fabric, FFI, and check/test file operations now route through `kain-fs`. Keep build artifacts, cache stamps, JSON/JSONL reports, manifest reads, C sidecar copies, GPU artifact writes, input hashing, and deterministic directory walks on `kain_fs` APIs rather than raw `std::fs`.

The repo now has an essential Kain-library blade layer under `blades/`: `kain-fmt`, `kain-log`, `kain-fsx`, `kain-config`, `kain-process-kit`, `kain-http`, `kain-actor-kit`, `kain-interop-kit`, plus the upgraded `kain-json`. Treat these as the first dependency targets for new runnable blades instead of rebuilding the same helpers locally.

## Start Here

- Read `ARCHITECTURE.md` and `MEMORY.md` first; the build entry dated `2026-05-13` captures the current lane-aware artifact design and known risks.
- Inspect `crates/kain-blades/src/lib.rs` before changing any caller. The resolver crate is imported as `blade` and should remain the single source for default patterns, manifest parsing, blade identity, diagnostics, and graph edges.
- Inspect `crates/kain-build/src/workspace.rs` before changing build behavior. It should remain the single source for task planning, caching, artifact roots, clean safety, and build reports.
- Treat `kain equip <blade>` as the human-facing resolver proof. Treat `kain blades list`, `kain blades graph`, and `kain blades check` as inspection/validation, not as separate discovery implementations.
- Treat `kain blades build .` and `blade build .` as the build proof. Do not add lab-local build scripts for artifacts the build graph can produce.
- Remember that blade module resolution now merges ancestor workspace module roots. A blade launched from `blades/<name>/src` should still be able to import sibling blades declared at the repo root; if it cannot, investigate `blade::discover_blade_module_roots_from` before adding ad hoc path workarounds.

## Core Files

- `crates/kain-blades/src/lib.rs`: typed workspace discovery, `KAIN.toml` parsing, synthetic Cargo blades, C FFI/Rust crate lookup, module roots, diagnostics, and graph edges.
- `crates/kain-build/src/workspace.rs`: typed Blade plus Kain file/project/native-ui build graph, C/Cargo/GPU/Kain/Fabric/Node/Bun task adapters, lane-aware `.kain/out` artifact schema, Cargo JSON artifact harvesting, stamp cache, safe clean, artifact manifests, and JSON/JSONL reports.
- `crates/cli/src/blades.rs`, `crates/cli/src/bin/blade.rs`, and `crates/cli/src/main.rs`: `kain blades ...`, `kain equip ...`, and standalone `blade ...` command surfaces.
- `crates/kain-core/src/module_resolution.rs`: consumes blade module roots for filesystem imports.
- `crates/kain-host/src/fabric.rs` and `crates/kain-omni/src/fabric.rs`: Fabric `blade = "..."` schema and runtime adapter resolution.
- `crates/kain-node/src/lib.rs`: Node bridge process launch. Keep Windows verbatim path normalization here when Fabric/Blade paths reach Node.
- `crates/kain-crate-ffi/src/resolve.rs`: Rust crate imports can fall back to equipped blades.
- `crates/kain-crate-ffi/src/{lib.rs,generate.rs,extract.rs,resolve.rs}`: Rust crate imports can fall back to equipped blades and generate cache artifacts through `kain-fs`.
- `crates/kain-c-ffi/src/{lib.rs,generate.rs,extract.rs}`: C ABI library imports can fall back to equipped blades and generate/load cache artifacts through `kain-fs`.
- `crates/kain-import/src/**` and `crates/kain-codebase/src/lib.rs`: adjacent import/workspace helpers that should stay aligned with `kain-fs` when Blade work touches source discovery, hashes, or generated artifacts.
- `blades/kain-mcp/**` and `scripts/python/launch_kain_mcp.py`: canonical Kain-authored MCP blade lane. Keep tool schemas in `config/tools.json`, runtime/env/binary policy in `config/runtime_policy.json`, and request handling in `src/*.kn` rather than hardcoding repo paths in launcher or docs. Root `mcp.json` and `codex.config.toml` now boot this lane directly through `kain run ...`; the Python launcher is the managed-sync fallback/debug path, not the default Codex route.
- `scripts/windows/sync-kain-source-of-truth.ps1` plus `C:\Users\Admin\.kain\state\kain_sync_stamp.json` and `build_counter.json`: managed multi-agent sync lane for the canonical PATH `kain.exe` / `kn.exe`. The launcher now stale-checks repo SHA + runtime stamp + binary stamp and may sync before boot. This script is invoked through Windows `powershell`, so keep its JSON parsing compatible with PowerShell 5 instead of depending on `ConvertFrom-Json -AsHashtable`.
- `labs/blades_workspace_smoke/**`: full workspace smoke covering root workspace discovery, app/Kain/C ABI/Rust/GPU/synthetic Cargo blades, Fabric `blade = "..."`, `kain equip`, `blade build`, CPU Fabric execution, cache hits, GPU artifact generation, and the Cargo-built `blade_singularity_atlas` executable proof.

## Manifest Rules

- A Kain blade is explicit when its `KAIN.toml` has `[blade]` metadata.
- A Rust crate is still a blade when a `Cargo.toml` is discovered under a blade pattern, even without `KAIN.toml`.
- Default discovery patterns are `blades/*`, `apps/*`, and `crates/*`; workspace `KAIN.toml` can add `blades`, `blade_roots`, or `members`.
- Keep new locator data in manifest sections instead of hardcoding paths in callers. Existing sections include Rust, C FFI, Fabric, GPU, entry path, source roots, module roots, dependencies, tags, and capabilities.
- Paths are resolved relative to the blade root unless explicitly absolute. Dynamic C library names can use `${kain_dynlib:name}`.
- `[build]` can declare `artifact_root`, `cache_root`, `profile`, and `[[build.tasks]]`; CLI build entrypoints can additionally select `--lane bootstrap|dev|release|dist|selfhost`.
- `[[build.tasks]]` supports `id`, `kind`, `blade`, `entry`, `manifest`, `command`, `args`, `cwd`, `target`, `profile`, `inputs`, `outputs`, and `depends_on`. Prefer this over hardcoded task behavior when adding user-authored build work.
- C FFI libraries that should be built by `kain-build` must declare `sources`; `header` alone is enough for import metadata but not for compiling a shared library.
- Synthetic Cargo blades may contain real binaries. When they are part of a smoke, build them through `blade build .`, locate the executable under `.kain/out`, and run/validate it from the smoke runner instead of adding a lab-local build script.

## Build Artifact Layout

- `.kain/out/<host>/<lane>/<target>/<unit>/<task>/...`: canonical build artifacts. C sidecars are copied from here into their declared `shared_lib` path when needed by existing FFI consumers; standalone Kain/native-ui/Rust outputs also write per-task `kain-artifacts.json` manifests here.
- `.kain/cache/build/stamps/*.stamp`: task fingerprints. Inputs include adapter settings, lane, profile, target, declared inputs, output paths, and content hashes. Generated/vendor folders such as `.kain`, `target`, `node_modules`, and `.git` are skipped.
- `.kain/reports/build/session-*.json` and `.jsonl`: build reports and event streams.
- `--clean` is intentionally safe and narrow. It should only remove workspace-local `.kain` build/cache/report roots.
- Managed PATH-install state for repo-local operator flows lives outside the workspace under `~/.kain/state`; do not treat those stamp/counter files as blade-owned artifacts.

## Change Pattern

1. Add or extend typed manifest data in the `blade` crate under `crates/kain-blades`.
2. Add unit tests in the `blade` crate for discovery and resolution behavior.
3. If the change affects build execution, add or extend task planning/adapters in `kain-build` rather than in labs or CLI code.
4. Wire callers to consume `ResolvedBlade`, `BladeBuildOptions`, or purpose-built resolver/build functions instead of parsing manifests again.
5. Add focused caller tests where behavior crosses a boundary, especially build graph, Fabric, FFI, module lookup, or Node/Python bridges.
6. Update `ARCHITECTURE.md` and `MEMORY.md` when semantics, CLI commands, build roots, or folder conventions change.

## Validation

Use a separate target dir on Windows to avoid locked default-target artifacts:

```powershell
cargo test -p blade --target-dir target\codex-blades
cargo test -p kain-build --target-dir target\codex-blades
cargo test -p kain-core blade_module_roots_extend_filesystem_candidates --target-dir target\codex-blades
cargo test -p kain-omni validate_default_polyglot_template_succeeds --target-dir target\codex-blades
cargo test -p kain-host python_harness_supports_mixed_multi_output_steps --target-dir target\codex-blades
$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\codex-blades
target\codex-blades\debug\kain.exe equip kain-core --json
target\codex-blades\debug\kain.exe blades list .
$env:KAIN_BIN=(Resolve-Path target\codex-blades\debug\kain.exe).Path
$env:BLADE_BIN=(Resolve-Path target\codex-blades\debug\blade.exe).Path
python labs\blades_workspace_smoke\scripts\run_blades_smoke.py
python labs\blades_workspace_smoke\scripts\run_blades_smoke.py --clean-cache
kain run plan blades\kain-mcp
kain blades build blades\kain-mcp --json
C:\Users\Admin\.cargo\bin\kain.exe run D:\Kain-Lang\blades\kain-mcp
py -3 scripts\python\launch_kain_mcp.py
powershell -NoProfile -ExecutionPolicy Bypass -File scripts\windows\sync-kain-source-of-truth.ps1 -ManagedSync
kain doctor
```

When touching FFI lookup, also run:

```powershell
cargo test -p kain-crate-ffi --target-dir target\codex-blades
cargo test -p kain-c-ffi --target-dir target\codex-blades
```

Use `blade build . --json --clean` from a workspace root when you need a direct clean build proof without the full lab assertions. Use `--include-vulkan` only on machines with a working Vulkan compute runtime.

For build-system invariant changes, also run the durable proof pack:

```powershell
mcp__z3_local__.run_proof_pack(path="D:\Kain-Lang\crates\kain-build", lane="build")
```

## Guardrails

- Do not add new ad hoc scans for `blades/*`, `apps/*`, or `crates/*` outside the `blade` crate.
- Do not make Fabric, C FFI, Rust FFI, or module resolution parse `KAIN.toml` directly for blade behavior.
- Do not add custom lab build scripts for C/Cargo/GPU/Fabric work that `kain-build` can own.
- Do not place build outputs in random `outputs/` folders unless they are runtime reports produced by a built executable or owned by a manifest. Build artifacts belong under `.kain/out`, stamps under `.kain/cache/build`, and build reports under `.kain/reports/build`.
- Do not reintroduce raw `std::fs` in Blade/build/Fabric/check/test/FFI/import hot paths. Use `kain_fs::read_text`, `write_text`, `atomic_write_text`, `atomic_write_bytes`, `append_text`, `copy_file` / `copy_path`, `remove_*`, `read_dir_entries`, `hash_file`, and `canonicalize_path`.
- For GPU smoke shader additions, keep Kain shader math inside the currently proven compiler surface. Sample-based Float math is known-good in the Blade lab; `Float(index)` casts failed SPIR-V artifact generation until that compiler surface is explicitly expanded.
- Keep remote install/update/sharpen ideas behind the `blade` API boundary when they land.
- Preserve synthetic Cargo blades; Rust crates must remain equip-able without needing duplicate Kain manifests.
- Keep Windows process path normalization at bridge/process-spawn boundaries. Node can fail on `\\?\` paths even though Rust accepts them.
- For repo-local MCP proof, prefer the direct owned operator surface `kain run blades/kain-mcp` or the managed installed `C:\Users\Admin\.cargo\bin\kain.exe run D:\Kain-Lang\blades\kain-mcp`. Keep `scripts/python/launch_kain_mcp.py` for explicit managed-sync/debug flows rather than as the default boot path.
- The CLI now suppresses its banner on non-terminal stdout. Preserve that rule for machine-facing consumers; do not add new stdout preambles ahead of MCP or JSON output without an explicit suppression path.
- When a Codex MCP config still targets the Python launcher, use a larger startup timeout and check `C:\Users\Admin\.kain\state\kain_mcp_launcher_trace.jsonl` before debugging the blade itself. If that trace file did not get a fresh `launcher_start` entry, Codex never invoked the launcher block and the fix is a session restart or config reload, not a Kain runtime patch.

## Recent Lessons

- New runnable blades should prefer the shared essential library blades first: `kain-fmt`, `kain-log`, `kain-fsx`, `kain-config`, `kain-process-kit`, `kain-http`, `kain-actor-kit`, `kain-interop-kit`, and `kain-json`.
- If a blade launched from `blades/<name>/src` cannot import a sibling blade from the repo-root workspace, debug `blade::discover_blade_module_roots_from` and ancestor-workspace module roots before adding path hacks.
- After blade-system changes, validate runtime imports with a freshly built repo CLI (`cargo run -p cli -- ...` or a rebuilt `target\\...\\kain.exe`). The PATH-installed `C:\\Users\\Admin\\.cargo\\bin\\kain.exe` can still point at older blade-system behavior.
