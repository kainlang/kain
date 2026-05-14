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
- `service-registry-magic-collision-free.smt2`
- `service-alias-canonicalizer-token-states.smt2`
- `reflection-ui-token-magic-collision-free.smt2`
- `reflection-kind-token-states.smt2`
