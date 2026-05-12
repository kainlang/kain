# Python + Cargo + Node Mixed Smokes

These smokes prove Kain can orchestrate Python FFI, Rust crate FFI, and the JavaScript/Node bridge together from the same `.kn` file.

Current examples:

- `trinity_web_lattice`: Python generates lattice payload data, Rust crate FFI generates structural markers, Kain composes the SVG scene, and Node packages document/image payloads for web output
- `shared_prism_contract`: Python materializes a neutral shared image contract, Rust crate FFI consumes the shared bytes, Kain inspects the contract, and Node emits encoded image/html artifacts

Each smoke folder is self-contained and includes both launchers and the local Rust crate used for the live bridge.
