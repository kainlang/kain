---
name: kain-amalgamate-capsules
description: Use when adding, changing, debugging, validating, or reviewing Kain's portable capsule lane, including `kain amalgamate`, the comment-safe `.kn` capsule format, pack/inspect/unpack/materialize logic in `crates/kain-amalgamate`, transparent capsule handling in `crates/cli`, preview/header generation, digest-scoped `.kain/cache/amalgamate` extraction, and the probe blade under `blades/amalgamate-capsule-probe`.
---

# Kain Amalgamate Capsules

## Files

- `crates/kain-amalgamate/src/lib.rs`: core format, metadata parse, pack, inspect, unpack, and materialize logic.
- `crates/cli/src/amalgamate.rs`: operator surface and text/JSON inspect rendering.
- `crates/cli/src/run.rs` and `crates/cli/src/main.rs`: transparent `run` / `build` / `check` capsule detection and delegation.
- `crates/kain-commands/src/kain.rs` and `crates/kain-commands/commands/amalgamate.toml`: typed CLI and registry metadata.
- `blades/amalgamate-capsule-probe/**`: dogfood blade for whole-workspace preservation.
- `docs/cli/*.md`, `docs/reference/command-matrix.md`, `ARCHITECTURE.md`, and `MEMORY.md`: operator and durable docs.

## Format Contract

- Keep capsules text-first and comment-safe. The live lane has two storage forms:
  - default editable capsules:
    - optional generated header comments
    - `//!kain-capsule` metadata block
    - one `//!kain-file` block per preserved file
    - UTF-8 text inline, binary payloads base64-wrapped per file
  - `--archive` capsules:
    - same generated header and metadata block
    - one compressed `//!kain-capsule-payload` base64 blob
- Treat the structured metadata block as source of truth. The human header is generated preview only.
- Preserve foreign/native files verbatim. Do not force C/C++/Rust/TS assets through `kain-import` for capsule correctness.
- Preserve relative paths exactly inside the archive and reject path-escape payloads during unpack/materialize.
- Materialize by digest under `.kain/cache/amalgamate/<digest>/workspace`.

## Operator Surface

- `kain amalgamate <path> -o artifact.kn`
- `kain amalgamate <path> -o artifact.kn --archive`
- `kain amalgamate inspect artifact.kn [--json]`
- `kain amalgamate unpack artifact.kn [-o dir]`
- `kain run`, `kain build`, and `kain check` should auto-detect capsule `.kn` inputs and materialize before normal pipeline handoff.
- File capsules should flow back into file-mode build/check behavior. Blade/workspace capsules should flow back into manifest/project behavior.

## Preview Rules

- `--header minimal|rich|off`
- `--preview-symbols <n>`
- `--api-index auto|off`
- `--module-index auto|off`
- Prefer generated sections such as `constants`, `types`, `functions`, `traits`, `actors`, `worlds`, `patches`, `laws`, `converges`, `orchestrates`, `shaders`, and `axioms`.
- Keep preview generation best-effort. `kain inspect` is the truth tool.

## Validation

```powershell
cargo test -p kain-commands --target-dir target\codex-kain-capsules
cargo check -p kain-amalgamate -p kain-commands -p cli --target-dir target\codex-kain-capsules
target\codex-kain-capsules\debug\kain.exe amalgamate blades\amalgamate-capsule-probe -o blades\amalgamate-capsule-probe\.kain\capsules\amalgamate-capsule-probe.kn --author "Taylor Kipp" --meta license=MIT --note "portable editable dogfood" --preview-symbols 8
target\codex-kain-capsules\debug\kain.exe amalgamate D:\Kain-Lang\blades\amalgamate-capsule-probe -o D:\Kain-Lang\target\capsule-archive-abs.kn --archive --preview-symbols 6
target\codex-kain-capsules\debug\kain.exe amalgamate inspect blades\amalgamate-capsule-probe\.kain\capsules\amalgamate-capsule-probe.kn
target\codex-kain-capsules\debug\kain.exe amalgamate unpack blades\amalgamate-capsule-probe\.kain\capsules\amalgamate-capsule-probe.kn -o blades\amalgamate-capsule-probe\.kain\unpacked
target\codex-kain-capsules\debug\kain.exe check blades\amalgamate-capsule-probe\.kain\capsules\amalgamate-capsule-probe.kn --target llvm
target\codex-kain-capsules\debug\kain.exe run blades\amalgamate-capsule-probe\.kain\capsules\amalgamate-capsule-probe.kn
target\codex-kain-capsules\debug\kain.exe build blades\amalgamate-capsule-probe\.kain\capsules\amalgamate-capsule-probe.kn
python tools/bazel/sync_rust_builds.py
python tools/bazel/sync_rust_builds.py --check
```

- Use the probe blade before touching larger workspaces such as `blades/network-domains` or `blades/pong`.

## Gotchas

- `kain-run` currently accepts manifest `[run].target` values `auto|kain|c|cargo|fabric|node|bun`; the probe blade uses `target = "kain"` for `run` validation even when `check` and `build` smoke the LLVM lane separately.
- Root-level `kain check <blade-root>` can still pick up `.kn` files under local `.kain` folders. Remove or relocate unpacked capsule copies when you want a clean tree-wide check.
- Manifest/workspace capsules materialize to a project root. `build -o` is meaningful for capsule packing, not as an override for project-capsule builds.
- Keep cache reuse keyed by digest plus metadata validation; do not trust an existing extracted tree blindly.
- Editable capsules intentionally refresh digest/file inventory from their inline file blocks on read. Do not make editable validation strict enough that a hand-edited capsule becomes unrunnable.

## Proof Hook

- Re-run the capsule path/bounds proof when changing path normalization, payload offsets, or unpack math.
- Current proof artifacts:
  - `z3/reports/20260517T081310Z-amalgamate-capsule-layout-three-file-nonoverlap.json`
  - `z3/reports/20260517T093524Z-amalgamate-path-depth-nonnegative.json`
