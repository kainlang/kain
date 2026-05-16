# Native Standard Library

`stdlib/native` is the shared native profile loaded by the LLVM and direct C backends.

It intentionally stays inside the backend subset both emitters can lower today:

- primitive `Int`, `Bool`, `Float`, and `String` signatures
- explicit `@extern` declarations for the C runtime ABI
- small Kain wrappers for actor, entangle, patch, law, converge, orchestrate, diagnostics, result, collection, runtime, time, net, http, tls, http2, process, filesystem, input, graphics, and raw UI helpers; root `stdlib/net.kn`, `http.kn`, `tls.kn`, and `http2.kn` mirror the native networking domains for the public `std::*` import surface

LLVM and C avoid the generic root stdlib profile by default so native object files only pull ABI symbols that exist in `runtime/native`.
