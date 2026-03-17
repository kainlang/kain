# Orbit Portal Smoke

This smoke proves that Kain can drive JavaScript and Node from one `.kn` file:

- Kain imports a local `.mjs` helper module
- JavaScript computes orbital point data and wraps the HTML shell
- Node built-ins write the final web artifact
- Kain itself composes the SVG payload and runtime report

Primary wrapper surface:

- `use std::javascript::bridge`

Run:

```powershell
run_all.bat
run_test.bat
run_interpret.bat
```

Artifacts:

- `outputs/orbit_portal.html`
- `outputs/orbit_portal.svg`
- `outputs/orbit_portal_report.txt`

If you want TypeScript-module interop, set `[node_ffi].command = "tsx"` in `KAIN.toml` and point the import at a `.ts` helper.
