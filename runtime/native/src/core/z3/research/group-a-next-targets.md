# Runtime Super-Optimization Targets — Group A (Memory, Concurrency, Core)

> **Mission:** Scan all 23 files for Z3-discoverable alien-code opportunities — magic constants, branchless rewrites, compressed state machines, SIMD lane abuse, de Bruijn hash tables, and arithmetic predicate replacement.
>
> **Style reference:** `proofs-experimental/*.smt2` — magic multiplier perfect hashing on 64-bit bit-vectors, collision-free service token maps, masked branchless selection, de Bruijn low-bit decoders with `0x03f79d71b4cb0a89`, and tiny-dispatch perfect-hash rebuild loops.

---

## Top Targets (ranked by impact × feasibility)

---

### 1. `buddy.c` — `kain_buddy_log2_exact()` and `kain_buddy_required_height()` — while-loop log2 on EVERY alloc/free

- **Current pattern:**
  ```c
  static uint8_t kain_buddy_log2_exact(uint32_t value) {
      uint8_t height = 0u;
      while (value > 1u) { value >>= 1u; height += 1u; }
      return height;
  }
  static uint32_t kain_buddy_required_height(uint32_t unit_count) {
      uint32_t rounded_units = 1u;
      uint32_t height = 0u;
      while (rounded_units < unit_count) {
          rounded_units <<= 1u; height += 1u;
      }
      return height;
  }
  ```
  These are called on **every** `kain_buddy_alloc()` and `kain_buddy_free()` — the two hottest functions in the buddy allocator.

- **Optimization opportunity:**
  Replace with `31 - __builtin_clz(value)` (or `_BitScanReverse` on MSVC). For `required_height`, compute `kain_buddy_log2_exact(unit_count - 1) + 1` using CLZ. This collapses a loop of up to 32 iterations into 2-3 instructions.

  Even more aggressive: use Z3 to search for a magic constant that maps `(value * magic) >> shift` to a perfect log2 table index (like the de Bruijn proofs but for ceiling log2). The buddy allocator only needs log2 for power-of-two values — this is exactly the problem de Bruijn sequences solve.

- **Estimated impact:** **HIGH** — called on every arena allocation/free. The buddy allocator is the hot path for Kain's memory subsystem. The while-loop can iterate up to 32 times per call. Even a 4× faster `log2_exact` would save thousands of cycles per frame on allocation-heavy workloads.

- **SMT2 approach:**
  - Model `kain_buddy_log2_exact` as `(bv2nat (bvshl (_ bv1 32) value))` domain: 32-bit bit-vector.
  - Prove equivalence of `clz`-based replacement for the power-of-two input domain.
  - For `required_height`: model as round-up-to-next-power-of-two via magic constant + `__builtin_ctz`.
  - Search for a de Bruijn magic that maps the 32 possible one-hot values (powers of two) to distinct 5-bit indices — then fold in the "round up" case.

- **Confidence:** **HIGH** — this is a textbook CLZ replacement with proven pattern in `actor.c` and `ownership.c`.

---

### 2. `cpu.c` — `abi_cpu_capability_mask_for_key()` — ~40 strcmp calls in a giant if/else ladder

- **Current pattern:**
  ```c
  uint64_t abi_cpu_capability_mask_for_key(const char* capability_key) {
      if (abi_text_equals(capability_key, "cpu.x86.sse2") ||
          abi_text_equals(capability_key, "x86.sse2") ||
          abi_text_equals(capability_key, "sse2"))
          return KAIN_CPU_FEATURE_X86_SSE2;
      if (abi_text_equals(capability_key, "cpu.x86.avx") ||
          abi_text_equals(capability_key, "x86.avx") ||
          abi_text_equals(capability_key, "avx"))
          return KAIN_CPU_FEATURE_X86_AVX;
      // ... ~30 more strcmp calls in this ladder
  ```
  Every call walks through a cascade of up to 40+ `strcmp` calls. Each `strcmp` is itself a byte-by-byte loop.

- **Optimization opportunity:**
  Replace with a token-signature switch (like `machine_stones.c: kain_machine_token_signature` already does). Compute a 32-bit token from the first ~4 characters + length, then use a switch statement (compiled to a jump table). Only fall back to `strcmp` on hash collision.

  Z3 can prove the token signature is collision-free for the known set of ~36 capability keys + their aliases.

