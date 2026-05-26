# Zero Cost Kain C Fusion

- Date: 2026-05-17
- Status: active
- Repo Root: `D:\Kain-Lang`
- Session Slug: `zero-cost-kain-c-fusion`

## Research Question

Can Kain make `use c::` behave like a zero-thickness substrate for Vulkan/runtime-class APIs, beating Zig-style C interop by erasing or fusing the boundary rather than merely making dynamic FFI fast?

## Constraints

- Latency: hot scalar/handle calls must not pay a dynamic bridge/trampoline cost.
- Throughput: Vulkan-style command emission must batch or fuse thousands of tiny host calls.
- Platform: Windows/native LLVM first; must keep normal C ABI linking available.
- Safety: raw pointers/handles require explicit ownership/layout contracts, preferably solver-backed.
- Implementation freedom: Kain can add new import modes, LLVM attributes, generated C shims, bitcode lanes, and runtime blade policy.
- Acceptable weirdness: compiler-owned C import metadata, inline header lowering, command-buffer DSLs, and proof-backed unsafe paths are acceptable.

## Hypothesis Lattice

### Baseline
- Mechanism: keep current `use c::` model: generated Kain `@extern` declarations plus declared shared/object libraries for LLVM linking.
- Expected upside: simple, works with existing manifests, already close to Zig for direct calls.
- Likely blocker: every hot call remains a real ABI call unless LLVM/LTO can see and inline the body.
- Proof obligation: ABI declarations match C signatures and linked library/object is present.

### Unconventional
- Mechanism: add a `c_inline` / `c_bitcode` import mode that compiles C sources to LLVM bitcode in the same module/LTO unit, attaches attributes, and lets Kain calls inline like native functions.
- Expected upside: source-level `use c::foo` can compile to zero call-boundary cost when body/layout is visible and legal to inline.
- Likely blocker: macros, platform headers, varargs, callbacks, and function pointers need import classification rather than naive header parsing.
- Proof obligation: imported layout/calling convention equivalence, no escaping pointer lifetime violation, and generated IR remains semantically equivalent to the C source under the selected target.

### Moonshot
- Mechanism: C is not called; C headers become a Kain-owned ABI schema plus a command-fusion DSL. For Vulkan, Kain records typed commands into a verified arena/ring and flushes through a small number of C floor calls.
- Expected upside: beats per-call Zig interop because Kain turns thousands of apparent C calls into a fused memory protocol and one or few ABI crossings.
- Likely blocker: only works for APIs where calls can be reordered/recorded or represented as data; immediate-return query APIs still need direct calls or cached capability tables.
- Proof obligation: command encoding round-trips, arena bounds hold, handle ownership state is legal, and the fused flush is equivalent to the visible call sequence.

## Mathematical Model

- Variables:
  - `kind`: 0 dynamic bridge/trampoline, 1 static external call, 2 same-module IR/LTO inline, 3 compiler intrinsic/builtin.
  - `boundary_cost`: call-boundary cost in ns.
  - `batch`: operations per C boundary.
  - `amortized_ps = boundary_cost * 1000 / batch`.
- Invariants:
  - Dynamic unresolved FFI has positive boundary cost.
  - Same-module/intrinsic imports can erase the call boundary if the body is visible or compiler-owned.
  - Batched calls reduce effective overhead but do not make the boundary physically zero.
- Objective: minimize hot-path boundary cost, then minimize memory traffic and synchronization around the C floor.
- Bad states:
  - A hot Vulkan/runtime path emits one ABI call per tiny operation.
  - Imported pointer/handle layouts mismatch C.
  - Kain reorders a C call sequence with externally visible side effects.
- Simplifying assumptions: 9 ns current dynamic/direct bridge measurement is the boundary baseline.

## Z3 Claims

1. Dynamic unresolved FFI cannot be zero-cost under the boundary model.
   - Report: `z3/reports/20260518T011337Z-zero_cost_c_boundary_kind_model.json`
   - Result: `unsat`
2. Same-module IR/LTO or compiler-intrinsic import can have zero call-boundary cost under the same model.
   - Report: `z3/reports/20260518T011337Z-zero_cost_c_boundary_kind_model.json`
   - Result: `unsat` for the violation checks.
3. If the existing 9 ns boundary is only amortized, the minimum batch for <= 1 ps/op effective boundary cost is 4501.
   - Report: `z3/reports/20260518T011330Z-zero_cost_c_min_batch_for_sub_ps.json`
   - Result: optimal model `batch = 4501`.

