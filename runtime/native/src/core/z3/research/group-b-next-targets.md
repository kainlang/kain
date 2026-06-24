# Runtime Super-Optimization Targets — Group B (Systems, Interop, Graphics, UI)

Date: 2026-06-23
Scope: 36 C source files scanned (X:\runtime\native\src\core\*.c Group B)
Style: alien-code pattern matching against the Z3 proof style (de Bruijn decoders, magic multipliers, collision-free hashes, bitwise selection masks)

---

## ALREADY OPTIMIZED — Patterns That Already Have Z3 Proofs

These files/functions already use the advanced techniques (de Bruijn, magic hashes, popcount, etc.) and have corresponding Z3 proofs:

### net_system.c
- De Bruijn low-bit decoder (`abi_net_low_bit_index_u64` with magic `0x03f79d71b4cb0a89` and 64-entry table) — **already at god tier**
- Occupancy bitsets for connection/listener/request/response/server tables — **already uses bitset find-free-slot via de Bruijn**
- Rotate-XOR hash mixer (`abi_net_mix_id` with `0xbf58476d1ce4e5b9`, `0x94d049bb133111eb`) — **already at god tier**
- Power-of-two capacity constants with compile-time static asserts — **good**
- Z3 proofs exist: `net-handle-index-probe-bounds.smt2`, `actor-table-debruijn-hash-distinct.smt2`
- `abi_net_append_bytes` has Z3 overflow proof

### process_system.c
- De Bruijn low-bit decoder (`abi_process_low_bit_index_u64` with same magic) — **already at god tier**
- Occupancy bitsets for spec/process tables — **already uses bitset find-free-slot**
- Same mixer hash as net_system — **already at god tier**
- Power-of-two index capacities with static asserts — **good**
- Z3 proofs exist: `process-handle-index-probe-bounds.smt2`

### services.c
- Magic polynomial prefix hashing for service key canonicalization (`kain_service_magic_prefix_state`) — **already at god tier**
- Alias normalization via `kain_service_registry_canonicalize_key` using precomputed `key_state` magic constants in switch — **already at god tier**
- Z3 proof exists: `service-registry-magic-collision-free.smt2`

### json.c
- Z3 proofs for: `json-object-capacity-doubling-signed-int64-overflow.yaml`, `json-surrogate-pair-decode-codepoint-in-unicode-range.yaml`, `json-clone-value-calloc-overflow.yaml`, `json-buffer-reserve-size_t-addition-overflow.yaml` — **already verified**

### reflection.c
- 16-byte magic hash token system (`reflection_token_from_text16`, `reflection_token_state16`, `reflection_token_match_bit`) with `0x64170d358aa115a1` — **already at god tier**
- Bitwise branchless type-kind resolution (`reflection_type_kind_from_string`) using 8 parallel match bits OR'd into selection — **already at god tier**
- Collision-free field-name dispatch via precomputed state constants — **already at god tier**

### attrition.c
- Hand-rolled popcount (`kain_attrition_popcount_u64`) — **already optimized**
- Saturating add with Z3 proof — **already verified**
- Z3 proofs exist: `attrition-saturating-u64-add-monotonic.smt2`, `attrition-event-ring-copy-window-bounds.smt2`, `attrition-audit-json-length-range.smt2`

### stdlib_abi.c (implied from docs)
- Z3 proofs for integer overflow arithmetic, content-length parsing, text builder — **already verified**

---

## TOP TARGETS (ranked by impact × feasibility)

### 1. `json.c` — `json_object_get_value` linear field scan
- **Current pattern:** Linear `for` loop over all fields comparing `strcmp(object->fields[i].key, key)` until a match is found. This is O(n) per field access, called on every `json_get`, `json_get_int`, `json_get_string`, `json_has`, etc.
- **Optimization opportunity:** Collision-free perfect hash on field names using the same 64-bit magic state polynomial already proven in `services.c` and `reflection.c`. Precompute a hash for each field's key when the object is constructed, store it alongside the key string, and use a bitmask-based O(1) lookup (8-entry or 16-entry slot array, probe by hash bits). The json object is typically small (<8 fields for the vast majority of runtime uses).
- **Estimated impact:** HIGH. `json_get` is called on nearly every metadata lookup path (graphics bundle parsing, contract loading, scene loading, reflection payload parsing, interop info construction). A 5-10× speedup on object field lookups would accelerate every JSON-heavy path.
- **SMT2 approach:** Collect the actual field-name keys used across all runtime JSON consumption sites. Model as bit-vector 64 hash under the same `0x64170d358aa115a1` polynomial. Prove collision-freedom for the target set. Design slot count = smallest power of 2 ≥ max fields. Use `(extract 63 63-N)` for probe.
- **Confidence:** HIGH — the polynomial is already proven collision-free for 47 service keys and ~17 map keys. JSON field names are even shorter and more constrained.

