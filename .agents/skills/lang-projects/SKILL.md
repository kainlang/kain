---
name: lang-projects
description: >-
  Use when authoring, explaining, reviewing, or repairing Kain project and workspace flow from the language side: build.kn project authority, platform.kn requirements, package and blade metadata, KAIN.toml compatibility, project layouts, imports and module roots, check/run/build/watch loops, native LLVM executable outputs, source tests, evidence DAGs with std build/proof/bench/attrition/certify APIs, portable amalgamate capsules, Fabric or Omni project manifests, and how agents should work efficiently inside Kain codebases without changing CLI, Bazel, compiler, runtime, or harness internals.
---

# Lang Projects

This is the authored Kain project pipeline skill. Use it when the task is not just "write a function", but "make a Kain thing live inside a project, build, run, test, prove, package, or travel as a portable unit."

## Prime Directive

- Treat `build.kn` as the future project authority. It is Kain source that declares package shape, run defaults, build defaults, platform requirements, evidence tasks, and certification gates.
- Treat `KAIN.toml` as compatibility and leftover metadata, not the mental center of Kain. It still works, and it still carries metadata not yet promoted into `build.kn`, especially some current C FFI library declarations.
- Keep the small case small. Kain can be a Python-like script, a 100-line full app, a UI framework, a C ABI bridge, a GPU language, a simulation lane, or a monorepo workspace. Do not force every Kain file into a blade-shaped crate box.
- Use blades when scale demands local package/workspace behavior: many modules, reusable package surfaces, sibling dependencies, native bridge folders, workspace graphs, or reusable app libraries.
- Make evidence explicit. A real Kain project should move from `check` to `test` to `proof` to `bench` to `attrition` to `native-executable` to `certify` as the claim gets stronger.
- Keep project orchestration in Kain where possible. Escalate to `tool-build-system` only when the issue is Bazel, launcher shims, generated BUILD drift, stale repo binaries, or building Kain itself.

## Fast Operator Loop

Use these before inventing project structure:

```powershell
rg -n "fn build\(ctx: BuildContext\)|build_check|test_suite|proof_obligation|bench_case|attrition_case|certify_gate|native_executable|platform_package|platform_requirement" build.kn blades apps templates docs stdlib
python query_stdlib.py --module build --limit 80
python query_stdlib.py --module test --limit 40
python query_stdlib.py --module proof --limit 40
python query_stdlib.py --module bench --limit 40
python query_stdlib.py --module attrition --limit 40
python query_stdlib.py --module certify --limit 40
kain check path\to\entry.kn --target llvm
kain run path\to\entry.kn --target llvm
kain run plan path\to\entry.kn --target llvm --json
kain build path\to\entry.kn --target llvm -o path\to\app.exe
kain test path\to\suite --target llvm --json-out .kain\reports\kain-test.json
```

For project DAGs:

```powershell
kain run plan . --target auto --json
kain build .
kain run .
```

For portable project capsules:

```powershell
kain amalgamate path\to\project -o .kain\capsules\project.kn --name project --tag portable
kain amalgamate path\to\project -o .kain\capsules\project.archive.kn --archive --compression zstd
kain amalgamate inspect .kain\capsules\project.kn --json
kain amalgamate unpack .kain\capsules\project.kn -o .kain\unpacked\project
kain check .kain\capsules\project.kn --target llvm
kain run .kain\capsules\project.kn --target llvm
```

Use the project helper when you need a root executable proof from a local entry:

```powershell
.\.agents\skills\lang-projects\scripts\compile_kain_project_to_root.ps1 -Entry blades\kaintana\src\main.kn -OutputName kaintana.exe -Run -VerifyLlvm
```

## Mental Model

Kain project shape is a scale ladder, not one mandatory workspace system:

