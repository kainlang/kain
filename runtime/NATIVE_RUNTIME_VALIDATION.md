# Native Runtime Validation

`cargo build -p cli` builds the Rust compiler host (`kain.exe`, `kn.exe`, `blade.exe`).
It does not prebuild the native C runtime bundle by itself.

The native runtime gets compiled in two main ways:

- On-demand during `kain build <file>.kn -t llvm` or `kain build <file>.kn -t c`.
  The CLI writes the backend artifact (`.ll` or `.c`), resolves `runtime/native_core_runtime.toml` first, compiles the listed C/C++ runtime sources with Clang, and links those objects/archives into the final executable.
- Standalone through the runtime validation helpers in `runtime/`, which prove the manifest-driven runtime bundle independently of a single Kain program.

## First-Class CLI Commands

Prefer these top-level commands when you are operating from a Kain checkout:

```powershell
kain runtime build
kain runtime validate
```

They resolve the repo root from `KAIN_REPO_ROOT`, the current working tree, or
a repo-built `kain` binary, then forward to the existing platform wrappers.
That keeps the runtime operator workflow discoverable in `kain --help` without
duplicating the underlying bash/PowerShell implementation.

## Canonical Commands

Unix-like shells:

```bash
cargo build -p cli
./runtime/compile_native_runtime.sh
./runtime/fixtures/validate_all.sh
./runtime/conformance/run_all.sh
./runtime/validate_native_runtime.sh
```

PowerShell wrappers:

```powershell
powershell -ExecutionPolicy Bypass -File runtime\compile_native_runtime.ps1
powershell -ExecutionPolicy Bypass -File runtime\fixtures\validate_all.ps1
powershell -ExecutionPolicy Bypass -File runtime\conformance\run_all.ps1
powershell -ExecutionPolicy Bypass -File runtime\validate_native_runtime.ps1
```

## Bazel Runtime Lane

The repo also has a Bazel-native runtime lane that mirrors the manifest split:

- `//runtime:native_runtime` is the lean default Bazel runtime target and currently aliases `native_core_runtime.toml`.
- `//runtime:native_full_runtime` is the broad manifest-backed Bazel target for app/vendor work.

Regenerate the Bazel manifest data any time `runtime/native_core_runtime.toml` or
`runtime/native_runtime.toml` changes:

```powershell
py -3 tools/bazel/sync_native_runtime_builds.py
py -3 tools/bazel/sync_native_runtime_builds.py --check
bazel build //runtime:all
bazel test //runtime:native_runtime_tests
```

Current Windows contract:

- The validated Windows/MSVC Bazel lane is the lean core runtime plus the actor C tests.
- `//runtime:native_full_runtime` is intentionally not part of the Windows default Bazel lane yet because the broad manifest still includes QuickJS/vendor and related sources that are not Bazel-clean under MSVC.

## Important Distinction

You do not normally ship a separate `kain_runtime.exe`.
The owned native runtime is primarily a bundle of C/C++ sources that compile to objects and archives, then get linked into each generated native program.

That means:

- `cargo build -p cli` builds the compiler host.
- `kain build ... -t llvm` or `-t c` builds the user program and links the runtime into that program.
- `kain runtime build` is the first-class standalone runtime-bundle command.
- `kain runtime validate` is the first-class aggregate validation command.
- `runtime/compile_native_runtime.*` validates that the manifest-declared runtime sources compile as a bundle on their own.

## Current Windows Contract

The canonical runtime implementation and validation lanes still live in the bash scripts.
The `.ps1` files are thin Windows operator wrappers that locate `bash`, forward the right flags, and keep the repo's source of truth in one place.
