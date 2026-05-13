# Kain GPU SPIR-V Z3 Proof Pack

This pack holds durable solver-backed proofs for `crates/gpu`, focused on the live
`codegen_spirv.rs` backend.

Current focus:

- Vulkan/std430 layout invariants for storage-buffer and uniform wrappers
- vector-constructor flattening/index safety
- control-flow safety around hoisted locals and compute local-size slot mapping

Common commands:

```powershell
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\gpu --lane smoke
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\gpu --lane layout
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\gpu --lane constructors
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\gpu --lane control
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\gpu --lane full
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --workspace --project-root D:\Kain-Lang --lane smoke
```

Pack layout:

- `z3.toml` defines lanes and report settings.
- `proofs/` holds curated proof cases only.
- `reports/` is generated output and should stay out of commits unless intentionally needed.
- `generated/` is reserved for proof-derived fixtures or counterexample artifacts.
