# Native Runtime Experimental Proofs

Reference-only SMT proofs for branchless native lookup and token-classifier
experiments.

These are kept out of the curated `proofs/` lanes while each strategy settles.
Run individual files with:

```powershell
Get-Content runtime\native\src\core\z3\proofs-experimental\<file>.smt2 -Raw |
  & C:\Users\Admin\.local\tools\z3-4.16.0\bin\z3.exe -smt2 -in
```

Current references:

- `map-magic-current-intent-pool.smt2`
- `map-eight-slot-selection.smt2`
- `map-power-two-window-index-bounds.smt2`
- `actor-scheduler-ring-mask-index-bounds.smt2`
- `actor-table-slot-composition-bounds.smt2`
- `actor-table-debruijn-hash-distinct.smt2`
- `actor-reply-port-copy-bounds.smt2`
- `actor-ask-live-snapshot-ref-match-equivalence.smt2`
- `service-registry-magic-collision-free.smt2`
- `service-alias-canonicalizer-token-states.smt2`
- `reflection-ui-token-magic-collision-free.smt2`
- `reflection-kind-token-states.smt2`
- `reflection-type-kind-selector-equivalence.smt2`
- `reflection-item-kind-selector-equivalence.smt2`
- `reflection-field-selector-equivalence.smt2`
- `native-ui-flag-selector-equivalence.smt2`
- `native-ui-flag-update-equivalence.smt2`
- `ownership-pointer-index-probe-bounds.smt2`
- `ownership-occupancy-slot-composition-bounds.smt2`
- `ownership-debruijn-low-bit-distinct.smt2`
