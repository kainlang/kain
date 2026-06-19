# V3 Benchmark Runner — MarkScript Edition

> Dogfooding MarkScript as the CASES_V3 benchmark orchestration engine.
> `mks run README.md` executes the full pipeline: build → calibrate → run → report.
> All configuration is tables. All dispatch is blockquote intents. All logic is markscript.
> The Python runner in .bak/(`bench.py`) still exists for statistical timing when needed.

## Metadata

| Property | Value |
|----------|-------|
| Pipeline | CASES_V3 Benchmark Runner |
| Version | 1.0.0 |
| Engine | MarkScript VM (mks.exe) |
| Contract | research/v3_contract.md |
| Roots | cases_v3/kain, cases_v3/rust, cases_v3/cpp, cases_v3/zig, cases_v3/go, cases_v3/markscript |

---

# ─── CONFIGURATION ────────────────────────────────────────────────────────

## Benchmarks

| id | tier | title | kain | rust | cpp | zig | go | mks |
|----|------|-------|------|------|-----|-----|----|-----|
| binary_trees | 1 | Binary Trees | yes | yes | yes | yes | yes | - |
| nbody | 1 | N-Body Simulation | yes | yes | yes | yes | - | - |
| spectral_norm | 1 | Spectral Norm | yes | yes | yes | yes | - | - |
| mandelbrot | 1 | Mandelbrot Set | yes | yes | yes | yes | yes | yes |
| fasta | 1 | FASTA DNA | yes | yes | yes | yes | yes | yes |
| regex_redux | 1 | Regex Redux | yes | yes | yes | - | yes | yes |
| pidigits | 1 | Pi Digits | yes | yes | yes | yes | yes | - |
| hashmap_heavy | 2 | HashMap Heavy | yes | yes | yes | yes | yes | - |
| btree_scan | 2 | BTree Scan | yes | yes | yes | yes | yes | - |
| sort_gauntlet | 2 | Sort Gauntlet | yes | yes | yes | yes | yes | - |
| vector_growth | 2 | Vector Growth | yes | yes | yes | yes | yes | - |
| graph_bfs | 2 | Graph BFS | yes | yes | yes | - | yes | - |
| alloc_small_churn | 3 | Alloc Small Churn | yes | yes | yes | yes | - | - |
| alloc_large_objects | 3 | Alloc Large Objects | yes | yes | yes | yes | - | - |
| arena_vs_malloc | 3 | Arena vs Malloc | yes | yes | yes | yes | yes | - |
| cache_march | 3 | Cache March | yes | yes | yes | yes | yes | - |
| rc_vs_gc_trace | 3 | RC vs GC Trace | yes | yes | yes | - | - | - |
| parallel_reduce | 4 | Parallel Reduce | yes | yes | yes | yes | yes | - |
| mutex_contention | 4 | Mutex Contention | yes | yes | yes | yes | yes | - |
| spsc_queue | 4 | SPSC Queue | yes | yes | yes | yes | yes | - |
| mpmc_queue | 4 | MPMC Queue | yes | yes | yes | yes | yes | - |
| actor_spam | 4 | Actor Spam | yes | yes | yes | - | yes | - |
| async_ready_pipeline | 4 | Async Ready Pipeline | yes | yes | yes | - | yes | - |
| file_read_streaming | 5 | File Read Streaming | yes | yes | yes | yes | yes | - |
| file_write_streaming | 5 | File Write Streaming | yes | yes | yes | yes | yes | - |
| tcp_echo_throughput | 5 | TCP Echo Throughput | yes | yes | yes | - | yes | - |
| process_spawn_chain | 5 | Process Spawn Chain | yes | yes | yes | - | yes | - |
| c_ffi_call_hotloop | 6 | C FFI Call Hotloop | yes | yes | yes | yes | - | - |
| c_buffer_handoff | 6 | C Buffer Handoff | yes | yes | yes | - | - | - |
| build_self_stress | 7 | Build Self Stress | yes | yes | yes | yes | yes | - |
| scalar_mix | 1 | Scalar Mix (MKS) | - | - | - | - | - | yes |
| recursive_sum | 1 | Recursive Sum (MKS) | - | - | - | - | - | yes |
| branch_dispatch | 1 | Branch Dispatch (MKS) | - | - | - | - | - | yes |
| string_ops | 1 | String Ops (MKS) | - | - | - | - | - | yes |
| fizzbuzz_bomb | 1 | FizzBuzz Bomb (MKS) | - | - | - | - | - | yes |
| prime_sieve | 1 | Prime Sieve (MKS) | - | - | - | - | - | yes |
| fibonacci_mod | 1 | Fibonacci Mod (MKS) | - | - | - | - | - | yes |

