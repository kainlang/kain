# Kain Root Stdlib Requirements

This file is the authoritative backlog and delivery contract for finishing the root `std::*` surface.

It exists so future work does not collapse into vague parity anxiety, donor-language cargo culting, or random one-off helpers that never become a coherent standard library. If a capability matters for authored Kain at the root `std::*` level, it should be tracked here until it is honestly finished.

This backlog is derived from:

- the live root atlas in `stdlib/STDLIB_MAP.llm.md`
- the current top-level `stdlib/*.kn` surface
- the local donor baseline in `reference/langs/zig/lib/std`
- secondary systems-language expectations from Rust, Go, and OTP-style runtime ergonomics, translated into Kain-shaped public surfaces instead of donor-file cosplay

Current reality:

- Kain root stdlib atlas currently reports `61` modules, `2619` public symbols, and `3307` total symbols.
- The root profile is still much thinner than Zig's local `lib/std` capability envelope.
- Kain is already unusually strong in `std::math`, `std::gpu`, `std::graphics`, `std::ui`, `std::build`, `std::bench`, `std::attrition`, `std::certify`, `std::actor`, and semantic/runtime-facing surfaces such as `std::intent` and `std::runtime`.
- Kain is still obviously missing a large amount of boring-but-necessary authoring, systems, container, path, stream, encoding, archive, and binary-introspection depth.

This document tracks capability parity, not donor file-count parity. We do not need a one-file clone of Zig. We do need a root stdlib that does not feel visibly unfinished.

## Scope

- This document governs the root native profile only: top-level `stdlib/*.kn`.
- Overlay trees, blade-local helpers, and ad hoc package shims do not satisfy missing root requirements by themselves.
- If a capability belongs in authored Kain everywhere, it needs a root `std::*` story here.
- If a missing capability is blocked by compiler, typechecker, lowering, or runtime truth, fix the owning subsystem instead of encoding a permanent workaround in demos.

## Status Legend

- `TODO` = missing or not started
- `PARTIAL` = some surface exists, but it is too thin, too stringly typed, too `Any`-shaped, too donor-incomplete, or not proven enough for v1
- `BLOCKED` = work is understood but cannot complete until an owning compiler/runtime dependency lands
- `DONE` = public surface, implementation, smoketest integration, and required evidence are all complete
- `WAIVED` = only allowed with an explicit reason, date, and replacement plan; do not use this casually

## Operating Rules

1. Treat this file as the live root-stdlib source of truth.
2. Do not silently delete incomplete rows. If scope changes, split or rename the row and keep the history legible.
3. When you finish a requirement, update its `Status` to `DONE` in this file during the same change.
4. If you partially finish something, leave it `PARTIAL` and tighten the requirement text so the remaining gap is obvious.
5. If a row is blocked on compiler/runtime work, mark it `BLOCKED` and name the owning surface in the row notes or commit message.
6. Do not count overlay-only, blade-only, or ad hoc helper code as stdlib completion.
7. Do not hand-edit `stdlib/STDLIB_MAP.llm.md` or `stdlib/stdlib.map.json`.
8. Prefer typed structs, enums, views, iterators, and builders over text blobs and `Any` handles when a capability is meant to be a reusable public API.
9. Prefer Kain-shaped semantic leverage over donor mimicry. If Zig has three modules and Kain can honestly subsume them into one stronger surface, do that, but update this backlog accordingly.
10. Keep the public root import obvious. If a capability becomes large enough to deserve its own root module, do not bury it forever inside an unrelated module.
11. Ensure before writing you are well versed in Kain and read the $lang-skills including $lang-semantics, $lang-systems, $lang-stdlib, and other applicable skills ($lang-gpu, $lang-projects, and $lang-interop etc)

## Definition Of Done

A stdlib requirement is not complete until all applicable items below are true:

1. The public root `std::*` API exists in `stdlib/*.kn`.
2. Any required compiler, portable-crate, or `runtime/native` backing work exists and is wired end-to-end.
3. The generated atlas is refreshed:
   - `cargo run -q -p kain-stdlib-map --bin kain_stdlib_map_tool -- --write`
   - `cargo run -q -p kain-stdlib-map --bin kain_stdlib_map_tool -- --check`
