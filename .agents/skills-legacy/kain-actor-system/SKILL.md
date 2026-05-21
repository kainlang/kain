---
name: kain-actor-system
description: Use when adding, changing, debugging, validating, or reviewing Kain's actor system, especially crates/kain-actor, actor semantics in crates/kain-core, stdlib actor helpers, runtime/native actor ABI integration, supervision/mailbox/scheduler policy, typed actor contracts, or LLVM/direct-C actor lowering.
---

# Kain Actor System

Use this skill for actor work in `D:\Kain-Lang`, especially:

- `crates/kain-actor/**`
- actor parsing/typechecking/runtime in `crates/kain-core/src/{ast.rs,parser.rs,types.rs,runtime.rs,runtime_contract.rs}`
- actor native ABI files under `runtime/native/include/kain_runtime_actor.h` and `runtime/native/src/core/kain_runtime_actor.c`
- `stdlib/gen_server.kn`, `stdlib/native/actor.kn`, and native LLVM/direct-C actor lowering

## Ownership

- `crates/kain-core` owns actor syntax, AST, typechecking, interpreter execution, and integration with typed programs.
- `crates/kain-actor` owns reusable actor-system model contracts: IDs, addresses, messages, definitions, mailbox policy, lifecycle, supervision, scheduler policy, behavior contracts, registry snapshots, system validation, runtime events/snapshots, and native ABI descriptors.
- `runtime/native` owns the C ABI floor and low-level actor runtime substrate.
- `stdlib/*` exposes Kain-facing helpers. Do not bury new actor APIs only in Rust if Kain source should use them.

Keep `crates/kain-actor/src/lib.rs` as a public index. Put implementation in focused files such as `id.rs`, `message.rs`, `definition.rs`, `mailbox.rs`, `lifecycle.rs`, `supervision.rs`, `scheduler.rs`, `behavior.rs`, `registry.rs`, `system.rs`, or `native.rs`.

`crates/kain-actor/src/native.rs` owns the Rust-side native actor ABI descriptor. Keep `runtime/native/include/kain_runtime_actor.h`, `runtime/native/include/kain_runtime_native_stdlib.h`, LLVM actor spawn/message layout, and actor parity tests in sync when changing actor ABI fields or symbols.

## Current Flow

`Kain actor AST -> kain-core typechecker resolves state/handler/method types -> TypedActor.actor_contract: kain_actor::ActorDefinition -> runtime-contract reflection and interpreter/runtime actor IDs/messages consume kain-actor primitives`

`kain-core` currently still runs actor loops directly in `runtime.rs`; do not claim `kain-actor` owns execution until a runtime-trait/scheduler pass actually lands.

Native ABI ownership:

- `KainActorAbiDescriptor`, `kain_actor_abi_descriptor`, and `kain_actor_abi_descriptor_is_compatible` are the C runtime compatibility surface.
- `KainActorSpawnConfig.retain_user_data` is the ownership boundary. LLVM-generated actor state must set it to `1`; native C/C++ callers should leave it `0` unless the pointer is a Kain RC allocation.
- `KainActorMessage.data_size` must survive mailbox send/receive. `MessageNode` carries `data_size`; tests should catch regressions.
- `kain_actor_reply_port_send`, `kain_actor_reply_port_wait`, and `kain_actor_reply_port_wait_i64` are now a real ask/reply ABI surface, not debug-only helpers. Native LLVM reply-port sends intentionally bypass generic mailbox enqueue/dequeue on the return leg; keep codegen, runtime, and Rust-side symbol contracts aligned.
- Actors shut down while still queued must still finalize lifecycle side effects: monitor notifications, supervisor observations, and link propagation when applicable.

## Validation

Use focused proof first:

```powershell
cargo fmt -p kain-actor -p kain-core
cargo test -p kain-actor --target-dir target\codex-kain-actor
cargo test -p kain-core --test actor_contract_test --target-dir target\codex-kain-actor-core
cargo test -p kain-core ask_timeout_builtin_round_trips_actor_reply --target-dir target\codex-kain-actor-core
```

The broad filter `cargo test -p kain-core actor` may hit unrelated fixture-path failures such as `m:/Code/Factory/Example_GAS/test_targets.kn`. Prefer targeted actor tests unless the fixture exists locally.

For native actor changes also validate the current native actor/intent fixture through the live `kain` binary and the native runtime manifest paths described in the `kain-engineer` skill.

For C runtime actor changes run:

```powershell
bash runtime/conformance/actor_runtime/run_tests.sh --test-timeout 45 --verbose
```

The actor conformance runner includes `test_actor_abi_contract.c`, basic spawn, registry, mailbox backpressure, monitors, links, supervision, and scheduler coverage. On Windows, Git Bash is currently the easiest path; direct `toolchain\llvm\bin\clang.exe` compilation of `test_actor_abi_contract.c` is a useful fallback.

For solver-backed arithmetic invariants in the native actor substrate, also run the repo-local proof pack:

```powershell
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\runtime\native\src\core --lane actor
```

That pack lives at `runtime/native/src/core/z3` and currently proves mailbox count bounds, receive underflow prevention, scheduler queue-depth accounting, max-depth monotonicity, restart-limit arithmetic, and invalid actor ID reservation.

If you touch shared templates, manifest lanes, or cross-runtime proof wiring in that pack, also rerun:

```powershell
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\runtime\native\src\core --lane full
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --workspace --project-root D:\Kain-Lang --lane smoke
```

For native LLVM ask/reply regressions, also run the real executable and benchmark surfaces:

```powershell
target\\debug\\kain.exe check runtime\\fixtures\\native_actor_ask_roundtrip\\main.kn --target llvm
target\\debug\\kain.exe blades\\actor-ask-roundtrip\\src\\main.kn -t llvm -o blades\\actor-ask-roundtrip\\actor-ask-roundtrip.ll
python benchmark\\run.py --case actor_mailbox_erlang --languages kain,erlang --runs 1 --warmups 0 --timeout 600
```

## Common Gotchas

- This repo can have unrelated dirty work in the same files. Stage actor hunks carefully; do not commit blades/fs/native-runtime changes while doing actor work.
- Actor ID raw `0` is invalid and reserved to match the native C ABI.
- Never restore unconditional `rc_retain` / `rc_release` on actor `user_data`; use `retain_user_data` to opt in only for Kain RC allocations.
- Direct C actor lowering currently proves generic facade spawn/send behavior, not specialized per-actor handler loops.
- If adding actor stdlib behavior, update both Rust model contracts and `.kn` helper surfaces when Kain source needs the feature.