| Scale | Use | Typical files | Commands |
| --- | --- | --- | --- |
| One-file script | Quick automation, proof, CLI-ish utility, algorithm, importer output | `tool.kn` | `kain check`, `kain run`, `kain build` |
| Small folder | App with modules, config, data, native bridge, shaders, tests | `src/*.kn`, `config/*`, `native/*`, `build.kn` | `kain run path`, `kain run plan`, `kain test` |
| Build authority project | Any source tree that needs reproducible evidence and outputs | `build.kn`, optional `platform.kn`, optional `KAIN.toml` | `kain build .`, `kain run .`, `kain run plan . --json` |
| Blade package | Reusable Kain package or app inside a larger local workspace | `build.kn`, `src/`, `native/`, `.kain/`, maybe `KAIN.toml` | `kain build .`, `kain run .`, `kain run plan . --json` |
| Monorepo workspace | Many packages, sibling dependencies, platform packages, native bridges | `blades/*`, `apps/*`, package roots | `kain build .`, `kain run .`, `kain run plan . --json` |
| Capsule | Portable source/project blob for handoff, inspection, or replay | one `.kn` capsule with comment-safe metadata and payload | `kain amalgamate`, `inspect`, `unpack`, transparent `check/run/build` |
| Fabric or Omni | Existing polyglot orchestration lanes, especially older mixed Rust/C/Node workflows | `KAIN.fabric.toml`, Omni manifests | `kain fabric validate/run`, `kain omni ...` |

The key move: start with the smallest form that proves the work, then graduate to `build.kn` when the project needs durable evidence, then to blade/workspace when reuse and dependency discovery matter.

## Project Authority

Prefer this order when deciding where metadata lives:

1. Put Kain-owned project authority in `build.kn`: package name, version, description, source roots, module roots, default entry, run target, artifact/cache roots, build tasks, evidence tasks, root executable outputs, certification.
2. Put platform requirements in `build.kn` or `platform.kn` with `platform_package(...)`, `platform_requirement(...)`, or package-specific build APIs when the source needs SDKs such as Vulkan.
3. Keep `KAIN.toml` only for compatibility, old tooling, and metadata not yet promoted into script authority. Current C FFI library declarations may still live there until that surface moves.
4. Do not create new TOML just because another language would. If a new Kain project can be represented by `build.kn`, author it there.
5. If both `build.kn` and `KAIN.toml` exist, `build.kn` is the explicit task authority and TOML contributes defaults or legacy metadata.

The death of TOML does not mean "delete every TOML immediately." It means agents should stop designing new Kain systems around TOML as the source of truth.

## Source Anchors

Use these when the project flow itself needs verification:

- Project authority doc: `docs/pipelines/build-kn-evidence-dag.md`.
- Build stdlib API: `stdlib/build.kn`, `stdlib/test.kn`, `stdlib/proof.kn`, `stdlib/bench.kn`, `stdlib/attrition.kn`, `stdlib/certify.kn`.
- Build graph extraction and task adapters: `crates/build/src/workspace.rs`.
- Workspace and package discovery: `crates/blades`.
- Run planning and watch inputs: `crates/run/src/lib.rs`.
- Command definitions: `crates/commands/src/kain.rs`, `crates/commands/src/blade.rs`, `crates/commands/src/fabric.rs`.
- Check and source test contract: `docs/cli/check-and-test.md`, `crates/check`, `crates/test`, `smoketest/kain-test`.
- Portable capsule lane: `crates/amalgamate`, `crates/cli/src/amalgamate.rs`, `blades/amalgamate-capsule-probe`.
- Existing dogfood DAGs: `blades/kloner/build.kn`, `blades/kaintana/build.kn`, `blades/kaintana-test/build.kn`, `blades/vulkain/build.kn`.

## Project Layouts

Use a one-file script when the task is small:

```kn
use std::runtime
use std::text

fn main() -> Int:
    let boot = runtime_init()
    if boot != 0:
        return boot
    println(text_trim("  kain project ok  "))
    return runtime_shutdown()
```

Use a folder when the app has real shape:

```text
project/
  build.kn
  src/
    main.kn
    state.kn
    runtime.kn
    ui.kn
  config/
    modes.json
  native/
    bridge.h
    bridge.c
  tests/
    smoke.kn
  z3/
    layout-proof.kn
  .kain/
    out/
    cache/
    reports/
```

Use a blade/package layout only when reuse, dependency graphing, or local package boundaries matter:

