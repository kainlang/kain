---
name: runtime-core
description: Use when adding, changing, debugging, validating, or reviewing Kain's native runtime core substrate, especially `runtime/native/src/core`, core ABI/service headers, actor/async/ownership/entangle/realtime startup-shutdown behavior, runtime conformance lanes, or native-core proof packs. Not for package-local bridges, Bazel plumbing, or authored Kain code.
---

# Runtime Core

Read `ARCHITECTURE.md` and `MEMORY.md` first, then narrow to the touched lane.

## Owns

- Generic native runtime substrate under `runtime/native/src/core/**` and matching headers in `runtime/native/include/**`.
- Process-local runtime machinery such as startup/shutdown, service tables, diagnostics, actor/runtime ownership, async, entangle, reflection, realtime, platform parity, and ABI/core contracts.
- Native-core proofs and conformance lanes such as `runtime/conformance/actor_runtime`, `async_runtime`, `diagnostics`, `abi_parity`, `host_bridge`, `hot_reload`, `platform_parity`, and `reflection`.

## Does Not Own

- Authored Kain code or public usage patterns. That belongs in `lang-*`.
- Parser, typechecker, LLVM lowering, or selfhost changes. Co-trigger `bootstrap-*` for those.
- Domain stdlib bridges like fs/input/net/process/ui. Use `runtime-stdlib`.
- GPU executors, graphics-runtime bundle execution, or Vulkan package bridges. Use `runtime-gpu` or `package-*`.
- Bazel sync, launcher shims, or generated BUILD drift. Use `tool-build-system`.

## Working Rules

- Keep generic runtime semantics here and keep package policy out of `runtime/native`.
- Prefer existing service-table and ABI surfaces over inventing side channels.
- If a change spans runtime core plus compiler lowering, split ownership cleanly and validate both sides.
- When pointer math, capacity logic, or ABI layout changes, prove the native-core invariant instead of relying on tests alone.

## Validation

```powershell
kain runtime build
mcp__z3_local__.run_proof_pack(path="D:\Kain-Lang\runtime\native\src\core", lane="full")
bash runtime/conformance/actor_runtime/run_tests.sh --verbose
bash runtime/conformance/async_runtime/run_tests.sh --verbose
bash runtime/conformance/diagnostics/run_tests.sh --verbose
kain runtime validate --skip-cli-build
```
