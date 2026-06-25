# Bench Report

**Folder:** `X:/blades/benchmark/test_benches/`  
**Date:** 2026-06-24 20:45:50  
**Target:** 1000ms per benchmark  
**Benchmarks:** 7

| Benchmark | Iters | Median | Min | Max | Mean | ns/iter | Checksum |
|-----------|-------|--------|-----|-----|------|---------|----------|
| `bench_actor_spam` | 256 | 20ms | 19ms | 20ms | 19ms | 78125 | `186472393` |
| `bench_json_parse` | 4096 | 26ms | 26ms | 27ms | 26ms | 6347 | `9637` |
| `bench_memory_atomics` | 4096 | 23ms | 22ms | 29ms | 23ms | 5615 | `320012` |
| `bench_ownership` | 8192 | 219ms | 211ms | 244ms | 223ms | 26733 | `390689129` |
| `levenshtein` | 32 | 61ms | 59ms | 126ms | 82ms | 1906250 | `3` |
| `scalar_template` | 65536 | 15499ms | 15372ms | 15613ms | 15494ms | 236495 | `522933064` |
| `shatter_ecs` | 8 | 873ms | 782ms | 932ms | 855ms | 109125000 | `0` |

## Details

### bench_actor_spam

Actor hot path: spawn 4 actors, send 20 rounds of ask/reply per iteration. Exercises actor spawn (slot allocation via de Bruijn/popcount), mailbox enqueue/dequeue, reply port generation (generation-tagged refs), scheduler dequeue, and ref resolution. Targets P0/P1 actor.c super-optimizations: popcount SWAR→POPCNT (10-15×), restart policy branch→bitwise (5×), borrowed message memmove (5-10×), ref packed compare.

- **Iterations:** 256
- **Checksum:** `186472393`
- **Median:** 20ms (78125 ns/iter)
- **Min/Max:** 19ms / 20ms
- **Mean:** 19ms
- **Samples:** 19ms, 19ms, 20ms, 20ms, 20ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\bench_actor_spam\compile\bench_actor_spam.exe`

**Custom telemetry:**
- `actors_per_iter=4`
- `messages_per_iter=80 (4 actors × 20 rounds)`
- `optimization_targets=popcount→POPCNT, restart→bitwise, memmove, ref-packed-compare`

### bench_json_parse

- **Iterations:** 4096
- **Checksum:** `9637`
- **Median:** 26ms (6347 ns/iter)
- **Min/Max:** 26ms / 27ms
- **Mean:** 26ms
- **Samples:** 26ms, 26ms, 27ms
- **Binary:** `X:\bench_json_parse.exe`

### bench_memory_atomics

Memory hot path: ordered atomic load/store/fence operations + pointer chains. Exercises `atomic_load_seqcst`, `atomic_store_seqcst`, `atomic_fence_seqcst`, raw pointer offset chains (`ptr_offset`), and `mem_load`/`mem_store` through collapse/observe/decay. Targets P0/P1 memory.c super-optimizations: `&&`→`&` bitwise (2×), deferred decay OR flatten (1.2×), ordering LUT branchless (2×), strength checks.

- **Iterations:** 4096
- **Checksum:** `320012`
- **Median:** 23ms (5615 ns/iter)
- **Min/Max:** 22ms / 29ms
- **Mean:** 23ms
- **Samples:** 22ms, 22ms, 23ms, 23ms, 29ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\bench_memory_atomics\compile\bench_memory_atomics.exe`

**Custom telemetry:**
- `atomic_ops_per_iter=4 (2 stores + 2 loads)`
- `alloc_cycles_per_iter=50 (alloc/collapse/observe/decay)`
- `optimization_targets=&&→&, OR-flatten, ordering-LUT, strength-checks`

### bench_ownership

Ownership hot path: 200 rapid alloc/collapse/observe/decay cycles per iteration. Each cycle allocates 4-16 Int cells via `alloc_zeroed`, writes data-dependent values inside `collapse`, reads one cell back via `observe`, then `decay`s the allocation. The read value feeds into the next cycle's write pattern via a multiplicative checksum chain. Allocation sizes vary per round to prevent size-based caching. This exercises three P0/P1 optimization targets: buddy allocator `kain_buddy_log2_exact()` (CLZ replacement, 10-30× expected), ownership ringbuffer `% 4096` (bitmask replacement, 2-5× expected), and ownership free-slot scan (summary word + ctz, 16-64× expected).

- **Iterations:** 8192
- **Checksum:** `390689129`
- **Median:** 219ms (26733 ns/iter)
- **Min/Max:** 211ms / 244ms
- **Mean:** 223ms
- **Samples:** 211ms, 215ms, 219ms, 227ms, 244ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\bench_ownership\compile\bench_ownership.exe`

**Custom telemetry:**
- `alloc_cycles_per_iter=200`
- `alloc_size_range=4-16 Int cells`
- `ownership_ops=collapse + observe + decay per cycle`
- `optimization_targets=buddy.c log2→CLZ, ownership.c %→&, free-slot→ctz`

### levenshtein

- **Iterations:** 32
- **Checksum:** `3`
- **Median:** 61ms (1906250 ns/iter)
- **Min/Max:** 59ms / 126ms
- **Mean:** 82ms
- **Samples:** 59ms, 61ms, 126ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\levenshtein\compile\levenshtein.exe`

### scalar_template

- **Iterations:** 65536
- **Checksum:** `522933064`
- **Median:** 15499ms (236495 ns/iter)
- **Min/Max:** 15372ms / 15613ms
- **Mean:** 15494ms
- **Samples:** 15372ms, 15499ms, 15613ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\scalar_template\compile\scalar_template.exe`

### shatter_ecs

- **Iterations:** 8
- **Checksum:** `0`
- **Median:** 873ms (109125000 ns/iter)
- **Min/Max:** 782ms / 932ms
- **Mean:** 855ms
- **Samples:** 782ms, 810ms, 873ms, 880ms, 932ms
- **Binary:** `X:/blades/benchmark\.kain\out\x86_64-windows\dev\ll\shatter_ecs\compile\shatter_ecs.exe`

