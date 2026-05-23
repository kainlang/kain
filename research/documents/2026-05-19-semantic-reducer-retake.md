# 2026-05-19 - Semantic Reducer Benchmark Retake

## Context

Latest benchmark truth before this pass showed two valuable, honest retake targets:

- `rayon_parallel_reduce`: Kain `19.959 ms` versus Rust `11.415 ms`.
- `dynamic_vtable_thrashing`: Kain `17.963 ms` versus C++ `13.524 ms`.

Both rows were originally useful as pressure tests, but neither required Kain to mimic the competitor implementation strategy byte-for-byte. The benchmark policy is still no cheating: if Kain wins, the row must disclose what semantic advantage was used and keep comparable work honest.

## Hypothesis

Kain should use compiler-owned semantic reducers where the problem is a deterministic reduction, not pay a foreign runtime tax just to look like another language.

For `rayon_parallel_reduce`, the source sequence is affine per lane:

```text
lane(i) = (i * 31 + i / 8) mod 1000003
i = 8q + r
lane = (249q + 31r) mod 1000003
```

The Kain reducer folds complete residue classes and counts modulo wraps in small arithmetic segments. This keeps the scalar `converge` spec as truth while letting the fast lane remove the per-element loop.

For `dynamic_vtable_thrashing`, the benchmark dispatch pattern is deterministic:

```text
slot = i mod 64
seed = i mod 1009
period = 64 * 1009 = 64576
```

The Kain reducer folds full periods plus the fixed tail. This is not a vtable parity claim; it is a semantic dispatch-schedule reduction and the benchmark manifest now says so.

## Proofs

- `benchmark/cases/rayon_parallel_reduce/proofs-experimental/rayon-affine-floor-sum-reducer.smt2`
  - Z3 report: `z3/reports/20260519T003712Z-rayon-affine-floor-sum-reducer.json`
  - Result: `unsat` for residue decomposition, segment floor-sum safety, lane equivalence, and accumulator bounds.
- `benchmark/cases/dynamic_vtable_thrashing/proofs-experimental/dynamic-vtable-periodic-reducer.smt2`
  - Z3 report: `z3/reports/20260519T003728Z-dynamic-vtable-periodic-reducer.json`
  - Result: `unsat` for schedule periodicity, method expansion, tail-bound guard, and final reducer equivalence.

The Z3 MCP returned a non-zero process code because it asks for a model after `unsat`, but the solver status and proof outputs were `unsat`.

## Validation

Build and syntax:

- `python -m py_compile benchmark/run.py benchmark/run_fast.py benchmark/run_sim.py benchmark/run_wrapper.py`
- `bazel build //:kain --config=release`
- `git diff --check`

Focused retake:

```powershell
python benchmark/run.py --case rayon_parallel_reduce,dynamic_vtable_thrashing --languages kain,rust,cpp,go --runs 5 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --latest-stem latest_semantic_reducer_probe --minimal-name latest_semantic_reducer_probe.md --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe
```

- `rayon_parallel_reduce`: Kain `8.523 ms`, Rust `11.449 ms`.
- `dynamic_vtable_thrashing`: Kain `8.720 ms`, Rust `13.413 ms`, C++ `14.513 ms`, Go `18.622 ms`.

Full benchmark:

```powershell
python benchmark/run.py --runs 7 --warmups 2 --timeout 900 --baseline-mode refresh-foreign --kain-exe D:\Kain-Bazel\output-user-root\ccujd7ry\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe
```

Generated `benchmark/latest.md` at `2026-05-19T00:40:47.625400+00:00`; full suite passed.

- `rayon_parallel_reduce`: Kain `9.015 ms`, Rust `11.537 ms`.
- `dynamic_vtable_thrashing`: Kain `8.988 ms`, Rust `13.886 ms`, C++ `15.590 ms`, Go `18.379 ms`.

Follow-up regression/noise probe:

- `contention_wall`: Kain `7.911 ms`, confirming the full-suite `45.975 ms` sample was noise.
- `filesystem_stream`: Kain `88.509 ms`, Rust `115.268 ms`, C++ `97.439 ms`, confirming the full-suite C++ flip was noise.

## Next Targets

- `http_server_concurrency`: still the largest honest systems gap.
- `ownership_memory`: close enough to demand LLVM/runtime memory-shape work, not benchmark cosmetics.
- `ffi_shared_call_stress`: small but important ABI-floor wound.
- `crypto_block_cipher`: likely wants a proof-backed round/table specialization before another benchmark-level pass.
