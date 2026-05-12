# TypeScript Signal Forge Smoke

This smoke proves the Node bridge can run through a TypeScript-aware runtime and import a local `.ts` helper directly.

It also proves the typed JS buffer lane:

- TypeScript returns a `Uint8Array`
- Kain inspects it through `js_web_buffer_info`
- Kain snapshots the bytes through `js_web_buffer_bytes`

Run:

```powershell
run_all.bat
run_test.bat
run_interpret.bat
```

Artifacts:

- `outputs/typescript_signal_forge.html`
- `outputs/typescript_signal_forge.svg`
- `outputs/typescript_signal_report.txt`
