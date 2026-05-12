# Signal Workbench Smoke

This smoke proves Kain can drive Rust crate FFI and Node from the same `.kn` file.

Roles:

- Cargo FFI: generates signal bars, beacon points, and signatures
- Kain: composes the SVG workbench scene and runtime report
- Node: packages document/image payloads and writes the final web artifacts

Run:

```powershell
run_import_crate.bat
run_all.bat
run_test.bat
run_interpret.bat
```

Artifacts:

- `outputs/signal_workbench.html`
- `outputs/signal_workbench.svg`
- `outputs/signal_workbench_report.txt`
- `outputs/generated/cargo_node_weave.kn`