```text
blades/my-package/
  build.kn
  KAIN.toml
  src/
  native/
  reference/
  .kain/
```

Keep authored source in `src/`, external design/spec material in `reference/`, native package or app bridges in `native/`, generated artifacts in `.kain/`, and checked reports under `.kain/reports/`.

## Build Dot Kn Evidence DAG

Author `build.kn` as Kain. The planner treats the stdlib constructors as build intrinsics today, so graph extraction is deterministic and side-effect free.

Preferred imports:

```kn
use std::build
use std::test
use std::proof
use std::bench
use std::attrition
use std::certify
```

Canonical shape:

```kn
use std::build
use std::test
use std::proof
use std::bench
use std::attrition
use std::certify

fn build(ctx: BuildContext) -> BuildGraph:
    let pkg = package("my-app")
        .version("0.1.0")
        .description("Kain-owned app with explicit evidence.")

    let app = blade("my-app")
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

    let run = run_defaults()
        .entry("src/main.kn")
        .target("llvm")

    let check = build_check("check-llvm")
        .entry("src/main.kn")
        .target("llvm")
        .axis("target", "llvm")
        .telemetry("llm.evidence")
        .input("src/main.kn")
        .input("build.kn")

    let source_tests = test_suite("source-tests")
        .entry("tests/smoke.kn")
        .target("llvm")
        .requires("check-llvm")
        .input("tests/smoke.kn")
        .input("src/main.kn")

    let proof = proof_obligation("z3-layout-proof")
        .entry("z3/layout-proof.kn")
        .requires("check-llvm")
        .axis("solver", "z3")
        .telemetry("llm.proof")
        .input("z3/layout-proof.kn")

    let bench = bench_case("bench-hot-path")
        .requires("root-executable")
        .arg("--case")
        .arg("my_app_hot_path")
        .arg("--runs")
        .arg("1")

    let abuse = attrition_case("attrition-small")
        .requires("root-executable")
        .arg("--scale")
        .arg("small")
        .arg("--timeout")
        .arg("300")

    let root_exe = native_executable("root-executable")
        .entry("src/main.kn")
        .root_output("$blade/my-app.exe")
        .requires("check-llvm")
        .requires("source-tests")
        .requires("z3-layout-proof")
        .input("src/main.kn")
        .input("build.kn")

    let gate = certify_gate("certify")
        .requires("check-llvm")
        .requires("source-tests")
        .requires("z3-layout-proof")
        .requires("root-executable")
        .certifies("my-app.local")

    return build_graph()
        .package(pkg)
        .blade(app)
        .defaults(defaults)
        .run(run)
        .task(check)
        .task(source_tests)
        .task(proof)
        .task(root_exe)
        .task(bench)
        .task(abuse)
        .task(gate)
```

Evidence rules:

- Use `build_check(...)` for frontend/source validation.
- Use `test_suite(...)` for `kain-test` source tests and directive-backed suites.
- Use `proof_obligation(...)` for Z3 proof mode. It should depend on at least the check lane.
- Use `bench_case(...)` for performance claims. Prefer named benchmark cases over raw commands when the repo runner knows the case.
- Use `attrition_case(...)` for teardown, long-run, sabotage, or runtime cleanliness claims.
- Use `native_executable(...)` for root executable proofs through LLVM/native.
- Use `certify_gate(...)` as the final certificate node, never as a substitute for the evidence it depends on.
- Use `.requires(...)` so failed proof, benchmark, attrition, or executable tasks block certification.
- Use `.input(...)` and `.output(...)` aggressively so cache keys, reports, and future agents know what mattered.

## Task Metadata

Tasks can carry scheduler and report metadata:

```kn
let check = build_check("check-llvm")
    .entry("src/main.kn")
    .target("llvm")
    .axis("target", "llvm")
    .telemetry("llm.evidence")
    .requires_capability("host.os.windows")
```

Current capability names include:

- `host`
- `host.os.<os>`
- `host.arch.<arch>`
- `os.<os>`
- `arch.<arch>`
- `lane.<lane>`
- `profile.<profile>`
- `target.<plan-target>`

