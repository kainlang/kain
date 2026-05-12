# Node Smokes

These smokes prove Kain can drive JavaScript and Node through `std::javascript::*` while keeping the authored pipeline in `.kn`.

Current examples:

- `orbit_portal`: local JS module import, Node built-in module import, Kain-authored SVG composition, and HTML artifact output
- `typescript_signal_forge`: local `.ts` helper import through `tsx`, typed-array buffer inspection, and HTML artifact output

Mixed-runtime examples now live beside this folder:

- `smoketest/py_node`: Python + Node
- `smoketest/cargo_node`: Cargo FFI + Node
- `smoketest/py_cargo_node`: Python + Cargo FFI + Node
