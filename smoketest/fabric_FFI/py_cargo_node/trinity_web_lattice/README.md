# Trinity Web Lattice Smoke

This smoke proves Kain can orchestrate Python FFI, Rust crate FFI, and Node from one `.kn` file.

Roles:

- Python: generates lattice points and band amplitudes with NumPy
- Cargo FFI: generates structural spokes, markers, and signatures
- Kain: composes the SVG scene and runtime report
- Node: packages document/image payloads and writes the final web artifacts

Run:

```powershell
run_import_crate.bat
run_all.bat
run_test.bat
run_interpret.bat
```

Artifacts:

- `outputs/trinity_web_lattice.html`
- `outputs/trinity_web_lattice.svg`
- `outputs/trinity_web_lattice_report.txt`
- `outputs/generated/trinity_stack_node.kn`
