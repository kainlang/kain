# build.kn Evidence DAG

`build.kn` is no longer only a build script sidecar. It can now describe the evidence DAG for a blade: source checks, `std::test` suites, Z3 proof obligations, benchmark runs, attrition runs, root executables, and final certification gates all sit in one typed task graph.

The preferred surface is now first-class Kain API shape:

```kn
use std::build
use std::test
use std::proof
use std::bench
use std::attrition
use std::certify
```

The planner treats these constructors as build intrinsics today. That means `build.kn` is authored as Kain with typed `std::*` specs, while graph extraction remains deterministic and side-effect free until the self-hosted evaluator is ready.

## Agent Loop

Use this when changing a blade, package, or workspace pipeline:

1. Put blade/package/build/run authority in `build.kn` with `package(...)`, `blade(...)`, `build_defaults(...)`, and `run_defaults(...)`.
2. Declare every evidence edge with first-class constructors such as `build_check(...)`, `test_suite(...)`, `proof_obligation(...)`, `bench_case(...)`, `attrition_case(...)`, `native_executable(...)`, and `certify_gate(...)`.
3. Use `depends_on(...)` to make certification depend on the evidence, not on vibes.
4. Use explicit `.input(...)` and `.output(...)` paths so cache keys and reports explain what mattered.
5. Run `kain blades build . --json` or `blade build . --json` from the blade root when you want the whole DAG.

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
let source_tests = test_suite("source-tests")
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
- `blades/build-kn-system-smoke/build.kn`: script-only root workspace authority, nested blade discovery via `workspace_defaults()`, polyglot adapter tasks, evidence DAG execution, capability skip behavior, and negative planner fixtures.

These scripts intentionally keep C-FFI libraries in `KAIN.toml` until that metadata is promoted into build script authority. The task graph itself now lives in `build.kn`.
