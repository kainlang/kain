# CASES_V3 Benchmark Runner

> Dogfooding MarkScript as the CASES_V3 benchmark orchestration engine.
> `mks run README.md` executes the pipeline: build → calibrate → run → report.
> All configuration is tables. All dispatch is blockquote intents. All logic is markscript.

## Status: SCAFFOLDED ~> Not Yet Calibrated

**All language god files compile and run, but EXPECTED checksum constants are set to 0.** Run `--compute-all` on each binary to calibrate, then copy values into the EXPECTED constants. Until then, all benchmarks pass trivially (0 == 0 on some languages) or fail trivially (got N, expected 0 on others).

---

## Quick Start

```
# 1. Build all language god files
mks run cases_v3/README.md --section Build

# 2. Calibrate checksums (run each once, capture values)
mks run cases_v3/README.md --section Calibrate

# 3. Smoke test (one run each)
mks run cases_v3/README.md --section Smoke

# 4. Full statistical suite with Python
python cases_v3/bench.py suite full

# 5. Run a single benchmark directly
./cases_v3/cpp/bench.exe binary_trees
./cases_v3/rust/bench.exe nbody
./cases_v3/zig/bench.exe mandelbrot
./cases_v3/go/bench.exe fasta
```

---

## Architecture

```
cases_v3/
├── README.md              ← This file (also the MKS runner)
├── bench.py               ← Python statistical timing runner
├── kain/
│   └── bench.kn           ← ONE god file, 30 functions (check-passed, not compiled)
├── rust/
│   └── bench.rs           ← ONE god file (compiled: bench.exe, 437 KB)
├── cpp/
│   └── bench.cpp          ← ONE god file (compiled: bench.exe, 460 KB)
├── zig/
│   └── bench.zig          ← ONE god file (compiled: bench.exe, 913 KB)
├── go/
│   └── bench.go           ← ONE god file (compiled: bench.exe, 2.9 MB)
└── markscript/
    └── bench.md           ← 12 mini-language benchmarks (runs via mks.exe)
```

### Execution Model

```
COMPILE (once, parallel):
  rustc bench.rs -O3 -C target-cpu=native     → bench_rust.exe
  clang++ bench.cpp -O3 -march=native          → bench_cpp.exe
  zig build-exe bench.zig -O ReleaseFast       → bench_zig.exe
  go build -ldflags="-s -w" bench.go           → bench_go.exe
  kain build bench.kn --target llvm            → bench_kain.exe (NOT YET BUILT)

RUN (parallel across languages):
  bench_cpp.exe binary_trees    # 30 benches
  bench_rust.exe binary_trees   # 30 benches
  bench_zig.exe binary_trees    # 24 benches (6 skipped)
  bench_go.exe binary_trees     # 24 benches (7 skipped)
  mks run bench.md              # 12 MKS mini-language benches
```

---

## Benchmark Table

30 benchmarks across 7 tiers. Each language god file implements a subset:

| # | ID | Tier | C++ | Rust | Zig | Go | Kain | MKS |
|---|-----|------|-----|------|-----|-----|------|-----|
| 1 | binary_trees | 1 | ✅ | ✅ | ✅ | ✅ | ✅ | - |
| 2 | nbody | 1 | ✅ | ✅ | ✅ | - | ✅ | - |
| 3 | spectral_norm | 1 | ✅ | ✅ | ✅ | - | ✅ | - |
| 4 | mandelbrot | 1 | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| 5 | fasta | 1 | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| 6 | regex_redux | 1 | ✅ | ✅ | ✅ | ✅ | ✅ | ✅ |
| 7 | pidigits | 1 | ✅ | ✅ | ✅ | ✅ | ✅ | - |
| 8 | hashmap_heavy | 2 | ✅ | ✅ | ✅ | ✅ | ✅ | - |
| 9 | btree_scan | 2 | ✅ | ✅ | ✅ | ✅ | ✅ | - |
| 10 | sort_gauntlet | 2 | ✅ | ✅ | ✅ | ✅ | ✅ | - |
| 11 | vector_growth | 2 | ✅ | ✅ | ✅ | ✅ | ✅ | - |
| 12 | graph_bfs | 2 | ✅ | ✅ | ✅ | ✅ | ✅ | - |
| 13 | alloc_small_churn | 3 | ✅ | ✅ | ✅ | - | ✅ | - |
| 14 | alloc_large_objects | 3 | ✅ | ✅ | ✅ | - | ✅ | - |
| 15 | arena_vs_malloc | 3 | ✅ | ✅ | ✅ | ✅ | ✅ | - |
| 16 | cache_march | 3 | ✅ | ✅ | ✅ | ✅ | ✅ | - |
| 17 | rc_vs_gc_trace | 3 | ✅ | ✅ | SKIP | - | ✅ | - |
| 18 | parallel_reduce | 4 | ✅ | ✅ | ✅ | ✅ | ✅ | - |
| 19 | mutex_contention | 4 | ✅ | ✅ | ✅ | ✅ | ✅ | - |
| 20 | spsc_queue | 4 | ✅ | ✅ | ✅ | ✅ | ✅ | - |
| 21 | mpmc_queue | 4 | ✅ | ✅ | ✅ | ✅ | ✅ | - |
| 22 | actor_spam | 4 | ✅ | ✅ | SKIP | ✅ | ✅ | - |
| 23 | async_ready_pipeline | 4 | ✅ | ✅ | SKIP | ✅ | ✅ | - |
| 24 | file_read_streaming | 5 | ✅ | ✅ | ✅ | ✅ | ✅ | - |
| 25 | file_write_streaming | 5 | ✅ | ✅ | ✅ | ✅ | ✅ | - |
| 26 | tcp_echo_throughput | 5 | ✅ | ✅ | SKIP | ✅ | ✅ | - |
| 27 | process_spawn_chain | 5 | ✅ | ✅ | SKIP | ✅ | ✅ | - |
| 28 | c_ffi_call_hotloop | 6 | ✅ | ✅ | ✅ | - | ✅ | - |
| 29 | c_buffer_handoff | 6 | ✅ | ✅ | SKIP | - | ✅ | - |
| 30 | build_self_stress | 7 | ✅ | ✅ | ✅ | ✅ | ✅ | - |
| 31 | scalar_mix | MKS | - | - | - | - | - | ✅ |
| 32 | recursive_sum | MKS | - | - | - | - | - | ✅ |
| 33 | branch_dispatch | MKS | - | - | - | - | - | ✅ |
| 34 | call_chain | MKS | - | - | - | - | - | ✅ |
| 35 | fizzbuzz_bomb | MKS | - | - | - | - | - | ✅ |
| 36 | prime_sieve | MKS | - | - | - | - | - | ✅ |
| 37 | collatz_conjecture | MKS | - | - | - | - | - | ✅ |
| 38 | fibonacci_mod | MKS | - | - | - | - | - | ✅ |
| 39 | pi_approx | MKS | - | - | - | - | - | ✅ |
| 40 | vm_bytecode_stress | MKS | - | - | - | - | - | ✅ |
| 41 | checksum_ladder | MKS | - | - | - | - | - | ✅ |
| 42 | array_scan | MKS | - | - | - | - | - | ✅ |
| 43 | string_ops | MKS | - | - | - | - | - | ✅ |

✅ = implemented, SKIP = deliberately skipped (no runtime support), - = not applicable

---

## Running Each Language

### C++ (Reference Implementation)
```
clang++ -std=c++20 -O3 -march=native -DNDEBUG bench.cpp -o bench -lws2_32
./bench binary_trees           # single benchmark
./bench --compute-all          # print all checksums for calibration
```

All 30 benchmarks implemented. EXPECTED constants set to 0 (uncalibrated).

### Rust
```
rustc -C opt-level=3 -C target-cpu=native -C debuginfo=0 bench.rs -o bench
./bench binary_trees           # single benchmark
```

All 30 benchmarks implemented. Prints `[FAIL]` when checksum != expected (all expected=0). Does not support `--compute-all` yet.

### Zig
```
zig build-exe bench.zig -O ReleaseFast --name bench
./bench binary_trees           # single benchmark
```

24 benchmarks implemented (6 skipped: rc_vs_gc_trace, actor_spam, async_ready_pipeline, tcp_echo_throughput, process_spawn_chain, c_buffer_handoff). Prints raw checksum.

### Go
```
go build -ldflags="-s -w"
./bench binary_trees           # single benchmark
```

24 benchmarks implemented (7 skipped: nbody, spectral_norm, alloc_small_churn, alloc_large_objects, rc_vs_gc_trace, c_ffi_call_hotloop, c_buffer_handoff). Prints raw checksum, exits 0.

### Kain
```
kain build bench.kn --target llvm    # NOT YET COMPILED
```

`kain check bench.kn` passes with 1880 items. Uses advanced features (world, Unsafe, alloc/decay, ptr_offset, mem_store/mem_load). Requires LLVM build, not yet done.

### MarkScript
```
mks run cases_v3/markscript/bench.md
```

12 mini-language benchmarks inside markscript blocks. Runs directly in the MKS VM. No compilation step needed. Outputs checksums via `print(str(cs))`.

---

## MKS Benchmark Commands

```
# Full MKS pipeline (build + calibrate + smoke)
mks run cases_v3/README.md

# Run specific stages
mks run cases_v3/README.md --section Build
mks run cases_v3/README.md --section Calibrate
mks run cases_v3/README.md --section Smoke

# Run only MKS mini-language benchmarks
mks run cases_v3/markscript/bench.md

# Check for errors without running
mks check cases_v3/markscript/bench.md

# Statistical timing with Python
python cases_v3/bench.py suite full
python cases_v3/bench.py suite dev      # Kain-only fast iteration
```

