# SQLite Natural Include

This blade proves that Kain can import a real C amalgamation without a hand-written
`[c_ffi]` entry:

```kn
include sqlite3.h as sql
```

The C import lane resolves `sqlite3.h`, discovers the sibling `sqlite3.c`, emits
alias-aware externs such as `sql_libversion_number`, and links the C source into
the LLVM build.

