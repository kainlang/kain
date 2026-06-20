# Build

Master build orchestrator for the reson8 DAW.
Auto-detects the Kain project layout and dispatches build steps
through the IVT to the native `kain` compiler and C toolchain.

Run with: `kain run reson8 -- --mks src-mks/build.md`

---

## Pipeline Metadata

| Property     | Value                  |
|--------------|------------------------|
| Project      | reson8                 |
| Language     | Kain                   |
| BuildTool    | kain build             |
| Target       | llvm                   |
| ArtifactRoot | .kain/out/llvm         |
| EntryPoint   | src/main.kn            |
| NativeBridges| 3 (audio_device, vst3, clap) |

---

## Pre-flight

Verify the toolchain is on PATH and the project tree is sane
before kicking off a multi-minute build.

```markscript
let toolchain_ok = exists("kain.exe")
print(toolchain_ok)
```

> exists "build.kn"

> exists "KAIN.toml"

> exists "src/main.kn"

---

## build_kain

Full Kain build: parse, typecheck, monomorphize, codegen to LLVM IR,
link against the native C runtime, and produce `reson8.exe`.

> spawn "kain build X:/blades/reson8/ --target llvm"

> print "Kain build dispatched"

---

## check_kain

Typecheck-only pass — fast feedback loop for editor integration
and CI. No object files produced.

> spawn "kain check X:/blades/reson8/src/ --json"

> print "Typecheck complete"

---

## build_bridges

Compile the three native C bridges (audio_device, vst3_host, clap_host)
through the Kain C-FFI pipeline. The build graph wires these as
dependencies of the main executable.

> spawn "kain build X:/blades/reson8/ --target llvm --bridges"

> print "Native bridges compiled"

---

## build_native_runtime

Build the Kain native C runtime library that reson8 links against.
Covers arena/buddy allocators, actor scheduler, async runtime,
machine-stones substrate, and crash forensics.

> spawn "kain build //runtime:native_core_runtime --config=dev"

> print "Native runtime built"

---

## clean_kain

Remove all build artifacts. Use before a from-scratch build or
when the cache is suspected to be corrupt.

> print "Cleaning build artifacts"

> spawn "kain clean X:/blades/reson8/"

> print "Clean complete"

---

## build_all

Orchestrate the full build chain in dependency order. Each step
spawns a tracked process; the build halts on any non-zero exit.

> print "=== reson8 build pipeline ==="

> run check_kain

> run build_native_runtime

> run build_bridges

> run build_kain

> print "=== Build complete ==="

---

## build_release

Release-mode build with thin LTO. Slower link time, faster runtime.
Used for benchmarks and public distribution.

> print "=== reson8 release build ==="

> spawn "kain build X:/blades/reson8/ --target llvm --config=release"

> print "Release build dispatched"

---

## build_debug

Debug-mode build with full symbol info. Use when stepping through
the Rust compiler or diagnosing a crash with LLDB.

> print "=== reson8 debug build ==="

> spawn "kain build X:/blades/reson8/ --target llvm --config=debug"

> print "Debug build dispatched"

---

## build_speed

Maximum performance build: opt mode + thin LTO. Use for production
benchmarks and the daily-dev compile cycle.

> print "=== reson8 speed build ==="

> spawn "kain build X:/blades/reson8/ --target llvm --config=speed"

> print "Speed build dispatched"

---

## verify_build

After a successful build, validate the produced executable exists
and has reasonable size (sanity check against a zero-byte truncation).

```markscript
let artifact_path = "X:/blades/reson8/.kain/out/llvm/reson8.exe"
let present = exists(artifact_path)
print(present)
```

> exists "X:/blades/reson8/.kain/out/llvm/reson8.exe"

> print "Artifact verified"

---

## watch

Watch mode: rebuild on any source file change. Spawn-and-forget
loop that survives across edits.

> print "Watch mode active — rebuilding on change"

> spawn "kain run dev X:/blades/reson8/ --target llvm"

---

## Build Graph Summary

| Step             | Tool         | Duration  | Output                          |
|------------------|--------------|-----------|---------------------------------|
| check_kain       | kain check   | 5-15s     | diagnostics JSON                |
| build_native_runtime | bazel    | 30-60s    | kain_runtime.lib                |
| build_bridges    | kain build   | 10-30s    | *_bridge.obj                    |
| build_kain       | kain build   | 60-180s   | reson8.exe                      |
| build_release    | kain build   | 180-300s  | reson8.exe (optimized)          |
| build_speed      | kain build   | 300-600s  | reson8.exe (max perf)           |
| verify_build     | fs stat      | <1s       | size + mtime                    |
| clean_kain       | kain clean   | <1s       | .kain/out removed               |

---

## Exit Codes

| Code | Meaning                        |
|------|--------------------------------|
| 0    | Build succeeded                |
| 1    | Typecheck or codegen error     |
| 2    | Linker error                   |
| 3    | Native bridge build failed     |
| 4    | Runtime build failed           |
| 100  | Pre-flight check failed        |
