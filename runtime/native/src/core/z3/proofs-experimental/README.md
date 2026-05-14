# Native Map Experimental Proofs

Reference-only SMT proofs for the branchless native `KainMap` lookup rewrite in
`runtime/native/src/core/kain_runtime_core.c`.

These are kept out of the curated `proofs/` lanes while the map strategy settles.
Run individual files with:

```powershell
Get-Content runtime\native\src\core\z3\proofs-experimental\<file>.smt2 -Raw |
  & C:\Users\Admin\.local\tools\z3-4.16.0\bin\z3.exe -smt2 -in
```

