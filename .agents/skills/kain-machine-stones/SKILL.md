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
6. Add or update durable proofs under `crates/kain-core/z3/proofs/keywords-*.yaml` for semantic invariants. Run the `keywords` lane after changes.
7. Dogfood through `blades/machine-stones`: run `kain check ... --target llvm`, compile to `blades/machine-stones/machine-stones.exe`, run it from the blade root, and keep `.ll`, `.pdb`, `.ilk`, runtime-contract JSON, and realtime bundle sidecars under `blades/machine-stones/.kain/out/`.

## Validation

```powershell
cargo check -p kain-core
cargo check -p kain-sys-codegen
cargo check -p gpu
cargo check -p cli
cargo test -p kain-core --test ownership_keywords_test
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-core --lane keywords
.\target\debug\kain.exe check blades/machine-stones/src/main.kn --target llvm
.\target\debug\kain.exe blades/machine-stones/src/main.kn -t llvm -o blades/machine-stones/machine-stones.exe
.\blades\machine-stones\machine-stones.exe
```

## Current Boundary

The quartet is currently core language semantics plus runtime-contract metadata. LLVM/native lowering treats `teleport` as a value pass-through after typechecking, and `pulse` / `shatter` / `axiom` are not yet full backend-transforming optimizers. Future backend passes should implement timer scheduling, layout transformation, and ABI/GPU zero-copy handoff against the existing contract fields instead of changing the surface syntax.
