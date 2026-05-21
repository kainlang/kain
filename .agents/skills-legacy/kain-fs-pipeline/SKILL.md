---
name: kain-fs-pipeline
description: Use when adding, changing, debugging, validating, or reviewing Kain's filesystem pipeline, including crates/kain-fs, fs_* runtime globals, sandbox/capability and fs:// virtual roots, streaming/ranged IO, watchers, transactions/journals, stdlib/native/fs.kn, native C filesystem facade functions, LLVM/direct-C filesystem codegen behavior, filesystem fixtures, and SHA-256/hash/path/file operation parity across interpreter and native targets.
---

# Kain Filesystem Pipeline

## Source Of Truth

- `crates/kain-fs`: portable Rust filesystem semantics. Keep file operations, path helpers, metadata, directory entries, temp paths, atomic writes, copy/move/remove, SHA-256 hashing, and `FsError` values here.
- `crates/kain-fs/src/capabilities.rs`: `FsCapability`, `FsSandbox`, `FsMount`, host-path gating, and `fs://` virtual mount resolution.
- `crates/kain-fs/src/streaming.rs`: ranged text/byte reads, writes at offsets, file chunking, and streaming copies.
- `crates/kain-fs/src/watch.rs`: portable polling watchers and watch event snapshots.
- `crates/kain-fs/src/transaction.rs`: process-local transaction plans, journals, commit rollback, and rollback-only cleanup.
- `crates/kain-core/src/runtime.rs`: interpreter-facing `fs_*` globals. Strict helpers should raise runtime errors; `fs_try_*` helpers should return structured `Result` values. FS v2 process-local `FsSandbox`, watcher handles, and transaction handles live here.
- `crates/kain-core/src/types.rs`: filesystem type/global registration for `FsError`, `FsMetadata`, `FsDirEntry`, `FsChunk`, `FsWatchEvent`, `FsJournalEntry`, and runtime-visible fs functions.
- `crates/kain-core/src/stdlib.rs`: target/global function registry. Precise return types matter for LLVM call lowering; do not leave native-callable string/int/bool/unit functions as `Any`.
- `stdlib/native/fs.kn`: Kain wrappers over the native C facade for LLVM and direct C targets.
- `runtime/native/include/kain_runtime_native_stdlib.h`: public C ABI declarations for native filesystem helpers.
- `runtime/native/src/core/kain_runtime_native_stdlib.c`: native filesystem implementation, encoded parity helpers, and last-status helpers.
- `runtime/conformance/native_stdlib_bridge/test_native_stdlib_bridge.c`: direct C conformance proof for the facade.
- `runtime/fixtures/native_fs/main.kn`: generated LLVM/direct-C fixture for real Kain filesystem calls.
- `crates/kain-sys-codegen/src/codegen_c.rs`: direct C lowering details. It must lower string equality checks through `strcmp` for stdlib helper logic.
- `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`: LLVM stdlib wrapper signatures and native function declaration behavior.
- `crates/kain-build`, `crates/kain-blades`, `crates/kain-check`, `crates/kain-test`, `crates/kain-host`, `crates/kain-omni`, `crates/kain-c-ffi`, `crates/kain-crate-ffi`, `crates/kain-import`, and `crates/kain-codebase`: core build/workspace/import/FFI lanes that should consume `kain-fs` for source discovery, artifact/report writes, hashes, canonicalization, copy/move/remove, and deterministic directory entries.

## Operating Rules

- Keep `kain-fs` as the portable semantic owner. Do not fork behavior separately inside `kain-core`, `stdlib/native`, or codegen unless the host ABI requires a wrapper.
- Route new interpreter helpers through `FsSandbox` when they can touch a path. Older v1 helpers may still accept host paths directly; migrate them deliberately rather than assuming `fs://` support is universal.
- Keep `fs_hash_file` SHA-256 in both Rust and C lanes. If a faster hash is added, give it a different API name.
- Update all layers when adding a native-callable fs helper: `kain-fs`, `kain-core` runtime/types/stdlib registry, `stdlib/native/fs.kn`, native C header/source, conformance test, native fixture, and codegen tests if ABI/signature behavior changes.
- Treat native byte arrays and rich records as ABI-sensitive. Current native parity uses lowercase hex for bytes, key-value metadata text, and newline-delimited path lists until the C ABI has better typed arrays/records/results.
- Treat target stdlib wrappers as real Kain functions. LLVM must not emit duplicate `declare` lines for functions defined by loaded target stdlib source.
- Keep result/status plumbing explicit. The C facade exposes last status/kind/message so Kain wrappers can fail loudly instead of silently returning null-ish values.
- Preserve deterministic behavior where feasible: sort directory walks/entries, return stable metadata shapes, and keep temp path helpers predictable enough for tests while still using safe unique names.
- Build and workspace crates should not wrap `std::fs` directly unless they are inside `crates/kain-fs` or need a low-level OS file handle. Use atomic writes for complete artifact/report replacement, append helpers for event streams, `hash_file` for fingerprints, and `read_dir_entries` for stable scans.
- `canonicalize_path` returns a normalized string. Convert with `PathBuf::from(...)` in Rust callers that need path operations after canonicalization.
- Watchers are portable polling watchers today, not OS-backed notification subscriptions. Transactions are best-effort process-local rollback journals, not durable crash-safe commits.

