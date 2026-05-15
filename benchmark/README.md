# Kain vs Rust LLVM Benchmarks

This folder is a paired benchmark lane for native Kain LLVM output against Rust LLVM output.

The contract is intentionally simple:

- Every Kain benchmark in `cases/<case>/main.kn` must have a Rust sibling at `cases/<case>/main.rs`.
- Case programs may import local files later, but they must not use external packages or crates.
- Build time is recorded separately; timed samples run the already-built executables.
- The runner prefers a release-built `kain.exe` and passes a benchmark-native tuning profile into the Kain compiler unless you override it.
- Every run writes `out/reports/latest.html`, a timestamped HTML report, and `out/reports/latest.json`.
- The report includes a maturity/fairness note per case. Some pressure tests are honest proxies until Kain exposes the matching runtime primitive directly in LLVM.

Current pressure cases:

- `contention_wall`: Rust 100-thread atomic contention versus Kain `collapse` exclusive ownership over the same total increment count.
- `ghost_mirror`: Rust std TCP transfer of a 1 MiB payload versus Kain entangle-backed world mirroring plus in-process payload mutation.
- `evolutionary_loop`: Rust runtime feature-detected lane choice versus Kain `converge` / `orchestrate` dispatch syntax.

Current basic language-edge cases:

- `branch_dispatch`: branch-heavy scalar dispatch.
- `call_chain`: small-function call graph in a hot loop.
- `memory_stream`: sequential helper-owned buffer write/read.
- `alloc_churn`: many small allocation/lifetime cycles.
- `scalar_mix`: hot scalar loop with top-level const expressions and a checksum guard.
- `struct_method`: aggregate construction plus explicit score function over fields.
- `option_result`: tagged Option/Result creation, branching, and unwrap.

Known Kain gaps exposed while shaping these cases:

- Scalar `match` in the standalone branch hot loop built but trapped at runtime, so `branch_dispatch` currently uses equivalent `if` dispatch.
- Method receiver field access in the struct benchmark hit a native codegen gap, so `struct_method` uses `score_pair(pair)` instead of `pair.score()`.

Run the suite from the repo root:

```powershell
python benchmark/run.py
```

Useful variants:

```powershell
python benchmark/run.py --runs 9 --warmups 2
python benchmark/run.py --case ownership_memory
python benchmark/run.py --kain-exe D:\Kain-Lang\target\release\kain.exe
```

The runner prefers a direct Bazel-built release `kain.exe` to avoid the Windows PowerShell launcher `-o` forwarding ambiguity. Use `--kain-exe` or `KAIN_EXE` to pin a specific compiler. Use `--kain-native-profile`, `--kain-native-opt-level`, `--kain-native-target-cpu`, and `--kain-native-debug-info` only if you are intentionally changing the native benchmark tuning.
