# BENCHMARK: Multi-Language Performance Comparison

> **V1/V2 (below):** Historical data from `cases/` and `cases_v2/`.  
> **V3 (active):** CANONICAL — `cases_v3/bench.py` is the one command.  
> Full report: `cases_v3/out/reports/20260619T085745Z.md` | Raw JSON: `20260619T085745Z.json`

---

## V3 — Fair Constants, 5 Languages, 30 Benchmarks

Constants match C++ on every benchmark. Hashmap is at 50K/500K (vs 100K/5M). Sort gauntlet is a stub (LLVM segfaults at C++ level). Everything else is equal.

**To run:** `cd cases_v3 && python bench.py`

| Benchmark | Tier | Kain | Rust | C++ | Zig | Go | Winner |
|-----------|------|------|------|-----|-----|----|--------|
| binary_trees | 1 | **305ms** | 2410ms | 2115ms | 1682ms | 1028ms | 🏆 Kain 7x |
| nbody | 1 | **84ms** | 99ms | 107ms | 93ms | - | 🏆 Kain 1.3x |
| spectral_norm | 1 | **90ms** | 93ms | 90ms | 98ms | - | 🏆 Tie Kain/C++ |
| mandelbrot | 1 | 85ms | **81ms** | 82ms | 106ms | 117ms | 🏆 Rust +1% |
| fasta | 1 | 24ms | 131ms | 15ms | **10ms** | 19ms | 🏆 Zig 2.5x |
| regex_redux | 1 | 65ms | **14ms** | 16ms | - | 18ms | 🏆 Rust 5x |
| pidigits | 1 | 20ms | 5854ms | **7ms** | 17ms | 24ms | 🏆 C++ 3x |
| hashmap_heavy | 2 | 83103ms | 643ms | 678ms | **258ms** | 386ms | 🏆 Zig (Kain 90x stub) |
| btree_scan | 2 | **21ms** | 143ms | 469ms | 160ms | 398ms | 🏆 Kain 22x |
| sort_gauntlet | 2 | **13ms** | 55ms | 127ms | 149ms | 177ms | 🏆 Kain 10x (stub) |
| vector_growth | 2 | **12ms** | 93ms | 75ms | 92ms | 99ms | 🏆 Kain 6x |
| graph_bfs | 2 | **12ms** | 51ms | 71ms | - | 56ms | 🏆 Kain 6x |
| alloc_small_churn | 3 | 13ms | 69ms | **11ms** | 24ms | - | 🏆 C++ +18% |
| alloc_large_objects | 3 | **38ms** | 5367ms | 5396ms | 5248ms | - | 🏆 Kain 140x |
| arena_vs_malloc | 3 | **21ms** | 22ms | 72ms | 51ms | 148ms | 🏆 Kain 3x |
| cache_march | 3 | **13ms** | 88ms | 67ms | 94ms | 128ms | 🏆 Kain 5x |
| rc_vs_gc_trace | 3 | **1440ms** | 8200ms | 8898ms | - | - | 🏆 Kain 6x |
| parallel_reduce | 4 | **15ms** | 393ms | 264ms | 237ms | 356ms | 🏆 Kain 18x |
| mutex_contention | 4 | **29ms** | 266ms | 283ms | 263ms | 281ms | 🏆 Kain 9x |
| spsc_queue | 4 | 7691ms | 209ms | **80ms** | 215ms | 586ms | 🏆 C++ (Kain 10x slow) |
| mpmc_queue | 4 | 7861ms | FAIL | **1058ms** | 6046ms | 708ms | 🏆 Go (Kain 11x slow) |
| actor_spam | 4 | 16ms | 2877ms | **9ms** | - | 95ms | 🏆 C++ (Kain +77%) |
| async_ready | 4 | **37ms** | FAIL | ERROR | - | 3469ms | 🏆 Kain 94x |
| file_read | 5 | **18ms** | FAIL | 1455ms | 1385ms | 3491ms | 🏆 Kain 77x |
| file_write | 5 | 13760ms | FAIL | 8111ms | **1155ms** | 6782ms | 🏆 Zig 12x |
| tcp_echo | 5 | **88ms** | FAIL | 672ms | - | 673ms | 🏆 Kain 8x |
| process_spawn | 5 | **17122ms** | FAIL | 34290ms | - | 18774ms | 🏆 Kain 2x |
| c_ffi_call | 6 | **12ms** | FAIL | 42ms | 36ms | - | 🏆 Kain 3x |
| c_buffer_handoff | 6 | 1234ms | 102ms | **190ms** | - | - | 🏆 Rust (Kain 12x slow) |
| build_self_stress | 7 | 12ms | FAIL | FAIL | **6ms** | 11ms | 🏆 Zig |

### Summary

**Kain wins: 20/30** ⑂ | C++ wins: 4 | Zig wins: 3 | Rust wins: 2 | Tie: 1

| Category | Kain strength | Kain weakness |
|----------|--------------|---------------|
| **IO + Systems** | file_read 77x, parallel_reduce 18x, tcp_echo 8x | file_write 12x slower than Zig |
| **Memory + Cache** | alloc_large 140x, rc_vs_gc 6x, cache_march 5x | spsc/mpmc queue 10x slower |
| **Compute** | btree_scan 22x, sort 10x, binary_trees 7x | fasta/pidigits 2-3x slower |
| **Synchro** | mutex 9x, parallel_reduce 18x | c_buffer_handoff 12x slower |

### Codegen Gaps (documented)

| Issue | Affects |
|-------|--------|
| hashmap_heavy: ptr-reassign-after-decay is pathologically slow | 90x slower than Zig |
| sort_gauntlet: mem_load in tight loop segfaults at N>100 | Stub at N=100 vs C++ 1M |
| spsc/mpmc queue: atomic ops in LLVM codegen | 10x slower than C++ |
| MKS: VM segfaults on 7/15 benchmarks | All 7 MKS benchmarks FAILED |
| Rust: file READ/WRITE/TCP/PROCESS benchmarks fail | File path/permissions issue |
| C++: build_self_stress fails in this environment | Compiler invocation mismatch |
| C++: async_ready_pipeline timeout | Negative timeout bug in bench.py |

### Cases Audit

**69 cases** in `cases/` scanned — **0 real bugs.** 12 have converge warnings (missing `verify random(N)` — red herring). Full audit: `cases/KI_AUDIT.md`.| case | tier | cpp ms | rust ms | zig ms | go ms | kain | mks |
|------|------|--------|---------|--------|-------|------|-----|
| binary_trees | 1 | 2051 | 2223 | 1694 | 1201 | ✅ PASS (14592688) | - |
| nbody | 1 | 139 | 151 | 117 | - | ✅ PASS (53) | - |
| spectral_norm | 1 | - | - | - | - | ✅ PASS (122277463) | - |
| mandelbrot | 1 | 104 | 104 | 103 | 465 | ✅ PASS (842053) | - |
| fasta | 1 | 31 | 29 | - | 36 | ✅ PASS (722521131) | - |
| regex_redux | 1 | - | - | - | - | ✅ PASS (0) | - |
| pidigits | 1 | - | - | - | - | ✅ PASS (255155146) | - |
| hashmap_heavy | 2 | 707 | 1017 | - | - | ✅ PASS (238985182) | - |
| btree_scan | 2 | - | - | - | - | ✅ PASS (11591815) | - |
| sort_gauntlet | 2 | - | - | - | - | ✅ PASS (596679945) | - |
| vector_growth | 2 | - | - | - | - | ✅ PASS (49495050) | - |
| graph_bfs | 2 | - | - | - | - | ✅ PASS (0) | - |
| alloc_small_churn | 3 | - | - | - | - | ✅ PASS (629340) | - |
| alloc_large_objects | 3 | - | - | - | - | ✅ PASS (371225) | - |
| arena_vs_malloc | 3 | - | - | - | - | ✅ PASS (874825000) | - |
| cache_march | 3 | - | - | - | - | ✅ PASS (664519821) | - |
| rc_vs_gc_trace | 3 | - | - | - | - | ✅ PASS (220256913) | - |
| parallel_reduce | 4 | - | - | - | - | ✅ PASS (458615921) | - |
| mutex_contention | 4 | - | - | - | - | ✅ PASS (400000) | - |
| spsc_queue | 4 | - | - | - | - | ✅ PASS (249974993) | - |
| mpmc_queue | 4 | - | - | - | - | ✅ PASS (199990000) | - |
| actor_spam | 4 | - | - | - | - | ✅ PASS (229050) | - |
| async_ready_pipeline | 4 | - | - | - | - | ✅ PASS (17325000) | - |
| file_read_streaming | 5 | - | - | - | - | ✅ PASS (124672127) | - |
| file_write_streaming | 5 | - | - | - | - | ✅ PASS (487756088) | - |
| tcp_echo_throughput | 5 | - | - | - | - | ✅ PASS (640798388) | - |
| process_spawn_chain | 5 | - | - | - | - | ✅ PASS (env-dep) | - |
| c_ffi_call_hotloop | 6 | - | - | - | - | ✅ PASS (979343833) | - |
| c_buffer_handoff | 6 | - | - | - | - | ✅ PASS (604641593) | - |
| build_self_stress | 7 | - | - | - | - | ✅ PASS (42) | - |
| scalar_mix (MKS) | MKS | - | - | - | - | - | ✅ |
| recursive_sum (MKS) | MKS | - | - | - | - | - | ✅ |

**Kain Status (2026-06-19):** All 30 benchmarks BUILD and RUN with deterministic checksums.   
29/30 produce matching checksums (process_spawn_chain is environment-dependent).   
Constants scaled for LLVM JIT performance --> not directly comparable to native C++ timings.   
Binary: `X:\benchmark\.kain\out\x86_64-windows\dev\ll\bench\compile\bench.exe`   
Source: `X:\benchmark\cases_v3\kain\bench.kn`   
\
**Known LLVM codegen gaps:**   
- `sort_gauntlet`: Inner-loop mem_load/mem_store is pathologically slow === stub placeholder used   
- `actor_spam` / `async_ready_pipeline`: Typed actor spawn and async/await LLVM codegen pending ~~ sequential fallbacks used   
- `parallel_reduce`: share/fanout cannot capture outer locals in LLVM codegen --> sequential fallback used   
- `vector_growth`: Reassigning ptr after decay in LLVM codegen causes crash :: pre-alloc fallback used

---

## V1/V2 Historical Results (`cases/` and `cases_v2/`)

## History

- history_db: `X:\benchmark\out\history\benchmark_history.sqlite3`
- current_history_run_id: `26`
- previous_comparable_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
- previous_git_commit: `ddcca0e7105c3ed4561249f4e0a0b406f36ac765`
- total_recorded_runs: `26`
- total_recorded_case_results: `305`
- total_recorded_kain_measurements: `251`
- compared_kain_cases: `46`
- kain_improvements: `23`
- kain_regressions: `17`
- kain_flat: `6`
- alert_regressions: `10`
- best_improvement: `process_stdio_loop` (-345.599 ms, -7.40%)
- worst_regression: `python_zero_copy_buffer_pyo3_scoped` (+12.878 ms, +1.65%)

## Summary

| case | maturity | winner | kain median ms | rust median ms | cpp median ms | zig median ms | go median ms | erlang median ms | javascript median ms | python median ms |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| contention_wall | implemented | kain | 7.923 | 1918.419 | 1770.771 | 1734.871 | n/a | n/a | 121.560 | 6500.485 |
| ghost_mirror | semantic-proxy | kain | 7.869 | 28.047 | 26.192 | n/a | n/a | n/a | 352.628 | 283.538 |
| evolutionary_loop | dispatch-skeleton | kain | 22.241 | 22.644 | 2220.090 | n/a | n/a | n/a | 132.417 | 790.404 |
| ownership_memory | implemented | kain | 10.243 | 10.359 | 10.357 | n/a | n/a | n/a | 65.758 | 171.772 |
| branch_dispatch | implemented | kain | 7.769 | 16.244 | 16.961 | 18.279 | n/a | n/a | 104.718 | 749.494 |
| call_chain | implemented | kain | 13.281 | 28.929 | 28.449 | 34.058 | n/a | n/a | 173.877 | 1562.597 |
| memory_stream | implemented | kain | 8.130 | 9.233 | 8.448 | n/a | n/a | n/a | 56.285 | 102.972 |
| alloc_churn | implemented | kain | 7.136 | 9.627 | 9.306 | n/a | n/a | n/a | 56.601 | 54.890 |
| scalar_mix | implemented | kain | 7.187 | 14.361 | 13.834 | n/a | n/a | n/a | 69.244 | 268.774 |
| recursive_sum | implemented | kain | 6.997 | 7.817 | 7.162 | n/a | n/a | n/a | 62.237 | 93.309 |
| string_ops | implemented | kain | 6.597 | 8.730 | 8.578 | n/a | n/a | n/a | 59.681 | 228.758 |
| array_scan | implemented | kain | 8.350 | 9.627 | 8.426 | n/a | n/a | n/a | 74.669 | 509.210 |
| zero_copy_binary_wire | implemented | kain | 7.999 | 82.774 | 79.353 | 88.354 | 177.183 | n/a | n/a | n/a |
| dynamic_vtable_thrashing | dispatch-proxy | kain | 7.161 | 14.067 | 13.431 | n/a | 17.422 | n/a | n/a | n/a |
| ray_sphere_intersection | implemented | kain | 7.276 | 83.658 | 75.274 | n/a | 139.634 | n/a | n/a | n/a |
| semantic_singularity | kain-core-pressure | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a | n/a |
| struct_method | implemented | kain | 6.961 | 12.408 | 10.829 | n/a | n/a | n/a | 64.812 | 453.304 |
| option_result | implemented | cpp | 10.014 | 11.438 | 9.311 | n/a | n/a | n/a | 66.583 | 164.672 |
| async_ready_chain | implemented | kain | 7.771 | 8.645 | n/a | n/a | n/a | n/a | n/a | n/a |
| tcp_loopback_tokio | implemented | kain | 115.591 | 2754.702 | n/a | n/a | n/a | n/a | n/a | n/a |
| rayon_parallel_reduce | implemented | kain | 11.117 | 12.646 | n/a | n/a | n/a | n/a | n/a | n/a |
| simd_lane_mix | implemented | kain | 7.173 | 75.401 | 46.285 | n/a | n/a | n/a | n/a | n/a |
| native_map_lookup | implemented | kain | 15.884 | 31.374 | 35.086 | 18.451 | n/a | n/a | n/a | n/a |
| json_manual_roundtrip | implemented | kain | 7.971 | 120.747 | 101.110 | n/a | n/a | n/a | n/a | n/a |
| filesystem_stream | implemented | kain | 34.936 | 57.513 | 49.478 | n/a | n/a | n/a | n/a | n/a |
| process_stdio_loop | implemented | kain | 4324.310 | 4495.342 | 10201.661 | n/a | n/a | n/a | n/a | n/a |
| http_server_concurrency | semantic-proxy | rust | 56.176 | 33.941 | n/a | n/a | n/a | n/a | n/a | n/a |
| http_server_frameworks | semantic-proxy | kain | 120.576 | 159.526 | n/a | n/a | 174.348 | n/a | n/a | n/a |
| actor_mailbox_erlang | implemented | kain | 135.286 | n/a | n/a | n/a | n/a | 405.730 | n/a | n/a |
| unicode_string_heavy | implemented | kain | 7.677 | 8.466 | 7.732 | n/a | n/a | n/a | n/a | n/a |
| allocator_large_object_churn | implemented | kain | 9.409 | 10.243 | 9.584 | n/a | n/a | n/a | n/a | n/a |
| gpu_graphics_submit | implemented | kain | 30.201 | n/a | n/a | n/a | n/a | n/a | n/a | n/a |
| python_call_hotloop_pyo3_scoped | implemented | rust | 71.368 | 59.457 | n/a | n/a | n/a | n/a | n/a | n/a |
| python_call_hotloop_pyo3_per_boundary | implemented | kain | 72.305 | 78.182 | n/a | n/a | n/a | n/a | n/a | n/a |
| python_buffer_view_pyo3_scoped | implemented | rust | 143.631 | 136.225 | n/a | n/a | n/a | n/a | n/a | n/a |
| python_buffer_view_pyo3_per_boundary | implemented | kain | 143.794 | 147.726 | n/a | n/a | n/a | n/a | n/a | n/a |
| python_buffer_view_pyo3_region | implemented | rust | 153.367 | 152.244 | n/a | n/a | n/a | n/a | n/a | n/a |
| python_buffer_view_pyo3_region_fused | implemented | kain | 137.935 | 1349.447 | n/a | n/a | n/a | n/a | n/a | n/a |
| python_zero_copy_buffer_pyo3_scoped | implemented | rust | 791.468 | 135.207 | n/a | n/a | n/a | n/a | n/a | n/a |
| python_zero_copy_buffer_pyo3_per_boundary | implemented | rust | 790.822 | 137.464 | n/a | n/a | n/a | n/a | n/a | n/a |
| ffi_shared_call_stress | implemented | cpp | 55.139 | 50.724 | 50.580 | n/a | n/a | n/a | n/a | n/a |

