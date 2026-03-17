# Kain Shared Interop Contract

`std::interop::*` is the neutral host-payload contract for Kain.

Current surfaces:

- `interop_shared_buffer_*`: byte-addressable buffer contract
- `interop_shared_image_*`: image contract for raster and encoded payloads

Why it exists:

- Python and JavaScript/Node can materialize the same Kain-native shape
- Rust crate FFI can consume the exported metadata and byte arrays today
- future C and C++ bridges get one stable seam instead of separate per-runtime models

The contract is explicit about:

- contract version
- source runtime
- source backend
- ownership
- format and mime type
- image representation (`raster` vs `encoded`)
