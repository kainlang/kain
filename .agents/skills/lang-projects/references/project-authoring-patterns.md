# Project Authoring Patterns

Load this reference when the main `lang-projects` skill is not enough and you need concrete project shapes, dogfood examples, or escalation clues.

## Good Examples To Inspect

- `blades/kloner/build.kn`: large app authority with package metadata, many source/module roots, platform requirement, check, source-test, root executable, and certify tasks.
- `blades/kaintana/build.kn`: reusable UI framework package with source tests, Z3 proof obligation, root executable, and certify gate.
- `blades/kaintana-test/build.kn`: consumer proof project that depends on Kaintana and emits a root executable.
- `blades/vulkain/build.kn`: compact package-owned Vulkan requirement and explicit check task.
- `blades/amalgamate-capsule-probe`: portable capsule dogfood for pack, inspect, unpack, and transparent check/run/build routing.
- `benchmark/cases/semantic_singularity_crucible/main.kn`: dense single-file Kain proof of language semantics without requiring a workspace first.
- `blades/kain-example/src/main.kn`: broad native LLVM proving ground for app-style Kain.
- `blades/kain-labs`: reference-driven GPU/UI lab that shows when project scale earns modules, generated artifacts, and root executable proof.

## Choosing Scale

Use a loose file when:

- The user asks for a sketch, probe, importer result, algorithm, or compact proof.
- The code can be validated by `kain check`, `kain run`, or `kain build` directly.
- There is no reusable package boundary or dependency graph.

Use `build.kn` when:

- The project needs repeatable evidence.
- Outputs need to be explicit.
- A root executable should be generated.
- Source tests, Z3, benchmark, attrition, or certification are part of the claim.
- Agents need a single map of what matters.

Use a blade or workspace when:

- The project is reusable.
- The project has sibling dependencies.
- The project has native bridge files, package-owned platform locks, or shared `.kain` artifacts.
- The project is big enough that discovery, graph, equip, and run/build defaults matter.

Use a capsule when:

- A whole Kain project needs to travel as one file.
- A repro needs to be sent to another agent without a folder tree.
- A tool should inspect or unpack a Kain project without trusting a random archive.

Use Fabric only when:

- Maintaining an existing Fabric manifest.
- The project is intentionally using the older polyglot orchestration lane.
- The requested output depends on `KAIN.fabric.toml`.

## Project Skeleton

Small app:

```text
my-app/
  build.kn
  src/
    main.kn
    state.kn
    ui.kn
  config/
    modes.json
  .kain/
```

Interop app:

```text
my-bridge/
  build.kn
  src/
    main.kn
    bridge_surface.kn
  native/
    bridge.h
    bridge.c
  .kain/
```

GPU app:

```text
my-gpu/
  build.kn
  src/
    main.kn
    kernels.kn
    resources.kn
  .kain/
    gpu/
    out/
```

Package workspace:

```text
blades/
  my-lib/
    build.kn
    src/
  my-app/
    build.kn
    src/
    native/
```

## Build Dot Kn Checklist

Every serious `build.kn` should answer:

- What is the package name and version?
- What is the entry file?
- What source roots and module roots are valid?
- What target is the default?
- What artifacts and caches should be under `.kain/`?
- What platform packages are required?
- What source files, native files, shader files, config files, and reference files are task inputs?
- What check task proves frontend validity?
- What source test task proves authored behavior?
- What proof task proves unsafe math, layout, state, or capacity claims?
- What benchmark or attrition task certifies performance or runtime health?
- What root executable is emitted?
- What final certification gate depends on all evidence?

## Compact Build Dot Kn Pattern

```kn
use std::build
use std::test
use std::certify

fn build(ctx: BuildContext) -> BuildGraph:
    let pkg = package("tiny")
        .version("0.1.0")
        .description("Tiny Kain project with explicit evidence.")

    let app = blade("tiny")
        .entry("src/main.kn")
        .source_root("src")
        .module_root("src")
        .build_target("llvm")

    let defaults = build_defaults()
        .entry("src/main.kn")
        .artifact_root(".kain/out")
        .cache_root(".kain/cache/build")
        .profile("debug")
        .target("llvm")

    let check = build_check("check-llvm")
        .entry("src/main.kn")
        .target("llvm")
        .input("src/main.kn")
        .input("build.kn")

    let tests = test_suite("source-tests")
        .entry("src/main.kn")
        .target("llvm")
        .requires("check-llvm")

    let exe = native_executable("root-executable")
        .entry("src/main.kn")
        .root_output("$blade/tiny.exe")
        .requires("source-tests")

    let gate = certify_gate("certify")
        .requires("root-executable")
        .certifies("tiny.local")

    return build_graph()
        .package(pkg)
        .blade(app)
        .defaults(defaults)
        .task(check)
        .task(tests)
        .task(exe)
        .task(gate)
```

## Failure Routing

- Parser/typechecker error in project code: fix authored Kain or escalate to `bootstrap-core` if valid Kain fails.
- Import/module-root error: inspect `build.kn` roots first, then `crates/core/src/module_resolution.rs` only if the project is correct.
- Native LLVM verifier or lowering error: preserve emitted `.ll` and use `bootstrap-core` or the lowering owner.
- Runtime service/link error: use `runtime-core`, `runtime-stdlib`, or `runtime-gpu`.
- C ABI or platform package error: use `lang-interop` for authored design, `runtime-*` or package skill for implementation defects.
- Source test directive/report bug: use `test-harness`.
- Benchmark runner bug: use `test-bench`.
- Attrition runner bug: use `test-attrition`.
- Bazel, wrapper, stale binary, generated BUILD drift: use `tool-build-system`.

## Agent Hygiene

- Read `build.kn` before assuming a project root is TOML-owned.
- Run a dry-run JSON plan before rewriting structure.
- Query `stdlib` with `query_stdlib.py` before opening huge generated maps.
- Keep generated files out of source roots unless they are intended inputs.
- Add evidence tasks when adding capability claims.
- Put project lessons into `MEMORY.md` when they change future agent behavior.
