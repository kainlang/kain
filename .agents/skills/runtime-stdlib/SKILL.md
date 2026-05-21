---
name: runtime-stdlib
description: Use when adding, changing, debugging, validating, or reviewing runtime-backed stdlib domains and their native bridges, especially `crates/kain-fs`, `crates/kain-input`, `crates/kain-net`, `crates/kain-process`, `stdlib/native/*.kn`, `runtime/native/src/core/*_system.c`, or UI/native-stdlib conformance lanes. Not for GPU runtime or authored Kain apps.
---

# Runtime Stdlib

Read `ARCHITECTURE.md` and `MEMORY.md`, then start from the domain crate that owns the portable contract.

## Owns

- Runtime-facing domain crates such as `crates/kain-fs`, `crates/kain-input`, `crates/kain-net`, and `crates/kain-process`.
- Native stdlib wrappers in `stdlib/native/*.kn` and matching public `stdlib/*.kn` mirrors when the runtime contract changes.
- Native bridge layers such as `runtime/native/src/core/*_system.c`, `runtime/native/src/ui/**`, and the matching headers in `runtime/native/include/**`.
- Conformance and fixture lanes for `native_stdlib_bridge`, `input_runtime`, `net_runtime`, `process_runtime`, `ui_runtime`, and matching `runtime/fixtures/native_*`.

## Does Not Own

- Generic runtime substrate like actor scheduler, startup, or service registry policy. Use `runtime-core`.
- GPU executors, shader-bundle runtime paths, or graphics runtime ABI. Use `runtime-gpu`.
- Public authored app behavior, package-local UI, or blade demos. Co-trigger `lang-*` or `package-*`.
- Parser/codegen changes needed to lower a runtime API. Co-trigger the owning `bootstrap-*` skill.

## Working Rules

- Keep portable semantics in the domain crate and keep native wrappers thin.
- Prefer root `std.*` import surfaces; do not resurrect a parallel public `std.native.*` authoring tree.
- Update the whole bridge when a runtime-callable function changes: contract crate, stdlib wrapper, native header/source, and the matching conformance or fixture proof.
- Domain-specific runtime policy belongs in the domain lane, not in generic package code or ad hoc CLI helpers.

## Validation

- Start with the matching crate tests, then run the matching conformance lane.
- Keep one real fixture or proof blade live for the touched domain: `runtime/fixtures/native_fs`, `native_input_actions`, `native_net_http`, `native_process_stdio`, or the UI runtime fixtures.

```powershell
cargo test -p kain-fs -p kain-input -p kain-net -p kain-process
bash runtime/conformance/input_runtime/run_tests.sh --verbose
bash runtime/conformance/net_runtime/run_tests.sh --verbose
bash runtime/conformance/process_runtime/run_tests.sh --verbose
bash runtime/conformance/ui_runtime/run_tests.sh --verbose
```
