# Bench Report

**Folder:** `X:/blades/benchmark/test_benches`  
**Date:** 2026-06-24 02:26:28  
**Target:** 1000ms per benchmark  
**Benchmarks:** 5

| Benchmark | Iters | Median | Min | Max | Mean | ns/iter | Checksum |
|-----------|-------|--------|-----|-----|------|---------|----------|
| `bench_json_parse` | 4096 | 27ms | 27ms | 28ms | 27ms | 6591 | `9637` |
| `bench_ownership` | 4096 | 14ms | 13ms | 14ms | 13ms | 3417 | `627176` |
| `levenshtein` | 64 | 158ms | 149ms | 296ms | 201ms | 2468750 | `3` |
| `scalar_template` | 65536 | 15538ms | 15536ms | 15710ms | 15594ms | 237091 | `522933064` |
| `shatter_ecs` | 4 | 632ms | 536ms | 637ms | 607ms | 158000000 | `0` |

## Details

### bench_json_parse

- **Iterations:** 4096
- **Checksum:** `9637`
- **Median:** 27ms (6591 ns/iter)
- **Min/Max:** 27ms / 28ms
- **Mean:** 27ms
- **Samples:** 27ms, 27ms, 28ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\bench_json_parse\compile\bench_json_parse.exe`

### bench_ownership

- **Iterations:** 4096
- **Checksum:** `627176`
- **Median:** 14ms (3417 ns/iter)
- **Min/Max:** 13ms / 14ms
- **Mean:** 13ms
- **Samples:** 13ms, 13ms, 14ms, 14ms, 14ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\bench_ownership\compile\bench_ownership.exe`

### levenshtein

- **Iterations:** 64
- **Checksum:** `3`
- **Median:** 158ms (2468750 ns/iter)
- **Min/Max:** 149ms / 296ms
- **Mean:** 201ms
- **Samples:** 149ms, 158ms, 296ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\levenshtein\compile\levenshtein.exe`

### scalar_template

- **Iterations:** 65536
- **Checksum:** `522933064`
- **Median:** 15538ms (237091 ns/iter)
- **Min/Max:** 15536ms / 15710ms
- **Mean:** 15594ms
- **Samples:** 15536ms, 15538ms, 15710ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\scalar_template\compile\scalar_template.exe`

### shatter_ecs

- **Iterations:** 4
- **Checksum:** `0`
- **Median:** 632ms (158000000 ns/iter)
- **Min/Max:** 536ms / 637ms
- **Mean:** 607ms
- **Samples:** 536ms, 599ms, 632ms, 635ms, 637ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\shatter_ecs\compile\shatter_ecs.exe`