## Validation Commands

Run the narrow crate tests first:

```powershell
cargo test -p kain-fs --target-dir target\codex-kain-fs-v2
cargo test -p kain-core filesystem --target-dir target\codex-kain-fs-v2-core
cargo test -p kain-sys-codegen --test c_codegen_test --target-dir target\codex-kain-fs-v2-codegen-c -- --nocapture
cargo test -p kain-sys-codegen --test llvm_codegen_test --target-dir target\codex-kain-fs-v2-codegen-llvm -- --nocapture
```

Build a fresh CLI before native fixture checks:

```powershell
$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'
cargo build -p cli --target-dir target\codex-kain-fs-v2-cli
```

Prove the native C facade directly:

```powershell
toolchain\llvm\bin\clang.exe runtime\conformance\native_stdlib_bridge\test_native_stdlib_bridge.c runtime\native\src\core\kain_runtime_core.c runtime\native\src\core\kain_runtime_version.c runtime\native\src\core\kain_runtime_diagnostics.c runtime\native\src\core\kain_runtime_actor.c runtime\native\src\core\kain_runtime_entangle.c runtime\native\src\core\kain_runtime_native_stdlib.c -Iruntime\native\include -o target\codex-kain-fs-v2-native\native_stdlib_bridge.exe -lws2_32 -luser32 -lgdi32 -lopengl32
target\codex-kain-fs-v2-native\native_stdlib_bridge.exe
```

Prove Kain source through direct C and LLVM:

```powershell
target\codex-kain-fs-v2-cli\debug\kain.exe check runtime\fixtures\native_fs\main.kn --target c
target\codex-kain-fs-v2-cli\debug\kain.exe build runtime\fixtures\native_fs\main.kn -t c -o target\codex-kain-fs-v2-native\native_fs_c.c
target\codex-kain-fs-v2-native\native_fs_c.exe
target\codex-kain-fs-v2-cli\debug\kain.exe build runtime\fixtures\native_fs\main.kn -t llvm -o target\codex-kain-fs-v2-native\native_fs.ll
target\codex-kain-fs-v2-native\native_fs.exe
```

When touching Blade/Fabric/FFI/import workspace IO, also prove the orchestration lane:

```powershell
$env:PYO3_USE_ABI3_FORWARD_COMPATIBILITY='1'
cargo check -p kain-codebase -p kain-import -p kain-c-ffi -p kain-crate-ffi -p kain-build -p kain-blades -p kain-check -p kain-test -p kain-omni -p kain-host --target-dir target\codex-fs-unified
cargo test -p kain-blades -p kain-build -p kain-check -p kain-test --target-dir target\codex-fs-unified -- --nocapture
cargo test -p kain-codebase --target-dir target\codex-fs-unified -- --nocapture
cargo test -p kain-crate-ffi --target-dir target\codex-fs-unified -- --nocapture
cargo test -p kain-c-ffi --target-dir target\codex-fs-unified -- --nocapture
cargo test -p kain-omni validate_default_polyglot_template_succeeds --target-dir target\codex-fs-unified -- --nocapture
cargo test -p kain-host python_harness_supports_mixed_multi_output_steps --target-dir target\codex-fs-unified -- --nocapture
cargo build -p cli --target-dir target\codex-fs-unified
$env:KAIN_BIN=(Resolve-Path target\codex-fs-unified\debug\kain.exe).Path
$env:BLADE_BIN=(Resolve-Path target\codex-fs-unified\debug\blade.exe).Path
python labs\blades_workspace_smoke\scripts\run_blades_smoke.py --clean-cache
```

## Common Failure Modes

- Wrong LLVM ABI for `fs_read_text` or other wrappers usually means `crates/kain-core/src/stdlib.rs` has an imprecise return type or LLVM ignored explicit AST signatures.
- Duplicate LLVM declaration/definition errors usually mean codegen declared a target stdlib function that `stdlib/native/*.kn` also defines.
- Native hash mismatches usually mean the C facade drifted from `kain-fs::hash_file`; both should return lowercase SHA-256.
- Virtual path failures usually mean the helper is still on the older host-path-only v1 runtime path. Check whether it calls the scoped resolver in `crates/kain-core/src/runtime.rs`.
- Native bytes/metadata/listing helpers intentionally return encoded strings. Do not parse those as final language design; they are compatibility wrappers until typed native records/results mature.
- Direct C fixture assertions should avoid generic `len(string)` until the C backend lowers that form everywhere needed by native fixtures.
- C backend syntax errors around string comparisons usually mean string `==` or `!=` was emitted as pointer comparison instead of `strcmp(...) == 0` / `!= 0`.
- Passing interpreter tests alone is not enough. Filesystem behavior must prove interpreter crate tests, native C facade conformance, direct C generated executable, and LLVM generated executable.
- Full `cargo test -p kain-import` currently has unrelated transformer failures in some checkouts. For FS work, prove `kain-codebase`, C/Rust FFI, Blade/build/check/test, Omni/Fabric targeted tests, and the Blade smoke; investigate import transformer failures separately before blaming `kain-fs`.
