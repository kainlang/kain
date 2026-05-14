# Kain LLVM Codegen Z3 Proof Pack

This is the durable solver-backed proof workspace for `crates/kain-sys-codegen`,
with the first pass focused on `src/codegen_llvm/mod.rs`.

Current proof focus:

- ABI alignment and struct-layout arithmetic used by `align_abi_size` and
  `abi_layout_for_ty`
- match-lowering control-flow invariants around label allocation, guard-fail
  cleanup, and PHI predecessor shape
- SSA register allocation monotonicity via `next_reg`
- integer and boolean coercion semantics emitted by `coerce_to_i64_storage` and
  `cast_numeric_value`
- runtime memory bridge span preconditions shared by
  `compile_runtime_mem_load`, `compile_runtime_mem_store`, and
  `compile_tagged_payload_copy`

Common commands:

```powershell
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-sys-codegen --lane smoke
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-sys-codegen --lane layout
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-sys-codegen --lane control
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-sys-codegen --lane casts
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-sys-codegen --lane memory
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-sys-codegen --lane full
python D:\Kain-Lang\crates\kain-sys-codegen\z3\scripts\analyze_codegen_llvm_targets.py
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --workspace --project-root D:\Kain-Lang --lane smoke
```

Pack layout:

- `z3.toml` defines lanes, scope, and report history.
- `proofs/` holds curated proof obligations only.
- `scripts/` holds local helper automation for source mining.
- `generated/` is for script output and proof-derived artifacts such as the
  `codegen_llvm_target_inventory.*` files emitted by the helper script.
- `reports/` is generated proof output.

Current limitation:

- `mcp__z3_local__.analyze_source_file` can parse large chunks of
  `codegen_llvm/mod.rs`, but the full Rust parser still reports token-stream
  failures on this file. The helper script exists so future agents can keep
  mining candidate seams even when parser-based extraction is noisy.

Open seams from counterexample search:

- `double -> i1` lowering currently behaves like LLVM `fcmp one double value, 0.0`,
  which means `NaN` becomes `false` instead of matching a naive "non-zero float"
  interpretation.
- `double -> i64/i32/i8` lowering relies on `fptosi`, so future hardening should
  make the finite and in-range precondition explicit before treating those casts
  as a safety proof target.