- **Estimated impact:** **MEDIUM-HIGH** — called whenever user code queries CPU capabilities (e.g., `std::machine` calls). From strcmp chains of 36+ calls to a single switch-table dispatch, this is roughly **30-50× faster** per call. Not on the hottest path (not per-alloc), but called frequently enough in system-initialization code paths.

- **SMT2 approach:**
  - Collect all 36+ capability key strings with all aliases.
  - Model `KAIN_MACHINE_TOKEN_SIG(length, first, second, last)` as a 32-bit bit-vector function.
  - Assert `(distinct sig_1 sig_2 ... sig_n)` — prove collision freedom.
  - For any collisions, iterate on the token function (include middle char, XOR pairs, etc.) until collision-free.
  - Output: a switch with computed `goto` table.

- **Confidence:** **HIGH** — exact same pattern as `machine_stones.c` which already has a Z3 proof (`native-machine-capability-token-signatures-are-collision-free.yaml`).

---

### 3. `ownership.c` — `kain_ownership_find_range_slot_unlocked()` — LINEAR SCAN through 4096 regions

- **Current pattern:**
  ```c
  static int kain_ownership_find_range_slot_unlocked(const void* ptr) {
      // ... try exact hash lookup first ...
      address = (uintptr_t)ptr;
      for (uint32_t slot = 0u; slot < KAIN_OWNERSHIP_MAX_REGIONS; ++slot) {
          // linear scan checking if ptr falls within [base, base+size)
      }
      return -1;
  }
  ```
  When the exact pointer index misses (because the pointer points into the middle of a tracked allocation, not at its base), this falls back to an O(N) scan of **all 4096 regions** with a range check.

- **Optimization opportunity:**
  Three-tier approach:
  1. **LRU range cache** — exactly like `fixup.c` already does (`KAIN_FIXUP_LAST_RANGE` caches the last range lookup with `kain_lru_range_update/lookup`). The ownership registry has no such cache.
  2. **Interval tree** — maintain a red-black or splay tree of `[base, limit)` intervals. 4096 entries is small enough for a cache-oblivious flat array.
  3. **Z3 magic** — if region sizes follow a pattern (typical heap allocation sizes), Z3 could discover a compact hash.

- **Estimated impact:** **HIGH** — the ownership `observe`/`collapse`/`decay` path hits this on every access. A single misplaced pointer in a hot inner loop (e.g., particle simulation with hundreds of thousands of observations) could trigger a 4096-element scan repeatedly. Fixup.c already fixed the same problem with its LRU range cache — this is the same bug, same solution.

- **SMT2 approach:**
  - Model the LRU cache hit/miss behavior probabilistically.
  - Prove that a single-entry cache (`kain_lru_range_update`) covers the temporal locality of ownership lookups (most lookups repeat near the same pointer).
  - No bit vector needed — this is a cache-coherence argument.

- **Confidence:** **HIGH** — `fixup.c` already solved this exact problem. Port the LRU range cache pattern.

---

### 4. `fixup.c` — `kain_fixup_find_object_by_pointer_unlocked()` — same LINEAR SCAN fallback

- **Current pattern:**
  ```c
  static KainFixupObject* kain_fixup_find_object_by_pointer_unlocked(const void* ptr) {
      // ... try exact base match first ...
      // ... try LRU range cache ...
      // ... FALLBACK: linear scan through ALL 4096 objects:
      for (slot = 0u; slot < KAIN_FIXUP_MAX_OBJECTS; ++slot) {
          KainFixupObject* object = &KAIN_FIXUP_OBJECTS[slot];
          if (!object->live || !object->base) continue;
          start = (uintptr_t)object->base;
          if (!kain_fixup_try_range_limit(start, object->size, &limit)) continue;
          if (address >= start && address < limit) { ... }
      }
  }
  ```
  Has a range cache (`KAIN_FIXUP_LAST_RANGE`) but when it misses, drops to an O(N) scan of 4096 objects.

- **Optimization opportunity:**
  The fixup subsystem has the same problem as ownership — the range cache is single-entry. Replace with:
  1. **Small interval tree** or **splay tree** for range lookups (4096 entries is tiny — perfect for a flat array of intervals sorted by base).
  2. **Shadow the ownership registry** — since fixup objects are a subset of ownership regions, the ownership index could serve double duty.
  3. **Z3 perfect hash on the interval boundaries** — if object sizes are known static (unlikely), magic-constant hash.

