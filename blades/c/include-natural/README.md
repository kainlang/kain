# Natural C Include

This blade proves the compact C import surface:

```kn
include native/native_math.h as nm
```

The include is alias-aware in the AST/runtime contract. During C-FFI source
augmentation, Kain resolves the local header, finds the sibling `.c` source, and
emits `nm_*` extern aliases linked back to the real C symbols with `@link_name`.
