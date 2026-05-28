---
name: tool-build-system
description: "Use when adding, changing, debugging, or validating Kain's repo build plumbing: Bazel Rust/runtime sync, generated `BUILD.bazel` drift, launcher shims, runtime build wrappers, `.bazelrc`, `kain doctor` build provenance, or 'how do I build Kain itself?'. This is the single explicit skill for repo build-system ownership."
---

# Tool Build System

This is the one explicit skill for repo build plumbing. If the question is Bazel sync, generated Rust graph drift, launcher shims, managed PATH binaries, runtime build wrappers, or "how do I build Kain itself?", use this skill.

## Owns

- Rust Bazel graph ownership: `.bazelrc`, `MODULE.bazel`, root `BUILD.bazel`, `Cargo.Bazel.lock`, `tools/bazel/kain_public_targets.bzl`, `tools/bazel/kain_rust_workspace.bzl`, `tools/bazel/kain_rust_workspace_generator.py`, `tools/bazel/sync_rust_builds.py`, and the generated first-party overlay repo `@kain_workspace_rust`.
- Native runtime Bazel sync: `runtime/BUILD.bazel`, `runtime/native_runtime_rules.bzl`, `runtime/runtime_manifest_data.bzl`, `runtime/native_core_runtime.toml`, `runtime/native_runtime.toml`, and `tools/bazel/sync_native_runtime_builds.py`.
- Launcher and provenance plumbing: `scripts/windows/sync-kain-source-of-truth.ps1`, `scripts/windows/launch-bazel-cli.ps1`, managed PATH wrappers, and `kain doctor` build/source-of-truth status.
- Build/operator entrypoints for producing the repo binaries and runtime bundle: `bazel build //:kain //:kn`, `kain runtime build`, and `kain runtime validate`.

## Does Not Own

- Compiler/frontend/runtime semantics themselves. Use the owning `bootstrap-*` or `runtime-*` skill once the build lane is healthy.
- Package-local build helpers inside a blade unless the issue is really shared build plumbing.
- Public command UX beyond launcher/build provenance behavior.

## Working Rules

- Cargo manifests and runtime manifests are the sources of truth. First-party Rust Bazel targets come from the generated `@kain_workspace_rust` overlay repo, not from hand-maintained checked-in crate `BUILD.bazel` files.
- If a first-party Rust crate uses `build.rs` to read package-local manifests, spec packs, or generated-data inputs outside the usual `src/tests/examples/...` tree, teach `tools/bazel/sync_rust_builds.py` to mirror that directory into `COMMON_COMPILE_DATA` or Bazel can compile a lying build-script output with missing inputs.
- Keep heavy Bazel outputs on `D:` and preserve the launcher contract that `kain`/`kn` resolve to Bazel-backed wrappers on this workstation.
- When a change touches build provenance, prove it through `kain doctor`, not just by eyeballing `bazel-bin`.
- If the problem is "runtime build wrapper fails" or "fresh Kain binary is stale", keep it here rather than scattering that guidance across runtime or package skills.

## Validation

```powershell
bazel query @kain_workspace_rust//:kain
py -3 tools/bazel/sync_native_runtime_builds.py --check
bazel build //:kain --config=dev
bazel build //:kn --config=dev
bazel test //:developer_smoke_tests --config=dev
powershell -ExecutionPolicy Bypass -File scripts/windows/sync-kain-source-of-truth.ps1 -PersistUserEnv
kain doctor
```