- **Estimated impact:** **MEDIUM-HIGH** — fixup is tier-gated (`KAIN_RUNTIME_FIXUP_ENABLED()`), so it's only active in debug/instrumented builds. However, when active, every pointer relocation triggers this path.

- **SMT2 approach:**
  - Model the temporal locality of fixup queries (they tend to cluster around recently-relocated allocations).
  - Prove that a larger LRU cache (4 or 8 entries) eliminates the linear scan in practice.
  - Use Z3 to find the optimal replacement policy.

- **Confidence:** **MEDIUM** — fixup is tier-gated, so the payoff is smaller. But the fix is the same as ownership.

---

### 5. `ownership.c` — Deferred decay ringbuffer uses `% 4096` where 4096 is a power of two

- **Current pattern:**
  ```c
  // Line 249
  KAIN_OWNERSHIP_DEFERRED_DECAY_TAIL =
      (KAIN_OWNERSHIP_DEFERRED_DECAY_TAIL + 1u) % KAIN_OWNERSHIP_MAX_REGIONS;
  // Line 1157
  KAIN_OWNERSHIP_DEFERRED_DECAY_HEAD =
      (KAIN_OWNERSHIP_DEFERRED_DECAY_HEAD + 1u) % KAIN_OWNERSHIP_MAX_REGIONS;
  ```
  `KAIN_OWNERSHIP_MAX_REGIONS = 4096u` — a clean power of two. The compiler *might* optimize this to `& 4095u`, but with unsigned 32-bit arithmetic, `x % 4096` on x86-64 requires a `div` instruction unless the compiler proves the modulus is constant. With the `+ 1u` increment this is always true, but MSVC has historically missed this optimization.

- **Optimization opportunity:**
  Replace with explicit bitmask: `(head + 1u) & (KAIN_OWNERSHIP_MAX_REGIONS - 1u)`. This is a single `and` instruction vs a multi-cycle `div`.

- **Estimated impact:** **LOW per call** but called on **every** deferred decay enqueue/dequeue, which maps to every `decay` call in Kain code. Hundreds of thousands of calls over a program's lifetime.

- **SMT2 approach:**
  - 32-bit bit-vector: prove `(bvadd head #x00000001) mod #x00001000` ≡ `(bvand (bvadd head #x00000001) #x00000fff)` for `head < 4096`.
  - Trivial: `(assert (not (=> (bvult head #x00001000) (= (bvurem (bvadd head #x00000001) #x00001000) (bvand (bvadd head #x00000001) #x00000fff)))))` → unsat.

- **Confidence:** **HIGH** — trivial proof, trivial fix, immediate win.

---

### 6. `event.c` — DJB2 hash uses `% KAIN_EVENT_BUS_BUCKETS` where buckets = 256

- **Current pattern:**
  ```c
  #define KAIN_EVENT_BUS_BUCKETS 256
  static unsigned int kain_event_hash(const char* name) {
      unsigned long hash = 5381;
      int c;
      while ((c = (unsigned char)*name++) != '\0')
          hash = ((hash << 5) + hash) ^ c;
      return (unsigned int)(hash % KAIN_EVENT_BUS_BUCKETS);
  }
  ```
  256 is a power of two → `% 256` should be `& 255`.

- **Estimated impact:** **LOW** — event system is not the hottest path. But it's a free win.

- **Confidence:** **HIGH**

---

### 7. `event.c` — Topic lookup walks a linked-list chain, duplicate subscribe check walks subscriber list

- **Current pattern:**
  ```c
  // Topic lookup in bucket chain:
  topic = g_event_bus.buckets[bucket];
  while (topic != NULL) {
      if (strncmp(topic->name, event_name, ...) == 0) break;
      topic = topic->next;
  }
  // Duplicate subscribe check:
  existing = topic->head;
  while (existing != NULL) {
      if (existing->actor_id == actor_id) { /* already subscribed */ }
      existing = existing->next;
  }
  ```

- **Optimization opportunity:**
  Two ideas:
  1. **Perfect hash topic names** — the event names are typically compile-time string literals in Kain. Z3 could find a perfect hash to directly index into a flat topic array instead of walking a linked-list bucket chain.
  2. **Bitmap per topic for subscribers** — use a 64-bit occupancy word to track the first 64 subscribers, enabling O(1) duplicate check and O(popcount) iteration.

