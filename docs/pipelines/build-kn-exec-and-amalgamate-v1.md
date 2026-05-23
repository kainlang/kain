# build.kn Exec Graph And Amalgamate V1

Snapshot: May 22, 2026.

This note is the shipped-status companion to `docs/pipelines/build-kn-evidence-dag.md`.
It records the current state of `build.kn` after first-class `exec` and `amalgamate` landed,
what the V1 surface looks like in practice, and which boundaries are still intentionally missing.

## Why This Exists

`build.kn` already has the right center of gravity. It is Kain-authored project authority, task
graph authority, and evidence authority. The next pressure is not "should `build.kn` own the
pipeline?" It already does. The real question is whether the graph should remain only an evidence
DAG, or whether it should also become a disciplined exec graph.

This note argues for:

- keeping the DAG model
- allowing explicit task nodes to execute real host work
- promoting `amalgamate` into a first-class build task
- preferring Kain packages over a dynamic plugin system for V1

## Current Status

As of this snapshot, `build.kn` is the preferred blade and workspace authority surface, and the
V1 exec-graph lane is live.

Current repo truth:

1. `build.kn` is authored as typed Kain, not ad hoc shell.
2. The planner extracts known `std::build` and evidence constructors deterministically.
3. The executor now has first-class `exec` and `amalgamate` adapters in addition to the earlier
   evidence and artifact lanes.
4. `smoketest/build.kn` dogfoods both surfaces:
   - `exec_task(...)` drives the telemetry, benchmark, and attrition album runners
   - `amalgamate_capsule(...)` emits the editable smoketest capsule after `certify`
5. The full smoketest DAG succeeded with these lanes enabled, and the emitted capsule remained
   editable with a rich comment-preserving header.

One important implementation detail: process-bound Windows paths are normalized at child-process
boundaries. Internal graph/report paths can stay canonical, but `exec` and sibling host-runner
lanes strip verbatim `\\?\\` prefixes before spawning cargo, Kain, Python, Node, or similar host
tools.

## What build.kn Can Do Today

Current first-class evidence and build lanes include:

- `check`
- `exec`
- `amalgamate`
- `native-executable`
- `test`
- `proof`
- `benchmark`
- `attrition`
- `certify`
- `cargo-build`
- `c-shared-library`
- `gpu-artifacts`
- `fabric-validate`
- `fabric-run`
- `node`
- `bun`

The stdlib surface already exposes the generic task-builder shape:

- `build_task(...)`
- `exec_task(...)`
- `command_task(...)`
- `amalgamate_capsule(...)`
- `.kind(...)`
- `.entry(...)`
- `.manifest(...)`
- `.command(...)`
- `.arg(...)`
- `.cwd(...)`
- `.env(...)`
- `.input(...)`
- `.output(...)`
- `.depends_on(...)`
- `.requires_capability(...)`
- `.telemetry(...)`
- `.timeout_ms(...)`
- `.stdout(...)`
- `.stderr(...)`
- `.always_run()`
- `.name(...)`
- `.version(...)`
- `.author(...)`
- `.note(...)`
- `.tag(...)`
- `.meta(...)`
- `.storage(...)`
- `.archive(...)`
- `.editable()`
- `.header(...)`
- `.compression(...)`
- `.preview_symbols(...)`
- `.api_index(...)`
- `.module_index(...)`

That means `build.kn` is already more than a passive manifest. It is a typed graph declaration
that can drive real adapters.

## What Still Does Not Exist

### 1. There is still no dynamic build-task plugin system

There is no current dynamic plugin or adapter registry for `build.kn` task execution, and that is
still the right call for V1.

### 2. `exec` is intentionally conservative

The generic host-command lane is real now, but it is intentionally narrow:

- one program plus explicit argv
- nonzero exit code fails the task
- declared outputs must exist
- per-task evidence is always written
- the task is non-cacheable by default

That keeps the first version useful without pretending arbitrary host commands are reproducible.

### 3. Process-path portability is execution-boundary behavior

