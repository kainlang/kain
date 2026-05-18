# Kain Runtime Blades

This workspace is the first Kain-authored runtime lane over the native C ABI floor.
The goal is not to replace the C floor; it is to let Kain decide runtime policy,
batching, routing, ownership shape, actor turns, and service composition while C
continues to expose the metal.

## Performance Rule

The measured `use c::`/native boundary cost should be treated as a per-boundary
cost, not a tax on all Kain-authored runtime code.

- One tiny scalar C call from a hot loop pays the boundary every time.
- One Kain LLVM loop that calls C once for a raw OS/runtime primitive pays it once.
- One batched runtime pump with 64 operations turns a 9 ns boundary into about
  140 ps per operation before the real work is counted.

So the first law for this workspace is: no chatty ABI. Kain can author the runtime
as long as the boundary is coarse, batched, and proof-backed.

## Layout

- `runtime-core`: reusable Kain runtime policy module.
- `runtime-abi-probe`: native LLVM probe that boots the existing runtime and
  proves the first Kain-authored policy layer can sit above it.
- `config/runtime_abi_map.json`: data-driven first cut of the C floor modules
  that should become coarse `use c::` services.

## Next Moves

1. Promote runtime-owned header imports such as `use c::version` into the normal
   runtime authoring style so Kain files can reach `runtime/native/include`
   without blade-local `[c_ffi]` ceremony.
2. Move HTTP pump scheduling policy out of the benchmark case and into
   `runtime-core`.
3. Add Z3 cases for batch arithmetic: no request index escapes the batch span,
   no ring-buffer cursor wraps into live data, and boundary amortization math
   matches the selected batch size.
