# Native Standard Library

`stdlib/native` is the shared native profile loaded by the LLVM and direct C backends.

It intentionally stays inside the backend subset both emitters can lower today:

- primitive `Int`, `Bool`, `Float`, and `String` signatures
- explicit `@extern` declarations for the C runtime ABI
- small Kain wrappers for actor, entangle, patch, law, converge, orchestrate, diagnostics, result, collection, runtime, CPU capability, converge autotune/telemetry, time, net, http, tls, http2, process, filesystem, input, graphics, and raw UI helpers; root `stdlib/*.kn` mirrors now exist for the native domains so authors can import `std::actor`, `std::fs`, `std::http`, `std::ui`, and the rest without spelling the `native` profile

LLVM and C avoid the generic root stdlib profile by default so native object files only pull ABI symbols that exist in `runtime/native`.