The executor normalizes Windows verbatim paths when it spawns child processes. Internal graph and
report paths may still appear in canonical `\\?\\...` form, especially in dry-run JSON and some
evidence metadata, because those are internal build-engine surfaces rather than child argv/env.

## Design Goal

The goal is not to turn `build.kn` into a shell replacement. The goal is to make it a complete
project and evidence authority that can also run explicit, inspectable host actions when the build
really needs them.

In other words:

- keep the graph explicit
- keep the actions typed
- keep the planner inspectable
- keep side effects declared

This is the difference between a serious build system and a pile of hidden scripts.

## Non-Goals

V1 should not introduce:

- arbitrary shell-string execution as the default authoring model
- hidden side effects with no declared inputs or outputs
- dynamic third-party plugin loading inside the planner
- a second mini language for quoting, pipes, and shell control flow
- "just run anything" convenience that makes reports and cache keys meaningless

## Recommendation

The V1 shape that landed is:

1. ship a first-class generic `exec` task kind
2. ship a first-class `amalgamate` task kind
3. use Kain packages for reusable task helpers and addons
4. defer a true plugin system until there is clear pressure for one

## V1: First-Class Exec Task

### Why

The build graph should be able to express host work such as:

- generating inputs before downstream checks
- running repo-local tooling
- staging deterministic artifacts
- driving adapters that do not yet justify their own built-in task kind

Right now that kind of work has to masquerade as `node`, `bun`, `benchmark`, or `attrition`.
That is mechanically workable but semantically wrong.

### Shipped API

```kn
use std::build

let prep = exec_task("refresh-generated")
    .command("powershell")
    .arg("-File")
    .arg("scripts/refresh_generated.ps1")
    .cwd("$root")
    .env("KAIN_BIN", "$root/target/debug/kain.exe")
    .input("scripts/refresh_generated.ps1")
    .input("src")
    .output("$task/refresh.ok")
    .depends_on("check-llvm")
    .timeout_ms(60000)
    .always_run()
```

Aliases can exist if they improve taste:

- `exec_task(...)`
- `command_task(...)`

The internal planner kind should still be one thing, likely `exec`.

### Shipped V1 Methods

Base fields should reuse the existing builder style wherever possible:

- `.command(String)`
- `.arg(String)`
- `.cwd(String)`
- `.input(String)`
- `.output(String)`
- `.depends_on(String)`
- `.requires_capability(String)`
- `.telemetry(String)`

New V1 methods:

- `.env(String, String)`
- `.timeout_ms(Int)`
- `.always_run()`
- `.stdout(String)`
- `.stderr(String)`

Optional V1 alias:

- `.script(String)` as sugar for `.entry(...)` is probably unnecessary

The important part is that the task describes a program plus argv, not a shell snippet.

### V1 Execution Rules

`exec` should be intentionally strict:

1. The task launches one program with explicit argv.
2. Exit code `0` means success; any nonzero exit code fails the task.
3. Declared outputs must exist after a successful run.
4. The executor writes a per-task report under the normal task artifact root.
5. `stdout` and `stderr` may be captured into declared files when requested.

### V1 Caching Rule

For V1, `exec` should default to conservative behavior.

Current rule:

- `exec` is non-cacheable by default
- `.always_run()` makes that explicit in source
- a later V2 may add opt-in cacheable exec tasks when inputs, outputs, environment, and tool
  identity are fully modeled

That keeps the first version useful without pretending arbitrary host commands are reproducible
just because they sat in the graph.

### Why Not Shell Strings

V1 should not add `sh("echo hi | other-tool")` style authoring.

That path creates:

- quoting problems
- platform drift
- hidden interpreter dependency
- poor tooling visibility

If a task needs PowerShell, Python, Node, Bun, or Kain itself, it can call that executable
explicitly:

```kn
exec_task("regen")
    .command("python")
    .arg("scripts/regen.py")
```

That is already expressive enough and much easier to reason about.

## V1: First-Class Amalgamate Task

### Why

`amalgamate` is not a random helper. It is already a core Kain packaging and portability lane.
The CLI already understands it, and the rest of the toolchain already knows how to consume the
resulting capsule.

