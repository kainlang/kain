# Benchmark Frontier Speedup Hunt

- Date: 2026-05-19
- Status: active
- Repo Root: `D:\Kain-Lang`
- Session Slug: `benchmark-frontier-speedup-hunt`

## Research Question

Which latest benchmark frontier yields the biggest honest Kain win next?

## Constraints

- Target honest wins on the latest benchmark suite, not proxy theater.
- Prefer compiler/runtime changes that generalize beyond one benchmark source file.
- Keep the row fair against Rust/C++ rather than shrinking the benchmark contract.
- Full-suite validation is required before calling the pass landed.

## Hypothesis Lattice

### Baseline
- Mechanism: keep attacking `http_server_concurrency` with smaller request/response and scheduler overhead.
- Expected upside: mid-single-digit milliseconds on the remaining runtime-owned honest frontier.
- Likely blocker: the row is dominated by short-lived loopback socket lifecycle and scheduler behavior, not easy arithmetic.
- Proof obligation: request frame, path, body length, and checksum must stay exact.

### Unconventional
- Mechanism: remove runtime shatter allocation/free from fixed local `shatter struct` literals by lowering closed local field-projection loops to stack-backed SoA lane buffers.
- Expected upside: flip `machine_stones_shatter_loop` from a C++ loss into a Kain win.
- Likely blocker: the fast lane is only legal when the shattered array never escapes into broader array/value semantics.
- Proof obligation: 8-byte slot addressing must stay within the per-lane stack buffer for every valid element index.

### Moonshot
- Mechanism: `alloc_churn` is not obviously slow code anymore; the generated LLVM already uses a local stack cell. Treat it as a runtime/startup jitter investigation and hunt the 10-11 ms fast mode versus 60-67 ms slow mode split.
- Expected upside: if the bimodal overhead is removed, `alloc_churn` could collapse by 5x without touching the hot loop body.
- Likely blocker: the loss may live outside the loop in executable startup, host/runtime init, or noisy Windows process behavior.
- Proof obligation: show the slow mode comes from an external/runtime seam rather than the scalar loop IR.

## Mathematical Model

- Variables: `element_count`, `index`, `access_width`, lane slot byte offset `8 * index`.
- Invariants: `0 <= index < element_count`, `1 <= access_width <= 8`.
- Objective: prove stack-backed shatter lane slot accesses stay within `[element_count x i64]` storage while removing runtime shatter allocation/free overhead.
- Bad states: a closed-lane shattered local escapes into bare array/value use, or an addressed slot overruns the lane buffer.
- Simplifying assumptions: each shatter lane keeps the existing runtime ABI's fixed 8-byte slot stride.

## Z3 Claims

1. `crates/kain-sys-codegen/z3/proofs-experimental/shatter-stack-slot-span.smt2` asks whether any valid closed-lane slot access with width <= 8 can overrun the stack lane span. Result: `unsat`.
2. The solver result is enough for the landed transform because separate lanes use separate allocas; the only new arithmetic claim is per-lane slot span safety.

## Evidence And Sources

- Local:
  - `benchmark/latest.md` and `benchmark/out/reports/latest.llm.md` before the pass showed `machine_stones_shatter_loop` as the worst honest implemented ratio loss and `http_server_concurrency` as the biggest absolute delta.
  - `benchmark/cases/machine_stones_shatter_loop/main.kn`
  - `crates/kain-sys-codegen/src/codegen_llvm/mod.rs`
  - `crates/kain-sys-codegen/tests/llvm_codegen_test.rs`
  - `benchmark/out/build/machine_stones_shatter_loop/kain/machine_stones_shatter_loop.ll` before the pass showed `kain_machine_shatter_alloc` / `kain_machine_shatter_lane_base` still present for a tiny fixed local literal.
  - Focused retake after the pass: `benchmark/latest.md` generated `2026-05-20T02:16:21.084251+00:00` with `machine_stones_shatter_loop` Kain `12.797 ms`, Rust `13.332 ms`, C++ `13.232 ms`.
  - Full-suite refresh after the pass: `benchmark/latest.md` generated `2026-05-20T02:19:06.967886+00:00`, status `PASS`.
- External:
  - None.

## Dead Ends

- Another immediate HTTP concurrency rewrite would have been premature. The benchmark triage showed a stronger compiler-owned win first.
- `alloc_churn` is not an obvious heap-allocation codegen miss anymore. The lowered LLVM already uses a stack cell, so a naive "make alloc faster" plan would be chasing the wrong abstraction.

## Conclusion

Landed the unconventional lane. Closed local `shatter struct` array literals now lower to stack-backed SoA lane buffers in LLVM when the remaining block stays in len/field-projection form. The proof artifact is `crates/kain-sys-codegen/z3/proofs-experimental/shatter-stack-slot-span.smt2`, checked `unsat`.

The focused benchmark flipped `machine_stones_shatter_loop` from the previous Kain loss (`19.082 ms` in the prior canonical snapshot) to a clean Kain win at `12.797 ms` versus Rust `13.332 ms` and C++ `13.232 ms`. The canonical full suite stayed green and now reports `machine_stones_shatter_loop` as an effective tie (`13.765 ms` Kain vs `13.742 ms` C++) with one large Kain outlier sample, so the speedup is real but the row is now noise-sensitive instead of structurally behind.

Fresh frontier ranking after the pass:

1. `alloc_churn`: biggest honest implemented loss, but the emitted IR is already stack-local and the samples are bimodal (`10.812/11.287 ms` fast mode versus `61-67 ms` slow mode). Treat this as runtime/startup jitter forensics.
2. `http_server_concurrency`: still the largest absolute honest gap (`58.143 ms` Kain vs `40.170 ms` Rust) and remains the clearest runtime-owned frontier.
3. `struct_method`: `23.167 ms` Kain vs `11.992 ms` C++; another noisy row with large Kain outliers.
4. `sim_nbody_gravity`: `14.460 ms` Kain vs `9.013 ms` C++; likely a real float-loop/codegen/vectorization frontier.

Best next experiment: instrument or isolate the `alloc_churn` slow mode first, because the hot loop already looks collapsed and the 5x loss may be hiding outside the loop body.
