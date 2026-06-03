# build.kn Evidence DAG

`build.kn` is no longer only a build script sidecar. It now has an evaluator-first lane that turns authored Kain into the evidence DAG for a project or package: source checks, `std::test` suites, Z3 proof obligations, benchmark runs, attrition runs, root executables, GPU artifact tasks, capsules, and final certification gates all sit in one typed task graph.

The preferred surface is now first-class Kain API shape:

```kn
use std::build
use std::test
use std::proof
use std::bench
use std::attrition
use std::certify
```

The planner now tries the deterministic `build.kn` evaluator first and falls back to the legacy scanner only when the evaluator cannot lower the script. The evaluator supports the low-friction project surface: helper functions, constants, `map(...)`, computed strings, source sets, task objects as dependency references, multiline fluent chains, inferred build constants, and returned graph semantics. The legacy scanner remains a compatibility lane for older literal method-chain scripts.

## Agent Loop

Use this when changing a project, package, or workspace pipeline:

1. Put project/build/run authority in `build.kn` with `project(...)`; keep `package(...)`, `blade(...)`, `build_defaults(...)`, and `run_defaults(...)` for compatibility or split metadata.
2. Declare evidence edges with first-class constructors such as `check_task(...)`, `source_tests(...)`, `gpu_suite(...)`, `cuda_artifacts(...)`, `native_executable(...)`, `album_mode(...)`, `capsule_set(...)`, and `certify(...)`.
3. Use task values in `.requires(...)` when possible; the evaluator expands multi-task values such as GPU suites and mapped arrays into concrete DAG node ids.
4. Use `source_set(...)` with `.glob(...)`, `.file(...)`, `.dir(...)`, and `.exclude(...)` to avoid hand-listing every source input.
5. Run `kain build .` from the project root when you want the whole DAG.

`KAIN.toml` still works and still owns compatibility metadata not yet promoted into script authority, especially current C-FFI library declarations. When both exist, `build.kn` is the explicit task authority and `KAIN.toml` contributes defaults/legacy metadata.

## Workspace Authority

`build.kn` can also own workspace discovery directly through `std::build::workspace_defaults()`.

```kn
use std::build

fn build(ctx: BuildContext) -> BuildGraph:
    let ws = workspace_defaults()
        .blade_pattern("packages/*")
        .search_root("packages")
    return build_graph().workspace(ws)
```

This is the root-authority lane for script-only workspaces: no `KAIN.toml` is required just to discover nested blades, choose generated roots, or steer search patterns.

## Evaluated Project Surface

The preferred greenfield shape is a returned graph, not a source-scanned pile of exact literal calls:

```kn
use std::build
use std::test
use std::proof
use std::bench
use std::attrition
use std::certify

const KERNELS = ["search_kernel", "repair_kernel"]

fn kernel_check(name: String) -> BuildTask:
    return check_task("check-" + name + "-cuda")
        .entry("src/" + name + ".kn")
        .target("cuda")
        .axis("target", "cuda")

fn build(ctx: BuildContext) -> BuildGraph:
    let app = project("semantic-oracle")
        .kind("kain_tool")
        .entry("src/main.kn")
        .source_root("src")
        .targets("llvm", "cuda")
        .artifact_root(".kain/out")

    let sources = source_set("sources")
        .glob("src/**/*.kn")
        .exclude("src/*_kernel.kn")
        .dir("error_corpus")

    let host = check_task("check-host")
        .project(app)
        .target("llvm")
        .inputs(sources)

    let cuda = map(KERNELS, kernel_check)

    let exe = native_executable("root-executable")
        .project(app)
        .requires(host, cuda)

    return build_graph(app)
        .sources(sources)
        .tasks(host, cuda, exe, certify("semantic-oracle.local").requires(exe))
```

Evaluator notes:

- `const NAME = ...` is accepted in `build.kn` as a build-surface convenience even before the general parser grows inferred constants.
- Multiline fluent chains are accepted by the evaluator, so authors do not need scanner-era single-line method piles.
- `.fragment(...)` and `.compute(...)` are public GPU suite methods; the evaluator normalizes them internally because those words are reserved elsewhere in Kain.

## Task Kinds

`check` runs `kain-check` against a Kain entry.