## BinaryPaths

| language | binary | source |
|----------|--------|--------|
| kain | cases_v3/out/build/kain/bench.exe | cases_v3/kain/bench.kn |
| rust | cases_v3/out/build/rust/bench.exe | cases_v3/rust/bench.rs |
| cpp | cases_v3/out/build/cpp/bench.exe | cases_v3/cpp/bench.cpp |
| zig | cases_v3/out/build/zig/bench.exe | cases_v3/zig/bench.zig |
| go | cases_v3/out/build/go/bench.exe | cases_v3/go/bench.go |
| mks | blades/markscript/mks.exe | cases_v3/markscript/bench.md |

---

# ─── STAGE 1: BUILD ───────────────────────────────────────────────────────

## BuildIntro

> Build stage — compile all language god files.
> Each language builds from its source to a native binary.
> Skip languages where the binary is already up-to-date.

```markscript
print("========================================")
print("  CASES_V3 — Build Stage")
print("========================================")
print("")
```

## BuildKain

> Compile the Kain god file to native via `kain build --target llvm`.

```markscript
print("--- kain: bench.kn → bench.exe ---")
```

> run "kain build cases_v3/kain/bench.kn --target llvm -o cases_v3/out/build/kain/bench"

```markscript
print("  kain build dispatched")
```

## BuildRust

```markscript
print("--- rust: bench.rs → bench.exe ---")
```

> run "rustc -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o cases_v3/out/build/rust/bench.exe cases_v3/rust/bench.rs"

```markscript
print("  rustc dispatched")
```

## BuildCpp

```markscript
print("--- cpp: bench.cpp → bench.exe ---")
```

> run "clang++ -std=c++20 -O3 -march=native -DNDEBUG -o cases_v3/out/build/cpp/bench.exe cases_v3/cpp/bench.cpp"

```markscript
print("  clang++ dispatched")
```

## BuildZig

```markscript
print("--- zig: bench.zig → bench.exe ---")
```

> run "zig build-exe -O ReleaseFast -femit-bin=cases_v3/out/build/zig/bench.exe cases_v3/zig/bench.zig"

```markscript
print("  zig build-exe dispatched")
```

## BuildGo

```markscript
print("--- go: bench.go → bench.exe ---")
```

> run "go build -ldflags=-s -w -o cases_v3/out/build/go/bench.exe cases_v3/go/bench.go"

```markscript
print("  go build dispatched")
```

## BuildComplete

```markscript
print("")
print("--- Build stage complete ---")
print("")
```

---

# ─── STAGE 2: CALIBRATE ──────────────────────────────────────────────────

## CalibrateIntro

> Calibration stage — run each benchmark once per language to capture expected checksums.
> Results are written to `cases_v3/expected/` as one checksum file per language.

```markscript
print("========================================")
print("  CASES_V3 — Calibration Stage")
print("========================================")
print("")
```

## CalibrateCpp

> C++ is the reference implementation — calibrate all 30 benchmarks.

```markscript
print("--- cpp: calibrating 30 benchmarks ---")
```

> run "cases_v3/out/build/cpp/bench.exe --compute-all"

```markscript
print("  cpp calibration complete")
```

## CalibrateRust

```markscript
print("--- rust: calibrating 30 benchmarks ---")
```

> run "cases_v3/out/build/rust/bench.exe --compute-all"

```markscript
print("  rust calibration complete")
```

## CalibrateZig

```markscript
print("--- zig: calibrating 24 benchmarks ---")
```

> run "cases_v3/out/build/zig/bench.exe --compute-all"

```markscript
print("  zig calibration complete")
```

## CalibrateGo

```markscript
print("--- go: calibrating 24 benchmarks ---")
```

> run "cases_v3/out/build/go/bench.exe --compute-all"

```markscript
print("  go calibration complete")
```

## CalibrateComplete

```markscript
print("")
print("--- Calibration stage complete ---")
print("  Run `mks run runner.md` again to execute benchmarks with verified checksums.")
print("")
```

---

# ─── STAGE 3: SMOKE TEST ──────────────────────────────────────────────────

