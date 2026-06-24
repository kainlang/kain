# Bench Report

**Folder:** `X:/blades/benchmark/test_benches`  
**Date:** 2026-06-24 03:14:29  
**Target:** 1000ms per benchmark  
**Benchmarks:** 5

| Benchmark | Iters | Median | Min | Max | Mean | ns/iter | Checksum |
|-----------|-------|--------|-----|-----|------|---------|----------|
| `bench_json_parse` | 4096 | 29ms | 28ms | 29ms | 28ms | 7080 | `9637` |
| `bench_ownership` | 8192 | 223ms | 218ms | 231ms | 223ms | 27221 | `390689129` |
| `levenshtein` | 64 | 137ms | 131ms | 289ms | 185ms | 2140625 | `3` |
| `scalar_template` | 65536 | 15575ms | 15348ms | 15693ms | 15538ms | 237655 | `522933064` |
| `shatter_ecs` | 8 | 823ms | 799ms | 833ms | 816ms | 102875000 | `0` |

## Details

### bench_json_parse

- **Iterations:** 4096
- **Checksum:** `9637`
- **Median:** 29ms (7080 ns/iter)
- **Min/Max:** 28ms / 29ms
- **Mean:** 28ms
- **Samples:** 28ms, 29ms, 29ms
- **Binary:** `X:\bench_json_parse.exe`

### bench_ownership

Ownership hot path: 200 rapid alloc/collapse/observe/decay cycles per iteration. Each cycle allocates 4-16 Int cells via `alloc_zeroed`, writes data-dependent values inside `collapse`, reads one cell back via `observe`, then `decay`s the allocation. The read value feeds into the next cycle's write pattern via a multiplicative checksum chain. Allocation sizes vary per round to prevent size-based caching. This exercises three P0/P1 optimization targets: buddy allocator `kain_buddy_log2_exact()` (CLZ replacement, 10-30× expected), ownership ringbuffer `% 4096` (bitmask replacement, 2-5× expected), and ownership free-slot scan (summary word + ctz, 16-64× expected).

- **Iterations:** 8192
- **Checksum:** `390689129`
- **Median:** 223ms (27221 ns/iter)
- **Min/Max:** 218ms / 231ms
- **Mean:** 223ms
- **Samples:** 218ms, 221ms, 223ms, 224ms, 231ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\bench_ownership\compile\bench_ownership.exe`

**Custom telemetry:**
- `alloc_cycles_per_iter=200`
- `alloc_size_range=4-16 Int cells`
- `ownership_ops=collapse + observe + decay per cycle`
- `optimization_targets=buddy.c log2→CLZ, ownership.c %→&, free-slot→ctz`

### levenshtein

- **Iterations:** 64
- **Checksum:** `3`
- **Median:** 137ms (2140625 ns/iter)
- **Min/Max:** 131ms / 289ms
- **Mean:** 185ms
- **Samples:** 131ms, 137ms, 289ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\levenshtein\compile\levenshtein.exe`

### scalar_template

- **Iterations:** 65536
- **Checksum:** `522933064`
- **Median:** 15575ms (237655 ns/iter)
- **Min/Max:** 15348ms / 15693ms
- **Mean:** 15538ms
- **Samples:** 15348ms, 15575ms, 15693ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\scalar_template\compile\scalar_template.exe`

### shatter_ecs

- **Iterations:** 8
- **Checksum:** `0`
- **Median:** 823ms (102875000 ns/iter)
- **Min/Max:** 799ms / 833ms
- **Mean:** 816ms
- **Samples:** 799ms, 800ms, 823ms, 828ms, 833ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\shatter_ecs\compile\shatter_ecs.exe`

