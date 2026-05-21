---
name: kain-bazel-runtime-sync
description: Use when adding, changing, debugging, or validating the Bazel lane for Kain's native C runtime, especially `runtime/BUILD.bazel`, `runtime/native_runtime_rules.bzl`, `runtime/runtime_manifest_data.bzl`, `tools/bazel/sync_native_runtime_builds.py`, or the split between `runtime/native_core_runtime.toml` and `runtime/native_runtime.toml`.
---

# Kain Bazel Runtime Sync

Use this skill when the task touches Bazel support for the native runtime bundle.

## What this lane owns

- `tools/bazel/sync_native_runtime_builds.py` is the manifest-to-Bazel generator.
- `runtime/runtime_manifest_data.bzl` is generated output. Do not hand-edit it.
- `runtime/native_runtime_rules.bzl` owns the shared Bazel macro and platform compile/link settings.
- `runtime/BUILD.bazel` owns the Bazel-visible runtime targets and C tests.

## Default target contract

- `runtime/native_core_runtime.toml` is the lean/default runtime manifest.
- `runtime/native_runtime.toml` is the lean compatibility mirror of the core manifest.
- `//runtime:native_runtime` should stay aligned with the lean default lane unless there is a deliberate architecture change.
- `//runtime:native_full_runtime` is only a legacy-named compatibility mirror target over the same lean source set.

## Required workflow

1. Read `runtime/native_core_runtime.toml`, `runtime/native_runtime.toml`, `runtime/BUILD.bazel`, and `tools/bazel/sync_native_runtime_builds.py`.
2. If a runtime manifest changed, regenerate:
   - `py -3 tools/bazel/sync_native_runtime_builds.py`
3. Verify the generated file is current:
   - `py -3 tools/bazel/sync_native_runtime_builds.py --check`
4. Validate the Bazel package:
   - `bazel build //runtime:all`
   - `bazel test //runtime:native_runtime_tests`

## Windows notes

- The validated Windows/MSVC Bazel lane is `bazel build //runtime:all` plus `bazel test //runtime:native_runtime_tests`.
- `//runtime:native_full_runtime` should resolve to the same lean runtime data as `//runtime:native_runtime`; do not repurpose it into a second vendor lane.
- If `<stdatomic.h>` fails under Bazel MSVC, check that `runtime/native_runtime_rules.bzl` still passes `/experimental:c11atomics`.

## Editing rules

- Prefer changing the TOML manifests and rerunning the generator over editing `runtime/runtime_manifest_data.bzl` by hand.
- Keep platform-specific Bazel logic in `runtime/native_runtime_rules.bzl`, not duplicated across many targets.
- Reuse the existing actor C tests under `runtime/native/tests` before inventing new one-off Bazel-only smoke tests.