That makes it a strong candidate for first-class build graph support.

### Shipped API

```kn
use std::build

let capsule = amalgamate_capsule("smoketest-capsule")
    .path("smoketest")
    .output("$root/capsules/smoketest.kn")
    .name("smoketest")
    .tag("portable")
    .header("rich")
    .preview_symbols(32)
    .depends_on("certify")
```

Archive variant:

```kn
let archive = amalgamate_capsule("smoketest-archive")
    .path("smoketest")
    .output("$root/capsules/smoketest.archive.kn")
    .archive(true)
    .compression("zstd")
    .depends_on("certify")
```

### Shipped V1 Methods

Required:

- `.path(String)`
- `.output(String)`

Portable metadata:

- `.name(String)`
- `.version(String)`
- `.author(String)`
- `.note(String)`
- `.tag(String)`
- `.meta(String, String)`

Capsule formatting:

- `.archive(Bool)`
- `.compression(String)`
- `.header(String)`
- `.preview_symbols(Int)`
- `.api_index(String)`
- `.module_index(String)`

The build graph should use the crate implementation directly, not shell out to `kain amalgamate`.

### V1 Execution Rules

1. The task packages a declared file, blade root, or workspace root into one `.kn` capsule.
2. The task emits the capsule to the declared output path.
3. The task report records digest, capsule mode, file count, and source root.
4. The task is cacheable because the crate already has a real content-driven model.

### Why This Should Be First-Class

Without a native task kind, authors will abuse generic command nodes for something the toolchain
already understands deeply. That would be a step backward.

This:

```kn
build_task("capsule")
    .kind("node")
    .command("kain")
    .arg("amalgamate")
```

is exactly the kind of workaround V1 should retire.

## Packages Versus Plugins

The right V1 answer is "packages first, plugins later if the pressure becomes real."

### Why packages are enough for now

Kain packages can already provide reusable authored helpers and task composition patterns without
teaching the planner how to load third-party execution code at runtime.

That means teams can author addon-style helpers like:

```kn
use my_build_addons

let capsule = my_build_addons.smoketest_capsule("smoketest-capsule")
```

This gives reuse, taste, and local conventions without sacrificing planner determinism.

### Why a plugin system is premature in V1

A dynamic plugin system is worth the complexity only when the core repo can no longer comfortably
own the adapter surface.

Today, Kain still benefits more from:

- a small number of first-class built-ins
- a generic exec escape hatch
- package-authored helper layers

than from loading external task providers into the build engine itself.

## Current-State Summary

The current `build.kn` system is now strong in these areas:

- typed task authoring in Kain
- deterministic graph extraction
- explicit evidence DAG composition
- first-class host exec nodes
- first-class capsule production
- path-rooted outputs and reports
- host capability gating
- several real adapter lanes beyond plain checks

The current weak spots are:

- no dynamic plugin/provider system
- no cacheable generic exec mode yet
- internal/dry-run path rendering still exposes canonical `\\?\\` paths in some reports
- no package-level documented pattern yet for reusable build addons

## What Landed

The implemented order ended up being:

1. add `BuildTaskKind::Exec` and `std::build::exec_task(...)`
2. add `BuildTaskKind::Amalgamate` and `std::build::amalgamate_capsule(...)`
3. extend `BuildTaskSpec` with env, timeout, stdout, stderr, and capsule metadata
4. add planner and executor coverage in `crates/kain-build`
5. dogfood both tasks in `smoketest/build.kn`
6. normalize Windows process-bound paths at child execution boundaries
7. validate the full smoketest DAG and emit the editable smoketest capsule

## Senior-Engineer Bottom Line

`build.kn` should become an exec graph, but only in a disciplined way.

The winning shape is:

- DAG first
- typed actions second
- built-ins for core lanes
- packages for reuse
- no dynamic plugin system yet

That gives Kain a build pipeline that feels complete without inheriting the worst parts of
`build.rs`, shell-heavy CI YAML, or opaque plugin-based build logic.
