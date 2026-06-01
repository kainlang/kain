---
name: tool-build-system
description: "Use when adding, changing, debugging, or validating Kain's repo build plumbing: Bazel Rust/runtime sync, generated `BUILD.bazel` drift, launcher shims, runtime build wrappers, `.bazelrc`, `kain doctor` build provenance, or 'how do I build Kain itself?'. This is the single explicit skill for repo build-system ownership."
---

# Tool Build System

This is the one explicit skill for repo build plumbing. If the question is Bazel sync, generated Rust graph drift, launcher shims, managed PATH binaries, runtime build wrappers, or "how do I build Kain itself?", use this skill.

## Owns

- Rust Bazel graph ownership: `.bazelrc`, `MODULE.bazel`, root `BUILD.bazel`, `Cargo.Bazel.lock`, `tools/bazel/kain_public_targets.bzl`, `tools/bazel/sync_rust_builds.py`, and the generated in-tree `crates/*/BUILD.bazel` files.
- Native runtime Bazel sync: `runtime/BUILD.bazel`, `runtime/native_runtime_rules.bzl`, `runtime/runtime_manifest_data.bzl`, `runtime/native_core_runtime.toml`, `runtime/native_runtime.toml`, and `tools/bazel/sync_native_runtime_builds.py`.
- Launcher and provenance plumbing: `scripts/windows/sync-kain-source-of-truth.ps1`, `scripts/windows/launch-bazel-cli.ps1`, managed PATH wrappers, and `kain doctor` build/source-of-truth status.
- Windows Bazel operator GUI: root `bazel_server_gui.kn`, compiled to `X:\bazel_server_gui.exe`, for live server status, start, stop, refresh, and output-base inspection.
- Build/operator entrypoints for producing the repo binaries and runtime bundle: `bazel build //:kain //:kn`, `kain runtime build`, and `kain runtime validate`.

## Does Not Own

- Compiler/frontend/runtime semantics themselves. Use the owning `bootstrap-*` or `runtime-*` skill once the build lane is healthy.
- Package-local build helpers inside a blade unless the issue is really shared build plumbing.
- Public command UX beyond launcher/build provenance behavior.

## Working Rules

- Cargo manifests and runtime manifests are the sources of truth. First-party Rust Bazel targets come from generated in-tree `crates/*/BUILD.bazel` files; do not reintroduce the legacy `@kain_workspace_rust` overlay as a second first-party graph.
- If a first-party Rust crate uses `build.rs` to read package-local manifests, spec packs, or generated-data inputs outside the usual `src/tests/examples/...` tree, teach `tools/bazel/sync_rust_builds.py` to mirror that directory into `COMMON_COMPILE_DATA` or Bazel can compile a lying build-script output with missing inputs.
- On Windows, `force_all_deps_direct` should add a compact `_compact_dependency_search` root without replacing the normal non-proc-macro dependency roots. Bazel can stage some rlib symlinks as link artifacts that rustc will not accept as crate files, so the compact root reduces proc-macro/DLL search pressure but must not become sole metadata authority.
- Keep heavy Bazel outputs under the short Windows root `Z:\_b\...` and preserve the launcher contract that `kain`/`kn` resolve to Bazel-backed wrappers on this workstation.
- `X:\.kain\bin\kain.exe` is only the tiny launcher shim; do not treat its timestamp alone as proof that the real CLI is stale. The authoritative freshness check is `python scripts\python\kain_bazel_sync.py status --json`.
- On Windows, if Bazel cannot delete `bazel-out\...\crates\cli\kain.exe` because the file is locked, the launcher sync lane should build `//:kn` and stage that binary under the requested `kain` identity. `kain` and `kn` share the same Rust entrypoint and only diverge by launcher name / `argv[0]`.
- Launcher sync should treat `kain` and `kn` as one refresh transaction: capture one source stamp per pass and rerun if the watched source stamp moves during the sync, or one binary can come out stale immediately after the other.
- Build the launcher shim with `TMP`, `TEMP`, and `TMPDIR` pointed under the sync state root (`X:\.kain\state\tmp` on this machine). The host default temp drive can hit `WinError 112` / `LNK1108` and make a healthy launcher look broken.
- If `X:\.kain\bin\kain.exe` is live during shim install, expect a `kain.exe.pending.*` staged replacement instead of an in-place overwrite. That is an operator lock issue, not proof that the staged Bazel binary is stale.
- Native LLVM/clang link steps on Windows should not assume a VS Developer Shell. The repo now auto-discovers `LIB` search roots from `VCToolsInstallDir`, `WindowsSdkDir`, and common VS/Windows Kits install paths so `kain -t llvm` can link `legacy_stdio_definitions`, `ucrt`, and WinSDK libs from a normal shell.
- When a change touches build provenance, prove it through `kain doctor`, not just by eyeballing `bazel-bin`.
- If the problem is "runtime build wrapper fails" or "fresh Kain binary is stale", keep it here rather than scattering that guidance across runtime or package skills.

## Validation

```powershell
bazel query //:kain
py -3 tools/bazel/sync_native_runtime_builds.py --check
bazel build //:kain --config=dev
bazel build //:kn --config=dev
bazel test //:developer_smoke_tests --config=dev
powershell -ExecutionPolicy Bypass -File scripts/windows/sync-kain-source-of-truth.ps1 -PersistUserEnv
python scripts/python/kain_bazel_sync.py status --json
kain doctor
```