4. The new surface is meshed into `smoketest/`:
   - add or extend `smoketest/src/stdlib/*_lane.kn`
   - import and call the lane from `smoketest/src/main.kn`
   - add any new files to `smoketest/build.kn`
   - bump `total_tracks` in `smoketest/src/main.kn` if a new track is added
   - when practical, make at least one non-stdlib track or blade consume the new surface so it is not an isolated vanity test
5. If the capability is runtime-backed, add or extend the relevant conformance lane under `runtime/conformance/`.
6. If the capability is performance-sensitive, branchless, allocator-heavy, or claims to beat conventional designs, add or extend a benchmark lane under `benchmark/`.
7. If the capability owns resources, sessions, handles, or teardown-sensitive state, add or extend an attrition lane under `attrition/`.
8. If the capability relies on unsafe math, pointer/index arithmetic, packed bit layouts, or branchless invariants, add or extend a Z3 proof case and leave a code comment pointing at the proof path.
9. Update `MEMORY.md` when the change materially alters how future agents should understand stdlib completion, caveats, or evidence.

## Delivery Rules For New Families

- Use dedicated root modules when a family is broad and obvious to authors, for example `std::json`, `std::fmt`, `std::io`, or `std::random`.
- Expanding an existing module is fine when the capability is naturally local, for example `std::fs` gaining typed metadata or `std::process` gaining richer status objects.
- Do not keep growing stringly-typed shims forever. `fs_metadata_text`, semantic string counters, and similar transitional shapes should graduate to typed public records.
- Do not hide major missing families behind internal helpers or blade-local code. If the family matters generally, promote it.
- A donor capability is considered covered only if authored Kain can use it through a sane public surface, not merely because some Rust crate or runtime helper exists somewhere in the repo.

## Validation Surfaces

Prefer extending existing proof surfaces when possible:

- `blades/stdlib-foundations`
- `blades/stdlib-domains`
- `blades/network-domains`
- `blades/hash-domains`
- `blades/math-domains`
- `smoketest/`
- `runtime/conformance/native_stdlib_bridge`
- `runtime/conformance/net_runtime`
- `runtime/conformance/process_runtime`
- `runtime/conformance/input_runtime`
- `runtime/conformance/ui_runtime`
- `runtime/conformance/graphics_runtime`
- `runtime/conformance/platform_parity`

If none of the above is the right owner, add the smallest honest new blade, conformance lane, benchmark case, attrition case, or proof pack necessary.

## Priority Model

- `P0` = v1 root-stdlib completeness blockers; these should not be casually deferred
- `P1` = important systems-completeness work that should follow immediately after `P0` if not landed inside the same arc
- `P2` = deeper parity and advanced tooling families that still belong here, but can land after the root v1 floor is real
- `KX` = Kain-only leverage; these are not donor parity chores, they are the semantic surfaces we should be better at than everyone else

## Current Strong Surfaces

These are not "finished forever," but they are not the obvious parity holes right now:

- `std::math`
- `std::gpu`
- `std::graphics`
- `std::graphics::shared`
- `std::ui`
- `std::build`
- `std::bench`
- `std::attrition`
- `std::certify`

Do not churn these just to imitate Zig structure. Expand them only when a concrete missing capability demands it.

## P0 - Root Authoring And Systems Foundations

Every row below is a v1-facing gap compared to a serious systems language stdlib.

