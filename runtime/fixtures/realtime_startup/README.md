# Realtime Bundle Startup Smoke Fixture

**Purpose:** Validates realtime bundle ingestion and scene metadata loading

**Requirements:** 4.2, 8.1, 13.4, 13.5

---

## What This Tests

This fixture validates the realtime bundle startup path:

1. **Realtime bundle JSON parsing** - Can the runtime parse `kain_realtime_app_bundle.json`?
2. **Scene metadata validation** - Can the runtime validate scene structure?
3. **Asset reference resolution** - Can the runtime handle empty asset lists?
4. **Shader bundle reference resolution** - Can the runtime handle empty shader refs?
5. **Material reference resolution** - Can the runtime handle empty material lists?
6. **Startup diagnostics** - Does the runtime emit diagnostics on success?

---

## Artifacts

- `main.kn` - Minimal program with scene reference
- `kain_runtime_contract.json` - Contract with realtime requirements
- `kain_realtime_app_bundle.json` - Minimal realtime bundle
- `README.md` - This file

---

## Usage

### Compile

```bash
cd runtime/fixtures/realtime_startup
kain build main.kn --target rust
```

### Expected Behavior

- Compilation succeeds
- Runtime contract is loaded successfully
- Realtime bundle is loaded successfully
- Scene metadata is validated
- No startup errors or warnings
- Program prints "Realtime startup smoke: OK"

### Failure Modes

If this fixture fails, it indicates:

- Realtime bundle JSON parser is broken
- Scene metadata validation is broken
- Asset/shader/material reference resolution is broken
- Realtime runtime service binding is broken

---

## Extension Points

Later phases may extend this fixture with:

- **Phase 3:** Reflection-driven scene metadata
- **Phase 9:** Actual shader/material artifacts
- **Phase 10:** Compatibility metadata for hot reload

When extending, preserve the minimal baseline and document changes here.

---

## Notes

- Scene has no actual geometry, materials, or shaders
- Focus is purely on bundle loading and metadata validation
- Should work on all platforms (no platform-specific services)
- This fixture tests `runtime/native/src/core/kain_runtime_realtime.c`
