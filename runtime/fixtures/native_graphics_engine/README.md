# Native Graphics Engine Fixture

This fixture proves the LLVM/direct-native stdlib can call the raw native graphics kernel without a runtime-authored scene, primitive catalog, or default geometry.

It creates two separate Kain-authored graphics sessions, registers SPIR-V shader modules, creates different vertex/index buffers, builds different mesh and pipeline handles, records draw commands, and presents both frames through the same generic runtime substrate.

The fixture intentionally does not ask Rust or C to make a triangle, cube, panel, or demo scene. Kain source provides the mesh identity, buffer payloads, counts, backend target, and frame submission flow.

Validate from the repo root:

```bash
./runtime/fixtures/validate_all.sh
```
