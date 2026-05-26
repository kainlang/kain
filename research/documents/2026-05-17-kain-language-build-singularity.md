# Kain Language Build Singularity

- Date: 2026-05-17
- Status: active
- Repo Root: `D:\Kain-Lang`
- Session Slug: `kain-language-build-singularity`

## Research Question

How can Kain's language and compiler architecture make program builds approach the theoretical lower bound of recompiling only the semantically affected atoms, while still supporting native LLVM/runtime artifacts, proofs, and multi-target output?

## Constraints

- Target: Kain-the-language build latency, not just this repo's Bazel/Cargo wrapper latency.
- Platform reality: Windows workstation today, native LLVM/runtime priority, multi-target output must remain possible.
- Acceptable weirdness: high. Semantic atoms, persistent compiler worlds, proof-indexed caches, and native daemonized compilation are in scope.
- Safety/correctness: cached artifacts must be invalidated by semantic dependency truth, not timestamps or broad source-file guesses.
- Output classes: check/interpreter, LLVM/native, C/Rust/C++/Wasm/SPIR-V/PTX, runtime contracts, realtime bundles, proofs, and benchmark artifacts.

## Hypothesis Lattice

### Baseline
- Mechanism: classical module/file incremental compilation with content hashes, parallel parsing/typechecking, warmed stdlib/prelude caches, and artifact reuse.
- Expected upside: 2x-10x on ordinary edit loops if Kain stops concatenating all imported source into one frontend string.
- Likely blocker: still over-invalidates because source files are too coarse; a comment or private helper edit can poison more work than necessary.
- Proof obligation: for every cached module artifact, prove its interface digest captures all externally observable type/value/effect/runtime-contract facts.

### Unconventional
- Mechanism: semantic atom graph. Each function, type, impl, actor handler, world field, converge lane, shader entry, FFI binding, and runtime-contract fragment gets a stable digest and dependency edge set. Build = re-evaluate the changed transitive closure only.
- Expected upside: hot edit loops approach O(changed_atoms + affected_edges) instead of O(files + stdlib + target backend).
- Likely blocker: comptime, macros/import shaping, trait/impl resolution, and compiler-owned semantics can create hidden dependencies unless they are recorded as first-class graph edges.
- Proof obligation: if an atom's input digest vector is unchanged, its typed artifact and emitted target fragment are observationally equivalent to the previous build.

### Moonshot
- Mechanism: resident Kain compiler world. The compiler is a long-lived Kain-authored actor/world system that keeps parsed green trees, typed atoms, LLVM bitcode fragments, proof results, benchmark priors, and runtime manifests alive across edits. Source changes become patches against compiler state.
- Expected upside: sub-100ms `check` for most edits, native relink measured in touched symbols, and "build while typing" as a normal language property.
- Likely blocker: the compiler must make mutation boundaries exact. A wrong cache hit is a silent compiler bug, not a performance bug.
- Proof obligation: state-transition invariant: applying a patch to the compiler world and rebuilding affected atoms yields the same result as cold whole-program compilation.

## Mathematical Model

- Variables: `N` total semantic atoms, `A` changed transitive affected atoms, `E` dependency edges, `C` mean per-atom compile cost, `H` fixed incremental overhead, `L` link/materialization cost, `P` proof reuse cost.
- Invariants: every artifact digest includes source content, public interface facts, target capability set, runtime manifest facts, compiler version, backend version, and imported atom digests.
- Objective: minimize `T_edit = H + compile(A) + link(delta_symbols) + proofs(delta_claims)` while preserving equality with cold build output.
- Bad states: stale type artifact, stale runtime contract, stale LLVM fragment, stale proof result, stale ABI/layout fact, or cache key that omits a semantic input.
- Simplifying assumptions: the first model treats atom cost as uniform and ignores IO contention; later models need per-target and per-atom cost classes.

## Z3 Claims

1. Atom-lower-bound sanity: if `H < (N - A) * C`, then `H + A*C < N*C`. Z3 result: `unsat` for the counterexample. Report: `z3/reports/20260517T223128Z-kain-language-build-atom-lower-bound.json`.
2. Next claim to model: no-stale-hit invariant for artifact keys, where equality of all semantic inputs implies equality of emitted fragment.

## Evidence And Sources

