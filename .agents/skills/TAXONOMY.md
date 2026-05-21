# Skill Taxonomy

This file is only the live skill split for current agents.

## Namespaces

- `lang-*`: writing in Kain. Authored `.kn` code, blades, stdlib usage, systems-level actors/effects/ownership/raw memory, interop/OS/native package boundaries, UI, GPU, and translation.
- `bootstrap-*`: changing compiler/frontend/selfhost truth.
- `runtime-*`: changing native substrate and runtime-backed stdlib behavior.
- `test-*`: certification lanes such as harness, benchmark, attrition, and crash forensics.
- `package-*`: durable package-owned surfaces.
- `tool-*`: rare cross-cutting operator lanes such as build plumbing, black-magic optimization, and release gating.

## Active Skills

- `lang-authoring`
- `lang-semantics`
- `lang-systems`
- `lang-interop`
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

## Rules

- Prefer updating an existing namespaced skill over adding a new micro-skill.
- Do not create `misc-*`.
- Use `lang-systems` for fused actor/effects/ownership/raw-memory authoring; keep `lang-actors` and `lang-ownership` as narrower domain lanes until they are intentionally merged or archived.
- Use `lang-interop` for Kain-side native/foreign boundaries; keep `lang-c-abi-ffi` as the narrow C ABI card until it is intentionally merged or archived.
- `tool-build-system` is the single build/Bazel/operator lane.
- `package-*` should stay rare and only exist for real package surfaces.
