# Kain Amalgamate Capsule

- Date: 2026-05-17
- Status: active
- Repo Root: `D:\Kain-Lang`
- Session Slug: `kain-amalgamate-capsule`

## Research Question

How should Kain represent a full multi-language workspace or blade as a single .kn carrier file without breaking the existing import, blade, and native build pipelines?

## Constraints

- Must emit a single `.kn` carrier artifact that can move across machines and still feel like a first-class Kain entrypoint.
- Must preserve real blade/workspace semantics: `KAIN.toml`, `src/`, `native/`, `config/`, `Cargo.toml`, `package.json`, shader assets, and `c_ffi` metadata.
- Must not force every foreign file through semantic translation just to make the bundle work. `nuklear.h` and the SQLite amalgamation are strongest when preserved, not flattened.
- Must reuse the current `kain-blades`, `kain-build`, `kain-run`, and `kain-core` filesystem/module behavior instead of inventing a second execution model.
- Must keep materialization deterministic and cacheable, ideally under `.kain/cache/amalgamate/<digest>/`.
- Must support future operator surfaces such as `inspect`, `verify`, and `unpack`, not just one-way packing.

## Hypothesis Lattice

### Baseline
- Mechanism: put the feature under `kain-import` and translate every foreign input into one giant Kain bundle file, similar to the existing C/Rust/TypeScript import lanes.
- Expected upside: minimal new crate surface, immediate reuse of current AST-to-source generation code, and the operator model already exists.
- Likely blocker: this destroys fidelity for real mixed workspaces. Headers, macros, `KAIN.toml`, `native/` assets, `build.tasks`, and sidecar manifests are not "imported code"; they are workspace structure.
- Proof obligation: show that a transliterated mega-module can still preserve blade/build behavior for `use c::...`, `c_ffi`, imported manifests, and multi-root workspace layout.

### Unconventional
- Mechanism: create `crates/amalgamate` as a capsule pack/unpack/materialize lane. A generated `.kn` file acts as a carrier, but the packed payload materializes back into a normal workspace tree before the usual blade/build/run systems take over.
- Expected upside: preserves exact source trees, keeps `nuklear.h` and SQLite usable as raw foreign sources, and makes `kain run capsule.kn` feel like running an ordinary blade instead of a special importer path.
- Likely blocker: requires a pre-parse detection/materialization seam in the CLI and run/build front doors, plus a durable capsule schema.
- Proof obligation: prove blob layout safety, deterministic materialization, and equivalence between "mounted capsule workspace" resolution and original on-disk workspace resolution.

### Moonshot
- Mechanism: skip on-disk extraction entirely and mount the capsule as a virtual filesystem root such as `fs://capsule/<digest>/...`, letting compiler, blade discovery, and runtime tooling read directly from the embedded payload.
- Expected upside: zero extraction churn, instant portable execution, and a path toward network-distributed self-contained Kain artifacts.
- Likely blocker: pushes VFS awareness through every path-sensitive seam in compiler/build/run/blade discovery and makes debugging/materialization more opaque.
- Proof obligation: prove deterministic module resolution, hashing, and artifact provenance without relying on normal filesystem normalization.

## Mathematical Model

- Variables: `F` bundled files, `p_i` relative file path, `o_i` payload offset, `l_i` payload length, `B` total payload bytes, `d` bundle digest, `r` materialization root, `e` manifest entry path, `m` capsule metadata.
- Invariants:
  - Each bundled path is unique inside one capsule.
  - Offsets are monotone: `o_(i+1) >= o_i + l_i`.
  - Final bounds hold: `o_last + l_last <= B`.
  - The manifest entry path exists in the bundled file set.
  - Materialized path projection is stable: `materialized_i = r / d / p_i`.
  - Blade/workspace discovery over the materialized root returns the same manifest/entry relation as the original source tree.
- Objective: maximize source-fidelity and portability while minimizing new semantics in the compiler frontend.
- Bad states: path collisions, truncated file payloads, stale extracted trees after bundle updates, path escape outside the cache root, special-case build behavior that diverges from normal blades, or forcing foreign sources through lossy translation when preservation was possible.
- Simplifying assumptions: v1 is allowed to materialize a cache tree on disk; SHA-256 digest collision risk is ignored; compression ratio matters more than textual readability; current run/build commands may intercept the input before normal Kain parsing.

## Z3 Claims

1. Payload-table non-overlap claim: if three representative file spans have nonnegative lengths, monotone offsets, and the final file end stays within blob size, then no file end can overlap the next file start or extend past the blob. Status: `unsat` witness to the negated bad state. Report: `z3/reports/20260517T081310Z-amalgamate-capsule-layout-three-file-nonoverlap.json`.
2. Future claim: digest-scoped materialization roots should make two distinct capsules path-disjoint unless the digest layer itself is broken. Not modeled yet because the first research bottleneck is bundle layout and workspace handoff, not hash cryptography.

