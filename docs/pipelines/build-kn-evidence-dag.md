# build.kn Evidence DAG

`build.kn` is no longer only a build script sidecar. It can now describe the evidence DAG for a blade: source checks, `std::test` suites, Z3 proof obligations, benchmark runs, attrition runs, root executables, and final certification gates all sit in one typed task graph.

## Agent Loop

Use this when changing a blade, package, or workspace pipeline:

1. Put blade/package/build/run authority in `build.kn` with `package(...)`, `blade(...)`, `build_defaults(...)`, and `run_defaults(...)`.
2. Declare every evidence edge as a `build_task("id")` with a first-class `kind(...)`.
3. Use `depends_on(...)` to make certification depend on the evidence, not on vibes.
4. Use explicit `.input(...)` and `.output(...)` paths so cache keys and reports explain what mattered.
5. Run `kain blades build . --json` or `blade build . --json` from the blade root when you want the whole DAG.

`KAIN.toml` still works and still owns compatibility metadata not yet promoted into script authority, especially current C-FFI library declarations. When both exist, `build.kn` is the explicit task authority and `KAIN.toml` contributes defaults/legacy metadata.

## Task Kinds

`check` runs `kain-check` against a Kain entry.

```kn
let check = build_task("check-llvm")
    .kind("check")
    .entry("src/main.kn")
    .target("llvm")
    .input("src/main.kn")
```

`test` runs the `kain-test` harness. It honors `//@` and `#@` directives, and without an override it infers `kain-test` from `test` items or `check-pass` from ordinary files.

```kn
let source_tests = build_task("source-tests")
    .kind("test")
    .entry("src/main.kn")
    .target("llvm")
    .depends_on("check-llvm")
```

`proof` runs the same harness in Z3 proof mode. It defaults to `prove-pass` and requires at least one proof evidence record. Use `.arg("prove-sat")` for witness/counterexample tasks.

```kn
let proof = build_task("z3-layout-proof")
    .kind("proof")
    .entry("z3/build-kn-evidence-proof.kn")
    .depends_on("check-llvm")
```

`benchmark` and `attrition` run external evidence commands and write structured `kain-evidence.json` reports. By default they run `python benchmark/run.py ...` and `python attrition/run.py ...` from the repo root. Override with `.command(...)`, `.entry(...)`, `.cwd(...)`, and `.arg(...)` when needed.

```kn
let bench = build_task("bench-ui")
    .kind("benchmark")
    .arg("--case")
    .arg("kaintana_layout")
    .arg("--runs")
    .arg("1")

let abuse = build_task("attrition-small")
    .kind("attrition")
    .arg("--scale")
    .arg("small")
    .arg("--timeout")
    .arg("300")
```

`native-executable` uses the blade helper to compile a Kain entry through LLVM into an executable. This is the easy root-output lane.

```kn
let root_exe = build_task("root-executable")
    .kind("native-executable")
    .entry("src/main.kn")
    .output("$blade/my-blade.exe")
    .depends_on("check-llvm")
```

`certify` writes a certificate report after its dependency evidence passed. Dependency failure now gates dependents, so a certificate is not emitted after a failed proof, benchmark, attrition run, or executable build.

```kn
let certify = build_task("certify")
    .kind("certify")
    .depends_on("check-llvm")
    .depends_on("source-tests")
    .depends_on("z3-layout-proof")
    .depends_on("root-executable")
```

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

These scripts intentionally keep C-FFI libraries in `KAIN.toml` until that metadata is promoted into build script authority. The task graph itself now lives in `build.kn`.
