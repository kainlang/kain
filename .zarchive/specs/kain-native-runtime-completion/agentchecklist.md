# Agent Checklist: KAIN Native Runtime Completion

## Purpose

This file exists so a future agent does not lose the intent of this spec or blindly trust a long-haul autonomous run.

This spec was written to drive a very aggressive implementation pass for finishing out the KAIN native C runtime. Any future agent reviewing work done against this spec must treat the result like junior-dev output at scale:

- assume some parts may be great
- assume some parts may be half-right
- assume some parts may be fake-complete, drifted, or poorly integrated
- verify everything against the real repo state before declaring success

## Conversation Memory

The intent behind this spec was:

- finish out the native C runtime as a real Kain runtime lane
- stay grounded in what the repo already has today
- respect current active seams in:
  - `runtime/native`
  - `crates/kain-core`
  - `crates/kain-driver`
  - `crates/kain-sys-codegen`
  - `crates/kain-ui`
  - `crates/kain-ui-native`
- treat `tasks.md` as the main execution contract for a long-haul AI run
- require aggressive verification instead of trusting “completed” checkboxes

Important framing:

- the native runtime is not empty; it already has real viewport, UI bundle, runtime contract, realtime bundle, asset, and Win32 host work
- the biggest historical gaps were actor runtime completeness, reflection payload materialization, low-level helper parity, async/runtime depth, compatibility/versioning, and broader runtime services
- low-level memory should be reviewed carefully with nuance; the compiler/ABI side is more advanced than some other runtime areas

## Mandatory Review Flow

Future agent: run this checklist in order before reporting that the autonomous implementation is done.

### 1. Re-read the spec package

- Read `requirements.md`
- Read `design.md`
- Read `tasks.md`
- Confirm that the implementation actually follows the execution order and architecture instead of inventing a new one silently

### 2. Rebuild repo context before judging results

- Re-scan:
  - `runtime/native_runtime.toml`
  - `runtime/kain_runtime.c`
  - `runtime/native/include/*`
  - `runtime/native/src/**/*`
  - `crates/kain-core/src/runtime_contract.rs`
  - `crates/kain-core/src/comptime.rs`
  - `crates/kain-core/src/low_level_memory.rs`
  - `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - `crates/kain-sys-codegen/src/codegen_cpp/mod.rs`
  - `crates/kain-driver/src/native_app.rs`
  - `crates/cli/src/main.rs`
- Check `repomap.md` files if they exist and are relevant
- Verify that file paths and ownership still match the assumptions in the spec

### 3. Audit changed files by phase

- Map every changed file to a phase in `tasks.md`
- Flag any large runtime-facing change that does not correspond to a planned phase
- Flag any planned phase marked “done” without meaningful code/test/doc changes

### 4. Validate contract and reflection work first

- Confirm runtime ABI/version metadata exists and is actually used
- Confirm service registry/capability logic is canonical and not scattered hacks
- Confirm `kain-core` emits real reflection payloads if that task was claimed complete
- Confirm `kain-driver` actually packages reflection/runtime artifacts together
- Confirm the native runtime actually consumes those artifacts and validates them

### 5. Validate low-level helper parity carefully

- Do not accept vague “helper parity improved” claims
- Compare:
  - compiler-side helper expectations
  - LLVM emission
  - native exported helpers
  - tests proving parity
- Check for missing `__kain_*` helpers, mismatched signatures, ABI drift, or silently unsupported cases

### 6. Validate actor runtime claims aggressively

- Confirm LLVM actor bootstrap no longer routes through the wrong default path if that phase was claimed complete
- Confirm there is a real actor bootstrap ABI
- Confirm actor state/mailbox/lifecycle structures exist
- Confirm supervision/monitor/registry claims with real code, not comments or placeholder structs
- Confirm actor tests actually prove runtime behavior rather than compile-only stubs

### 7. Validate async/runtime depth honestly

- Confirm a real task/future/timer runtime exists before accepting async completion claims
- Check for:
  - wake/poll behavior
  - cancellation
  - timer integration
  - scheduler integration
  - diagnostics
- Reject “uses threads and sleep” as fake async completeness

### 8. Validate UI/runtime convergence without falling for overlay inflation

- Distinguish:
  - overlay rendering
  - compiled bundle loading
  - actual component/runtime behavior
- Confirm focus/input/event/state plumbing if those tasks were claimed complete
- Check parity between raw-native and Rust-native bundle interpretation if that was part of the work

### 9. Validate graphics/material/compute claims skeptically

- Confirm artifact schemas and loaders actually exist
- Confirm resource binding is reflection-driven if claimed
- Confirm compute support is real dispatch/runtime support and not naming-only scaffolding
- Check whether the implementation is still hardwired to the old GL path under a new label

### 10. Validate hot reload, compatibility, and versioning

- Confirm compatibility metadata exists in compiler/driver outputs
- Confirm native runtime startup actually compares versions and compatibility classes
- Confirm migration/update/install/uninstall APIs are real if claimed
- Reject docs-only completion for these tasks

### 11. Validate host/plugin bridge work

- Confirm service registration is capability-aware and versioned
- Confirm plugin/module ABI checks are real
- Confirm foreign bridge contracts have marshaling/lifetime/error rules, not just TODO comments

### 12. Validate platform boundary work

- Confirm Win32 assumptions were isolated instead of merely renamed
- Confirm Linux/macOS stubs or adapters fail cleanly if that phase was touched
- Confirm platform availability is reflected in contracts/capabilities

### 13. Validate tests before trusting summaries

- Check whether tests were actually added where the spec required them
- Prefer real harnesses, golden tests, and runtime smokes over one-off assertions
- Flag any phase completed without meaningful validation coverage

### 14. Re-run the highest-value validations

- Run the most relevant `cargo test` targets for changed crates
- Run native runtime compilation checks
- Run runtime-contract/reflection golden tests
- Run actor/runtime smokes if actor code changed
- Run bundle/startup/runtime validation smokes if startup or packaging changed

### 15. Review docs for truthfulness

- Check that updated docs reflect the actual implementation, not the plan
- If docs overclaim, fix them before reporting success
- Update:
  - runtime matrices
  - roadmap docs
  - spec progress notes
  - any new runtime ABI or service docs

## Red Flags

If any of these show up, stop calling the work complete:

- phases marked complete without tests
- reflection still emitted as placeholder-only while docs claim completion
- actor bootstrap still using `default_actor_run` or equivalent fallback path
- runtime services added through scattered string checks instead of canonical tables
- C++ or LLVM parity claimed without conformance tests
- “cross-platform” claimed when only Win32 code changed
- “async runtime” claimed but implemented as thread spawn + sleep
- “UI runtime complete” claimed but only overlay rendering changed
- “hot reload/versioning complete” claimed but only docs or manifests changed
- startup validation still prints generic messages without structured diagnostics

## Final Review Output Template

When reporting on the long-haul agent’s work, answer these explicitly:

1. Which phases are truly complete?
2. Which phases are partially complete?
3. Which claims were overstated or false?
4. Which tests were added and actually passed?
5. Which critical risks remain before trusting the runtime?
6. What should be done next before more autonomous work continues?

## Non-Negotiable Rule

Do not trust checked boxes.

Trust:

- code
- tests
- emitted artifacts
- runtime behavior
- docs that match reality
