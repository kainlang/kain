# Native UI Runtime Systems Fixture

This fixture proves that one Kain file can author a compact native UI system over the raw native UI ABI without a runtime-owned widget catalog.

It validates:

- stable keyed nodes for reload-friendly identity
- host frame presentation through `native_ui_host_present`
- generic font, texture, canvas, and shader resource handles
- text measurement metadata
- accessibility labels and roles
- clipboard, IME, drag/drop, menu, and dialog state
- draw rect, draw text, and draw resource submission

Use:

```bash
kain check runtime/fixtures/native_ui_runtime_systems/main.kn --target llvm
kain build runtime/fixtures/native_ui_runtime_systems/main.kn --target llvm --output generated/native_ui_runtime_systems.ll
```
