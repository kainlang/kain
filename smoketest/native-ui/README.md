# Native UI Smoketests

Focused smokes for Kain-authored native UI systems.

- `pilot/` is the first raw LLVM executable smoke. It compiles one Kain file against `stdlib/native/ui.kn`, links the native runtime, runs the resulting `.exe`, and verifies native UI ABI calls in the generated LLVM IR.
- `episode-two/` is the aggressive mixed smoke. It keeps one authored Kain file crossing live raw UI, generic shape/hit/draw/resource state payloads, native input, native graphics metadata, actors, and entangle/intent hooks, then captures a Win32/GL screenshot.

Do not wire these smokes through the older `/smoketest` delegate pipelines.
