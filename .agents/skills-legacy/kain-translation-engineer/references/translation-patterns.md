# Translation Patterns

Use these crosswalks after inventorying the donor code. They are not syntax recipes; they are prompts for choosing the Kain semantic owner.

## Rust To Kain

| Rust shape | Kain move |
| --- | --- |
| `Arc<Mutex<T>>`, atomics, shared mutable state | Prefer `actor`/`ask`/`send`, `world`/`entangle`, or a bounded `collapse` ownership scope. Only use raw pointer mutation when the layout is the point. |
| `Result<T, E>`, `Option<T>` | Use Kain `Result`/`Option` and `law` checks at boundaries. Keep rich error semantics instead of flattening to status strings. |
| `async`, Tokio tasks, channels | Use Kain actors, `Future`, `await`, `pulse`, or a runtime/std actor facade depending on whether this is semantic scheduling or OS IO. |
| `serde_json`, hand parsers, schema-heavy config | Use `std::text` and explicit structs/laws; for fixed schemas consider a `converge` reference parser plus target-gated fast lane. |
| `std::fs`, `PathBuf`, temp files | Use `std::fs` root imports and current map symbols. Keep path rules data-driven and prove path/span bounds when needed. |
| crates/workspace modules | Use `kain import crates` only as a sketch. Promote useful shapes into Kain modules or blades with Kain naming and stdlib imports. |
| `unsafe`, slices, pointer arithmetic | Use `ptr<T>`, `alloc_zeroed`, `ptr_offset`, `mem_load`, `mem_store`, `collapse`, `observe`, and `decay`; add Z3 bounds/equivalence proof. |

## C++ To Kain

| C++ shape | Kain move |
| --- | --- |
| RAII resource wrappers | Model ownership directly with `collapse`/`observe`/`decay`, `std::alloc`, or a Kain world/actor lifetime. Use native C substrate only for OS handles. |
| templates/policy classes | Prefer `trait`, `comptime`, data-driven manifests, or a `converge` selector. Do not clone template ceremony unless it is the semantic contract. |
| virtual dispatch | Use explicit enums/match, trait methods, actors, or a table/selector. Benchmark before adding indirection. |
| SIMD/intrinsics | Keep a Kain scalar spec and call a target-gated native lane through `converge`; prove the lane equivalent before claiming parity. |
| packed structs/byte protocols | Use Kain packed integer math and pointer loads/stores with proof cases for field round-trip and span safety. |
| STL containers | Check `std::collections` first. If a missing container matters, add it to the stdlib or native runtime rather than hiding it in one translated file. |

## TypeScript To Kain

| TypeScript shape | Kain move |
| --- | --- |
| `Promise`, async callbacks, event emitters | Use `Future`, actors, `pulse`, or an explicit Kain message contract. Do not preserve callback pyramids. |
| structural interfaces and DTOs | Use Kain `struct`, `enum`, `Result`, and `law` validation. Convert `any` to explicit boundary types. |
| Express/MCP routers/tool registries | Use data-driven tool catalogs plus Kain actors or blade services. Keep tool metadata out of hardcoded branch ladders. |
| Node `fs`, `child_process`, `http`, `tls` | Use `std::fs`, `std::process`, `std::net`, `std::http`, `std::tls`, and `std::http2` from `stdlib/STDLIB_MAP.llm.md`. |
| npm library dependency | Decide whether it is semantic logic, OS substrate, or ecosystem glue. Rewrite semantic logic in Kain; bridge substrate through runtime/FFI. |
| TS import output | Use `kain import-ts` as an x-ray of the source, then rewrite into Kain-owned modules. Do not ship generated TypeScript-shaped Kain as the final form. |

## MCP/Tool Server Translation

Start from the public tool contract: tool name, input schema, output schema, side effects, resource handles, caching, telemetry, cancellation, and error shape.

Then place each tool into a Kain owner:

- Tool catalog and runtime policy: JSON/config under the blade, loaded by Kain.
- Request routing: actor or command-service module.
- File operations: `std::fs`, with explicit path laws and sandbox policy.
- Process execution: `std::process`, with stdio mode and timeout policy in data.
- Network fetches: `std::net`/`std::http`/`std::tls`/`std::http2`.
- Search/index/query: Kain spec first; native index or AST substrate behind an isolated ABI lane if the stdlib cannot express it yet.
- Telemetry: diagnostics/status counters, not ad hoc global strings.

For `mcp/reference`, inventory each Rust file as a donor capability, then compare to `blades/kain-mcp/src/*.kn` and `blades/kain-mcp/config/*.json` before authoring new modules.

For net-heavy MCP tools, read `blades/network-domains/src/main.kn` first. It is the compact current proof for localhost servers, route-to-actor, request parsing, response writes, raw TCP client IO, TLS request setup, and HTTP/2 request setup.

## Validation Pattern

1. Create or update the smallest Kain proof blade/module first.
2. Compile/check the Kain file on the intended target, usually LLVM for native runtime work.
3. Run donor-oracle comparisons where possible.
4. Add Z3 for pointer span, buffer growth, packed layout, selector equivalence, state-machine, or arithmetic claims.
5. Add a benchmark row when performance is part of the reason for translation.
6. Add attrition only when resource cleanup, sabotage, or long-run runtime stability is part of the claim.