Use capability requirements for platform-specific app lanes, GPU/Vulkan probes, native DLL dependencies, or build machines with required toolchains.

## Path Rules

Normal relative paths resolve from the project or blade root. Script task paths also support prefixes:

- `$blade/...` resolves under the current blade or task root.
- `$root/...`, `$repo/...`, and `$workspace/...` resolve under the workspace root.
- `$task/...` and `$out/...` resolve under the task artifact directory.
- Absolute paths are honored as-is.

Use root outputs deliberately:

```kn
native_executable("root-executable")
    .entry("src/main.kn")
    .root_output("$blade/my-app.exe")
    .output("$task/generated.ll")
```

Do not scatter generated artifacts under repo-root `target/` unless the user explicitly wants shared build output. Prefer `.kain/out`, `.kain/cache`, and `.kain/reports`.

## Reports

Every evidence-style task writes `kain-evidence.json` under its task artifact directory unless outputs are overridden.

Canonical locations:

```text
.kain/out/<host>/<lane>/<target>/<project>/<task>/
.kain/cache/build/stamps/
.kain/reports/build/
```

When summarizing a project build, inspect the JSON report or event stream if the answer depends on task status, skipped capabilities, or dependency gating.

## Checking And Testing

Use `kain check` for fast frontend validation:

```powershell
kain check src\main.kn --target llvm
kain check . --target llvm --fail-fast --json-out .kain\reports\check.json
Get-Content src\main.kn | kain check -
```

Use `kain test` for source certification:

```powershell
kain test tests --target llvm
kain test smoketest\kain-test --json-out .kain\reports\kain-test.json
kain test tests --mode check-pass --fail-fast
kain test tests --ignored
```

Supported source directives:

```kn
//@ check-pass
//@ target: llvm

fn main() -> Int:
    return 0
```

Current modes:

- `check-pass`: frontend validation must pass.
- `check-fail`: frontend validation must fail.
- `run-pass`: interpreter/runtime execution must pass.
- `run-fail`: interpreter/runtime execution must fail.
- `kain-test`: run Kain `test` items.
- `prove-pass`: run embedded SMT2 and require Z3 `unsat`.
- `prove-sat`: run embedded SMT2 and require a witness.

If the harness behavior itself is wrong, use `test-harness`. If the test exposes a parser/typechecker/lowering/runtime bug, preserve the repro and hand the implementation to the owning `bootstrap-*` or `runtime-*` skill.

## Build And LLVM Native

For a loose file:

```powershell
kain check src\main.kn --target llvm
kain build src\main.kn --target llvm -o .kain\out\my-app.exe
.\.kain\out\my-app.exe
```

For a project root executable:

```powershell
.\.agents\skills\lang-projects\scripts\compile_kain_project_to_root.ps1 -Entry src\main.kn -OutputName my-app.exe -Run -VerifyLlvm
```

For serious repo-scale compiler freshness:

```powershell
bazel build //:kain --config=dev
$bazelBin = (bazel info bazel-bin --config=dev | Select-Object -Last 1).Trim()
$kainBin = Join-Path $bazelBin "crates\cli\kain.exe"
& $kainBin check src\main.kn --target llvm
& $kainBin build src\main.kn --target llvm -o .kain\out\my-app.exe
```

For large evidence DAGs such as `smoketest/build.kn`, prefer a fresh Bazel release launcher for final proof:

```powershell
bazel build //:kain --config=release
$bazelBin = (bazel info bazel-bin --config=release | Select-Object -Last 1).Trim()
$kainBin = Join-Path $bazelBin "crates\cli\kain.exe"
& $kainBin build smoketest
```

Native-executable tasks now pass the current `kain.exe` into the helper as `-KainBin`, so the child compile follows the same dev/release lane as the parent build graph. If the parent is release and a child process is still a debug `kain.exe`, treat that as build-runner drift.

Use `tool-build-system` if Bazel sync, launcher provenance, generated `BUILD.bazel`, or `kain doctor` is the problem. Stay in `lang-projects` if the question is how authored Kain projects should build and prove themselves.