| Status | Priority | Surface | Requirement |
| --- | --- | --- | --- |
| `DONE` | `P0` | `std::text` plus new `std::bytes` | The authoring floor now covers zero-copy text views, zero-copy byte views, explicit text-byte conversion, basic escape/unescape, byte builders, text-builder interop, and owned byte/text materialization instead of forcing every caller into ad hoc string-as-bytes glue. |
| `DONE` | `P0` | new `std::ascii` or `std::text` subfamily | Expose ASCII classification, case fold, digit/hex helpers, whitespace checks, and predictable byte-level transforms instead of forcing every caller to re-encode them manually. |
| `DONE` | `P0` | new `std::unicode` | Add UTF-8 validation, codepoint iteration, encoding/decoding helpers, normalization hooks, case mapping where practical, and explicit invalid-sequence behavior. |
| `DONE` | `P0` | new `std::base64` plus binary encodings | Base64, URL-safe base64, base16/hex encode-decode, and related helpers now exist as a root family instead of being scattered across unrelated domains. |
| `DONE` | `P0` | new `std::fmt` | Root formatting now ships spec-driven string, number, and bool rendering plus `FmtWriter` and `std::io::StringBuilder` integration for buffered accumulation and stream-backed authoring sinks instead of staying a string-only helper shelf. |
| `DONE` | `P0` | new `std::json` | Root JSON now ships typed field and array result structs, structured scan/status reporting, richer object/array encode helpers, writer/builder integration, and a repaired LLVM/native bridge that preserves string, float, bool, object, and array value lanes through JSON-aware `Any` lowering and owned-return transfer fixes verified by focused runtime lanes and Z3 proofs. |
| `DONE` | `P0` | new `std::uri` | Add URI/URL parsing, normalization, authority/path/query helpers, percent encoding/decoding, and typed integration with HTTP/TLS surfaces. |
| `DONE` | `P0` | new `std::semver` | Add semantic version parse/format/compare and version-range matching so package/build/runtime metadata is not forced into raw strings. |
| `DONE` | `P0` | `std::collections` generic container floor | Done with `IntQueue`/`Deque`/`PriorityQueue`, `SlotMap`, and zero-allocation `IntrusiveHashMap` (`uthash` evolution), now complete with standard `ArrayList` and `HashMap` structures. |
| `DONE` | `P0` | `std::collections` algorithms and data-structure depth | Add generic `SlotMap<T>`, bitset/bitmap, static string map or trie-like lookup, sort/search/select/dedup helpers, and data-oriented container shapes such as multi-array or SoA storage where justified. |
| `DONE` | `P0` | `std::alloc` | Keep bump/arena/pool, but add allocator traits/interfaces, growable buffer support, allocation result structs, span helpers, and integration points the generic containers can actually use. |
| `DONE` | `P0` | `std::fs` plus likely new `std::path` | Expand from `join` plus text/hex wrappers into a full path toolkit: parent, file name, stem, extension, normalize, canonicalize, relative path, absolute path, split, and platform-safe separators. |
| `DONE` | `P0` | `std::fs` typed metadata and directory surfaces | Replace text-shaped metadata and path listings with typed metadata, typed directory entries, structured error/status, richer temp/workspace helpers, and honest file/dir kind reporting. |
| `DONE` | `P0` | `std::fs` file handles | Add open/read/write/append/seek/flush/close file handles and explicit binary/text read models. Root stdlib should not stay permanently path-only. |
| `DONE` | `P0` | new `std::io` | Add reader/writer/seek/stream abstractions, buffered I/O, in-memory streams, adapters between file/process/net/http bodies, and a sane common I/O vocabulary. |
| `DONE` | `P0` | `std::time` | Grow beyond millis/sleep/deadline. Add `Duration`, monotonic `Instant`, wall-clock time, conversion helpers, deadlines/timeouts, interval helpers, and timing primitives that benchmarks and async code can share. |
| `DONE` | `P0` | new `std::random` | Add deterministic and nondeterministic RNG families, seeded generators, split/fork helpers, fast non-crypto RNG for benchmarks/simulations, and public binary output APIs. |
| `DONE` | `P0` | new `std::atomic` | Add public atomics, ordering vocabulary, typed atomic cells, compare/exchange, fetch ops, and memory-order helpers so low-level authored Kain stops reinventing them piecemeal. |
| `DONE` | `P0` | new `std::sync` | Root sync now ships a real Kain-native floor with an intrusive MCS mutex, padded SPSC teleport channel, `Once`, and `WaitGroup`, meshed through smoketest plus benchmark/attrition evidence and backed by Z3 index/counter proofs. |
| `DONE` | `P0` | new `std::thread` | Add spawn/join, names, affinity, ids, and thread-local or per-thread helpers where appropriate. Today we only have tiny machine-thread fragments. |
| `DONE` | `P0` | `std::diagnostics` plus likely new `std::debug` or `std::log` | Expand from status helpers into structured error values, trace/log helpers, human and machine renderers, progress emitters, and debug-facing utilities that do not require everyone to wire bespoke logging. |

## P1 - Systems, Network, Crypto, Process, And Platform Depth

These are the next obvious completeness gaps once the authoring floor is honest.

