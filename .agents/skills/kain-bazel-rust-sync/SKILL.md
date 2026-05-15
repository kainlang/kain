---
name: kain-bazel-rust-sync
description: Use when adding, changing, debugging, or validating Kain's Bazel Rust workspace lane, especially `MODULE.bazel`, root `BUILD.bazel` aliases, generated crate `BUILD.bazel` files, `.bazelrc` cache/temp behavior, `Cargo.Bazel.lock`, or `tools/bazel/sync_rust_builds.py`.
---

# Kain Bazel Rust Sync

Use this skill when the task touches Bazel support for Rust workspace crates.

## What this lane owns

- `MODULE.bazel` owns `rules_rust`, crate-universe, Rust toolchain, and crate annotations.
- `Cargo.Bazel.lock` is the crate-universe lockfile.
- `tools/bazel/sync_rust_builds.py` is the Cargo-metadata-to-`BUILD.bazel` generator.
- Crate-local `BUILD.bazel` files under `crates/`, promoted `apps/`, and `runtime/parallel/rust/` are generated output. Do not hand-edit them.
- Root `BUILD.bazel` owns convenience aliases such as `//:kain`, `//:kn`, and `//:blade`.

## Required workflow

1. Read `.bazelrc`, `MODULE.bazel`, root `BUILD.bazel`, and `tools/bazel/sync_rust_builds.py`.
2. If Cargo manifests changed, repin crate-universe when needed:
   - PowerShell: `$env:CARGO_BAZEL_REPIN='1'; bazel fetch //:kain`
3. Regenerate generated crate builds:
   - `python tools/bazel/sync_rust_builds.py`
4. Verify generator drift:
   - `python tools/bazel/sync_rust_builds.py --check`
5. Validate focused targets before broad suites:
   - `bazel build //:kain --config=dev`
   - `bazel build //:kn --config=dev`
   - `bazel build //:blade --config=dev`
   - `bazel test //crates/kain-build:unit_test --config=dev`

## Shared launcher contract

- On this Windows workstation, `kain` and `kn` in PATH should resolve through the Bazel-backed launcher shims installed in `D:/Kain-Bazel/bin` and shadowed into `%USERPROFILE%/.cargo/bin`, not through copied Cargo release binaries.
- The native launcher shim is the source of truth because it dispatches to `scripts/windows/launch-bazel-cli.ps1`, which runs `bazel build //:kain` or `//:kn` before executing the real Bazel artifact. That is what prevents library-only Bazel work from leaving the CLI image stale even when an agent inherits an old PATH order.
- Refresh or install the wrappers with:
  - `powershell -ExecutionPolicy Bypass -File scripts/windows/sync-kain-source-of-truth.ps1 -PersistUserEnv`
- The Bazel-backed repo binary launched through `kain`, `kn`, `D:/Kain-Bazel/bin/kain.exe`, or `%USERPROFILE%/.cargo/bin/kain.exe` should report `Binary Kind: bazel-output` in `kain doctor`.

## D-drive cache contract

- Keep Bazel heavy outputs off `C:`.
- `.bazelrc` should keep `--output_user_root`, `--repository_cache`, repo/action/test temp env, and disk cache on `D:`. The current shared disk cache path is `D:/Kain-Bazel/disk-cache`.
- If these paths change, rerun `bazel info output_base` and confirm it reports `D:/kain-bazel/output-user-root/...`.
- Keep the default lane interactive by budgeting local resources instead of blindly using all host threads. The current machine profile is:
  - `--jobs=HOST_CPUS*.625`
  - `--loading_phase_threads=HOST_CPUS*.5`
  - `--local_resources=cpu=HOST_CPUS*.625`
  - `--local_resources=memory=HOST_RAM*.75`
  - `--local_test_jobs=HOST_CPUS*.25`
- When you explicitly want to push harder, use `--config=maxperf`.

## Generator rules

- Cargo manifests remain source of truth; generated `BUILD.bazel` files should be reproducible.
- Keep throwaway local test apps out of the root Cargo workspace unless they are intentionally promoted into the core Bazel lane. If a one-off experiment disappears from disk but stays in `Cargo.toml`, both `cargo metadata` and crate-universe repins will break.
- Binaries/tests in a package with a normal library target must depend on `:<package>` because Bazel does not inherit Cargo's implicit same-package library visibility.
- Keep local path deps explicit and use `all_crate_deps(...)` for external crate-universe deps.
- Keep build-script handling in the generator and crate annotations, not one-off edits in generated files.

## Known host notes

- Windows Bazel tests need `PATH` and `PATHEXT` inherited so subprocess-driven tests can find tools like `rustc`.
- `MODULE.bazel` now carries a two-patch `rules_swift` override on Windows: one guards the missing-`SDKROOT` path, and the second fixes the generated Windows toolchain stanza so it no longer emits `target_compatible_with = APPLE_PLATFORMS_CONSTRAINTS[arch]`. If the old `name 'arch' is not defined` analysis error returns, treat it as override drift rather than accepted noise.
- `bazel test //:developer_smoke_tests --config=dev` is the current green Rust suite on this host.
- `bazel test //:workspace_diagnostic_tests --config=dev` is intentionally diagnostic. Known failures include source-level `kain-core`, `cli`, and `runtime:native_test_actor_monitor_link` failures.
