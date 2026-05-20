# tiny_math platform SDK fixture

This fixture is the smallest platform-package proof surface. Tests can compile
`src/tiny_math.c` into the host dynamic-library format, then run
`kain import platform` against this SDK-shaped folder to prove deterministic
scan, lock, typed thunk generation, and runtime open/resolve/close behavior
without depending on a local Vulkan SDK.

The header deliberately contains a small negative surface too: a callback
typedef, an opaque handle, and a by-value aggregate return. Platform-package
locks must preserve stable blocked-symbol reasons for those shapes instead of
pretending every C declaration is safely callable in v1.
