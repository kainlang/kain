# C Bridge

`std::c::bridge` is the first-party helper layer for the host-backed C ABI FFI in Kain.

It is meant for:

- consuming generated `use c::...` libraries
- inspecting shared buffer and shared image payloads passed into or out of native code
- keeping C-native payload work on the same `kain-interop` contracts used by Python and Node

The current runtime lane is host-backed only:

- `interpret`
- `test`

Core wrapper module:

- `std::c::bridge`

Current shared-contract helpers:

- `c_bridge_shared_buffer_info`
- `c_bridge_shared_buffer_bytes`
- `c_bridge_shared_buffer_from_bytes`
- `c_bridge_shared_image_info`
- `c_bridge_shared_image_bytes`
- `c_bridge_shared_image_from_bytes`