---

## Current Binary Sizes

| Binary | Size | Source Lines |
|--------|------|-------------|
| cpp/bench.exe | 460 KB | ~1815 lines |
| rust/bench.exe | 437 KB | ~1900 lines |
| zig/bench.exe | 913 KB | ~1677 lines |
| go/bench.exe | 2.9 MB | ~1395 lines |
| kain/bench.kn | 43 KB | ~1365 lines |
| mks.exe | 3.2 MB | 17 .kn files, 429 KB |

---

## Sample Timings (from this session, uncalibrated)

These are raw wall-clock timings on a single run. Not statistically valid ___ warmup, JIT, and OS jitter are uncontrolled. Use `python bench.py suite full` for proper measurements.

| Case | C++ ms | Rust ms | Zig ms | Go ms | Notes |
|------|--------|---------|--------|-------|-------|
| binary_trees | 2051 | 2223 | 1694 | 1201 | Tree alloc + traverse + teardown |
| nbody | 139 | 151 | 117 | - | Double-precision N-body |
| mandelbrot | 104 | 104 | 103 | 465 | Mandelbrot escape iterations |
| fasta | 31 | 29 | - | 36 | LCG + DNA generation |
| hashmap_heavy | 707 | 1017 | - | - | 1M string key operations |

---

## Known Limitations

### All Language God Files
- **EXPECTED constants are 0**: Benchmarks need calibration via `--compute-all` (C++) or manual run-and-copy
- **No statistical timing in native binaries**: Each binary runs once and exits. Use Python `bench.py` for warmup+timed runs
- **No cross-language checksum verification**: Each language may produce different checksums for the same algorithm (different RNG, different precision)

### Kain
- **Not compiled to native**: `kain check` passes, but `kain build --target llvm` has not been run
- Uses advanced features (world, alloc/decay, collapse/observe) that require full LLVM codegen path
- Requires runtime manifest + clang path configuration for compilation

### MarkScript
- **String display broken**: String literals are hashed at parse time -- `print("hello")` outputs `<invalid>` instead of "hello" (Issue #1 in roadmap)
- **No variable interpolation in blockquotes**: Cannot generate dynamic `> run` commands
- **No handler result chaining**: Cannot capture output of `> run` for use in subsequent intents
- **process_output_text stdout capture broken** on Windows: `> run "cmd"` executes but cannot capture output (Issue #3 in roadmap)
- **Multi-word intents limited**: Some work ("read file", "parse json") via aliases, but multi-word dispatch fundamentally requires single-word aliases (Issue #2 in roadmap)
- **Mini-language is integer-only**: No string manipulation in markscript blocks (only math + control flow)
- **No compile-time intent validation**: `mks check` doesn't verify that blockquote intents match registered handlers (Issue #6 in roadmap)

---

## The God-File Contract

Every language god file implements the same contract:

1. **30 benchmark functions** (or a subset), each returning an exit code
2. **Deterministic checksum** computed from a fixed workload
3. **Comparison against EXPECTED constant**
4. **Dispatcher** via CLI arg: `bench <benchmark_name>`
5. **Shared helpers**: LCG RNG (seed 42), djb2 hash, modulus 1000000007

```
Input:  none (constants are file-local)
Output: Int (exit code --- 0 = correct checksum, non-zero = failure)
```

### Checksum Contract
```
const EXPECTED: Int = <precomputed>

fn bench_case() -> Int:
    let result = compute_workload()
    if result != EXPECTED:
        return 1    # FAIL
    return 0        # PASS
```

### Why God Files Beat Per-Case Files

| | V1 (cases/) | V2 (cases_v2/) | V3 (cases_v3/) |
|---|-------------|----------------|----------------|
| Compiles per run | ~240 | ~20 | **5** |
| Files to manage | 414 | ~80 | **5** |
| Adding benchmark | Create folder + 6 files | Add function to pack | **Add 1 function** |
| Cross-benchmark sharing | Impossible | Implicit | **Shared helpers** |
| Wall-clock build time | 15-30 min | 5-10 min | **~30 sec** |

---

## Invariants

| # | Invariant |
|---|-----------|
| 1 | Every benchmark has a deterministic checksum stable across runs |
| 2 | Exit code 0 = checksum matched expected. Non-zero = failure |
| 3 | No proof-backed collapse tricks |-> the algorithm runs what it says |
| 4 | Every language god file implements the same algorithm shape |
| 5 | The runner never hardcodes expected values – calibration produces them |
| 6 | MKS orchestrates build + calibrate + smoke; Python handles statistical timing |
| 7 | All configuration lives in tables, not external JSON |
| 8 | Adding a benchmark = adding one row to the table + one function per language |

---

*Built with [MarkScript](https://kain-lang.org/markscript) ~~ dogfooding Kain's companion language.*
*"The benchmark runner IS the documentation."*
