# Kain Core Z3 Proof Pack

This pack holds durable solver-backed proofs for `crates/kain-core`.

Current focus:

- low-level memory layout arithmetic in `src/low_level_memory.rs`
- signed literal bounds for lowered memory helper integers
- span-clamping and line-end arithmetic in `src/diagnostics.rs`
- parser slice/index guards in `src/parser.rs`

Common commands:

```powershell
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-core --lane smoke
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-core --lane memory
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-core --lane diagnostics
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-core --lane literals
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-core --lane parser
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-core --lane full
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --workspace --project-root D:\Kain-Lang --lane smoke
```

Pack layout:

- `z3.toml` defines lanes and report settings.
- `proofs/` holds curated proof cases only.
- `reports/` is generated output and should stay out of commits unless intentionally needed.
- `generated/` is reserved for proof-derived fixtures or counterexample artifacts.