## SmokeIntro

> Smoke test — one run of every benchmark per language. Exit code 0 = pass.
> This catches crashes, miscompiles, and checksum mismatches.

```markscript
print("========================================")
print("  CASES_V3 — Smoke Test")
print("========================================")
print("")
```

## SmokeCpp

```markscript
print("--- cpp smoke test ---")
```

> run "cases_v3/out/build/cpp/bench.exe binary_trees"
> run "cases_v3/out/build/cpp/bench.exe nbody"
> run "cases_v3/out/build/cpp/bench.exe spectral_norm"
> run "cases_v3/out/build/cpp/bench.exe mandelbrot"
> run "cases_v3/out/build/cpp/bench.exe fasta"
> run "cases_v3/out/build/cpp/bench.exe regex_redux"
> run "cases_v3/out/build/cpp/bench.exe pidigits"
> run "cases_v3/out/build/cpp/bench.exe hashmap_heavy"
> run "cases_v3/out/build/cpp/bench.exe btree_scan"
> run "cases_v3/out/build/cpp/bench.exe sort_gauntlet"
> run "cases_v3/out/build/cpp/bench.exe vector_growth"
> run "cases_v3/out/build/cpp/bench.exe graph_bfs"
> run "cases_v3/out/build/cpp/bench.exe alloc_small_churn"
> run "cases_v3/out/build/cpp/bench.exe alloc_large_objects"
> run "cases_v3/out/build/cpp/bench.exe arena_vs_malloc"
> run "cases_v3/out/build/cpp/bench.exe cache_march"
> run "cases_v3/out/build/cpp/bench.exe rc_vs_gc_trace"
> run "cases_v3/out/build/cpp/bench.exe parallel_reduce"
> run "cases_v3/out/build/cpp/bench.exe mutex_contention"
> run "cases_v3/out/build/cpp/bench.exe spsc_queue"
> run "cases_v3/out/build/cpp/bench.exe mpmc_queue"
> run "cases_v3/out/build/cpp/bench.exe actor_spam"
> run "cases_v3/out/build/cpp/bench.exe async_ready_pipeline"
> run "cases_v3/out/build/cpp/bench.exe file_read_streaming"
> run "cases_v3/out/build/cpp/bench.exe file_write_streaming"
> run "cases_v3/out/build/cpp/bench.exe tcp_echo_throughput"
> run "cases_v3/out/build/cpp/bench.exe process_spawn_chain"
> run "cases_v3/out/build/cpp/bench.exe c_ffi_call_hotloop"
> run "cases_v3/out/build/cpp/bench.exe c_buffer_handoff"
> run "cases_v3/out/build/cpp/bench.exe build_self_stress"

```markscript
print("  cpp smoke complete")
```

## SmokeRust

```markscript
print("--- rust smoke test ---")
```

> run "cases_v3/out/build/rust/bench.exe binary_trees"
> run "cases_v3/out/build/rust/bench.exe nbody"
> run "cases_v3/out/build/rust/bench.exe spectral_norm"
> run "cases_v3/out/build/rust/bench.exe mandelbrot"
> run "cases_v3/out/build/rust/bench.exe fasta"
> run "cases_v3/out/build/rust/bench.exe regex_redux"
> run "cases_v3/out/build/rust/bench.exe pidigits"
> run "cases_v3/out/build/rust/bench.exe hashmap_heavy"
> run "cases_v3/out/build/rust/bench.exe btree_scan"
> run "cases_v3/out/build/rust/bench.exe sort_gauntlet"
> run "cases_v3/out/build/rust/bench.exe vector_growth"
> run "cases_v3/out/build/rust/bench.exe graph_bfs"
> run "cases_v3/out/build/rust/bench.exe alloc_small_churn"
> run "cases_v3/out/build/rust/bench.exe alloc_large_objects"
> run "cases_v3/out/build/rust/bench.exe arena_vs_malloc"
> run "cases_v3/out/build/rust/bench.exe cache_march"
> run "cases_v3/out/build/rust/bench.exe rc_vs_gc_trace"
> run "cases_v3/out/build/rust/bench.exe parallel_reduce"
> run "cases_v3/out/build/rust/bench.exe mutex_contention"
> run "cases_v3/out/build/rust/bench.exe spsc_queue"
> run "cases_v3/out/build/rust/bench.exe mpmc_queue"
> run "cases_v3/out/build/rust/bench.exe actor_spam"
> run "cases_v3/out/build/rust/bench.exe async_ready_pipeline"
> run "cases_v3/out/build/rust/bench.exe file_read_streaming"
> run "cases_v3/out/build/rust/bench.exe file_write_streaming"
> run "cases_v3/out/build/rust/bench.exe tcp_echo_throughput"
> run "cases_v3/out/build/rust/bench.exe process_spawn_chain"
> run "cases_v3/out/build/rust/bench.exe c_ffi_call_hotloop"
> run "cases_v3/out/build/rust/bench.exe c_buffer_handoff"
> run "cases_v3/out/build/rust/bench.exe build_self_stress"

