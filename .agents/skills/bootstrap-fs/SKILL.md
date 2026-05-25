---
name: bootstrap-fs
description: >-
  Use when changing compiler, frontend, or selfhost filesystem truth in
  `crates/kain-fs`, `crates/kain-core`, or Rust workspace tooling that
  consumes `kain-fs`: sandbox or capability policy, `fs://` resolution,
  interpreter `fs_*` globals, module resolution, deterministic workspace IO,
  or shared filesystem error and metadata shapes. Do not use for the native C
  filesystem facade or for authored `std.fs` usage.
---

# Bootstrap Fs

Use this skill when the primary work changes Kain-owned filesystem semantics on the Rust or compiler side.

## Trigger Surface

- `crates/kain-fs/**` for portable file operations, path normalization, metadata, hashing, sandbox and capability policy, virtual mounts, streaming, watchers, and transactional journals.
- `crates/kain-core/src/{module_resolution.rs,runtime.rs,types.rs,stdlib.rs}` for filesystem globals, import resolution, runtime-visible types, and target-callable registry metadata.
- Rust workspace and tooling crates that should consume `kain-fs` instead of forking filesystem behavior, especially `kain-build`, `kain-run`, `kain-check`, `kain-test`, `kain-blades`, and `kain-codebase`.

## Boundaries

- Co-trigger `runtime-stdlib` when `stdlib/native/fs.kn`, native C filesystem ABI, or `runtime/fixtures/native_fs` must change.
- Co-trigger `lang-stdlib` or `lang-authoring` when the primary task is authored Kain using `std.fs`.
- Co-trigger `tool-build-system` when Bazel sync, runtime manifests, or generated BUILD state must move with filesystem tooling changes.
- If the problem is generic parser or typechecker plumbing rather than filesystem semantics, hand it back to `bootstrap-core`.

## Workflow

1. Keep `crates/kain-fs` as the portable owner. Add shared path, capability, watch, or transaction behavior there first.
2. Update `kain-core` globals, types, and module resolution deliberately. Strict helpers should fail loudly; `fs_try_*` helpers should stay structured.
3. Route selfhost and workspace IO through `kain-fs` instead of ad hoc `std::fs` calls.
4. When native parity is required, co-trigger `runtime-stdlib` instead of letting this skill absorb C runtime policy.

## Validation Loop

```powershell
cargo test -p kain-fs --target-dir target\codex-bootstrap-fs
cargo test -p kain-core filesystem --target-dir target\codex-bootstrap-fs-core
cargo check -p kain-build -p kain-run -p kain-check -p kain-test -p kain-blades -p kain-codebase --target-dir target\codex-bootstrap-fs-consumers
```

If module resolution or import guard math changed, also run:

```powershell
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path crates\kain-core --lane full
```

## Guardrails

- Do not make `runtime/native` the owner of portable filesystem semantics.
- Do not fork path, hash, canonicalization, or deterministic directory-entry behavior across Rust tooling crates.
- Keep workspace IO explicit and inspectable; if the real job is build plumbing, route it through `tool-build-system`.
