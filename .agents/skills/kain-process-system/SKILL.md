---
name: kain-process-system
description: Use when adding, changing, debugging, validating, or reviewing Kain's native child process, stdio pipe, and PTY pipeline, including crates/kain-process, stdlib/native/process.kn, runtime/native/include/kain_native_process_system.h, runtime/native/src/core/kain_native_process_system.c, io.process service-table wiring, process conformance tests, and native_process_stdio fixtures.
---

# Kain Process System

## Start Here

- Work from `D:\Kain-Lang`.
- Read `ARCHITECTURE.md` and `MEMORY.md` before changing process runtime behavior.
- Keep `crates/kain-process` as the portable contract crate, `runtime/native/include/kain_native_process_system.h` as the raw C ABI, `runtime/native/src/core/kain_native_process_system.c` as the host implementation, and `stdlib/native/process.kn` as the Kain-facing wrapper.

## Architecture Rules

- Keep process, pipe, and PTY semantics capability-shaped. Do not hardcode app workflows, MCP server behavior, terminal UI behavior, or shell-specific assumptions into the runtime.
- `io.process` exposes a native function table. On Windows it should report available; on unsupported hosts it should report degraded and return explicit unsupported diagnostics.
- Reset/shutdown must clean up child processes and handles through `kain_native_process_reset()`.
- Standard process spawn should support executable, args, cwd, env, inherit-environment, and inherit/pipe/null stdio modes.
- PTY support is Windows ConPTY in v1. Load ConPTY entrypoints dynamically and keep non-Windows behavior explicit rather than silently pretending PTY exists.
- Kain wrappers should stay thin and predictable: expose spec builders, spawn/wait/poll, stdio read/write/capture, PTY write/resize/read/capture, and diagnostics.
- Keep the checked `size_t` helpers in `kain_native_process_system.c` on the hot path for buffer growth and allocation math. New capture, UTF-8, wide-text, or environment-block code should route through explicit overflow checks instead of open-coded `length + extra` arithmetic.

## Key Files

- `crates/kain-process/src/lib.rs`: portable Rust data contracts and builder defaults.
- `runtime/native/include/kain_native_process_system.h`: exported ABI and `KainNativeProcessFunctionTable`.
- `runtime/native/src/core/kain_native_process_system.c`: native process registry, Win32 process spawn, pipe draining, ConPTY spawn, and unsupported-host stubs.
- `runtime/native/src/core/kain_runtime_services.c`: `io.process` service descriptor and function-table pointer.
- `runtime/native_core_runtime.toml` and `runtime/native_runtime.toml`: include the process C source for native builds.
- `stdlib/native/process.kn`: Kain stdlib wrappers used by LLVM/direct-C targets.
- `runtime/conformance/process_runtime/`: C conformance harness for process and PTY ABI behavior.
- `runtime/fixtures/native_process_stdio/main.kn`: executable Kain proof for process stdout, stdin, and PTY smoke coverage.

## Validation

Run the focused checks before claiming process runtime changes are done:

```powershell
cargo fmt -p kain-process
cargo test -p kain-process
clang -fsyntax-only -I runtime/native/include runtime/native/src/core/kain_native_process_system.c
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\runtime\native\src\core --lane process
cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_native_process_and_pty_primitives -- --exact
cargo test -p kain-sys-codegen --test c_codegen_test c_backend_keeps_native_process_symbols_as_declarations -- --exact
bash runtime/conformance/process_runtime/run_tests.sh --verbose
target\debug\kain.exe build runtime\fixtures\native_process_stdio\main.kn --target llvm --output runtime\fixtures\native_process_stdio\generated\native_process_stdio.ll
runtime\fixtures\native_process_stdio\generated\native_process_stdio.exe
```

Delete `runtime\fixtures\native_process_stdio\generated` before committing unless the repo intentionally starts tracking generated fixture outputs.
Do not commit `runtime\native\src\core\z3\reports` or root `z3\reports`; those JSON files are validation output.
