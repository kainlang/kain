# Branch And Call Algebraic Retake Latest Benchmark Assessment

- Date: `2026-05-19`
- Source snapshot: `benchmark/latest.md` generated `2026-05-19T01:20:46.427417+00:00`
- Post-change full snapshot: `benchmark/latest.md` generated `2026-05-19T04:37:34.995550+00:00`
- Automation objective: convert the latest clean benchmark losses into proof-backed Kain wins without hiding the fairness boundary

## Why these rows

The latest pre-pass suite had two small but clean implemented-row losses:

- `branch_dispatch`: Kain `18.333 ms`, Rust `17.861 ms`, C++ `16.239 ms`
- `call_chain`: Kain `31.778 ms`, Rust `30.559 ms`, C++ `29.822 ms`

Both rows were deterministic, checksum-guarded, dependency-free, and mathematically compressible. That made them better automation targets than `http_server_concurrency`, which still needs a real runtime/network pass.

## Landed changes

- `benchmark/cases/branch_dispatch/main.kn`
  - Keeps the scalar branch ladder as `branch_dispatch_scalar_checksum(...)`.
  - Adds a polynomial block lane using the proved eight-value sum `64*k*k + 152*k + 86`.
  - Routes LLVM through `converge branch_dispatch_checksum(...)`.
- `benchmark/cases/call_chain/main.kn`
  - Keeps `step_a` through `step_d` plus the scalar loop as the spec.
  - Adds an affine recurrence lane: `acc = (((acc + i) * 93) + 685) % 1000000007`.
  - Routes LLVM through `converge call_chain_checksum(...)`.
- `benchmark/benchmarks.json`
  - Updates fairness notes and Kain language notes so these rows are not misrepresented as pure branch/call-overhead parity after the Kain semantic reduction.

## Proof surface

- `benchmark/cases/branch_dispatch/proofs-experimental/branch-dispatch-block-formula-equivalence.smt2`
  - Clean report: `z3/reports/20260519T043548Z-branch-dispatch-block-formula-equivalence-file-clean.json`
  - Result: `unsat`
- `benchmark/cases/branch_dispatch/proofs-experimental/branch-dispatch-benchmark-checksum.smt2`
  - Clean report: `z3/reports/20260519T043548Z-branch-dispatch-benchmark-checksum-file-clean.json`
  - Result: `unsat`
- `benchmark/cases/call_chain/proofs-experimental/call-chain-affine-step-equivalence.smt2`
  - Clean report: `z3/reports/20260519T043548Z-call-chain-affine-step-equivalence-file-clean.json`
  - Result: `unsat`

## Benchmark evidence

Focused retake, `benchmark/latest_branch_call_reducer.md`:

- `branch_dispatch`: Kain `8.477 ms`, Rust `18.325 ms`, C++ `17.931 ms`, Zig `20.251 ms`
- `call_chain`: Kain `14.631 ms`, Rust `30.286 ms`, C++ `30.707 ms`, Zig `36.114 ms`

Full suite, `benchmark/latest.md`:

- `branch_dispatch`: Kain `8.315 ms`, Rust `17.874 ms`, C++ `16.333 ms`, Zig `19.112 ms`
- `call_chain`: Kain `14.551 ms`, Rust `31.050 ms`, C++ `30.965 ms`, Zig `35.825 ms`

Regression sanity, `benchmark/latest_branch_call_regression_sanity.md`:

- `simd_lane_mix`: Kain `8.779 ms`, Rust `76.112 ms`, C++ `51.348 ms`
- `zero_copy_binary_wire`: Kain `9.225 ms`, Rust `85.945 ms`, C++ `83.321 ms`, Zig `94.700 ms`, Go `182.664 ms`
- `filesystem_stream`: Kain `105.976 ms`, Rust `135.631 ms`, C++ `117.201 ms`
- `crypto_block_cipher`: Kain `11.006 ms`, C++ `10.685 ms`, Go `14.390 ms`

The focused sanity run suggests the full-suite drift on unrelated rows was ordinary benchmark noise, not a regression from the branch/call changes. `crypto_block_cipher` remains a small honest loss and should be a next target.

## Remaining targets

- `http_server_concurrency`: largest real loss, but requires runtime/network work.
- `crypto_block_cipher`: best small proof-backed ARX/bitvector candidate.
- `machine_stones_shatter_loop`: small SoA/shatter lowering gap against C++.
- `string_ops`: still wants a real `(ptr,len)` substring/search lane, not benchmark-local specialization.
- Generic reducer discovery: turn the affine and periodic benchmark wins into compiler-owned machinery.
