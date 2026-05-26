# platform-package-smoke

Tiny proof blade for the platform-package lane before Vulkan/Kloner build on it.

`run.ps1` stages the `tiny_math` SDK fixture, imports it twice, byte-compares
the lock/report, checks relocatable path rendering, verifies stable negative
surface reasons, and then runs the Kain std::platform open/resolve/close smoke.

The callable typed function-pointer proof lives in the native runtime test
`//runtime:native_test_platform_library`; the public v1 surface remains
open/resolve/close plus generated typed package thunks, with no `call_typed`
API.
