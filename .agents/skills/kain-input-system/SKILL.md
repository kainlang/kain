---
name: kain-input-system
description: Use when adding, debugging, validating, or reviewing Kain's canonical input semantics, including `crates/kain-input`, `stdlib/input.kn`, `stdlib/native/input.kn`, `runtime/native/include/kain_native_input_system.h`, `runtime/native/src/core/kain_native_input_system.c`, input runtime conformance, native input fixtures, semantic action/axis binding, replay traces, or first-class agent/LLM input.
---

# Kain Input System

## Ownership

- `crates/kain-input` owns portable input semantics: source provenance, typed events, data-driven action/axis bindings, frame reduction, text commits, `agent.intent`, traces, and replay.
- `crates/kain-core/src/runtime.rs` owns interpreter bridge builtins under `kain_input_*`.
- `stdlib/input.kn` is the portable Kain-facing API for interpreted/root stdlib flows.
- `stdlib/native/input.kn` wraps the native ABI for LLVM/direct-C targets.
- `runtime/native/include/kain_native_input_system.h` and `runtime/native/src/core/kain_native_input_system.c` own the raw native C input kernel.
- Adapters such as Win32, web DOM, UE5 Enhanced Input, CLI stdin, UI runtime, and agent tooling should translate host events into this model. Do not let adapters define app-facing input policy.

## Design Rules

- Keep input as a stdlib/runtime capability, not parser syntax. Do not add an `input` keyword.
- Prefer frames/actions/axes/text commits for app code. Raw events are for inspection, debugging, replay, and adapter tests.
- Keep source kinds as stable strings: `human.keyboard`, `human.pointer`, `cli.stdin`, `ui.runtime`, `agent.intent`, `test.synthetic`, and `native.platform`.
- Treat `agent.intent` as first-class source provenance. It should be replayable and bindable like human/device input, not hidden behind test-only synthetic events.
- Keep binding maps data-driven. Avoid hardcoded app commands or target-specific keymaps in runtime code.
- Native stdlib wrappers must stay in the primitive ABI subset that LLVM and direct C both lower today: `Int`, `Float`, `String`, and status codes.

## Common Workflow

1. Start with `ARCHITECTURE.md` and `MEMORY.md`, then inspect `crates/kain-input/src/lib.rs`.
2. For semantic changes, update `crates/kain-input` first and add crate tests before touching interpreter or C.
3. Mirror public behavior through `stdlib/input.kn` and `stdlib/native/input.kn`.
4. If native behavior changes, update the C header/source together and keep `runtime/native_core_runtime.toml`, `runtime/native_runtime.toml`, and service metadata in sync.
5. Update `runtime/fixtures/native_input_actions/main.kn` when the Kain-facing contract changes.
6. Keep `ARCHITECTURE.md` structural and `MEMORY.md` durable when the pipeline changes materially.

## Validation

Use the smallest relevant loop first:

```powershell
cargo test -p kain-input --target-dir target\codex-kain-input
cargo check -p kain-core --target-dir target\codex-kain-input-core
cargo test -p kain-sys-codegen native_input --target-dir target\codex-kain-input-codegen -- --nocapture
bash runtime/conformance/input_runtime/run_tests.sh --verbose
```

For full native proof:

```powershell
$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'; cargo build -p cli --target-dir target\codex-kain-input-cli
target\codex-kain-input-cli\debug\kain.exe build runtime\fixtures\native_input_actions\main.kn -t llvm
runtime\fixtures\native_input_actions\main.exe
target\codex-kain-input-cli\debug\kain.exe build runtime\fixtures\native_input_actions\main.kn -t c
runtime\fixtures\native_input_actions\main.exe
```

Delete generated fixture artifacts such as `.ll`, `.c`, `.exe`, and emitted JSON after local validation unless the user explicitly asks to keep them.
