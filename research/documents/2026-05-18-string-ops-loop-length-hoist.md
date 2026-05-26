# String Ops Loop Length Hoist

- Date: 2026-05-18
- Status: landed
- Repo Root: `D:\Kain-Lang`
- Session Slug: `string-ops-loop-length-hoist`

## Research Question

Can Kain close the remaining string_ops gap by hoisting invariant string-parameter length loads out of loop-carried runtime calls while preserving reassignment semantics?

## Constraints

- Preserve current string reassignment semantics inside lowered functions and methods.
- Only pay the entry-time `len(...)` cost when a string parameter is actually loop-carried.
- Keep the win in the compiler/backend, not by specializing the benchmark source.

## Hypothesis Lattice

### Baseline
- Mechanism: prime `string_length_values` once for string parameters whose identifiers appear in loop-bearing blocks.
- Expected upside: remove repeated loop-carried `@len(i8* ...)` scans in `string_ops`-shaped loops.
- Likely blocker: a parameter may be reassigned later, so the cache must not survive semantic rebinding.
- Proof obligation: cached entry length is only reused while the original binding is still current.

### Unconventional
- Mechanism: extend the current known-string fast path toward a first-class `(ptr,len)` lane for `starts_with_at` / `find_substring`.
- Expected upside: erase the remaining Rust gap without benchmark-only pattern hacks.
- Likely blocker: Kain string lowering still threads plain `i8*` in many places.
- Proof obligation: helper fast paths must match the scalar substring semantics for every valid index domain.

### Moonshot
- Mechanism: synthesize a packed-window substring kernel or bit-parallel probe for short fixed needles.
- Expected upside: turn `string_ops` into a compiler-owned alien-math win instead of a near-tie.
- Likely blocker: needs a durable proof story and a general lowering hook, not a benchmark-local shortcut.
- Proof obligation: exact first-match position and "not found" behavior remain identical.

## Mathematical Model

- Variables: `entry_ptr`, `reassigned`, `reassigned_ptr`, `len(ptr)`.
- Invariants: cached length is `len(entry_ptr)`; a reassigned parameter must force fresh length computation.
- Objective: prove emitted loop guards agree with the semantic `len(current_ptr)` value.
- Bad states: loop guard keeps using stale `len(entry_ptr)` after reassignment.
- Simplifying assumptions: only the binding identity matters for correctness; the backend may still choose any equivalent fresh computation after rebinding.

## Z3 Claims

1. The emitted length expression equals `len(current_ptr)` for both the unreassigned and reassigned branches.
2. There is no model where the guarded cache protocol returns a stale length after reassignment.

## Evidence And Sources

- Local:
  - `crates/sys-codegen/src/codegen_llvm/mod.rs`
  - `crates/sys-codegen/tests/llvm_codegen_test.rs`
  - `crates/sys-codegen/z3/proofs-experimental/string-param-loop-length-cache-valid-under-reassign-guard.smt2`
  - `benchmark/out/reports/latest_string_ops_len_hoist.llm.md`
  - `benchmark/out/reports/latest.llm.md`
- External:
  - None. This pass was repo-local and solver-backed.

## Dead Ends

- The first full-suite refresh produced a noisy `allocator_large_object_churn` snapshot with bimodal native-language samples.
- A focused rerun showed the allocator row was not a semantic regression, so the correct response was a clean full-suite rerun rather than chasing a false wound.

## Conclusion

The landed backend change primes loop-carried string parameter lengths once at function entry, guarded by a structural AST scan for loops that actually mention the parameter. The Z3 model in `crates/sys-codegen/z3/proofs-experimental/string-param-loop-length-cache-valid-under-reassign-guard.smt2` remains `unsat`, and the durable proof pack run stayed green.

Measured outcome:

- Focused refresh (`benchmark/latest_string_ops_len_hoist.md`): Kain `10.553 ms`, Rust `9.357 ms`, C++ `9.389 ms`.
- Canonical full-suite refresh (`benchmark/latest.md`): Kain `11.865 ms`, Rust `8.819 ms`, C++ `9.542 ms`.
- Previous full-suite snapshot (`benchmark/out/reports/20260518T094400Z.json`): Kain `13.958 ms`.

That means the full-suite `string_ops` median dropped by about `15.0%` (`13.958 -> 11.865`) without changing the benchmark case itself. The remaining worthwhile follow-up is a true `(ptr,len)` substring lane so the compiler can attack the last ~35% gap to Rust from the backend instead of from benchmark-local source tricks.
