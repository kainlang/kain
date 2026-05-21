---
name: kain-spirv-codegen-validation
description: Use when adding, changing, debugging, validating, or reviewing Kain's SPIR-V backend in `crates/gpu/src/codegen_spirv.rs`, especially storage-buffer layout math, vector constructor lowering, hoisted local-slot plumbing, or the durable proof pack at `crates/gpu/z3`.
---

# Kain SPIR-V Codegen Validation

Use this skill when work touches the live GPU backend in `crates/gpu`. The goal is not just "did it compile" but "did we prove the backend math and emit a module that Vulkan tools accept."

## Quick Workflow

1. Read `ARCHITECTURE.md`, `MEMORY.md`, `crates/gpu/z3/z3.toml`, and `crates/gpu/z3/README.md`.
2. Treat `crates/gpu/src/codegen_spirv.rs` as the backend truth and `crates/gpu/tests/spirv_layout.rs` as the external validator smoke.
3. Before changing arithmetic or indexing logic, ask Z3 for a counterexample or encode the intended invariant directly in `crates/gpu/z3/proofs/`.
4. Keep proof filenames lane-prefixed:
   - `layout-*` for std430/base-alignment, wrapper offsets, access-chain member bounds
   - `constructors-*` for vector flattening and component extraction bounds
   - `control-*` for local-size slot mapping and hoisted-local control flow/index safety
5. Re-run the focused lane first, then `full`, then workspace `smoke`.

## Current Proof Scope

- Storage-buffer and uniform wrapper layout invariants
- Vulkan/std430 stride expectations for scalar, vec2, vec3, vec4, and mat4 wrappers
- `OpCompositeExtract` index safety for vector constructor flattening
- Hoisted-local slot removal safety after `position(...)`
- Compute local-size slot mapping staying inside the X/Y/Z domain

## Validation Commands

```powershell
cargo test -p gpu --lib storage_buffer_stride_matches_vulkan_base_alignment_for_common_types --target-dir target\codex-spirv-proof-lib -- --nocapture
cargo test -p gpu --test spirv_layout --target-dir target\codex-spirv-proof-layout -- --nocapture
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\gpu --lane layout
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\gpu --lane constructors
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\gpu --lane control
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\gpu --lane full
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --workspace --project-root D:\Kain-Lang --lane smoke
```

## Gotchas

- Vulkan std430/base alignment keeps 3-lane vectors at 16-byte alignment even though the payload is 12 bytes. Do not "optimize" `Vec3`/`IVec3`/`UVec3` storage-buffer stride back to 12.
- A green Z3 pack is necessary but not sufficient. Pair proof-pack runs with `spirv-val --target-env vulkan1.3` through `crates/gpu/tests/spirv_layout.rs`.
- The broader `spirv_smoke.rs` and `spirv_execute.rs` suites currently include pre-existing frontend/typechecker gaps. Distinguish backend proof failures from authoring/frontend failures before changing the backend.
- Do not commit `crates/gpu/z3/reports/`; keep commits focused on curated proofs, manifest updates, and code/test changes.
