# Kain vs Rust LLVM Benchmarks

This folder is a paired benchmark lane for native Kain LLVM output against Rust LLVM output.

The contract is intentionally simple:

- Every Kain benchmark in `cases/<case>/main.kn` must have a Rust sibling at `cases/<case>/main.rs`.
- Case programs may import local files later, but they must not use external packages or crates.
- Build time is recorded separately; timed samples run the already-built executables.
- Every run writes `out/reports/latest.html`, a timestamped HTML report, and `out/reports/latest.json`.
- The report includes a maturity/fairness note per case. Some pressure tests are honest proxies until Kain exposes the matching runtime primitive directly in LLVM.

Current pressure cases:

- `contention_wall`: Rust 100-thread atomic contention versus Kain `collapse` exclusive ownership over the same total increment count.
- `ghost_mirror`: Rust std TCP transfer of a 1 MiB payload versus Kain entangle-backed world mirroring plus in-process payload mutation.
- `evolutionary_loop`: Rust runtime feature-detected lane choice versus Kain `converge` / `orchestrate` dispatch syntax.

Run the suite from the repo root:

```powershell
python benchmark/run.py
```

Useful variants:

```powershell
python benchmark/run.py --runs 9 --warmups 2
python benchmark/run.py --case ownership_memory
python benchmark/run.py --kain-exe D:\Kain-Lang\target\debug\kain.exe
```

The runner prefers a direct Bazel-built `kain.exe` to avoid the Windows PowerShell launcher `-o` forwarding ambiguity. Use `--kain-exe` or `KAIN_EXE` to pin a specific compiler.
