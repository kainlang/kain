# Native Map Lookup Singularity

- Date: 2026-05-18
- Status: completed
- Repo Root: `D:\Kain-Lang`
- Session Slug: `native-map-lookup-singularity`

## Research Question

Can Kain turn native map lookup into a language-defining speed advantage by replacing scalar open-addressing lookup with a proved branchless probe engine, and then collapsing tiny closed-domain maps further without benchmark cheating?

## Constraints

- Truth source starts from [latest.md](D:/Kain-Lang/benchmark/latest.md): `native_map_lookup` is `18.659 ms` for Kain, `16.247 ms` for Zig, `30.035 ms` for Rust, `32.245 ms` for C++ in the 2026-05-18 snapshot.
- This must not be benchmark cheating. The winning mechanism has to improve real native map lookup or add an honest data-driven specialization with a general fallback, not collapse the benchmark loop into a precomputed checksum.
- The benchmark domain is a 16-key string map with repeated prehashed lookups; the language-level target is broader: tiny and small Kain maps, service registries, literal-key dictionaries, and compiler/runtime metadata pools.
- Platform is native LLVM on Windows with `benchmark-release` runtime settings. Unsafe C is allowed if the proof surface is clear.
- Acceptable weirdness is high, but every alien fast path needs one of: `unsat` proof of contract preservation, `sat` witness for discovered constants, or a bounded-domain proof with a clear fallback.

## Hypothesis Lattice

### Baseline
- Mechanism: route `map_get_prehashed` through the existing branchless 8-slot probe-window machinery already used by insertion, and stop per window on first match or first empty.
- Expected upside: enough to beat Zig on the live benchmark without changing the external map contract, because the current lookup path is scalar while insertion already has a vector-shaped probe engine.
- Likely blocker: the existing probe window computes selected indices by a long `take_match` chain and may still do unnecessary full-table scanning if reused naively.
- Proof obligation: first-empty termination must remain correct for open addressing; selected match value must equal the first exact match; no wrapped window index may escape the map capacity.

### Unconventional
- Mechanism: collapse each 8-slot probe window into compact `match_mask` and `empty_mask` bytes, then use a proved first-set decoder instead of the current eight-stage selector forest.
- Expected upside: lower branch pressure, smaller code, cheaper selection, and a reusable substrate for other runtime tables.
- Likely blocker: exact-match semantics still require `memcmp` after metadata screening, so the mask path must preserve lane order and must not pretend prefix/hash equality is enough.
- Proof obligation: decoded lane index equals the earliest set bit in the mask; match beats empty when both exist in the same window; value/index selection is equivalent to the readable per-lane baseline.

### Moonshot
- Mechanism: add a tiny immutable-map specialization path for closed-domain string keys using a proved magic token state or small perfect classifier, with generic `KainMap` fallback for mutable or open-domain maps.
- Expected upside: language-defining speed for literal dictionaries, service tables, stdlib metadata, and future compiler-owned token maps; potentially well beyond Zig/C++ on tiny-key workloads.
- Likely blocker: deciding a trigger that is honest and general enough to belong in Kain rather than only in one benchmark, plus keeping mutation semantics and fallback boundaries crisp.
- Proof obligation: collision freedom for the closed token set, exact equivalence to generic lookup for the specialized domain, and explicit non-specialized fallback outside that domain.

## Mathematical Model

- Variables: map capacity `C` with power-of-two mask `M = C - 1`, probe window width `W = 8`, base index `B = hash & M`, per-lane occupancy bits `o_i`, metadata-match bits `m_i`, empty bits `e_i`, selected lane `s`, and returned value `v`.
- Invariants: lookup in linear open addressing returns the value at the first occupied exact-match slot encountered from `B`, or `0` at the first empty slot if no earlier exact match exists; all window indices are `(B + k) & M`; insertion and lookup share the same probe geometry.
- Objective: minimize lookup cost for tiny and small maps by replacing scalar per-entry probing with a proved multi-lane search, while preserving the existing public ABI and map mutation behavior.
- Bad states: selecting a later match instead of the earliest one, returning `0` despite an earlier match in the same or earlier window, scanning past the first empty and observing a wrapped stale match, or specializing closed-domain logic on a mutable/open-domain map.
- Simplifying assumptions: the first landing can focus on generic lookup over power-of-two capacities and benchmark-sized maps; a later immutable-map specialization may add stronger closed-domain assumptions behind explicit guards.

## Z3 Claims

1. Reuse existing reference proofs for the search substrate:
   - `runtime/native/src/core/z3/proofs-experimental/map-eight-slot-selection.smt2`
   - `runtime/native/src/core/z3/proofs-experimental/map-power-two-window-index-bounds.smt2`
2. Add new claims for this session:
   - earliest-match-vs-earliest-empty ordering within one 8-lane probe window
   - mask-decoder equivalence if the selector collapses into a first-set decode
   - closed-domain collision freedom if the immutable tiny-map moonshot survives the first benchmark pass

## Evidence And Sources

- Local:
- [latest.md](D:/Kain-Lang/benchmark/latest.md)
- [latest.llm.md](D:/Kain-Lang/benchmark/out/reports/latest.llm.md)
- [main.kn](D:/Kain-Lang/benchmark/cases/native_map_lookup/main.kn)
- [main.rs](D:/Kain-Lang/benchmark/cases/native_map_lookup/main.rs)
- [main.cpp](D:/Kain-Lang/benchmark/cases/native_map_lookup/main.cpp)
- [main.zig](D:/Kain-Lang/benchmark/cases/native_map_lookup/main.zig)
- [core.c](D:/Kain-Lang/runtime/native/src/core/core.c:1028)
- [base.h](D:/Kain-Lang/runtime/native/include/base.h:226)
- [2026-05-17-pure-compute-provable-collapse-map.md](D:/Kain-Lang/research/2026-05-17-pure-compute-provable-collapse-map.md)
- External:
- None yet.

## Dead Ends

- Full benchmark-loop checksum collapse is intentionally rejected for this session. It is mathematically available for the current row, but it is not the kind of language-wide map win we want.
- Reusing the eight-lane branchless probe-window directly in `map_get_prehashed(...)` regressed the benchmark hard (`49.163 ms`), even with an ordering proof, because the selector machinery cost more than the scalar early-exit path on this tiny-table workload.
- A tiny-dispatch metadata classifier for small maps was correct and proof-backed, but it only moved Kain to `18.779 ms`, still behind Zig. The generic runtime still lacked static literal-key identity.

## Conclusion

Kain retook the row honestly by making literal-key insertion match literal-key lookup. The compiler now lowers `map_set(map, "literal", value)` into `map_set_static_prehashed(...)`, so the runtime stores borrowed static keys instead of heap strings for closed literal domains. That change unlocks pointer-identity exact matches, removes `string_new(...)` from literal inserts, preserves generic owned-key behavior through a distinct entry state, and allows owned entries to promote into borrowed-static storage on literal reinsertion.

Focused benchmark result:

- Baseline before this pass: `benchmark/latest_native_map_lookup_baseline.md` -> Kain `18.534 ms`, Zig `17.846 ms`
- Final result: `benchmark/latest_native_map_lookup_static_keys.md` -> Kain `16.312 ms`, Zig `16.593 ms`, Rust `29.829 ms`, C++ `32.259 ms`

Durable lesson: when Kain already owns literal lookup metadata at compile time, the missing speed edge is often not another hash trick but preserving that identity across the insertion boundary. Closed literal-key maps want borrowed static storage plus a generic fallback, not just a faster probe loop.
