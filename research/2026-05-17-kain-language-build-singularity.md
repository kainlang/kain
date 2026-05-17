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

- Local: `crates/kain-driver/src/lib.rs` currently builds a `FrontendSourceBundle` by collecting imported module sources and assembling `full_source`, then lexing/parsing/comptime/typechecking the combined program.
- Local: `crates/kain-core/src/lib.rs` legacy `compile` also prepends target stdlib source and compiles as one source string.
- Local: `ARCHITECTURE.md` identifies native LLVM/runtime priority, `crates/kain-run`, `crates/kain-build`, runtime contracts, realtime bundles, and proof packs as the relevant build surfaces.
- External: none checked yet in this note; local architecture is enough for the first hypothesis split.

## Dead Ends

- "Just use faster Bazel/Cargo settings" is not the language answer. It can help this repo, but it does not make Kain programs intrinsically fast to build.
- Whole-file incremental is a baseline, not the record-breaking design, because Kain's semantic surface is richer than files: worlds, entangles, actors, converge lanes, runtime contracts, shader artifacts, and proofs all have finer dependency structure.

## Conclusion

Pending.