- **Estimated impact:** **MEDIUM** — the event system is used for `emit` keyword in Kain, which maps to runtime pub/sub. For games and simulations with many entities, this could be hot.

- **SMT2 approach:**
  - Collect all compile-time known event topic names from the Kain stdlib and user code (from `z3/data/` catalog).
  - Prove collision-free perfect hash into a flat array.
  - Model subscriber bitmap as 64-bit bit-vector: `(bvand subscriber_bits (bvshl (_ bv1 64) actor_id))` for duplicate check.

- **Confidence:** **MEDIUM** — depends on how many event topics are known at compile time. The topic names are dynamic (any string can be emitted), so a perfect hash only covers the static subset.

---

### 8. `memory.c` — Large alloc cache bucket linked-list walk

- **Current pattern:**
  ```c
  if (kain_alloc_cache_large_eligible(payload_size, 0u)) {
      size_t bucket = kain_alloc_cache_large_bucket(payload_size);
      KainAllocHeader** link = &cache->large_buckets[bucket];
      while (*link != NULL) {
          KainAllocHeader* candidate = *link;
          if (candidate->metadata.payload_size == payload_size &&
              __kain_alloc_header_memtype(candidate) == memtype) {
              // found match
          }
          link = next;
      }
  }
  ```
  The large cache bucket is a singly-linked list. Finding a matching `(payload_size, memtype)` pair requires walking the list until found.

- **Optimization opportunity:**
  Replace with a **secondary size-indexed hash** within each bucket: use `kain_alloc_cache_small_bin(payload_size)` mod a small power-of-two within the bucket for O(1) lookup. Or use a small fixed-size "recently freed" per-bucket array of matching slots (temporal locality: recently freed large allocations tend to have the same size).

- **Estimated impact:** **MEDIUM** — large allocations (>2KB) are less frequent than small ones but more expensive to cache-miss on. The linked-list walk is O(collisions-in-bucket), which with 64 buckets and 256 max nodes averages 4 entries.

- **SMT2 approach:**
  - Model the distribution of large allocation sizes from the runtime.
  - Search for a compact perfect hash over `(payload_size, memtype)` pairs within each bucket.
  - Use Z3 to find a magic multiplier that separates the common size classes.

- **Confidence:** **MEDIUM** — the linked list is short but the alloc cache is on every allocation. Even a small improvement compounds.

---

### 9. `core.c` — `kain_map_key_metadata()` calls `strlen` and then `memcpy` separately — double scan on short keys

- **Current pattern:**
  ```c
  static void kain_map_key_metadata(const char* key, ...) {
      size_t key_length = strlen(key);                    // scan 1
      size_t prefix_length = key_length < 32u ? key_length : 32u;
      uint64_t prefix_words[4] = {0u, 0u, 0u, 0u};
      if (prefix_length > 0u)
          memcpy(prefix_words, key, prefix_length);        // scan 2
      // ... hash the full string again                   // scan 3
      key_prefix = kain_map_magic_prefix_state(prefix_words, ...);
      *out_hash = kain_hash_bytes((const unsigned char*)key, key_length);
  }
  ```
  Map keys in Kain are typically short strings (service names, world property names). `strlen` walks the full string to find `\0`, then `memcpy` walks the same bytes again, then `kain_hash_bytes` walks them a third time.

- **Optimization opportunity:**
  **GOD-TIER:** Combine all three scans into one:
  1. Scan the string once, finding both the length AND the first 32 bytes AND computing the hash in one pass.
  2. For short keys (<32 bytes), the length scan naturally discovers the prefix bytes.
  3. Use a streaming hash (like the tiny hash `kain_map_magic_prefix_state`) that can process prefix bytes as they're discovered.

  **Z3 approach:** Build a combined `scan_prefix_hash()` that returns `(length, prefix_state, full_hash)` in a single pass over memory. The key insight: for keys < 32 bytes, which is the common case for Kain map keys (service names like `"base.memory"` are 11 chars), the prefix IS the entire key. Skip the hash entirely and use `kain_map_magic_prefix_state` as both prefix AND hash.

- **Estimated impact:** **HIGH** — every `map_set()`, `map_get()`, `map_remove()` call in Kain goes through `kain_map_key_metadata`. This is the hottest path in the native map implementation. Reducing 3× data cache misses to 1× would be a major win.