- Local: `crates/driver/src/lib.rs` currently builds a `FrontendSourceBundle` by collecting imported module sources and assembling `full_source`, then lexing/parsing/comptime/typechecking the combined program.
- Local: `crates/core/src/lib.rs` legacy `compile` also prepends target stdlib source and compiles as one source string.
- Local: `ARCHITECTURE.md` identifies native LLVM/runtime priority, `crates/run`, `crates/build`, runtime contracts, realtime bundles, and proof packs as the relevant build surfaces.
- Local: `crates/blades/src/lib.rs` already owns Blade workspace discovery, `ResolvedBlade`, `BladeWorkspace::dependency_edges`, source roots, module roots, build targets, C/Rust FFI, GPU, Fabric, and synthetic Cargo blades.
- Local: `crates/build/src/workspace.rs` already owns the Blade build DAG, lane/profile/target-aware artifact roots, cacheable `BuildTask`s, stamp files, cache-hit reporting, and Kain/Cargo/C/GPU/Fabric/Node/Bun adapters.
- Local: `crates/core/src/module_resolution.rs` consumes `blade::discover_blade_module_roots_from`, so any language-level incremental system must preserve Blade-owned module discovery instead of adding a parallel scanner.
- External: none checked yet in this note; local architecture is enough for the first hypothesis split.

## Dead Ends

- "Just use faster Bazel/Cargo settings" is not the language answer. It can help this repo, but it does not make Kain programs intrinsically fast to build.
- Whole-file incremental is a baseline, not the record-breaking design, because Kain's semantic surface is richer than files: worlds, entangles, actors, converge lanes, runtime contracts, shader artifacts, and proofs all have finer dependency structure.

## Implementation Slice Plan

### Slice 0 - Measurement Harness
- Add timing telemetry around frontend import collection, lex, parse, comptime, type registration, item checking, monomorphization, codegen, runtime-contract emission, realtime-bundle emission, and link/materialization.
- Output a stable JSON report from `kain check/build/run` so every later slice has proof instead of vibes.
- Risk: near zero if read-only telemetry is behind a flag or report field.

### Slice 0B - Blade Workspace Fingerprint
- Treat `BladeWorkspace` as the top-level package graph for multi-file/workspace builds: `workspace_digest -> blade_digest -> module_digest -> semantic_atom_digest -> target_fragment_digest`.
- Compute a conservative blade fingerprint from manifest path/content, blade name/version/kind, entry, source roots, module roots, build targets, explicit dependencies, FFI/GPU/Fabric/Rust metadata, lane, profile, target, and compiler/runtime versions.
- Record actual import edges discovered during frontend collection in addition to declared `[[blade.dependencies]]`; undeclared module-root coupling must be visible before it can be optimized.
- Risk: low. The Blade crate already centralizes discovery and the Build crate already has task stamps; this slice mostly promotes that data into the language build cache key.

### Slice 1 - Module Bundle Without Behavior Change
- Replace `FrontendSourceBundle { full_source }` with `FrontendModuleBundle { modules, entry }` in `crates/driver`, but keep an adapter that assembles the old `full_source`.
- Each module record should carry canonical path, source hash, target, imports, and prepared source.
- Each module should also carry optional blade ownership: workspace root, blade name, blade root, source root, module root, and the build lane/profile/target that selected it.
- Existing tests around imported stdlib/filesystem module materialization become the acceptance floor.
- Risk: low. This is mostly data-shape extraction around existing import collection.

### Slice 2 - Parsed Module Cache
- Cache token streams and parsed `Program` per `(canonical_path/source_hash/target/compiler_version)`.
- Entry source can use a synthetic path and content hash.
- Keep typechecking whole-program initially by assembling parsed module items in the current order.
- Risk: low to medium. Parser diagnostics and span mapping need care because current code maps spans against one combined source.

### Slice 3 - Semantic Atom Inventory
- Introduce `SemanticAtomId`, `SemanticAtomKind`, `SemanticDigest`, and `SemanticAtomGraph` in or near `kain-core`.
- Atom kinds should map directly to `TypedItem` variants: function, struct, enum, trait, impl, actor, world, entangle, converge, law, patch, shader, const, type alias, component, material graph, async task, editor/gameplay items, etc.
- Scope stable atom IDs by workspace and blade identity, then module path and item path. Example shape: `workspace_root::blade_name::module_path::item_path::atom_kind`.
- First pass only inventories atoms and dependencies without using the cache for reuse.
- Risk: low. Read-only graph extraction is safe and immediately useful for diagnostics/build reports.

