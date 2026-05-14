# Kain Ownership Z3 Proof Pack

This pack holds durable solver-backed proofs for `crates/kain-ownership`.

Current focus:

- `observe` and `collapse` exclusivity
- balanced scoped `observe` and `collapse` returning to `Idle`
- `decay` preconditions
- terminal decayed-state behavior
- conservative policy shape for world and entangle regions
- imported pointer borrowed lifetime policy without heap-free ownership

Common commands:

```powershell
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-ownership --lane smoke
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-ownership --lane state
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-ownership --lane policy
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-ownership --lane full
```

Pack layout:

- `z3.toml` defines lanes and report settings.
- `proofs/` holds curated proof cases.
- `reports/` is generated output.
- `generated/` is reserved for proof-derived fixtures or counterexample artifacts.