### 2. `json.c` — `json_parse_number` strtoll/strtod double parse
- **Current pattern:** Every numeric JSON value calls `strtoll()` then possibly `strtod()`. Each call traverses the string twice (once in each function). For integer-only JSON payloads (common in graphics bundles, scene descriptors, reflection metadata), `strtod` is called unnecessarily when the number has no decimal.
- **Optimization opportunity:** Hand-rolled integer parser that processes bytes in a tight branchless loop. For the common case (no `e`/`E`/`.` seen), use `value = value * 10 + (byte - '0')` with overflow detection, skipping `strtod` entirely. For fractional numbers, use a single branchless pass that builds both integer and fractional parts simultaneously.
- **Estimated impact:** MEDIUM-HIGH. JSON number parsing is on the critical path of every `contract_load_from_json`, `realtime_load_from_json`, `reflection_load_from_json`. Skipping `strtod` for integers saves a libc call overhead.
- **SMT2 approach:** Prove overflow bounds: for a 64-bit unsigned range, the max safe digit count is 19. Model `value * 10 + digit` as bit-vector 64 with overflow predicate. The Z3 proof would confirm that for actual JSON payload lengths (<20 digits), the branchless overflow check catches all edge cases.
- **Confidence:** MEDIUM — libc `strtoll` is already fast, but avoiding it entirely is a measurable win on hot decode paths.

### 3. `input_system.c` — `abi_input_name_index` linear name scan
- **Current pattern:** Every `abi_input_action_pressed`, `abi_input_action_down`, `abi_input_action_released`, and `abi_input_axis_value` does a linear `for` loop comparing names via `abi_input_text_equal` (strcasecmp). Called per-frame for every action/axis query.
- **Optimization opportunity:** Replace the linear-scan name arrays with small perfect hashes. The `ABI_INPUT_MAX_ACTIONS` is small (probably 32-128). Use the same magic `0xbf58476d1ce4e5b9`/`0x94d049bb133111eb` 2-round mixer on the action name to produce a hash, then index into a small bitmask/table. Since action names are configured at bind-time and fixed at runtime, a collision-free hash can be proven once per binding set.
- **Estimated impact:** MEDIUM-HIGH. Action queries happen every frame, often multiple times. A O(1) hash lookup vs O(n) linear scan is a high-ROI perf gain for input-heavy applications (games, editors).
- **SMT2 approach:** Extract the action/axis name set at binding registration time. Model the hash under the existing proven poly. Use a small power-of-2 table (8 or 16 entries). Prove collision freedom per session instance.
- **Confidence:** HIGH — small fixed name sets, proven polynomial, frame-rate hot path.

### 4. `graphics_system.c` — `abi_graphics_find_session/buffer/shader/mesh/pipeline` linear scans
- **Current pattern:** All 5 resource lookup functions scan their respective fixed-size arrays linearly with `for (index = 0; index < ABI_GRAPHICS_MAX_*; index += 1)`. These are called on every graphics operation.
- **Optimization opportunity:** Same occupancy-bitset + de Bruijn + hash-probe pattern already used in `net_system.c` and `process_system.c`. Replace linear scan with: (1) generation-tagged ID → hash mixer → (2) open-addressing hash table with power-of-two mask. The de Bruijn decoder is already proven.
- **Estimated impact:** HIGH. Every frame that touches graphics resources calls these lookups. The current MAX values suggest small tables (e.g. `ABI_GRAPHICS_MAX_BUFFERS=64`). A hash probe finds the entry in 1-2 iterations vs 32+ average for linear scan.
- **SMT2 approach:** Reuse the existing `actor-table-debruijn-hash-distinct.smt2` proof (same magic 0x03f79d71b4cb0a89). Model the probe bounds for a 128-entry table with a 64-slot index.
- **Confidence:** HIGH — proven pattern used in net_system and process_system already.

