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
- Crate-local `BUILD.bazel` files under `crates/`, `apps/`, and `runtime/parallel/rust/` are generated output. Do not hand-edit them.
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

## D-drive cache contract

- Keep Bazel heavy outputs off `C:`.
- `.bazelrc` should keep `--output_user_root`, `--repository_cache`, repo/action/test temp env, and disk cache on `D:` or inside the D-drive workspace.
- If these paths change, rerun `bazel info output_base` and confirm it reports `D:/kain-bazel/output-user-root/...`.

## Generator rules

- Cargo manifests remain source of truth; generated `BUILD.bazel` files should be reproducible.
- Binaries/tests in a package with a normal library target must depend on `:<package>` because Bazel does not inherit Cargo's implicit same-package library visibility.
- Keep local path deps explicit and use `all_crate_deps(...)` for external crate-universe deps.
- Keep build-script handling in the generator and crate annotations, not one-off edits in generated files.

## Known host notes

- Windows may emit a noisy `rules_swift` local-config error: `name 'arch' is not defined`. With `--keep_going`, Rust targets can still complete; do not chase it unless it blocks the requested target.
- `bazel test //:crate_tests --config=dev` is currently diagnostic, not required green. Known failures include source-level `kain-core` unit assumptions and a `cli` unit-test process exit under Bazel.