| Status | Priority | Surface | Requirement |
| --- | --- | --- | --- |
| `PARTIAL` | `P1` | new `std::os` or expansions across `std::platform` and `std::process` | Add env reads/writes, cwd, user/temp/home/config/cache dirs, host/process identity, page-size and memory facts, signal/terminal basics, and platform feature reporting. |
| `TODO` | `P1` | new `std::posix` | Expose POSIX-specific handles/constants/helpers cleanly when authored Kain genuinely needs them, without poisoning the generic root APIs. |
| `PARTIAL` | `P1` | `std::platform` | Expand beyond dynamic library open/resolve/close into richer platform capability discovery, library metadata, and OS integration helpers. |
| `DONE` | `P1` | new `std::target` or `std::meta` target lane | Add public target triples, ABI and calling-convention vocab, arch/os/env/object-format descriptors, and CPU feature tables where authored tools need them. |
| `PARTIAL` | `P1` | `std::process` | Replace pure id/status/string handling with typed process specs, typed exit status, pipelines, env snapshots, current-process helpers, stream-backed stdin/stdout/stderr, and tighter `std::io` integration. |
| `PARTIAL` | `P1` | `std::net` | Add address types, UDP, DNS resolution, non-blocking/timeouts, Unix-domain sockets where supported, listener/connection option structs, and typed errors instead of raw ids everywhere. |
| `PARTIAL` | `P1` | `std::http` | Add typed methods, status codes, headers, body streaming, chunked transfer, client/server config structs, and better integration with `std::uri`, `std::io`, and `std::tls`. |
| `PARTIAL` | `P1` | `std::tls` | Add configuration structs, certificate loading/inspection, verification policy, handshake state, session info, and better error surfacing. |
| `PARTIAL` | `P1` | `std::http2` | Grow past basic request helpers into streams, headers, state, and protocol-level controls or else explicitly narrow its scope. |
| `PARTIAL` | `P1` | `std::crypto` | Expand beyond SHA-256, HMAC-SHA256, BLAKE3, and random hex into bytes-based APIs, streaming digests, KDF/HKDF/PBKDF2/Argon2-class primitives, AEAD, signatures, key exchange, and certificate helpers where appropriate. |
| `PARTIAL` | `P1` | `std::hash` | Add 64-bit and larger families, streaming hash builders, stable typed fingerprint helpers, and better integration with generic collections and routing use cases. |
| `PARTIAL` | `P1` | `std::memory` and `std::machine` | Add typed slices/spans, cache-line and page helpers, memory-mapped regions where justified, and a cleaner public low-level systems vocabulary than scattered raw pointer math alone. |
| `DONE` | `P1` | new `std::reflect` or `std::meta` | Expose public reflection/introspection helpers where authored Kain packages, serializers, UI systems, and debug tools need them. Kain has reflection ownership deeper in the repo; the root stdlib surface is still thin here. |

## KX - Kain-Native Semantic Leverage

This is where Kain should stop merely catching up and start having surfaces other languages do not.

| Status | Priority | Surface | Requirement |
| --- | --- | --- | --- |
| `PARTIAL` | `KX` | `std::intent` | Replace string-and-counter-only semantic inspection with typed `EntangleBinding`, patch journal records, converge lane records, law verdicts, and orchestration stage records. Add query surfaces that make semantic tooling composable. |
| `PARTIAL` | `KX` | `std::actor` | Expand supervision, tracing, mailbox policy, ask/reply telemetry, restart history, and typed helper records so actor-heavy authored Kain has a richer public runtime vocabulary. |
| `PARTIAL` | `KX` | `std::gen_server` | Graduate from the tiny alias layer into real link/monitor/timeouts/deferred replies/continuations/state helpers if we want OTP-like ergonomics to be a first-class Kain story. |
| `PARTIAL` | `KX` | `std::runtime` | Add typed capability descriptors, telemetry records, converge cache and lane metadata, machine facts, and runtime report structs instead of only scalar counters. |
| `PARTIAL` | `KX` | `std::reload` | Add typed world/actor/resource migration structs, snapshot schemas, compatibility helpers, and reload evidence helpers so hot reload is not just string constants and session ids. |
| `TODO` | `KX` | semantic serialization surface | Add a deterministic root-stdlib story for serializing world state, actor messages, patch journals, entangle graphs, and reload snapshots. This can be a new module or a coordinated family across `std::intent`, `std::actor`, and `std::json`. |
| `TODO` | `KX` | message/channel/mailbox helpers | Decide whether parts of non-actor coordination belong in `std::sync`, `std::actor`, or a new message/mailbox module, then give Kain an honest public surface for them. |
| `PARTIAL` | `KX` | `std::gpu`, `std::graphics`, `std::graphics::shared`, `std::ui` | Keep the strong rendering/resource surface, but add more typed bridges between semantic state, resource graphs, UI data, and runtime ownership rules where that lowers repeated boilerplate. |
| `PARTIAL` | `KX` | new `std::z3` | Add an optional host-solver lane over Python `z3-solver` so authored Kain can build SMT constraints, inspect models, and pressure proof/test workflows without pretending the runtime bundles Z3. Keep the surface Kain-shaped, gateable, and honest about the external Python dependency. |
| `PARTIAL` | `KX` | `std::test` and `std::proof` | Expand from task constructors and basic outcomes into richer authored assertions, property-style helpers, structured witness/proof result records, and solver-friendly harness helpers. |

