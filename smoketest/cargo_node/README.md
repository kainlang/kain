# Cargo + Node Mixed Smokes

These smokes prove Kain can run Rust crate FFI and the JavaScript/Node bridge from the same `.kn` file.

Current examples:

- `signal_workbench`: Rust crate FFI generates signal geometry and metadata, Kain composes the SVG scene, and Node packages document/image payloads for web output

Each smoke folder includes a local crate plus `run_import_crate.bat` so the generated Rust bindings stay visible and reusable.