- **SMT2 approach:**
  - Model the streaming hash function in 64-bit BV arithmetic.
  - Prove `kain_hash_bytes(key, len) == streaming_hash(key, len)` for len ≤ 32 (the common case).
  - For the prefix-state-only approach: prove that `kain_map_magic_prefix_state` provides sufficient entropy as a full hash replacement for keys < 32 bytes (no collisions on a training set of known map keys).

- **Confidence:** **MEDIUM** — the win is clear but the combined scan is tricky to implement without introducing UB (reading past the length found). Requires careful pointer handling.

---

### 10. `ownership.c` — Fixed occupancy linear scan in `kain_ownership_find_free_slot` could use CTZ-based word scan

- **Current pattern:** Already uses bitset + de Bruijn (good), but the outer loop:
  ```c
  for (uint32_t word_index = 0u; word_index < KAIN_OWNERSHIP_WORD_COUNT; ++word_index) {
      uint64_t free_mask = ~KAIN_OWNERSHIP_OCCUPANCY_WORDS[word_index];
      if (free_mask != 0u) {
          uint64_t low_bit = kain_ownership_isolate_low_bit_u64(free_mask);
          unsigned int bit_index = kain_ownership_low_bit_index_u64(low_bit);
          ...
      }
  }
  ```
  This scans up to 64 words (worst case: all 64 words have `free_mask == 0` before finding one with a free slot).

- **Optimization opportunity:**
  Maintain a **summary word** — a 64-bit value where each bit represents whether that word has any free slot. Only 64 words → one summary `uint64_t free_summary`:
  ```c
  static uint64_t free_summary = ALL_WORDS_FREE_INIT;
  // On alloc: if word becomes full, clear bit in summary.
  // On free: if word was full, set bit in summary.
  // Find free slot: low_bit_index_u64(free_summary & -free_summary) -> word_index.
  ```
  This collapses all 64 word scans to: `ctz(free_summary)` → single instruction.

- **Estimated impact:** **MEDIUM** — `find_free_slot` is called on every ownership registration (every `collapse`, `observe`, `decay` entry point). With 4096 regions, 64 words, the average scan is 32 iterations. With a summary word, it's 1 instruction + 1 de Bruijn lookup.

- **SMT2 approach:**
  - Model the summary word as a 64-bit bit-vector.
  - Prove: `free_summary & (1 << word_index) != 0` iff `~occupancy_words[word_index] != 0`.
  - Prove the update invariants: on alloc: `if new_occupancy_word == ALL_ONES then clear bit in summary`. On free: `if old_occupancy_word == ALL_ONES then set bit in summary`.

- **Confidence:** **HIGH** — simple bit-vector proof, well-understood pattern.

---

### 11. `ownership.c` — `kain_ownership_enqueue_deferred_decay_unlocked` uses `% 4096` (same pattern as #5)

- **Current pattern:**
  ```c
  (KAIN_OWNERSHIP_DEFERRED_DECAY_TAIL + 1u) % KAIN_OWNERSHIP_MAX_REGIONS;
  (KAIN_OWNERSHIP_DEFERRED_DECAY_HEAD + 1u) % KAIN_OWNERSHIP_MAX_REGIONS;
  ```
  Same `% 4096` → `& 4095` opportunity. But also the ring buffer count is checked separately:
  ```c
  if (KAIN_OWNERSHIP_DEFERRED_DECAY_COUNT >= KAIN_OWNERSHIP_MAX_REGIONS)
      return KAIN_OWNERSHIP_ERR_CAPACITY;
  ```
  This capacity check makes the wrap-safe `&` mask correct.

- **Estimated impact:** Same as #5.

- **Confidence:** **HIGH**

---

### 12. `core.c` — `kain_hash_bytes` — tail loop with memcpy of 0-7 bytes, could be fully branchless

- **Current pattern:**
  ```c
  if (length > 0u) {
      uint64_t tail = 0u;
      memcpy(&tail, bytes, length);  // length < 8
      hash ^= kain_mix_u64(tail ^ ((uint64_t)length << 56u));
  }
  ```

- **Optimization opportunity:**
  The tail memcpy of 1-7 bytes is a byte-by-byte copy. Could be replaced with a branchless load using a mask:
  ```c
  // Z3-discovered mask for each possible tail length
  static const uint64_t tail_masks[8] = {
      0x00, 0xFF, 0xFFFF, 0xFFFFFF, 0xFFFFFFFF,
      0xFFFFFFFFFF, 0xFFFFFFFFFFFF, 0xFFFFFFFFFFFFFF
  };
  uint64_t tail = *(const uint64_t*)&bytes[-offset] & tail_masks[length];
  ```
  Or use `memcpy` + a magic constant to XOR away the garbage bytes. The current `memcpy` of ≤7 bytes typically compiles to a `rep movsb` or byte loop.