## Evidence And Sources

- Local:
  - `crates/c-ffi/src/generate.rs` emits generated `mod c` modules with `@extern fn` declarations and a separate Rust bridge for interpret/test lanes.
  - `crates/c-ffi/src/lib.rs` detects `use c::...` imports and augments source for runtime targets.
  - `crates/cli/src/main.rs::resolve_c_ffi_shared_libraries_for_linking` currently requires an active C import to declare an existing `shared_lib` for LLVM linking.
  - `crates/build/src/workspace.rs::add_c_tasks` can already build C shared libraries from blade manifests; this is the natural hook for a future bitcode/static-object fusion task.
- External:
  - None yet.

## Dead Ends

- "Make dynamic FFI literally free" is physically blocked. The boundary can be optimized, amortized, or hidden by CPU prediction, but a real unresolved ABI call still has positive cost.
- "Batching alone makes it zero" is only effective-zero at very large batches. With a 9 ns baseline, sub-1-ps amortized overhead needs batch 4501.

## Conclusion

Active thesis: Kain should not merely copy Zig's C interop. The winning architecture is a tiered C import optimizer:

1. `c_dynamic`: current flexible shared-library lane.
2. `c_static`: link object/static library with direct external calls and aggressive attributes.
3. `c_bitcode` / `c_inline`: compile C sources into the same LLVM/LTO unit so Kain can inline and erase the call boundary.
4. `c_fused`: for APIs like Vulkan, compile apparent C calls into a Kain-owned typed command protocol and flush in batches through the C floor.

The moonshot is plausible because Vulkan is already command-buffer shaped. Kain can make the authoring surface look like direct C while the compiler/runtime turns it into fused arena writes plus a handful of native calls.

## 2026-05-18 - File-Local Header Ingestion

User clarified the Zig comparison: the killer property is not only low call cost, but being able to use C headers from any source file without writing a bridge or manifest entry.

Current Kain reality:

- `use c::foo` can generate bindings, but resolution expects `[c_ffi]` / `[[c_ffi.libraries]]` metadata with a named header.
- LLVM linking currently expects the active C import to declare an existing `shared_lib`/object path.
- This means Kain has a bridge/config ceremony that Zig mostly hides behind `@cImport`.

New thesis:

- Add file-local C import syntax that creates an ephemeral c-ffi library config directly from the Kain source.
- The existing `[c_ffi]` manifest stays as an override/cache/policy surface, not a required bridge.
- The generated Rust bridge remains useful for interpreter/test/plugin lanes, but native LLVM should prefer static object, bitcode, or inline import lanes so no hot dynamic bridge exists.

Candidate syntax:

```kn
use c::header("nuklear.h") as nk

use c::header("nuklear.h") as nk with:
    include "vendor/nuklear"
    define "NK_INCLUDE_FIXED_TYPES"
    define "NK_INCLUDE_DEFAULT_ALLOCATOR"
    implementation define "NK_IMPLEMENTATION"
    mode auto
```

Lowering:

1. Parser records a `CHeaderImport` item before normal module resolution.
2. `kain-c-ffi` resolves header/include/defines from the source file directory plus global defaults.
3. It generates the same canonical `mod c::nk` declarations into `.kain/cache/c_ffi/<hash>/`.
4. If implementation is requested, it generates a tiny C translation unit such as:
   `#define NK_IMPLEMENTATION` then `#include "nuklear.h"`.
5. Build picks the strongest legal lane:
   - `inline`: static inline/header body imported into Kain IR.
   - `bitcode`: C TU compiled to LLVM bitcode and linked in the same optimization unit.
   - `static`: C TU compiled to object/static library.
   - `dynamic`: fallback shared-library bridge.

For `nuklear.h`, "do nothing" can mean:

- If using declarations only: source-local `use c::header("nuklear.h") as nk` is enough when the header is discoverable.
- If using Nuklear's single-header implementation: Kain should auto-own the one implementation TU from the source-local import, so the user does not write `nuklear_bridge.c`.

Proof obligations:

- Header import hash includes header bytes, transitive includes, defines, target triple, clang version, and selected mode.
- Exactly one implementation TU is materialized per import hash when single-header implementation is requested.
- Generated Kain declarations match C ABI layout/calling convention.
- Inline/bitcode/static/dynamic lanes expose the same callable symbols.

This is the direct answer to "nuklear.h in a Kain file and do absolutely nothing": make C headers first-class Kain imports, with bridge generation hidden and native lanes avoiding the bridge entirely when possible.