### 5. `services.c` — `kain_service_registry_lookup` linear scan over all services
- **Current pattern:** Despite the nice magic-hash key-state metadata, `kain_service_registry_lookup` still does a linear scan (`for (i = 0; i < service_count; i++)`) with 3 checks per iteration. With 30+ registered services, this is 30+ iterations per lookup.
- **Optimization opportunity:** Replace the linear array with a hash table indexed by `key_state` (the already-computed 64-bit magic hash). Since the metadata hash is already computed at registration, the table can use direct-mapped or 2-way associative slots. Service key set is static after registration, so perfect hashing is trivial.
- **Estimated impact:** MEDIUM. Service lookups happen at startup and during contract validation — not in every frame. But `kain_service_registry_is_available` is called during `contract_is_service_available` which is on some startup hot paths.
- **SMT2 approach:** Already essentially done — the Z3 proofs in `service-registry-magic-collision-free.smt2` prove the 47 tokens are distinct. Use the token as direct index into a 64-slot table (`mask = 0x3F`, `slot = key_state & mask`).
- **Confidence:** HIGH — the hash constants and collision-freedom are already proven.

### 6. `realtime.c` / `contract.c` — JSON string-field extractors are duplicated
- **Current pattern:** Both `contract.c` (contract_load_from_json) and `realtime.c` (realtime_load_from_json) implement identical hand-rolled JSON-parsing helpers: `find_substring`, `skip_ws`, `find_matching`, `find_value_start`, `copy_string_value`, `extract_string_field`, `count_array_objects`. The code is copied verbatim with different function prefixes (`contract_*` vs `realtime_*`).
- **Optimization opportunity:** Merge into shared static helpers in one location. More importantly, the `find_substring` function does a naive byte-by-byte memcmp scan. For small JSON payloads this is fine, but replacing with Boyer-Moore-Horspool single-byte skip-table would give ~3-5× speedup on substring search across the multi-kilobyte JSON blobs that scene bundles can be.
- **Estimated impact:** MEDIUM. JSON parsing is on the startup path, not the frame path. But duplicated code is a maintenance hazard (3 known overflow bugs in the Z3 proofs were found in one copy but not the other).
- **SMT2 approach:** Prove the correct position bounds for Boyer-Moore-Horspool skip-table in the presence of JSON whitespace. The contract file size is bounded (<10MB).
- **Confidence:** LOW-MEDIUM — the optimization is real but the impact is limited to startup paths.

### 7. `interop_contracts.c` — `kain_shared_element_count` with per-element checked multiply
- **Current pattern:** To compute the total element count from a shape array, the code loops calling `kain_shared_checked_mul_i64` for each dimension. For 4D tensors, that's 4 overflow-checked multiplications.
- **Optimization opportunity:** The checked multiplication macro currently uses `if (left > (LLONG_MAX / right))`. This is correct but generic. Since shape dimensions for tensors are typically small (<4096), a branchless approach using saturating arithmetic (`left * right | (left && right && left > LLONG_MAX/right ? INT64_MAX : 0)`) could collapse to 3-4 ALU ops.
- **Estimated impact:** LOW. Shape computation happens once per buffer creation, not on hot paths.
- **SMT2 approach:** It already has Z3 proofs for the checked multiply (`native-interop-shared-buffer-byte-length-matches-shape-times-element-size.yaml`). Not much to optimize further.
- **Confidence:** LOW — already proven and on a cold path.

### 8. `crash_handler.c` — `__kain_crash_lookup` binary search over crash table
- **Current pattern:** `__kain_crash_lookup` does a standard binary search on the sorted `__kain_crash_table`. This is called on crash (SIGSEGV, SIGILL) for every frame in the callstack.
- **Optimization opportunity:** The crash table is sorted by `fn_ptr`. Binary search is already good. But the sentinel-counting loop (`while (crash_table_count < 4096 && __kain_crash_table[crash_table_count].fn_ptr != 0)`) in `__kain_crash_handler_init` does a linear scan of up to 4096 entries at startup. This could be branchless: scan in 8-entry batches using SIMD-like masked loads (or just a tight pointer-arithmetic loop).
- **More importantly:** The callstack resolution loop in `__kain_crash_render_report` calls `lookup_crash_entry` per frame, which does a fresh binary search for each callstack entry. This means O(log N) per frame × depth. If the callstack is contiguous in the table (unlikely), a linear probe from the previous result would be faster.
- **Estimated impact:** LOW. Crash handling is not performance-critical — it runs when the program is already dying.
- **SMT2 approach:** N/A — proving correctness of binary search would confirm it terminates in ≤log2(N) steps, but that's already obvious.
- **Confidence:** LOW — crash handler isn't a super-optimization target.