## P2 - Archives, Compression, Binary Formats, And Deep Systems Tooling

These are still part of a serious stdlib story. They may land after the `P0` floor, but they belong on the board until they are real or honestly waived.

| Status | Priority | Surface | Requirement |
| --- | --- | --- | --- |
| `DONE` | `P2` | new `std::compress` | Add compression/decompression families such as flate/zlib and modern codecs such as zstd where justified, with stream integration instead of one-shot-only helpers. |
| `DONE` | `P2` | new `std::tar` | Add tar read/write/introspection helpers and typed archive entry surfaces. |
| `DONE` | `P2` | new `std::zip` | Add zip read/write/introspection helpers and typed archive entry surfaces. |
| `DONE` | `P2` | new `std::elf` | Add ELF constants, headers, sections, symbols, relocations, and introspection helpers where authored toolchains or profilers need them. |
| `TODO` | `P2` | new `std::dwarf` | Add DWARF constants and targeted parsing/introspection helpers if we want serious debug-format work from authored Kain. |
| `TODO` | `P2` | new `std::macho` | Add Mach-O object and binary introspection helpers. |
| `TODO` | `P2` | new `std::coff` | Add COFF and PE-style object introspection helpers where Windows-facing tooling needs them. |
| `TODO` | `P2` | new `std::pdb` | Add PDB-oriented debug info introspection helpers where Windows tool flows need them. |
| `DONE` | `P2` | new `std::wasm` | Add public authored helpers for WASM module introspection or manipulation if we want the root stdlib to own that tooling story directly. |

## Likely Remaining Root Modules

These names are not sacred, but they are the most obvious concrete root families still missing from the live tree right now:

- `stdlib/os.kn`
- `stdlib/posix.kn`
- `stdlib/dwarf.kn`
- `stdlib/macho.kn`
- `stdlib/coff.kn`
- `stdlib/pdb.kn`

If a future implementation chooses a different public module split, update both this section and the requirement rows so the difference is explicit.

## Completion Worksheet

When landing work for any row above, leave a short completion note in the commit or PR description and update the row status here. If helpful, copy this checklist into your work log:

- [ ] Public root API landed in `stdlib/*.kn`
- [ ] Owning runtime/bootstrap/crate work landed if required
- [ ] `stdlib/STDLIB_MAP.llm.md` and `stdlib/stdlib.map.json` regenerated and checked
- [ ] `smoketest/src/stdlib/*_lane.kn` added or updated
- [ ] `smoketest/src/main.kn` wired
- [ ] `smoketest/build.kn` inputs wired
- [ ] `smoketest` total track count updated if new track added
- [ ] Cross-track or blade-level consumer added when practical
- [ ] Benchmark lane added for performance-sensitive work
- [ ] Attrition lane added for teardown/resource-sensitive work
- [ ] Z3 proof added for unsafe invariants
- [ ] Runtime conformance updated for ABI-backed surfaces
- [ ] `MEMORY.md` updated if the work changes durable operator truth
- [ ] Requirement row status changed in this file

## Final Rule

Do not call the Kain root stdlib "complete" because the directory looks busy or the symbol count is big.

Call it complete when the missing capability families above are either:

- `DONE` with real public surfaces and evidence, or
- `WAIVED` with an explicit reason that a future agent can respect without guessing.

Until then, this file stays live.