### Slice 4 - Public Interface Digest
- For each atom, compute an interface digest: name, kind, exported visibility, signature/type shape, effects, capability requirements, layout-sensitive fields, and target-relevant metadata.
- Prove/cache only against interface digests first, not full implementation digests.
- Risk: medium. Missing an interface fact causes over-reuse; this needs conservative digesting at first.

### Slice 5 - TypeEnv Snapshot And Item-Level Type Reuse
- Split the current `types::check_with_extra_globals` pipeline into reusable phases:
  - predeclare item types
  - register item types/globals/methods
  - check item bodies
- Cache checked item bodies when their implementation digest and dependency interface digests are unchanged.
- Still return a normal `TypedProgram` to all existing codegen backends.
- Risk: medium-high. `TypeEnv` has global maps for types, globals, methods, enum variants, and entangle endpoints; dependency recording must be honest.

### Slice 6 - Resident Compiler World
- Add a daemon/session mode that holds the `BladeWorkspace`, per-blade module parse caches, atom graphs, typed atom caches, stdlib atoms, and latest build reports in memory.
- `kain check`, `kain build`, `kain run`, Blade build tasks, and LSP can query the same resident world.
- Risk: medium. Process lifecycle and cache invalidation are manageable; correctness depends on earlier digest discipline.

### Slice 7 - Backend Fragment Cache
- Cache target fragments per typed atom: LLVM IR/bitcode/object where feasible, SPIR-V/PTX shader artifacts, runtime-contract fragments, realtime-bundle fragments, proof results.
- Start with runtime contracts and shader artifacts before LLVM linking because their boundaries are cleaner.
- Risk: medium-high for LLVM native due symbol/link/layout interactions; low-medium for contract/proof/shader fragments.

## Risk Assessment

- Architecture fit: supported. `kain-driver` owns frontend orchestration, `kain-core` owns typed semantics, and downstream codegen already consumes `TypedProgram`.
- Blade architecture fit: strong. `crates/blades` already owns workspace/package discovery and `crates/build` already owns task planning, artifacts, and stamps, so blades should be the outer incremental boundary rather than an afterthought.
- Refactor size: bounded but real. The first four slices are additive and can be hidden behind compatibility assembly; item-level typed reuse requires changing the typechecker phase boundaries.
- Correctness risk: nonzero. The dangerous class is stale cache hits from missing semantic dependencies. Mitigation is conservative invalidation first, then proofs for narrower reuse.
- Backend risk: avoidable early. Do not touch LLVM/codegen first; keep producing `TypedProgram`.
- Diagnostic risk: real in Slice 2 because one combined `SpanMapper` currently makes span reporting simple. Module-local spans need source-map stitching.
- Comptime risk: medium. Comptime can create hidden dependencies and should initially force conservative invalidation of the module/atom region it touches.
- Trait/impl/method risk: medium. Method tables and impl registrations are global in `TypeEnv`; atom dependencies must include method-resolution facts.
- World/entangle/converge risk: medium. Their runtime-contract/realtime-bundle effects mean digest keys need more than ordinary type signatures.
- Blade manifest risk: medium-low. Manifest edits to entry/source roots/module roots/build targets/lane/profile/target/FFI/GPU/Fabric metadata must invalidate the right tasks even when Kain source content is unchanged.
- Cross-blade import risk: medium. Declared blade dependencies are not enough by themselves; module-root resolution can create actual import edges that must be captured during frontend collection.
- Polyglot sidecar risk: bounded. Cargo, C shared libraries, GPU artifacts, Fabric, Node, and Bun tasks are not pure Kain semantic atoms; model them as external task atoms keyed by adapter inputs and outputs through `kain-build`.
- Existential risk: low. Nothing in the repo shape blocks this; the path is incremental if compatibility `TypedProgram` remains the backend boundary.

## Recommended First Landing Sequence

1. Land timing telemetry and a `kain check --build-profile-json` style report.
2. Land Blade workspace fingerprints in `kain-build` reports and stamp inputs, using the existing `blade` resolver.
3. Land `FrontendModuleBundle` while still assembling old `full_source`, with optional blade ownership attached to each module.
4. Land parsed module cache and prove output parity on existing driver tests plus Blade module-root tests.
5. Land atom inventory reports with no cache reuse.
6. Land interface digests and conservative invalidation.
7. Only then cache typed item bodies.

## Conclusion

Pending.
