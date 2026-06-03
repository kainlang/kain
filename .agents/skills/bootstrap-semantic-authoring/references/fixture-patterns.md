# Semantic Corpus Fixture Patterns

Use this reference only when a batch needs quick pattern ideas.

## Reliable Metadata Families

- `KAIN-TYPE-0002` + `Typo`: unknown identifier where `@expected_repair` is the intended visible symbol.
- `KAIN-TYPE-0003` + `MissingSurface`: malformed or incomplete `world` surface expectations.
- `KAIN-EFFECT-0001` + `GenericUnknown` or a more specific effect mode when `expert.rs` supports it.
- `KAIN-BORROW-0004` + `OwnershipViolation`: decay/use-after-move/exclusive mutation conflicts.
- `KAIN-PARSE-0005`, `KAIN-PARSE-0008`, `KAIN-PARSE-0009` + `ParserDelimiterDamage`: missing delimiters or damaged syntax.
- `KAIN-SHADER-0001` + `ShaderHostBoundary`: host-only calls in shaders.
- `KAIN-SHADER-0003` through `KAIN-SHADER-0012` + `ShaderResourceContract`: resource layout, binding, memory, and stage contract failures.
- `KAIN-CODEGEN-0008` + `CAbiBoundary`: native/foreign ABI mismatch.

## Good Donor Sources

- `benchmark/cases_v2/keyword_expansion.kn`: `where`, `defer`, `workgroup`, `dispatch`, raw memory, and semantic keyword shapes.
- `benchmark/cases_v2/classic_systems.kn`, `metal.kn`, `core_actor.kn`: actor, ownership, memory, and systems pressure.
- `benchmark/cases_v2/python_interop.kn`, `python_semantic.kn`: Python bridge shape.
- `benchmark/cases_v2/system_headers.kn`: `include <...> as ...` C system-header style.
- `benchmark/cases_v2/gpu_cpu_pipeline.kn`, `vulkan_loader.kn`: GPU/host boundary shapes.

## Template Rule

Start from a compile-worthy donor shape, then break one thing. The best fixture lets the reader say: "that exact wrong token caused the diagnostic." If two unrelated failures can fire first, shrink the file.
