# Skill Taxonomy

This file is only the live skill split for current agents.

## Namespaces

- `lang-*`: writing in Kain. Authored `.kn` code, project/build authority, stdlib usage, systems-level actors/effects/ownership/raw memory, interop/OS/native package boundaries, UI, GPU, and translation.
- `bootstrap-*`: changing compiler/frontend/selfhost truth.
- `runtime-*`: changing native substrate and runtime-backed stdlib behavior.
- `test-*`: certification lanes such as harness, benchmark, attrition, and crash forensics.
- `package-*`: durable package-owned surfaces.
- `wildcard-*`: explicit high-freedom authoring overrides that deliberately cap repo context and favor intuition-first Kain drafting when the user wants creativity and speed over pattern-matching.
- `tool-*`: rare cross-cutting operator lanes such as build plumbing, black-magic optimization, exploratory bug hunting, and release gating.

## Active Skills

- `lang-semantics`
- `lang-systems`
- `lang-interop`
- `lang-projects`
- `lang-stdlib`
- `lang-translation`
- `lang-gpu`
- `lang-feedback`
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
- `wildcard-justwritebro`
- `tool-build-system`
- `tool-z3-black-magic`
- `tool-z3-bug-hunter`
- `tool-release-readiness`

## Rules

- Prefer updating an existing namespaced skill over adding a new micro-skill.
- Do not create `misc-*`.
- Use `lang-projects` for `build.kn`, project authority, evidence DAGs, blades/workspaces, portable capsules, and authored project/run/build/test flow. Blades are now a scale mode inside projects, not a top-level skill.
- Use `lang-systems` for fused actor/effects/ownership/raw-memory authoring; keep `lang-actors` and `lang-ownership` as narrower domain lanes until they are intentionally merged or archived.
- Use `lang-interop` for Kain-side native/foreign boundaries; keep `lang-c-abi-ffi` as the narrow C ABI card until it is intentionally merged or archived.
- Keep `wildcard-*` rare and explicit. They are authoring overrides, not replacements for the owning `lang-*` field manuals.
- `tool-build-system` is the single build/Bazel/operator lane.
- `tool-z3-bug-hunter` is the logging-only sibling of `tool-z3-black-magic`: use it to inventory reproducible bugs and edge cases, not to patch them inline.
- `package-*` should stay rare and only exist for real package surfaces.