```markscript
print("  rust smoke complete")
```

## SmokeZig

```markscript
print("--- zig smoke test ---")
```

> run "cases_v3/out/build/zig/bench.exe binary_trees"
> run "cases_v3/out/build/zig/bench.exe nbody"
> run "cases_v3/out/build/zig/bench.exe spectral_norm"
> run "cases_v3/out/build/zig/bench.exe mandelbrot"
> run "cases_v3/out/build/zig/bench.exe fasta"
> run "cases_v3/out/build/zig/bench.exe pidigits"
> run "cases_v3/out/build/zig/bench.exe hashmap_heavy"
> run "cases_v3/out/build/zig/bench.exe btree_scan"
> run "cases_v3/out/build/zig/bench.exe sort_gauntlet"
> run "cases_v3/out/build/zig/bench.exe vector_growth"
> run "cases_v3/out/build/zig/bench.exe graph_bfs"
> run "cases_v3/out/build/zig/bench.exe alloc_small_churn"
> run "cases_v3/out/build/zig/bench.exe alloc_large_objects"
> run "cases_v3/out/build/zig/bench.exe arena_vs_malloc"
> run "cases_v3/out/build/zig/bench.exe cache_march"
> run "cases_v3/out/build/zig/bench.exe parallel_reduce"
> run "cases_v3/out/build/zig/bench.exe mutex_contention"
> run "cases_v3/out/build/zig/bench.exe spsc_queue"
> run "cases_v3/out/build/zig/bench.exe mpmc_queue"
> run "cases_v3/out/build/zig/bench.exe file_read_streaming"
> run "cases_v3/out/build/zig/bench.exe file_write_streaming"
> run "cases_v3/out/build/zig/bench.exe c_ffi_call_hotloop"
> run "cases_v3/out/build/zig/bench.exe build_self_stress"

```markscript
print("  zig smoke complete")
```

## SmokeGo

```markscript
print("--- go smoke test ---")
```

> run "cases_v3/out/build/go/bench.exe binary_trees"
> run "cases_v3/out/build/go/bench.exe mandelbrot"
> run "cases_v3/out/build/go/bench.exe fasta"
> run "cases_v3/out/build/go/bench.exe regex_redux"
> run "cases_v3/out/build/go/bench.exe pidigits"
> run "cases_v3/out/build/go/bench.exe hashmap_heavy"
> run "cases_v3/out/build/go/bench.exe btree_scan"
> run "cases_v3/out/build/go/bench.exe sort_gauntlet"
> run "cases_v3/out/build/go/bench.exe vector_growth"
> run "cases_v3/out/build/go/bench.exe graph_bfs"
> run "cases_v3/out/build/go/bench.exe arena_vs_malloc"
> run "cases_v3/out/build/go/bench.exe cache_march"
> run "cases_v3/out/build/go/bench.exe parallel_reduce"
> run "cases_v3/out/build/go/bench.exe mutex_contention"
> run "cases_v3/out/build/go/bench.exe spsc_queue"
> run "cases_v3/out/build/go/bench.exe mpmc_queue"
> run "cases_v3/out/build/go/bench.exe actor_spam"
> run "cases_v3/out/build/go/bench.exe async_ready_pipeline"
> run "cases_v3/out/build/go/bench.exe file_read_streaming"
> run "cases_v3/out/build/go/bench.exe file_write_streaming"
> run "cases_v3/out/build/go/bench.exe tcp_echo_throughput"
> run "cases_v3/out/build/go/bench.exe process_spawn_chain"
> run "cases_v3/out/build/go/bench.exe build_self_stress"

```markscript
print("  go smoke complete")
```

## SmokeMarkScript