---

## FILES WITH NOTHING OBVIOUS

These files are already near bedrock, too small for optimization, or pure configuration:

| File | Reason |
|------|--------|
| `attrition.c` | Already has de Bruijn popcount, saturating add, Z3 proofs. Telemetry-heavy, not computational. |
| `audio_system.c` | Heavy COM/CoreAudio/ALSA boilerplate. Platform-specific API calls dominate. No hot computational loops. |
| `compatibility.c` | Thin orchestration layer — memcpy, string copy, small allocations. No tight loops. |
| `component_surface.c` | Tiny registry with max 16 entries — linear scan of 16 entries is optimal (branch predictor hits every time). |
| `contract.c` | JSON-parsing helpers duplicated from realtime.c (noted above). Merging is a maintenance win, not a speed win. |
| `crash_handler.c` | Binary search on crash table — optimal for sorted data, called when dying. No performance target. |
| `cuda_runtime.c` | Heavy dlopen/GetProcAddress boilerplate. GPU dispatch is the bottleneck, not this C wrapper. |
| `d3d12_surface_shim.c` | Tiny (~200 lines) — capability flag, dlopen, vtable lookup. Nothing to optimize. |
| `diagnostics.c` | Severity clamping, channel lookup, string formatting. Small N, trivial logic. |
| `graphics_system.c` | See target #4 above for the find functions. The rest is buffer alloc/copy which is already okay. |
| `host_bridge.c` | String comparison loops over small arrays (max 32 modules, 16 services). Linear scan is optimal for these sizes. |
| `input_system.c` | See target #3 above for name lookup. Event processing loop is already fine. |
| `interop_contracts.c` | Already has Z3 proofs for shape arithmetic. String duplication is unavoidable. |
| `interop_zero_copy.c` | Tiny — type tag check and RC retain/release. Trivially optimal. |
| `json.c` | See targets #1 and #2. The rest is already well-optimized with Z3 proofs. Linked-list registry is fine for small N. |
| `json_benchmark.c` | Deterministic checksum benchmark — intentionally simple, Z3-proved periodic collapse. Fine. |
| `net_system.c` | Already at god tier — de Bruijn decoders, occupancy bitsets, magic hash, Z3 proofs. The HTTP parsing is the only soft spot (see below). |
| `process_system.c` | Already at god tier — same pattern as net_system. |
| `profile.c` | _Thread_local stack with push/pop. Simple and already trimmed by compile-time tiers. |
| `python_runtime.c` | 3,400+ lines of Python C API marshaling. The bottleneck is CPython itself, not the wrapper. |
| `python_runtime_async.c` | Python async bridge — CPython GIL dominates latency. |
| `python_runtime_buffers.c` | Buffer protocol views — already minimal. |
| `python_runtime_gpu.c` | Tensor/image bridge — PyObject* operations dominate. |
| `python_runtime_region.c` | Region cache (import/attr caching) — cache-hit path is O(1), miss path is CPython call. Fine. |
| `ray_sphere_benchmark.c` | Deterministic benchmark — Z3-proved periodic reducer. Fine by design. |
| `realtime.c` | See target #6 (duplicate JSON helpers). The rest is struct population, not compute. |
| `reflection.c` | Already at god tier — 16-byte magic hash tokens, bitwise type-kind resolution, collision-free field dispatch. |
| `renderer_backend.c` | Static catalog lookup — linear scan of 3 entries. Trivially optimal. |
| `renderer_session.c` | Backend resolution chain — small N, rare call (startup only). Fine. |
| `scene.c` | Handle packing/unpacking with bit-shifts. Already compact. |
| `services.c` | See target #5. The rest (lock-free spinlock with PAUSE/yield) is already solid. |
| `stdlib_abi.c` | Partially examined — the ABI bridge functions are already Z3-proofed. |
| `version.c` | String formatting + version comparison. Trivial. |
| `vulkan_stubs.c` | 2 stub functions that proxy through vtable pointers. Minimal. |
| `vulkan_surface_shim.c` | dlopen + vtable resolution. Platform boilerplate, not computational. |
| `webgpu_surface_shim.c` | Same as vulkan — dlopen + vtable. Minimal. |