- **Estimated impact:** **LOW** — tail handling is a small fraction of hash calls, and most keys are 0-32 bytes so the fast-path 8-byte chunks dominate.

- **Confidence:** **LOW** — compiler already optimizes small memcpy well on modern toolchains.

---

### 13. `core.c` — `kain_map_entry_matches_prehashed` — fallback memcmp for exact match could use 8-byte word compare

- **Current pattern:**
  ```c
  static int kain_map_entry_matches_prehashed(MapEntry* entry, ..., uint64_t key_hash, ...) {
      return entry->occupied &&
          entry->hash == key_hash &&
          entry->key_prefix == key_prefix &&
          entry->key_length == key_length &&
          (entry->key == key || memcmp(entry->key, key, key_length) == 0);
  }
  ```
  The `memcmp` fallback for short keys (typically <32 bytes for maps) is a byte-by-byte comparison.

- **Optimization opportunity:**
  For strings ≤ 32 bytes (which is the common case for Kain map keys — think service names, config keys), compare as aligned 8-byte chunks instead of bytes. The key_length is known, so the comparison can use `memcmp` in 8-byte chunks with a tail mask.

  Even better: the `key_prefix` already contains the first 32 bytes of the key as a hash. The prefix hash collision proves the first 32 bytes match. Combined with the `key_length` check, the only remaining match is byte 32+. For keys ≤ 32 bytes (common), the prefix match is sufficient and no memcmp is needed at all.

- **Estimated impact:** **MEDIUM** — every `map_get()` that misses the tiny dispatch goes through this function. The `memcmp` is the last and most expensive check. If prefix match + length check is already exhaustive for keys ≤ 32 bytes, the memcmp is dead code for the common case.

- **Confidence:** **MEDIUM** — depends on key length distribution. If the average map key is ≥ 32 bytes (unlikely for Kain), this won't help.

---

## Files With Nothing Obvious (already near bedrock)

| File | Reason |
|------|--------|
| **`arena.c`** | 205 lines. Already uses power-of-two alignment math, `& (align-1)`, simple init/reset. No linear scans, no modulus, no hot loops beyond frame markers. |
| **`batch_queue.c`** | 149 lines. Simple batch lock/unlock/drain pattern. `memcpy` of pending→active is the only bulk operation. No hot loops. |
| **`converge.c`** | Already uses `mix64`, magic constants, branchless `kain_converge_lowbit_lane` with `_BitScanForward64`/`__builtin_ctzll`, proper power-of-two masking. God-tier optimization already present. |
| **`deferred_free.c`** | 80 lines. Free-list with sentinel markers. Pure pointer-chasing linked list. Tiny — no hot path optimization needed. |
| **`entangle.c`** | 90 lines. Tiny array of 128 bindings. Simple copy/strlen. Not hot. |
| **`fanout.c`** | 260 lines. Thread pool with atomic work stealing. The only loop is `kain_fanout_drain_job` which calls a user-provided function. No optimization opportunity. |
| **`handle.c`** | 124 lines. Simple magic-packed uint64_t encoding with bit shifts. Tiny, clean, already efficient. |
| **`simd.c`** | 350 lines. Already has AVX2 (`__builtin_ia32_pmuludq256`) and AVX512 (`__builtin_ia32_pmuludq512`) intrinsics with Z3 proofs. Already near bedrock. |
| **`virtual_alloc.c`** | 140 lines. Thin wrapper around OS VM calls (VirtualAlloc, mmap). Two small loops (batch_map). OS calls dominate, not CPU. |
| **`wire.c`** | 150 lines. Esoteric periodic checksum with precomputed table (KAIN_WIRE_PERIOD_WRAP_COUNTS of 256 entries). Already strange/optimized by nature. |
| **`bitfield.c`** | 75 lines. Simple bitfield extract/insert with memcpy of uint64_t. Thin wrapper. |
| **`union.c`** | 80 lines. Simple union copy with `memset`/`memcpy`. Thin wrapper. |
| **`freestanding_stubs.c`** | 95 lines. Bare-metal no-op stubs — not a hot path in hosted builds. |
| **`machine_stones.c`** | Already uses token-signature switch (`kain_machine_token_signature`), `mix64`, power-of-two masks, Z3-proven capability dispatch. Near bedrock. |