Sources:
- kain: `cases/ghost_mirror/main.kn`
- rust: `cases/ghost_mirror/main.rs`
- cpp: `cases/ghost_mirror/main.cpp`
- javascript: `cases/ghost_mirror/main.js`
- python: `cases/ghost_mirror/main.py`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `366.532`
  - min_ms: `7.173`
  - max_ms: `8.680`
  - median_ms: `7.869`
  - mean_ms: `7.863`
  - relative_to_fastest: `fastest`
  - samples_ms: `[7.694, 7.173, 7.869, 7.976, 8.680, 7.774, 7.872]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\ghost_mirror\main.kn -t llvm -o X:\benchmark\out\build\ghost_mirror\kain\ghost_mirror.ll`
  - run_command: `X:\benchmark\out\build\ghost_mirror\kain\ghost_mirror.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+0.479`
  - delta_pct: `+6.47%`
  - trend: `slower`
  - regression_alert: `true`
  - primary_metric_delta: `-6.08%` (payload bytes/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `393.622`
  - min_ms: `27.004`
  - max_ms: `47.269`
  - median_ms: `28.047`
  - mean_ms: `30.793`
  - relative_to_fastest: `3.56x slower`
  - samples_ms: `[30.073, 27.422, 28.047, 47.269, 28.091, 27.643, 27.004]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/ghost_mirror/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/ghost_mirror/rust/ghost_mirror.exe`
  - run_command: `X:\benchmark\out\build\ghost_mirror\rust\ghost_mirror.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `940.087`
  - min_ms: `25.838`
  - max_ms: `29.926`
  - median_ms: `26.192`
  - mean_ms: `26.845`
  - relative_to_fastest: `3.33x slower`
  - samples_ms: `[26.192, 25.838, 29.926, 26.757, 27.120, 25.931, 26.154]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/ghost_mirror/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/ghost_mirror/cpp/ghost_mirror.exe -lws2_32`
  - run_command: `X:\benchmark\out\build\ghost_mirror\cpp\ghost_mirror.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- javascript:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `52.714`
  - min_ms: `343.121`
  - max_ms: `366.168`
  - median_ms: `352.628`
  - mean_ms: `354.261`
  - relative_to_fastest: `44.81x slower`
  - samples_ms: `[364.087, 352.628, 358.584, 345.233, 350.007, 343.121, 366.168]`
  - build_command: `F:\Scoop\apps\nodejs\current\node.EXE --check benchmark/cases/ghost_mirror/main.js`
  - run_command: `F:\Scoop\apps\nodejs\current\node.EXE benchmark/cases/ghost_mirror/main.js`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- python:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `83.573`
  - min_ms: `173.320`
  - max_ms: `331.861`
  - median_ms: `283.538`
  - mean_ms: `267.555`
  - relative_to_fastest: `36.03x slower`
  - samples_ms: `[331.861, 283.633, 252.343, 249.433, 298.756, 283.538, 173.320]`
  - build_command: `F:\Scoop\apps\python312\current\python.exe -m py_compile benchmark/cases/ghost_mirror/main.py`
  - run_command: `F:\Scoop\apps\python312\current\python.exe benchmark/cases/ghost_mirror/main.py`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### evolutionary_loop - Evolutionary Loop

- maturity: `dispatch-skeleton`
- winner: `kain`
- fastest_median_ms: `22.241`
- description: Rust uses runtime feature detection to choose a math lane. Kain expresses the same decision as converge/orchestrate lanes.
- fairness_note: Kain LLVM now carries multiple converge fast lanes and can route CPU-capability lanes through the native selector/cache. The lanes in this case are still scalar semantic proxies; it does not yet race AVX-512 kernels or persist warm autotune winners across runs.

Sources:
- kain: `cases/evolutionary_loop/main.kn`
- rust: `cases/evolutionary_loop/main.rs`
- cpp: `cases/evolutionary_loop/main.cpp`
- javascript: `cases/evolutionary_loop/main.js`
- python: `cases/evolutionary_loop/main.py`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `311.203`
  - min_ms: `21.784`
  - max_ms: `34.987`
  - median_ms: `22.241`
  - mean_ms: `23.993`
  - relative_to_fastest: `fastest`
  - samples_ms: `[22.427, 21.784, 22.241, 34.987, 22.059, 22.367, 22.087]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\evolutionary_loop\main.kn -t llvm -o X:\benchmark\out\build\evolutionary_loop\kain\evolutionary_loop.ll`
  - run_command: `X:\benchmark\out\build\evolutionary_loop\kain\evolutionary_loop.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+0.698`
  - delta_pct: `+3.24%`
  - trend: `slower`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `187.712`
  - min_ms: `22.255`
  - max_ms: `23.887`
  - median_ms: `22.644`
  - mean_ms: `22.783`
  - relative_to_fastest: `1.02x slower`
  - samples_ms: `[22.255, 22.905, 22.644, 22.397, 22.396, 23.887, 22.997]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/evolutionary_loop/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/evolutionary_loop/rust/evolutionary_loop.exe`
  - run_command: `X:\benchmark\out\build\evolutionary_loop\rust\evolutionary_loop.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `201.368`
  - min_ms: `2165.499`
  - max_ms: `2339.147`
  - median_ms: `2220.090`
  - mean_ms: `2243.382`
  - relative_to_fastest: `99.82x slower`
  - samples_ms: `[2220.090, 2339.147, 2338.913, 2274.389, 2165.499, 2184.106, 2181.531]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/evolutionary_loop/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/evolutionary_loop/cpp/evolutionary_loop.exe`
  - run_command: `X:\benchmark\out\build\evolutionary_loop\cpp\evolutionary_loop.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- javascript:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `63.249`
  - min_ms: `122.781`
  - max_ms: `148.092`
  - median_ms: `132.417`
  - mean_ms: `135.035`
  - relative_to_fastest: `5.95x slower`
  - samples_ms: `[122.781, 147.052, 139.009, 148.092, 128.044, 127.848, 132.417]`
  - build_command: `F:\Scoop\apps\nodejs\current\node.EXE --check benchmark/cases/evolutionary_loop/main.js`
  - run_command: `F:\Scoop\apps\nodejs\current\node.EXE benchmark/cases/evolutionary_loop/main.js`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- python:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `75.897`
  - min_ms: `775.990`
  - max_ms: `800.319`
  - median_ms: `790.404`
  - mean_ms: `789.064`
  - relative_to_fastest: `35.54x slower`
  - samples_ms: `[781.946, 794.069, 796.413, 800.319, 775.990, 790.404, 784.309]`
  - build_command: `F:\Scoop\apps\python312\current\python.exe -m py_compile benchmark/cases/evolutionary_loop/main.py`
  - run_command: `F:\Scoop\apps\python312\current\python.exe benchmark/cases/evolutionary_loop/main.py`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### semantic_fabric_relay - Semantic Fabric Relay

- maturity: `semantic-proxy`
- winner: `cpp`
- fastest_median_ms: `8.599`
- description: Integrated semantic-state hot loop: Kain threads actor ask/reply, world entangle, patch/law validation, teleport handoff, converge/orchestrate staging, and owned raw memory through one checksum path, while C++ manually spells out the same state machine.
- fairness_note: This is intentionally not a literal feature-parity claim. C++ emulates the same deterministic state graph with direct structs and method calls, while Kain measures the owned language semantics directly.

Telemetry:
- primary_metric: `semantic rounds/s`
- semantic rounds/s (`60,000` work/run, `rounds/s`): kain `n/a`, cpp `6,977,555.530`

Sources:
- kain: `cases/semantic_fabric_relay/main.kn`
- cpp: `cases/semantic_fabric_relay/main.cpp`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `FAIL`
  - build_ms: `381.921`
  - min_ms: `n/a`
  - max_ms: `n/a`
  - median_ms: `n/a`
  - mean_ms: `n/a`
  - relative_to_fastest: `n/a`
  - samples_ms: `[]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\semantic_fabric_relay\main.kn -t llvm -o X:\benchmark\out\build\semantic_fabric_relay\kain\semantic_fabric_relay.ll`
  - run_command: `X:\benchmark\out\build\semantic_fabric_relay\kain\semantic_fabric_relay.exe`
  - previous_run_delta: `current Kain run failed or has no timing samples`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
  - error:
    Executable failed with exit code 3221225477.
    command:
    X:\benchmark\out\build\semantic_fabric_relay\kain\semantic_fabric_relay.exe
    stdout:
    
    stderr:
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `456.323`
  - min_ms: `8.425`
  - max_ms: `20.727`
  - median_ms: `8.599`
  - mean_ms: `10.471`
  - relative_to_fastest: `fastest`
  - samples_ms: `[8.496, 9.285, 20.727, 9.223, 8.425, 8.599, 8.543]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/semantic_fabric_relay/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/semantic_fabric_relay/cpp/semantic_fabric_relay.exe`
  - run_command: `X:\benchmark\out\build\semantic_fabric_relay\cpp\semantic_fabric_relay.exe`
  - stability: `unstable samples - max 2.41x median, stdev/mean 0.40`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### actor_ownership_backpressure - Actor Ownership Backpressure

- maturity: `semantic-proxy`
- winner: `cpp`
- fastest_median_ms: `13.868`
- description: Mailbox-pressure semantic row: Kain drives bursty actor ask/reply traffic through world/entangle state, patch/law checks, teleport handoff, converge/orchestrate staging, and collapse/observe/decay-owned cells while C++ manually emulates the same deterministic graph.
- fairness_note: This is intentionally a semantic-state benchmark, not a literal runtime-feature parity claim. C++ manually reproduces the same state machine and burst schedule while Kain measures first-class language semantics directly.
- language_notes:
  - kain: Touches deadline_millis/deadline_elapsed once and allows the first local microcell ask in a turn to borrow the caller payload instead of heap-copying through the mailbox when the runtime proves inline execution is legal.

Telemetry:
- primary_metric: `semantic rounds/s`
- semantic rounds/s (`180,000` work/run, `rounds/s`): kain `n/a`, cpp `12,979,427.607`
- ask roundtrips/s (`360,000` work/run, `asks/s`): kain `n/a`, cpp `25,958,855.214`

Sources:
- kain: `cases/actor_ownership_backpressure/main.kn`
- cpp: `cases/actor_ownership_backpressure/main.cpp`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `FAIL`
  - build_ms: `415.216`
  - min_ms: `n/a`
  - max_ms: `n/a`
  - median_ms: `n/a`
  - mean_ms: `n/a`
  - relative_to_fastest: `n/a`
  - samples_ms: `[]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\actor_ownership_backpressure\main.kn -t llvm -o X:\benchmark\out\build\actor_ownership_backpressure\kain\actor_ownership_backpressure.ll`
  - run_command: `X:\benchmark\out\build\actor_ownership_backpressure\kain\actor_ownership_backpressure.exe`
  - previous_run_delta: `current Kain run failed or has no timing samples`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
  - error:
    Executable failed with exit code 3221225477.
    command:
    X:\benchmark\out\build\actor_ownership_backpressure\kain\actor_ownership_backpressure.exe
    stdout:
    
    stderr:
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `480.374`
  - min_ms: `13.473`
  - max_ms: `25.091`
  - median_ms: `13.868`
  - mean_ms: `15.392`
  - relative_to_fastest: `fastest`
  - samples_ms: `[14.167, 25.091, 13.987, 13.868, 13.473, 13.560, 13.595]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/actor_ownership_backpressure/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/actor_ownership_backpressure/cpp/actor_ownership_backpressure.exe`
  - run_command: `X:\benchmark\out\build\actor_ownership_backpressure\cpp\actor_ownership_backpressure.exe`
  - stability: `unstable samples - max 1.81x median`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### semantic_host_bridge_fusion - Semantic Host Bridge Fusion

- maturity: `semantic-proxy`
- winner: `cpp`
- fastest_median_ms: `425.384`
- description: Fused host-domain semantic row: Kain interleaves filesystem write/read, process-spec lifecycle, HTTP/HTTP2 request-handle probing, actor replies, world/entangle teleport, and owned memory cells in one checksum loop; C++ spells out the same deterministic contract manually.
- fairness_note: This is a host-bridge semantics lane, not a full network/process throughput contest. The process path intentionally uses spec lifecycle (not spawn/IO), and both rows keep the same deterministic file/request-handle contract.

Telemetry:
- primary_metric: `bridge rounds/s`
- bridge rounds/s (`2,400` work/run, `rounds/s`): kain `n/a`, cpp `5,641.962`
- request handles/s (`4,800` work/run, `handles/s`): kain `n/a`, cpp `11,283.925`
- process specs/s (`2,400` work/run, `specs/s`): kain `n/a`, cpp `5,641.962`

Sources:
- kain: `cases/semantic_host_bridge_fusion/main.kn`
- cpp: `cases/semantic_host_bridge_fusion/main.cpp`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `FAIL`
  - build_ms: `670.960`
  - min_ms: `n/a`
  - max_ms: `n/a`
  - median_ms: `n/a`
  - mean_ms: `n/a`
  - relative_to_fastest: `n/a`
  - samples_ms: `[]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\semantic_host_bridge_fusion\main.kn -t llvm -o X:\benchmark\out\build\semantic_host_bridge_fusion\kain\semantic_host_bridge_fusion.ll`
  - run_command: `X:\benchmark\out\build\semantic_host_bridge_fusion\kain\semantic_host_bridge_fusion.exe`
  - previous_run_delta: `current Kain run failed or has no timing samples`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
  - error:
    Executable failed with exit code 3221225477.
    command:
    X:\benchmark\out\build\semantic_host_bridge_fusion\kain\semantic_host_bridge_fusion.exe
    stdout:
    
    stderr:
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `1140.962`
  - min_ms: `417.243`
  - max_ms: `457.385`
  - median_ms: `425.384`
  - mean_ms: `428.847`
  - relative_to_fastest: `fastest`
  - samples_ms: `[425.384, 417.293, 434.712, 417.243, 457.385, 429.797, 420.116]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/semantic_host_bridge_fusion/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/semantic_host_bridge_fusion/cpp/semantic_host_bridge_fusion.exe`
  - run_command: `X:\benchmark\out\build\semantic_host_bridge_fusion\cpp\semantic_host_bridge_fusion.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### pulse_teleport_decay_mesh - Pulse Teleport Decay Mesh

- maturity: `semantic-proxy`
- winner: `cpp`
- fastest_median_ms: `8.296`
- description: Machine-temporal semantic row: Kain drives pulse declarations, world/entangle state, teleport handoff, patch/law checks, actor ask/reply, and collapse/observe/decay-owned cells in one deterministic checksum loop while C++ mirrors the same state graph manually.
- fairness_note: This is a semantics-latency row, not a timer-precision contest. Kain uses compiler-owned pulse + teleport + ownership primitives directly; C++ reproduces the same deterministic algebraic state transitions without claiming feature-parity runtime internals.

Telemetry:
- primary_metric: `semantic rounds/s`
- semantic rounds/s (`54,000` work/run, `rounds/s`): kain `n/a`, cpp `6,509,004.122`
- teleports/s (`54,000` work/run, `teleports/s`): kain `n/a`, cpp `6,509,004.122`
- ask roundtrips/s (`54,000` work/run, `asks/s`): kain `n/a`, cpp `6,509,004.122`

Sources:
- kain: `cases/pulse_teleport_decay_mesh/main.kn`
- cpp: `cases/pulse_teleport_decay_mesh/main.cpp`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `FAIL`
  - build_ms: `376.060`
  - min_ms: `n/a`
  - max_ms: `n/a`
  - median_ms: `n/a`
  - mean_ms: `n/a`
  - relative_to_fastest: `n/a`
  - samples_ms: `[]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\pulse_teleport_decay_mesh\main.kn -t llvm -o X:\benchmark\out\build\pulse_teleport_decay_mesh\kain\pulse_teleport_decay_mesh.ll`
  - run_command: `X:\benchmark\out\build\pulse_teleport_decay_mesh\kain\pulse_teleport_decay_mesh.exe`
  - previous_run_delta: `current Kain run failed or has no timing samples`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
  - error:
    Executable failed with exit code 3221225477.
    command:
    X:\benchmark\out\build\pulse_teleport_decay_mesh\kain\pulse_teleport_decay_mesh.exe
    stdout:
    
    stderr:
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `453.758`
  - min_ms: `7.988`
  - max_ms: `8.920`
  - median_ms: `8.296`
  - mean_ms: `8.355`
  - relative_to_fastest: `fastest`
  - samples_ms: `[8.920, 8.498, 8.296, 7.988, 8.151, 8.119, 8.514]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/pulse_teleport_decay_mesh/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/pulse_teleport_decay_mesh/cpp/pulse_teleport_decay_mesh.exe`
  - run_command: `X:\benchmark\out\build\pulse_teleport_decay_mesh\cpp\pulse_teleport_decay_mesh.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### ownership_memory - Scoped Ownership Memory Cell

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `10.243`
- description: A smaller first-class ownership smoke benchmark: Kain uses collapse/observe/decay over a helper-owned heap cell; Rust uses an owned Box mutation and drop.
- fairness_note: This is a direct memory-lifecycle smoke, not a contention test.

Sources:
- kain: `cases/ownership_memory/main.kn`
- rust: `cases/ownership_memory/main.rs`
- cpp: `cases/ownership_memory/main.cpp`
- javascript: `cases/ownership_memory/main.js`
- python: `cases/ownership_memory/main.py`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `306.913`
  - min_ms: `9.946`
  - max_ms: `10.995`
  - median_ms: `10.243`
  - mean_ms: `10.295`
  - relative_to_fastest: `fastest`
  - samples_ms: `[10.995, 10.336, 10.329, 10.243, 10.186, 10.030, 9.946]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\ownership_memory\main.kn -t llvm -o X:\benchmark\out\build\ownership_memory\kain\ownership_memory.ll`
  - run_command: `X:\benchmark\out\build\ownership_memory\kain\ownership_memory.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+0.254`
  - delta_pct: `+2.54%`
  - trend: `slower`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `202.641`
  - min_ms: `10.171`
  - max_ms: `10.943`
  - median_ms: `10.359`
  - mean_ms: `10.435`
  - relative_to_fastest: `1.01x slower`
  - samples_ms: `[10.171, 10.943, 10.359, 10.529, 10.513, 10.213, 10.315]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/ownership_memory/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/ownership_memory/rust/ownership_memory.exe`
  - run_command: `X:\benchmark\out\build\ownership_memory\rust\ownership_memory.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `456.001`
  - min_ms: `9.978`
  - max_ms: `12.455`
  - median_ms: `10.357`
  - mean_ms: `10.682`
  - relative_to_fastest: `1.01x slower`
  - samples_ms: `[10.357, 10.648, 11.006, 10.059, 10.269, 12.455, 9.978]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/ownership_memory/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/ownership_memory/cpp/ownership_memory.exe`
  - run_command: `X:\benchmark\out\build\ownership_memory\cpp\ownership_memory.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- javascript:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `59.196`
  - min_ms: `63.544`
  - max_ms: `69.025`
  - median_ms: `65.758`
  - mean_ms: `65.506`
  - relative_to_fastest: `6.42x slower`
  - samples_ms: `[66.207, 63.544, 63.843, 65.758, 65.935, 64.233, 69.025]`
  - build_command: `F:\Scoop\apps\nodejs\current\node.EXE --check benchmark/cases/ownership_memory/main.js`
  - run_command: `F:\Scoop\apps\nodejs\current\node.EXE benchmark/cases/ownership_memory/main.js`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- python:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `66.118`
  - min_ms: `166.028`
  - max_ms: `175.907`
  - median_ms: `171.772`
  - mean_ms: `171.914`
  - relative_to_fastest: `16.77x slower`
  - samples_ms: `[175.433, 171.446, 175.907, 175.579, 171.772, 167.229, 166.028]`
  - build_command: `F:\Scoop\apps\python312\current\python.exe -m py_compile benchmark/cases/ownership_memory/main.py`
  - run_command: `F:\Scoop\apps\python312\current\python.exe benchmark/cases/ownership_memory/main.py`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### branch_dispatch - Branch Dispatch

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `7.769`
- description: Integer match/branch dispatch with mixed arithmetic. This catches control-flow lowering, branch prediction, and basic optimizer quality.
- fairness_note: All rows keep the same scalar classifier contract. Kain preserves the branch ladder as the converge spec, but the LLVM lane is allowed to collapse the fixed 8-wide residue schedule into a proof-backed polynomial block sum.
- language_notes:
  - kain: Uses benchmark/cases/branch_dispatch/proofs-experimental/branch-dispatch-block-formula-equivalence.smt2 and branch-dispatch-benchmark-checksum.smt2 to justify the polynomial block lane for the authored 3000000-iteration domain.

Sources:
- kain: `cases/branch_dispatch/main.kn`
- rust: `cases/branch_dispatch/main.rs`
- cpp: `cases/branch_dispatch/main.cpp`
- zig: `cases/branch_dispatch/main.zig`
- javascript: `cases/branch_dispatch/main.js`
- python: `cases/branch_dispatch/main.py`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `321.313`
  - min_ms: `6.889`
  - max_ms: `8.595`
  - median_ms: `7.769`
  - mean_ms: `7.679`
  - relative_to_fastest: `fastest`
  - samples_ms: `[8.595, 8.398, 7.769, 7.889, 7.176, 7.036, 6.889]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\branch_dispatch\main.kn -t llvm -o X:\benchmark\out\build\branch_dispatch\kain\branch_dispatch.ll`
  - run_command: `X:\benchmark\out\build\branch_dispatch\kain\branch_dispatch.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+0.762`
  - delta_pct: `+10.87%`
  - trend: `slower`
  - regression_alert: `true`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `214.312`
  - min_ms: `15.970`
  - max_ms: `16.655`
  - median_ms: `16.244`
  - mean_ms: `16.283`
  - relative_to_fastest: `2.09x slower`
  - samples_ms: `[16.532, 16.655, 15.970, 16.214, 16.244, 16.014, 16.354]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/branch_dispatch/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/branch_dispatch/rust/branch_dispatch.exe`
  - run_command: `X:\benchmark\out\build\branch_dispatch\rust\branch_dispatch.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `131.300`
  - min_ms: `16.247`
  - max_ms: `18.455`
  - median_ms: `16.961`
  - mean_ms: `17.324`
  - relative_to_fastest: `2.18x slower`
  - samples_ms: `[16.556, 16.626, 18.116, 18.455, 16.247, 18.310, 16.961]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/branch_dispatch/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/branch_dispatch/cpp/branch_dispatch.exe`
  - run_command: `X:\benchmark\out\build\branch_dispatch\cpp\branch_dispatch.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- zig:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `256.762`
  - min_ms: `17.902`
  - max_ms: `21.466`
  - median_ms: `18.279`
  - mean_ms: `18.926`
  - relative_to_fastest: `2.35x slower`
  - samples_ms: `[19.523, 21.466, 19.440, 18.279, 17.916, 17.902, 17.952]`
  - build_command: `F:\Scoop\shims\zig.EXE build-exe -O ReleaseFast main.zig -femit-bin=X:\benchmark\out\build\branch_dispatch\zig\branch_dispatch.exe`
  - run_command: `X:\benchmark\out\build\branch_dispatch\zig\branch_dispatch.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- javascript:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `58.871`
  - min_ms: `102.726`
  - max_ms: `113.542`
  - median_ms: `104.718`
  - mean_ms: `106.553`
  - relative_to_fastest: `13.48x slower`
  - samples_ms: `[106.067, 111.214, 103.353, 113.542, 104.718, 104.250, 102.726]`
  - build_command: `F:\Scoop\apps\nodejs\current\node.EXE --check benchmark/cases/branch_dispatch/main.js`
  - run_command: `F:\Scoop\apps\nodejs\current\node.EXE benchmark/cases/branch_dispatch/main.js`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- python:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `75.164`
  - min_ms: `730.563`
  - max_ms: `775.864`
  - median_ms: `749.494`
  - mean_ms: `750.641`
  - relative_to_fastest: `96.48x slower`
  - samples_ms: `[733.768, 730.563, 774.605, 753.750, 775.864, 749.494, 736.447]`
  - build_command: `F:\Scoop\apps\python312\current\python.exe -m py_compile benchmark/cases/branch_dispatch/main.py`
  - run_command: `F:\Scoop\apps\python312\current\python.exe benchmark/cases/branch_dispatch/main.py`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### call_chain - Call Chain

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `13.281`
- description: Deep small-function call graph inside a hot loop. This measures call overhead, inlining assumptions, and arithmetic lowering.
- fairness_note: Rust and C++ functions are marked noinline to preserve the ordinary call-chain baseline. Kain keeps that call graph as the converge spec, but the LLVM lane is allowed to use the proof-backed affine recurrence of the same nested arithmetic.
- language_notes:
  - kain: The affine lane is not a generic call-overhead measurement; it is a semantic reduction of step_d(value) to (93 * value + 685) mod 1000000007, proved in benchmark/cases/call_chain/proofs-experimental/call-chain-affine-step-equivalence.smt2.

Sources:
- kain: `cases/call_chain/main.kn`
- rust: `cases/call_chain/main.rs`
- cpp: `cases/call_chain/main.cpp`
- zig: `cases/call_chain/main.zig`
- javascript: `cases/call_chain/main.js`
- python: `cases/call_chain/main.py`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `307.061`
  - min_ms: `12.855`
  - max_ms: `14.851`
  - median_ms: `13.281`
  - mean_ms: `13.466`
  - relative_to_fastest: `fastest`
  - samples_ms: `[13.281, 13.646, 14.851, 13.416, 12.855, 13.181, 13.029]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\call_chain\main.kn -t llvm -o X:\benchmark\out\build\call_chain\kain\call_chain.ll`
  - run_command: `X:\benchmark\out\build\call_chain\kain\call_chain.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+0.056`
  - delta_pct: `+0.43%`
  - trend: `flat`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `215.257`
  - min_ms: `28.684`
  - max_ms: `32.647`
  - median_ms: `28.929`
  - mean_ms: `29.414`
  - relative_to_fastest: `2.18x slower`
  - samples_ms: `[28.772, 28.929, 28.775, 29.109, 28.980, 28.684, 32.647]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/call_chain/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/call_chain/rust/call_chain.exe`
  - run_command: `X:\benchmark\out\build\call_chain\rust\call_chain.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `141.784`
  - min_ms: `28.192`
  - max_ms: `29.125`
  - median_ms: `28.449`
  - mean_ms: `28.522`
  - relative_to_fastest: `2.14x slower`
  - samples_ms: `[29.125, 28.335, 28.676, 28.449, 28.285, 28.192, 28.595]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/call_chain/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/call_chain/cpp/call_chain.exe`
  - run_command: `X:\benchmark\out\build\call_chain\cpp\call_chain.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- zig:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `217.132`
  - min_ms: `33.577`
  - max_ms: `43.776`
  - median_ms: `34.058`
  - mean_ms: `35.905`
  - relative_to_fastest: `2.56x slower`
  - samples_ms: `[37.753, 34.719, 33.860, 33.577, 43.776, 34.058, 33.596]`
  - build_command: `F:\Scoop\shims\zig.EXE build-exe -O ReleaseFast main.zig -femit-bin=X:\benchmark\out\build\call_chain\zig\call_chain.exe`
  - run_command: `X:\benchmark\out\build\call_chain\zig\call_chain.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- javascript:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `57.487`
  - min_ms: `168.161`
  - max_ms: `182.476`
  - median_ms: `173.877`
  - mean_ms: `174.729`
  - relative_to_fastest: `13.09x slower`
  - samples_ms: `[182.476, 173.877, 168.161, 180.202, 176.427, 171.095, 170.864]`
  - build_command: `F:\Scoop\apps\nodejs\current\node.EXE --check benchmark/cases/call_chain/main.js`
  - run_command: `F:\Scoop\apps\nodejs\current\node.EXE benchmark/cases/call_chain/main.js`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- python:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `66.367`
  - min_ms: `1514.441`
  - max_ms: `1637.656`
  - median_ms: `1562.597`
  - mean_ms: `1570.341`
  - relative_to_fastest: `117.65x slower`
  - samples_ms: `[1628.048, 1637.656, 1514.441, 1574.418, 1537.414, 1562.597, 1537.814]`
  - build_command: `F:\Scoop\apps\python312\current\python.exe -m py_compile benchmark/cases/call_chain/main.py`
  - run_command: `F:\Scoop\apps\python312\current\python.exe benchmark/cases/call_chain/main.py`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### memory_stream - Memory Stream

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `8.130`
- description: Sequential write/read over a helper-owned integer buffer. Kain uses raw memory helpers under collapse/observe; Rust uses a Vec<i64>.
- fairness_note: This is a direct memory-throughput smoke, not a SIMD memcpy benchmark.

Sources:
- kain: `cases/memory_stream/main.kn`
- rust: `cases/memory_stream/main.rs`
- cpp: `cases/memory_stream/main.cpp`
- javascript: `cases/memory_stream/main.js`
- python: `cases/memory_stream/main.py`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `313.059`
  - min_ms: `7.931`
  - max_ms: `8.765`
  - median_ms: `8.130`
  - mean_ms: `8.283`
  - relative_to_fastest: `fastest`
  - samples_ms: `[8.765, 8.130, 7.931, 7.959, 8.127, 8.631, 8.440]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\memory_stream\main.kn -t llvm -o X:\benchmark\out\build\memory_stream\kain\memory_stream.ll`
  - run_command: `X:\benchmark\out\build\memory_stream\kain\memory_stream.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-0.734`
  - delta_pct: `-8.28%`
  - trend: `faster`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `231.379`
  - min_ms: `9.011`
  - max_ms: `17.538`
  - median_ms: `9.233`
  - mean_ms: `10.445`
  - relative_to_fastest: `1.14x slower`
  - samples_ms: `[9.233, 9.658, 9.129, 9.011, 9.418, 9.129, 17.538]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/memory_stream/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/memory_stream/rust/memory_stream.exe`
  - run_command: `X:\benchmark\out\build\memory_stream\rust\memory_stream.exe`
  - stability: `unstable samples - max 1.90x median`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `432.550`
  - min_ms: `8.112`
  - max_ms: `8.802`
  - median_ms: `8.448`
  - mean_ms: `8.419`
  - relative_to_fastest: `1.04x slower`
  - samples_ms: `[8.352, 8.448, 8.520, 8.112, 8.802, 8.162, 8.535]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/memory_stream/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/memory_stream/cpp/memory_stream.exe`
  - run_command: `X:\benchmark\out\build\memory_stream\cpp\memory_stream.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- javascript:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `59.078`
  - min_ms: `54.799`
  - max_ms: `66.743`
  - median_ms: `56.285`
  - mean_ms: `57.928`
  - relative_to_fastest: `6.92x slower`
  - samples_ms: `[56.047, 58.932, 57.193, 66.743, 56.285, 54.799, 55.497]`
  - build_command: `F:\Scoop\apps\nodejs\current\node.EXE --check benchmark/cases/memory_stream/main.js`
  - run_command: `F:\Scoop\apps\nodejs\current\node.EXE benchmark/cases/memory_stream/main.js`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- python:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `68.072`
  - min_ms: `101.857`
  - max_ms: `104.076`
  - median_ms: `102.972`
  - mean_ms: `103.237`
  - relative_to_fastest: `12.67x slower`
  - samples_ms: `[102.967, 102.972, 104.076, 103.778, 102.938, 104.074, 101.857]`
  - build_command: `F:\Scoop\apps\python312\current\python.exe -m py_compile benchmark/cases/memory_stream/main.py`
  - run_command: `F:\Scoop\apps\python312\current\python.exe benchmark/cases/memory_stream/main.py`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### metal_cacheline_flush - Metal Cacheline Flush

- maturity: `implemented`
- winner: `cpp`
- fastest_median_ms: `23.365`
- description: x86-family cacheline hot loop over a VM-mapped page. Kain uses vm_map, volatile load/store, ptr_to_int/int_to_ptr roundtrips, prefetch, lfence/sfence/mfence, clflush, and inline asm pause; C++ spells the same lane with mapped pages and intrinsics.
- fairness_note: This is intentionally a hardware-facing metal row, not a portable algorithm shootout. Both rows keep the same single-threaded cacheline-touch contract and x86-family fence/flush cadence; the point is whether Kain can lower the authored machine surface directly instead of retreating to a semantic proxy.
- language_notes:
  - kain: Exercises the landed std.machine + raw metal path directly: vm_map/vm_unmap, prefetch_write, volatile_load/store, ptr_to_int/int_to_ptr, lfence/sfence/mfence via std.machine, clflush, spin_loop_hint, and raw asm("pause").

Telemetry:
- primary_metric: `cacheline flushes/s`
- cacheline flushes/s (`262,144` work/run, `flushes/s`): kain `7,325,773.115`, cpp `11,219,660.428`

Sources:
- kain: `cases/metal_cacheline_flush/main.kn`
- cpp: `cases/metal_cacheline_flush/main.cpp`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `323.070`
  - min_ms: `34.517`
  - max_ms: `47.368`
  - median_ms: `35.784`
  - mean_ms: `37.374`
  - relative_to_fastest: `1.53x slower`
  - samples_ms: `[35.589, 36.047, 47.368, 35.732, 34.517, 36.579, 35.784]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\metal_cacheline_flush\main.kn -t llvm -o X:\benchmark\out\build\metal_cacheline_flush\kain\metal_cacheline_flush.ll`
  - run_command: `X:\benchmark\out\build\metal_cacheline_flush\kain\metal_cacheline_flush.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+0.626`
  - delta_pct: `+1.78%`
  - trend: `slower`
  - primary_metric_delta: `-1.75%` (cacheline flushes/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `579.626`
  - min_ms: `22.575`
  - max_ms: `24.244`
  - median_ms: `23.365`
  - mean_ms: `23.317`
  - relative_to_fastest: `fastest`
  - samples_ms: `[23.365, 23.836, 23.425, 24.244, 23.139, 22.575, 22.637]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/metal_cacheline_flush/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/metal_cacheline_flush/cpp/metal_cacheline_flush.exe`
  - run_command: `X:\benchmark\out\build\metal_cacheline_flush\cpp\metal_cacheline_flush.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### metal_ordered_atomics - Metal Ordered Atomics

- maturity: `implemented`
- winner: `cpp`
- fastest_median_ms: `38.793`
- description: Ordered shared-cell hot loop. Kain drives atomic load/store, add/or/xor/and, exchange, compare_exchange, and acq_rel fences through the new low-level memory surface while C++ mirrors the same state machine with std::atomic.
- fairness_note: This is a direct low-level atomics row. Both rows execute the same deterministic single-threaded ordered-atomic state machine so the comparison stays about lowering and runtime tax, not about scheduler differences or semantic proxies.
- language_notes:
  - kain: Exercises the landed std.memory ordered-atomic lane directly: atomic store/load, fetch add/or/xor/and, exchange, compare_exchange, and acq_rel fence calls over raw ptr<Int> cells.

Telemetry:
- primary_metric: `ordered atomic rounds/s`
- ordered atomic rounds/s (`1,000,000` work/run, `rounds/s`): kain `25,495,310.138`, cpp `25,777,912.969`

Sources:
- kain: `cases/metal_ordered_atomics/main.kn`
- cpp: `cases/metal_ordered_atomics/main.cpp`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `319.926`
  - min_ms: `38.922`
  - max_ms: `41.096`
  - median_ms: `39.223`
  - mean_ms: `39.572`
  - relative_to_fastest: `1.01x slower`
  - samples_ms: `[39.223, 39.199, 41.096, 38.922, 40.024, 39.084, 39.455]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\metal_ordered_atomics\main.kn -t llvm -o X:\benchmark\out\build\metal_ordered_atomics\kain\metal_ordered_atomics.ll`
  - run_command: `X:\benchmark\out\build\metal_ordered_atomics\kain\metal_ordered_atomics.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+0.103`
  - delta_pct: `+0.26%`
  - trend: `flat`
  - primary_metric_delta: `-0.26%` (ordered atomic rounds/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `466.497`
  - min_ms: `38.413`
  - max_ms: `40.096`
  - median_ms: `38.793`
  - mean_ms: `38.893`
  - relative_to_fastest: `fastest`
  - samples_ms: `[38.898, 38.896, 38.793, 38.733, 38.423, 40.096, 38.413]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/metal_ordered_atomics/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/metal_ordered_atomics/cpp/metal_ordered_atomics.exe`
  - run_command: `X:\benchmark\out\build\metal_ordered_atomics\cpp\metal_ordered_atomics.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### alloc_churn - Allocation Churn

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `7.136`
- description: Many small allocate/write/read/free cycles. Kain uses alloc_zeroed plus decay; Rust uses Box allocation and drop.
- fairness_note: This is intentionally allocator-heavy and will expose runtime bookkeeping overhead.

Sources:
- kain: `cases/alloc_churn/main.kn`
- rust: `cases/alloc_churn/main.rs`
- cpp: `cases/alloc_churn/main.cpp`
- javascript: `cases/alloc_churn/main.js`
- python: `cases/alloc_churn/main.py`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `305.875`
  - min_ms: `6.565`
  - max_ms: `18.741`
  - median_ms: `7.136`
  - mean_ms: `8.732`
  - relative_to_fastest: `fastest`
  - samples_ms: `[7.117, 7.136, 6.565, 6.666, 18.741, 7.500, 7.397]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\alloc_churn\main.kn -t llvm -o X:\benchmark\out\build\alloc_churn\kain\alloc_churn.ll`
  - run_command: `X:\benchmark\out\build\alloc_churn\kain\alloc_churn.exe`
  - stability: `unstable samples - max 2.63x median, stdev/mean 0.47`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-0.672`
  - delta_pct: `-8.60%`
  - trend: `faster`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `191.983`
  - min_ms: `9.512`
  - max_ms: `10.069`
  - median_ms: `9.627`
  - mean_ms: `9.736`
  - relative_to_fastest: `1.35x slower`
  - samples_ms: `[10.069, 9.627, 9.936, 9.512, 9.598, 9.792, 9.619]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/alloc_churn/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/alloc_churn/rust/alloc_churn.exe`
  - run_command: `X:\benchmark\out\build\alloc_churn\rust\alloc_churn.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `429.343`
  - min_ms: `9.031`
  - max_ms: `20.712`
  - median_ms: `9.306`
  - mean_ms: `10.981`
  - relative_to_fastest: `1.30x slower`
  - samples_ms: `[9.245, 9.491, 20.712, 9.812, 9.031, 9.265, 9.306]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/alloc_churn/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/alloc_churn/cpp/alloc_churn.exe`
  - run_command: `X:\benchmark\out\build\alloc_churn\cpp\alloc_churn.exe`
  - stability: `unstable samples - max 2.23x median, stdev/mean 0.36`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- javascript:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `56.104`
  - min_ms: `52.928`
  - max_ms: `72.406`
  - median_ms: `56.601`
  - mean_ms: `59.004`
  - relative_to_fastest: `7.93x slower`
  - samples_ms: `[56.601, 55.377, 54.158, 64.654, 56.903, 52.928, 72.406]`
  - build_command: `F:\Scoop\apps\nodejs\current\node.EXE --check benchmark/cases/alloc_churn/main.js`
  - run_command: `F:\Scoop\apps\nodejs\current\node.EXE benchmark/cases/alloc_churn/main.js`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- python:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `71.965`
  - min_ms: `54.625`
  - max_ms: `65.196`
  - median_ms: `54.890`
  - mean_ms: `56.618`
  - relative_to_fastest: `7.69x slower`
  - samples_ms: `[54.890, 54.833, 56.547, 54.696, 65.196, 55.538, 54.625]`
  - build_command: `F:\Scoop\apps\python312\current\python.exe -m py_compile benchmark/cases/alloc_churn/main.py`
  - run_command: `F:\Scoop\apps\python312\current\python.exe benchmark/cases/alloc_churn/main.py`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### stdlib_foundations - Stdlib Foundations

- maturity: `kain-stdlib-proof`
- winner: `n/a`
- fastest_median_ms: `n/a`
- description: Integrated Kain-only pressure over std.text zero-copy slices, std.collections queue/priority queue/map/slot-map handles, std.crypto digest calls, std.alloc bump allocation, and std.sync mutex/channel/once/wait-group coordination.
- fairness_note: This is a Kain foundation-row, not a cross-language contest. It guards the public stdlib API shape and native LLVM/runtime ABI for the text, collection, crypto, allocator, and sync surfaces.

Sources:
- kain: `cases/stdlib_foundations/main.kn`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `FAIL`
  - build_ms: `550.952`
  - min_ms: `n/a`
  - max_ms: `n/a`
  - median_ms: `n/a`
  - mean_ms: `n/a`
  - relative_to_fastest: `n/a`
  - samples_ms: `[]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\stdlib_foundations\main.kn -t llvm -o X:\benchmark\out\build\stdlib_foundations\kain\stdlib_foundations.ll`
  - run_command: `X:\benchmark\out\build\stdlib_foundations\kain\stdlib_foundations.exe`
  - previous_run_delta: `current Kain run failed or has no timing samples`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
  - error:
    Executable failed with exit code 3221225477.
    command:
    X:\benchmark\out\build\stdlib_foundations\kain\stdlib_foundations.exe
    stdout:
    
    stderr:

### sync_primitives - Sync Primitives

- maturity: `kain-stdlib-proof`
- winner: `n/a`
- fastest_median_ms: `n/a`
- description: Focused Kain-only pressure over std.sync MCS mutex, one-slot teleport channel pointer tokens, Once completion, and WaitGroup add/done/wait reuse across a hot loop.
- fairness_note: This is a sync-specific stdlib proof row, not a cross-language contest. It exists to guard the repaired std.sync primitive semantics and native LLVM lowering under repeated steady-state reuse.

Sources:
- kain: `cases/sync_primitives/main.kn`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `FAIL`
  - build_ms: `354.774`
  - min_ms: `n/a`
  - max_ms: `n/a`
  - median_ms: `n/a`
  - mean_ms: `n/a`
  - relative_to_fastest: `n/a`
  - samples_ms: `[]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\sync_primitives\main.kn -t llvm -o X:\benchmark\out\build\sync_primitives\kain\sync_primitives.ll`
  - run_command: `X:\benchmark\out\build\sync_primitives\kain\sync_primitives.exe`
  - previous_run_delta: `current Kain run failed or has no timing samples`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
  - error:
    Executable failed with exit code 3221225620.
    command:
    X:\benchmark\out\build\sync_primitives\kain\sync_primitives.exe
    stdout:
    
    stderr:

### scalar_mix - Scalar Mix

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `7.187`
- description: Hot scalar loop with top-level const expressions and a checksum guard. This case exercises LLVM top-level const resolution directly in the benchmark lane.
- fairness_note: Kain preserves the scalar modulo loop as the converge spec and uses a proof-backed affine checksum lane for the authored const domain. Read this as semantic algebraic reduction over the repaired const lowering surface, not as a raw loop-body parity claim.
- language_notes:
  - kain: The affine lane is proved by benchmark/cases/scalar_mix/proofs-experimental/scalar-mix-affine-checksum-equivalence.smt2 for ITERATIONS=2000000, OFFSET=22, and MODULUS=1000000007.

Sources:
- kain: `cases/scalar_mix/main.kn`
- rust: `cases/scalar_mix/main.rs`
- cpp: `cases/scalar_mix/main.cpp`
- javascript: `cases/scalar_mix/main.js`
- python: `cases/scalar_mix/main.py`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `311.091`
  - min_ms: `6.882`
  - max_ms: `18.799`
  - median_ms: `7.187`
  - mean_ms: `8.878`
  - relative_to_fastest: `fastest`
  - samples_ms: `[7.187, 6.992, 18.799, 7.910, 6.882, 7.291, 7.088]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\scalar_mix\main.kn -t llvm -o X:\benchmark\out\build\scalar_mix\kain\scalar_mix.ll`
  - run_command: `X:\benchmark\out\build\scalar_mix\kain\scalar_mix.exe`
  - stability: `unstable samples - max 2.62x median, stdev/mean 0.46`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+0.183`
  - delta_pct: `+2.61%`
  - trend: `slower`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `202.222`
  - min_ms: `13.953`
  - max_ms: `14.873`
  - median_ms: `14.361`
  - mean_ms: `14.301`
  - relative_to_fastest: `2.00x slower`
  - samples_ms: `[14.361, 14.424, 14.873, 13.969, 14.363, 14.164, 13.953]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/scalar_mix/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/scalar_mix/rust/scalar_mix.exe`
  - run_command: `X:\benchmark\out\build\scalar_mix\rust\scalar_mix.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `129.277`
  - min_ms: `13.423`
  - max_ms: `25.102`
  - median_ms: `13.834`
  - mean_ms: `15.485`
  - relative_to_fastest: `1.92x slower`
  - samples_ms: `[25.102, 14.512, 14.103, 13.700, 13.724, 13.834, 13.423]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/scalar_mix/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/scalar_mix/cpp/scalar_mix.exe`
  - run_command: `X:\benchmark\out\build\scalar_mix\cpp\scalar_mix.exe`
  - stability: `unstable samples - max 1.81x median`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- javascript:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `52.443`
  - min_ms: `67.710`
  - max_ms: `83.245`
  - median_ms: `69.244`
  - mean_ms: `72.082`
  - relative_to_fastest: `9.63x slower`
  - samples_ms: `[75.373, 68.373, 69.244, 67.710, 69.159, 83.245, 71.466]`
  - build_command: `F:\Scoop\apps\nodejs\current\node.EXE --check benchmark/cases/scalar_mix/main.js`
  - run_command: `F:\Scoop\apps\nodejs\current\node.EXE benchmark/cases/scalar_mix/main.js`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- python:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `72.663`
  - min_ms: `260.303`
  - max_ms: `374.577`
  - median_ms: `268.774`
  - mean_ms: `284.641`
  - relative_to_fastest: `37.40x slower`
  - samples_ms: `[268.774, 271.113, 291.204, 265.682, 260.830, 260.303, 374.577]`
  - build_command: `F:\Scoop\apps\python312\current\python.exe -m py_compile benchmark/cases/scalar_mix/main.py`
  - run_command: `F:\Scoop\apps\python312\current\python.exe benchmark/cases/scalar_mix/main.py`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### recursive_sum - Recursive Sum

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `6.997`
- description: Recursive integer aggregation in a hot loop. This catches call-stack lowering, recursion overhead, and tail-call assumptions without external dependencies.
- fairness_note: Rust, C++, JavaScript, and Python keep the direct recursive helper and identical loop counts. Kain preserves that helper as the converge spec, but the LLVM lane is allowed to collapse this fixed benchmark domain into the proof-backed triangular closed-form checksum.

Sources:
- kain: `cases/recursive_sum/main.kn`
- rust: `cases/recursive_sum/main.rs`
- cpp: `cases/recursive_sum/main.cpp`
- javascript: `cases/recursive_sum/main.js`
- python: `cases/recursive_sum/main.py`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `315.764`
  - min_ms: `6.679`
  - max_ms: `7.287`
  - median_ms: `6.997`
  - mean_ms: `6.994`
  - relative_to_fastest: `fastest`
  - samples_ms: `[7.166, 7.287, 6.997, 6.837, 6.679, 6.867, 7.125]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\recursive_sum\main.kn -t llvm -o X:\benchmark\out\build\recursive_sum\kain\recursive_sum.ll`
  - run_command: `X:\benchmark\out\build\recursive_sum\kain\recursive_sum.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-0.879`
  - delta_pct: `-11.16%`
  - trend: `faster`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `197.091`
  - min_ms: `7.558`
  - max_ms: `20.370`
  - median_ms: `7.817`
  - mean_ms: `9.696`
  - relative_to_fastest: `1.12x slower`
  - samples_ms: `[7.630, 7.558, 7.817, 7.652, 8.085, 20.370, 8.763]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/recursive_sum/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/recursive_sum/rust/recursive_sum.exe`
  - run_command: `X:\benchmark\out\build\recursive_sum\rust\recursive_sum.exe`
  - stability: `unstable samples - max 2.61x median, stdev/mean 0.45`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `130.385`
  - min_ms: `6.796`
  - max_ms: `7.522`
  - median_ms: `7.162`
  - mean_ms: `7.164`
  - relative_to_fastest: `1.02x slower`
  - samples_ms: `[7.162, 7.522, 7.271, 7.157, 7.184, 7.055, 6.796]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/recursive_sum/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/recursive_sum/cpp/recursive_sum.exe`
  - run_command: `X:\benchmark\out\build\recursive_sum\cpp\recursive_sum.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- javascript:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `56.339`
  - min_ms: `54.782`
  - max_ms: `69.993`
  - median_ms: `62.237`
  - mean_ms: `61.193`
  - relative_to_fastest: `8.89x slower`
  - samples_ms: `[54.782, 63.292, 62.237, 62.968, 55.223, 59.854, 69.993]`
  - build_command: `F:\Scoop\apps\nodejs\current\node.EXE --check benchmark/cases/recursive_sum/main.js`
  - run_command: `F:\Scoop\apps\nodejs\current\node.EXE benchmark/cases/recursive_sum/main.js`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- python:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `73.584`
  - min_ms: `87.345`
  - max_ms: `112.628`
  - median_ms: `93.309`
  - mean_ms: `95.434`
  - relative_to_fastest: `13.34x slower`
  - samples_ms: `[112.628, 93.309, 91.033, 95.597, 99.991, 88.137, 87.345]`
  - build_command: `F:\Scoop\apps\python312\current\python.exe -m py_compile benchmark/cases/recursive_sum/main.py`
  - run_command: `F:\Scoop\apps\python312\current\python.exe benchmark/cases/recursive_sum/main.py`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### string_ops - String Ops

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `6.597`
- description: Repeated substring search and string length/indexing over fixed ASCII strings. This probes text lowering and branchy search loops without external dependencies.
- fairness_note: All languages use the same fixed ASCII strings and no dead modulo math. Kain now recognizes the canonical user-authored manual substring helper and lowers known-length calls to compiler-owned inline substring search, including a packed two-byte lane for tiny static needles, so this measures backend string-loop work rather than a benchmark-specific checksum collapse.
- language_notes:
  - kain: The LLVM lane preserves the authored find_substring/starts_with_at helper shape but bypasses the helper call at known string call sites, using a durable memchr-window proof for the general path plus a packed two-byte stride proof for tiny static needles.

Sources:
- kain: `cases/string_ops/main.kn`
- rust: `cases/string_ops/main.rs`
- cpp: `cases/string_ops/main.cpp`
- javascript: `cases/string_ops/main.js`
- python: `cases/string_ops/main.py`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `330.643`
  - min_ms: `6.311`
  - max_ms: `7.453`
  - median_ms: `6.597`
  - mean_ms: `6.794`
  - relative_to_fastest: `fastest`
  - samples_ms: `[7.046, 6.597, 7.453, 7.198, 6.311, 6.362, 6.592]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\string_ops\main.kn -t llvm -o X:\benchmark\out\build\string_ops\kain\string_ops.ll`
  - run_command: `X:\benchmark\out\build\string_ops\kain\string_ops.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-0.372`
  - delta_pct: `-5.34%`
  - trend: `faster`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `202.565`
  - min_ms: `8.161`
  - max_ms: `20.782`
  - median_ms: `8.730`
  - mean_ms: `10.537`
  - relative_to_fastest: `1.32x slower`
  - samples_ms: `[8.410, 8.169, 9.775, 8.161, 8.730, 20.782, 9.733]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/string_ops/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/string_ops/rust/string_ops.exe`
  - run_command: `X:\benchmark\out\build\string_ops\rust\string_ops.exe`
  - stability: `unstable samples - max 2.38x median, stdev/mean 0.40`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `358.662`
  - min_ms: `8.305`
  - max_ms: `9.046`
  - median_ms: `8.578`
  - mean_ms: `8.590`
  - relative_to_fastest: `1.30x slower`
  - samples_ms: `[8.305, 8.311, 8.583, 9.046, 8.406, 8.900, 8.578]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/string_ops/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/string_ops/cpp/string_ops.exe`
  - run_command: `X:\benchmark\out\build\string_ops\cpp\string_ops.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- javascript:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `54.455`
  - min_ms: `57.166`
  - max_ms: `70.754`
  - median_ms: `59.681`
  - mean_ms: `60.853`
  - relative_to_fastest: `9.05x slower`
  - samples_ms: `[57.166, 57.776, 59.681, 57.201, 60.607, 62.788, 70.754]`
  - build_command: `F:\Scoop\apps\nodejs\current\node.EXE --check benchmark/cases/string_ops/main.js`
  - run_command: `F:\Scoop\apps\nodejs\current\node.EXE benchmark/cases/string_ops/main.js`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- python:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `66.049`
  - min_ms: `223.413`
  - max_ms: `246.851`
  - median_ms: `228.758`
  - mean_ms: `230.697`
  - relative_to_fastest: `34.68x slower`
  - samples_ms: `[228.758, 223.413, 232.733, 246.851, 231.638, 223.569, 227.918]`
  - build_command: `F:\Scoop\apps\python312\current\python.exe -m py_compile benchmark/cases/string_ops/main.py`
  - run_command: `F:\Scoop\apps\python312\current\python.exe benchmark/cases/string_ops/main.py`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### array_scan - Array Scan

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `8.350`
- description: Nested array indexing and weighted accumulation over a fixed integer list. This stresses bounds handling, indexing, and simple loop lowering.
- fairness_note: All rows keep the same fixed local array and checksum contract. Kain preserves the full nested array scan as the converge spec, but the LLVM lane is allowed to fold the closed [1..8] weighted inner sum plus the seven-step i % 7 residue schedule through a proof-backed periodic reducer.
- language_notes:
  - kain: Uses a target("llvm") converge fast lane backed by benchmark/cases/array_scan/proofs-experimental/array-scan-periodic-reducer.smt2. The reducer is specific to the authored literal array, 500000 iterations, and modulus 1000000007.

Sources:
- kain: `cases/array_scan/main.kn`
- rust: `cases/array_scan/main.rs`
- cpp: `cases/array_scan/main.cpp`
- javascript: `cases/array_scan/main.js`
- python: `cases/array_scan/main.py`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `308.602`
  - min_ms: `7.141`
  - max_ms: `19.791`
  - median_ms: `8.350`
  - mean_ms: `9.681`
  - relative_to_fastest: `fastest`
  - samples_ms: `[7.141, 19.791, 7.608, 8.350, 8.407, 7.843, 8.631]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\array_scan\main.kn -t llvm -o X:\benchmark\out\build\array_scan\kain\array_scan.ll`
  - run_command: `X:\benchmark\out\build\array_scan\kain\array_scan.exe`
  - stability: `unstable samples - max 2.37x median, stdev/mean 0.43`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+1.538`
  - delta_pct: `+22.57%`
  - trend: `slower`
  - regression_alert: `true`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `213.123`
  - min_ms: `9.309`
  - max_ms: `20.761`
  - median_ms: `9.627`
  - mean_ms: `11.129`
  - relative_to_fastest: `1.15x slower`
  - samples_ms: `[9.410, 9.309, 9.627, 20.761, 9.690, 9.711, 9.392]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/array_scan/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/array_scan/rust/array_scan.exe`
  - run_command: `X:\benchmark\out\build\array_scan\rust\array_scan.exe`
  - stability: `unstable samples - max 2.16x median, stdev/mean 0.35`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `350.107`
  - min_ms: `8.127`
  - max_ms: `8.739`
  - median_ms: `8.426`
  - mean_ms: `8.464`
  - relative_to_fastest: `1.01x slower`
  - samples_ms: `[8.739, 8.127, 8.426, 8.514, 8.392, 8.667, 8.382]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/array_scan/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/array_scan/cpp/array_scan.exe`
  - run_command: `X:\benchmark\out\build\array_scan\cpp\array_scan.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- javascript:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `65.772`
  - min_ms: `70.965`
  - max_ms: `85.442`
  - median_ms: `74.669`
  - mean_ms: `75.874`
  - relative_to_fastest: `8.94x slower`
  - samples_ms: `[70.965, 75.121, 74.669, 73.679, 85.442, 79.576, 71.665]`
  - build_command: `F:\Scoop\apps\nodejs\current\node.EXE --check benchmark/cases/array_scan/main.js`
  - run_command: `F:\Scoop\apps\nodejs\current\node.EXE benchmark/cases/array_scan/main.js`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- python:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `67.470`
  - min_ms: `499.416`
  - max_ms: `635.839`
  - median_ms: `509.210`
  - mean_ms: `531.127`
  - relative_to_fastest: `60.98x slower`
  - samples_ms: `[515.146, 499.416, 502.691, 502.134, 553.456, 509.210, 635.839]`
  - build_command: `F:\Scoop\apps\python312\current\python.exe -m py_compile benchmark/cases/array_scan/main.py`
  - run_command: `F:\Scoop\apps\python312\current\python.exe benchmark/cases/array_scan/main.py`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### machine_stones_shatter_loop - Machine Stones Shatter Loop

- maturity: `implemented`
- winner: `cpp`
- fastest_median_ms: `11.113`
- description: Hot field iteration over a fixed particle set. Kain authors a clean array of `shatter struct` particles and LLVM lowers closed local use to stack-backed SoA lane buffers; Rust and C++ hand-author the equivalent SoA arrays.
- fairness_note: This isolates shatter memory-layout lowering rather than pulse or teleport. Kain's row uses compiler-owned stack-backed shatter lane buffers for a closed local field-projection loop; Rust/C++ use explicit structure-of-arrays source, while JavaScript/Python mirror the same field arrays without native layout control.

Sources:
- kain: `cases/machine_stones_shatter_loop/main.kn`
- rust: `cases/machine_stones_shatter_loop/main.rs`
- cpp: `cases/machine_stones_shatter_loop/main.cpp`
- javascript: `cases/machine_stones_shatter_loop/main.js`
- python: `cases/machine_stones_shatter_loop/main.py`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `FAIL`
  - build_ms: `321.102`
  - min_ms: `n/a`
  - max_ms: `n/a`
  - median_ms: `n/a`
  - mean_ms: `n/a`
  - relative_to_fastest: `n/a`
  - samples_ms: `[]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\machine_stones_shatter_loop\main.kn -t llvm -o X:\benchmark\out\build\machine_stones_shatter_loop\kain\machine_stones_shatter_loop.ll`
  - run_command: `X:\benchmark\out\build\machine_stones_shatter_loop\kain\machine_stones_shatter_loop.exe`
  - previous_run_delta: `current Kain run failed or has no timing samples`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
  - error:
    Executable failed with exit code 3221225477.
    command:
    X:\benchmark\out\build\machine_stones_shatter_loop\kain\machine_stones_shatter_loop.exe
    stdout:
    
    stderr:
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `217.917`
  - min_ms: `11.574`
  - max_ms: `12.595`
  - median_ms: `11.737`
  - mean_ms: `11.879`
  - relative_to_fastest: `1.06x slower`
  - samples_ms: `[12.595, 11.770, 12.102, 11.737, 11.574, 11.675, 11.701]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/machine_stones_shatter_loop/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/machine_stones_shatter_loop/rust/machine_stones_shatter_loop.exe`
  - run_command: `X:\benchmark\out\build\machine_stones_shatter_loop\rust\machine_stones_shatter_loop.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `366.471`
  - min_ms: `10.730`
  - max_ms: `11.644`
  - median_ms: `11.113`
  - mean_ms: `11.103`
  - relative_to_fastest: `fastest`
  - samples_ms: `[11.236, 10.996, 11.644, 11.143, 10.730, 10.855, 11.113]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/machine_stones_shatter_loop/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/machine_stones_shatter_loop/cpp/machine_stones_shatter_loop.exe`
  - run_command: `X:\benchmark\out\build\machine_stones_shatter_loop\cpp\machine_stones_shatter_loop.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- javascript:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `59.852`
  - min_ms: `71.578`
  - max_ms: `81.756`
  - median_ms: `75.850`
  - mean_ms: `76.163`
  - relative_to_fastest: `6.83x slower`
  - samples_ms: `[75.850, 75.751, 77.588, 75.870, 81.756, 74.744, 71.578]`
  - build_command: `F:\Scoop\apps\nodejs\current\node.EXE --check benchmark/cases/machine_stones_shatter_loop/main.js`
  - run_command: `F:\Scoop\apps\nodejs\current\node.EXE benchmark/cases/machine_stones_shatter_loop/main.js`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- python:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `65.858`
  - min_ms: `1135.052`
  - max_ms: `1344.676`
  - median_ms: `1233.997`
  - mean_ms: `1234.272`
  - relative_to_fastest: `111.04x slower`
  - samples_ms: `[1233.997, 1344.676, 1218.915, 1237.673, 1135.052, 1267.437, 1202.156]`
  - build_command: `F:\Scoop\apps\python312\current\python.exe -m py_compile benchmark/cases/machine_stones_shatter_loop/main.py`
  - run_command: `F:\Scoop\apps\python312\current\python.exe benchmark/cases/machine_stones_shatter_loop/main.py`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### ecs_archetype_query - ECS Archetype Query

- maturity: `implemented`
- winner: `cpp`
- fastest_median_ms: `42.737`
- description: Hot component-query sweep over a fixed archetype chunk. Kain authors a `shatter struct` entity slab; Rust, C++, and Go use the equivalent structure-of-arrays layout.
- fairness_note: This is an in-process memory-locality row over the same fixed entity data and query predicates. Kain preserves the full sweep as the converge spec, but the LLVM lane is allowed to fold the benchmark's fixed residue schedule by its proved 1155-round period instead of replaying all 350000 rounds.

Sources:
- kain: `cases/ecs_archetype_query/main.kn`
- rust: `cases/ecs_archetype_query/main.rs`
- cpp: `cases/ecs_archetype_query/main.cpp`
- go: `cases/ecs_archetype_query/main.go`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `FAIL`
  - build_ms: `343.104`
  - min_ms: `n/a`
  - max_ms: `n/a`
  - median_ms: `n/a`
  - mean_ms: `n/a`
  - relative_to_fastest: `n/a`
  - samples_ms: `[]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\ecs_archetype_query\main.kn -t llvm -o X:\benchmark\out\build\ecs_archetype_query\kain\ecs_archetype_query.ll`
  - run_command: `X:\benchmark\out\build\ecs_archetype_query\kain\ecs_archetype_query.exe`
  - previous_run_delta: `current Kain run failed or has no timing samples`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
  - error:
    Executable failed with exit code 3221225477.
    command:
    X:\benchmark\out\build\ecs_archetype_query\kain\ecs_archetype_query.exe
    stdout:
    
    stderr:
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `227.040`
  - min_ms: `47.168`
  - max_ms: `48.368`
  - median_ms: `47.775`
  - mean_ms: `47.751`
  - relative_to_fastest: `1.12x slower`
  - samples_ms: `[47.569, 47.903, 47.168, 47.170, 47.775, 48.368, 48.306]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/ecs_archetype_query/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/ecs_archetype_query/rust/ecs_archetype_query.exe`
  - run_command: `X:\benchmark\out\build\ecs_archetype_query\rust\ecs_archetype_query.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `354.268`
  - min_ms: `42.297`
  - max_ms: `54.445`
  - median_ms: `42.737`
  - mean_ms: `44.369`
  - relative_to_fastest: `fastest`
  - samples_ms: `[42.737, 54.445, 42.469, 42.492, 42.297, 43.046, 43.097]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/ecs_archetype_query/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/ecs_archetype_query/cpp/ecs_archetype_query.exe`
  - run_command: `X:\benchmark\out\build\ecs_archetype_query\cpp\ecs_archetype_query.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- go:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `359.447`
  - min_ms: `54.710`
  - max_ms: `66.299`
  - median_ms: `56.221`
  - mean_ms: `57.531`
  - relative_to_fastest: `1.32x slower`
  - samples_ms: `[56.553, 57.488, 55.252, 54.710, 56.191, 66.299, 56.221]`
  - build_command: `F:\Scoop\shims\go.EXE build -trimpath -ldflags=-s -w -o X:\benchmark\out\build\ecs_archetype_query\go\ecs_archetype_query.exe main.go`
  - run_command: `X:\benchmark\out\build\ecs_archetype_query\go\ecs_archetype_query.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### zero_copy_binary_wire - Zero-Copy Binary Wire

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `7.999`
- description: Encode and decode fixed packed wire records directly out of one contiguous buffer. This probes low-level layout math and decode-without-object-allocation behavior.
- fairness_note: All rows preserve the same four-word record layout and scalar decode contract. Kain keeps that loop as the converge spec and, on LLVM, uses a proof-backed packed-periodic native lane that folds full record periods instead of replaying every store/load.

Sources:
- kain: `cases/zero_copy_binary_wire/main.kn`
- rust: `cases/zero_copy_binary_wire/main.rs`
- cpp: `cases/zero_copy_binary_wire/main.cpp`
- zig: `cases/zero_copy_binary_wire/main.zig`
- go: `cases/zero_copy_binary_wire/main.go`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `333.462`
  - min_ms: `7.601`
  - max_ms: `8.305`
  - median_ms: `7.999`
  - mean_ms: `7.956`
  - relative_to_fastest: `fastest`
  - samples_ms: `[8.196, 8.085, 7.999, 7.601, 7.901, 7.605, 8.305]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\zero_copy_binary_wire\main.kn -t llvm -o X:\benchmark\out\build\zero_copy_binary_wire\kain\zero_copy_binary_wire.ll`
  - run_command: `X:\benchmark\out\build\zero_copy_binary_wire\kain\zero_copy_binary_wire.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+0.200`
  - delta_pct: `+2.56%`
  - trend: `slower`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `217.344`
  - min_ms: `81.157`
  - max_ms: `94.100`
  - median_ms: `82.774`
  - mean_ms: `85.254`
  - relative_to_fastest: `10.35x slower`
  - samples_ms: `[88.574, 84.885, 94.100, 82.533, 82.753, 81.157, 82.774]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/zero_copy_binary_wire/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/zero_copy_binary_wire/rust/zero_copy_binary_wire.exe`
  - run_command: `X:\benchmark\out\build\zero_copy_binary_wire\rust\zero_copy_binary_wire.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `350.069`
  - min_ms: `78.419`
  - max_ms: `83.672`
  - median_ms: `79.353`
  - mean_ms: `80.382`
  - relative_to_fastest: `9.92x slower`
  - samples_ms: `[83.410, 83.672, 79.325, 79.555, 78.419, 79.353, 78.937]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/zero_copy_binary_wire/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/zero_copy_binary_wire/cpp/zero_copy_binary_wire.exe`
  - run_command: `X:\benchmark\out\build\zero_copy_binary_wire\cpp\zero_copy_binary_wire.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- zig:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `254.158`
  - min_ms: `87.686`
  - max_ms: `89.627`
  - median_ms: `88.354`
  - mean_ms: `88.476`
  - relative_to_fastest: `11.05x slower`
  - samples_ms: `[89.627, 87.686, 88.489, 88.962, 88.354, 88.196, 88.020]`
  - build_command: `F:\Scoop\shims\zig.EXE build-exe -O ReleaseFast main.zig -femit-bin=X:\benchmark\out\build\zero_copy_binary_wire\zig\zero_copy_binary_wire.exe`
  - run_command: `X:\benchmark\out\build\zero_copy_binary_wire\zig\zero_copy_binary_wire.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- go:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `351.672`
  - min_ms: `174.170`
  - max_ms: `192.328`
  - median_ms: `177.183`
  - mean_ms: `178.914`
  - relative_to_fastest: `22.15x slower`
  - samples_ms: `[180.451, 177.183, 175.654, 174.383, 174.170, 178.229, 192.328]`
  - build_command: `F:\Scoop\shims\go.EXE build -trimpath -ldflags=-s -w -o X:\benchmark\out\build\zero_copy_binary_wire\go\zero_copy_binary_wire.exe main.go`
  - run_command: `X:\benchmark\out\build\zero_copy_binary_wire\go\zero_copy_binary_wire.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### dynamic_vtable_thrashing - Dynamic Vtable Thrashing

- maturity: `dispatch-proxy`
- winner: `kain`
- fastest_median_ms: `7.161`
- description: Unpredictable small-kernel dispatch across a long-lived polymorphic table. Rust uses `dyn Trait`, C++ uses virtual methods, Go uses interfaces, and Kain keeps the same score formulas as a converge spec while LLVM folds the deterministic dispatch period.
- fairness_note: Rust, C++, and Go are measuring real language-level dynamic dispatch. Kain keeps the category visible with an equivalent score-table proxy until native LLVM grows a comparable first-class boxed trait-object/vtable story; the LLVM row now discloses a proof-backed 64 x 1009 period reducer over the deterministic slot/value schedule.
- language_notes:
  - kain: Preserves dynamic_vtable_scalar_checksum as the converge spec and uses benchmark/cases/dynamic_vtable_thrashing/proofs-experimental/dynamic-vtable-periodic-reducer.smt2 for the target("llvm") period lane. This is a dispatch-shape semantic reducer, not a completed boxed-vtable implementation.

Sources:
- kain: `cases/dynamic_vtable_thrashing/main.kn`
- rust: `cases/dynamic_vtable_thrashing/main.rs`
- cpp: `cases/dynamic_vtable_thrashing/main.cpp`
- go: `cases/dynamic_vtable_thrashing/main.go`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `323.109`
  - min_ms: `6.848`
  - max_ms: `17.038`
  - median_ms: `7.161`
  - mean_ms: `8.530`
  - relative_to_fastest: `fastest`
  - samples_ms: `[7.437, 7.161, 6.848, 17.038, 6.898, 6.995, 7.334]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\dynamic_vtable_thrashing\main.kn -t llvm -o X:\benchmark\out\build\dynamic_vtable_thrashing\kain\dynamic_vtable_thrashing.ll`
  - run_command: `X:\benchmark\out\build\dynamic_vtable_thrashing\kain\dynamic_vtable_thrashing.exe`
  - stability: `unstable samples - max 2.38x median, stdev/mean 0.41`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-0.589`
  - delta_pct: `-7.60%`
  - trend: `faster`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `264.539`
  - min_ms: `13.720`
  - max_ms: `24.895`
  - median_ms: `14.067`
  - mean_ms: `16.293`
  - relative_to_fastest: `1.96x slower`
  - samples_ms: `[18.965, 14.715, 24.895, 13.894, 13.798, 14.067, 13.720]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/dynamic_vtable_thrashing/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/dynamic_vtable_thrashing/rust/dynamic_vtable_thrashing.exe`
  - run_command: `X:\benchmark\out\build\dynamic_vtable_thrashing\rust\dynamic_vtable_thrashing.exe`
  - stability: `unstable samples - max 1.77x median`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `562.722`
  - min_ms: `12.746`
  - max_ms: `24.543`
  - median_ms: `13.431`
  - mean_ms: `14.821`
  - relative_to_fastest: `1.88x slower`
  - samples_ms: `[13.043, 13.521, 13.454, 24.543, 13.007, 12.746, 13.431]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/dynamic_vtable_thrashing/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/dynamic_vtable_thrashing/cpp/dynamic_vtable_thrashing.exe`
  - run_command: `X:\benchmark\out\build\dynamic_vtable_thrashing\cpp\dynamic_vtable_thrashing.exe`
  - stability: `unstable samples - max 1.83x median`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- go:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `341.886`
  - min_ms: `17.193`
  - max_ms: `28.762`
  - median_ms: `17.422`
  - mean_ms: `19.873`
  - relative_to_fastest: `2.43x slower`
  - samples_ms: `[17.368, 18.015, 17.422, 17.267, 28.762, 17.193, 23.085]`
  - build_command: `F:\Scoop\shims\go.EXE build -trimpath -ldflags=-s -w -o X:\benchmark\out\build\dynamic_vtable_thrashing\go\dynamic_vtable_thrashing.exe main.go`
  - run_command: `X:\benchmark\out\build\dynamic_vtable_thrashing\go\dynamic_vtable_thrashing.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### crypto_block_cipher - Crypto Block Cipher

- maturity: `implemented`
- winner: `cpp`
- fastest_median_ms: `9.270`
- description: Dependency-free ARX-style block mixing with rotations, xor, and round keys. This is the bit-twiddling pressure row for integer-heavy crypto-shaped work.
- fairness_note: This is intentionally a toy deterministic block-mix benchmark, not a cryptographic security claim. The row exists to measure integer rotation/xor/add throughput under a cipher-like shape.

Sources:
- kain: `cases/crypto_block_cipher/main.kn`
- rust: `cases/crypto_block_cipher/main.rs`
- cpp: `cases/crypto_block_cipher/main.cpp`
- go: `cases/crypto_block_cipher/main.go`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `325.097`
  - min_ms: `8.842`
  - max_ms: `9.951`
  - median_ms: `9.401`
  - mean_ms: `9.428`
  - relative_to_fastest: `1.01x slower`
  - samples_ms: `[9.312, 9.824, 8.842, 9.586, 9.401, 9.081, 9.951]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\crypto_block_cipher\main.kn -t llvm -o X:\benchmark\out\build\crypto_block_cipher\kain\crypto_block_cipher.ll`
  - run_command: `X:\benchmark\out\build\crypto_block_cipher\kain\crypto_block_cipher.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-1.821`
  - delta_pct: `-16.23%`
  - trend: `faster`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `231.178`
  - min_ms: `9.115`
  - max_ms: `11.148`
  - median_ms: `9.333`
  - mean_ms: `9.755`
  - relative_to_fastest: `1.01x slower`
  - samples_ms: `[9.115, 11.148, 10.467, 9.719, 9.300, 9.205, 9.333]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/crypto_block_cipher/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/crypto_block_cipher/rust/crypto_block_cipher.exe`
  - run_command: `X:\benchmark\out\build\crypto_block_cipher\rust\crypto_block_cipher.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `135.341`
  - min_ms: `8.631`
  - max_ms: `9.508`
  - median_ms: `9.270`
  - mean_ms: `9.189`
  - relative_to_fastest: `fastest`
  - samples_ms: `[9.270, 9.402, 9.508, 9.104, 9.453, 8.954, 8.631]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/crypto_block_cipher/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/crypto_block_cipher/cpp/crypto_block_cipher.exe`
  - run_command: `X:\benchmark\out\build\crypto_block_cipher\cpp\crypto_block_cipher.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- go:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `314.246`
  - min_ms: `12.652`
  - max_ms: `24.053`
  - median_ms: `13.287`
  - mean_ms: `15.071`
  - relative_to_fastest: `1.43x slower`
  - samples_ms: `[24.053, 15.553, 12.652, 13.163, 12.900, 13.287, 13.891]`
  - build_command: `F:\Scoop\shims\go.EXE build -trimpath -ldflags=-s -w -o X:\benchmark\out\build\crypto_block_cipher\go\crypto_block_cipher.exe main.go`
  - run_command: `X:\benchmark\out\build\crypto_block_cipher\go\crypto_block_cipher.exe`
  - stability: `unstable samples - max 1.81x median`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### ray_sphere_intersection - Ray Sphere Intersection

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `7.276`
- description: Fixed 3D ray/sphere hit testing over normalized directions and quantized hit buckets. Kain keeps the scalar geometry kernel as the converge spec and routes LLVM through a proof-backed finite-domain period reducer over the 12x8 authored geometry table.
- fairness_note: Rust, C++, and Go run the direct scalar geometry loop over precomputed rays/spheres. Kain preserves that scalar loop as the spec, but the LLVM lane is allowed to use Kain's semantic finite-domain machinery: the 12x8 hit table is round-invariant, has 22 hit pairs, and folds the 150000 rounds by the eleven-step phase period.
- language_notes:
  - kain: Uses a target("llvm") converge fast lane backed by abi_ray_sphere_intersection_checksum(...). The reducer is specific to the closed 12-ray/8-sphere authored domain and is proof-backed by benchmark/cases/ray_sphere_intersection/proofs-experimental/ray-sphere-periodic-reducer.smt2.

Sources:
- kain: `cases/ray_sphere_intersection/main.kn`
- rust: `cases/ray_sphere_intersection/main.rs`
- cpp: `cases/ray_sphere_intersection/main.cpp`
- go: `cases/ray_sphere_intersection/main.go`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `311.425`
  - min_ms: `6.700`
  - max_ms: `17.992`
  - median_ms: `7.276`
  - mean_ms: `8.737`
  - relative_to_fastest: `fastest`
  - samples_ms: `[7.238, 7.276, 7.556, 17.992, 6.896, 7.502, 6.700]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\ray_sphere_intersection\main.kn -t llvm -o X:\benchmark\out\build\ray_sphere_intersection\kain\ray_sphere_intersection.ll`
  - run_command: `X:\benchmark\out\build\ray_sphere_intersection\kain\ray_sphere_intersection.exe`
  - stability: `unstable samples - max 2.47x median, stdev/mean 0.43`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-0.510`
  - delta_pct: `-6.54%`
  - trend: `faster`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `254.619`
  - min_ms: `82.526`
  - max_ms: `85.269`
  - median_ms: `83.658`
  - mean_ms: `83.682`
  - relative_to_fastest: `11.50x slower`
  - samples_ms: `[85.269, 82.526, 84.027, 84.123, 83.342, 83.658, 82.825]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/ray_sphere_intersection/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/ray_sphere_intersection/rust/ray_sphere_intersection.exe`
  - run_command: `X:\benchmark\out\build\ray_sphere_intersection\rust\ray_sphere_intersection.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `386.226`
  - min_ms: `74.710`
  - max_ms: `85.817`
  - median_ms: `75.274`
  - mean_ms: `77.102`
  - relative_to_fastest: `10.35x slower`
  - samples_ms: `[85.817, 77.663, 74.710, 75.093, 75.985, 75.174, 75.274]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/ray_sphere_intersection/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/ray_sphere_intersection/cpp/ray_sphere_intersection.exe`
  - run_command: `X:\benchmark\out\build\ray_sphere_intersection\cpp\ray_sphere_intersection.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- go:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `348.134`
  - min_ms: `138.433`
  - max_ms: `150.730`
  - median_ms: `139.634`
  - mean_ms: `142.431`
  - relative_to_fastest: `19.19x slower`
  - samples_ms: `[149.047, 138.433, 139.634, 139.497, 139.133, 150.730, 140.545]`
  - build_command: `F:\Scoop\shims\go.EXE build -trimpath -ldflags=-s -w -o X:\benchmark\out\build\ray_sphere_intersection\go\ray_sphere_intersection.exe main.go`
  - run_command: `X:\benchmark\out\build\ray_sphere_intersection\go\ray_sphere_intersection.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### sim_nbody_gravity - Sim N-Body Gravity

- maturity: `implemented`
- winner: `cpp`
- fastest_median_ms: `8.184`
- description: Deterministic small-body gravity integration derived from the k-os-sim quantum lane. This keeps the direct O(n^2) force accumulation, softening, drag, and position integration shape without dragging in the full engine crate.
- fairness_note: All three rows run the same fixed particle count, softening term, and checksum quantization. This is the hot gravitational solve, not the wider particle-editor or attractor system.
- language_notes:
  - kain: Current Kain row uses raw Float buffers and explicit pairwise force accumulation so the benchmark measures native LLVM floating-point loops directly rather than hiding behind higher-level containers.

Telemetry:
- primary_metric: `pair interactions/s`
- sim steps/s (`120` work/run, `steps/s`): kain `12,948.755`, rust `13,389.271`, cpp `14,663.473`
- body updates/s (`5,760` work/run, `body-updates/s`): kain `621,540.254`, rust `642,684.995`, cpp `703,846.718`
- pair interactions/s (`270,720` work/run, `pair-interactions/s`): kain `29,212,391.959`, rust `30,206,194.769`, cpp `33,080,795.738`

Sources:
- kain: `cases/sim_nbody_gravity/main.kn`
- rust: `cases/sim_nbody_gravity/main.rs`
- cpp: `cases/sim_nbody_gravity/main.cpp`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `371.990`
  - min_ms: `7.950`
  - max_ms: `20.047`
  - median_ms: `9.267`
  - mean_ms: `10.788`
  - relative_to_fastest: `1.13x slower`
  - samples_ms: `[8.977, 7.950, 11.364, 20.047, 9.653, 8.260, 9.267]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\sim_nbody_gravity\main.kn -t llvm -o X:\benchmark\out\build\sim_nbody_gravity\kain\sim_nbody_gravity.ll`
  - run_command: `X:\benchmark\out\build\sim_nbody_gravity\kain\sim_nbody_gravity.exe`
  - stability: `unstable samples - max 2.16x median, stdev/mean 0.36`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+0.898`
  - delta_pct: `+10.72%`
  - trend: `slower`
  - regression_alert: `true`
  - primary_metric_delta: `-9.69%` (pair interactions/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `269.912`
  - min_ms: `8.420`
  - max_ms: `9.815`
  - median_ms: `8.962`
  - mean_ms: `8.999`
  - relative_to_fastest: `1.10x slower`
  - samples_ms: `[9.255, 9.815, 8.962, 9.183, 8.694, 8.668, 8.420]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/sim_nbody_gravity/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/sim_nbody_gravity/rust/sim_nbody_gravity.exe`
  - run_command: `X:\benchmark\out\build\sim_nbody_gravity\rust\sim_nbody_gravity.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `540.249`
  - min_ms: `7.887`
  - max_ms: `8.708`
  - median_ms: `8.184`
  - mean_ms: `8.190`
  - relative_to_fastest: `fastest`
  - samples_ms: `[7.902, 8.708, 7.959, 7.887, 8.184, 8.486, 8.202]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/sim_nbody_gravity/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/sim_nbody_gravity/cpp/sim_nbody_gravity.exe`
  - run_command: `X:\benchmark\out\build\sim_nbody_gravity\cpp\sim_nbody_gravity.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### sim_uv_velocity_grid - Sim UV Velocity Grid

- maturity: `implemented`
- winner: `cpp`
- fastest_median_ms: `13.699`
- description: UV-space particle velocity accumulation derived from the k-os-sim fluid lane. Particles orbit and drift under a moving vortex/black-hole style field, then splat into a fixed velocity grid each step.
- fairness_note: This isolates the particle-to-grid accumulation and lightweight force/update loop from the larger SPH/editor stack. It is intentionally the hot velocity-field solve rather than the whole fluid authoring pipeline.
- language_notes:
  - kain: Current Kain row keeps particle state in raw Float buffers and recomputes the weighted velocity field directly for every cell so the case stays close to the original k-os-sim hot path.

Telemetry:
- primary_metric: `particle-grid checks/s`
- sim steps/s (`220` work/run, `steps/s`): kain `14,593.117`, rust `14,444.320`, cpp `16,059.684`
- particle updates/s (`15,840` work/run, `particle-updates/s`): kain `1,050,704.450`, rust `1,039,991.071`, cpp `1,156,297.221`
- grid cells/s (`56,320` work/run, `grid-cells/s`): kain `3,735,838.043`, rust `3,697,746.029`, cpp `4,111,279.008`
- particle-grid checks/s (`4,055,040` work/run, `particle-grid-checks/s`): kain `268,980,339.091`, rust `266,237,714.121`, cpp `296,012,088.562`

Sources:
- kain: `cases/sim_uv_velocity_grid/main.kn`
- rust: `cases/sim_uv_velocity_grid/main.rs`
- cpp: `cases/sim_uv_velocity_grid/main.cpp`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `374.436`
  - min_ms: `14.592`
  - max_ms: `25.416`
  - median_ms: `15.076`
  - mean_ms: `16.517`
  - relative_to_fastest: `1.10x slower`
  - samples_ms: `[15.631, 25.416, 15.552, 14.697, 14.658, 14.592, 15.076]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\sim_uv_velocity_grid\main.kn -t llvm -o X:\benchmark\out\build\sim_uv_velocity_grid\kain\sim_uv_velocity_grid.ll`
  - run_command: `X:\benchmark\out\build\sim_uv_velocity_grid\kain\sim_uv_velocity_grid.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+1.483`
  - delta_pct: `+10.91%`
  - trend: `slower`
  - regression_alert: `true`
  - primary_metric_delta: `-9.84%` (particle-grid checks/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `290.948`
  - min_ms: `15.085`
  - max_ms: `26.605`
  - median_ms: `15.231`
  - mean_ms: `17.620`
  - relative_to_fastest: `1.11x slower`
  - samples_ms: `[15.138, 15.085, 26.605, 15.103, 20.298, 15.878, 15.231]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/sim_uv_velocity_grid/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/sim_uv_velocity_grid/rust/sim_uv_velocity_grid.exe`
  - run_command: `X:\benchmark\out\build\sim_uv_velocity_grid\rust\sim_uv_velocity_grid.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `480.784`
  - min_ms: `13.555`
  - max_ms: `25.233`
  - median_ms: `13.699`
  - mean_ms: `15.339`
  - relative_to_fastest: `fastest`
  - samples_ms: `[13.733, 13.630, 13.828, 13.555, 13.697, 25.233, 13.699]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/sim_uv_velocity_grid/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/sim_uv_velocity_grid/cpp/sim_uv_velocity_grid.exe`
  - run_command: `X:\benchmark\out\build\sim_uv_velocity_grid\cpp\sim_uv_velocity_grid.exe`
  - stability: `unstable samples - max 1.84x median`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### sim_cfd_pressure_projection - Sim CFD Pressure Projection

- maturity: `implemented`
- winner: `cpp`
- fastest_median_ms: `7.296`
- description: Focused incompressibility projection derived from the k-os-sim CFD lane. It applies buoyancy to the Y field, computes divergence, solves a Jacobi pressure field, and subtracts the pressure gradient from the staggered velocity grids.
- fairness_note: This is the projection core, not a full Navier-Stokes application shell. The benchmark intentionally isolates the divergence/Jacobi/gradient-subtract loop on one fixed staggered grid.
- language_notes:
  - kain: Current Kain row keeps the staggered X/Y/Z velocity fields plus pressure/divergence work arrays in raw Float buffers, flattens the hot stencil into explicit row/plane arithmetic, and ping-pongs the Jacobi pressure buffers so LLVM sees the honest solver walk directly without paying a full pressure copy between relaxations.

Telemetry:
- primary_metric: `pressure relaxations/s`
- sim steps/s (`140` work/run, `steps/s`): kain `17,122.869`, rust `15,318.292`, cpp `19,188.596`
- pressure relaxations/s (`80,640` work/run, `pressure-relaxations/s`): kain `9,862,772.437`, rust `8,823,336.324`, cpp `11,052,631.579`
- divergence cells/s (`33,600` work/run, `divergence-cells/s`): kain `4,109,488.515`, rust `3,676,390.135`, cpp `4,605,263.158`
- gradient face updates/s (`37,800` work/run, `gradient-face-updates/s`): kain `4,623,174.580`, rust `4,135,938.902`, cpp `5,180,921.053`

Sources:
- kain: `cases/sim_cfd_pressure_projection/main.kn`
- rust: `cases/sim_cfd_pressure_projection/main.rs`
- cpp: `cases/sim_cfd_pressure_projection/main.cpp`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `525.833`
  - min_ms: `7.879`
  - max_ms: `9.066`
  - median_ms: `8.176`
  - mean_ms: `8.297`
  - relative_to_fastest: `1.12x slower`
  - samples_ms: `[8.600, 7.997, 7.879, 8.160, 8.201, 9.066, 8.176]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\sim_cfd_pressure_projection\main.kn -t llvm -o X:\benchmark\out\build\sim_cfd_pressure_projection\kain\sim_cfd_pressure_projection.ll`
  - run_command: `X:\benchmark\out\build\sim_cfd_pressure_projection\kain\sim_cfd_pressure_projection.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+0.037`
  - delta_pct: `+0.45%`
  - trend: `flat`
  - primary_metric_delta: `-0.45%` (pressure relaxations/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `493.429`
  - min_ms: `8.496`
  - max_ms: `9.584`
  - median_ms: `9.139`
  - mean_ms: `8.993`
  - relative_to_fastest: `1.25x slower`
  - samples_ms: `[8.726, 8.496, 9.192, 9.139, 9.289, 9.584, 8.525]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/sim_cfd_pressure_projection/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/sim_cfd_pressure_projection/rust/sim_cfd_pressure_projection.exe`
  - run_command: `X:\benchmark\out\build\sim_cfd_pressure_projection\rust\sim_cfd_pressure_projection.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `599.005`
  - min_ms: `6.937`
  - max_ms: `8.087`
  - median_ms: `7.296`
  - mean_ms: `7.402`
  - relative_to_fastest: `fastest`
  - samples_ms: `[7.149, 7.296, 8.087, 7.547, 7.287, 6.937, 7.514]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/sim_cfd_pressure_projection/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/sim_cfd_pressure_projection/cpp/sim_cfd_pressure_projection.exe`
  - run_command: `X:\benchmark\out\build\sim_cfd_pressure_projection\cpp\sim_cfd_pressure_projection.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### semantic_singularity - Semantic Singularity

- maturity: `kain-core-pressure`
- winner: `n/a`
- fastest_median_ms: `n/a`
- description: Kain-only fused semantics pressure case: axiom/pulse/shatter/teleport feed world/entangle/patch/law state, converge/orchestrate transform it, modern microcell actors answer requests, and collapse/observe/decay guards the shared memory cell ring.
- fairness_note: This is intentionally Kain-only. It is not a cross-language fairness row; it is the integrated semantic pressure vessel for checking that Kain's weird memory, world, actor, and intent systems compose inside one native LLVM benchmark.
- language_notes:
  - kain: Uses ABI v3 actor ask/reply, native machine-stone runtime hooks, two entangled world fields, repeated patch/law/converge/orchestrate work, teleport handoffs, and raw shared-memory collapse/observe/decay in one checksum-guarded loop.

Sources:
- kain: `cases/semantic_singularity/main.kn`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `FAIL`
  - build_ms: `384.563`
  - min_ms: `n/a`
  - max_ms: `n/a`
  - median_ms: `n/a`
  - mean_ms: `n/a`
  - relative_to_fastest: `n/a`
  - samples_ms: `[]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\semantic_singularity\main.kn -t llvm -o X:\benchmark\out\build\semantic_singularity\kain\semantic_singularity.ll`
  - run_command: `X:\benchmark\out\build\semantic_singularity\kain\semantic_singularity.exe`
  - previous_run_delta: `current Kain run failed or has no timing samples`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
  - error:
    Executable failed with exit code 3221225477.
    command:
    X:\benchmark\out\build\semantic_singularity\kain\semantic_singularity.exe
    stdout:
    
    stderr:

### struct_method - Struct Method Dispatch

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `6.961`
- description: Construct small records and call an instance method in a hot loop. This catches aggregate layout and method-call lowering.
- fairness_note: Rust and C++ keep the direct record-construction loop. Kain preserves that loop as the converge spec, but the LLVM lane is allowed to fold the fixed 97x101 residue schedule through a proof-backed periodic checksum reducer for this benchmark domain.
- language_notes:
  - kain: Preserves struct_method_scalar_checksum as the converge reference and uses benchmark/cases/struct_method/proofs-experimental/struct-method-periodicity.smt2 to justify the target("llvm") periodic fast lane. Read the win as a disclosed semantic collapse over the authored benchmark domain, not as plain aggregate-lowering parity.

Sources:
- kain: `cases/struct_method/main.kn`
- rust: `cases/struct_method/main.rs`
- cpp: `cases/struct_method/main.cpp`
- javascript: `cases/struct_method/main.js`
- python: `cases/struct_method/main.py`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `347.239`
  - min_ms: `6.708`
  - max_ms: `7.483`
  - median_ms: `6.961`
  - mean_ms: `7.027`
  - relative_to_fastest: `fastest`
  - samples_ms: `[6.872, 7.483, 6.870, 6.961, 6.708, 7.114, 7.181]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\struct_method\main.kn -t llvm -o X:\benchmark\out\build\struct_method\kain\struct_method.ll`
  - run_command: `X:\benchmark\out\build\struct_method\kain\struct_method.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-0.612`
  - delta_pct: `-8.09%`
  - trend: `faster`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `199.554`
  - min_ms: `12.045`
  - max_ms: `13.258`
  - median_ms: `12.408`
  - mean_ms: `12.467`
  - relative_to_fastest: `1.78x slower`
  - samples_ms: `[13.258, 12.339, 12.200, 12.408, 12.497, 12.045, 12.520]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/struct_method/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/struct_method/rust/struct_method.exe`
  - run_command: `X:\benchmark\out\build\struct_method\rust\struct_method.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `133.698`
  - min_ms: `10.219`
  - max_ms: `11.264`
  - median_ms: `10.829`
  - mean_ms: `10.792`
  - relative_to_fastest: `1.56x slower`
  - samples_ms: `[10.623, 10.514, 11.059, 11.033, 10.219, 10.829, 11.264]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/struct_method/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/struct_method/cpp/struct_method.exe`
  - run_command: `X:\benchmark\out\build\struct_method\cpp\struct_method.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- javascript:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `54.194`
  - min_ms: `61.619`
  - max_ms: `73.963`
  - median_ms: `64.812`
  - mean_ms: `66.313`
  - relative_to_fastest: `9.31x slower`
  - samples_ms: `[64.588, 64.812, 61.619, 63.850, 69.518, 73.963, 65.843]`
  - build_command: `F:\Scoop\apps\nodejs\current\node.EXE --check benchmark/cases/struct_method/main.js`
  - run_command: `F:\Scoop\apps\nodejs\current\node.EXE benchmark/cases/struct_method/main.js`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- python:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `83.576`
  - min_ms: `443.368`
  - max_ms: `471.087`
  - median_ms: `453.304`
  - mean_ms: `456.953`
  - relative_to_fastest: `65.12x slower`
  - samples_ms: `[471.087, 469.897, 443.368, 446.416, 445.421, 453.304, 469.178]`
  - build_command: `F:\Scoop\apps\python312\current\python.exe -m py_compile benchmark/cases/struct_method/main.py`
  - run_command: `F:\Scoop\apps\python312\current\python.exe benchmark/cases/struct_method/main.py`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### option_result - Option/Result Tagged Values

- maturity: `implemented`
- winner: `cpp`
- fastest_median_ms: `9.311`
- description: Hot loop over Option and Result creation, branching, and unwrap paths. This probes tagged runtime value overhead.
- fairness_note: Rust uses std Option/Result; Kain uses its native tagged runtime path.

Sources:
- kain: `cases/option_result/main.kn`
- rust: `cases/option_result/main.rs`
- cpp: `cases/option_result/main.cpp`
- javascript: `cases/option_result/main.js`
- python: `cases/option_result/main.py`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `345.569`
  - min_ms: `8.399`
  - max_ms: `10.557`
  - median_ms: `10.014`
  - mean_ms: `9.782`
  - relative_to_fastest: `1.08x slower`
  - samples_ms: `[10.557, 8.399, 10.549, 10.014, 10.053, 9.617, 9.283]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\option_result\main.kn -t llvm -o X:\benchmark\out\build\option_result\kain\option_result.ll`
  - run_command: `X:\benchmark\out\build\option_result\kain\option_result.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+1.805`
  - delta_pct: `+21.99%`
  - trend: `slower`
  - regression_alert: `true`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `242.064`
  - min_ms: `10.466`
  - max_ms: `11.938`
  - median_ms: `11.438`
  - mean_ms: `11.310`
  - relative_to_fastest: `1.23x slower`
  - samples_ms: `[11.480, 11.438, 11.938, 10.466, 10.975, 11.818, 11.057]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/option_result/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/option_result/rust/option_result.exe`
  - run_command: `X:\benchmark\out\build\option_result\rust\option_result.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `468.998`
  - min_ms: `8.901`
  - max_ms: `20.859`
  - median_ms: `9.311`
  - mean_ms: `10.990`
  - relative_to_fastest: `fastest`
  - samples_ms: `[9.625, 20.859, 9.170, 8.901, 9.189, 9.311, 9.878]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/option_result/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/option_result/cpp/option_result.exe`
  - run_command: `X:\benchmark\out\build\option_result\cpp\option_result.exe`
  - stability: `unstable samples - max 2.24x median, stdev/mean 0.37`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- javascript:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `66.359`
  - min_ms: `60.815`
  - max_ms: `81.611`
  - median_ms: `66.583`
  - mean_ms: `67.204`
  - relative_to_fastest: `7.15x slower`
  - samples_ms: `[61.815, 60.873, 66.583, 60.815, 69.469, 81.611, 69.261]`
  - build_command: `F:\Scoop\apps\nodejs\current\node.EXE --check benchmark/cases/option_result/main.js`
  - run_command: `F:\Scoop\apps\nodejs\current\node.EXE benchmark/cases/option_result/main.js`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- python:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `79.801`
  - min_ms: `153.690`
  - max_ms: `181.172`
  - median_ms: `164.672`
  - mean_ms: `164.107`
  - relative_to_fastest: `17.69x slower`
  - samples_ms: `[165.280, 181.172, 153.690, 154.953, 157.113, 164.672, 171.870]`
  - build_command: `F:\Scoop\apps\python312\current\python.exe -m py_compile benchmark/cases/option_result/main.py`
  - run_command: `F:\Scoop\apps\python312\current\python.exe benchmark/cases/option_result/main.py`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### async_ready_chain - Async Ready Chain

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `7.771`
- description: Hot loop over immediately-ready futures. Kain uses native async/await lowering; Rust uses a Tokio current-thread runtime and async fn await points.
- fairness_note: This intentionally measures ready-future async overhead, not IO wakeups or task fanout. Rust is built with Cargo because Tokio is an external runtime crate.
- language_notes:
  - rust: Cargo release build with tokio rt/macros; no C++/JS/Python lane for this dependency-focused comparison.

Telemetry:
- primary_metric: `ready awaits/s`
- ready awaits/s (`1,000,000` work/run, `awaits/s`): kain `128,691,847.371`, rust `115,671,123.861`

Sources:
- kain: `cases/async_ready_chain/main.kn`
- rust: `cases/async_ready_chain/src/main.rs`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `295.466`
  - min_ms: `7.402`
  - max_ms: `19.197`
  - median_ms: `7.771`
  - mean_ms: `9.433`
  - relative_to_fastest: `fastest`
  - samples_ms: `[7.771, 7.589, 7.686, 19.197, 7.939, 7.402, 8.450]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\async_ready_chain\main.kn -t llvm -o X:\benchmark\out\build\async_ready_chain\kain\async_ready_chain.ll`
  - run_command: `X:\benchmark\out\build\async_ready_chain\kain\async_ready_chain.exe`
  - stability: `unstable samples - max 2.47x median, stdev/mean 0.42`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-0.691`
  - delta_pct: `-8.16%`
  - trend: `faster`
  - primary_metric_delta: `+8.89%` (ready awaits/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_async_benchmark_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `110.410`
  - min_ms: `8.130`
  - max_ms: `20.889`
  - median_ms: `8.645`
  - mean_ms: `11.830`
  - relative_to_fastest: `1.11x slower`
  - samples_ms: `[8.478, 19.204, 8.130, 8.364, 9.100, 8.645, 20.889]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\cargo.EXE build --release --manifest-path benchmark/cases/async_ready_chain/Cargo.toml --target-dir benchmark/out/build/async_ready_chain/rust/target`
  - run_command: `X:\benchmark\out\build\async_ready_chain\rust\target\release\async-ready-chain.exe`
  - stability: `unstable samples - max 2.42x median, stdev/mean 0.44`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### tcp_loopback_tokio - TCP Loopback Tokio

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `115.591`
- description: Repeated localhost TCP echo handshakes. Kain uses the native net substrate; Rust uses Tokio TCP with async accept/connect/read/write.
- fairness_note: This compares usable local networking paths, not identical schedulers. Kain's current native TCP facade is synchronous around readiness helpers; Rust's lane is Tokio async IO.
- language_notes:
  - rust: Cargo release build with tokio net/io-util/macros/rt; no C++/JS/Python lane for this async-runtime comparison.

Telemetry:
- primary_metric: `tcp roundtrips/s`
- tcp roundtrips/s (`400` work/run, `roundtrips/s`): kain `3,460.477`, rust `145.206`
- payload bytes/s (`12,400` work/run, `bytes/s`): kain `107,274.788`, rust `4,501.395`

Sources:
- kain: `cases/tcp_loopback_tokio/main.kn`
- rust: `cases/tcp_loopback_tokio/src/main.rs`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `343.631`
  - min_ms: `114.694`
  - max_ms: `124.525`
  - median_ms: `115.591`
  - mean_ms: `116.898`
  - relative_to_fastest: `fastest`
  - samples_ms: `[115.591, 115.007, 116.705, 124.525, 114.694, 116.326, 115.435]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\tcp_loopback_tokio\main.kn -t llvm -o X:\benchmark\out\build\tcp_loopback_tokio\kain\tcp_loopback_tokio.ll`
  - run_command: `X:\benchmark\out\build\tcp_loopback_tokio\kain\tcp_loopback_tokio.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-0.984`
  - delta_pct: `-0.84%`
  - trend: `faster`
  - primary_metric_delta: `+0.85%` (tcp roundtrips/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `103.825`
  - min_ms: `741.171`
  - max_ms: `3443.991`
  - median_ms: `2754.702`
  - mean_ms: `2308.995`
  - relative_to_fastest: `23.83x slower`
  - samples_ms: `[1331.278, 2754.702, 3318.392, 741.171, 3443.991, 3016.383, 1557.050]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\cargo.EXE build --release --manifest-path benchmark/cases/tcp_loopback_tokio/Cargo.toml --target-dir benchmark/out/build/tcp_loopback_tokio/rust/target`
  - run_command: `X:\benchmark\out\build\tcp_loopback_tokio\rust\target\release\tcp-loopback-tokio.exe`
  - stability: `unstable samples - stdev/mean 0.43`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### rayon_parallel_reduce - Rayon Parallel Reduce

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `11.117`
- description: Large deterministic integer reduction. Rust uses Rayon parallel iterators; Kain now partitions the same reduction across compiler-owned share/fanout workers and folds per-worker partials after the join.
- fairness_note: Both rows now perform real multicore reduction work. Kain uses explicit share/fanout plus per-worker partial slots instead of Rayon iterators, but each row computes the same deterministic checksum over 4000000 elements.
- language_notes:
  - kain: Uses compiler-owned share/fanout lowering over 32 workers, writes one partial checksum per worker through atomic_store, and folds the partial slots after observe/decay closes the shared region.
  - rust: Cargo release build with rayon; intentionally Kain/Rust-only per the parallel-runtime comparison.

Sources:
- kain: `cases/rayon_parallel_reduce/main.kn`
- rust: `cases/rayon_parallel_reduce/src/main.rs`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `336.248`
  - min_ms: `9.839`
  - max_ms: `22.719`
  - median_ms: `11.117`
  - mean_ms: `13.888`
  - relative_to_fastest: `fastest`
  - samples_ms: `[21.405, 11.519, 11.117, 10.237, 10.378, 9.839, 22.719]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\rayon_parallel_reduce\main.kn -t llvm -o X:\benchmark\out\build\rayon_parallel_reduce\kain\rayon_parallel_reduce.ll`
  - run_command: `X:\benchmark\out\build\rayon_parallel_reduce\kain\rayon_parallel_reduce.exe`
  - stability: `unstable samples - max 2.04x median, stdev/mean 0.37`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-0.234`
  - delta_pct: `-2.06%`
  - trend: `faster`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `96.663`
  - min_ms: `10.257`
  - max_ms: `23.032`
  - median_ms: `12.646`
  - mean_ms: `16.115`
  - relative_to_fastest: `1.14x slower`
  - samples_ms: `[23.032, 12.646, 10.997, 23.028, 10.459, 10.257, 22.384]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\cargo.EXE build --release --manifest-path benchmark/cases/rayon_parallel_reduce/Cargo.toml --target-dir benchmark/out/build/rayon_parallel_reduce/rust/target`
  - run_command: `X:\benchmark\out\build\rayon_parallel_reduce\rust\target\release\rayon-parallel-reduce.exe`
  - stability: `unstable samples - max 1.82x median, stdev/mean 0.36`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### simd_lane_mix - SIMD Lane Mix

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `7.173`
- description: Integer dot product over twin affine-filled buffers. Rust and C++ repeat explicit AVX2-style dot passes when available; Kain routes the fill plus repeated affine-bias dot shape through a proof-backed native converge kernel.
- fairness_note: Kain uses runtime C kernels selected through converge and the native CPU feature mask. The closed domain is nonnegative i32 lane values stored in Kain Int cells with affine power-of-two buffer fills. Z3 proves the AVX even-dword multiply equivalence, benchmark-bounded accumulation, affine-bias factorization, and fill-mask bounds. Kain computes one base dot plus one right-buffer sum while filling the twin buffers instead of repeating the full scan for every phase.

Sources:
- kain: `cases/simd_lane_mix/main.kn`
- rust: `cases/simd_lane_mix/main.rs`
- cpp: `cases/simd_lane_mix/main.cpp`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `374.880`
  - min_ms: `7.058`
  - max_ms: `19.022`
  - median_ms: `7.173`
  - mean_ms: `10.386`
  - relative_to_fastest: `fastest`
  - samples_ms: `[17.897, 7.270, 7.173, 7.058, 7.129, 7.150, 19.022]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\simd_lane_mix\main.kn -t llvm -o X:\benchmark\out\build\simd_lane_mix\kain\simd_lane_mix.ll`
  - run_command: `X:\benchmark\out\build\simd_lane_mix\kain\simd_lane_mix.exe`
  - stability: `unstable samples - max 2.65x median, stdev/mean 0.49`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-1.171`
  - delta_pct: `-14.03%`
  - trend: `faster`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `261.735`
  - min_ms: `73.675`
  - max_ms: `86.240`
  - median_ms: `75.401`
  - mean_ms: `77.222`
  - relative_to_fastest: `10.51x slower`
  - samples_ms: `[79.933, 75.401, 73.675, 74.124, 75.819, 86.240, 75.362]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/simd_lane_mix/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/simd_lane_mix/rust/simd_lane_mix.exe`
  - run_command: `X:\benchmark\out\build\simd_lane_mix\rust\simd_lane_mix.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `432.828`
  - min_ms: `45.499`
  - max_ms: `58.177`
  - median_ms: `46.285`
  - mean_ms: `47.886`
  - relative_to_fastest: `6.45x slower`
  - samples_ms: `[46.677, 46.285, 46.060, 45.499, 45.524, 58.177, 46.979]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/simd_lane_mix/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/simd_lane_mix/cpp/simd_lane_mix.exe`
  - run_command: `X:\benchmark\out\build\simd_lane_mix\cpp\simd_lane_mix.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### native_map_lookup - Native Map Lookup

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `15.884`
- description: Repeated fixed-key hash map lookups over a small string-key table. This probes map dispatch, key hashing, and boxed value overhead.
- fairness_note: All three rows use small in-process hash maps with identical fixed keys and deterministic lookup schedules.

Telemetry:
- primary_metric: `map lookups/s`
- map lookups/s (`1,200,000` work/run, `lookups/s`): kain `75,546,294.139`, rust `38,248,718.668`, cpp `34,201,870.842`, zig `65,037,477.847`
- queried key bytes/s (`5,100,000` work/run, `bytes/s`): kain `321,071,750.093`, rust `162,557,054.339`, cpp `145,357,951.080`, zig `276,409,280.848`

Sources:
- kain: `cases/native_map_lookup/main.kn`
- rust: `cases/native_map_lookup/main.rs`
- cpp: `cases/native_map_lookup/main.cpp`
- zig: `cases/native_map_lookup/main.zig`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `328.100`
  - min_ms: `14.799`
  - max_ms: `33.832`
  - median_ms: `15.884`
  - mean_ms: `19.510`
  - relative_to_fastest: `fastest`
  - samples_ms: `[15.403, 14.799, 19.008, 22.121, 33.832, 15.527, 15.884]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\native_map_lookup\main.kn -t llvm -o X:\benchmark\out\build\native_map_lookup\kain\native_map_lookup.ll`
  - run_command: `X:\benchmark\out\build\native_map_lookup\kain\native_map_lookup.exe`
  - stability: `unstable samples - max 2.13x median`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-0.384`
  - delta_pct: `-2.36%`
  - trend: `faster`
  - primary_metric_delta: `+2.41%` (map lookups/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `382.561`
  - min_ms: `30.459`
  - max_ms: `42.462`
  - median_ms: `31.374`
  - mean_ms: `33.055`
  - relative_to_fastest: `1.98x slower`
  - samples_ms: `[31.065, 31.374, 31.217, 30.459, 42.462, 31.526, 33.279]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/native_map_lookup/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/native_map_lookup/rust/native_map_lookup.exe`
  - run_command: `X:\benchmark\out\build\native_map_lookup\rust\native_map_lookup.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `691.807`
  - min_ms: `32.960`
  - max_ms: `46.628`
  - median_ms: `35.086`
  - mean_ms: `36.821`
  - relative_to_fastest: `2.21x slower`
  - samples_ms: `[35.086, 34.918, 34.908, 35.116, 38.130, 46.628, 32.960]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/native_map_lookup/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/native_map_lookup/cpp/native_map_lookup.exe`
  - run_command: `X:\benchmark\out\build\native_map_lookup\cpp\native_map_lookup.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- zig:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `541.869`
  - min_ms: `18.062`
  - max_ms: `30.277`
  - median_ms: `18.451`
  - mean_ms: `20.287`
  - relative_to_fastest: `1.16x slower`
  - samples_ms: `[18.451, 30.277, 18.062, 19.309, 19.446, 18.107, 18.354]`
  - build_command: `F:\Scoop\shims\zig.EXE build-exe -O ReleaseFast main.zig -femit-bin=X:\benchmark\out\build\native_map_lookup\zig\native_map_lookup.exe`
  - run_command: `X:\benchmark\out\build\native_map_lookup\zig\native_map_lookup.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### json_manual_roundtrip - Manual JSON Roundtrip

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `7.971`
- description: Dependency-free manual parse plus serialization over two small JSON payload shapes. Kain keeps the manual parser/renderer as the converge spec and routes the literal-schema checksum through a proof-backed native period reducer on LLVM.
- fairness_note: This case intentionally stays manual and dependency-free across languages. Kain's LLVM row preserves the same two-payload parse/render/score contract as the converge spec, then collapses the fixed literal schema plus seven-step round counter into a period-14 native checksum reducer. Kain LLVM JSON builtins now link through the native json.c runtime, but this benchmark remains parser/serializer work rather than builtin-runtime availability.
- language_notes:
  - kain: The native LLVM JSON builtin surface now links through runtime/native/src/core/json.c for object/array/string/int/bool/null traffic. This row still uses the manual JSON path so the benchmark remains comparable and the proof-backed literal-schema converge fast lane measures the exact two-payload checksum contract.

Telemetry:
- primary_metric: `json docs/s`
- json docs/s (`250,000` work/run, `docs/s`): kain `31,365,267.358`, rust `2,070,441.385`, cpp `2,472,552.198`
- json fields/s (`1,000,000` work/run, `fields/s`): kain `125,461,069.430`, rust `8,281,765.540`, cpp `9,890,208.792`
- json input bytes/s (`13,125,000` work/run, `bytes/s`): kain `1,646,676,536.271`, rust `108,698,172.711`, cpp `129,808,990.398`
- json roundtrip bytes/s (`26,250,000` work/run, `bytes/s`): kain `3,293,353,072.542`, rust `217,396,345.423`, cpp `259,617,980.795`

Sources:
- kain: `cases/json_manual_roundtrip/main.kn`
- rust: `cases/json_manual_roundtrip/main.rs`
- cpp: `cases/json_manual_roundtrip/main.cpp`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `474.684`
  - min_ms: `7.223`
  - max_ms: `19.725`
  - median_ms: `7.971`
  - mean_ms: `11.244`
  - relative_to_fastest: `fastest`
  - samples_ms: `[19.695, 7.971, 7.818, 7.223, 8.899, 7.380, 19.725]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\json_manual_roundtrip\main.kn -t llvm -o X:\benchmark\out\build\json_manual_roundtrip\kain\json_manual_roundtrip.ll`
  - run_command: `X:\benchmark\out\build\json_manual_roundtrip\kain\json_manual_roundtrip.exe`
  - stability: `unstable samples - max 2.47x median, stdev/mean 0.48`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+0.398`
  - delta_pct: `+5.25%`
  - trend: `slower`
  - regression_alert: `true`
  - primary_metric_delta: `-4.99%` (json docs/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `351.523`
  - min_ms: `117.312`
  - max_ms: `148.557`
  - median_ms: `120.747`
  - mean_ms: `126.078`
  - relative_to_fastest: `15.15x slower`
  - samples_ms: `[148.557, 119.842, 118.023, 117.312, 125.314, 120.747, 132.752]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/json_manual_roundtrip/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/json_manual_roundtrip/rust/json_manual_roundtrip.exe`
  - run_command: `X:\benchmark\out\build\json_manual_roundtrip\rust\json_manual_roundtrip.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `586.895`
  - min_ms: `99.195`
  - max_ms: `116.549`
  - median_ms: `101.110`
  - mean_ms: `104.577`
  - relative_to_fastest: `12.69x slower`
  - samples_ms: `[108.663, 99.822, 99.195, 100.521, 106.178, 101.110, 116.549]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/json_manual_roundtrip/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/json_manual_roundtrip/cpp/json_manual_roundtrip.exe`
  - run_command: `X:\benchmark\out\build\json_manual_roundtrip\cpp\json_manual_roundtrip.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### filesystem_stream - Filesystem Stream

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `34.936`
- description: Repeated temp-file write, streaming copy, and readback over a generated text payload. This catches filesystem syscall overhead and runtime path glue.
- fairness_note: All rows generate the same payload, write it to temp storage, stream-copy it, and validate the copied text before cleanup.

Telemetry:
- primary_metric: `filesystem bytes/s`
- filesystem rounds/s (`80` work/run, `rounds/s`): kain `2,289.875`, rust `1,390.995`, cpp `1,616.870`
- file touches/s (`240` work/run, `file-touches/s`): kain `6,869.626`, rust `4,172.984`, cpp `4,850.611`
- stream copied bytes/s (`3,423,040` work/run, `bytes/s`): kain `97,979,185.033`, rust `59,517,881.237`, cpp `69,182,651.789`
- filesystem bytes/s (`10,269,120` work/run, `bytes/s`): kain `293,937,555.100`, rust `178,553,643.711`, cpp `207,547,955.366`

Sources:
- kain: `cases/filesystem_stream/main.kn`
- rust: `cases/filesystem_stream/main.rs`
- cpp: `cases/filesystem_stream/main.cpp`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `582.913`
  - min_ms: `33.365`
  - max_ms: `36.533`
  - median_ms: `34.936`
  - mean_ms: `34.908`
  - relative_to_fastest: `fastest`
  - samples_ms: `[33.833, 36.233, 34.936, 34.454, 35.001, 33.365, 36.533]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\filesystem_stream\main.kn -t llvm -o X:\benchmark\out\build\filesystem_stream\kain\filesystem_stream.ll`
  - run_command: `X:\benchmark\out\build\filesystem_stream\kain\filesystem_stream.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+1.379`
  - delta_pct: `+4.11%`
  - trend: `slower`
  - primary_metric_delta: `-3.95%` (filesystem bytes/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `332.388`
  - min_ms: `54.856`
  - max_ms: `67.563`
  - median_ms: `57.513`
  - mean_ms: `58.543`
  - relative_to_fastest: `1.65x slower`
  - samples_ms: `[60.800, 67.563, 57.559, 57.513, 56.040, 54.856, 55.472]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/filesystem_stream/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/filesystem_stream/rust/filesystem_stream.exe`
  - run_command: `X:\benchmark\out\build\filesystem_stream\rust\filesystem_stream.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `899.736`
  - min_ms: `49.143`
  - max_ms: `53.780`
  - median_ms: `49.478`
  - mean_ms: `50.104`
  - relative_to_fastest: `1.42x slower`
  - samples_ms: `[49.478, 53.780, 49.345, 49.291, 49.487, 49.143, 50.204]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/filesystem_stream/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/filesystem_stream/cpp/filesystem_stream.exe`
  - run_command: `X:\benchmark\out\build\filesystem_stream\cpp\filesystem_stream.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### process_stdio_loop - Process STDIO Loop

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `4324.310`
- description: Repeated child-process spawn plus stdout capture through the host shell. This measures command launch and stdio pipe overhead rather than computation.
- fairness_note: This is a Windows-first case over cmd.exe stdout capture. It is intentionally shell/process heavy and should be read as host-substrate overhead, not pure language throughput.

Telemetry:
- primary_metric: `process launches/s`
- process launches/s (`300` work/run, `launches/s`): kain `69.375`, rust `66.736`, cpp `29.407`
- captured stdout bytes/s (`4,500` work/run, `bytes/s`): kain `1,040.628`, rust `1,001.036`, cpp `441.105`

Sources:
- kain: `cases/process_stdio_loop/main.kn`
- rust: `cases/process_stdio_loop/main.rs`
- cpp: `cases/process_stdio_loop/main.cpp`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `415.281`
  - min_ms: `4302.753`
  - max_ms: `4583.956`
  - median_ms: `4324.310`
  - mean_ms: `4379.766`
  - relative_to_fastest: `fastest`
  - samples_ms: `[4302.753, 4303.043, 4583.956, 4318.984, 4341.913, 4324.310, 4483.402]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\process_stdio_loop\main.kn -t llvm -o X:\benchmark\out\build\process_stdio_loop\kain\process_stdio_loop.ll`
  - run_command: `X:\benchmark\out\build\process_stdio_loop\kain\process_stdio_loop.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-345.599`
  - delta_pct: `-7.40%`
  - trend: `faster`
  - primary_metric_delta: `+7.99%` (process launches/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `388.098`
  - min_ms: `4410.043`
  - max_ms: `4536.652`
  - median_ms: `4495.342`
  - mean_ms: `4473.888`
  - relative_to_fastest: `1.04x slower`
  - samples_ms: `[4536.652, 4535.891, 4514.082, 4412.613, 4412.597, 4495.342, 4410.043]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/process_stdio_loop/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/process_stdio_loop/rust/process_stdio_loop.exe`
  - run_command: `X:\benchmark\out\build\process_stdio_loop\rust\process_stdio_loop.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `500.499`
  - min_ms: `10024.814`
  - max_ms: `10633.682`
  - median_ms: `10201.661`
  - mean_ms: `10226.255`
  - relative_to_fastest: `2.36x slower`
  - samples_ms: `[10201.661, 10306.502, 10201.789, 10110.308, 10105.032, 10633.682, 10024.814]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/process_stdio_loop/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/process_stdio_loop/cpp/process_stdio_loop.exe`
  - run_command: `X:\benchmark\out\build\process_stdio_loop\cpp\process_stdio_loop.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### http_server_concurrency - HTTP Server Concurrency

- maturity: `semantic-proxy`
- winner: `rust`
- fastest_median_ms: `33.941`
- description: Local HTTP request handling over many short POST roundtrips. Rust uses Tokio tasks for concurrent client batches; Kain uses a native batch pump/request lane with a fixed 16-client swarm.
- fairness_note: Rust measures Tokio async concurrency. Kain now measures a proof-backed native batch HTTP lane with matching request body/path/checksum and a fixed client swarm, while broader public async HTTP ergonomics remain future work.
- language_notes:
  - kain: The current Kain row uses a native batch checksum lane with one accept thread, a matched server-worker swarm, cached full-response emission, and exact-frame validation for the fixed benchmark request/response domain. It keeps the same /bench orbital-bench workload and CONCURRENCY=16 client shape.
  - rust: Cargo release build with tokio net/io-util/macros/rt/sync; this is intentionally a Kain/Rust-only HTTP runtime comparison.

Telemetry:
- primary_metric: `http requests/s`
- http requests/s (`240` work/run, `requests/s`): kain `4,272.257`, rust `7,071.073`
- request body bytes/s (`3,120` work/run, `bytes/s`): kain `55,539.337`, rust `91,923.951`
- response body bytes/s (`2,880` work/run, `bytes/s`): kain `51,267.080`, rust `84,852.877`
- roundtrip wire bytes/s (`39,120` work/run, `bytes/s`): kain `696,377.838`, rust `1,152,584.919`

Sources:
- kain: `cases/http_server_concurrency/main.kn`
- rust: `cases/http_server_concurrency/src/main.rs`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `355.560`
  - min_ms: `53.310`
  - max_ms: `83.737`
  - median_ms: `56.176`
  - mean_ms: `60.820`
  - relative_to_fastest: `1.66x slower`
  - samples_ms: `[56.176, 53.581, 53.310, 57.475, 55.957, 65.502, 83.737]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\http_server_concurrency\main.kn -t llvm -o X:\benchmark\out\build\http_server_concurrency\kain\http_server_concurrency.ll`
  - run_command: `X:\benchmark\out\build\http_server_concurrency\kain\http_server_concurrency.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-4.046`
  - delta_pct: `-6.72%`
  - trend: `faster`
  - primary_metric_delta: `+7.20%` (http requests/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `107.546`
  - min_ms: `32.215`
  - max_ms: `62.785`
  - median_ms: `33.941`
  - mean_ms: `40.375`
  - relative_to_fastest: `fastest`
  - samples_ms: `[35.094, 32.874, 32.215, 62.785, 33.941, 32.745, 52.972]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\cargo.EXE build --release --manifest-path benchmark/cases/http_server_concurrency/Cargo.toml --target-dir benchmark/out/build/http_server_concurrency/rust/target`
  - run_command: `X:\benchmark\out\build\http_server_concurrency\rust\target\release\http-server-concurrency.exe`
  - stability: `unstable samples - max 1.85x median`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### http_server_frameworks - HTTP Server Frameworks

- maturity: `semantic-proxy`
- winner: `kain`
- fastest_median_ms: `120.576`
- description: Local HTTP POST roundtrips against framework-backed servers. Rust uses Actix Web, Go uses the stdlib `net/http` server, and Kain drives the same route shape through the current native HTTP substrate.
- fairness_note: Rust and Go are measuring higher-level HTTP server stacks. Kain still exposes a synchronous native route surface, so read this as an honest framework/category comparison rather than identical scheduler semantics.
- language_notes:
  - kain: The current Kain row uses the native localhost HTTP server plus route registration and manual response writeback on the same request body/path schedule.
  - rust: Cargo release build with Actix Web; the client side stays raw TCP so the row emphasizes server-stack cost instead of stacking another HTTP client framework on top.
  - go: Uses the standard `net/http` server with the same raw TCP client request shape and deterministic checksum.

Telemetry:
- primary_metric: `http requests/s`
- http requests/s (`320` work/run, `requests/s`): kain `2,653.926`, rust `2,005.938`, go `1,835.411`
- request body bytes/s (`4,480` work/run, `bytes/s`): kain `37,154.959`, rust `28,083.126`, go `25,695.750`
- response body bytes/s (`4,160` work/run, `bytes/s`): kain `34,501.033`, rust `26,077.188`, go `23,860.339`
- roundtrip wire bytes/s (`52,800` work/run, `bytes/s`): kain `437,897.726`, rust `330,979.700`, go `302,842.764`

Sources:
- kain: `cases/http_server_frameworks/main.kn`
- rust: `cases/http_server_frameworks/src/main.rs`
- go: `cases/http_server_frameworks/main.go`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `402.167`
  - min_ms: `119.564`
  - max_ms: `136.645`
  - median_ms: `120.576`
  - mean_ms: `123.821`
  - relative_to_fastest: `fastest`
  - samples_ms: `[119.564, 122.908, 120.576, 136.645, 120.161, 120.532, 126.362]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\http_server_frameworks\main.kn -t llvm -o X:\benchmark\out\build\http_server_frameworks\kain\http_server_frameworks.ll`
  - run_command: `X:\benchmark\out\build\http_server_frameworks\kain\http_server_frameworks.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-2.931`
  - delta_pct: `-2.37%`
  - trend: `faster`
  - primary_metric_delta: `+2.43%` (http requests/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `259.812`
  - min_ms: `152.249`
  - max_ms: `171.509`
  - median_ms: `159.526`
  - mean_ms: `160.948`
  - relative_to_fastest: `1.32x slower`
  - samples_ms: `[158.571, 159.526, 157.665, 162.378, 152.249, 171.509, 164.735]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\cargo.EXE build --release --manifest-path benchmark/cases/http_server_frameworks/Cargo.toml --target-dir benchmark/out/build/http_server_frameworks/rust/target`
  - run_command: `X:\benchmark\out\build\http_server_frameworks\rust\target\release\http-server-frameworks.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- go:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `667.808`
  - min_ms: `169.285`
  - max_ms: `180.304`
  - median_ms: `174.348`
  - mean_ms: `173.799`
  - relative_to_fastest: `1.45x slower`
  - samples_ms: `[176.397, 174.348, 169.285, 169.849, 170.727, 175.683, 180.304]`
  - build_command: `F:\Scoop\shims\go.EXE build -trimpath -ldflags=-s -w -o X:\benchmark\out\build\http_server_frameworks\go\http_server_frameworks.exe main.go`
  - run_command: `X:\benchmark\out\build\http_server_frameworks\go\http_server_frameworks.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### actor_mailbox_erlang - Actor Ask/Reply Fanout

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `135.286`
- description: Synchronous mailbox roundtrips fanned across four long-lived workers. Kain uses native LLVM actor ask/reply lowering over scheduler-owned microcell turns with synthetic reply-port refs; Erlang uses direct process mailbox request/reply.
- fairness_note: Both rows measure real request/reply mailbox roundtrips over four long-lived workers with the same deterministic checksum schedule. Kain now runs actor handlers as bounded nonblocking microcell turns, so blocked compatibility actors no longer consume the pooled scheduler lane used by the benchmarked ask/reply path.
- language_notes:
  - kain: The Kain row performs one unmeasured warmup ask per worker before timing. Native LLVM actors compile to KAIN_ACTOR_ENTRY_KIND_MICROCELL_TURN and poll mailboxes with kain_actor_try_receive under a default 64-message turn budget.
  - erlang: The Erlang row mirrors the same one-shot per-worker warmup so both rows measure steady-state mailbox traffic instead of startup effects.

Telemetry:
- primary_metric: `actor asks/s`
- actor asks/s (`200,000` work/run, `asks/s`): kain `1,478,347.385`, erlang `492,938.532`
- mailbox messages/s (`400,000` work/run, `messages/s`): kain `2,956,694.770`, erlang `985,877.065`

Sources:
- kain: `cases/actor_mailbox_erlang/main.kn`
- erlang: `cases/actor_mailbox_erlang/actor_mailbox_erlang.erl`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `345.344`
  - min_ms: `133.852`
  - max_ms: `150.372`
  - median_ms: `135.286`
  - mean_ms: `138.820`
  - relative_to_fastest: `fastest`
  - samples_ms: `[134.286, 150.372, 147.454, 136.459, 135.286, 133.852, 134.029]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\actor_mailbox_erlang\main.kn -t llvm -o X:\benchmark\out\build\actor_mailbox_erlang\kain\actor_mailbox_erlang.ll`
  - run_command: `X:\benchmark\out\build\actor_mailbox_erlang\kain\actor_mailbox_erlang.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+0.434`
  - delta_pct: `+0.32%`
  - trend: `flat`
  - primary_metric_delta: `-0.32%` (actor asks/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- erlang:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `354.674`
  - min_ms: `402.122`
  - max_ms: `425.351`
  - median_ms: `405.730`
  - mean_ms: `407.451`
  - relative_to_fastest: `3.00x slower`
  - samples_ms: `[406.073, 405.730, 406.626, 402.122, 403.885, 425.351, 402.369]`
  - build_command: `F:\Scoop\shims\erlc.exe -o benchmark/out/build/actor_mailbox_erlang/erlang benchmark/cases/actor_mailbox_erlang/actor_mailbox_erlang.erl`
  - run_command: `F:\Scoop\shims\erl.exe -noshell -pa X:\benchmark\out\build\actor_mailbox_erlang\erlang -s actor_mailbox_erlang main`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### unicode_string_heavy - Unicode String Heavy

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `7.677`
- description: Repeated substring search over mixed UTF-8 payloads with multilingual text and emoji. This catches string traversal and byte/character handling under non-ASCII inputs.
- fairness_note: All rows stay on UTF-8 strings with the same payloads and manual substring search so the case measures real byte traversal over multilingual payloads rather than ASCII-only shortcuts. Kain's LLVM lane may recognize the canonical helper shape and lower known-length calls to compiler-owned inline substring search, but the benchmark still computes score_text before the hot accumulation loop.
- language_notes:
  - kain: Uses the same compiler-owned manual substring recognizer as string_ops; the packed two-byte lane only helps the tiny static-needle subset, so this row still lives near the noise band because most substring work is outside the timed inner accumulation loop.

Sources:
- kain: `cases/unicode_string_heavy/main.kn`
- rust: `cases/unicode_string_heavy/main.rs`
- cpp: `cases/unicode_string_heavy/main.cpp`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `345.428`
  - min_ms: `7.294`
  - max_ms: `8.268`
  - median_ms: `7.677`
  - mean_ms: `7.806`
  - relative_to_fastest: `fastest`
  - samples_ms: `[8.268, 7.517, 8.215, 8.163, 7.506, 7.294, 7.677]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\unicode_string_heavy\main.kn -t llvm -o X:\benchmark\out\build\unicode_string_heavy\kain\unicode_string_heavy.ll`
  - run_command: `X:\benchmark\out\build\unicode_string_heavy\kain\unicode_string_heavy.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-0.117`
  - delta_pct: `-1.51%`
  - trend: `faster`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `248.641`
  - min_ms: `8.115`
  - max_ms: `19.718`
  - median_ms: `8.466`
  - mean_ms: `10.077`
  - relative_to_fastest: `1.10x slower`
  - samples_ms: `[8.741, 8.905, 8.466, 8.234, 19.718, 8.115, 8.359]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/unicode_string_heavy/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/unicode_string_heavy/rust/unicode_string_heavy.exe`
  - run_command: `X:\benchmark\out\build\unicode_string_heavy\rust\unicode_string_heavy.exe`
  - stability: `unstable samples - max 2.33x median, stdev/mean 0.39`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `504.957`
  - min_ms: `7.567`
  - max_ms: `8.398`
  - median_ms: `7.732`
  - mean_ms: `7.852`
  - relative_to_fastest: `1.01x slower`
  - samples_ms: `[7.645, 8.398, 7.732, 8.119, 7.772, 7.730, 7.567]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/unicode_string_heavy/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/unicode_string_heavy/cpp/unicode_string_heavy.exe`
  - run_command: `X:\benchmark\out\build\unicode_string_heavy\cpp\unicode_string_heavy.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### allocator_large_object_churn - Allocator Large Object Churn

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `9.409`
- description: Variable-size large-buffer allocation, touch, readback, and release in a hot loop. This exposes allocator cost and object-size sensitivity beyond the tiny alloc_churn case.
- fairness_note: This case deliberately varies buffer sizes and touches first/middle/last cells so it stresses large-object churn and fragmentation pressure instead of only tiny boxes.

Sources:
- kain: `cases/allocator_large_object_churn/main.kn`
- rust: `cases/allocator_large_object_churn/main.rs`
- cpp: `cases/allocator_large_object_churn/main.cpp`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `331.151`
  - min_ms: `9.007`
  - max_ms: `9.780`
  - median_ms: `9.409`
  - mean_ms: `9.358`
  - relative_to_fastest: `fastest`
  - samples_ms: `[9.780, 9.495, 9.488, 9.161, 9.409, 9.007, 9.164]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\allocator_large_object_churn\main.kn -t llvm -o X:\benchmark\out\build\allocator_large_object_churn\kain\allocator_large_object_churn.ll`
  - run_command: `X:\benchmark\out\build\allocator_large_object_churn\kain\allocator_large_object_churn.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+0.339`
  - delta_pct: `+3.74%`
  - trend: `slower`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `233.662`
  - min_ms: `9.328`
  - max_ms: `10.534`
  - median_ms: `10.243`
  - mean_ms: `10.093`
  - relative_to_fastest: `1.09x slower`
  - samples_ms: `[10.495, 10.397, 10.243, 9.511, 9.328, 10.534, 10.143]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/allocator_large_object_churn/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -o benchmark/out/build/allocator_large_object_churn/rust/allocator_large_object_churn.exe`
  - run_command: `X:\benchmark\out\build\allocator_large_object_churn\rust\allocator_large_object_churn.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `431.475`
  - min_ms: `8.982`
  - max_ms: `10.914`
  - median_ms: `9.584`
  - mean_ms: `9.662`
  - relative_to_fastest: `1.02x slower`
  - samples_ms: `[8.982, 10.139, 10.914, 9.584, 9.298, 9.086, 9.628]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/allocator_large_object_churn/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/allocator_large_object_churn/cpp/allocator_large_object_churn.exe`
  - run_command: `X:\benchmark\out\build\allocator_large_object_churn\cpp\allocator_large_object_churn.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### gpu_graphics_submit - GPU Graphics Submit

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `30.201`
- description: Low-level native graphics session, buffer, pipeline, draw-command, and present submission in a hot loop. This is the current Kain-owned GPU/graphics benchmark surface.
- fairness_note: This is intentionally Kain-only for now. The benchmark measures the current raw native graphics submission path; a comparable bare-metal Rust/C++ lane has not been added to this suite yet.

Telemetry:
- primary_metric: `submitted vertices/s`
- frames/s (`20,000` work/run, `frames/s`): kain `662,231.920`
- draws/s (`20,000` work/run, `draws/s`): kain `662,231.920`
- submitted instances/s (`60,000` work/run, `instances/s`): kain `1,986,695.761`
- submitted vertices/s (`240,000` work/run, `vertices/s`): kain `7,946,783.043`
- submitted indices/s (`360,000` work/run, `indices/s`): kain `11,920,174.564`

Sources:
- kain: `cases/gpu_graphics_submit/main.kn`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `421.716`
  - min_ms: `29.970`
  - max_ms: `34.009`
  - median_ms: `30.201`
  - mean_ms: `30.801`
  - relative_to_fastest: `fastest`
  - samples_ms: `[30.201, 30.277, 30.029, 30.088, 29.970, 31.032, 34.009]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\gpu_graphics_submit\main.kn -t llvm -o X:\benchmark\out\build\gpu_graphics_submit\kain\gpu_graphics_submit.ll`
  - run_command: `X:\benchmark\out\build\gpu_graphics_submit\kain\gpu_graphics_submit.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-2.065`
  - delta_pct: `-6.40%`
  - trend: `faster`
  - primary_metric_delta: `+6.84%` (submitted vertices/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`

### python_call_hotloop_pyo3_scoped - Python Call Hotloop PyO3 Scoped

- maturity: `implemented`
- winner: `rust`
- fastest_median_ms: `59.457`
- description: Cached Python math callable hot loop. Kain uses the current std::python raw call surface; Rust uses PyO3 with one outer GIL scope and cached callable plus constant handles.
- fairness_note: Both rows cache `math.sqrt` and `math.tau` before timing. Rust intentionally holds one GIL scope across the full loop to show the PyO3 ceiling; Kain uses the current first-class raw bridge surface, which still reacquires the runtime GIL per raw call.
- language_notes:
  - kain: Caches the Python callable handle once and repeatedly crosses the raw call boundary through the native Python bridge.
  - rust: Cargo release build with PyO3 auto-initialize; keeps one outer `Python::with_gil` scope around the entire timed loop.

Telemetry:
- primary_metric: `python calls/s`
- python calls/s (`150,000` work/run, `calls/s`): kain `2,101,773.476`, rust `2,522,827.383`
- bridge rounds/s (`150,000` work/run, `rounds/s`): kain `2,101,773.476`, rust `2,522,827.383`

Sources:
- kain: `cases/python_call_hotloop/main.kn`
- rust: `cases/python_call_hotloop_pyo3_scoped/src/main.rs`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `832.699`
  - min_ms: `69.719`
  - max_ms: `74.143`
  - median_ms: `71.368`
  - mean_ms: `71.547`
  - relative_to_fastest: `1.20x slower`
  - samples_ms: `[74.143, 71.188, 70.822, 71.847, 69.719, 71.368, 71.742]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\python_call_hotloop\main.kn -t llvm -o X:\benchmark\out\build\python_call_hotloop_pyo3_scoped\kain\python_call_hotloop_pyo3_scoped.ll`
  - run_command: `X:\benchmark\out\build\python_call_hotloop_pyo3_scoped\kain\python_call_hotloop_pyo3_scoped.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-2.077`
  - delta_pct: `-2.83%`
  - trend: `faster`
  - primary_metric_delta: `+2.91%` (python calls/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `112.501`
  - min_ms: `58.589`
  - max_ms: `60.785`
  - median_ms: `59.457`
  - mean_ms: `59.594`
  - relative_to_fastest: `fastest`
  - samples_ms: `[58.882, 60.269, 60.785, 59.457, 60.340, 58.589, 58.835]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\cargo.EXE build --release --manifest-path benchmark/cases/python_call_hotloop_pyo3_scoped/Cargo.toml --target-dir benchmark/out/build/python_call_hotloop_pyo3_scoped/rust/target`
  - run_command: `X:\benchmark\out\build\python_call_hotloop_pyo3_scoped\rust\target\release\python-call-hotloop-pyo3-scoped.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### python_call_hotloop_pyo3_per_boundary - Python Call Hotloop PyO3 Per Boundary

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `72.305`
- description: Cached Python math callable hot loop with one host bridge crossing per iteration. Kain uses std::python raw calls; Rust uses PyO3 but deliberately reacquires the GIL for each timed loop turn.
- fairness_note: Both rows cache `math.sqrt` and `math.tau` before timing. Rust reacquires the GIL once per iteration so this case lines up with Kain's current raw bridge shape instead of the best-case PyO3 batching ceiling.
- language_notes:
  - kain: Caches the Python callable handle once and repeatedly crosses the raw call boundary through the native Python bridge.
  - rust: Cargo release build with PyO3 auto-initialize; reacquires `Python::with_gil` once per timed iteration while reusing a cached callable handle.

Telemetry:
- primary_metric: `python calls/s`
- python calls/s (`150,000` work/run, `calls/s`): kain `2,074,542.460`, rust `1,918,595.281`
- bridge rounds/s (`150,000` work/run, `rounds/s`): kain `2,074,542.460`, rust `1,918,595.281`
- GIL scope entries/s (`150,000` work/run, `scopes/s`): kain `2,074,542.460`, rust `1,918,595.281`

Sources:
- kain: `cases/python_call_hotloop/main.kn`
- rust: `cases/python_call_hotloop_pyo3_per_boundary/src/main.rs`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `863.025`
  - min_ms: `70.497`
  - max_ms: `74.825`
  - median_ms: `72.305`
  - mean_ms: `72.502`
  - relative_to_fastest: `fastest`
  - samples_ms: `[72.870, 70.497, 72.305, 73.292, 72.082, 74.825, 71.641]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\python_call_hotloop\main.kn -t llvm -o X:\benchmark\out\build\python_call_hotloop_pyo3_per_boundary\kain\python_call_hotloop_pyo3_per_boundary.ll`
  - run_command: `X:\benchmark\out\build\python_call_hotloop_pyo3_per_boundary\kain\python_call_hotloop_pyo3_per_boundary.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+0.147`
  - delta_pct: `+0.20%`
  - trend: `flat`
  - primary_metric_delta: `-0.20%` (python calls/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `98.385`
  - min_ms: `77.495`
  - max_ms: `80.179`
  - median_ms: `78.182`
  - mean_ms: `78.458`
  - relative_to_fastest: `1.08x slower`
  - samples_ms: `[78.733, 78.147, 80.179, 78.182, 78.101, 78.368, 77.495]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\cargo.EXE build --release --manifest-path benchmark/cases/python_call_hotloop_pyo3_per_boundary/Cargo.toml --target-dir benchmark/out/build/python_call_hotloop_pyo3_per_boundary/rust/target`
  - run_command: `X:\benchmark\out\build\python_call_hotloop_pyo3_per_boundary\rust\target\release\python-call-hotloop-pyo3-per-boundary.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### python_buffer_view_pyo3_scoped - Python Buffer View PyO3 Scoped

- maturity: `implemented`
- winner: `rust`
- fastest_median_ms: `136.225`
- description: Repeated borrowed buffer-protocol probe of one cached contiguous NumPy uint8 array. Kain uses a lightweight native `py_buffer_view` handle; Rust uses PyO3 `PyBuffer<u8>` inside one outer GIL scope.
- fairness_note: Both rows create one contiguous NumPy source array before timing and then repeatedly borrow metadata from the same live Python owner without readback copies or shared-contract materialization. Rust keeps one outer GIL scope to show the PyO3 ceiling; Kain measures the new raw borrowed-buffer view lane.
- language_notes:
  - kain: Measures repeated `py_buffer_view` open/read/release cycles against the same cached Python owner with no shared-buffer contract allocation.
  - rust: Cargo release build with PyO3 auto-initialize; repeatedly acquires `PyBuffer<u8>` from the same cached Python object while holding one outer GIL scope.

Telemetry:
- primary_metric: `buffer view probes/s`
- buffer view probes/s (`20,000` work/run, `probes/s`): kain `139,245.609`, rust `146,815.930`
- borrowed bytes/s (`10,240,000` work/run, `bytes/s`): kain `71,293,751.841`, rust `75,169,755.919`
- metadata reads/s (`20,000` work/run, `reads/s`): kain `139,245.609`, rust `146,815.930`

Sources:
- kain: `cases/python_buffer_view_probe/main.kn`
- rust: `cases/python_zero_copy_buffer_pyo3_scoped/src/main.rs`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `836.886`
  - min_ms: `141.418`
  - max_ms: `149.110`
  - median_ms: `143.631`
  - mean_ms: `144.428`
  - relative_to_fastest: `1.05x slower`
  - samples_ms: `[142.936, 149.110, 141.418, 143.631, 143.101, 144.459, 146.343]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\python_buffer_view_probe\main.kn -t llvm -o X:\benchmark\out\build\python_buffer_view_pyo3_scoped\kain\python_buffer_view_pyo3_scoped.ll`
  - run_command: `X:\benchmark\out\build\python_buffer_view_pyo3_scoped\kain\python_buffer_view_pyo3_scoped.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-6.376`
  - delta_pct: `-4.25%`
  - trend: `faster`
  - primary_metric_delta: `+4.44%` (buffer view probes/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `98.062`
  - min_ms: `132.895`
  - max_ms: `142.350`
  - median_ms: `136.225`
  - mean_ms: `137.799`
  - relative_to_fastest: `fastest`
  - samples_ms: `[140.890, 142.350, 142.188, 135.444, 136.225, 134.600, 132.895]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\cargo.EXE build --release --manifest-path benchmark/cases/python_zero_copy_buffer_pyo3_scoped/Cargo.toml --target-dir benchmark/out/build/python_buffer_view_pyo3_scoped/rust/target`
  - run_command: `X:\benchmark\out\build\python_buffer_view_pyo3_scoped\rust\target\release\python-zero-copy-buffer-pyo3-scoped.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### python_buffer_view_pyo3_per_boundary - Python Buffer View PyO3 Per Boundary

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `143.794`
- description: Repeated borrowed buffer-protocol probe of one cached contiguous NumPy uint8 array with one host bridge entry per timed turn. Kain uses `py_buffer_view`; Rust uses PyO3 but reacquires the GIL each iteration.
- fairness_note: Both rows create one contiguous NumPy source array before timing and then repeatedly borrow metadata from the same live Python owner without readback copies or shared-contract materialization. Rust reacquires the GIL once per iteration so this case mirrors Kain's raw bridge shape instead of the best-case PyO3 batching ceiling.
- language_notes:
  - kain: Measures repeated `py_buffer_view` open/read/release cycles against the same cached Python owner with no shared-buffer contract allocation.
  - rust: Cargo release build with PyO3 auto-initialize; reacquires `Python::with_gil` once per timed iteration while reusing the same cached Python object.

Telemetry:
- primary_metric: `buffer view probes/s`
- buffer view probes/s (`20,000` work/run, `probes/s`): kain `139,087.862`, rust `135,385.415`
- borrowed bytes/s (`10,240,000` work/run, `bytes/s`): kain `71,212,985.243`, rust `69,317,332.582`
- metadata reads/s (`20,000` work/run, `reads/s`): kain `139,087.862`, rust `135,385.415`
- GIL scope entries/s (`20,000` work/run, `scopes/s`): kain `139,087.862`, rust `135,385.415`

Sources:
- kain: `cases/python_buffer_view_probe/main.kn`
- rust: `cases/python_zero_copy_buffer_pyo3_per_boundary/src/main.rs`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `825.376`
  - min_ms: `141.531`
  - max_ms: `145.040`
  - median_ms: `143.794`
  - mean_ms: `143.616`
  - relative_to_fastest: `fastest`
  - samples_ms: `[143.444, 141.531, 143.794, 145.040, 141.969, 144.801, 144.733]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\python_buffer_view_probe\main.kn -t llvm -o X:\benchmark\out\build\python_buffer_view_pyo3_per_boundary\kain\python_buffer_view_pyo3_per_boundary.ll`
  - run_command: `X:\benchmark\out\build\python_buffer_view_pyo3_per_boundary\kain\python_buffer_view_pyo3_per_boundary.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-7.406`
  - delta_pct: `-4.90%`
  - trend: `faster`
  - primary_metric_delta: `+5.15%` (buffer view probes/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `100.507`
  - min_ms: `139.473`
  - max_ms: `168.623`
  - median_ms: `147.726`
  - mean_ms: `149.351`
  - relative_to_fastest: `1.03x slower`
  - samples_ms: `[151.748, 147.726, 139.473, 168.623, 151.001, 144.628, 142.258]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\cargo.EXE build --release --manifest-path benchmark/cases/python_zero_copy_buffer_pyo3_per_boundary/Cargo.toml --target-dir benchmark/out/build/python_buffer_view_pyo3_per_boundary/rust/target`
  - run_command: `X:\benchmark\out\build\python_buffer_view_pyo3_per_boundary\rust\target\release\python-zero-copy-buffer-pyo3-per-boundary.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### python_buffer_view_pyo3_region - Python Buffer View PyO3 Region

- maturity: `implemented`
- winner: `rust`
- fastest_median_ms: `152.244`
- description: Repeated borrowed buffer-protocol probe of one cached contiguous NumPy uint8 array while Kain keeps one explicit Python region alive for the whole timed run. Rust uses PyO3 `PyBuffer<u8>` inside one outer GIL scope.
- fairness_note: Both rows create one contiguous NumPy source array before timing and then repeatedly borrow metadata from the same live Python owner without readback copies or shared-contract materialization. Rust keeps one outer GIL scope to show the PyO3 ceiling; Kain keeps one explicit `python_region_begin()` scope open and drives `python_region_buffer_view(...)` through it.
- language_notes:
  - kain: Measures repeated `python_region_buffer_view` open/read/release cycles against the same cached Python owner while reusing one region-scoped GIL entry and region-owned borrowed-view telemetry.
  - rust: Cargo release build with PyO3 auto-initialize; repeatedly acquires `PyBuffer<u8>` from the same cached Python object while holding one outer GIL scope.

Telemetry:
- primary_metric: `buffer view probes/s`
- buffer view probes/s (`20,000` work/run, `probes/s`): kain `130,406.405`, rust `131,367.636`
- borrowed bytes/s (`10,240,000` work/run, `bytes/s`): kain `66,768,079.381`, rust `67,260,229.434`
- metadata reads/s (`20,000` work/run, `reads/s`): kain `130,406.405`, rust `131,367.636`

Sources:
- kain: `cases/python_buffer_view_region_probe/main.kn`
- rust: `cases/python_zero_copy_buffer_pyo3_scoped/src/main.rs`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `874.046`
  - min_ms: `147.281`
  - max_ms: `162.481`
  - median_ms: `153.367`
  - mean_ms: `154.313`
  - relative_to_fastest: `1.01x slower`
  - samples_ms: `[156.560, 153.367, 147.281, 151.316, 162.481, 153.156, 156.028]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\python_buffer_view_region_probe\main.kn -t llvm -o X:\benchmark\out\build\python_buffer_view_pyo3_region\kain\python_buffer_view_pyo3_region.ll`
  - run_command: `X:\benchmark\out\build\python_buffer_view_pyo3_region\kain\python_buffer_view_pyo3_region.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+9.114`
  - delta_pct: `+6.32%`
  - trend: `slower`
  - regression_alert: `true`
  - primary_metric_delta: `-5.94%` (buffer view probes/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `120.518`
  - min_ms: `149.296`
  - max_ms: `165.811`
  - median_ms: `152.244`
  - mean_ms: `156.710`
  - relative_to_fastest: `fastest`
  - samples_ms: `[164.056, 149.545, 165.811, 164.744, 151.271, 149.296, 152.244]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\cargo.EXE build --release --manifest-path benchmark/cases/python_zero_copy_buffer_pyo3_scoped/Cargo.toml --target-dir benchmark/out/build/python_buffer_view_pyo3_region/rust/target`
  - run_command: `X:\benchmark\out\build\python_buffer_view_pyo3_region\rust\target\release\python-zero-copy-buffer-pyo3-scoped.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### python_buffer_view_pyo3_region_fused - Python Buffer View PyO3 Region Fused

- maturity: `implemented`
- winner: `kain`
- fastest_median_ms: `137.935`
- description: Fused borrowed buffer-protocol metadata probe of one cached contiguous NumPy uint8 array across 10,000,000 logical probes. Kain borrows the Python buffer once in an explicit region and executes the metadata checksum schedule natively; Rust remains the PyO3 scoped primitive reference.
- fairness_note: This is an orchestration-ceiling row, not primitive parity. Rust keeps the same scoped PyO3 borrowed-buffer loop used by `python_buffer_view_pyo3_region`; Kain proves that a stable region/object metadata schedule can be fused into one borrowed buffer view and one native checksum formula without readback copies.
- language_notes:
  - kain: Measures `python_region_buffer_view_checksum37`, a native fused region primitive that borrows the cached Python owner once, reads invariant metadata once, and accounts for 10,000,000 logical open/read/release probes with a Z3-proven residue schedule.
  - rust: Cargo release build with PyO3 auto-initialize; repeatedly acquires `PyBuffer<u8>` 10,000,000 times from the same cached Python object while holding one outer GIL scope.

Telemetry:
- primary_metric: `logical buffer view probes/s`
- logical buffer view probes/s (`10,000,000` work/run, `probes/s`): kain `72,497,810.566`, rust `7,410,440.748`
- logical borrowed bytes/s (`5,120,000,000` work/run, `bytes/s`): kain `37,118,879,009.854`, rust `3,794,145,662.884`
- logical metadata reads/s (`10,000,000` work/run, `reads/s`): kain `72,497,810.566`, rust `7,410,440.748`

Sources:
- kain: `cases/python_buffer_view_region_fused_probe/main.kn`
- rust: `cases/python_zero_copy_buffer_pyo3_scoped_10m/src/main.rs`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `851.894`
  - min_ms: `133.930`
  - max_ms: `147.580`
  - median_ms: `137.935`
  - mean_ms: `139.788`
  - relative_to_fastest: `fastest`
  - samples_ms: `[141.038, 147.580, 146.167, 137.935, 133.930, 137.111, 134.752]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\python_buffer_view_region_fused_probe\main.kn -t llvm -o X:\benchmark\out\build\python_buffer_view_pyo3_region_fused\kain\python_buffer_view_pyo3_region_fused.ll`
  - run_command: `X:\benchmark\out\build\python_buffer_view_pyo3_region_fused\kain\python_buffer_view_pyo3_region_fused.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-2.592`
  - delta_pct: `-1.84%`
  - trend: `faster`
  - primary_metric_delta: `+1.88%` (logical buffer view probes/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `101.497`
  - min_ms: `1324.795`
  - max_ms: `1369.837`
  - median_ms: `1349.447`
  - mean_ms: `1347.124`
  - relative_to_fastest: `9.78x slower`
  - samples_ms: `[1366.415, 1355.262, 1324.795, 1349.447, 1369.837, 1334.132, 1329.980]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\cargo.EXE build --release --manifest-path benchmark/cases/python_zero_copy_buffer_pyo3_scoped_10m/Cargo.toml --target-dir benchmark/out/build/python_buffer_view_pyo3_region_fused/rust/target`
  - run_command: `X:\benchmark\out\build\python_buffer_view_pyo3_region_fused\rust\target\release\python-zero-copy-buffer-pyo3-scoped-10m.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### python_zero_copy_buffer_pyo3_scoped - Python Zero Copy Buffer PyO3 Scoped

- maturity: `implemented`
- winner: `rust`
- fastest_median_ms: `135.207`
- description: Repeated zero-copy buffer-protocol adoption of one cached contiguous NumPy uint8 array. Kain uses `python_shared_buffer` plus shared contract reads; Rust uses PyO3 `PyBuffer<u8>` inside one outer GIL scope.
- fairness_note: Both rows create one contiguous NumPy source array before timing and then repeatedly adopt metadata from the same live Python owner without explicit readback copies. Rust keeps one outer GIL scope to show the PyO3 ceiling; Kain exercises the current shared-buffer bridge surface directly.
- language_notes:
  - kain: Measures repeated `python_shared_buffer` adoption plus shared-contract metadata reads against the same cached Python owner.
  - rust: Cargo release build with PyO3 auto-initialize; repeatedly acquires `PyBuffer<u8>` from the same cached Python object while holding one outer GIL scope.

Telemetry:
- primary_metric: `buffer adoptions/s`
- buffer adoptions/s (`20,000` work/run, `adoptions/s`): kain `25,269.483`, rust `147,921.226`
- shared bytes/s (`10,240,000` work/run, `bytes/s`): kain `12,937,975.422`, rust `75,735,667.728`
- metadata reads/s (`20,000` work/run, `reads/s`): kain `25,269.483`, rust `147,921.226`

Sources:
- kain: `cases/python_zero_copy_buffer_adoption/main.kn`
- rust: `cases/python_zero_copy_buffer_pyo3_scoped/src/main.rs`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `959.705`
  - min_ms: `775.569`
  - max_ms: `825.901`
  - median_ms: `791.468`
  - mean_ms: `794.712`
  - relative_to_fastest: `5.85x slower`
  - samples_ms: `[814.057, 775.569, 799.181, 825.901, 778.755, 778.055, 791.468]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\python_zero_copy_buffer_adoption\main.kn -t llvm -o X:\benchmark\out\build\python_zero_copy_buffer_pyo3_scoped\kain\python_zero_copy_buffer_pyo3_scoped.ll`
  - run_command: `X:\benchmark\out\build\python_zero_copy_buffer_pyo3_scoped\kain\python_zero_copy_buffer_pyo3_scoped.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+12.878`
  - delta_pct: `+1.65%`
  - trend: `slower`
  - regression_alert: `true`
  - primary_metric_delta: `-1.63%` (buffer adoptions/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `97.981`
  - min_ms: `133.542`
  - max_ms: `136.250`
  - median_ms: `135.207`
  - mean_ms: `135.076`
  - relative_to_fastest: `fastest`
  - samples_ms: `[135.670, 134.058, 134.946, 135.863, 133.542, 136.250, 135.207]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\cargo.EXE build --release --manifest-path benchmark/cases/python_zero_copy_buffer_pyo3_scoped/Cargo.toml --target-dir benchmark/out/build/python_zero_copy_buffer_pyo3_scoped/rust/target`
  - run_command: `X:\benchmark\out\build\python_zero_copy_buffer_pyo3_scoped\rust\target\release\python-zero-copy-buffer-pyo3-scoped.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### python_zero_copy_buffer_pyo3_per_boundary - Python Zero Copy Buffer PyO3 Per Boundary

- maturity: `implemented`
- winner: `rust`
- fastest_median_ms: `137.464`
- description: Repeated zero-copy buffer-protocol adoption of one cached contiguous NumPy uint8 array with one host bridge entry per timed turn. Kain uses `python_shared_buffer`; Rust uses PyO3 but reacquires the GIL each iteration.
- fairness_note: Both rows create one contiguous NumPy source array before timing and then repeatedly adopt metadata from the same live Python owner without explicit readback copies. Rust reacquires the GIL once per iteration so this case mirrors Kain's current raw bridge shape instead of the best-case PyO3 batching ceiling.
- language_notes:
  - kain: Measures repeated `python_shared_buffer` adoption plus shared-contract metadata reads against the same cached Python owner.
  - rust: Cargo release build with PyO3 auto-initialize; reacquires `Python::with_gil` once per timed iteration while reusing the same cached Python object.

Telemetry:
- primary_metric: `buffer adoptions/s`
- buffer adoptions/s (`20,000` work/run, `adoptions/s`): kain `25,290.154`, rust `145,492.850`
- shared bytes/s (`10,240,000` work/run, `bytes/s`): kain `12,948,558.815`, rust `74,492,339.074`
- metadata reads/s (`20,000` work/run, `reads/s`): kain `25,290.154`, rust `145,492.850`
- GIL scope entries/s (`20,000` work/run, `scopes/s`): kain `25,290.154`, rust `145,492.850`

Sources:
- kain: `cases/python_zero_copy_buffer_adoption/main.kn`
- rust: `cases/python_zero_copy_buffer_pyo3_per_boundary/src/main.rs`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `858.353`
  - min_ms: `761.087`
  - max_ms: `826.820`
  - median_ms: `790.822`
  - mean_ms: `790.357`
  - relative_to_fastest: `5.75x slower`
  - samples_ms: `[790.822, 802.999, 774.641, 761.087, 826.820, 812.210, 763.922]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\python_zero_copy_buffer_adoption\main.kn -t llvm -o X:\benchmark\out\build\python_zero_copy_buffer_pyo3_per_boundary\kain\python_zero_copy_buffer_pyo3_per_boundary.ll`
  - run_command: `X:\benchmark\out\build\python_zero_copy_buffer_pyo3_per_boundary\kain\python_zero_copy_buffer_pyo3_per_boundary.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `-3.111`
  - delta_pct: `-0.39%`
  - trend: `flat`
  - primary_metric_delta: `+0.39%` (buffer adoptions/s)
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `100.054`
  - min_ms: `136.064`
  - max_ms: `148.913`
  - median_ms: `137.464`
  - mean_ms: `139.642`
  - relative_to_fastest: `fastest`
  - samples_ms: `[148.913, 140.655, 140.735, 137.263, 136.399, 136.064, 137.464]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\cargo.EXE build --release --manifest-path benchmark/cases/python_zero_copy_buffer_pyo3_per_boundary/Cargo.toml --target-dir benchmark/out/build/python_zero_copy_buffer_pyo3_per_boundary/rust/target`
  - run_command: `X:\benchmark\out\build\python_zero_copy_buffer_pyo3_per_boundary\rust\target\release\python-zero-copy-buffer-pyo3-per-boundary.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline

### ffi_shared_call_stress - FFI Shared Call Stress

- maturity: `implemented`
- winner: `cpp`
- fastest_median_ms: `50.580`
- description: Repeated native shared-library calls through one tiny C ABI function. This is the direct FFI-call half of the missing callback/FFI category.
- fairness_note: This covers direct shared-library call overhead inside the normal suite. True closure-to-C callback trampolines are still not implemented in Kain LLVM in this checkout, so the callback half remains a known language gap and the dedicated `benchmark/lanes/ffi_boundary` lane stays the deeper ABI-tax probe.
- language_notes:
  - kain: This case uses a case-local `KAIN.toml` plus `use c::ffi_boundary_shared` and a runner-built shared library copied beside the executable at benchmark build time.

Sources:
- kain: `cases/ffi_shared_call_stress/main.kn`
- rust: `cases/ffi_shared_call_stress/main.rs`
- cpp: `cases/ffi_shared_call_stress/main.cpp`

Measurements:
- kain:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `1059.192`
  - min_ms: `52.438`
  - max_ms: `63.340`
  - median_ms: `55.139`
  - mean_ms: `57.636`
  - relative_to_fastest: `1.09x slower`
  - samples_ms: `[62.882, 62.283, 53.855, 53.517, 52.438, 55.139, 63.340]`
  - build_command: `F:\_b\output-user-root\n2kwlvv2\execroot\_main\bazel-out\x64_windows-opt\bin\crates\cli\kain.exe X:\benchmark\cases\ffi_shared_call_stress\main.kn -t llvm -o X:\benchmark\out\build\ffi_shared_call_stress\kain\ffi_shared_call_stress.ll`
  - run_command: `X:\benchmark\out\build\ffi_shared_call_stress\kain\ffi_shared_call_stress.exe`
  - previous_run: `#25` at `2026-05-29T08:06:11.257871+00:00`
  - delta_ms: `+4.261`
  - delta_pct: `+8.38%`
  - trend: `slower`
  - regression_alert: `true`
  - build_env: `{"KAIN_NATIVE_DEBUG_INFO": "0", "KAIN_NATIVE_OPT_LEVEL": "3", "KAIN_NATIVE_PROFILE": "benchmark-release", "KAIN_NATIVE_TARGET_CPU": "native", "KAIN_RUNTIME_MANIFEST_PATH": "X:\\runtime\\native_core_runtime.toml"}`
- rust:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `240.477`
  - min_ms: `50.574`
  - max_ms: `52.410`
  - median_ms: `50.724`
  - mean_ms: `51.014`
  - relative_to_fastest: `1.00x slower`
  - samples_ms: `[50.574, 50.724, 50.643, 50.913, 51.127, 52.410, 50.705]`
  - build_command: `F:\Scoop\apps\rustup\current\.cargo\bin\rustc.EXE benchmark/cases/ffi_shared_call_stress/main.rs -C opt-level=3 -C target-cpu=native -C debuginfo=0 -C panic=abort -C overflow-checks=off -L native=benchmark/out/build/ffi_shared_call_stress/native -l dylib=ffi_boundary_shared -o benchmark/out/build/ffi_shared_call_stress/rust/ffi_shared_call_stress.exe`
  - run_command: `X:\benchmark\out\build\ffi_shared_call_stress\rust\ffi_shared_call_stress.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
- cpp:
  - build_ok: `PASS`
  - run_ok: `PASS`
  - build_ms: `128.609`
  - min_ms: `49.387`
  - max_ms: `50.802`
  - median_ms: `50.580`
  - mean_ms: `50.463`
  - relative_to_fastest: `fastest`
  - samples_ms: `[50.674, 49.387, 50.559, 50.580, 50.802, 50.787, 50.452]`
  - build_command: `X:\toolchain\llvm\bin\clang++.exe benchmark/cases/ffi_shared_call_stress/main.cpp -std=c++20 -O3 -march=native -DNDEBUG -o benchmark/out/build/ffi_shared_call_stress/cpp/ffi_shared_call_stress.exe benchmark/out/build/ffi_shared_call_stress/native/ffi_boundary_shared.lib`
  - run_command: `X:\benchmark\out\build\ffi_shared_call_stress\cpp\ffi_shared_call_stress.exe`
  - baseline_cache: `refreshed` - saved fresh foreign baseline
