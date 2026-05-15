---
name: kain-foreign-abi-ffi
description: Use when changing, debugging, validating, or reviewing Kain's shared foreign ABI model or C ABI FFI bridge, especially crates/kain-foreign-abi, crates/kain-c-ffi, C header shape mining, callback/function-pointer imports, raw scalar pointers, multi-level pointers, byte-buffer pointer returns, by-value aggregate gates, or foreign pointer ownership policy.
---

# Kain Foreign ABI / C FFI

Use this skill for work on `crates/kain-foreign-abi`, `crates/kain-c-ffi`, and `tools/foreign_abi`.

## Ownership Boundaries

- `crates/kain-foreign-abi` owns the shared type graph, normalized scalar tables, pointer/callback/raw-pointer bridge classes, safety reports, and Z3 proofs for ABI arithmetic.
- `crates/kain-c-ffi` owns C-header discovery, `[c_ffi]` manifest resolution, Kain extern generation, binding reports/manifests, bridge crate source generation, and live/package bridge loading.
- `kain-c-ffi` should consume `kain-foreign-abi` policy instead of adding local scalar/pointer match ladders.
- Raw imported pointers are external-ownership values. Do not wire them into `collapse`/`observe`/`decay` semantics without an explicit foreign ownership contract.
- By-value aggregates must stay gated until parsed layout plus target ABI passing rules exist. Do not fake by-value struct calls as `void*`.

## Workflow

1. Read `ARCHITECTURE.md`, `MEMORY.md`, and the relevant source files:
   - `crates/kain-foreign-abi/src/lib.rs`
   - `crates/kain-c-ffi/src/extract.rs`
   - `crates/kain-c-ffi/src/model.rs`
   - `crates/kain-c-ffi/src/generate.rs`
2. Mine real header pressure before broadening policy:
   - `python tools/foreign_abi/mine_c_abi_shapes.py <header-or-dir> --out target/codex-foreign-abi/ffi_shape_report.json`
3. Prefer changes in this order:
   - model/classification in `kain-foreign-abi`
   - extractor/type-registry use in `kain-c-ffi`
   - bridge type rendering in `model.rs`
   - generated bridge runtime code in `generate.rs`
   - focused C FFI regression tests in `crates/kain-c-ffi/src/lib.rs`
4. Add or update Z3 proof cases for pointer-depth, bounds, layout, or ownership math under `crates/kain-foreign-abi/z3`.
5. If Cargo manifests changed, run `python tools/bazel/sync_rust_builds.py` and `python tools/bazel/sync_rust_builds.py --check`.

## Validation

- `cargo test -p kain-foreign-abi --target-dir target\codex-foreign-abi -- --nocapture`
- `cargo test -p kain-c-ffi --target-dir target\codex-foreign-abi -- --test-threads=1 --nocapture`
- `mcp__z3_local__.run_proof_pack(path="D:/Kain-Lang/crates/kain-foreign-abi/z3", report_name="foreign-abi-proof-pack-full")`
- `python tools/bazel/sync_rust_builds.py --check`
- `bazel test //crates/kain-foreign-abi:unit_test --config=dev` when Bazel coverage matters.

## Known Traps

- `cargo test -p kain-c-ffi` is process-global and can collide under default parallel execution because generated bridge loading and shared-library env vars are global. Use `--test-threads=1` until tests get isolated bridge names or registry reset hooks.
- Callback support currently covers null/passthrough callback pointers. Kain closure-to-C trampoline generation is a separate feature.
- Byte-buffer pointer returns are exposed as pointer host objects because C returns do not carry length. Do not materialize `Array<Int>` unless length metadata is explicit.
- Bazel may print the known Windows `rules_swift` local-config `name 'arch' is not defined` warning while Rust targets still pass under `--keep_going`.
