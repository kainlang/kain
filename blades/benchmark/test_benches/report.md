# Bench Report

**Folder:** `X:/blades/benchmark/test_benches`  
**Date:** 2026-06-24 20:20:06  
**Target:** 1000ms per benchmark  
**Benchmarks:** 7

| Benchmark | Iters | Median | Min | Max | Mean | ns/iter | Checksum |
|-----------|-------|--------|-----|-----|------|---------|----------|
| `bench_actor_spam` | 256 | 19ms | 18ms | 19ms | 18ms | 74218 | `186472393` |
| `bench_json_parse` | 4096 | 26ms | 26ms | 26ms | 26ms | 6347 | `9637` |
| `bench_memory_atomics` | 4096 | 24ms | 23ms | 25ms | 24ms | 5859 | `320012` |
| `bench_ownership` | 8192 | 228ms | 228ms | 265ms | 237ms | 27832 | `390689129` |
| `levenshtein` | 64 | 128ms | 128ms | 258ms | 171ms | 2000000 | `3` |
| `scalar_template` | 65536 | 15353ms | 15127ms | 15756ms | 15412ms | 234268 | `522933064` |
| `shatter_ecs` | 8 | 718ms | 704ms | 765ms | 733ms | 89750000 | `0` |

## Details

### bench_actor_spam

Actor hot path: spawn 4 actors, send 20 rounds of ask/reply per iteration. Exercises actor spawn (slot allocation via de Bruijn/popcount), mailbox enqueue/dequeue, reply port generation (generation-tagged refs), scheduler dequeue, and ref resolution. Targets P0/P1 actor.c super-optimizations: popcount SWAR→POPCNT (10-15×), restart policy branch→bitwise (5×), borrowed message memmove (5-10×), ref packed compare.

- **Iterations:** 256
- **Checksum:** `186472393`
- **Median:** 19ms (74218 ns/iter)
- **Min/Max:** 18ms / 19ms
- **Mean:** 18ms
- **Samples:** 18ms, 18ms, 19ms, 19ms, 19ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\bench_actor_spam\compile\bench_actor_spam.exe`

**Custom telemetry:**
- `actors_per_iter=4`
- `messages_per_iter=80 (4 actors × 20 rounds)`
- `optimization_targets=popcount→POPCNT, restart→bitwise, memmove, ref-packed-compare`

### bench_json_parse

- **Iterations:** 4096
- **Checksum:** `9637`
- **Median:** 26ms (6347 ns/iter)
- **Min/Max:** 26ms / 26ms
- **Mean:** 26ms
- **Samples:** 26ms, 26ms, 26ms
- **Binary:** `X:\bench_json_parse.exe`

### bench_memory_atomics

Memory hot path: ordered atomic load/store/fence operations + pointer chains. Exercises `atomic_load_seqcst`, `atomic_store_seqcst`, `atomic_fence_seqcst`, raw pointer offset chains (`ptr_offset`), and `mem_load`/`mem_store` through collapse/observe/decay. Targets P0/P1 memory.c super-optimizations: `&&`→`&` bitwise (2×), deferred decay OR flatten (1.2×), ordering LUT branchless (2×), strength checks.

- **Iterations:** 4096
- **Checksum:** `320012`
- **Median:** 24ms (5859 ns/iter)
- **Min/Max:** 23ms / 25ms
- **Mean:** 24ms
- **Samples:** 23ms, 24ms, 24ms, 24ms, 25ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\bench_memory_atomics\compile\bench_memory_atomics.exe`

**Custom telemetry:**
- `atomic_ops_per_iter=4 (2 stores + 2 loads)`
- `alloc_cycles_per_iter=50 (alloc/collapse/observe/decay)`
- `optimization_targets=&&→&, OR-flatten, ordering-LUT, strength-checks`

### bench_ownership

Ownership hot path: 200 rapid alloc/collapse/observe/decay cycles per iteration. Each cycle allocates 4-16 Int cells via `alloc_zeroed`, writes data-dependent values inside `collapse`, reads one cell back via `observe`, then `decay`s the allocation. The read value feeds into the next cycle's write pattern via a multiplicative checksum chain. Allocation sizes vary per round to prevent size-based caching. This exercises three P0/P1 optimization targets: buddy allocator `kain_buddy_log2_exact()` (CLZ replacement, 10-30× expected), ownership ringbuffer `% 4096` (bitmask replacement, 2-5× expected), and ownership free-slot scan (summary word + ctz, 16-64× expected).

- **Iterations:** 8192
- **Checksum:** `390689129`
- **Median:** 228ms (27832 ns/iter)
- **Min/Max:** 228ms / 265ms
- **Mean:** 237ms
- **Samples:** 228ms, 228ms, 228ms, 238ms, 265ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\bench_ownership\compile\bench_ownership.exe`

**Custom telemetry:**
- `alloc_cycles_per_iter=200`
- `alloc_size_range=4-16 Int cells`
- `ownership_ops=collapse + observe + decay per cycle`
- `optimization_targets=buddy.c log2→CLZ, ownership.c %→&, free-slot→ctz`

### levenshtein

- **Iterations:** 64
- **Checksum:** `3`
- **Median:** 128ms (2000000 ns/iter)
- **Min/Max:** 128ms / 258ms
- **Mean:** 171ms
- **Samples:** 128ms, 128ms, 258ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\levenshtein\compile\levenshtein.exe`

### scalar_template

- **Iterations:** 65536
- **Checksum:** `522933064`
- **Median:** 15353ms (234268 ns/iter)
- **Min/Max:** 15127ms / 15756ms
- **Mean:** 15412ms
- **Samples:** 15127ms, 15353ms, 15756ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\scalar_template\compile\scalar_template.exe`

### shatter_ecs

- **Iterations:** 8
- **Checksum:** `0`
- **Median:** 718ms (89750000 ns/iter)
- **Min/Max:** 704ms / 765ms
- **Mean:** 733ms
- **Samples:** 704ms, 715ms, 718ms, 764ms, 765ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\shatter_ecs\compile\shatter_ecs.exe`

