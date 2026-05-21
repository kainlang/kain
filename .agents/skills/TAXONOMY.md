# Skill Taxonomy

This file is the route map for the active repo-local skill tree under `.agents/skills/`.

## Active Namespaces

- `lang-*`: authored Kain work. Owns `.kn` authoring, blades, stdlib usage, actors, ownership, UI, GPU, translation, and project-facing command usage.
- `bootstrap-*`: compiler/frontend/selfhost truth. Owns parser, AST, lowering, semantic wiring, and bootstrap-side feature implementation.
- `runtime-*`: native substrate and runtime-backed stdlib behavior. Owns C/runtime host bridges, native services, GPU execution/runtime lanes, and runtime implementation.
- `test-*`: certification lanes. Owns harness, benchmark, attrition, and crash-forensics evidence.
- `package-*`: package-owned surfaces. Use only for real reusable package families.
- `tool-*`: rare cross-cutting operator lanes such as repo build plumbing, black-magic optimization, and release gating.

## Active Skill Set

- `lang-authoring`
- `lang-semantics`
- `lang-actors`
- `lang-commands`
- `lang-blades`
- `lang-stdlib`
- `lang-c-abi-ffi`
- `lang-ownership`
- `lang-ui`
- `lang-translation`
- `lang-gpu`
- `bootstrap-core`
- `bootstrap-actors`
- `bootstrap-ownership`
- `bootstrap-fs`
- `bootstrap-gpu`
- `runtime-core`
- `runtime-stdlib`
- `runtime-gpu`
- `test-harness`
- `test-bench`
- `test-attrition`
- `test-crash-forensics`
- `package-kaintana`
- `package-vulkain`
- `tool-build-system`
- `tool-z3-black-magic`
- `tool-release-readiness`

## Hard Rules

- Prefer updating an existing namespaced skill over creating a new micro-skill.
- Do not create `misc-*`.
- Keep `tool-*` small and rare.
- `tool-build-system` is the single owner for Bazel sync, repo build plumbing, launcher/build operator reality, generated BUILD drift, and command/build internals.
- `package-*` should exist only for durable package families. In v1 that means `package-kaintana` and `package-vulkain`.

## Legacy Alias Map

- `kain-actor-system` -> `lang-actors`, `bootstrap-actors`, `runtime-core`
- `kain-amalgamate-capsules` -> `lang-authoring`, `lang-blades`, `tool-build-system`
- `kain-attrition-pipeline` -> `test-attrition`
- `kain-bazel-runtime-sync` -> `tool-build-system`
- `kain-bazel-rust-sync` -> `tool-build-system`
- `kain-benchmark-pipeline` -> `test-bench`
- `kain-blade-workspace` -> `lang-authoring`, `lang-blades`
- `kain-blades-system` -> `tool-build-system`
- `kain-check-test-pipeline` -> `test-harness`
- `kain-command-platform` -> `lang-commands`, `tool-build-system`
- `kain-core-z3-proofs` -> `bootstrap-core`, `tool-z3-black-magic` when the work is exploratory rather than ordinary proof maintenance
- `kain-entangle-pipeline` -> `lang-semantics`, `bootstrap-core`, `runtime-core`
- `kain-engineer` -> `lang-authoring` first, then the specific `lang-*` domain such as `lang-semantics`, `lang-stdlib`, `lang-ui`, `lang-gpu`, or `lang-c-abi-ffi`
- `kain-foreign-abi-ffi` -> `lang-c-abi-ffi`, `bootstrap-core`, `runtime-core`
- `kain-fs-pipeline` -> `lang-stdlib`, `bootstrap-fs`, `runtime-stdlib`
- `kain-input-system` -> `lang-stdlib`, `runtime-stdlib`
- `kain-machine-stones` -> `lang-semantics`, `bootstrap-core`, `runtime-core`
- `kain-native-llvm-runtime` -> `runtime-core`, `bootstrap-core`
- `kain-net-system` -> `lang-stdlib`, `runtime-stdlib`
- `kain-ownership-system` -> `lang-ownership`, `bootstrap-ownership`, `runtime-core`
- `kain-pong-state-lattice` -> `lang-semantics`, `lang-actors`, `lang-ui`
- `kain-process-system` -> `lang-stdlib`, `runtime-stdlib`
- `kain-ptx-cuda-backend` -> `lang-gpu`, `bootstrap-gpu`, `runtime-gpu`
- `kain-release-readiness-gate` -> `tool-release-readiness`
- `kain-run-pipeline` -> `lang-commands`, `lang-blades`, `tool-build-system`
- `kain-spirv-codegen-validation` -> `lang-gpu`, `bootstrap-gpu`
- `kain-translation-engineer` -> `lang-translation`
- `kain-ui-native-pipeline` -> `lang-ui`, `runtime-stdlib`, `package-kaintana`
- `kaintana-framework` -> `package-kaintana`, `lang-ui`
- `native-crash-forensics` -> `test-crash-forensics`
- `z3-black-magic-optimizer` -> `tool-z3-black-magic`

## Legacy Archive

Archived pre-namespace skills live under `.agents/skills-legacy/`. Keep them for history and salvage, but do not treat them as the active routing surface.