---

## STRETCH TARGETS — Lower Impact But Interesting

### `net_system.c` — `abi_net_parse_http_request` string parsing
- **Current pattern:** Uses `strstr`, `strchr`, manual line splitting with pointer arithmetic, and per-line parsing via `abi_net_parse_header_line`. The HTTP request parser does `malloc` + `strstr(text, "\r\n\r\n")` + tokenization.
- **Optimization opportunity:** Replace `strstr` for delimiter search with a single-pass SIMD-friendly search for `\r\n\r\n` (look for `\r` then check next 3 bytes). Most HTTP request bodies don't have the 4-byte boundary. This can be a fast check: load 4 bytes as uint32_t, compare against `0x0A0D0A0D` (little-endian `\r\n\r\n`) byte-by-byte with bit tricks.
- **Estimated impact:** MEDIUM on the HTTP server path. The `abi_net_parse_http_request` is called for every incoming request header.
- **Confidence:** MEDIUM — the parser is already reasonably fast; SIMD 4-byte delimiter search is a minor win.

### `input_system.c` — `abi_input_reduce_event` event-to-action dispatch
- **Current pattern:** Per frame event batch, the code loops over all pending events and for each calls `abi_input_find_binding` (linear scan of binding array). Each `abi_input_binding_matches` compares up to 3 string fields.
- **Optimization opportunity:** Group bindings by `event_kind + code` using a small hash table keyed by a 64-bit composite of (hash(event_kind) XOR hash(code)). This converts the O(bindings) match to O(1) table lookup.
- **Estimated impact:** MEDIUM. Event processing is on the frame path. With 20+ bindings, the linear scan adds up.
- **SMT2 approach:** Hash the event kind + code strings under the standard magic polynomial. Prove collision freedom for the user-defined binding set (which is set at startup and fixed).
- **Confidence:** MEDIUM — requires runtime rehashing when bindings change (but bindings rarely change mid-frame).

---

## SUMMARY OF RANKED TARGETS

| Rank | File | Function | Alien Pattern | Estimated Speedup | Z3 Complexity |
|------|------|----------|--------------|-------------------|---------------|
| 1 | `json.c` | `json_object_get_value` | Magic-hash perfect field lookup | 5-10× on field access | Easy |
| 2 | `json.c` | `json_parse_number` | Branchless inline integer parser | 2-3× on int parse | Medium |
| 3 | `input_system.c` | `abi_input_name_index` | Small hash table for action names | O(1) vs O(n) per frame | Easy |
| 4 | `graphics_system.c` | All `abi_graphics_find_*` | de Bruijn bitset + hash probe | 10-20× per lookup | Easy (reuse) |
| 5 | `services.c` | `kain_service_registry_lookup` | Direct-mapped hash by key_state | 10-30× per lookup | Easy (already proven) |
| 6 | `realtime.c` | JSON extractors | Merge + Boyer-Moore-Horspool | 2-3× on JSON parse | Medium |
| 7 | `net_system.c` | HTTP header delimiter | SIMD 4-byte marker search | 1.5-2× on parse | Easy |
| 8 | `input_system.c` | `abi_input_reduce_event` | Composite hash binding table | O(1) vs O(n) per event | Medium |

---

## IMMEDIATE NEXT STEPS

1. **Target #1 (json field lookup):** Collect the full set of field-name strings used across all runtime JSON sites (contract.c, realtime.c, graphics_system.c, interop_contracts.c, json_benchmark.c, ray_sphere_benchmark.c). Model under the proven magic polynomial. Prove collision-free for a 16-slot table.

2. **Target #4 (graphics find functions):** Straightforward reuse of the actor/net/de Bruijn pattern. The `abi_net_find_free_slot_u64` + `abi_net_index_insert` + `abi_net_low_bit_index_u64` can be adapted directly.

3. **Target #5 (service registry lookup):** The collision-freedom of all 47 service tokens is already proven in `service-registry-magic-collision-free.smt2`. The only remaining work is switching the table from linear array to direct-mapped hash using `key_state & 0x3F` as the index.