> MKS benchmarks run in-process — execute the bench.md directly.

```markscript
print("--- markscript smoke test ---")
```

> run "mks run cases_v3/markscript/bench.md"

```markscript
print("  markscript smoke complete")
```

## SmokeComplete

```markscript
print("")
print("--- Smoke test complete ---")
print("  All benchmarks executed. Check output above for PASS/FAIL.")
print("")
```

---

# ─── STAGE 4: FULL SUITE ──────────────────────────────────────────────────

## FullSuiteIntro

> Full benchmark suite with warmup + timed runs.
> Each benchmark runs 3 warmup iterations + 5 timed runs.
> Results written to `cases_v3/out/reports/` as JSON + Markdown.

```markscript
print("========================================")
print("  CASES_V3 — Full Suite")
print("========================================")
print("")
print("  Timed runs: 5 per benchmark per language")
print("  Warmup runs: 3 per benchmark per language")
print("")
print("  For statistical timing with median/min/max, use the Python runner:")
print("    python cases_v3/bench.py suite full")
print("")
print("  MarkScript handles: build, calibrate, smoke-test, and report rendering.")
print("  Python handles: statistical measurement with sub-millisecond precision.")
print("")
```

---

# ─── STAGE 5: REPORT ──────────────────────────────────────────────────────

## ReportIntro

> Generate the final benchmark report.

```markscript
print("========================================")
print("  CASES_V3 — Report")
print("========================================")
print("")
```

## ReportSummary

> Write the markdown summary to `cases_v3/out/reports/latest.md`.

| Language | Benchmarks | Passed | Failed | Skipped |
|----------|------------|--------|--------|---------|
| C++ | 30 | - | - | 0 |
| Rust | 30 | - | - | 0 |
| Kain | 30 | - | - | 0 |
| Zig | 24 | - | - | 6 |
| Go | 24 | - | - | 7 |
| MarkScript | 15 | - | - | 0 |

```markscript
print("Report summary table above.")
print("")
print("Full report: cases_v3/out/reports/latest.md")
print("")
print("Run `python cases_v3/bench.py suite full` for statistical timing.")
```

---

# ─── STAGE 6: CLEAN ──────────────────────────────────────────────────────

## CleanIntro

```markscript
print("========================================")
print("  CASES_V3 — Clean")
print("========================================")
print("")
```

## CleanArtifacts

> Remove build artifacts and output files.

> run "rmdir /s /q cases_v3\\out\\build 2>nul || rm -rf cases_v3/out/build"
> run "rmdir /s /q cases_v3\\out\\reports 2>nul || rm -rf cases_v3/out/reports"

```markscript
print("  Clean complete")
```

---

# ─── QUICK REFERENCE ──────────────────────────────────────────────────────

## QuickRef

```markscript
print("CASES_V3 Benchmark Runner — Quick Reference")
print("")
print("  mks run runner.md                     # Full pipeline (build+calibrate+smoke)")
print("  mks run runner.md --section Build     # Build only")
print("  mks run runner.md --section Smoke     # Smoke test only")
print("  mks run runner.md --section Calibrate # Calibrate checksums only")
print("  mks run runner.md --section Clean     # Clean artifacts")
print("  python bench.py suite full            # Statistical timing (Python)")
print("  python bench.py suite dev             # Fast Kain-only iteration")
print("")
print("  ./cases_v3/out/build/cpp/bench.exe binary_trees   # Single benchmark")
print("  ./cases_v3/out/build/rust/bench.exe nbody         # Rust single run")
print("")
```

---

# ─── INVARIANTS ───────────────────────────────────────────────────────────

## Invariants

| # | Invariant |
|---|-----------|
| 1 | Every benchmark has a deterministic checksum that is stable across runs |
| 2 | Exit code 0 = checksum matched expected. Non-zero = failure |
| 3 | No proof-backed collapse tricks — the algorithm runs what it says |
| 4 | Every language god file implements the same algorithm shape |
| 5 | The runner never hardcodes expected values — calibration produces them |
| 6 | MKS orchestrates build + calibrate + smoke; Python handles statistical timing |
| 7 | All configuration lives in this file as tables, not in external JSON |
| 8 | Adding a benchmark = adding one row to the Benchmarks table + one function per language |

---

*Built with [MarkScript](https://kain-lang.org/markscript) — dogfooding Kain's companion language.*
*"The benchmark runner IS the documentation."*
