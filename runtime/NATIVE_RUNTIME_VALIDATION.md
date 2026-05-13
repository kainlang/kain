# Native Runtime Validation

`cargo build -p cli` builds the Rust compiler host (`kain.exe`, `kn.exe`, `blade.exe`).
It does not prebuild the native C runtime bundle by itself.

The native runtime gets compiled in two main ways:

- On-demand during `kain build <file>.kn -t llvm` or `kain build <file>.kn -t c`.
  The CLI writes the backend artifact (`.ll` or `.c`), resolves `runtime/native_core_runtime.toml` first, compiles the listed C/C++ runtime sources with Clang, and links those objects/archives into the final executable.
- Standalone through the runtime validation helpers in `runtime/`, which prove the manifest-driven runtime bundle independently of a single Kain program.

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

## Important Distinction

You do not normally ship a separate `kain_runtime.exe`.
The owned native runtime is primarily a bundle of C/C++ sources that compile to objects and archives, then get linked into each generated native program.

That means:

- `cargo build -p cli` builds the compiler host.
- `kain build ... -t llvm` or `-t c` builds the user program and links the runtime into that program.
- `runtime/compile_native_runtime.*` validates that the manifest-declared runtime sources compile as a bundle on their own.

## Current Windows Contract

The canonical runtime implementation and validation lanes still live in the bash scripts.
The `.ps1` files are thin Windows operator wrappers that locate `bash`, forward the right flags, and keep the repo's source of truth in one place.