If the compiler emits `.ll`, `.bc`, `.pdb`, `.ilk`, `*.runtime_contract.json`, or `*.realtime_app.json`, keep those sidecars under `.kain/out/<artifact>/` unless the project DAG already places them in a task output directory.

## Run, Plan, Dev, Watch

Use `kain run` for immediate execution:

```powershell
kain run src\main.kn --target llvm --json
kain run . --target auto --json
kain run . --target llvm -- arg1 arg2
```

Use `kain run plan` before debugging weird resolution:

```powershell
kain run plan . --target auto --json
```

Use dev/watch only when rerun feedback matters:

```powershell
kain run dev . --target llvm --json --keep-artifacts
kain watch . --target llvm --json
```

The run planner watches entry inputs plus `KAIN.toml`, `kain.toml`, `build.kn`, `platform.kn`, platform locks, generated modules, and binding reports when present.

## Imports And Modules

For authored imports:

- Prefer root `std::*` imports for public stdlib surface.
- Use `use c::...` for automatic runtime-owned C ABI import shape when appropriate. Do not require TOML just to mention a runtime-owned C namespace.
- Use explicit bridge/package metadata only when the bridge is blade/package-owned, inline, object/shared library backed, or needs local native source.
- Keep module roots explicit in `build.kn` for larger projects so agents do not fake import paths.
- Use `kain import-c`, `kain import-rust`, `kain import platform`, and related importers to generate starting surfaces when crossing ecosystems.

Escalate to `lang-c-abi` for OS, DLL, C ABI, Rust crate, host JSON, or platform package design. Escalate to `bootstrap-core` only when import resolution or compiler-owned module semantics are broken.

## Amalgamate Capsules

`kain amalgamate` is the portable Kain project container. It is not a random paste dump. It is closer to the SQLite amalgamation idea: collapse a source tree into one comment-safe `.kn` artifact with metadata, index preview, and materialization support.

Use it when:

- You need to hand a whole Kain project to an agent or tool as one file.
- You want a portable repro without copying many source files.
- You need to preserve module/source shape through a single artifact.
- You want `kain check`, `kain run`, or `kain build` to transparently materialize a capsule under `.kain/cache/amalgamate/<digest>/workspace`.

Pack editable blocks:

```powershell
kain amalgamate blades\amalgamate-capsule-probe -o .kain\capsules\probe.kn --name probe --tag smoke
```

Pack archive payload:

```powershell
kain amalgamate blades\amalgamate-capsule-probe -o .kain\capsules\probe.archive.kn --archive --compression zstd --header rich
```

Inspect and unpack:

```powershell
kain amalgamate inspect .kain\capsules\probe.kn
kain amalgamate inspect .kain\capsules\probe.kn --json
kain amalgamate unpack .kain\capsules\probe.kn -o .kain\unpacked\probe
```

Capsule rules:

- Keep the generated `//!kain-capsule` and `//!end-kain-capsule` metadata block intact.
- For archive capsules, keep the `//!kain-capsule-payload` block intact.
- Do not place unpacked capsule probes under a directory you later `kain check .` unless you want those generated `.kn` files included.
- Companion capsule outputs such as `project.artifacts.kn` and `project.evidence.kn` should not be packed as source siblings; current amalgamate skips sibling companions and writes capsules atomically to avoid Windows mapped-section write failures.
- Use `blades/amalgamate-capsule-probe` as the local dogfood shape.

## Blades As Scale Mode

Blades are useful; they are just not the definition of Kain.

Use a blade when:

- The code is a reusable Kain package.
- The code has sibling dependencies.
- The code needs package-owned native bridge files under `native/`.
- The code needs workspace graph/equip behavior.
- The code is a large app proof that should live under `blades/` for dogfooding.
- The code needs package-local `.kain/` artifacts and repeatable root executable proofs.

Project-first commands:

```powershell
kain build .
kain run .
kain run plan . --target auto --json
kain clean .
```

Blade design rules:

- Keep app entrypoints thin. Put semantics in `state.kn`, `runtime.kn`, `ui.kn`, `gpu.kn`, `scene.kn`, or domain-named modules.
- Keep native bridge ownership local to the package that exposes the bridge.
- Prefer `build.kn` for new authority and use `KAIN.toml` only where compatibility demands it.
- Keep generated artifacts under the package `.kain/` tree.
- If adding/changing core language/runtime behavior, dogfood it in a blade when practical, but do not make every feature wait for a monorepo package.

## Fabric And Omni

Fabric exists for polyglot local orchestration, and Kain can still run it:

```powershell
kain fabric init . --template polyglot
kain fabric validate -m KAIN.fabric.toml
kain fabric run -m KAIN.fabric.toml
kain run KAIN.fabric.toml --target fabric
```

Use Fabric when maintaining existing Fabric manifests or when a project genuinely needs the older polyglot orchestration lane. For fresh Kain-native project authority, prefer `build.kn` plus explicit evidence tasks. Do not route new Kain-native app work through Fabric just because it can coordinate Rust, Node, C, or external steps.

Use Omni only when the task explicitly targets the mixed-language Omni pipeline. Otherwise keep authored Kain project truth in `build.kn`.

## Domain Handoff

`lang-projects` owns project shape, authority, evidence graph usage, local outputs, and command flow. It does not own every subsystem touched by the project.

Use sibling skills when the core work is domain-specific:

| Project contains | Co-trigger |
| --- | --- |
| First-class Kain semantics, worlds, laws, patches, effects, components | `lang-semantics` |
| Actors, async, raw memory, ownership, zero-copy systems code | `lang-systems` |
| C ABI, DLLs, platform packages, Rust crate bridges | `lang-c-abi` |
| GPU shaders, compute, graphics resources, render loops | `lang-gpu` |
| UI components, native UI, framework surfaces | `lang-ui`, maybe `package-kaintana` |
| Stdlib usage or std domain selection | `lang-stdlib` |
| Source test harness behavior | `test-harness` |
| Bench runner behavior or performance report interpretation | `test-bench` |
| Attrition runner behavior or teardown certification | `test-attrition` |
| Crash dumps, hangs, native executable forensics | `test-crash-forensics` |
| Bazel, launcher shims, generated BUILD files, building Kain itself | `tool-build-system` |

## Efficient Agent Workflow

When entering an existing Kain codebase:

1. Search `ARCHITECTURE.md` and `MEMORY.md` for the project, feature, command, or error.
2. Inspect `build.kn`, `platform.kn`, and only then `KAIN.toml`.
3. Run `kain run plan . --target auto --json` when resolution is unclear, then `kain build .` when you want the full build authority DAG.
4. For the smoketest album, use `kain build smoketest`; do not substitute `kain run smoketest/src/main.kn` except as a focused debug lane, because the direct run bypasses the build graph and can leave noisy telemetry under `smoketest/src/telemetry/`.
5. Query the stdlib map instead of reading the whole atlas.
6. Read only the source modules that the graph, imports, or failing task actually touches.
7. Add or update evidence tasks when the project gains a claim.
8. Keep outputs under `.kain/` and summarize report paths.

This is how agents stop treating Kain as loose snippets and start working inside Kain codebases like they have a map.

## Anti-Patterns

- Do not make `KAIN.toml` the first design surface for new projects.
- Do not turn `build.kn` into an opaque shell script. It should be a typed evidence graph.
- Do not hide tests, proofs, benchmarks, or attrition behind prose. Put them in the DAG when they certify the project.
- Do not run directory-wide checks over `.kain/unpacked` or generated capsule materialization unless that is intentional.
- Do not use Fabric as the default new project story.
- Do not dump every module into `main.kn`; small Kain can be tiny, but real Kain projects should have semantic structure.
- Do not scatter executables and sidecars across repo root. Use `.kain/out`, `$task`, `$blade`, or explicit root outputs.
- Do not route a compiler/runtime defect around with project metadata. Preserve the repro and escalate to the owning skill.
- Do not put a second whole-project `kain check` inside native executable helpers when the `build.kn` DAG already has an explicit check prerequisite; the native compile path performs frontend validation and should stay in one process so sidecar staging can reuse typed frontend state.