## Evidence And Sources

- Local:
  - `crates/import/src/lib.rs`: current import ownership is AST translation by language lane, not workspace transport or mount orchestration.
  - `crates/cli/src/import_c.rs` and `crates/cli/src/import_rust.rs`: the operator surface already distinguishes between "single bundle" and "mirrored blades workspace", which is a strong pattern for an amalgamation command family.
  - `crates/blades/src/lib.rs`: workspace discovery already understands `KAIN.toml`, Cargo manifests, blade roots, app roots, crate roots, and `c_ffi` metadata.
  - `crates/build/src/workspace.rs`: build planning already delegates from discovered manifests and workspace structure, so a materialized capsule can reuse the same engine.
  - `blades/pong/KAIN.toml` and `blades/kain-labs/KAIN.toml`: real blades are mixed trees with `src/`, `native/`, `config/`, `build.tasks`, manifests, and C bridge metadata. A faithful feature must preserve that tree shape.
  - `blades/pong/src/main.kn`: real Kain blades depend on `use c::...` plus many sibling modules, which argues against flattening everything into one mega-module.
  - `crates/amalgamate/`: an empty reserved crate namespace already exists and cleanly fits the ownership boundary.
- External:
  - None. This pass is fully grounded in repo-local architecture and operator surfaces.

## Dead Ends

- Stuffing the whole feature into `kain-import` as "one more importer" is the wrong ownership boundary. Import translates language semantics; amalgamation preserves and transports whole workspaces.
- Requiring v1 to skip materialization completely is premature. A direct virtual mount is interesting, but it expands the problem into a repo-wide VFS migration before the core value is proven.

## Conclusion

Current thesis: the best design is not "import but bigger." It is a new `crates/amalgamate` capsule system whose `.kn` output is a portable carrier for an entire blade or workspace, with the packed tree materialized back into `.kain/cache/amalgamate/<digest>/...` before normal blade/build/run logic executes.

That keeps the real Kain graph sovereign. `kain-import` remains a helper, not the owner. If a capsule wants generated Kain facades for C/Rust/TypeScript, `kain-amalgamate` can call existing import lanes as an optional `--generate-adapters` phase while still preserving the original `nuklear.h`, SQLite amalgamation, manifests, and native assets in the payload.

Recommended v1 command family:

- `kain amalgamate <path> -o <name>.kn`
- `kain amalgamate inspect <name>.kn`
- `kain amalgamate unpack <name>.kn -o <dir>`
- `kain run <name>.kn`

Recommended v1 capsule contents:

- Capsule metadata: bundle kind (`entry`, `blade`, `workspace`), source root, entry path, digest, compression mode, file table, preserved manifest map, optional generated-adapter table.
- Payload encoding: compressed chunk stream with deterministic ordering and quote-safe text encoding.
- Materialization rule: expand into a digest-scoped cache root, then hand the resulting path to existing workspace discovery/build/run code as if it had always lived there.

Recommended header strategy:

- Separate the capsule into two truths:
  - a human-facing prelude at the top of the `.kn` file for glanceable identity
  - a machine-truth metadata block with a schema version and extensible key/value payload
- Do not make free-form top comments the only source of truth. Comments are for readability; the structured metadata block is for tools.
- Allow operator-supplied arbitrary metadata such as `author`, `company`, `copyright`, `notes`, `license`, `tags`, or custom org fields, but keep a reserved namespace for canonical keys such as `name`, `version`, `digest`, `entry`, `modules`, `public_api`, `created_at`, `source_kind`, and `compression`.
- Treat `modules` and `public_api` as derived indices by default. Let operators override or add display metadata, but do not require humans to maintain symbol inventories by hand.

One promising shape is:

```kn
// capsule: KAIN_AMALGAMATE v1
// name: kipp_physics
// version: 1.0.0-stable
// entry: src/main.kn
// digest: sha256:8f3c92a...
// modules: [broad_phase, narrow_phase, constraints, solvers]
// public_api: [init_physics_world, step_simulation, RaycastHit]
// author: Taylor Kipp
// notes: "portable build capsule"

@amalgamate(
    schema: 1,
    kind: "blade",
    compression: "zstd",
    manifest: "KAIN.toml",
    metadata: {
        company: "businessinc",
        copyright: "mybusiness2026"
    }
)
```

The human-facing comment prelude gives the "holy shit this is a whole project" feeling, while the attribute block gives the CLI and future LLM tooling something stable to parse. The better long-term rule is: render the prelude from the structured metadata during pack/update, never trust drifted comments over the structured block.

Best next experiment: spike `crates/amalgamate` against `blades/network-domains` first, then `blades/pong`, then a tiny wrapper blade that embeds `nuklear.h` plus the SQLite amalgamation. If `pong` survives intact, the direction is real instead of merely elegant.
