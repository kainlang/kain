---
name: kain-machine-stones
description: Use when adding, changing, debugging, validating, or reviewing Kain's machine-stones keyword quartet: `axiom`, `pulse`, `shatter struct`, and `teleport`, including parser/typechecker/runtime-contract wiring, formatter/import/LSP exhaustiveness, native LLVM dogfood blades, and keyword Z3 proofs.
---

# Kain Machine Stones

Use this skill for the post-intent keyword quartet:

- `axiom`: compiler-accepted machine/environment truth with predicates, guarantees, and fallback.
- `pulse`: first-class temporal beat with typed pulse locals.
- `shatter struct`: structure-of-arrays / silicon-layout intent marker on ordinary structs.
- `teleport`: destructive cross-world handoff that poisons the origin binding after moving ownership.

## Workflow

1. Read `ARCHITECTURE.md`, `MEMORY.md`, `blades/kain-example/src/main.kn`, and `blades/machine-stones/src/main.kn`.
2. Keep syntax, typing, runtime-contract, formatter/import/LSP/selfhost exhaustiveness, and native codegen behavior in sync. The main files are `crates/kain-core/src/{ast.rs,parser.rs,types.rs,formatter.rs,runtime.rs,runtime_contract.rs,low_level_memory.rs,ui.rs,comptime.rs}` plus backend/client exhaustiveness in `crates/kain-sys-codegen`, `crates/gpu`, `crates/ue5`, and `crates/cli`.
3. Preserve the current authored forms:
   ```kain
   axiom native_atomic_mask_truth:
       when target("llvm")
       when arch("x86_64")
       when capability("atomic.bitmask")
       guarantee "single-copy atomic mask writes are available"
       fallback portable_mask_update

   pulse agent_sinus every 16ms jitter 1ms:
       let gpu_particle = teleport particle from NativeWorld to GpuWorld via gpu_upload
   ```
4. `teleport` must validate both worlds, reject same-world handoffs, return the payload type, and reject later reads of a simple origin identifier with `was moved by teleport`.
5. Runtime contracts should continue emitting `axioms`, `pulses`, `shatters`, and capabilities `machine.axiom`, `time.pulse`, `time.hardware-timer`, `memory.shatter`, `world.teleport`, and `interop.zero-copy-handoff`.
6. Keep the native runtime ABI in sync when backend behavior changes: `runtime/native/include/machine_stones.h`, `runtime/native/src/core/machine_stones.c`, `runtime/native/include/stdlib_abi.h`, `stdlib/native/runtime.kn`, `stdlib/runtime.kn`, `runtime/native_core_runtime.toml`, `runtime/native_runtime.toml`, `runtime/runtime_manifest_data.bzl`, and `runtime/BUILD.bazel`.
7. Native LLVM lowering lives in `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`. It should emit axiom accept calls, pulse snapshot/fire wrappers registered through `kain_machine_pulse_start(...)`, `kain_machine_teleport_ptr` for pointer-shaped teleport values, and `kain_machine_shatter_*` SoA buffers for local direct shattered-struct array literals. For hot loop field reads, prefer compiler-proved `kain_machine_shatter_lane_base(...)` plus `(index << 3)` GEP lowering over per-access checked `kain_machine_shatter_lane_ptr(...)`.
8. Return-path cleanup must treat shatter handles as machine-stone handles, not generic RC pointers. `emit_all_scopes_cleanup` is called once per return branch, so it must preserve `shattered_array_locals` metadata while emitting sibling branches; otherwise one branch can free with `kain_machine_shatter_free(...)` and a later branch can miscompile the same handle as `rc_release(i8* ...)`.
9. Add or update durable proofs under `crates/kain-core/z3/proofs/keywords-*.yaml` for frontend semantic invariants, `runtime/native/src/core/z3/proofs/native-machine-*.yaml` for native C arithmetic, and `crates/kain-sys-codegen/z3/proofs/memory-teleport-*.yaml` / `memory-shatter-*.yaml` for LLVM pointer-handoff and shatter-cleanup invariants.
10. Dogfood through `blades/machine-stones`: run `kain check ... --target llvm`, compile to `blades/machine-stones/machine-stones.exe`, run it from the blade root, and keep `.ll`, `.pdb`, `.ilk`, runtime-contract JSON, and realtime bundle sidecars under `blades/machine-stones/.kain/out/`.

## Validation

```powershell
cargo check -p kain-core
cargo check -p kain-sys-codegen
cargo check -p gpu
cargo check -p cli
cargo test -p kain-core --test ownership_keywords_test
cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_lowers_machine_stones_to_native_runtime_abi -- --nocapture
cargo test -p kain-sys-codegen --test llvm_codegen_test llvm_cleans_shattered_array_locals_on_each_return_path -- --nocapture
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-core --lane keywords
mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\runtime\\native\\src\\core", lane="machine")
mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\crates\\kain-sys-codegen\\z3", lane="memory", pattern="proofs/memory-teleport-*.yaml")
mcp__z3_local__.run_proof_pack(path="D:\\Kain-Lang\\crates\\kain-sys-codegen\\z3", lane="memory", pattern="proofs/memory-shatter-*.yaml")
clang -I runtime/native/include runtime/native/tests/test_machine_stones.c runtime/native/src/core/machine_stones.c runtime/native/src/core/cpu.c -o target/codex-machine-stones/native_test_machine_stones.exe
target\codex-machine-stones\native_test_machine_stones.exe
py -3 tools/bazel/sync_native_runtime_builds.py --check
.\target\debug\kain.exe check blades/machine-stones/src/main.kn --target llvm
.\target\debug\kain.exe blades/machine-stones/src/main.kn -t llvm -o blades/machine-stones/machine-stones.exe
.\blades\machine-stones\machine-stones.exe
py -3 benchmark/run.py --case machine_stones_shatter_loop --languages kain,rust,cpp --runs 3 --warmups 1 --timeout 900 --kain-exe target\debug\kain.exe
```

## Native Backend Boundary

The quartet is now beyond metadata in the native LLVM/C lane, but it is intentionally bounded:

- `axiom` lowers to generated accept thunks that call `kain_machine_axiom_accept(...)` and gate target/arch/capability predicates against native runtime feature bits.
- `pulse` lowers to generated body/fire wrappers and registers generated fire thunks through `kain_machine_pulse_start(...)`; the runtime fires once immediately, then uses a process-local timer thread to keep the pulse alive while `kain_machine_pulse_snapshot(...)` supplies tick/dt/missed locals.
- `shatter struct` lowers local direct array literals of shattered struct literals into `kain_machine_shatter_alloc(...)` SoA buffers. The hot path caches `kain_machine_shatter_lane_base(...)` values and uses `(index << 3)` byte GEPs when literal or `for range(...)` indexes are compiler-proven in bounds; `kain_machine_shatter_lane_ptr(...)` remains the checked fallback for unproved indexes. Wider propagation through parameters, returns, iterators, mutation, and arbitrary arrays is future work.
- Shatter cleanup on normal lexical scope exit may remove shatter metadata after freeing the handle, but whole-function return cleanup must not mutate that metadata because it is replayed while lowering sibling return branches.
- `teleport` lowers pointer-shaped values through `kain_machine_teleport_ptr(...)`, preserving the exact address as a zero-copy handoff. Scalar teleports still preserve value semantics and call the note hook because there is no pointer identity to transfer.

When extending this surface, prefer widening these native ABI-backed lowerings over changing syntax. If a future pass adds full pulse scheduling or cross-GPU/world zero-copy ABI, prove ownership/address/liveness invariants with Z3 before landing it.
