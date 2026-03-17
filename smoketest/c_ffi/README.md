# C ABI FFI Smokes

These smokes prove Kain can drive local C shared libraries through the host-backed `use c::...` lane while keeping the authored workflow in `.kn`.

Current examples:

- `beacon_math`: local header parsing, live shared-library loading, scalar/string calls, generated runtime outputs
