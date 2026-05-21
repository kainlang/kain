---
name: kain-core-z3-proofs
description: Use when adding, changing, debugging, validating, or reviewing `crates/kain-core` arithmetic, bounds, indexing, or proof-pack logic, especially `crates/kain-core/src/low_level_memory.rs`, `src/diagnostics.rs`, `src/parser.rs`, or the durable proof pack at `crates/kain-core/z3`.
---

# Kain Core Z3 Proofs

Use this skill when work touches `crates/kain-core` seams that are easy to get "green by accident": layout arithmetic, signed literal conversions, span math, parser slice/index guards, or the proof pack itself.

## Quick Workflow

1. Read `crates/kain-core/z3/z3.toml` and the nearby `README.md` before editing proofs.
2. If the change is in `src/low_level_memory.rs`, prefer checked arithmetic and explicit diagnostics over raw `+`, `*`, or narrowing casts.
3. Use Z3 before trusting tests. Start with `find_counterexample` on suspected unsafe math, then convert the surviving invariant into a durable proof case in `crates/kain-core/z3/proofs/`.
4. Keep proof filenames lane-prefixed:
   - `memory-*` for layout arithmetic
   - `diagnostics-*` for span, line, or rendering bounds
   - `literal-*` for signed literal conversion domains
   - `parser-*` for slice/index preconditions
5. Re-run the focused lane first, then `full`, then workspace `smoke`.

## Preferred Proof Shapes

- Use `size_add_ok` for guarded `a + b`, `a + b + 1`, or `start + relative_end` math.
- Use `find_counterexample` when you want the solver to search for overflow or wrap witnesses against a natural-language claim.
- Use `range_check` for `len() - 1`, clamped offsets, and signed-domain assertions like `usize -> i64`.
- Use `ptr_offset_ok` only when the claim is genuinely about offset/stride pointer math.

## Code Conventions

- `DiagnosticCode::MemoryLayoutOverflow` is the canonical failure path for layout math that no longer fits target `usize`.
- Keep helper naming explicit: `checked_layout_add`, `checked_layout_mul`, `checked_align_up`, `size_literal_i64`.
- Prefer surfacing invalid layouts as `KainResult` errors in build-time lowering code. Only use panic wrappers in paths that still require infallible signatures and already assume validated inputs.

## Validation Commands

```powershell
cargo check -p kain-core
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-core --lane memory
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-core --lane diagnostics
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-core --lane literals
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-core --lane parser
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --pack-path D:\Kain-Lang\crates\kain-core --lane full
uv run --project C:\Dev\polytools\z3-mcp --no-sync z3-mcp-batch --workspace --project-root D:\Kain-Lang --lane smoke
```

## Gotchas

- If a proof only fails on values outside the ABI domain, fix the model before fixing code. `usize` proofs should stay within `18446744073709551615`, and `size_literal_i64` success-path proofs should stay within `9223372036854775807`.
- Do not commit `crates/kain-core/z3/reports/` or root `z3/reports/`; commit curated proof cases and manifest changes only.
- Keep `source.path`, `source.symbol`, `start_line`, and `reason` populated in every proof so future agents can trace the invariant quickly.
