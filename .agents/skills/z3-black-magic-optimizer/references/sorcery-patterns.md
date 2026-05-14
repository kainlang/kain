# Sorcery Patterns

Use this reference after the initial scan finds a hot path or suspicious constants.

Always save the SMT file that discovered, proved, or rejected a candidate in the nearest `proofs-experimental/` folder. Prefer descriptive names such as `service-token-magic-collision-free.smt2` or `actor-slot-debruijn-low-bit-distinct.smt2`.

## Closed-domain token classifiers

Best for service names, UI kind strings, reflection tokens, command names, opcode names, and static map keys.

1. Encode each token as one to four 64-bit little-endian words plus length.
2. Choose a parametric hash shape: xor/mul/rot/shift/finalizer.
3. Ask Z3 for a `sat` witness where all known tokens are distinct under a small table mask or full 64-bit state.
4. Freeze the witness constants.
5. Prove `(not (distinct token_0 ... token_n))` is `unsat` for the closed token list.
6. Benchmark against the old string compare, map lookup, or branch ladder.

Kain examples: `map-magic-current-intent-pool.smt2`, `service-registry-magic-collision-free.smt2`, `reflection-ui-token-magic-collision-free.smt2`.

## Branchless one-hot selection

Best when at most one predicate can be true.

Pattern:

```c
mask = 0 - (uint64_t)predicate;
selected = (a & mask_a) | (b & mask_b) | ...;
```

Z3 claim: if each mask is 0/1 and popcount of predicates is at most 1, selected equals the matching value or zero.

Kain example: `map-eight-slot-selection.smt2`.

## Power-of-two windowing

Best for ring buffers, fixed-capacity tables, frame slots, and scheduler queues.

Pattern:

```c
index = cursor & (capacity - 1);
```

Z3 claim: with power-of-two capacity, every masked index is `< capacity`, and cursor wrap preserves the intended slot relation. Reject this pattern for arbitrary capacities unless a fallback exists.

Kain examples: `map-power-two-window-index-bounds.smt2`, `actor-scheduler-ring-mask-index-bounds.smt2`.

## De Bruijn and low-bit decoding

Best for "find first set/free slot" in bitsets.

Pattern:

```c
isolated = word & (0 - word);
slot = table[(isolated * DEBRUIJN) >> shift];
```

Z3 claim: each possible low bit maps to a unique table index and the decoded slot is in range. If the occupancy word has fewer than 64 legal bits, prove the legal subset separately.

Kain examples: `actor-table-debruijn-hash-distinct.smt2`, `ownership-debruijn-low-bit-distinct.smt2`.

## Packed flags and selector equivalence

Best for UI flags, reflection kind tags, permission bits, and compact state machines.

1. Define the readable branchy selector first.
2. Define the packed or bitwise selector candidate.
3. Prove equivalence for every legal flag/tag state.
4. Ask Z3 for a counterexample over illegal/reserved states; decide whether to guard, mask, or document undefined input.

Kain examples: `native-ui-flag-selector-equivalence.smt2`, `native-ui-flag-update-equivalence.smt2`, `reflection-kind-token-states.smt2`.

## Search heuristics

- Prefer finite domains first; they are where wild constants are easiest to prove and easiest to keep honest.
- Constrain constants enough that generated code is cheap: odd multipliers, small table masks, rotate counts under word width, bounded probe counts.
- Optimize table size or probe count only after collision freedom exists.
- Keep a scalar baseline for readability and emergency fallback.
- Treat every `sat` witness as a hypothesis. Treat the final inverted `unsat` proof as the gate.
