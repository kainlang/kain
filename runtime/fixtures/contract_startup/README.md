# Contract Startup Smoke Fixture

**Purpose:** Validates minimal runtime contract loading and validation

**Requirements:** 4.2, 8.1, 13.4, 13.5

---

## What This Tests

This fixture validates the most basic native runtime startup path:

1. **Contract JSON parsing** - Can the runtime parse `kain_runtime_contract.json`?
2. **Schema version validation** - Does the runtime accept schema version 1?
3. **Required capabilities validation** - Can the runtime resolve required capabilities?
4. **Service bindings resolution** - Can the runtime resolve service bindings?
5. **Startup diagnostics** - Does the runtime emit structured diagnostics on success?

---

## Artifacts

- `main.kn` - Minimal Kain program with no runtime features
- `kain_runtime_contract.json` - Minimal contract bundle
- `README.md` - This file

---

## Usage

### Compile

```bash
cd runtime/fixtures/contract_startup
kain build main.kn --target rust
```

### Expected Behavior

- Compilation succeeds
- Runtime contract is loaded successfully
- No startup errors or warnings
- Program prints "Contract startup smoke: OK"

### Failure Modes

If this fixture fails, it indicates:

- Runtime contract JSON parser is broken
- Schema version validation is broken
- Required capabilities resolution is broken
- Service bindings resolution is broken
- Startup diagnostics are broken

---

## Extension Points

Later phases may extend this fixture with:

- **Phase 1:** ABI version metadata, service table validation
- **Phase 2:** Structured diagnostics validation
- **Phase 3:** Reflection payload loading
- **Phase 10:** Compatibility metadata validation

When extending, preserve the minimal baseline and document changes here.

---

## Notes

- This is the simplest possible contract startup path
- No actors, no UI, no async, no graphics
- Focus is purely on contract loading and validation
- Should work on all platforms (no platform-specific services)