---

## Already Optimized (existing proofs cover these)

| Pattern | Where | Proof Files |
|---------|-------|-------------|
| **de Bruijn low-bit decoder** (0x03f79d71b4cb0a89) | `actor.c` — `kain_actor_low_bit_index_u64` | `ownership-debruijn-low-bit-distinct.smt2`, `actor-table-debruijn-hash-distinct.smt2` |
| **de Bruijn low-bit decoder** | `ownership.c` — `kain_ownership_low_bit_index_u64` | Same proofs |
| **Perfect hash map tiny dispatch** (≤4 entries) | `core.c` — `kain_map_rebuild_tiny_dispatch` | `map-magic-current-intent-pool.smt2`, `map-eight-slot-selection.smt2` |
| **Service token collision freedom** | (used by service registry) | `service-registry-magic-collision-free.smt2` |
| **Capability token signature collision freedom** | `machine_stones.c` — `kain_machine_capability_mask_for_key` | `native-machine-capability-token-signatures-are-collision-free.yaml` |
| **SIMD pmuludq i32-domain equivalence** | `simd.c` — AVX2/AVX512 dot products | `simd-i32-domain-even-dword-mul-equivalence.smt2` |
| **SIMD affine bias-dot factorization** | `simd.c` — affine stats | `simd-affine-bias-dot-factorization.smt2` |
| **Alloc small cache bin bounds** | `memory.c` — `kain_alloc_cache_small_bin` | `memory-small-cache-bin-bounds.smt2` |
| **Ownership state classifier table** | `ownership.c` — `KAIN_OWNERSHIP_BUSY_TABLE` | `ownership-state-classifier-table.smt2` |
| **Ownership errno table lookup** | `ownership.c` — `kain_ownership_errno_from_status` | `ownership-errno-table-lookup.smt2` |

---

## Summary of Impact by Category

| Category | Count | Top Targets |
|----------|-------|-------------|
| **CLZ/CTZ replacement** | 2 | `buddy.c` log2_exact, buddy.c required_height |
| **strcmp ladder → token switch** | 1 | `cpu.c` capability_mask_for_key |
| **Linear scan → range cache** | 2 | `ownership.c` find_range_slot, `fixup.c` find_object_by_pointer |
| **`%` → `&` mask** | 2 | `ownership.c` deferred decay ring, `event.c` hash |
| **Linked-list walk → hash** | 2 | `event.c` topic/subscriber, `memory.c` large alloc cache |
| **Combined scan (strlen+memcpy+hash)** | 1 | `core.c` kain_map_key_metadata |
| **Bitset occupancy summary** | 1 | `ownership.c` find_free_slot |

**Total distinct high-value optimization targets identified: 13**

---

## Implementation Priority

| Priority | Target | Rough Speedup | Effort | Proof Complexity |
|----------|--------|--------------|--------|-----------------|
| **P0** | `buddy.c` `kain_buddy_log2_exact` → CLZ | 10-30× on hot alloc/free path | 1 file, 2 functions | Simple BV equivalence |
| **P0** | `ownership.c` `% 4096` → `& 4095` | 2-5× per deferred decay op | 2 lines | Trivial BV proof |
| **P0** | `event.c` `% 256` → `& 255` | 2-5× per event hash | 1 line | Trivial |
| **P1** | `cpu.c` strcmp ladder → token switch | 30-50× per capability query | 1 file, add token sig | Med (prove collision-free on 36+ strings) |
| **P1** | `ownership.c` free_slot → summary word | 16-64× per free slot search | 3-5 lines + update logic | Simple BV invariant |
| **P2** | `ownership.c` range scan → LRU cache | ~4096× worst-case improvement | New range cache pattern | Probabilistic/memory model |
| **P2** | `fixup.c` range scan → larger cache | Same as ownership | Same pattern | Same |
| **P2** | `memory.c` large cache linked list → secondary hash | ~4× per large alloc | Restructure bucket storage | Med (hash distribution) |
| **P3** | `core.c` key_metadata triple scan | ~3× per map operation | Combine strlen+memcpy+hash | Complex pointer arithmetic |
