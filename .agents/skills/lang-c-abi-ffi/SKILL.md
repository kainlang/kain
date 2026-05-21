---
name: lang-c-abi-ffi
description: Use when authoring Kain code that crosses the C ABI, including automatic runtime-owned `use c::...` imports, optional explicit bridge metadata for inline/object/shared/package bridges, and app-level FFI call shape without taking ownership of compiler or runtime ABI internals.
---

# Lang C ABI FFI

Use this skill when the task is about how to *use* C from Kain.

For full native/foreign boundary design, prefer `lang-interop`. This card stays
as the narrow C ABI usage lane for tasks that specifically ask for `use c::...`
or `[c_ffi]` details.

## Default Rule

- Start with plain `use c::...`.
- Runtime-owned headers can resolve automatically without blade-local manifest ceremony.
- Reach for explicit bridge metadata only when the import is not runtime-owned and you need to declare source/object/shared/package bridge details intentionally.

## Fast Loop

```powershell
rg -n "use c::|\[c_ffi\]|shared_lib|tier =" blades benchmark smoketest runtime
kain import-c <header-or-c-file> -I <include-dir> -D <DEFINE=1> -o <generated.kn>
kain check <entry.kn> --target llvm
kain build <entry.kn> --target llvm -o <output.exe>
```

## Automatic Runtime-Owned Pattern

```kn
use c::version

fn main() -> Int:
    if version_check_abi_compatibility(256):
        return 0
    return 1
```

## Explicit Bridge Pattern

Use this only when the bridge is blade-owned, package-owned, or otherwise not covered by the runtime-owned automatic lane.

Inline/source-backed bridge:

```toml
[c_ffi]

[[c_ffi.libraries]]
name = "beacon_math"
tier = "inline"
header = "native/beacon_math.h"
sources = ["native/beacon_math.c"]
include_paths = ["native"]
defines = ["_CRT_SECURE_NO_WARNINGS"]
link_libs = ["user32"]
```

Prebuilt object/shared-library bridge:

```toml
[c_ffi]

[[c_ffi.libraries]]
name = "ffi_boundary_shared"
header = "native/ffi_boundary.h"
shared_lib = ".kain/native/ffi_boundary_shared.dll"
```

## How To Use The Lane

- Try automatic `use c::...` first for runtime-owned headers.
- Use `tier = "inline"` when the blade or package owns C source and you want that source compiled into the native link.
- Use `shared_lib = "..."` when you already have a built `.obj`, `.dll`, or platform dynlib and Kain should consume that artifact directly.
- If the task explicitly calls for `bitcode`, `static`, `dynamic`, or runtime-owned `fused`, declare that tier intentionally instead of pretending every bridge is the same lane.
- Keep native sources, headers, and built bridge artifacts local to the blade or package that owns them.
- Use `kain import-c` as a fast preflight when you want to inspect header shape before hand-authoring the final Kain surface.

## Rules

- Prefer scalar values, handles, counters, and explicit status returns across the boundary.
- Do not make Kain depend on C-owned string lifetime unless ownership is explicit. Kain should own Kain strings.
- Keep the Kain-facing API small and idiomatic even if the C side is ugly.
- If a package already owns the bridge, consume the package surface instead of cloning the header and metadata locally.

## Hand Off When

- Use `lang-interop` when the task includes OS contracts, vendor SDKs, dynamic libraries, handles, callbacks, shared buffers/images, package bridge design, or broader "why/how should Kain cross this boundary?" reasoning.
- Use `bootstrap-core` when import resolution, pointer semantics, header modeling, or lowering truth is wrong.
- Use `runtime-core` when the native bridge implementation, runtime ABI glue, or shared-library loading path is wrong.
- Co-trigger `lang-gpu`, `runtime-gpu`, and `package-vulkain` when the bridge is graphics or compute facing.
