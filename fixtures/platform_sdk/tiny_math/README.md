# tiny_math platform SDK fixture

This fixture is the smallest platform-package proof surface. Tests can compile
`src/tiny_math.c` into the host dynamic-library format, then run
`kain import platform` against this SDK-shaped folder to prove deterministic
scan, lock, typed thunk generation, and runtime open/resolve/close behavior
without depending on a local Vulkan SDK.
