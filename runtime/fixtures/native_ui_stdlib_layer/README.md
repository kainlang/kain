# Native UI Stdlib Layer Fixture

This fixture proves the first Kain-authored stdlib UI layer above the raw native UI ABI.

It deliberately stays catalog-free. The fixture uses generic helpers from `stdlib/native/ui.kn` for:

- live/headless session setup
- stable keyed node reconciliation
- layout math
- style color, spacing, padding, and inherited color resolution
- text measurement
- generic state cells for arbitrary authored shape, hit, draw, and resource payloads
- render submission helpers
- generic pointer state flags driven by authored event handling

Validation:

```powershell
target\codex-native-ui-win32\debug\kain.exe check runtime\fixtures\native_ui_stdlib_layer\main.kn --target llvm
target\codex-native-ui-win32\debug\kain.exe build runtime\fixtures\native_ui_stdlib_layer\main.kn --target llvm --output target\codex-native-ui-stdlib-layer\native_ui_stdlib_layer.exe
target\codex-native-ui-stdlib-layer\native_ui_stdlib_layer.exe
```