```kn
let check = build_check("check-llvm")
    .entry("src/main.kn")
    .target("llvm")
    .axis("target", "llvm")
    .input("src/main.kn")
```

`test` runs the `kain-test` harness. It honors `//@` and `#@` directives, and without an override it infers `kain-test` from `test` items or `check-pass` from ordinary files.

```kn
let test_track = test_suite("source-tests")
    .entry("src/main.kn")
    .target("llvm")
    .requires("check-llvm")
```

`proof` runs the same harness in Z3 proof mode. It defaults to `prove-pass` and requires at least one proof evidence record. Use `.arg("prove-sat")` for witness/counterexample tasks.

```kn
let proof = proof_obligation("z3-layout-proof")
    .entry("z3/build-kn-evidence-proof.kn")
    .requires("check-llvm")
    .axis("solver", "z3")
```

`benchmark` and `attrition` run external evidence commands and write structured `kain-evidence.json` reports. By default they run `python benchmark/run.py ...` and `python attrition/run.py ...` from the repo root. Override with `.command(...)`, `.entry(...)`, `.cwd(...)`, and `.arg(...)` when needed.

```kn
let bench = bench_case("bench-ui")
    .arg("--case")
    .arg("kaintana_layout")
    .arg("--runs")
    .arg("1")

let abuse = attrition_case("attrition-small")
    .arg("--scale")
    .arg("small")
    .arg("--timeout")
    .arg("300")
```

`native-executable` uses the blade helper to compile a Kain entry through LLVM into an executable. This is the easy root-output lane.

```kn
let root_exe = native_executable("root-executable")
    .entry("src/main.kn")
    .root_output("$blade/my-blade.exe")
    .requires("check-llvm")
```

`certify` writes a certificate report after its dependency evidence passed. Dependency failure now gates dependents, so a certificate is not emitted after a failed proof, benchmark, attrition run, or executable build.

```kn
let certify = certify_gate("certify")
    .requires("check-llvm")
    .requires("source-tests")
    .requires("z3-layout-proof")
    .requires("root-executable")
    .certifies("my-blade.local")
```

## Alien Graph Metadata

Tasks can now carry graph metadata that appears in the JSON plan and future-proofs solver/capability-aware scheduling:

```kn
build_check("check-llvm")
    .axis("target", "llvm")
    .telemetry("llm.evidence")
    .requires_capability("host.os.windows")
```

`requires_capability(...)` is active: non-dry-run builds skip the task if the current host does not advertise the capability. Built-in host capabilities include `host`, `host.os.<os>`, `host.arch.<arch>`, `os.<os>`, `arch.<arch>`, `lane.<lane>`, `profile.<profile>`, and `target.<plan-target>`.

## Path Power

Normal relative paths resolve from the blade root. Script task paths also support prefixes:

- `$blade/...` resolves under the current blade or task root.
- `$root/...`, `$repo/...`, and `$workspace/...` resolve under the workspace root.
- `$task/...` and `$out/...` resolve under the task artifact directory.
- Absolute paths are honored as-is.

This makes root executable placement explicit:

```kn
.output("$blade/kaintana.exe")
.output("$root/bin/kaintana-nightly.exe")
.output("$task/generated.ll")
```

## Reports

Every evidence-style task writes `kain-evidence.json` under its task artifact directory unless you add outputs explicitly, in which case the report is appended to those outputs. Build sessions also write the normal build report and event stream under `.kain/reports/build/`.

Canonical artifacts stay under:

```text
.kain/out/<host>/<lane>/<target>/<blade>/<task>/
.kain/cache/build/stamps/
.kain/reports/build/
```

## Dogfood Examples

The first real adopters are:

- `blades/kloner/build.kn`: check, source-test, root executable, certify.
- `blades/kaintana/build.kn`: check, source-test, Z3 proof, root executable, certify.
- `blades/kaintana-test/build.kn`: consumer check, source-test, root executable, certify.
- `blades/build-kn-system-smoke/build.kn`: script-only root workspace authority, nested blade discovery via `workspace_defaults()`, polyglot adapter tasks, evidence DAG execution, coexistence with auto-discovered Fabric validate/run tasks, capability skip behavior, and negative planner fixtures.

These scripts intentionally keep C-FFI libraries in `KAIN.toml` until that metadata is promoted into build script authority. The task graph itself now lives in `build.kn`.
